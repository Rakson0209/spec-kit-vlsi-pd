// Global Placement (HPWL minimization) — self-contained analytical placer.
//
// Spec: problems/003-global-placement/experiments/claude-opus-4-8/spec/
// Build: g++ -std=c++20 -O3 -fopenmp -pthread -static -o hw4 main.cpp
//
// Pipeline (matches plan.md L1/L2/L3):
//   L1  Bookshelf parse (.aux -> .nodes/.nets/.pl/.scl) + CSR data model
//       + constructive legal spread (guaranteed-legal MVP start).
//   L2  Analytical placement: LSE wirelength surrogate (with pin offsets,
//       numerically shifted) + bell-shaped bin-density penalty (compact
//       support) minimized by Adam, with a lambda (density-weight) ramp.
//   L3  OpenMP-parallel objective/gradient (deterministic fixed-order
//       reductions) + size-adaptive bins + ~deadline wall-clock guard.
//
// The scorer (scorer/lib/{bookshelf,placement,legalize}.py) is the sole
// arbiter: lower-left coords, pin global = (cell.x+xoff, cell.y+yoff),
// unweighted HPWL, anti-collapse health check (legalize avg disp <= 0.05*core).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <omp.h>

using std::vector;
using std::string;

// ----------------------------------------------------------------------------
// Global state (single translation unit, flat SoA, index-based hot loops).
// ----------------------------------------------------------------------------
static int    N = 0;                 // number of cells (modules)
static vector<double> CW, CH;        // cell width / height
static vector<double> CX, CY;        // cell center (optimization var for movable)
static vector<uint8_t> CFIX;         // fixed flag (terminal or /FIXED)
static vector<string> CNAME;         // cell name (output only)

// nets in CSR
static int    M = 0;                 // number of nets
static vector<int>    NET_PTR;       // size M+1
static vector<int>    PIN_CELL;      // size P
static vector<double> PIN_POX, PIN_POY; // pin offset relative to CENTER = (xoff - w/2, yoff - h/2)

// movable index lists
static int    Mov = 0;
static vector<int> MOV2CELL;         // movable index -> cell id
static vector<int> CELL2MOV;         // cell id -> movable index (-1 if fixed)

// core / rows
static double XMIN, YMIN, XMAX, YMAX;
static double ROWH = 0;              // common row height
// per-subrow geometry (mirrors scorer legalize.parse_rows), sorted by y.
static vector<double> ROW_Y, ROW_X0, ROW_X1;

// derived sizes
static double COREW, COREH, CORE_AREA, MOV_AREA;

// ----------------------------------------------------------------------------
// Fast file reader.
// ----------------------------------------------------------------------------
static bool readFile(const string& path, vector<char>& buf) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf.resize(sz + 1);
    size_t rd = fread(buf.data(), 1, sz, f);
    buf[rd] = '\0';
    buf.resize(rd + 1);
    fclose(f);
    return true;
}

