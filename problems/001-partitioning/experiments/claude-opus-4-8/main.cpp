// Multi-Technology Die Partitioning (problem 001)
// Self-written solver: fast parser + multilevel (coarsen / initial-partition /
// uncoarsen+FM-refine) + area-constrained Fiduccia-Mattheyses (bucket gains,
// roll-back-to-best) + parallel multi-start (OpenMP). No Boost.
//
// Build: g++ -std=c++20 -O3 -fopenmp -pthread -static -static-libgcc -static-libstdc++ -o hw2 main.cpp
// Run  : hw2 <input.txt> <output.out>
//
// Matches scorer/lib/partitioning.py exactly (constitution R6):
//   legal iff full exclusive coverage and usedX/(W*H) <= utilX + 1e-9 for X in {A,B}
//   cut    = #nets with >=1 cell on A and >=1 cell on B (side area uses that die's Tech).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <random>
#include <filesystem>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using Clock = chrono::steady_clock;

static constexpr double EPS = 1e-9;
static constexpr double DEADLINE_SEC = 285.0;

static Clock::time_point g_start;
static inline double elapsed() { return chrono::duration<double>(Clock::now() - g_start).count(); }

// ----------------------------- string-view hash -----------------------------
struct SVHash {
    size_t operator()(string_view s) const noexcept {
        uint64_t h = 1469598103934665603ull;
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
        return (size_t)h;
    }
};

// ----------------------------- graph (a level) ------------------------------
// A hypergraph level: per-cell side-dependent area + net<->cell CSR. Area caps
// (capA/capB) are absolute (util*dieArea) and identical at every level because
// coarsening preserves total area exactly.
struct Graph {
    int nCells = 0, nNets = 0;
    double capA = 0, capB = 0;
    vector<double> areaA, areaB;
    vector<int> netOff, netIdx;     // net -> cells
    vector<int> cellOff, cellIdx;   // cell -> nets
    int maxDeg = 1;

    void buildCellCSR() {
        vector<int> deg(nCells + 1, 0);
        for (int id : netIdx) deg[id]++;
        cellOff.assign(nCells + 1, 0);
        for (int i = 0; i < nCells; ++i) cellOff[i + 1] = cellOff[i] + deg[i];
        cellIdx.resize(netIdx.size());
        vector<int> cur(cellOff.begin(), cellOff.end() - 1);
        for (int n = 0; n < nNets; ++n)
            for (int k = netOff[n]; k < netOff[n + 1]; ++k)
                cellIdx[cur[netIdx[k]]++] = n;
        int md = 1;
        for (int i = 0; i < nCells; ++i) md = max(md, cellOff[i + 1] - cellOff[i]);
        maxDeg = md;
    }
};

// ----------------------------- fast tokenizer -------------------------------
struct Tokenizer {
    const char* p; const char* end;
    Tokenizer(const char* b, const char* e) : p(b), end(e) {}
    string_view next() {
        while (p < end && (unsigned char)*p <= ' ') ++p;
        if (p >= end) return {};
        const char* s = p;
        while (p < end && (unsigned char)*p > ' ') ++p;
        return string_view(s, (size_t)(p - s));
    }
    long long nextInt() {
        string_view t = next();
        long long v = 0; bool neg = false; size_t i = 0;
        if (!t.empty() && (t[0] == '-' || t[0] == '+')) { neg = t[0] == '-'; i = 1; }
        for (; i < t.size(); ++i) v = v * 10 + (t[i] - '0');
        return neg ? -v : v;
    }
    double nextDouble() { return strtod(string(next()).c_str(), nullptr); }
};

