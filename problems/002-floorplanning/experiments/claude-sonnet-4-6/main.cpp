#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <climits>
#include <chrono>
#include <random>

using namespace std;
using Clock = chrono::steady_clock;
using TimePoint = chrono::time_point<Clock>;

// ── Data Structures ──────────────────────────────────────────────────────────

struct Module {
    int id;
    string name;
    int x, y, w, h;
    int min_area;
    bool is_fixed;
    vector<pair<int,int>> shapes; // valid (w,h) pairs
};

struct Net {
    int a, b, weight;
};

struct SequencePair {
    vector<int> pos_x; // Γ+
    vector<int> pos_y; // Γ-
};

// ── Shape Enumeration ─────────────────────────────────────────────────────────

void genShapes(int area, vector<pair<int,int>>& shapes) {
    shapes.clear();
    int w_min = max(1, (int)ceil(sqrt(area / 2.0)));
    int w_max = (int)floor(sqrt(2.0 * area));
    if (w_max < w_min) w_max = w_min;
    int range = w_max - w_min;
    int step = max(1, range / 19);
    for (int w = w_min; w <= w_max; w += step) {
        int h = (int)ceil((double)area / w);
        double r = (double)h / w;
        if (r >= 0.5 - 1e-9 && r <= 2.0 + 1e-9 && w * h >= area)
            shapes.push_back({w, h});
    }
    // Ensure near-square shapes are included
    for (int delta = 0; delta <= 2; delta++) {
        int w = (int)ceil(sqrt((double)area)) + delta;
        if (w < 1) continue;
        int h = (int)ceil((double)area / w);
        double r = (double)h / w;
        if (r >= 0.5 - 1e-9 && r <= 2.0 + 1e-9 && w * h >= area)
            shapes.push_back({w, h});
        // transposed
        if (w != h) {
            r = (double)w / h;
            if (r >= 0.5 - 1e-9 && r <= 2.0 + 1e-9 && w * h >= area)
                shapes.push_back({h, w});
        }
    }
    sort(shapes.begin(), shapes.end());
    shapes.erase(unique(shapes.begin(), shapes.end()), shapes.end());
    shapes.erase(remove_if(shapes.begin(), shapes.end(), [&](const pair<int,int>& p){
        double r = (double)p.second / p.first;
        return p.first * p.second < area || r < 0.5 - 1e-9 || r > 2.0 + 1e-9;
    }), shapes.end());
    if (shapes.empty()) {
        int w = (int)ceil(sqrt((double)area));
        int h = (int)ceil((double)area / w);
        shapes.push_back({w, h});
    }
}

// ── Parser ────────────────────────────────────────────────────────────────────

static vector<string> tokenize(const string& line) {
    vector<string> t; istringstream ss(line); string w;
    while (ss >> w) t.push_back(w);
    return t;
}

void parseInput(const string& path, int& chip_W, int& chip_H,
                vector<Module>& modules, vector<Net>& nets, int& n_soft) {
    ifstream f(path);
    if (!f) { cerr << "Cannot open: " << path << "\n"; exit(1); }
    unordered_map<string,int> nm;
    string line;
    auto next = [&]() -> vector<string> {
        while (getline(f, line)) { auto t = tokenize(line); if (!t.empty()) return t; }
        return {};
    };
    auto t = next();
    chip_W = stoi(t[1]); chip_H = stoi(t[2]);

    t = next(); int ns = stoi(t[1]); n_soft = ns;
    for (int i = 0; i < ns; i++) {
        t = next();
        Module m; m.id = (int)modules.size(); m.name = t[1];
        m.min_area = stoi(t[2]); m.is_fixed = false;
        int sq = max(1, (int)ceil(sqrt((double)m.min_area)));
        m.w = sq; m.h = (int)ceil((double)m.min_area / sq);
        m.x = m.y = 0;
        genShapes(m.min_area, m.shapes);
        nm[m.name] = m.id; modules.push_back(m);
    }
    t = next(); int nf = stoi(t[1]);
    for (int i = 0; i < nf; i++) {
        t = next();
        Module m; m.id = (int)modules.size(); m.name = t[1];
        m.x = stoi(t[2]); m.y = stoi(t[3]); m.w = stoi(t[4]); m.h = stoi(t[5]);
        m.min_area = m.w * m.h; m.is_fixed = true;
        nm[m.name] = m.id; modules.push_back(m);
    }
    t = next(); int nn = stoi(t[1]);
    for (int i = 0; i < nn; i++) {
        t = next();
        Net n; n.a = nm.at(t[1]); n.b = nm.at(t[2]); n.weight = stoi(t[3]);
        nets.push_back(n);
    }
}

