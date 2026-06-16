// 001 — Multi-technology Die Partitioning
// Constrained hypergraph min-cut: assign every cell to DieA or DieB so that
//   sum_{c in A} areaA[c] <= utilA*dieArea  and  sum_{c in B} areaB[c] <= utilB*dieArea
// minimizing the number of nets that touch both dies (cut size).
//
// Approach (第一性原理): the problem is hypergraph bipartitioning with two
// independent, asymmetric area caps. Strongest practical lever = MULTILEVEL FM:
//   coarsen by heavy-edge matching -> initial feasible partition on coarsest ->
//   uncoarsen + Fiduccia-Mattheyses refinement at every level.
// Wrapped in PARALLEL MULTI-START (OpenMP); keep best feasible cut.
//
// Build (to allowed exec dir):
//   g++ -std=c++20 -O3 -fopenmp -pthread -o D:\FSecret\hw2.exe main.cpp
// Run:
//   D:\FSecret\hw2.exe <input.txt> <output.out>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <random>
#include <atomic>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using ld = long double;
using Clock = chrono::steady_clock;

static Clock::time_point T0;
static double TIME_LIMIT = 280.0;
static inline double elapsed() {
    return chrono::duration<double>(Clock::now() - T0).count();
}

// ----------------------------- fast input -------------------------------------
struct Reader {
    vector<char> buf;
    size_t p = 0, n = 0;
    bool load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf.resize((size_t)sz + 1);
        n = fread(buf.data(), 1, (size_t)sz, f);
        buf[n] = 0;
        fclose(f);
        return true;
    }
    // next whitespace-delimited token as a (ptr,len) view; returns false at EOF
    bool tok(const char*& s, size_t& len) {
        while (p < n && (buf[p] == ' ' || buf[p] == '\n' || buf[p] == '\r' || buf[p] == '\t'))
            ++p;
        if (p >= n) return false;
        size_t start = p;
        while (p < n && buf[p] != ' ' && buf[p] != '\n' && buf[p] != '\r' && buf[p] != '\t')
            ++p;
        s = &buf[start];
        len = p - start;
        return true;
    }
    string str() { const char* s; size_t l; if (!tok(s, l)) return string(); return string(s, l); }
    long long i64() {
        const char* s; size_t l;
        if (!tok(s, l)) return 0;
        long long v = 0; bool neg = false; size_t k = 0;
        if (l && s[0] == '-') { neg = true; k = 1; }
        for (; k < l; ++k) v = v * 10 + (s[k] - '0');
        return neg ? -v : v;
    }
    ld real() {
        string t = str();
        return (ld)strtold(t.c_str(), nullptr);
    }
};

// ----------------------------- hypergraph -------------------------------------
struct HG {
    int n = 0;            // number of nodes (cells, or clustered cells)
    int m = 0;            // number of nets
    vector<ld> aA, aB;    // node area on die A / die B
    // net -> nodes (CSR)
    vector<int> netOff, netPin;
    // node -> nets (CSR)
    vector<int> nodeOff, nodeNet;
    int maxDeg = 1;       // max #incident nets over nodes (gain bound)

    void buildNodeCSR() {
        nodeOff.assign(n + 1, 0);
        for (int e = 0; e < m; ++e)
            for (int k = netOff[e]; k < netOff[e + 1]; ++k)
                nodeOff[netPin[k] + 1]++;
        for (int i = 0; i < n; ++i) nodeOff[i + 1] += nodeOff[i];
        nodeNet.assign(nodeOff[n], 0);
        vector<int> cur(nodeOff.begin(), nodeOff.begin() + n);
        for (int e = 0; e < m; ++e)
            for (int k = netOff[e]; k < netOff[e + 1]; ++k) {
                int v = netPin[k];
                nodeNet[cur[v]++] = e;
            }
        maxDeg = 1;
        for (int i = 0; i < n; ++i) maxDeg = max(maxDeg, nodeOff[i + 1] - nodeOff[i]);
    }
};