// ----------------------------- parser ---------------------------------------
static bool parseInput(const char* path, vector<char>& buf, Graph& G, vector<string_view>& name) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open input %s\n", path); return false; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf.resize((size_t)sz + 1);
    size_t rd = fread(buf.data(), 1, (size_t)sz, f);
    buf[rd] = '\0'; fclose(f);

    Tokenizer tk(buf.data(), buf.data() + rd);
    tk.next(); // NumTechs
    int numTechs = (int)tk.nextInt();

    unordered_map<string_view, int, SVHash> techIdx; techIdx.reserve(numTechs * 2 + 4);
    vector<unordered_map<string_view, double, SVHash>> techLib(numTechs);
    for (int t = 0; t < numTechs; ++t) {
        tk.next(); // Tech
        string_view tname = tk.next();
        int nlib = (int)tk.nextInt();
        techIdx.emplace(tname, t);
        auto& lib = techLib[t]; lib.reserve(nlib * 2 + 4);
        for (int j = 0; j < nlib; ++j) {
            tk.next(); // LibCell
            string_view lname = tk.next();
            double w = tk.nextDouble(), h = tk.nextDouble();
            lib.emplace(lname, w * h);
        }
    }
    tk.next(); // DieSize
    double W = tk.nextDouble(), H = tk.nextDouble();
    double dieArea = W * H;
    tk.next(); string_view techA = tk.next(); double utilA = tk.nextDouble() / 100.0;
    tk.next(); string_view techB = tk.next(); double utilB = tk.nextDouble() / 100.0;
    int tA = techIdx.at(techA), tB = techIdx.at(techB);
    G.capA = utilA * dieArea; G.capB = utilB * dieArea;

    tk.next(); // NumCells
    int nCells = (int)tk.nextInt();
    G.nCells = nCells;
    G.areaA.resize(nCells); G.areaB.resize(nCells); name.resize(nCells);
    unordered_map<string_view, int, SVHash> cellId; cellId.reserve((size_t)nCells * 2 + 16);
    auto& libA = techLib[tA]; auto& libB = techLib[tB];
    for (int i = 0; i < nCells; ++i) {
        tk.next(); // Cell
        string_view cname = tk.next(); string_view lname = tk.next();
        cellId.emplace(cname, i); name[i] = cname;
        auto ia = libA.find(lname); auto ib = libB.find(lname);
        G.areaA[i] = (ia != libA.end()) ? ia->second : 0.0;
        G.areaB[i] = (ib != libB.end()) ? ib->second : 0.0;
    }
    tk.next(); // NumNets
    int nNetsDecl = (int)tk.nextInt();
    vector<int> seen(nCells, -1);
    G.netOff.reserve(nNetsDecl + 1);
    G.netIdx.reserve((size_t)nNetsDecl * 4);
    G.netOff.push_back(0);
    vector<int> tmp; tmp.reserve(64);
    int netCount = 0;
    for (int nn = 0; nn < nNetsDecl; ++nn) {
        tk.next(); tk.next(); // Net, name
        int deg = (int)tk.nextInt();
        tmp.clear();
        for (int d = 0; d < deg; ++d) {
            tk.next(); // Cell
            string_view cm = tk.next();
            auto it = cellId.find(cm);
            if (it == cellId.end()) continue;
            int id = it->second;
            if (seen[id] == netCount) continue;
            seen[id] = netCount; tmp.push_back(id);
        }
        if ((int)tmp.size() < 2) continue;
        for (int id : tmp) G.netIdx.push_back(id);
        G.netOff.push_back((int)G.netIdx.size());
        ++netCount;
    }
    G.nNets = netCount;
    G.buildCellCSR();
    return true;
}

// ----------------------------- partitioner ----------------------------------
// Area-constrained FM on a given Graph level. Operates on/refines side[].
struct Partitioner {
    const Graph& G;
    int nCells, nNets, off;
    vector<uint8_t> side;
    vector<int> gain;
    vector<uint8_t> state;          // 0=free(in bucket),1=locked
    vector<int> cntA, cntB;
    double usedA = 0, usedB = 0;
    vector<int> bhead, bnext, bprev;
    int maxPtr = 0;
    vector<int> moves;
    vector<uint8_t> bestSide;
    int bestCut = INT32_MAX;
    int bestSeed = INT32_MAX;
    mt19937 rng;

    explicit Partitioner(const Graph& g)
        : G(g), nCells(g.nCells), nNets(g.nNets), off(g.maxDeg) {
        side.resize(nCells); gain.resize(nCells); state.resize(nCells);
        cntA.resize(nNets); cntB.resize(nNets);
        bhead.assign(2 * off + 1, -1); bnext.resize(nCells); bprev.resize(nCells);
        moves.reserve(nCells); bestSide.resize(nCells);
    }

