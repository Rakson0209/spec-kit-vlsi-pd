// Fixed-Outline Floorplanning — opus self-written solver.
// Pure weighted-HPWL objective (pins at integer-floor centers), grid-free
// rectangle geometry, constructive bottom-left init, weighted-median coordinate
// descent + simulated annealing, OpenMP parallel multi-start.
// Build: g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <omp.h>

using namespace std;

// ----------------------------- Problem (shared, read-only) -----------------------------
struct Problem {
    int W = 0, H = 0;
    int n = 0;          // total modules (soft first, then fixed)
    int nsoft = 0, nfixed = 0;
    vector<string> name;
    vector<char>   isFixed;          // per module
    vector<long long> area;          // soft: min area
    // fixed geometry (also stored in initial state arrays for fixed ids)
    vector<int> fx, fy, fw, fh;      // per module; for soft only meaningful for fixed entries
    vector<vector<pair<int,int>>> shapes; // soft modules: candidate (w,h)
    // nets
    vector<int> na, nb, nw;
    vector<vector<int>> incident;    // module id -> net ids
};

// ----------------------------- Shape generation -----------------------------
// All integer (w,h) with w*h>=A and 0.5<=h/w<=2 (1e-9 tol). Always includes a near-square.
static vector<pair<int,int>> genShapes(long long A) {
    vector<pair<int,int>> res;
    // dedup via a small set
    vector<pair<int,int>> raw;
    auto ratioOK = [](long long w, long long h) {
        double r = (double)h / (double)w;
        return r >= 0.5 - 1e-9 && r <= 2.0 + 1e-9;
    };
    auto add = [&](long long w, long long h) {
        if (w <= 0 || h <= 0) return;
        if ((long long)w * h < A) return;
        if (!ratioOK(w, h)) return;
        raw.push_back({(int)w, (int)h});
    };
    long long wlo = max(1LL, (long long)floor(sqrt((double)A / 2.0)) - 1);
    long long whi = (long long)ceil(sqrt((double)A * 2.0)) + 1;
    long long range = whi - wlo;
    long long step = max(1LL, range / 56);   // cap candidate count ~ <=64
    for (long long w = wlo; w <= whi; w += step) {
        long long h = (A + w - 1) / w;        // ceil(A/w)
        long long hmin = (w + 1) / 2;         // ceil(w/2) -> ratio >= 0.5
        if (h < hmin) h = hmin;
        add(w, h);
    }
    // guaranteed near-square candidates
    long long s = (long long)ceil(sqrt((double)A));
    add(s, (A + s - 1) / s);
    add(s, s);
    add(s + 1, s + 1);
    // dedup
    sort(raw.begin(), raw.end());
    raw.erase(unique(raw.begin(), raw.end()), raw.end());
    if (raw.empty()) {                        // ultimate fallback
        long long w = s, h = s;
        while ((long long)w * h < A) h++;
        raw.push_back({(int)w, (int)h});
    }
    res = move(raw);
    return res;
}