static inline bool isWS(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

// Iterate "data lines": skip blank, '#' comment, and 'UCLA' header lines
// (mirrors bookshelf._data_lines). Calls fn(line_start, line_end).
template <class F>
static void eachDataLine(vector<char>& buf, F fn) {
    char* p = buf.data();
    char* end = buf.data() + buf.size() - 1; // exclude trailing '\0'
    while (p < end) {
        char* ls = p;
        while (p < end && *p != '\n') ++p;
        char* le = p;
        if (p < end) ++p; // skip newline
        // strip leading ws
        char* s = ls;
        while (s < le && isWS(*s)) ++s;
        if (s >= le) continue;                 // blank
        if (*s == '#') continue;               // comment
        if (le - s >= 4 && strncmp(s, "UCLA", 4) == 0) continue; // header
        fn(s, le);
    }
}

// token scan helpers over [s,e)
struct Tok { char* p; char* e; };
static inline char* nextToken(char*& s, char* e) {
    while (s < e && isWS(*s)) ++s;
    if (s >= e) return nullptr;
    char* t = s;
    while (s < e && !isWS(*s)) ++s;
    return t; // token = [t, s)
}

// ----------------------------------------------------------------------------
// Parsing.
// ----------------------------------------------------------------------------
static std::unordered_map<string, int> NAME2ID;

static void parseAux(const string& auxPath, std::unordered_map<string,string>& files) {
    vector<char> buf;
    if (!readFile(auxPath, buf)) { fprintf(stderr, "cannot read aux %s\n", auxPath.c_str()); exit(2); }
    // base dir
    string dir;
    {
        size_t pos = auxPath.find_last_of("/\\");
        dir = (pos == string::npos) ? "." : auxPath.substr(0, pos);
    }
    // tokens (replace ':' with space)
    string content(buf.begin(), buf.end() - 1);
    for (char& c : content) if (c == ':') c = ' ';
    size_t i = 0, n = content.size();
    while (i < n) {
        while (i < n && isWS(content[i])) ++i;
        size_t j = i;
        while (j < n && !isWS(content[j])) ++j;
        if (j > i) {
            string tok = content.substr(i, j - i);
            size_t dot = tok.find_last_of('.');
            if (dot != string::npos) {
                string ext = tok.substr(dot + 1);
                for (char& c : ext) c = (char)tolower(c);
                files[ext] = dir + "/" + tok;
            }
        }
        i = j;
    }
}

static void parseNodes(const string& path) {
    vector<char> buf;
    if (!readFile(path, buf)) { fprintf(stderr, "cannot read nodes %s\n", path.c_str()); exit(2); }
    // first pass: count
    NAME2ID.reserve(60000);
    eachDataLine(buf, [&](char* s, char* e) {
        char* t = nextToken(s, e);
        if (!t) return;
        size_t tl = s - t;
        if ((tl == 8 && strncmp(t, "NumNodes", 8) == 0) ||
            (tl == 12 && strncmp(t, "NumTerminals", 12) == 0)) return;
        char* wt = nextToken(s, e); if (!wt) return;
        double w = strtod(wt, nullptr);
        char* ht = nextToken(s, e); if (!ht) return;
        double h = strtod(ht, nullptr);
        char* tm = nextToken(s, e);
        bool term = false;
        if (tm) { // prefix "terminal"
            size_t l = s - tm;
            if (l >= 8 && strncmp(tm, "terminal", 8) == 0) term = true;
            else if (l >= 8 && strncmp(tm, "Terminal", 8) == 0) term = true;
        }
        string name(t, tl);
        int id = (int)CNAME.size();
        NAME2ID.emplace(name, id);
        CNAME.push_back(std::move(name));
        CW.push_back(w); CH.push_back(h);
        CFIX.push_back(term ? 1 : 0);
        CX.push_back(0); CY.push_back(0);
    });
    N = (int)CNAME.size();
}

static void parsePL(const string& path) {
    vector<char> buf;
    if (!readFile(path, buf)) { fprintf(stderr, "cannot read pl %s\n", path.c_str()); exit(2); }
    eachDataLine(buf, [&](char* s, char* e) {
        char* t = nextToken(s, e); if (!t) return;
        size_t tl = s - t;
        char* xt = nextToken(s, e); if (!xt) return;
        char* yt = nextToken(s, e); if (!yt) return;
        // x, y must be numbers
        char* xend; double x = strtod(xt, &xend);
        if (xend == xt) return;
        char* yend; double y = strtod(yt, &yend);
        if (yend == yt) return;
        // remaining tokens: look for FIXED (leading '/' stripped, any case)
        bool fixed = false;
        char* r = s;
        while (true) {
            char* tk = nextToken(r, e);
            if (!tk) break;
            char* te = r;
            char* a = tk; if (*a == '/') ++a;
            size_t l = te - a;
            if (l == 5 &&
                (a[0]=='F'||a[0]=='f') && (a[1]=='I'||a[1]=='i') && (a[2]=='X'||a[2]=='x') &&
                (a[3]=='E'||a[3]=='e') && (a[4]=='D'||a[4]=='d')) { fixed = true; }
        }
        auto it = NAME2ID.find(string(t, tl));
        if (it == NAME2ID.end()) return;
        int id = it->second;
        if (fixed) CFIX[id] = 1;
        // store center for fixed cells (their pinned position); movable will be re-init.
        if (CFIX[id]) {
            CX[id] = x + CW[id] * 0.5;
            CY[id] = y + CH[id] * 0.5;
        } else {
            CX[id] = x + CW[id] * 0.5;
            CY[id] = y + CH[id] * 0.5;
        }
    });
}

static void parseNets(const string& path) {
    vector<char> buf;
    if (!readFile(path, buf)) { fprintf(stderr, "cannot read nets %s\n", path.c_str()); exit(2); }
    NET_PTR.clear(); NET_PTR.push_back(0);
    PIN_CELL.reserve(200000); PIN_POX.reserve(200000); PIN_POY.reserve(200000);
    bool inNet = false;
    eachDataLine(buf, [&](char* s, char* e) {
        char* t = nextToken(s, e); if (!t) return;
        size_t tl = s - t;
        if (tl == 7 && strncmp(t, "NumNets", 7) == 0) return;
        if (tl == 7 && strncmp(t, "NumPins", 7) == 0) return;
        if (tl == 9 && strncmp(t, "NetDegree", 9) == 0) {
            // close previous net
            if (inNet) NET_PTR.push_back((int)PIN_CELL.size());
            inNet = true;
            return;
        }
        if (!inNet) return;
        // pin line: <node> <I/O> : <xoff> <yoff>
        auto it = NAME2ID.find(string(t, tl));
        if (it == NAME2ID.end()) return; // unknown node — skip
        int id = it->second;
        double xoff = 0, yoff = 0;
        // find ':' then two numbers
        char* r = s;
        char* colon = nullptr;
        // scan remaining tokens; locate the ":" token
        while (true) {
            char* tk = nextToken(r, e);
            if (!tk) break;
            char* te = r;
            if (te - tk == 1 && *tk == ':') { colon = r; break; }
        }
        if (colon) {
            char* x1 = nextToken(r, e);
            if (x1) { xoff = strtod(x1, nullptr);
                char* y1 = nextToken(r, e);
                if (y1) yoff = strtod(y1, nullptr);
            }
        }
        PIN_CELL.push_back(id);
        // offset relative to center: (xoff - w/2, yoff - h/2)
        PIN_POX.push_back(xoff - CW[id] * 0.5);
        PIN_POY.push_back(yoff - CH[id] * 0.5);
    });
    if (inNet) NET_PTR.push_back((int)PIN_CELL.size());
    M = (int)NET_PTR.size() - 1;
}

static void parseSCL(const string& path) {
    vector<char> buf;
    if (!readFile(path, buf)) { fprintf(stderr, "cannot read scl %s\n", path.c_str()); exit(2); }
    double coord = 0, height = 0, spacing = 0, sitewidth = 0;
    bool hasC = false, hasH = false, hasSp = false, hasSw = false;
    double xmin = 1e300, ymin = 1e300, xmax = -1e300, ymax = -1e300;
    bool any = false;
    vector<double> heights;
    eachDataLine(buf, [&](char* s, char* e) {
        char* t = nextToken(s, e); if (!t) return;
        size_t tl = s - t;
        auto keyEq = [&](const char* k, size_t kl){ return tl == kl && strncmp(t, k, kl) == 0; };
        if (keyEq("Coordinate", 10)) {
            // Coordinate : val
            char* c = nextToken(s, e); // ':'
            char* v = nextToken(s, e);
            if (v) { coord = strtod(v, nullptr); hasC = true; }
        } else if (keyEq("Height", 6)) {
            char* c = nextToken(s, e); char* v = nextToken(s, e);
            if (v) { height = strtod(v, nullptr); hasH = true; }
        } else if (keyEq("Sitespacing", 11)) {
            char* c = nextToken(s, e); char* v = nextToken(s, e);
            if (v) { spacing = strtod(v, nullptr); hasSp = true; }
        } else if (keyEq("Sitewidth", 9)) {
            char* c = nextToken(s, e); char* v = nextToken(s, e);
            if (v) { sitewidth = strtod(v, nullptr); hasSw = true; }
        } else if (keyEq("SubrowOrigin", 12)) {
            // SubrowOrigin : <x>  NumSites : <n>   (NumSites is last token)
            char* c = nextToken(s, e); // ':'
            char* ot = nextToken(s, e);
            if (!ot) return;
            double origin = strtod(ot, nullptr);
            // last token = nsites
            double nsites = 0;
            char* r = s; char* last = nullptr;
            while (true) { char* tk = nextToken(r, e); if (!tk) break; last = tk; }
            if (last) nsites = strtod(last, nullptr);
            double sp = hasSp ? spacing : (hasSw ? sitewidth : 1.0);
            double x0 = origin, x1 = origin + nsites * sp;
            double y0 = hasC ? coord : 0;
            double y1 = y0 + (hasH ? height : 0);
            if (x0 < xmin) xmin = x0;
            if (x1 > xmax) xmax = x1;
            if (y0 < ymin) ymin = y0;
            if (y1 > ymax) ymax = y1;
            if (hasH) heights.push_back(height);
            ROW_Y.push_back(y0); ROW_X0.push_back(x0); ROW_X1.push_back(x1);
            any = true;
        }
    });
    if (!any) { fprintf(stderr, "no rows parsed from scl\n"); exit(2); }
    XMIN = xmin; YMIN = ymin; XMAX = xmax; YMAX = ymax;
    // common row height = mode/median; use the most frequent (here: median is robust)
    if (!heights.empty()) {
        std::sort(heights.begin(), heights.end());
        ROWH = heights[heights.size() / 2];
    } else ROWH = height;
}

// ----------------------------------------------------------------------------
// Exact scorer HPWL: pin global = (cx + pox, cy + poy); sum (max-min)x+y.
// ----------------------------------------------------------------------------
static double computeHPWL() {
    double total = 0.0;
    #pragma omp parallel for reduction(+:total) schedule(static)
    for (int n = 0; n < M; ++n) {
        int b = NET_PTR[n], en = NET_PTR[n + 1];
        if (en <= b) continue;
        double xmn = 1e300, xmx = -1e300, ymn = 1e300, ymx = -1e300;
        for (int p = b; p < en; ++p) {
            int c = PIN_CELL[p];
            double px = CX[c] + PIN_POX[p];
            double py = CY[c] + PIN_POY[p];
            if (px < xmn) xmn = px; if (px > xmx) xmx = px;
            if (py < ymn) ymn = py; if (py > ymx) ymx = py;
        }
        total += (xmx - xmn) + (ymx - ymn);
    }
    return total;
}

// ----------------------------------------------------------------------------
// Row legalizer (faithful port of scorer/lib/legalize.py single-row Tetris).
// Returns the normalized average displacement (avg_disp / min(coreW,coreH)),
// which the scorer compares against 0.05. Used to pick the lowest-HPWL placement
// that genuinely legalizes — the real legality metric, not a proxy.
// ----------------------------------------------------------------------------
static void finalizeRows() {
    int n = (int)ROW_Y.size();
    vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [](int a, int b){ return ROW_Y[a] < ROW_Y[b]; });
    vector<double> y(n), x0(n), x1(n);
    for (int i = 0; i < n; ++i) { y[i] = ROW_Y[idx[i]]; x0[i] = ROW_X0[idx[i]]; x1[i] = ROW_X1[idx[i]]; }
    ROW_Y.swap(y); ROW_X0.swap(x0); ROW_X1.swap(x1);
}