    inline bool fitsA(int c) const { return usedA + G.areaA[c] <= G.capA + EPS; }
    inline bool fitsB(int c) const { return usedB + G.areaB[c] <= G.capB + EPS; }
    bool legal() const { return usedA <= G.capA + EPS && usedB <= G.capB + EPS; }

    void recomputeUsed() {
        usedA = usedB = 0;
        for (int c = 0; c < nCells; ++c) { if (side[c] == 0) usedA += G.areaA[c]; else usedB += G.areaB[c]; }
    }

    void greedyInit(const vector<int>& order, int seed) {
        usedA = usedB = 0;
        bool randomize = (seed != 0);
        uniform_real_distribution<double> U(0.0, 1.0);
        for (int c : order) {
            bool fa = fitsA(c), fb = fitsB(c);
            int s;
            if (fa && fb) {
                double ra = (usedA + G.areaA[c]) / G.capA;
                double rb = (usedB + G.areaB[c]) / G.capB;
                s = (ra <= rb) ? 0 : 1;
                if (randomize && U(rng) < 0.30) s ^= 1;
            } else if (fa) s = 0;
            else if (fb) s = 1;
            else { double ra = (usedA + G.areaA[c]) / G.capA, rb = (usedB + G.areaB[c]) / G.capB; s = (ra <= rb) ? 0 : 1; }
            side[c] = (uint8_t)s;
            if (s == 0) usedA += G.areaA[c]; else usedB += G.areaB[c];
        }
        repairFeasibility(order);
    }

    void repairFeasibility(const vector<int>& order) {
        if (legal()) return;
        for (int c : order) {
            if (legal()) break;
            if (side[c] == 0 && usedA > G.capA + EPS) {
                if (usedB + G.areaB[c] <= G.capB + EPS) { usedA -= G.areaA[c]; usedB += G.areaB[c]; side[c] = 1; }
            } else if (side[c] == 1 && usedB > G.capB + EPS) {
                if (usedA + G.areaA[c] <= G.capA + EPS) { usedB -= G.areaB[c]; usedA += G.areaA[c]; side[c] = 0; }
            }
        }
    }

    void rebuildCounts() {
        fill(cntA.begin(), cntA.end(), 0);
        fill(cntB.begin(), cntB.end(), 0);
        for (int n = 0; n < nNets; ++n) {
            int a = 0, b = 0;
            for (int k = G.netOff[n]; k < G.netOff[n + 1]; ++k) { if (side[G.netIdx[k]] == 0) ++a; else ++b; }
            cntA[n] = a; cntB[n] = b;
        }
    }
    int computeCut() const {
        int cut = 0;
        for (int n = 0; n < nNets; ++n) if (cntA[n] > 0 && cntB[n] > 0) ++cut;
        return cut;
    }

    inline void bInsert(int c) {
        int gi = gain[c] + off, h = bhead[gi];
        bprev[c] = -1; bnext[c] = h;
        if (h != -1) bprev[h] = c;
        bhead[gi] = c;
        if (gi > maxPtr) maxPtr = gi;
    }
    inline void bRemove(int c) {
        int gi = gain[c] + off, pr = bprev[c], nx = bnext[c];
        if (pr != -1) bnext[pr] = nx; else bhead[gi] = nx;
        if (nx != -1) bprev[nx] = pr;
    }
    inline void adjustGain(int c, int delta) {
        if (state[c] != 0) return;
        bRemove(c); gain[c] += delta; bInsert(c);
    }
    inline int extractMax() {
        while (maxPtr >= 0 && bhead[maxPtr] == -1) --maxPtr;
        return maxPtr < 0 ? -1 : bhead[maxPtr];
    }
    int initialGain(int c) const {
        int f = side[c], g = 0;
        for (int k = G.cellOff[c]; k < G.cellOff[c + 1]; ++k) {
            int n = G.cellIdx[k];
            int cf = (f == 0) ? cntA[n] : cntB[n];
            int ct = (f == 0) ? cntB[n] : cntA[n];
            if (cf == 1) ++g;
            if (ct == 0) --g;
        }
        return g;
    }