// ----------------------------- Input parsing -----------------------------
static bool parseInput(const string& path, Problem& P) {
    ifstream in(path);
    if (!in) return false;
    // tokenize whole file (whitespace, skip blanks naturally)
    vector<string> tok;
    string w;
    while (in >> w) tok.push_back(w);
    size_t i = 0;
    auto next = [&]() -> string { return (i < tok.size()) ? tok[i++] : string(); };
    auto expect = [&](const char* kw) { string t = next(); (void)t; (void)kw; };

    unordered_map<string,int> id;

    expect("ChipSize");
    P.W = stoi(next()); P.H = stoi(next());

    expect("NumSoftModules");
    int ns = stoi(next());
    P.nsoft = ns;
    vector<string> softNames(ns);
    vector<long long> softArea(ns);
    for (int k = 0; k < ns; ++k) {
        expect("SoftModule");
        softNames[k] = next();
        softArea[k]  = stoll(next());
    }
    expect("NumFixedModules");
    int nf = stoi(next());
    P.nfixed = nf;
    vector<string> fixNames(nf);
    vector<array<int,4>> fixGeo(nf);
    for (int k = 0; k < nf; ++k) {
        expect("FixedModule");
        fixNames[k] = next();
        int x = stoi(next()), y = stoi(next()), ww = stoi(next()), hh = stoi(next());
        fixGeo[k] = {x, y, ww, hh};
    }

    // build id space: soft 0..ns-1, fixed ns..ns+nf-1
    P.n = ns + nf;
    P.name.resize(P.n);
    P.isFixed.assign(P.n, 0);
    P.area.assign(P.n, 0);
    P.fx.assign(P.n, 0); P.fy.assign(P.n, 0); P.fw.assign(P.n, 0); P.fh.assign(P.n, 0);
    P.shapes.assign(P.n, {});
    for (int k = 0; k < ns; ++k) {
        P.name[k] = softNames[k];
        P.area[k] = softArea[k];
        P.shapes[k] = genShapes(softArea[k]);
        id[softNames[k]] = k;
    }
    for (int k = 0; k < nf; ++k) {
        int gid = ns + k;
        P.name[gid] = fixNames[k];
        P.isFixed[gid] = 1;
        P.fx[gid] = fixGeo[k][0]; P.fy[gid] = fixGeo[k][1];
        P.fw[gid] = fixGeo[k][2]; P.fh[gid] = fixGeo[k][3];
        P.area[gid] = (long long)fixGeo[k][2] * fixGeo[k][3];
        id[fixNames[k]] = gid;
    }

    expect("NumNets");
    int nn = stoi(next());
    P.incident.assign(P.n, {});
    for (int k = 0; k < nn; ++k) {
        expect("Net");
        string a = next(), b = next();
        int wt = stoi(next());
        auto ia = id.find(a), ib = id.find(b);
        if (ia == id.end() || ib == id.end()) continue; // unknown ref: scorer would flag; skip safely
        int A = ia->second, B = ib->second;
        int nid = (int)P.na.size();
        P.na.push_back(A); P.nb.push_back(B); P.nw.push_back(wt);
        P.incident[A].push_back(nid);
        if (B != A) P.incident[B].push_back(nid);
    }
    return true;
}

// ----------------------------- State (per chain) -----------------------------
struct State {
    vector<int> x, y, w, h;   // size n
    vector<int> cx, cy;       // centers (floor)
    long long wl = 0;
};

static inline int centerX(int x, int w) { return x + w / 2; }
static inline int centerY(int y, int h) { return y + h / 2; }

// Initialize fixed modules into a state (constant), soft left as 0-size placeholder.
static State makeBaseState(const Problem& P) {
    State s;
    s.x.assign(P.n, 0); s.y.assign(P.n, 0);
    s.w.assign(P.n, 0); s.h.assign(P.n, 0);
    s.cx.assign(P.n, 0); s.cy.assign(P.n, 0);
    for (int i = 0; i < P.n; ++i) {
        if (P.isFixed[i]) {
            s.x[i] = P.fx[i]; s.y[i] = P.fy[i];
            s.w[i] = P.fw[i]; s.h[i] = P.fh[i];
            s.cx[i] = centerX(s.x[i], s.w[i]);
            s.cy[i] = centerY(s.y[i], s.h[i]);
        }
    }
    return s;
}