static vector<int> LG_ORDER;     // scratch (movable order by x)
static vector<double> LG_CUR;    // scratch (per-row cursor)

static double legalizeDispNorm(double minside) {
    int nrow = (int)ROW_Y.size();
    if (nrow == 0 || Mov == 0) return 0.0;
    LG_CUR = ROW_X0;                                 // reset cursors
    LG_ORDER.resize(Mov);
    for (int k = 0; k < Mov; ++k) LG_ORDER[k] = k;
    std::sort(LG_ORDER.begin(), LG_ORDER.end(), [](int a, int b){
        int ia = MOV2CELL[a], ib = MOV2CELL[b];
        return (CX[ia] - CW[ia] * 0.5) < (CX[ib] - CW[ib] * 0.5);
    });
    const double INF = 1e300;
    const double abortDisp = 0.15 * minside;
    double sum = 0.0; int cnt = 0;
    for (int oi = 0; oi < Mov; ++oi) {
        int i = MOV2CELL[LG_ORDER[oi]];
        double w = CW[i];
        double tx = CX[i] - w * 0.5;
        double ty = CY[i] - CH[i] * 0.5;
        // ri0 = first row with y >= ty
        int ri0 = (int)(std::lower_bound(ROW_Y.begin(), ROW_Y.end(), ty) - ROW_Y.begin());
        if (ri0 >= nrow) ri0 = nrow - 1;
        double best_disp = INF; int best_ri = -1; double best_px = 0;
        int radius = 0;
        while (radius < nrow) {
            int cand[2]; int nc = 0;
            if (radius == 0) { cand[nc++] = ri0; }
            else { cand[nc++] = ri0 - radius; cand[nc++] = ri0 + radius; }
            for (int ci = 0; ci < nc; ++ci) {
                int ri = cand[ci];
                if (ri < 0 || ri >= nrow) continue;
                double vd = std::fabs(ROW_Y[ri] - ty);
                if (vd >= best_disp) continue;
                double px = LG_CUR[ri] > tx ? LG_CUR[ri] : tx;
                if (px + w > ROW_X1[ri]) continue;   // row full
                double d = std::fabs(px - tx) + vd;
                if (d < best_disp) { best_disp = d; best_ri = ri; best_px = px; }
            }
            int lo = ri0 - radius, hi = ri0 + radius;
            double vlo = (lo >= 0) ? std::fabs(ROW_Y[lo] - ty) : INF;
            double vhi = (hi < nrow) ? std::fabs(ROW_Y[hi] - ty) : INF;
            if (best_ri >= 0 && std::min(vlo, vhi) >= best_disp) break;
            if (lo < 0 && hi >= nrow) break;
            radius++;
        }
        if (best_ri < 0) {                            // no space anywhere: least-full row, overflow
            int ri = 0; double mn = LG_CUR[0];
            for (int r = 1; r < nrow; ++r) if (LG_CUR[r] < mn) { mn = LG_CUR[r]; ri = r; }
            best_ri = ri; best_px = LG_CUR[ri];
        }
        LG_CUR[best_ri] = best_px + w;
        double d = std::fabs(best_px - tx) + std::fabs(ROW_Y[best_ri] - ty);
        sum += d; cnt++;
        if (cnt >= 500 && sum / cnt > abortDisp) return (sum / cnt) / minside; // aborted (collapsed)
    }
    return (sum / std::max(cnt, 1)) / minside;
}