    void applyMove(int c) {
        int F = side[c], T = 1 - F;
        for (int k = G.cellOff[c]; k < G.cellOff[c + 1]; ++k) {
            int n = G.cellIdx[k];
            int s0 = G.netOff[n], s1 = G.netOff[n + 1];
            int cntT = (T == 0) ? cntA[n] : cntB[n];
            if (cntT == 0) {
                for (int j = s0; j < s1; ++j) { int d = G.netIdx[j]; if (d != c) adjustGain(d, +1); }
            } else if (cntT == 1) {
                for (int j = s0; j < s1; ++j) { int d = G.netIdx[j]; if (d != c && side[d] == T) { adjustGain(d, -1); break; } }
            }
            if (F == 0) { cntA[n]--; cntB[n]++; } else { cntB[n]--; cntA[n]++; }
            int cntFnew = (F == 0) ? cntA[n] : cntB[n];
            if (cntFnew == 0) {
                for (int j = s0; j < s1; ++j) { int d = G.netIdx[j]; if (d != c) adjustGain(d, -1); }
            } else if (cntFnew == 1) {
                for (int j = s0; j < s1; ++j) { int d = G.netIdx[j]; if (d != c && side[d] == F) { adjustGain(d, +1); break; } }
            }
        }
        if (F == 0) { usedA -= G.areaA[c]; usedB += G.areaB[c]; }
        else        { usedB -= G.areaB[c]; usedA += G.areaA[c]; }
        side[c] = (uint8_t)T;
    }
    void undoMove(int c) {
        int cur = side[c], prev = 1 - cur;
        for (int k = G.cellOff[c]; k < G.cellOff[c + 1]; ++k) {
            int n = G.cellIdx[k];
            if (cur == 0) { cntA[n]--; cntB[n]++; } else { cntB[n]--; cntA[n]++; }
        }
        if (cur == 0) { usedA -= G.areaA[c]; usedB += G.areaB[c]; }
        else          { usedB -= G.areaB[c]; usedA += G.areaA[c]; }
        side[c] = (uint8_t)prev;
    }

    int onePass() {
        fill(bhead.begin(), bhead.end(), -1); maxPtr = 0;
        for (int c = 0; c < nCells; ++c) { state[c] = 0; gain[c] = initialGain(c); bInsert(c); }
        moves.clear();
        long long cum = 0, bestCum = 0; int bestIdx = 0, sinceCheck = 0;
        while (true) {
            int c = extractMax();
            if (c < 0) break;
            int T = 1 - side[c];
            bool fit = (T == 0) ? fitsA(c) : fitsB(c);
            bRemove(c); state[c] = 1;
            if (!fit) continue;
            int g = gain[c];
            applyMove(c);
            cum += g; moves.push_back(c);
            if (cum > bestCum) { bestCum = cum; bestIdx = (int)moves.size(); }
            if ((++sinceCheck & 16383) == 0 && elapsed() > DEADLINE_SEC) break;
        }
        for (int i = (int)moves.size() - 1; i >= bestIdx; --i) undoMove(moves[i]);
        return (int)bestCum;
    }

    void runFM(int maxPasses = 200) {
        rebuildCounts();
        for (int pass = 0; pass < maxPasses; ++pass) {
            if (elapsed() > DEADLINE_SEC) break;
            if (onePass() <= 0) break;
        }
    }

    // greedy init + FM; track local best (legal, lowest cut, tie lowest seed)
    void oneStart(const vector<int>& order, int seed) {
        rng.seed((uint32_t)(seed * 2654435761u + 12345u));
        greedyInit(order, seed);
        if (!legal()) return;
        runFM();
        if (!legal()) return;
        rebuildCounts();
        int cut = computeCut();
        if (cut < bestCut || (cut == bestCut && seed < bestSeed)) { bestCut = cut; bestSeed = seed; bestSide = side; }
    }
};