// ----------------------------- FM refinement ----------------------------------
// Gain-bucket Fiduccia-Mattheyses for 2-way hypergraph partitioning with two
// asymmetric area caps. part[i] in {0,1} (0=DieA, 1=DieB).
struct FM {
    const HG& h;
    ld capA, capB;
    ld relax = 0.0;             // allow transient overshoot up to cap*(1+relax)
    vector<int>& part;
    ld areaA = 0, areaB = 0;

    vector<int> cnt0, cnt1;     // per-net count on side 0 / side 1
    vector<int> gain;           // per-node gain
    vector<char> locked;
    // bucket linked lists indexed by gain+offset
    int offset, B;              // B = 2*maxDeg+1
    vector<int> head;           // bucket head
    vector<int> nxt, prv;       // intrusive list
    int topB;                   // highest non-empty bucket index

    FM(const HG& hg, ld cA, ld cB, vector<int>& pt)
        : h(hg), capA(cA), capB(cB), part(pt) {
        offset = h.maxDeg;
        B = 2 * h.maxDeg + 1;
    }

    inline void blink_insert(int v) {
        int g = gain[v] + offset;
        nxt[v] = head[g];
        prv[v] = -1;
        if (head[g] != -1) prv[head[g]] = v;
        head[g] = v;
        if (g > topB) topB = g;
    }
    inline void blink_remove(int v) {
        int g = gain[v] + offset;
        if (prv[v] != -1) nxt[prv[v]] = nxt[v]; else head[g] = nxt[v];
        if (nxt[v] != -1) prv[nxt[v]] = prv[v];
    }
    inline void changeGain(int v, int delta) {
        if (locked[v] || delta == 0) { gain[v] += delta; return; }
        blink_remove(v);
        gain[v] += delta;
        blink_insert(v);
    }

    void recomputeAreas() {
        areaA = areaB = 0;
        for (int i = 0; i < h.n; ++i) {
            if (part[i] == 0) areaA += h.aA[i];
            else areaB += h.aB[i];
        }
    }