// ----------------------------------------------------------------------------
// Bin grid (adaptive).
// ----------------------------------------------------------------------------
static int    BCX, BCY, NBINS;
static double WB, HB, BIN_AREA, TARGET_DENS;

static void setupBins() {
    // target ~1-2 cells/bin, bins roughly square, support bounded by
    // forcing WB >= avgCellW and HB >= rowHeight (=> kernel touches ~4 bins/axis).
    double avgW = 0;
    for (int k = 0; k < Mov; ++k) avgW += CW[MOV2CELL[k]];
    avgW = (Mov ? avgW / Mov : 1.0);
    double aspect = (COREH > 0) ? (COREW / COREH) : 1.0;
    // Coarse bins act as a smooth GLOBAL spreading field: they inflate the
    // WL-arranged layout to fill the core while preserving relative positions
    // (low HPWL). Fine bins over-equalize locally and scramble the arrangement
    // (confirmed: public3 at 33x33 stalled; the reference uses a fixed 14x14).
    // Keep it coarse and roughly size-independent.
    int nb = std::max(196, std::min(Mov / 120, 400));
    int bcx = (int)std::lround(std::sqrt((double)nb * aspect));
    int bcy = (int)std::lround(std::sqrt((double)nb / aspect));
    bcx = std::max(1, bcx); bcy = std::max(1, bcy);
    // bound support
    if (COREW / bcx < avgW && avgW > 0)  bcx = std::max(1, (int)(COREW / avgW));
    if (COREH / bcy < ROWH && ROWH > 0)  bcy = std::max(1, (int)(COREH / ROWH));
    // cap total
    while ((long)bcx * bcy > 250000) { if (bcx > bcy) bcx = bcx * 9 / 10; else bcy = bcy * 9 / 10; }
    BCX = std::max(1, bcx); BCY = std::max(1, bcy);
    NBINS = BCX * BCY;
    WB = COREW / BCX; HB = COREH / BCY;
    BIN_AREA = WB * HB;
    TARGET_DENS = MOV_AREA / CORE_AREA; // utilization
}

// bell-shaped theta + derivative along one axis.
// returns theta; sets dtheta (d theta / d coord).
static inline double thetaAxis(double d, double Wi, double Wb, double aX, double bX, double& dtheta) {
    double ad = std::fabs(d);
    double half = Wi * 0.5 + Wb * 0.5;
    double full = Wi * 0.5 + Wb;
    if (ad <= half) {
        dtheta = -2.0 * aX * d;
        return 1.0 - aX * ad * ad;
    } else if (ad <= full) {
        double k = Wb + Wi * 0.5;
        if (d > 0) { dtheta = 2.0 * bX * (d - k); return bX * (ad - k) * (ad - k); }
        else       { dtheta = 2.0 * bX * (d + k); return bX * (ad - k) * (ad - k); }
    } else { dtheta = 0.0; return 0.0; }
}