// ----------------------------- coarsening -----------------------------------
// Heavy-edge / first-choice matching: pair each cell with its most strongly
// connected unmatched neighbour. Returns the coarser graph and fills cmap
// (fine cell id -> coarse cell id). matchOrder allows randomized restarts.
static Graph coarsen(const Graph& G, vector<int>& cmap, const vector<int>& visitOrder, int matchDegCap) {
    int n = G.nCells;
    vector<int> mate(n, -1);
    vector<double> score(n, 0.0);
    vector<int> touched; touched.reserve(256);

    for (int u : visitOrder) {
        if (mate[u] != -1) continue;
        int best = -1; double bestScore = 0.0;
        for (int k = G.cellOff[u]; k < G.cellOff[u + 1]; ++k) {
            int net = G.cellIdx[k];
            int deg = G.netOff[net + 1] - G.netOff[net];
            if (deg > matchDegCap || deg < 2) continue;
            double w = 1.0 / (double)(deg - 1);
            for (int j = G.netOff[net]; j < G.netOff[net + 1]; ++j) {
                int v = G.netIdx[j];
                if (v == u || mate[v] != -1) continue;
                if (score[v] == 0.0) touched.push_back(v);
                score[v] += w;
                if (score[v] > bestScore || (score[v] == bestScore && (best == -1 || v < best))) {
                    bestScore = score[v]; best = v;
                }
            }
        }
        for (int v : touched) score[v] = 0.0;
        touched.clear();
        if (best != -1) { mate[u] = best; mate[best] = u; }
        else mate[u] = u;
    }

    // assign coarse ids
    cmap.assign(n, -1);
    int nc = 0;
    for (int u = 0; u < n; ++u) {
        if (cmap[u] != -1) continue;
        int m = mate[u];
        cmap[u] = nc;
        if (m != u && m != -1) cmap[m] = nc;
        ++nc;
    }

    Graph C;
    C.nCells = nc; C.capA = G.capA; C.capB = G.capB;
    C.areaA.assign(nc, 0.0); C.areaB.assign(nc, 0.0);
    for (int u = 0; u < n; ++u) { C.areaA[cmap[u]] += G.areaA[u]; C.areaB[cmap[u]] += G.areaB[u]; }

    // coarse nets: remap members, dedup, keep deg>=2
    vector<int> seen(nc, -1);
    C.netOff.reserve(G.nNets + 1); C.netIdx.reserve(G.netIdx.size()); C.netOff.push_back(0);
    vector<int> tmp; tmp.reserve(64);
    int cn = 0;
    for (int net = 0; net < G.nNets; ++net) {
        tmp.clear();
        for (int k = G.netOff[net]; k < G.netOff[net + 1]; ++k) {
            int cc = cmap[G.netIdx[k]];
            if (seen[cc] == cn) continue;
            seen[cc] = cn; tmp.push_back(cc);
        }
        if ((int)tmp.size() < 2) continue;
        for (int x : tmp) C.netIdx.push_back(x);
        C.netOff.push_back((int)C.netIdx.size());
        ++cn;
    }
    C.nNets = cn;
    C.buildCellCSR();
    return C;
}

static vector<int> areaOrder(const Graph& G) {
    vector<int> order(G.nCells);
    for (int i = 0; i < G.nCells; ++i) order[i] = i;
    sort(order.begin(), order.end(), [&](int a, int b) {
        double ma = max(G.areaA[a], G.areaB[a]), mb = max(G.areaA[b], G.areaB[b]);
        if (ma != mb) return ma > mb;
        return a < b;
    });
    return order;
}

// ----------------------------- multilevel V-cycle ---------------------------
// Build a level hierarchy, partition the coarsest with multi-start FM, then
// uncoarsen with FM refinement at each level. Returns finest side[] + cut.
struct VResult { vector<uint8_t> side; int cut = INT32_MAX; bool ok = false; };