// ── Sequence Pair ─────────────────────────────────────────────────────────────

void initSP(int n, SequencePair& sp) {
    sp.pos_x.resize(n); sp.pos_y.resize(n);
    for (int i = 0; i < n; i++) sp.pos_x[i] = sp.pos_y[i] = i;
}

static vector<int> makeRank(const vector<int>& perm) {
    int n = (int)perm.size();
    vector<int> rank(n);
    for (int i = 0; i < n; i++) rank[perm[i]] = i;
    return rank;
}

// Longest path in DAG (Kahn's topological sort + relaxation).
// lb[i] = initial distance (lower bound) for node i.
static vector<int> longestPathLB(int n,
    const vector<vector<pair<int,int>>>& adj,
    const vector<int>& indeg_init,
    const vector<int>& lb) {
    vector<int> indeg = indeg_init;
    vector<int> dist = lb;
    vector<int> q; q.reserve(n);
    for (int i = 0; i < n; i++) if (indeg[i] == 0) q.push_back(i);
    int qi = 0;
    while (qi < (int)q.size()) {
        int u = q[qi++];
        for (int ei = 0; ei < (int)adj[u].size(); ei++) {
            int v = adj[u][ei].first, w = adj[u][ei].second;
            if (dist[u] + w > dist[v]) dist[v] = dist[u] + w;
            if (--indeg[v] == 0) q.push_back(v);
        }
    }
    return dist;
}

// Pack soft modules using sequence pair with iterative fixed-module LB propagation.
// softMods positions are updated in place. Returns (packed_W, packed_H).
pair<int,int> pack(const SequencePair& sp,
                   vector<Module>& softMods,
                   const vector<Module>& fixedMods,
                   int chip_W, int chip_H,
                   int& overflow_W, int& overflow_H) {
    int n = (int)sp.pos_x.size();
    if (n == 0) {
        // Compute bounding box from fixed modules only
        int pw = 0, ph = 0;
        for (const auto& f : fixedMods) { pw = max(pw, f.x+f.w); ph = max(ph, f.y+f.h); }
        overflow_W = max(0, pw - chip_W); overflow_H = max(0, ph - chip_H);
        return {pw, ph};
    }

    auto rankX = makeRank(sp.pos_x);
    auto rankY = makeRank(sp.pos_y);

    // Build HCG: edge i→j (weight w_i) if i is left-of j (before j in both Γ+,Γ-)
    // Build VCG: edge i→j (weight h_i) if i is below j (before in Γ+, after in Γ-)
    vector<vector<pair<int,int>>> hcg(n), vcg(n);
    vector<int> hcg_ind(n, 0), vcg_ind(n, 0);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (rankX[i] < rankX[j] && rankY[i] < rankY[j]) {
                hcg[i].push_back({j, softMods[i].w});
                hcg_ind[j]++;
            }
            if (rankX[i] < rankX[j] && rankY[i] > rankY[j]) {
                vcg[i].push_back({j, softMods[i].h});
                vcg_ind[j]++;
            }
        }
    }

    // Iterative lower-bound propagation for fixed module avoidance
    vector<int> lb_x(n, 0), lb_y(n, 0);
    for (int iter = 0; iter < 8; iter++) {
        // Recompute positions with current lower bounds
        auto dx = longestPathLB(n, hcg, hcg_ind, lb_x);
        auto dy = longestPathLB(n, vcg, vcg_ind, lb_y);
        for (int i = 0; i < n; i++) { softMods[i].x = dx[i]; softMods[i].y = dy[i]; }

        // Check soft-fixed overlaps and update lower bounds
        bool any_new = false;
        for (int i = 0; i < n; i++) {
            const auto& s = softMods[i];
            for (const auto& f : fixedMods) {
                if (s.x < f.x + f.w && f.x < s.x + s.w &&
                    s.y < f.y + f.h && f.y < s.y + s.h) {
                    int push_r = f.x + f.w;
                    int push_u = f.y + f.h;
                    if (push_r - s.x <= push_u - s.y) {
                        if (push_r > lb_x[i]) { lb_x[i] = push_r; any_new = true; }
                    } else {
                        if (push_u > lb_y[i]) { lb_y[i] = push_u; any_new = true; }
                    }
                }
            }
        }
        if (!any_new) break;
    }

    // Compute bounding box
    int pw = 0, ph = 0;
    for (const auto& m : softMods) { pw = max(pw, m.x + m.w); ph = max(ph, m.y + m.h); }
    for (const auto& f : fixedMods) { pw = max(pw, f.x + f.w); ph = max(ph, f.y + f.h); }
    overflow_W = max(0, pw - chip_W);
    overflow_H = max(0, ph - chip_H);
    return {pw, ph};
}