// ----------------------------------------------------------------------------
// Objective / gradient (parallel, deterministic fixed-order reductions).
//   f = f_WL(gamma) + lambda * f_D
//   gradM[2*Mov] = d f / d (movable center)
// Returns f; also outputs f_wl, f_d, overflow (for diagnostics/scheduling).
// ----------------------------------------------------------------------------
static int NT = 1;                       // thread count (fixed)
static vector<double> GLOC;              // NT * 2*Mov  (WL gradient partials)
static vector<double> BLOC;              // NT * NBINS  (density partials)
static vector<double> BINDENS;           // NBINS
static vector<double> GRADM;             // 2*Mov
static vector<double> FLOC;              // NT (f_wl partials)

static void allocWork() {
    GLOC.assign((size_t)NT * 2 * Mov, 0.0);
    GRADM.assign((size_t)2 * Mov, 0.0);
    FLOC.assign(NT, 0.0);
    BINDENS.assign(NBINS, 0.0);
    BLOC.assign((size_t)NT * NBINS, 0.0);
}

static double evalFG(double gamma, double lambda,
                     double& f_wl_out, double& f_d_out, double& overflow_out) {
    const int twoM = 2 * Mov;
    // ---- Wirelength (LSE with per-net shift), parallel over nets ----
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        double* gl = GLOC.data() + (size_t)tid * twoM;
        std::fill(gl, gl + twoM, 0.0);
        double lf = 0.0;
        // static schedule: each thread always owns the same nets, so its partial
        // gradient/f is bit-identical run-to-run → deterministic (FR-013).
        #pragma omp for schedule(static) nowait
        for (int n = 0; n < M; ++n) {
            int b = NET_PTR[n], en = NET_PTR[n + 1];
            if (en - b <= 1) continue; // degree<=1 contributes 0
            double Mx = -1e300, mx = 1e300, My = -1e300, my = 1e300;
            for (int p = b; p < en; ++p) {
                int c = PIN_CELL[p];
                double px = CX[c] + PIN_POX[p];
                double py = CY[c] + PIN_POY[p];
                if (px > Mx) Mx = px; if (px < mx) mx = px;
                if (py > My) My = py; if (py < my) my = py;
            }
            double SxA = 0, SxB = 0, SyA = 0, SyB = 0;
            for (int p = b; p < en; ++p) {
                int c = PIN_CELL[p];
                double px = CX[c] + PIN_POX[p];
                double py = CY[c] + PIN_POY[p];
                SxA += std::exp((px - Mx) / gamma);
                SxB += std::exp((mx - px) / gamma);
                SyA += std::exp((py - My) / gamma);
                SyB += std::exp((my - py) / gamma);
            }
            double maxX = Mx + gamma * std::log(SxA);
            double minX = mx - gamma * std::log(SxB);
            double maxY = My + gamma * std::log(SyA);
            double minY = my - gamma * std::log(SyB);
            lf += (maxX - minX) + (maxY - minY);
            // gradient: per pin weights
            for (int p = b; p < en; ++p) {
                int c = PIN_CELL[p];
                int m = CELL2MOV[c];
                if (m < 0) continue;
                double px = CX[c] + PIN_POX[p];
                double py = CY[c] + PIN_POY[p];
                double wmaxX = std::exp((px - Mx) / gamma) / SxA;
                double wminX = std::exp((mx - px) / gamma) / SxB;
                double wmaxY = std::exp((py - My) / gamma) / SyA;
                double wminY = std::exp((my - py) / gamma) / SyB;
                gl[2 * m]     += (wmaxX - wminX);
                gl[2 * m + 1] += (wmaxY - wminY);
            }
        }
        FLOC[tid] = lf;
    }
    // reduce WL gradient (fixed thread order) and f
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < twoM; ++k) {
        double s = 0;
        for (int t = 0; t < NT; ++t) s += GLOC[(size_t)t * twoM + k];
        GRADM[k] = s;
    }
    double f_wl = 0;
    for (int t = 0; t < NT; ++t) f_wl += FLOC[t];

    double f_d = 0.0, overflow = 0.0;
    if (lambda > 0.0) {
        // ---- density scatter (parallel over movable, per-thread bins) ----
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double* bl = BLOC.data() + (size_t)tid * NBINS;
            std::fill(bl, bl + NBINS, 0.0);
            #pragma omp for schedule(static)
            for (int k = 0; k < Mov; ++k) {
                int i = MOV2CELL[k];
                double X = CX[i], Y = CY[i], Wi = CW[i], Hi = CH[i];
                double c = (Wi * Hi) / BIN_AREA;
                double aX = 4.0 / ((WB + Wi) * (2 * WB + Wi));
                double bX = 4.0 / (WB * (2 * WB + Wi));
                double aY = 4.0 / ((HB + Hi) * (2 * HB + Hi));
                double bY = 4.0 / (HB * (2 * HB + Hi));
                double rx = Wi * 0.5 + WB, ry = Hi * 0.5 + HB;
                int a_lo = (int)std::floor((X - XMIN - rx) / WB - 0.5);
                int a_hi = (int)std::ceil((X - XMIN + rx) / WB - 0.5);
                int b_lo = (int)std::floor((Y - YMIN - ry) / HB - 0.5);
                int b_hi = (int)std::ceil((Y - YMIN + ry) / HB - 0.5);
                if (a_lo < 0) a_lo = 0; if (a_hi >= BCX) a_hi = BCX - 1;
                if (b_lo < 0) b_lo = 0; if (b_hi >= BCY) b_hi = BCY - 1;
                for (int a = a_lo; a <= a_hi; ++a) {
                    double bcx = XMIN + (a + 0.5) * WB;
                    double dX = X - bcx, dtx;
                    double tx = thetaAxis(dX, Wi, WB, aX, bX, dtx);
                    if (tx == 0.0) continue;
                    for (int bb = b_lo; bb <= b_hi; ++bb) {
                        double bcy = YMIN + (bb + 0.5) * HB;
                        double dY = Y - bcy, dty;
                        double ty = thetaAxis(dY, Hi, HB, aY, bY, dty);
                        if (ty == 0.0) continue;
                        bl[a + BCX * bb] += c * tx * ty;
                    }
                }
            }
        }
        // reduce bins (fixed order)
        #pragma omp parallel for schedule(static)
        for (int bn = 0; bn < NBINS; ++bn) {
            double s = 0;
            for (int t = 0; t < NT; ++t) s += BLOC[(size_t)t * NBINS + bn];
            BINDENS[bn] = s;
        }
        double totalDens = 0;
        for (int bn = 0; bn < NBINS; ++bn) {
            double bd = BINDENS[bn];
            double diff = bd - TARGET_DENS;
            f_d += diff * diff;                 // symmetric: push toward uniform target
            if (diff > 0) overflow += diff;     // overflow above uniform = non-uniformity measure
            totalDens += bd;
        }
        overflow = (totalDens > 1e-30) ? overflow / totalDens : 0.0; // scale-free fraction
        // ---- density gradient (parallel over movable, unique writes) ----
        #pragma omp parallel for schedule(static)
        for (int k = 0; k < Mov; ++k) {
            int i = MOV2CELL[k];
            double X = CX[i], Y = CY[i], Wi = CW[i], Hi = CH[i];
            double c = (Wi * Hi) / BIN_AREA;
            double aX = 4.0 / ((WB + Wi) * (2 * WB + Wi));
            double bX = 4.0 / (WB * (2 * WB + Wi));
            double aY = 4.0 / ((HB + Hi) * (2 * HB + Hi));
            double bY = 4.0 / (HB * (2 * HB + Hi));
            double rx = Wi * 0.5 + WB, ry = Hi * 0.5 + HB;
            int a_lo = (int)std::floor((X - XMIN - rx) / WB - 0.5);
            int a_hi = (int)std::ceil((X - XMIN + rx) / WB - 0.5);
            int b_lo = (int)std::floor((Y - YMIN - ry) / HB - 0.5);
            int b_hi = (int)std::ceil((Y - YMIN + ry) / HB - 0.5);
            if (a_lo < 0) a_lo = 0; if (a_hi >= BCX) a_hi = BCX - 1;
            if (b_lo < 0) b_lo = 0; if (b_hi >= BCY) b_hi = BCY - 1;
            double gx = 0, gy = 0;
            for (int a = a_lo; a <= a_hi; ++a) {
                double bcx = XMIN + (a + 0.5) * WB;
                double dX = X - bcx, dtx;
                double tx = thetaAxis(dX, Wi, WB, aX, bX, dtx);
                for (int bb = b_lo; bb <= b_hi; ++bb) {
                    double bcy = YMIN + (bb + 0.5) * HB;
                    double dY = Y - bcy, dty;
                    double ty = thetaAxis(dY, Hi, HB, aY, bY, dty);
                    double diff = BINDENS[a + BCX * bb] - TARGET_DENS;
                    double coef = 2.0 * diff * c;            // symmetric: pull toward uniform
                    gx += coef * dtx * ty;
                    gy += coef * tx * dty;
                }
            }
            GRADM[2 * k]     += lambda * gx;
            GRADM[2 * k + 1] += lambda * gy;
        }
    }
    f_wl_out = f_wl; f_d_out = f_d; overflow_out = overflow;
    return f_wl + lambda * f_d;
}