static VResult vcycle(const Graph& finest, uint32_t seed, int coarseTarget, int coarseStarts) {
    VResult R;
    vector<Graph> levels;
    vector<vector<int>> cmaps;
    levels.reserve(48);
    levels.push_back(finest);          // copy (shared CSR); acceptable, few V-cycles

    mt19937 rng(seed * 2654435761u + 7u);
    const int matchDegCap = 200;
    while ((int)levels.back().nCells > coarseTarget) {
        const Graph& cur = levels.back();
        // visit order: index order for seed 0, shuffled otherwise
        vector<int> vorder(cur.nCells);
        for (int i = 0; i < cur.nCells; ++i) vorder[i] = i;
        if (seed != 0) shuffle(vorder.begin(), vorder.end(), rng);
        vector<int> cmap;
        Graph C = coarsen(cur, cmap, vorder, matchDegCap);
        if (C.nCells >= (int)(cur.nCells * 0.92) || C.nCells == cur.nCells) {
            // poor contraction: stop here
            break;
        }
        cmaps.push_back(move(cmap));
        levels.push_back(move(C));
        if (levels.size() > 46) break;
        if (elapsed() > DEADLINE_SEC) break;
    }

    int L = (int)levels.size();
    // partition coarsest
    {
        Partitioner Pc(levels[L - 1]);
        vector<int> ord = areaOrder(levels[L - 1]);
        for (int s = 0; s < coarseStarts; ++s) {
            if (elapsed() > DEADLINE_SEC) break;
            Pc.oneStart(ord, (int)(seed * 1000 + s));
        }
        if (Pc.bestCut == INT32_MAX) return R; // no legal
        R.side = Pc.bestSide;
    }
    // uncoarsen + refine
    for (int lv = L - 2; lv >= 0; --lv) {
        const vector<int>& cmap = cmaps[lv];           // levels[lv] -> levels[lv+1]
        const Graph& fineG = levels[lv];
        vector<uint8_t> projected(fineG.nCells);
        for (int i = 0; i < fineG.nCells; ++i) projected[i] = R.side[cmap[i]];
        Partitioner Pf(fineG);
        Pf.side = move(projected);
        Pf.recomputeUsed();
        if (!Pf.legal()) Pf.repairFeasibility(areaOrder(fineG));
        Pf.runFM();
        R.side = move(Pf.side);
        if (elapsed() > DEADLINE_SEC) {
            // project remaining levels without refine to keep a full-coverage answer
            for (int lv2 = lv - 1; lv2 >= 0; --lv2) {
                const vector<int>& cm2 = cmaps[lv2];
                const Graph& fg2 = levels[lv2];
                vector<uint8_t> pj(fg2.nCells);
                for (int i = 0; i < fg2.nCells; ++i) pj[i] = R.side[cm2[i]];
                R.side = move(pj);
            }
            break;
        }
    }
    // final cut on finest
    Partitioner Pf(finest);
    Pf.side = R.side;
    Pf.recomputeUsed();
    if (!Pf.legal()) { Pf.repairFeasibility(areaOrder(finest)); R.side = Pf.side; }
    Pf.rebuildCounts();
    R.cut = Pf.computeCut();
    R.ok = Pf.legal();
    return R;
}

// ----------------------------- output ---------------------------------------
static bool writeOutput(const char* path, const Graph& G, const vector<string_view>& name,
                        const vector<uint8_t>& side, int cut) {
    namespace fs = std::filesystem;
    try { fs::path p(path); if (p.has_parent_path()) fs::create_directories(p.parent_path()); } catch (...) {}
    int ca = 0, cb = 0;
    for (int i = 0; i < G.nCells; ++i) (side[i] == 0 ? ca : cb)++;
    string out; out.reserve((size_t)G.nCells * 8 + 64);
    char hdr[64];
    out.append(hdr, snprintf(hdr, sizeof(hdr), "CutSize %d\n", cut));
    out.append(hdr, snprintf(hdr, sizeof(hdr), "DieA %d\n", ca));
    for (int i = 0; i < G.nCells; ++i) if (side[i] == 0) { out.append(name[i]); out.push_back('\n'); }
    out.append(hdr, snprintf(hdr, sizeof(hdr), "DieB %d\n", cb));
    for (int i = 0; i < G.nCells; ++i) if (side[i] == 1) { out.append(name[i]); out.push_back('\n'); }
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open output %s\n", path); return false; }
    fwrite(out.data(), 1, out.size(), f); fclose(f);
    return true;
}