    // one FM pass; returns cut improvement (>=0). part updated to best prefix.
    long long pass() {
        // net counts
        cnt0.assign(h.m, 0); cnt1.assign(h.m, 0);
        for (int e = 0; e < h.m; ++e)
            for (int k = h.netOff[e]; k < h.netOff[e + 1]; ++k) {
                if (part[h.netPin[k]] == 0) cnt0[e]++; else cnt1[e]++;
            }
        // gains
        gain.assign(h.n, 0);
        for (int v = 0; v < h.n; ++v) {
            int from = part[v], to = 1 - from;
            int g = 0;
            for (int k = h.nodeOff[v]; k < h.nodeOff[v + 1]; ++k) {
                int e = h.nodeNet[k];
                int cf = (from == 0 ? cnt0[e] : cnt1[e]);
                int ct = (to == 0 ? cnt0[e] : cnt1[e]);
                if (cf == 1) g++;
                if (ct == 0) g--;
            }
            gain[v] = g;
        }
        locked.assign(h.n, 0);
        head.assign(B, -1);
        nxt.assign(h.n, -1); prv.assign(h.n, -1);
        topB = 0;
        for (int v = 0; v < h.n; ++v) blink_insert(v);

        recomputeAreas();

        // move sequence; track best prefix by cumulative gain
        struct Mv { int v; int from; };
        vector<Mv> log; log.reserve(h.n);
        long long cum = 0, best = 0; size_t bestStep = 0;

        int moves = 0;
        while (true) {
            // find highest-gain feasible node
            while (topB >= 0 && head[topB] == -1) --topB;
            if (topB < 0) break;
            ld limA = capA * (1.0L + relax), limB = capB * (1.0L + relax);
            int v = -1;
            for (int g = topB; g >= 0 && v == -1; --g) {
                for (int u = head[g]; u != -1; u = nxt[u]) {
                    int from = part[u], to = 1 - from;
                    bool ok = (to == 0) ? (areaA + h.aA[u] <= limA)
                                        : (areaB + h.aB[u] <= limB);
                    if (ok) { v = u; break; }
                }
            }
            if (v == -1) break;

            int from = part[v], to = 1 - from;
            int gv = gain[v];
            blink_remove(v);
            locked[v] = 1;

            // FM gain update over incident nets (textbook 2-way hypergraph)
            for (int k = h.nodeOff[v]; k < h.nodeOff[v + 1]; ++k) {
                int e = h.nodeNet[k];
                int cf = (from == 0 ? cnt0[e] : cnt1[e]);
                int ct = (to == 0 ? cnt0[e] : cnt1[e]);
                // before removing v from 'from'
                if (ct == 0) {
                    for (int q = h.netOff[e]; q < h.netOff[e + 1]; ++q)
                        if (!locked[h.netPin[q]]) changeGain(h.netPin[q], +1);
                } else if (ct == 1) {
                    for (int q = h.netOff[e]; q < h.netOff[e + 1]; ++q) {
                        int u = h.netPin[q];
                        if (!locked[u] && part[u] == to) { changeGain(u, -1); break; }
                    }
                }
                // move v: 'from' loses one, 'to' gains one
                if (from == 0) { cnt0[e]--; cnt1[e]++; } else { cnt1[e]--; cnt0[e]++; }
                cf = (from == 0 ? cnt0[e] : cnt1[e]);
                if (cf == 0) {
                    for (int q = h.netOff[e]; q < h.netOff[e + 1]; ++q)
                        if (!locked[h.netPin[q]]) changeGain(h.netPin[q], -1);
                } else if (cf == 1) {
                    for (int q = h.netOff[e]; q < h.netOff[e + 1]; ++q) {
                        int u = h.netPin[q];
                        if (!locked[u] && part[u] == from) { changeGain(u, +1); break; }
                    }
                }
            }
            // commit areas + part
            if (from == 0) { areaA -= h.aA[v]; areaB += h.aB[v]; }
            else           { areaB -= h.aB[v]; areaA += h.aA[v]; }
            part[v] = to;

            cum += gv;
            log.push_back({v, from});
            // only snapshot strictly feasible states as candidate best prefixes
            bool feas = (areaA <= capA + 1e-9L && areaB <= capB + 1e-9L);
            if (feas && cum > best) { best = cum; bestStep = log.size(); }

            if ((++moves & 8191) == 0 && elapsed() > TIME_LIMIT) break;
        }
        // revert moves after best prefix
        for (size_t i = log.size(); i > bestStep; --i) {
            Mv& mv = log[i - 1];
            int v = mv.v, from = mv.from, to = 1 - from;
            // currently on 'to'; move back to 'from'
            if (to == 0) { areaA -= h.aA[v]; areaB += h.aB[v]; }
            else         { areaB -= h.aB[v]; areaA += h.aA[v]; }
            part[v] = from;
        }
        return best;
    }

    void refine(int maxPasses) {
        for (int it = 0; it < maxPasses; ++it) {
            long long imp = pass();
            if (imp <= 0) break;
            if (elapsed() > TIME_LIMIT) break;
        }
    }
};

// ----------------------------- cut size ---------------------------------------
static long long cutSize(const HG& h, const vector<int>& part) {
    long long cut = 0;
    for (int e = 0; e < h.m; ++e) {
        bool a = false, b = false;
        for (int k = h.netOff[e]; k < h.netOff[e + 1]; ++k) {
            if (part[h.netPin[k]] == 0) a = true; else b = true;
            if (a && b) { cut++; break; }
        }
    }
    return cut;
}