// ── HPWL ─────────────────────────────────────────────────────────────────────
// allMods must contain ALL modules (soft + fixed) indexed by their global ID.

long long computeHPWL(const vector<Module>& allMods, const vector<Net>& nets) {
    long long hpwl = 0;
    for (const auto& net : nets) {
        int cx_a = allMods[net.a].x + allMods[net.a].w / 2;
        int cy_a = allMods[net.a].y + allMods[net.a].h / 2;
        int cx_b = allMods[net.b].x + allMods[net.b].w / 2;
        int cy_b = allMods[net.b].y + allMods[net.b].h / 2;
        hpwl += (long long)net.weight * (abs(cx_a - cx_b) + abs(cy_a - cy_b));
    }
    return hpwl;
}

double computeCost(long long hpwl, int ow, int oh, double lambda) {
    return (double)hpwl + lambda * (ow + oh);
}

// Build combined module list for HPWL: softMods[0..n_soft-1] then fixedMods[0..n_fixed-1]
static vector<Module> combined(const vector<Module>& soft, const vector<Module>& fixed) {
    vector<Module> all = soft;
    all.insert(all.end(), fixed.begin(), fixed.end());
    return all;
}

// ── SA Operators ──────────────────────────────────────────────────────────────

static mt19937 rng(42);

struct UndoSP   { int axis; int i, j; }; // axis: 0=pos_x, 1=pos_y
struct UndoAR   { int idx; int ow, oh; }; // soft module index, old w, old h
struct UndoMove { int val, opx, opy; };   // module value, old positions

UndoSP perturbSwapGP(SequencePair& sp) {
    int n = (int)sp.pos_x.size();
    int i = rng() % n, j = rng() % n;
    while (j == i) j = rng() % n;
    swap(sp.pos_x[i], sp.pos_x[j]);
    return {0, i, j};
}
void undoSwapGP(SequencePair& sp, const UndoSP& u) { swap(sp.pos_x[u.i], sp.pos_x[u.j]); }

UndoSP perturbSwapGM(SequencePair& sp) {
    int n = (int)sp.pos_y.size();
    int i = rng() % n, j = rng() % n;
    while (j == i) j = rng() % n;
    swap(sp.pos_y[i], sp.pos_y[j]);
    return {1, i, j};
}
void undoSwapGM(SequencePair& sp, const UndoSP& u) { swap(sp.pos_y[u.i], sp.pos_y[u.j]); }