// ----------------------------- main -----------------------------------------
int main(int argc, char** argv) {
    g_start = Clock::now();
    if (argc < 3) { fprintf(stderr, "usage: %s <input.txt> <output.out>\n", argv[0]); return 1; }

    vector<char> buf;
    Graph G;
    vector<string_view> name;
    if (!parseInput(argv[1], buf, G, name)) return 1;
    if (G.nCells == 0) { writeOutput(argv[2], G, name, {}, 0); return 0; }

    vector<int> order = areaOrder(G);

    vector<uint8_t> gBestSide;
    int gBestCut = INT32_MAX;

    const int SMALL = 3000;  // below this: flat multi-start FM is enough

    if (G.nCells <= SMALL) {
        // flat parallel multi-start FM
        const int MAX_STARTS = 4096;
        atomic<int> nextSeed{0};
        int gSeed = INT32_MAX;
        #pragma omp parallel
        {
            Partitioner P(G);
            while (true) {
                int seed = nextSeed.fetch_add(1);
                if (seed >= MAX_STARTS) break;
                if (elapsed() > DEADLINE_SEC) break;
                P.oneStart(order, seed);
            }
            #pragma omp critical
            {
                if (P.bestCut < gBestCut || (P.bestCut == gBestCut && P.bestSeed < gSeed)) {
                    gBestCut = P.bestCut; gSeed = P.bestSeed; gBestSide = vector<uint8_t>(P.bestSide);
                }
            }
        }
    } else {
        // multilevel: run V-cycles (parallel over seeds) until the deadline or a
        // stall (no improvement for many consecutive V-cycles); keep best legal.
        int coarseTarget = 1500;
        int coarseStarts = 80;
        const int VCYCLES = 1000000;          // effectively deadline/stall-bounded
        const int STALL_LIMIT = 800;          // consecutive non-improving V-cycles
        bool verbose = (getenv("SOLVER_VERBOSE") != nullptr);
        atomic<int> nextV{0};
        atomic<int> noImprove{0};
        atomic<bool> stop{false};
        atomic<int> vdone{0};
        int gSeed = INT32_MAX;
        #pragma omp parallel
        {
            while (true) {
                if (stop.load()) break;
                int v = nextV.fetch_add(1);
                if (v >= VCYCLES) break;
                if (v > 0 && elapsed() > DEADLINE_SEC) break;
                VResult R = vcycle(G, (uint32_t)v, coarseTarget, coarseStarts);
                bool improved = false;
                #pragma omp critical
                {
                    if (R.ok && (R.cut < gBestCut || (R.cut == gBestCut && v < gSeed))) {
                        if (R.cut < gBestCut) improved = true;
                        gBestCut = R.cut; gSeed = v; gBestSide = vector<uint8_t>(R.side);
                    }
                }
                int done = vdone.fetch_add(1) + 1;
                if (improved) noImprove.store(0);
                else if (noImprove.fetch_add(1) + 1 >= STALL_LIMIT) stop.store(true);
                if (verbose && (done % 50 == 0))
                    fprintf(stderr, "[v=%d] best=%d noImp=%d t=%.1f\n",
                            done, gBestCut, noImprove.load(), elapsed());
            }
        }
        if (verbose) fprintf(stderr, "[done] vcycles=%d best=%d t=%.1f\n",
                             vdone.load(), gBestCut, elapsed());
    }

    if (gBestCut == INT32_MAX) {
        // safety fallback: deterministic greedy + FM
        Partitioner P(G);
        P.greedyInit(order, 0);
        if (P.legal()) { P.runFM(); }
        P.recomputeUsed();
        if (!P.legal()) P.repairFeasibility(order);
        P.rebuildCounts();
        gBestSide = P.side; gBestCut = P.computeCut();
    }

    writeOutput(argv[2], G, name, gBestSide, gBestCut);
    return 0;
}