// ----------------------- initial feasible partition ---------------------------
// Best-fit decreasing: place hardest cells first to the die with most relative
// slack while respecting both caps. Guarantees feasibility when one exists for
// this greedy order; returns false if a cell fits nowhere.
static bool greedyInit(const HG& h, ld capA, ld capB, vector<int>& part,
                       std::mt19937_64& rng, bool randomized) {
    vector<int> order(h.n);
    for (int i = 0; i < h.n; ++i) order[i] = i;
    sort(order.begin(), order.end(), [&](int x, int y) {
        return max(h.aA[x], h.aB[x]) > max(h.aA[y], h.aB[y]);
    });
    if (randomized) {
        // light perturbation: swap a few neighbors to diversify starts
        for (int i = 0; i + 1 < h.n; ++i)
            if ((rng() & 3) == 0) swap(order[i], order[i + 1]);
    }
    part.assign(h.n, -1);
    ld useA = 0, useB = 0;
    for (int idx : order) {
        bool fitA = useA + h.aA[idx] <= capA;
        bool fitB = useB + h.aB[idx] <= capB;
        int pick;
        if (fitA && fitB) {
            // choose die with larger remaining relative slack after placing
            ld slackA = (capA - useA - h.aA[idx]) / (capA > 0 ? capA : 1);
            ld slackB = (capB - useB - h.aB[idx]) / (capB > 0 ? capB : 1);
            pick = (slackA >= slackB) ? 0 : 1;
        } else if (fitA) pick = 0;
        else if (fitB) pick = 1;
        else return false;
        part[idx] = pick;
        if (pick == 0) useA += h.aA[idx]; else useB += h.aB[idx];
    }
    return true;
}

// ----------------------------- coarsening -------------------------------------
// Heavy-edge matching: pair each unmatched node with its best-connected
// unmatched neighbor (weight = sum 1/(deg-1) over shared nets). Returns coarse
// HG and the fine->coarse map.
static HG coarsen(const HG& h, vector<int>& fine2coarse, std::mt19937_64& rng,
                  const vector<int>* part = nullptr) {
    vector<int> match(h.n, -1);
    vector<int> order(h.n);
    for (int i = 0; i < h.n; ++i) order[i] = i;
    for (int i = h.n - 1; i > 0; --i) { int j = rng() % (i + 1); swap(order[i], order[j]); }

    vector<ld> score(h.n, 0);
    vector<int> touched;
    int nc = 0;
    fine2coarse.assign(h.n, -1);

    for (int oi = 0; oi < h.n; ++oi) {
        int v = order[oi];
        if (match[v] != -1) continue;
        // accumulate connection scores to unmatched neighbors
        touched.clear();
        int best = -1; ld bestS = -1;
        for (int k = h.nodeOff[v]; k < h.nodeOff[v + 1]; ++k) {
            int e = h.nodeNet[k];
            int deg = h.netOff[e + 1] - h.netOff[e];
            if (deg < 2 || deg > 200) continue;     // skip huge nets (weak signal)
            ld w = (ld)1.0 / (deg - 1);
            for (int q = h.netOff[e]; q < h.netOff[e + 1]; ++q) {
                int u = h.netPin[q];
                if (u == v || match[u] != -1) continue;
                if (part && (*part)[u] != (*part)[v]) continue;   // V-cycle: same side only
                if (score[u] == 0) touched.push_back(u);
                score[u] += w;
            }
        }
        for (int u : touched) {
            ld s = score[u];
            if (s > bestS) { bestS = s; best = u; }
            score[u] = 0;
        }
        if (best == -1) {
            fine2coarse[v] = nc++;            // unmatched singleton
        } else {
            match[v] = best; match[best] = v;
            int id = nc++;
            fine2coarse[v] = id; fine2coarse[best] = id;
        }
    }

    HG c;
    c.n = nc;
    c.aA.assign(nc, 0); c.aB.assign(nc, 0);
    for (int i = 0; i < h.n; ++i) {
        c.aA[fine2coarse[i]] += h.aA[i];
        c.aB[fine2coarse[i]] += h.aB[i];
    }
    // contract nets: map pins, dedup, drop size<2
    c.netOff.reserve(h.m + 1);
    c.netOff.push_back(0);
    c.netPin.reserve(h.netPin.size());
    vector<int> seenMark(nc, -1);
    vector<int> tmp;
    int eid = 0;
    for (int e = 0; e < h.m; ++e) {
        tmp.clear();
        for (int k = h.netOff[e]; k < h.netOff[e + 1]; ++k) {
            int cv = fine2coarse[h.netPin[k]];
            if (seenMark[cv] != e) { seenMark[cv] = e; tmp.push_back(cv); }
        }
        if (tmp.size() < 2) continue;            // internal net, no cut possible
        for (int x : tmp) c.netPin.push_back(x);
        c.netOff.push_back((int)c.netPin.size());
        ++eid;
    }
    c.m = eid;
    c.buildNodeCSR();
    return c;
}