// ----------------------------------------------------------------------------
// Clamp movable centers so lower-left bbox stays inside core (FR-004).
// ----------------------------------------------------------------------------
static void clampAll() {
    const double eps = 1e-3;
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < Mov; ++k) {
        int i = MOV2CELL[k];
        double w = CW[i], h = CH[i];
        double lo = XMIN + w * 0.5 + eps, hi = XMAX - w * 0.5 - eps;
        if (lo > hi) { CX[i] = 0.5 * (XMIN + XMAX); } else { if (CX[i] < lo) CX[i] = lo; else if (CX[i] > hi) CX[i] = hi; }
        double loy = YMIN + h * 0.5 + eps, hiy = YMAX - h * 0.5 - eps;
        if (loy > hiy) { CY[i] = 0.5 * (YMIN + YMAX); } else { if (CY[i] < loy) CY[i] = loy; else if (CY[i] > hiy) CY[i] = hiy; }
    }
}

// ----------------------------------------------------------------------------
// Output writer (.gp.pl), lower-left coords, movable cells.
// ----------------------------------------------------------------------------
static void writeOutput(const string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write output %s\n", path.c_str()); exit(2); }
    string out;
    out.reserve((size_t)Mov * 40 + 32);
    out += "UCLA pl 1.0\n\n";
    char line[256];
    for (int k = 0; k < Mov; ++k) {
        int i = MOV2CELL[k];
        double x = CX[i] - CW[i] * 0.5;
        double y = CY[i] - CH[i] * 0.5;
        int len = snprintf(line, sizeof(line), "%s %.4f %.4f : N\n", CNAME[i].c_str(), x, y);
        out.append(line, len);
    }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
}