UndoMove perturbRotatePair(SequencePair& sp) {
    int n = (int)sp.pos_x.size();
    int mod = rng() % n;
    // Find positions
    int px = 0, py = 0;
    for (int i = 0; i < n; i++) { if (sp.pos_x[i] == mod) px = i; if (sp.pos_y[i] == mod) py = i; }
    UndoMove u{mod, px, py};
    // Remove and re-insert at random positions
    sp.pos_x.erase(sp.pos_x.begin() + px);
    int npx = rng() % n; // n is now size after erase (size was n, now n-1... but we use n range for uniform)
    if (npx >= (int)sp.pos_x.size()) npx = (int)sp.pos_x.size();
    sp.pos_x.insert(sp.pos_x.begin() + npx, mod);
    sp.pos_y.erase(sp.pos_y.begin() + py);
    int npy = rng() % n;
    if (npy >= (int)sp.pos_y.size()) npy = (int)sp.pos_y.size();
    sp.pos_y.insert(sp.pos_y.begin() + npy, mod);
    return u;
}
void undoRotate(SequencePair& sp, const UndoMove& u) {
    int n = (int)sp.pos_x.size();
    // Remove u.val from both sequences
    for (int i = 0; i < n; i++) if (sp.pos_x[i] == u.val) { sp.pos_x.erase(sp.pos_x.begin()+i); break; }
    int ins_x = min(u.opx, (int)sp.pos_x.size());
    sp.pos_x.insert(sp.pos_x.begin() + ins_x, u.val);
    for (int i = 0; i < n; i++) if (sp.pos_y[i] == u.val) { sp.pos_y.erase(sp.pos_y.begin()+i); break; }
    int ins_y = min(u.opy, (int)sp.pos_y.size());
    sp.pos_y.insert(sp.pos_y.begin() + ins_y, u.val);
}

UndoAR perturbResizeAR(vector<Module>& softMods) {
    int n = (int)softMods.size();
    int i = rng() % n;
    if (softMods[i].shapes.size() <= 1) return {-1, 0, 0};
    UndoAR u{i, softMods[i].w, softMods[i].h};
    int idx = rng() % (int)softMods[i].shapes.size();
    for (int t = 0; t < 10 && softMods[i].shapes[idx].first == u.ow && softMods[i].shapes[idx].second == u.oh; t++)
        idx = rng() % (int)softMods[i].shapes.size();
    softMods[i].w = softMods[i].shapes[idx].first;
    softMods[i].h = softMods[i].shapes[idx].second;
    return u;
}
void undoResizeAR(vector<Module>& softMods, const UndoAR& u) {
    if (u.idx < 0) return;
    softMods[u.idx].w = u.ow; softMods[u.idx].h = u.oh;
}

// ── Timing ────────────────────────────────────────────────────────────────────

static bool timeUp(const TimePoint& start, double limit) {
    return chrono::duration<double>(Clock::now() - start).count() >= limit;
}

// ── Temperature Calibration ───────────────────────────────────────────────────

double calibrateTemp(SequencePair sp, vector<Module> softMods,
                     const vector<Module>& fixedMods,
                     const vector<Net>& nets,
                     int chip_W, int chip_H, double lambda) {
    int ow, oh;
    pack(sp, softMods, fixedMods, chip_W, chip_H, ow, oh);
    auto allMods = combined(softMods, fixedMods);
    double prev = computeCost(computeHPWL(allMods, nets), ow, oh, lambda);

    double sum = 0; int cnt = 0;
    for (int iter = 0; iter < 500; iter++) {
        SequencePair sp2 = sp;
        vector<Module> sm2 = softMods;
        int op = rng() % 4;
        if (op == 0)      perturbSwapGP(sp2);
        else if (op == 1) perturbSwapGM(sp2);
        else if (op == 2) perturbRotatePair(sp2);
        else              perturbResizeAR(sm2);
        pack(sp2, sm2, fixedMods, chip_W, chip_H, ow, oh);
        auto all2 = combined(sm2, fixedMods);
        double nc = computeCost(computeHPWL(all2, nets), ow, oh, lambda);
        double d = abs(nc - prev);
        if (d > 0) { sum += d; cnt++; }
    }
    return (cnt > 0 ? sum / cnt : 1e6);
}

// ── Output Writer ─────────────────────────────────────────────────────────────