// One V-cycle: coarsen (restricted to current sides when seedPart != null),
// (re)partition the coarsest level, then uncoarsen with FM refinement at each
// level. On a fresh cycle (seedPart == null) the coarsest level is initialised
// greedily; on a refinement cycle the current partition is carried down and
// improved. Updates `part` in place; returns false only if no feasible init.
static bool oneCycle(const HG& root, ld capA, ld capB, vector<int>& part,
                     std::mt19937_64& rng, const vector<int>* seedPart,
                     double relax) {
    vector<HG> levels;
    vector<vector<int>> maps;          // maps[l]: level l -> level l+1
    levels.push_back(root);            // copy (independent per thread)
    const int COARSEST = 100;

    // carry the partition down level by level (only needed for V-cycles)
    vector<int> curPart;
    if (seedPart) curPart = *seedPart;
    vector<vector<int>> levelPart;     // levelPart[l] valid only when seedPart
    if (seedPart) levelPart.push_back(curPart);

    while ((int)levels.back().n > COARSEST) {
        if (elapsed() > TIME_LIMIT) break;
        vector<int> f2c;
        HG c = coarsen(levels.back(), f2c, rng, seedPart ? &curPart : nullptr);
        if (c.n >= (int)(levels.back().n * 0.95) || c.n == levels.back().n)
            break;                      // no meaningful reduction
        if (seedPart) {
            vector<int> np(c.n, 0);
            for (int i = 0; i < levels.back().n; ++i) np[f2c[i]] = curPart[i];
            curPart.swap(np);
            levelPart.push_back(curPart);
        }
        maps.push_back(std::move(f2c));
        levels.push_back(std::move(c));
    }

    int top = (int)levels.size() - 1;
    if (seedPart) {
        part = levelPart[top];          // projected partition at coarsest
    } else {
        if (!greedyInit(levels[top], capA, capB, part, rng, false)) {
            bool ok = false;
            for (int t = 0; t < 8 && !ok; ++t)
                ok = greedyInit(levels[top], capA, capB, part, rng, true);
            if (!ok) return false;
        }
    }
    {
        FM fm(levels[top], capA, capB, part); fm.relax = relax;
        fm.refine(seedPart ? 10 : 20);
    }

    for (int l = top - 1; l >= 0; --l) {
        const vector<int>& mp = maps[l];
        vector<int> finePart(levels[l].n);
        for (int i = 0; i < levels[l].n; ++i) finePart[i] = part[mp[i]];
        part.swap(finePart);
        FM fm(levels[l], capA, capB, part); fm.relax = relax;
        fm.refine(l == 0 ? 12 : 6);
        if (elapsed() > TIME_LIMIT) {
            for (int ll = l - 1; ll >= 0; --ll) {
                const vector<int>& mp2 = maps[ll];
                vector<int> fp(levels[ll].n);
                for (int i = 0; i < levels[ll].n; ++i) fp[i] = part[mp2[i]];
                part.swap(fp);
            }
            break;
        }
    }
    return true;
}

// --------------------- one full multilevel run (fresh + V-cycles) -------------
static bool multilevelRun(const HG& root, ld capA, ld capB,
                          vector<int>& outPart, long long& outCut,
                          uint64_t seed, double relax) {
    std::mt19937_64 rng(seed);
    vector<int> part;
    if (!oneCycle(root, capA, capB, part, rng, nullptr, relax)) return false;
    long long cut = cutSize(root, part);

    // V-cycles: re-coarsen along the current cut and refine; keep improvements.
    const int MAX_VCYCLES = 6;
    int stale = 0;
    for (int v = 0; v < MAX_VCYCLES && stale < 2; ++v) {
        if (elapsed() > TIME_LIMIT) break;
        vector<int> trial = part;
        oneCycle(root, capA, capB, trial, rng, &part, relax);
        long long c2 = cutSize(root, trial);
        if (c2 < cut) { cut = c2; part.swap(trial); stale = 0; }
        else stale++;
    }

    outPart = std::move(part);
    outCut = cut;
    return true;
}