// ----------------------------------------------------------------------------
// Main solve.
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    double t0 = omp_get_wtime();
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.aux> <output.gp.pl>\n", argv[0]);
        return 1;
    }
    const double DEADLINE = 560.0; // wall-clock guard (s)

    // ---- parse ----
    std::unordered_map<string,string> files;
    parseAux(argv[1], files);
    auto need = [&](const char* ext) -> string {
        auto it = files.find(ext);
        if (it == files.end()) { fprintf(stderr, "aux missing .%s\n", ext); exit(2); }
        return it->second;
    };
    parseNodes(need("nodes"));
    parsePL(need("pl"));
    parseNets(need("nets"));
    parseSCL(need("scl"));

    // ---- movable lists ----
    CELL2MOV.assign(N, -1);
    for (int i = 0; i < N; ++i) {
        if (!CFIX[i]) { CELL2MOV[i] = (int)MOV2CELL.size(); MOV2CELL.push_back(i); }
    }
    Mov = (int)MOV2CELL.size();
    finalizeRows();

    COREW = XMAX - XMIN; COREH = YMAX - YMIN; CORE_AREA = COREW * COREH;
    MOV_AREA = 0;
    for (int k = 0; k < Mov; ++k) MOV_AREA += CW[MOV2CELL[k]] * CH[MOV2CELL[k]];

    NT = omp_get_max_threads();
    if (NT < 1) NT = 1;

    fprintf(stderr, "[gp] cells=%d nets=%d pins=%d movable=%d rows-h=%.0f core=[%.0f,%.0f]x[%.0f,%.0f] util=%.3f threads=%d\n",
            N, M, (int)PIN_CELL.size(), Mov, ROWH, XMIN, XMAX, YMIN, YMAX, MOV_AREA / CORE_AREA, NT);

    if (Mov == 0) { writeOutput(argv[2]); return 0; }

    // ---- L1: constructive legal spread (uniform grid over core) ----
    {
        int G = (int)std::ceil(std::sqrt((double)Mov));
        if (G < 1) G = 1;
        double spanx = COREW, spany = COREH;
        for (int k = 0; k < Mov; ++k) {
            int i = MOV2CELL[k];
            int gx = k % G, gy = k / G;
            CX[i] = XMIN + (gx + 0.5) / G * spanx;
            CY[i] = YMIN + (gy + 0.5) / G * spany;
        }
        clampAll();
    }

    setupBins();
    allocWork();
    fprintf(stderr, "[gp] bins=%dx%d=%d Wb=%.1f Hb=%.1f target=%.3f\n",
            BCX, BCY, NBINS, WB, HB, TARGET_DENS);

    // ---- L2/L3: analytical placement (Adam) with a proportional lambda-controller ----
    //
    // Strategy (avoids the collapse trap): the placement STARTS spread (the
    // constructive grid above) and is never collapsed. Adam descends
    // f = WL + lambda*density; a feedback controller adjusts lambda each step to
    // hold the bin-overflow at a small target OVF_SET. WL pulls connected cells
    // together (lowers HPWL) while the density term keeps the layout spread
    // (legal). Minimizing HPWL *subject to* the spread constraint = the lowest
    // legal HPWL. The uniform spread is retained as a guaranteed-legal fallback.
    vector<double> mAd(2 * Mov, 0.0), vAd(2 * Mov, 0.0);
    const double beta1 = 0.9, beta2 = 0.999, adeps = 1e-8;
    int adt = 0;

    double gammaHi = COREW * 0.11, gammaLo = COREW * 0.05;
    double lr0 = COREW * 0.008;     // step scale (length units)
    double gamma = gammaHi;
    double lambda = 0.0;
    double f_wl, f_d, overflow;

    auto adamStep = [&](double lr) {
        adt++;
        double bc1 = 1.0 - std::pow(beta1, adt);
        double bc2 = 1.0 - std::pow(beta2, adt);
        #pragma omp parallel for schedule(static)
        for (int j = 0; j < 2 * Mov; ++j) {
            double g = GRADM[j];
            mAd[j] = beta1 * mAd[j] + (1 - beta1) * g;
            vAd[j] = beta2 * vAd[j] + (1 - beta2) * g * g;
            double mh = mAd[j] / bc1;
            double vh = vAd[j] / bc2;
            double step = lr * mh / (std::sqrt(vh) + adeps);
            int i = MOV2CELL[j >> 1];
            if (j & 1) CY[i] -= step; else CX[i] -= step;
        }
        clampAll();
    };

    auto saveX = [&](vector<double>& dst){ for (int k = 0; k < Mov; ++k){ int i = MOV2CELL[k]; dst[2*k]=CX[i]; dst[2*k+1]=CY[i]; } };
    auto loadX = [&](const vector<double>& src){ for (int k = 0; k < Mov; ++k){ int i = MOV2CELL[k]; CX[i]=src[2*k]; CY[i]=src[2*k+1]; } };

    // --- probe lambda scale: slightly cluster (WL-only) to get nonzero density
    //     gradient, measure ||g_WL|| / ||g_density||, then restore spread. ---
    vector<double> spreadX(2 * Mov);
    saveX(spreadX);
    // --- calibrate the (uniform) density target = mean bin density at the
    //     spread state, so the bell kernel's absolute scale is irrelevant:
    //     the symmetric penalty's minimum is the uniform (legalizable) layout. ---
    TARGET_DENS = 0.0;
    evalFG(gammaHi, 1.0, f_wl, f_d, overflow);
    { double sb = 0; for (int bn = 0; bn < NBINS; ++bn) sb += BINDENS[bn]; TARGET_DENS = sb / NBINS; }
    fprintf(stderr, "[gp] uniform-target bd=%.4f\n", TARGET_DENS);
    for (int it = 0; it < 6; ++it) { evalFG(gammaHi, 0.0, f_wl, f_d, overflow); adamStep(lr0); }
    evalFG(gammaHi, 0.0, f_wl, f_d, overflow);
    double gwl = 0; for (int j = 0; j < 2 * Mov; ++j) gwl += GRADM[j] * GRADM[j];
    gwl = std::sqrt(gwl);
    vector<double> gWLonly(GRADM.begin(), GRADM.end());
    evalFG(gammaHi, 1.0, f_wl, f_d, overflow);
    double gden = 0; for (int j = 0; j < 2 * Mov; ++j){ double d = GRADM[j] - gWLonly[j]; gden += d*d; }
    gden = std::sqrt(gden);
    if (gden < 1e-30) gden = 1e-30;
    loadX(spreadX);                                  // back to fully-spread
    std::fill(mAd.begin(), mAd.end(), 0.0); std::fill(vAd.begin(), vAd.end(), 0.0); adt = 0;
    double balLambda = gwl / gden;                    // WL/density gradient balance
    double lambda0 = 0.03 * balLambda;                // start WL-dominant (cluster by connectivity)
    double lamCap  = 1e5 * balLambda;                 // hard ceiling (freeze normally stops first)
    lambda = lambda0;
    fprintf(stderr, "[gp] probe gwl=%.3e gden=%.3e bal=%.3e lambda0=%.3e spreadHPWL=%.0f\n",
            gwl, gden, balLambda, lambda0, computeHPWL());

    // --- lambda-ramp penalty method (ePlace-style) with freeze-on-legal ---
    // Start WL-dominant (cells cluster by connectivity → low HPWL, non-uniform).
    // Ramp lambda up: the symmetric density term spreads the layout toward
    // uniform (legal) while preserving the arrangement. FREEZE lambda the moment
    // the *real* row legalizer says the placement is legal (densest = lowest
    // HPWL), then polish at fixed lambda. The lowest-HPWL legal snapshot wins;
    // the uniform spread is the guaranteed-legal fallback.
    const double DISP_LIMIT  = 0.043; // snapshot margin under the scorer's 0.05 (~14% headroom)
    const double DISP_FREEZE = 0.040; // freeze lambda once at least this legal
    const int    DISP_EVERY  = 6;     // legalizer cadence
    const int    CLUSTER     = 1800;  // WL-dominant steps: let cells arrange before spreading
    const double GAMMA_STEPS = 2400.0;
    const double rampMul = std::exp(0.0035); // ~ +0.35%/step
    const double minside = std::min(COREW, COREH);
    int step = 0, maxStep = 80000;
    double ovfNorm = 1.0, dispCur = 1.0;
    bool frozen = false, wentPiled = false; int sinceImprove = 0;

    double bestHPWL = computeHPWL();                  // fallback = uniform spread (legal)
    vector<double> bestX = spreadX;
    int bestStep = -1; double bestDisp = legalizeDispNorm(minside);

    while (step < maxStep) {
        if (omp_get_wtime() - t0 > DEADLINE) break;
        // Phase A (step<CLUSTER): hold lambda low, gamma high — WL clusters cells
        // by connectivity into a good (overlapping) arrangement, like the
        // reference's WL-only warmup. Phase B: ramp lambda + anneal gamma to
        // spread back to a legal layout preserving the arrangement.
        double prog = (step < CLUSTER) ? 0.0 : std::min(1.0, (double)(step - CLUSTER) / GAMMA_STEPS);
        gamma = gammaHi + (gammaLo - gammaHi) * prog;
        double lr = lr0 * (1.0 - 0.4 * prog) * (frozen ? 0.5 : 1.0);

        evalFG(gamma, lambda, f_wl, f_d, overflow);
        adamStep(lr);
        ovfNorm = overflow;

        if ((step % DISP_EVERY) == 0 && ovfNorm < 0.9) {
            dispCur = legalizeDispNorm(minside);
            if (!wentPiled && dispCur > 0.12) wentPiled = true;   // WL has clustered first
            if (wentPiled && !frozen && dispCur <= DISP_FREEZE) frozen = true; // densest legal on the way back
            if (dispCur <= DISP_LIMIT) {
                double h = computeHPWL();
                if (h < bestHPWL * (1.0 - 1e-5)) { bestHPWL = h; saveX(bestX); bestStep = step; bestDisp = dispCur; sinceImprove = 0; }
            }
        }
        if (step >= CLUSTER && !frozen && lambda < lamCap) lambda *= rampMul;
        if (frozen && ++sinceImprove > 2500) break;   // converged

        if ((step % 200) == 0) {
            fprintf(stderr, "[gp] step %d gamma=%.0f lam=%.2e ovf=%.3f disp=%.3f HPWL=%.0f best=%.0f(d=%.3f) frz=%d t=%.1f\n",
                    step, gamma, lambda, ovfNorm, dispCur, computeHPWL(), bestHPWL, bestDisp, (int)frozen, omp_get_wtime() - t0);
        }
        step++;
    }

    loadX(bestX);
    clampAll();

    double finalH = computeHPWL();
    fprintf(stderr, "[gp] DONE steps=%d bestStep=%d finalHPWL=%.0f t=%.1f\n",
            step, bestStep, finalH, omp_get_wtime() - t0);

    writeOutput(argv[2]);
    return 0;
}