static inline bool overlapRect(int ax, int ay, int aw, int ah,
                               int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

// ----------------------------- WL -----------------------------
static long long computeWL(const Problem& P, const State& s) {
    long long wl = 0;
    for (size_t k = 0; k < P.na.size(); ++k) {
        int a = P.na[k], b = P.nb[k];
        wl += (long long)P.nw[k] * (llabs((long long)s.cx[a] - s.cx[b]) +
                                    llabs((long long)s.cy[a] - s.cy[b]));
    }
    return wl;
}

// legality of placing soft module i at (x,y,w,h) — checks outline + overlap vs all others
static bool legalAt(const Problem& P, const State& s, int i,
                    int x, int y, int w, int h, int ignore = -1) {
    if (x < 0 || y < 0 || x + w > P.W || y + h > P.H) return false;
    for (int k = 0; k < P.n; ++k) {
        if (k == i || k == ignore) continue;
        if (s.w[k] == 0 && s.h[k] == 0 && !P.isFixed[k]) continue; // unplaced soft
        if (overlapRect(x, y, w, h, s.x[k], s.y[k], s.w[k], s.h[k])) return false;
    }
    return true;
}

// ----------------------------- Constructive bottom-left packing -----------------------------
// Returns true if all soft modules placed legally.
static bool constructivePack(const Problem& P, State& s, const vector<int>& order,
                             mt19937& rng, bool randomizeShape) {
    // reset soft modules to unplaced
    for (int i = 0; i < P.nsoft; ++i) { s.w[i] = 0; s.h[i] = 0; }

    for (int idx : order) {
        int i = idx;
        // candidate contact points from already-placed modules
        vector<int> xs, ys;
        xs.push_back(0); ys.push_back(0);
        for (int k = 0; k < P.n; ++k) {
            bool placed = P.isFixed[k] || (k < P.nsoft && s.w[k] > 0);
            if (!placed || k == i) continue;
            xs.push_back(s.x[k] + s.w[k]);
            ys.push_back(s.y[k] + s.h[k]);
            xs.push_back(s.x[k]);
            ys.push_back(s.y[k]);
        }
        sort(xs.begin(), xs.end()); xs.erase(unique(xs.begin(), xs.end()), xs.end());
        sort(ys.begin(), ys.end()); ys.erase(unique(ys.begin(), ys.end()), ys.end());

        // shape preference: near-square first (sorted by |ratio-1|)
        vector<pair<int,int>> shp = P.shapes[i];
        if (randomizeShape) {
            shuffle(shp.begin(), shp.end(), rng);
        } else {
            sort(shp.begin(), shp.end(), [](const pair<int,int>& a, const pair<int,int>& b){
                double ra = fabs((double)a.second/a.first - 1.0);
                double rb = fabs((double)b.second/b.first - 1.0);
                return ra < rb;
            });
        }
        if ((int)shp.size() > 12) shp.resize(12);

        bool placed = false;
        // bottom-left: lowest y, then lowest x, then preferred shape
        for (int yy : ys) {
            if (placed) break;
            for (int xx : xs) {
                if (placed) break;
                for (auto& sh : shp) {
                    int ww = sh.first, hh = sh.second;
                    if (legalAt(P, s, i, xx, yy, ww, hh)) {
                        s.x[i] = xx; s.y[i] = yy; s.w[i] = ww; s.h[i] = hh;
                        placed = true;
                        break;
                    }
                }
            }
        }
        if (!placed) return false;
    }
    // set centers
    for (int i = 0; i < P.nsoft; ++i) {
        s.cx[i] = centerX(s.x[i], s.w[i]);
        s.cy[i] = centerY(s.y[i], s.h[i]);
    }
    s.wl = computeWL(P, s);
    return true;
}

// Try several orderings until one packs. Guarantees a legal start (or false if truly infeasible).
static bool robustInit(const Problem& P, State& s, mt19937& rng, int variant) {
    vector<int> soft(P.nsoft);
    for (int i = 0; i < P.nsoft; ++i) soft[i] = i;

    auto byAreaDesc = soft;
    sort(byAreaDesc.begin(), byAreaDesc.end(),
         [&](int a, int b){ return P.area[a] > P.area[b]; });
    auto byAreaAsc = byAreaDesc; reverse(byAreaAsc.begin(), byAreaAsc.end());
    auto byMaxDim = soft;
    sort(byMaxDim.begin(), byMaxDim.end(), [&](int a, int b){
        return P.shapes[a][0].first > P.shapes[b][0].first; });

    // ordered list of strategies; variant selects a starting one
    vector<pair<vector<int>,bool>> strategies = {
        {byAreaDesc, false},
        {byMaxDim,  false},
        {byAreaAsc, false},
    };
    // variant>0: try a shuffled order with randomized shapes
    if (variant > 0) {
        vector<int> shuf = byAreaDesc;
        // keep large ones roughly first but jitter
        for (int t = 0; t < variant && (int)shuf.size() > 2; ++t) {
            int a = rng() % shuf.size(), b = rng() % shuf.size();
            swap(shuf[a], shuf[b]);
        }
        strategies.insert(strategies.begin(), {shuf, (variant % 2) == 1});
    }

    for (auto& [ord, rs] : strategies) {
        if (constructivePack(P, s, ord, rng, rs)) return true;
    }
    // last resort: many random tries
    for (int t = 0; t < 200; ++t) {
        vector<int> ord = soft;
        shuffle(ord.begin(), ord.end(), rng);
        if (constructivePack(P, s, ord, rng, (t % 2) == 0)) return true;
    }
    return false;
}

// ----------------------------- Weighted median -----------------------------
static int weightedMedian(vector<pair<int,long long>>& vw) {
    if (vw.empty()) return INT_MIN;
    sort(vw.begin(), vw.end());
    long long total = 0;
    for (auto& p : vw) total += p.second;
    long long half = total / 2;
    long long acc = 0;
    for (auto& p : vw) { acc += p.second; if (acc * 2 >= total) return p.first; }
    (void)half;
    return vw.back().first;
}

// delta WL if soft module i moves to new (x,y,w,h)  (only i moves)
static long long deltaMoveOne(const Problem& P, const State& s, int i,
                              int nx, int ny, int nw, int nh) {
    int ncx = centerX(nx, nw), ncy = centerY(ny, nh);
    long long delta = 0;
    for (int nid : P.incident[i]) {
        int a = P.na[nid], b = P.nb[nid];
        if (a == b) continue;
        int other = (a == i) ? b : a;
        long long oldd = llabs((long long)s.cx[i] - s.cx[other]) +
                         llabs((long long)s.cy[i] - s.cy[other]);
        long long newd = llabs((long long)ncx - s.cx[other]) +
                         llabs((long long)ncy - s.cy[other]);
        delta += (long long)P.nw[nid] * (newd - oldd);
    }
    return delta;
}

static inline void applyMoveOne(State& s, int i, int nx, int ny, int nw, int nh, long long delta) {
    s.x[i] = nx; s.y[i] = ny; s.w[i] = nw; s.h[i] = nh;
    s.cx[i] = centerX(nx, nw); s.cy[i] = centerY(ny, nh);
    s.wl += delta;
}

// Build contact-point candidate positions (excluding module i).
static void contactPoints(const Problem& P, const State& s, int i,
                          vector<int>& xs, vector<int>& ys) {
    xs.clear(); ys.clear();
    xs.push_back(0); ys.push_back(0);
    for (int k = 0; k < P.n; ++k) {
        if (k == i) continue;
        if (k < P.nsoft && s.w[k] == 0) continue;
        xs.push_back(s.x[k] + s.w[k]);
        ys.push_back(s.y[k] + s.h[k]);
        xs.push_back(s.x[k]);
        ys.push_back(s.y[k]);
    }
    sort(xs.begin(), xs.end()); xs.erase(unique(xs.begin(), xs.end()), xs.end());
    sort(ys.begin(), ys.end()); ys.erase(unique(ys.begin(), ys.end()), ys.end());
}

// Find best legal position for module i (current shape) closest to desired center,
// among contact points + clamped desired; returns best (x,y) minimizing WL, or false.
static bool relocateToward(const Problem& P, State& s, int i, int desCx, int desCy,
                           int& bestx, int& besty, long long& bestDelta) {
    int w = s.w[i], h = s.h[i];
    vector<int> xs, ys;
    contactPoints(P, s, i, xs, ys);
    // also the clamped desired position
    int dx = min(max(desCx - w / 2, 0), P.W - w);
    int dy = min(max(desCy - h / 2, 0), P.H - h);
    xs.push_back(dx); ys.push_back(dy);

    bool found = false;
    bestDelta = 0;
    long long bestScore = 0;
    for (int yy : ys) {
        if (yy < 0 || yy + h > P.H) continue;
        for (int xx : xs) {
            if (xx < 0 || xx + w > P.W) continue;
            if (xx == s.x[i] && yy == s.y[i]) continue;
            if (!legalAt(P, s, i, xx, yy, w, h)) continue;
            long long d = deltaMoveOne(P, s, i, xx, yy, w, h);
            if (!found || d < bestScore) {
                bestScore = d; bestx = xx; besty = yy; found = true;
            }
        }
    }
    if (found) bestDelta = bestScore;
    return found;
}

// One sweep of weighted-median coordinate descent. Returns improvement amount (<=0).
static long long medianDescentSweep(const Problem& P, State& s) {
    long long improved = 0;
    for (int i = 0; i < P.nsoft; ++i) {
        // weighted median of neighbor centers
        vector<pair<int,long long>> vx, vy;
        for (int nid : P.incident[i]) {
            int a = P.na[nid], b = P.nb[nid];
            if (a == b) continue;
            int other = (a == i) ? b : a;
            vx.push_back({s.cx[other], P.nw[nid]});
            vy.push_back({s.cy[other], P.nw[nid]});
        }
        if (vx.empty()) continue;
        int mx = weightedMedian(vx);
        int my = weightedMedian(vy);
        int bx, by; long long delta;
        if (relocateToward(P, s, i, mx, my, bx, by, delta) && delta < 0) {
            applyMoveOne(s, i, bx, by, s.w[i], s.h[i], delta);
            improved += delta;
        }
    }
    return improved;
}

// Reshape pass: for each soft module try alternative shapes keeping center near current.
static long long reshapeSweep(const Problem& P, State& s) {
    long long improved = 0;
    for (int i = 0; i < P.nsoft; ++i) {
        int curCx = s.cx[i], curCy = s.cy[i];
        long long bestDelta = 0; int bx = s.x[i], by = s.y[i], bw = s.w[i], bh = s.h[i];
        bool found = false;
        for (auto& sh : P.shapes[i]) {
            int ww = sh.first, hh = sh.second;
            if (ww == s.w[i] && hh == s.h[i]) continue;
            int nx = min(max(curCx - ww / 2, 0), P.W - ww);
            int ny = min(max(curCy - hh / 2, 0), P.H - hh);
            if (nx < 0 || ny < 0) continue;
            if (!legalAt(P, s, i, nx, ny, ww, hh)) continue;
            long long d = deltaMoveOne(P, s, i, nx, ny, ww, hh);
            if (d < bestDelta) { bestDelta = d; bx = nx; by = ny; bw = ww; bh = hh; found = true; }
        }
        if (found && bestDelta < 0) {
            applyMoveOne(s, i, bx, by, bw, bh, bestDelta);
            improved += bestDelta;
        }
    }
    return improved;
}

static void localOpt(const Problem& P, State& s, int maxSweeps) {
    for (int it = 0; it < maxSweeps; ++it) {
        long long imp = medianDescentSweep(P, s);
        imp += reshapeSweep(P, s);
        if (imp == 0) break;
    }
}

// ----------------------------- Simulated annealing -----------------------------
struct Best {
    State st;
    long long wl;
    bool has = false;
};

// swap positions of soft modules i and j (keep own shapes), check legality, metropolis.
static bool trySwap(const Problem& P, State& s, int i, int j, double T, mt19937& rng,
                    uniform_real_distribution<double>& uni) {
    if (i == j) return false;
    int xi = s.x[i], yi = s.y[i], xj = s.x[j], yj = s.y[j];
    int wi = s.w[i], hi = s.h[i], wj = s.w[j], hj = s.h[j];
    // new positions: i -> (xj,yj), j -> (xi,yi) with own shapes; clamp to outline
    int nxi = min(max(xj, 0), P.W - wi), nyi = min(max(yj, 0), P.H - hi);
    int nxj = min(max(xi, 0), P.W - wj), nyj = min(max(yi, 0), P.H - hj);
    if (nxi < 0 || nyi < 0 || nxj < 0 || nyj < 0) return false;
    // legality: place both tentatively, check overlaps ignoring each other appropriately
    // i at new pos vs all except j; j at new pos vs all except i; then i vs j
    // temporarily zero-out by checking manually
    // check i vs everyone except j
    auto okVsAllExcept = [&](int id, int x, int y, int w, int h, int ex) {
        if (x < 0 || y < 0 || x + w > P.W || y + h > P.H) return false;
        for (int k = 0; k < P.n; ++k) {
            if (k == id || k == ex) continue;
            if (k < P.nsoft && s.w[k] == 0) continue;
            if (overlapRect(x, y, w, h, s.x[k], s.y[k], s.w[k], s.h[k])) return false;
        }
        return true;
    };
    if (!okVsAllExcept(i, nxi, nyi, wi, hi, j)) return false;
    if (!okVsAllExcept(j, nxj, nyj, wj, hj, i)) return false;
    if (overlapRect(nxi, nyi, wi, hi, nxj, nyj, wj, hj)) return false;

    // compute delta over union of incident nets of i and j
    int oci_x = s.cx[i], oci_y = s.cy[i], ocj_x = s.cx[j], ocj_y = s.cy[j];
    int nci_x = centerX(nxi, wi), nci_y = centerY(nyi, hi);
    int ncj_x = centerX(nxj, wj), ncj_y = centerY(nyj, hj);
    long long delta = 0;
    // helper to get center of an endpoint under the tentative move
    auto cxOf = [&](int id)->int { return id==i?nci_x : id==j?ncj_x : s.cx[id]; };
    auto cyOf = [&](int id)->int { return id==i?nci_y : id==j?ncj_y : s.cy[id]; };
    // iterate nets of i, then nets of j skipping shared
    auto accNet = [&](int nid) {
        int a = P.na[nid], b = P.nb[nid];
        if (a == b) return;
        long long oldd = llabs((long long)s.cx[a]-s.cx[b]) + llabs((long long)s.cy[a]-s.cy[b]);
        long long newd = llabs((long long)cxOf(a)-cxOf(b)) + llabs((long long)cyOf(a)-cyOf(b));
        delta += (long long)P.nw[nid]*(newd-oldd);
    };
    for (int nid : P.incident[i]) accNet(nid);
    for (int nid : P.incident[j]) {
        int a = P.na[nid], b = P.nb[nid];
        bool touchesI = (a==i||b==i);
        if (touchesI) continue; // already counted
        accNet(nid);
    }
    (void)oci_x;(void)oci_y;(void)ocj_x;(void)ocj_y;
    if (delta <= 0 || uni(rng) < exp(-(double)delta / T)) {
        s.x[i]=nxi; s.y[i]=nyi; s.cx[i]=nci_x; s.cy[i]=nci_y;
        s.x[j]=nxj; s.y[j]=nyj; s.cx[j]=ncj_x; s.cy[j]=ncj_y;
        s.wl += delta;
        return true;
    }
    return false;
}

static void simulatedAnnealing(const Problem& P, State& s, mt19937& rng,
                               long long iters, double deadline, Best& best) {
    uniform_real_distribution<double> uni(0.0, 1.0);
    uniform_int_distribution<int> softPick(0, max(0, P.nsoft - 1));
    if (P.nsoft == 0) return;

    int maxDim = max(P.W, P.H);

    // adaptive T0: sample some random single-module moves
    double T0 = 1.0;
    {
        double acc = 0; int cnt = 0;
        for (int t = 0; t < 200; ++t) {
            int i = softPick(rng);
            int step = max(1, maxDim / 4);
            uniform_int_distribution<int> dd(-step, step);
            int nx = min(max(s.x[i] + dd(rng), 0), P.W - s.w[i]);
            int ny = min(max(s.y[i] + dd(rng), 0), P.H - s.h[i]);
            long long d = deltaMoveOne(P, s, i, nx, ny, s.w[i], s.h[i]);
            acc += llabs(d); cnt++;
        }
        if (cnt) T0 = max(1.0, acc / cnt);
    }
    double Tend = max(1.0, T0 * 1e-4);
    double alpha = pow(Tend / T0, 1.0 / max(1LL, iters));
    double T = T0;

    if (!best.has || s.wl < best.wl) { best.st = s; best.wl = s.wl; best.has = true; }

    long long sinceCheck = 0;
    for (long long it = 0; it < iters; ++it) {
        if (++sinceCheck >= 4096) {
            sinceCheck = 0;
            if (omp_get_wtime() > deadline) break;
        }
        double prog = (double)it / iters;
        int mv = rng() % 100;
        if (mv < 55) {
            // translate
            int i = softPick(rng);
            int step = max(1, (int)(maxDim * (0.30 * (1.0 - prog) + 0.01)));
            uniform_int_distribution<int> dd(-step, step);
            int nx, ny;
            if ((rng() & 3) == 0) {
                // jump to a contact point
                vector<int> xs, ys; contactPoints(P, s, i, xs, ys);
                nx = xs[rng() % xs.size()];
                ny = ys[rng() % ys.size()];
                nx = min(max(nx, 0), P.W - s.w[i]);
                ny = min(max(ny, 0), P.H - s.h[i]);
            } else {
                nx = min(max(s.x[i] + dd(rng), 0), P.W - s.w[i]);
                ny = min(max(s.y[i] + dd(rng), 0), P.H - s.h[i]);
            }
            if (nx == s.x[i] && ny == s.y[i]) continue;
            if (!legalAt(P, s, i, nx, ny, s.w[i], s.h[i])) continue;
            long long d = deltaMoveOne(P, s, i, nx, ny, s.w[i], s.h[i]);
            if (d <= 0 || uni(rng) < exp(-(double)d / T))
                applyMoveOne(s, i, nx, ny, s.w[i], s.h[i], d);
        } else if (mv < 75) {
            // nudge toward weighted median
            int i = softPick(rng);
            vector<pair<int,long long>> vx, vy;
            for (int nid : P.incident[i]) {
                int a = P.na[nid], b = P.nb[nid];
                if (a == b) continue;
                int other = (a == i) ? b : a;
                vx.push_back({s.cx[other], P.nw[nid]});
                vy.push_back({s.cy[other], P.nw[nid]});
            }
            if (vx.empty()) continue;
            int mx = weightedMedian(vx), my = weightedMedian(vy);
            int bx, by; long long delta;
            if (relocateToward(P, s, i, mx, my, bx, by, delta)) {
                if (delta <= 0 || uni(rng) < exp(-(double)delta / T))
                    applyMoveOne(s, i, bx, by, s.w[i], s.h[i], delta);
            }
        } else if (mv < 90) {
            // reshape
            int i = softPick(rng);
            auto& shp = P.shapes[i];
            if (shp.size() < 2) continue;
            auto sh = shp[rng() % shp.size()];
            int ww = sh.first, hh = sh.second;
            if (ww == s.w[i] && hh == s.h[i]) continue;
            int nx = min(max(s.cx[i] - ww / 2, 0), P.W - ww);
            int ny = min(max(s.cy[i] - hh / 2, 0), P.H - hh);
            if (nx < 0 || ny < 0) continue;
            if (!legalAt(P, s, i, nx, ny, ww, hh)) continue;
            long long d = deltaMoveOne(P, s, i, nx, ny, ww, hh);
            if (d <= 0 || uni(rng) < exp(-(double)d / T))
                applyMoveOne(s, i, nx, ny, ww, hh, d);
        } else {
            // swap
            if (P.nsoft >= 2) {
                int i = softPick(rng), j = softPick(rng);
                trySwap(P, s, i, j, T, rng, uni);
            }
        }
        if (s.wl < best.wl) { best.st = s; best.wl = s.wl; best.has = true; }
        T *= alpha;
        if (T < Tend) T = Tend;
    }
    // intensify from best
    s = best.st; s.wl = best.wl;
    localOpt(P, s, 60);
    if (s.wl < best.wl) { best.st = s; best.wl = s.wl; best.has = true; }
}

// ----------------------------- Output -----------------------------
static void writeOutput(const Problem& P, const State& s, const string& path, long long wl) {
    namespace fs = std::filesystem;
    try {
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
    } catch (...) {}
    ofstream out(path);
    out << "Wirelength " << wl << "\n";
    out << "NumSoftModules " << P.nsoft << "\n";
    for (int i = 0; i < P.nsoft; ++i) {
        out << P.name[i] << " " << s.x[i] << " " << s.y[i] << " "
            << s.w[i] << " " << s.h[i] << "\n";
    }
}

// ----------------------------- Main -----------------------------
int main(int argc, char** argv) {
    double tStart = omp_get_wtime();
    if (argc < 3) {
        fprintf(stderr, "usage: %s <input.txt> <output.floorplan>\n", argv[0]);
        return 1;
    }
    string inPath = argv[1], outPath = argv[2];

    Problem P;
    if (!parseInput(inPath, P)) {
        fprintf(stderr, "failed to read input: %s\n", inPath.c_str());
        return 1;
    }

    const double TIME_BUDGET = 585.0;             // wall-clock cap < 600s
    double deadline = tStart + TIME_BUDGET;        // hard SA truncation (safety only)
    double startCutoff = tStart + 555.0;           // never START a restart past this

    // Per-restart SA iteration budget by instance size. With a FIXED restart count this
    // makes the run deterministic (research §9 / FR-015): the result is the min over a fixed
    // set of seeded restarts; the wall-clock deadline only truncates in a rare overflow.
    long long baseIters;
    {
        long long m = P.nsoft;
        if (m <= 4)        baseIters = 2000000;
        else if (m <= 18)  baseIters = 9000000;    // public1/public2
        else if (m <= 24)  baseIters = 4000000;    // public4
        else               baseIters = 2400000;    // public3
    }

    // Fixed restart budget — diversified parallel multi-start (R2). 72 = 6 per core on 12
    // cores; chosen so every case completes well under the deadline (~200-350s measured).
    const int totalRestarts = 72;

    Best global; global.has = false;

    // Guarantee at least one legal solution up-front (seed 0, deterministic).
    {
        mt19937 rng0(12345);
        State s0 = makeBaseState(P);
        if (robustInit(P, s0, rng0, 0)) {
            localOpt(P, s0, 40);
            global.st = s0; global.wl = s0.wl; global.has = true;
        }
    }

    #pragma omp parallel
    {
        Best localBest; localBest.has = false;
        #pragma omp for schedule(dynamic)
        for (int r = 0; r < totalRestarts; ++r) {
            if (omp_get_wtime() > startCutoff) continue;   // safety: skip late restarts
            mt19937 rng(1000003u * (unsigned)(r + 1) + 7u);
            State s = makeBaseState(P);
            if (!robustInit(P, s, rng, (r % 7))) continue;
            localOpt(P, s, 40);
            Best b; b.has = false;
            simulatedAnnealing(P, s, rng, baseIters, deadline, b);
            if (b.has && (!localBest.has || b.wl < localBest.wl)) localBest = b;
        }
        #pragma omp critical
        {
            if (localBest.has && (!global.has || localBest.wl < global.wl))
                global = localBest;
        }
    }

    // Fallback: if somehow nothing legal (should not happen), try harder single-thread.
    if (!global.has) {
        mt19937 rng(999);
        State s = makeBaseState(P);
        if (robustInit(P, s, rng, 5)) {
            localOpt(P, s, 80);
            global.st = s; global.wl = s.wl; global.has = true;
        }
    }

    if (!global.has) {
        fprintf(stderr, "no legal solution found\n");
        return 1;
    }

    // recompute WL exactly (scorer semantics) before writing
    long long finalWL = computeWL(P, global.st);
    writeOutput(P, global.st, outPath, finalWL);
    return 0;
}