// ----------------------------- main -------------------------------------------
int main(int argc, char** argv) {
    T0 = Clock::now();
    if (const char* e = getenv("PART_TIME")) {       // optional override for tuning
        double v = atof(e);
        if (v > 0) TIME_LIMIT = v;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: %s <input.txt> <output.out>\n", argv[0]);
        return 1;
    }
    Reader rd;
    if (!rd.load(argv[1])) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }

    // --- parse ---
    // techs: libcell name -> id; area[tech][libid]
    unordered_map<string, int> libId;
    libId.reserve(1024);
    auto getLib = [&](const string& s) -> int {
        auto it = libId.find(s);
        if (it != libId.end()) return it->second;
        int id = (int)libId.size();
        libId.emplace(s, id);
        return id;
    };

    rd.str();                          // "NumTechs"
    int numTechs = (int)rd.i64();
    unordered_map<string, int> techId;
    vector<vector<ld>> techArea(numTechs);   // techArea[t][libid]
    for (int t = 0; t < numTechs; ++t) {
        rd.str();                      // "Tech"
        string tname = rd.str();
        int nlc = (int)rd.i64();
        techId[tname] = t;
        techArea[t].assign(max(1, nlc) * 2, 0);
        for (int j = 0; j < nlc; ++j) {
            rd.str();                  // "LibCell"
            string lname = rd.str();
            ld w = rd.real(), hgt = rd.real();
            int lid = getLib(lname);
            if (lid >= (int)techArea[t].size()) techArea[t].resize(lid + 1, 0);
            techArea[t][lid] = w * hgt;
        }
    }
    // normalize tech area tables to full lib size
    int numLib = (int)libId.size();
    for (int t = 0; t < numTechs; ++t)
        if ((int)techArea[t].size() < numLib) techArea[t].resize(numLib, 0);

    rd.str();                          // "DieSize"
    ld dieW = rd.real(), dieH = rd.real();
    ld dieArea = dieW * dieH;

    rd.str(); string techA = rd.str(); ld utilA = rd.real() / 100.0L;
    rd.str(); string techB = rd.str(); ld utilB = rd.real() / 100.0L;
    int tA = techId[techA], tB = techId[techB];
    ld capA = utilA * dieArea;
    ld capB = utilB * dieArea;

    rd.str();                          // "NumCells"
    int numCells = (int)rd.i64();
    vector<string> cellName(numCells);
    unordered_map<string, int> cellId;
    cellId.reserve(numCells * 2);
    HG h;
    h.n = numCells;
    h.aA.assign(numCells, 0);
    h.aB.assign(numCells, 0);
    for (int i = 0; i < numCells; ++i) {
        rd.str();                      // "Cell"
        string cn = rd.str();
        string ln = rd.str();
        cellName[i] = cn;
        cellId[cn] = i;
        int lid = getLib(ln);
        h.aA[i] = (lid < (int)techArea[tA].size()) ? techArea[tA][lid] : 0;
        h.aB[i] = (lid < (int)techArea[tB].size()) ? techArea[tB][lid] : 0;
    }

    rd.str();                          // "NumNets"
    int numNets = (int)rd.i64();
    h.netOff.reserve(numNets + 1);
    h.netOff.push_back(0);
    h.netPin.reserve(numNets * 3);
    for (int e = 0; e < numNets; ++e) {
        rd.str();                      // "Net"
        rd.str();                      // net name
        int deg = (int)rd.i64();
        for (int k = 0; k < deg; ++k) {
            rd.str();                  // "Cell"
            string cn = rd.str();
            auto it = cellId.find(cn);
            if (it != cellId.end()) h.netPin.push_back(it->second);
        }
        h.netOff.push_back((int)h.netPin.size());
    }
    h.m = (int)h.netOff.size() - 1;
    h.buildNodeCSR();

    // --- solve: parallel multi-start multilevel ---
    int nthreads = 1;
#ifdef _OPENMP
    nthreads = omp_get_max_threads();