void writeFloorplan(const string& path, const vector<Module>& softMods, long long hpwl) {
    ofstream f(path);
    f << "Wirelength " << hpwl << "\n\n";
    f << "NumSoftModules " << softMods.size() << "\n";
    for (const auto& m : softMods)
        f << m.name << " " << m.x << " " << m.y << " " << m.w << " " << m.h << "\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 3) { cerr << "Usage: " << argv[0] << " <input.txt> <output.floorplan>\n"; return 1; }
    auto t_start = Clock::now();
    const double TIME_LIMIT = 580.0;

    int chip_W, chip_H, n_soft;
    vector<Module> modules;
    vector<Net> nets;
    parseInput(argv[1], chip_W, chip_H, modules, nets, n_soft);

    vector<Module> softMods(modules.begin(), modules.begin() + n_soft);
    vector<Module> fixedMods(modules.begin() + n_soft, modules.end());

    for (auto& m : softMods) {
        int sq = max(1, (int)ceil(sqrt((double)m.min_area)));
        m.w = sq; m.h = (int)ceil((double)m.min_area / sq);
    }

    SequencePair sp;
    initSP(n_soft, sp);

    int ow, oh;
    pack(sp, softMods, fixedMods, chip_W, chip_H, ow, oh);
    auto allMods = combined(softMods, fixedMods);
    long long init_hpwl = computeHPWL(allMods, nets);

    double lambda = 10.0 * max((double)init_hpwl, 1.0);

    SequencePair best_sp = sp;
    vector<Module> best_soft = softMods;
    long long best_hpwl = (ow == 0 && oh == 0) ? init_hpwl : LLONG_MAX;

    const double T_MIN = 0.1;
    const double COOL = 0.92;
    int N = max(50 * n_soft * n_soft, 10000);

    int restart = 0;
    while (!timeUp(t_start, TIME_LIMIT)) {
        if (restart > 0) {
            if (restart % 2 == 1 && best_hpwl < LLONG_MAX) {
                // Intensification: start from best known solution
                sp = best_sp;
                softMods = best_soft;
            } else {
                // Diversification: random permutation from best
                sp = best_sp;
                shuffle(sp.pos_x.begin(), sp.pos_x.end(), rng);
                shuffle(sp.pos_y.begin(), sp.pos_y.end(), rng);
                for (auto& m : softMods) {
                    int sq = max(1, (int)ceil(sqrt((double)m.min_area)));
                    m.w = sq; m.h = (int)ceil((double)m.min_area / sq);
                }
            }
            pack(sp, softMods, fixedMods, chip_W, chip_H, ow, oh);
        }

        auto allR = combined(softMods, fixedMods);
        double cur_cost = computeCost(computeHPWL(allR, nets), ow, oh, lambda);

        double T = calibrateTemp(sp, softMods, fixedMods, nets, chip_W, chip_H, lambda);
        if (T < 1.0) T = 1e6;

        while (T > T_MIN && !timeUp(t_start, TIME_LIMIT)) {
            for (int iter = 0; iter < N && !timeUp(t_start, TIME_LIMIT); iter++) {
                int op = rng() % 4;

                SequencePair sp_bak = sp;
                vector<Module> sm_bak = softMods;

                if (op == 0)      perturbSwapGP(sp);
                else if (op == 1) perturbSwapGM(sp);
                else if (op == 2) perturbRotatePair(sp);
                else              perturbResizeAR(softMods);

                int new_ow, new_oh;
                pack(sp, softMods, fixedMods, chip_W, chip_H, new_ow, new_oh);
                auto all2 = combined(softMods, fixedMods);
                long long new_hpwl = computeHPWL(all2, nets);
                double new_cost = computeCost(new_hpwl, new_ow, new_oh, lambda);

                double delta = new_cost - cur_cost;
                bool accept = (delta < 0) ||
                    ((double)(rng() % 1000000) / 1000000.0 < exp(-delta / T));

                if (accept) {
                    cur_cost = new_cost;
                    if (new_ow == 0 && new_oh == 0 && new_hpwl < best_hpwl) {
                        best_hpwl = new_hpwl;
                        best_soft = softMods;
                        best_sp = sp;
                    }
                } else {
                    sp = sp_bak;
                    softMods = sm_bak;
                }
            }
            T *= COOL;
        }
        restart++;
    }

    if (best_hpwl == LLONG_MAX) {
        best_soft = softMods;
        auto all2 = combined(softMods, fixedMods);
        best_hpwl = computeHPWL(all2, nets);
    }

    writeFloorplan(argv[2], best_soft, best_hpwl);
    return 0;
}