#endif

    vector<int> bestPart;
    long long bestCut = -1;
    std::atomic<long long> startCtr{0};
    // Time-budgeted parallel multi-start: each thread keeps launching fresh
    // multilevel runs (unique deterministic seed per start index) until the
    // global time limit, keeping the best feasible cut. Reserve a tail margin
    // so even the largest instance's last run can finish + write output.
    double launchLimit = TIME_LIMIT - 2.0;
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        while (true) {
            if (elapsed() > launchLimit) break;
            long long s = startCtr.fetch_add(1);
            // diversify balance tolerance across starts: some tight (good when
            // the cap has slack), some loose (essential under tight balance,
            // e.g. public5). best feasible cut across the pool is kept.
            static const double RELAXES[4] = {0.02, 0.05, 0.10, 0.20};
            double relax = RELAXES[s & 3];
            vector<int> part; long long cut;
            if (multilevelRun(h, capA, capB, part, cut,
                              0x9E3779B97F4A7C15ULL * (uint64_t)(s + 1) + 12345, relax)) {
#ifdef _OPENMP
                #pragma omp critical
#endif
                {
                    if (bestCut < 0 || cut < bestCut) { bestCut = cut; bestPart = std::move(part); }
                }
            }
            // a single run already exceeding the budget: stop (huge instance)
            if (elapsed() > launchLimit) break;
        }
    }

    // guaranteed feasible fallback if every start failed / timed out
    if (bestCut < 0) {
        std::mt19937_64 rng(777);
        vector<int> part;
        if (!greedyInit(h, capA, capB, part, rng, false)) {
            // last resort: relaxed pack ignoring nothing fits (should not happen)
            part.assign(h.n, 0);
            ld useA = 0;
            for (int i = 0; i < h.n; ++i) {
                if (useA + h.aA[i] <= capA) { part[i] = 0; useA += h.aA[i]; }
                else part[i] = 1;
            }
        }
        FM fm(h, capA, capB, part);
        fm.refine(20);
        bestPart = part;
        bestCut = cutSize(h, bestPart);
    }

    // --- verify feasibility; repair if needed ---
    {
        ld useA = 0, useB = 0;
        for (int i = 0; i < h.n; ++i)
            if (bestPart[i] == 0) useA += h.aA[i]; else useB += h.aB[i];
        if (useA > capA || useB > capB) {
            // greedy repair: move overflowing cells to the other die if it fits
            std::mt19937_64 rng(999);
            vector<int> p2;
            if (greedyInit(h, capA, capB, p2, rng, false)) {
                FM fm(h, capA, capB, p2);
                fm.refine(20);
                long long c2 = cutSize(h, p2);
                bestPart = p2; bestCut = c2;
            }
        }
    }

    // --- output ---
    long long na = 0, nb = 0;
    for (int i = 0; i < h.n; ++i) (bestPart[i] == 0 ? na : nb)++;

    FILE* out = fopen(argv[2], "wb");
    if (!out) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
    // big buffer for fast write
    static vector<char> obuf;
    obuf.reserve((size_t)numCells * 12 + 64);
    auto emit = [&](const char* s, size_t l) { obuf.insert(obuf.end(), s, s + l); };
    auto emitNum = [&](long long v) { char t[24]; int l = snprintf(t, sizeof t, "%lld", v); emit(t, l); };

    emit("CutSize ", 8); emitNum(bestCut); emit("\n", 1);
    emit("DieA ", 5); emitNum(na); emit("\n", 1);
    for (int i = 0; i < h.n; ++i) if (bestPart[i] == 0) { emit(cellName[i].data(), cellName[i].size()); emit("\n", 1); }
    emit("DieB ", 5); emitNum(nb); emit("\n", 1);
    for (int i = 0; i < h.n; ++i) if (bestPart[i] == 1) { emit(cellName[i].data(), cellName[i].size()); emit("\n", 1); }
    fwrite(obuf.data(), 1, obuf.size(), out);
    fclose(out);

    fprintf(stderr, "cells=%d nets=%d cut=%lld A=%lld B=%lld time=%.1fs\n",
            h.n, h.m, bestCut, na, nb, elapsed());
    return 0;
}
