// 001-partitioning — Multi-Technology Die Partitioning (fable-5)
// Multilevel FM + parallel multi-start hypergraph bipartitioning.
// Build: g++ -std=c++20 -O3 -fopenmp -pthread -static -o hw2 main.cpp
// Usage: hw2 <input.txt> <output.out>
//
// Legality/metric semantics mirror scorer/lib/partitioning.py (constitution R6):
//   legal  iff full exclusive coverage && usedArea_d/(W*H) <= cap_d (+1e-9), per-die Tech areas
//   metric  = #nets touching both dies (deduped identical nets carry weight = multiplicity)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <chrono>
#include <charconv>
#include <filesystem>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using ll = long long;
using Clock = chrono::steady_clock;

static Clock::time_point T0;
static double DEADLINE = 282.0; // ~285s budget minus output margin (research §8)
static inline double elapsedS() { return chrono::duration<double>(Clock::now() - T0).count(); }
static inline bool timeUp() { return elapsedS() > DEADLINE; }

// ---------------------------------------------------------------- hypergraph

struct HG {
    int n = 0;                 // cells
    int m = 0;                 // nets (deg>=2 after dedup)
    vector<int> cOff, cPins;   // cell -> incident net ids (CSR)
    vector<int> nOff, nPins;   // net  -> member cell ids (CSR)
    vector<int> w;             // net weight (multiplicity)
    vector<double> aA, aB;     // per-cell area on DieA / DieB tech
    double capA = 0, capB = 0; // absolute area caps (= util * W*H)
};

static void buildCellCSR(HG& g) {
    g.cOff.assign(g.n + 1, 0);
    for (int e = 0; e < g.m; e++)
        for (int k = g.nOff[e]; k < g.nOff[e + 1]; k++) g.cOff[g.nPins[k] + 1]++;
    for (int i = 0; i < g.n; i++) g.cOff[i + 1] += g.cOff[i];
    g.cPins.assign(g.nOff[g.m], 0);
    vector<int> cur(g.cOff.begin(), g.cOff.end() - 1);
    for (int e = 0; e < g.m; e++)
        for (int k = g.nOff[e]; k < g.nOff[e + 1]; k++) g.cPins[cur[g.nPins[k]]++] = e;
}

// Contract nets through cell->cluster map `cl` (identity allowed): per-net pin
// dedup, drop deg<2, merge identical nets accumulating weight.
static void contractNets(const vector<int>& srcOff, const vector<int>& srcPins,
                         const vector<int>* srcW, const vector<int>& cl, HG& out) {
    int srcM = (int)srcOff.size() - 1;
    vector<int> noff;  noff.reserve(srcM + 1);  noff.push_back(0);
    vector<int> npins; npins.reserve(srcPins.size());
    vector<int> wts;   wts.reserve(srcM);
    unordered_map<uint64_t, int> firstOf;
    firstOf.reserve((size_t)srcM * 2);
    vector<int> chain; chain.reserve(srcM);
    vector<int> tmp;
    for (int e = 0; e < srcM; e++) {
        tmp.clear();
        for (int k = srcOff[e]; k < srcOff[e + 1]; k++) tmp.push_back(cl[srcPins[k]]);
        sort(tmp.begin(), tmp.end());
        tmp.erase(unique(tmp.begin(), tmp.end()), tmp.end());
        if ((int)tmp.size() < 2) continue;
        uint64_t h = 1469598103934665603ull;
        for (int x : tmp) { h ^= (uint64_t)(x + 1); h *= 1099511628211ull; }
        int we = srcW ? (*srcW)[e] : 1;
        auto it = firstOf.find(h);
        int idx = (it == firstOf.end()) ? -1 : it->second;
        bool dup = false;
        while (idx != -1) {
            int len = noff[idx + 1] - noff[idx];
            if (len == (int)tmp.size() &&
                equal(tmp.begin(), tmp.end(), npins.begin() + noff[idx])) {
                wts[idx] += we; dup = true; break;
            }
            idx = chain[idx];
        }
        if (!dup) {
            int ni = (int)wts.size();
            chain.push_back(it == firstOf.end() ? -1 : it->second);
            firstOf[h] = ni;
            wts.push_back(we);
            npins.insert(npins.end(), tmp.begin(), tmp.end());
            noff.push_back((int)npins.size());
        }
    }
    out.m = (int)wts.size();
    out.nOff = move(noff);
    out.nPins = move(npins);
    out.w = move(wts);
    buildCellCSR(out);
}

// ------------------------------------------------------------------ FM engine

static constexpr int BIGNET = 3000; // nets this large are tracked in counts but skipped for gains

struct FM {
    const HG* g = nullptr;
    vector<uint8_t> side;          // 0=A, 1=B
    vector<int> cntA, cntB;        // per-net side counts
    vector<int> gain;
    vector<int> head, nxt, prv;    // gain buckets (doubly linked)
    vector<uint8_t> inb;           // cell currently in a bucket
    vector<int> moves;
    double usedA = 0, usedB = 0;
    ll cut = 0;
    int off = 0, maxPtr = -1;

    void init(const HG* hg) {
        g = hg;
        int n = g->n, m = g->m;
        side.assign(n, 0);
        cntA.assign(m, 0); cntB.assign(m, 0);
        gain.assign(n, 0);
        nxt.assign(n, -1); prv.assign(n, -1); inb.assign(n, 0);
        ll mx = 1;
        for (int c = 0; c < n; c++) {
            ll s = 0;
            for (int k = g->cOff[c]; k < g->cOff[c + 1]; k++) {
                int e = g->cPins[k];
                if (g->nOff[e + 1] - g->nOff[e] < BIGNET) s += g->w[e];
            }
            if (s > mx) mx = s;
        }
        off = (int)mx;
        head.assign(2 * (size_t)mx + 1, -1);
    }

    void rebuild() {
        usedA = usedB = 0;
        for (int c = 0; c < g->n; c++) {
            if (side[c]) usedB += g->aB[c]; else usedA += g->aA[c];
        }
        fill(cntA.begin(), cntA.end(), 0);
        fill(cntB.begin(), cntB.end(), 0);
        for (int e = 0; e < g->m; e++)
            for (int k = g->nOff[e]; k < g->nOff[e + 1]; k++) {
                if (side[g->nPins[k]]) cntB[e]++; else cntA[e]++;
            }
        cut = 0;
        for (int e = 0; e < g->m; e++)
            if (cntA[e] > 0 && cntB[e] > 0) cut += g->w[e];
    }

    void setSide(const vector<uint8_t>& s) { side = s; rebuild(); }
    bool legal() const { return usedA <= g->capA && usedB <= g->capB; }

    void binsert(int c) {
        int b = gain[c] + off;
        nxt[c] = head[b]; prv[c] = -1;
        if (head[b] >= 0) prv[head[b]] = c;
        head[b] = c;
        if (b > maxPtr) maxPtr = b;
        inb[c] = 1;
    }
    void bremove(int c) {
        int b = gain[c] + off;
        if (prv[c] >= 0) nxt[prv[c]] = nxt[c]; else head[b] = nxt[c];
        if (nxt[c] >= 0) prv[nxt[c]] = prv[c];
        inb[c] = 0;
    }
    void gup(int c, int d) { bremove(c); gain[c] += d; binsert(c); }

    // Flip cell c; maintain counts, areas, exact cut; optionally FM gain updates.
    void moveCore(int c, bool updateGains) {
        int from = side[c], to = 1 - from;
        side[c] = (uint8_t)to;
        if (from == 0) { usedA -= g->aA[c]; usedB += g->aB[c]; }
        else           { usedB -= g->aB[c]; usedA += g->aA[c]; }
        for (int k = g->cOff[c]; k < g->cOff[c + 1]; k++) {
            int e = g->cPins[k];
            int W = g->w[e];
            int deg = g->nOff[e + 1] - g->nOff[e];
            bool small = deg < BIGNET;
            int& cA = cntA[e];
            int& cB = cntB[e];
            bool was = cA > 0 && cB > 0;
            int Tbefore = (to == 0) ? cA : cB;
            if (updateGains && small) {
                if (Tbefore == 0) {
                    for (int kk = g->nOff[e]; kk < g->nOff[e + 1]; kk++) {
                        int u = g->nPins[kk];
                        if (u != c && inb[u]) gup(u, +W);
                    }
                } else if (Tbefore == 1) {
                    for (int kk = g->nOff[e]; kk < g->nOff[e + 1]; kk++) {
                        int u = g->nPins[kk];
                        if (u != c && side[u] == (uint8_t)to) { if (inb[u]) gup(u, -W); break; }
                    }
                }
            }
            if (from == 0) { cA--; cB++; } else { cB--; cA++; }
            int Fafter = (from == 0) ? cA : cB;
            if (updateGains && small) {
                if (Fafter == 0) {
                    for (int kk = g->nOff[e]; kk < g->nOff[e + 1]; kk++) {
                        int u = g->nPins[kk];
                        if (u != c && inb[u]) gup(u, -W);
                    }
                } else if (Fafter == 1) {
                    for (int kk = g->nOff[e]; kk < g->nOff[e + 1]; kk++) {
                        int u = g->nPins[kk];
                        if (u != c && side[u] == (uint8_t)from) { if (inb[u]) gup(u, +W); break; }
                    }
                }
            }
            bool now = cA > 0 && cB > 0;
            if (now != was) cut += now ? W : -W;
        }
    }

    // One FM pass with roll-back-to-best-prefix. Returns cut improvement (>=0).
    ll pass() {
        int n = g->n;
        for (int c = 0; c < n; c++) {
            ll gg = 0;
            int s = side[c];
            for (int k = g->cOff[c]; k < g->cOff[c + 1]; k++) {
                int e = g->cPins[k];
                if (g->nOff[e + 1] - g->nOff[e] >= BIGNET) continue;
                int F = (s == 0) ? cntA[e] : cntB[e];
                int T = (s == 0) ? cntB[e] : cntA[e];
                if (F == 1) gg += g->w[e];
                if (T == 0) gg -= g->w[e];
            }
            gain[c] = (int)gg;
        }
        fill(head.begin(), head.end(), -1);
        maxPtr = -1;
        for (int c = 0; c < n; c++) binsert(c);
        ll startCut = cut, bestCut = cut;
        size_t bestIdx = 0;
        moves.clear();
        ll stall = max<ll>(3000, (ll)n / 8);
        int ctr = 0;
        while (true) {
            if (((++ctr) & 1023) == 0 && timeUp()) break;
            while (maxPtr >= 0 && head[maxPtr] < 0) maxPtr--;
            if (maxPtr < 0) break;
            int c = head[maxPtr];
            int s = side[c];
            double a = (s == 0) ? g->aB[c] : g->aA[c];
            double ut = (s == 0) ? usedB : usedA;
            double cap = (s == 0) ? g->capB : g->capA;
            if (ut + a > cap) { bremove(c); continue; } // infeasible: lock out this pass
            bremove(c);
            moveCore(c, true);
            moves.push_back(c);
            if (cut < bestCut) { bestCut = cut; bestIdx = moves.size(); }
            if ((ll)(moves.size() - bestIdx) > stall) break;
        }
        for (size_t i = moves.size(); i > bestIdx; i--) moveCore(moves[i - 1], false);
        cut = bestCut;
        (void)startCut;
        return startCut - bestCut;
    }

    ll run(int maxPasses) {
        ll tot = 0;
        for (int p = 0; p < maxPasses; p++) {
            if (timeUp()) break;
            ll im = pass();
            tot += im;
            if (im <= 0) break;
        }
        return tot;
    }
};

// ------------------------------------------------------------- initial split

// Feasibility-first greedy (research §3). seed 0 = deterministic natural order;
// other seeds shuffle the order and (seed%4!=1) add random preference flips.
static bool greedyInit(const HG& g, vector<uint8_t>& side, uint32_t seed) {
    int n = g.n;
    side.assign(n, 0);
    vector<int> order(n);
    for (int i = 0; i < n; i++) order[i] = i;
    mt19937 rng(seed * 2654435761u + 12345u);
    if (seed != 0)
        for (int i = n - 1; i > 0; i--) { int j = (int)(rng() % (uint32_t)(i + 1)); swap(order[i], order[j]); }
    int flipPct = (seed == 0 || seed % 4 == 1) ? 0 : 12;
    bool balanceMode = seed != 0 && seed % 3 == 2; // fill both dies evenly (slack for FM)
    double uA = 0, uB = 0;
    for (int oi = 0; oi < n; oi++) {
        int c = order[oi];
        bool preferA = balanceMode
                           ? (uA + g.aA[c]) * g.capB <= (uB + g.aB[c]) * g.capA
                           : g.aA[c] * g.capB <= g.aB[c] * g.capA; // aA/capA <= aB/capB
        if (flipPct && (int)(rng() % 100) < flipPct) preferA = !preferA;
        int pref = preferA ? 0 : 1;
        int chosen = -1;
        for (int t = 0; t < 2 && chosen < 0; t++) {
            int s = (t == 0) ? pref : 1 - pref;
            double a = (s == 0) ? g.aA[c] : g.aB[c];
            double u = (s == 0) ? uA : uB;
            double cap = (s == 0) ? g.capA : g.capB;
            if (u + a <= cap) chosen = s;
        }
        if (chosen < 0) { // neither fits: smaller relative overflow, repair below
            double oa = (uA + g.aA[c]) / g.capA, ob = (uB + g.aB[c]) / g.capB;
            chosen = (oa <= ob) ? 0 : 1;
        }
        side[c] = (uint8_t)chosen;
        if (chosen == 0) uA += g.aA[c]; else uB += g.aB[c];
    }
    if (uA <= g.capA && uB <= g.capB) return true;
    for (int s = 0; s < 2; s++) { // repair: move largest cells off the overfull die
        double& u = (s == 0) ? uA : uB;
        double cap = (s == 0) ? g.capA : g.capB;
        double& v = (s == 0) ? uB : uA;
        double capO = (s == 0) ? g.capB : g.capA;
        if (u <= cap) continue;
        vector<pair<double, int>> cs;
        for (int c = 0; c < n; c++)
            if (side[c] == (uint8_t)s) cs.push_back({(s == 0) ? g.aA[c] : g.aB[c], c});
        sort(cs.rbegin(), cs.rend());
        for (auto& pr : cs) {
            if (u <= cap) break;
            int c = pr.second;
            double ao = (s == 0) ? g.aB[c] : g.aA[c];
            if (v + ao <= capO) { side[c] = (uint8_t)(1 - s); u -= pr.first; v += ao; }
        }
        if (u > cap) return false;
    }
    return true;
}

// Parallel multi-start: K restarts of (greedy init + FM), keep best legal (research §4).
static bool multistart(const HG& g, vector<uint8_t>& bestSide, uint32_t seedBase, int K,
                       int maxPasses) {
    vector<ll> cuts((size_t)K, LLONG_MAX);
    vector<vector<uint8_t>> sides((size_t)K);
#pragma omp parallel for schedule(dynamic, 1)
    for (int r = 0; r < K; r++) {
        vector<uint8_t> s;
        uint32_t seed = (r == 0) ? 0u : seedBase * 977u + (uint32_t)r;
        if (!greedyInit(g, s, seed)) continue;
        FM fm;
        fm.init(&g);
        fm.setSide(s);
        if (!fm.legal()) continue;
        fm.run(maxPasses);
        cuts[r] = fm.cut;
        sides[r] = move(fm.side);
    }
    int best = -1;
    for (int r = 0; r < K; r++)
        if (cuts[r] != LLONG_MAX && (best < 0 || cuts[r] < cuts[best])) best = r;
    if (best < 0) return false;
    bestSide = move(sides[best]);
    return true;
}

// ----------------------------------------------------------------- multilevel

static constexpr int COARSE_N = 4000;

// First-choice heavy-edge clustering (research §5). Fills cl (fine->coarse) and out.
// If `lock` is given (V-cycle mode), only same-side cells may merge, so the
// projected partition survives coarsening exactly.
static void coarsen(const HG& f, uint32_t seed, HG& out, vector<int>& cl,
                    const vector<uint8_t>* lock = nullptr) {
    int n = f.n;
    cl.assign(n, -1);
    vector<int> order(n);
    for (int i = 0; i < n; i++) order[i] = i;
    mt19937 rng(seed ^ 0x9e3779b9u);
    for (int i = n - 1; i > 0; i--) { int j = (int)(rng() % (uint32_t)(i + 1)); swap(order[i], order[j]); }
    double totA = 0, totB = 0;
    for (int c = 0; c < n; c++) { totA += f.aA[c]; totB += f.aB[c]; }
    double limA = 1.5 * totA / COARSE_N, limB = 1.5 * totB / COARSE_N;
    vector<double> caA, caB;
    caA.reserve(n / 2 + 1); caB.reserve(n / 2 + 1);
    vector<double> sc(n, 0.0);
    vector<int> touched;
    touched.reserve(512);
    int nc = 0;
    for (int oi = 0; oi < n; oi++) {
        int v = order[oi];
        if (cl[v] != -1) continue;
        for (int k = f.cOff[v]; k < f.cOff[v + 1]; k++) {
            int e = f.cPins[k];
            int deg = f.nOff[e + 1] - f.nOff[e];
            if (deg > 64) continue; // negligible connectivity signal, O(deg^2) cost
            double wgt = (double)f.w[e] / (deg - 1);
            for (int kk = f.nOff[e]; kk < f.nOff[e + 1]; kk++) {
                int u = f.nPins[kk];
                if (u == v) continue;
                if (sc[u] == 0.0) touched.push_back(u);
                sc[u] += wgt;
            }
        }
        int best = -1;
        double bestSc = 0;
        for (int u : touched) {
            double s = sc[u];
            if (s < bestSc || (s == bestSc && best != -1 && u >= best)) continue;
            if (lock && (*lock)[u] != (*lock)[v]) continue;
            double cA2 = f.aA[v] + (cl[u] == -1 ? f.aA[u] : caA[cl[u]]);
            double cB2 = f.aB[v] + (cl[u] == -1 ? f.aB[u] : caB[cl[u]]);
            if (cA2 > limA || cB2 > limB) continue;
            best = u; bestSc = s;
        }
        if (best != -1) {
            if (cl[best] == -1) {
                cl[best] = cl[v] = nc;
                caA.push_back(f.aA[v] + f.aA[best]);
                caB.push_back(f.aB[v] + f.aB[best]);
                nc++;
            } else {
                int k2 = cl[best];
                cl[v] = k2;
                caA[k2] += f.aA[v];
                caB[k2] += f.aB[v];
            }
        } else {
            cl[v] = nc;
            caA.push_back(f.aA[v]);
            caB.push_back(f.aB[v]);
            nc++;
        }
        for (int u : touched) sc[u] = 0.0;
        touched.clear();
    }
    out.n = nc;
    out.aA = move(caA);
    out.aB = move(caB);
    out.capA = f.capA;
    out.capB = f.capB;
    contractNets(f.nOff, f.nPins, &f.w, cl, out);
}

// Iterated local search: perturb (random feasible flips) + FM, keep-if-better.
static void ils(const HG& g, vector<uint8_t>& side, ll& cut, int rounds, uint32_t seed) {
    FM fm;
    fm.init(&g);
    fm.setSide(side);
    fm.run(60);
    vector<uint8_t> best = fm.side;
    ll bestC = fm.cut;
    mt19937 rng(seed * 0x85ebca6bu + 7u);
    int n = g.n;
    for (int it = 0; it < rounds && !timeUp(); it++) {
        fm.setSide(best);
        int k = 1 + (int)(rng() % (uint32_t)max(2, n / 30));
        for (int j = 0; j < k; j++) {
            if (rng() & 1u) { // single flip
                int c = (int)(rng() % (uint32_t)n);
                int s = fm.side[c];
                double a = (s == 0) ? g.aB[c] : g.aA[c];
                double ut = (s == 0) ? fm.usedB : fm.usedA;
                double cap = (s == 0) ? g.capB : g.capA;
                if (ut + a <= cap) fm.moveCore(c, false);
            } else { // pairwise swap: works even when caps leave no single-move slack
                int a1 = (int)(rng() % (uint32_t)n), b1 = (int)(rng() % (uint32_t)n);
                if (fm.side[a1] == fm.side[b1]) continue;
                if (fm.side[a1] != 0) swap(a1, b1);
                double nA = fm.usedA - g.aA[a1] + g.aA[b1];
                double nB = fm.usedB + g.aB[a1] - g.aB[b1];
                if (nA <= g.capA && nB <= g.capB) {
                    fm.moveCore(a1, false);
                    fm.moveCore(b1, false);
                }
            }
        }
        fm.run(60);
        if (fm.cut < bestC) { bestC = fm.cut; best = fm.side; }
    }
    side = move(best);
    cut = bestC;
}

// One multilevel cycle: coarsen to ~COARSE_N, init (multi-start, or projected
// `warm` partition for V-cycles), refine down level by level.
static bool mlCycle(const HG& g0, uint32_t seed, const vector<uint8_t>* warm,
                    vector<uint8_t>& outSide, ll& outCut) {
    vector<HG> Ls;
    vector<vector<int>> maps;
    const HG* cur = &g0;
    vector<uint8_t> warmBuf;
    const vector<uint8_t>* curWarm = warm;
    while (cur->n > COARSE_N && (int)Ls.size() < 40 && !timeUp()) {
        HG nl;
        vector<int> cl;
        coarsen(*cur, seed + (uint32_t)Ls.size() * 7919u, nl, cl, curWarm);
        if (nl.n > (ll)cur->n * 95 / 100) break; // coarsening stalled
        if (curWarm) {
            vector<uint8_t> ws((size_t)nl.n);
            for (int i = 0; i < cur->n; i++) ws[cl[i]] = (*curWarm)[i];
            warmBuf = move(ws);
            curWarm = &warmBuf;
        }
        Ls.push_back(move(nl));
        maps.push_back(move(cl));
        cur = &Ls.back();
    }
    int top = (int)Ls.size() - 1;
    vector<uint8_t> side;
    bool ok = false;
    if (curWarm && top >= 0) { // V-cycle: refine the projected partition
        side = *curWarm;
        FM fm;
        fm.init(&Ls[top]);
        fm.setSide(side);
        if (fm.legal()) {
            fm.run(60);
            side = move(fm.side);
            ll c0;
            ils(Ls[top], side, c0, 100, seed ^ 0xabcu);
            ok = true;
        }
    }
    while (!ok && top >= 0) {
        ok = multistart(Ls[top], side, seed * 31 + 7, 24, 60);
        if (ok) {
            ll c0;
            ils(Ls[top], side, c0, 100, seed ^ 0xabcu);
            break;
        }
        Ls.pop_back();
        maps.pop_back();
        top--;
    }
    if (top < 0) { // could not even init on a coarse level: flat greedy on g0
        if (!greedyInit(g0, side, 0)) return false;
        FM fm;
        fm.init(&g0);
        fm.setSide(side);
        if (!fm.legal()) return false;
        fm.run(20);
        outSide = move(fm.side);
        outCut = fm.cut;
        return true;
    }
    for (int li = top; li >= 0; li--) {
        const HG& fine = (li == 0) ? g0 : Ls[li - 1];
        vector<uint8_t> fs((size_t)fine.n);
        const vector<int>& mp = maps[li];
        for (int i = 0; i < fine.n; i++) fs[i] = side[mp[i]];
        side = move(fs);
        FM fm;
        fm.init(&fine);
        fm.setSide(side);
        int mp2 = fine.n > 1000000 ? 8 : (fine.n > 200000 ? 12 : 60);
        fm.run(mp2);
        side = move(fm.side);
        if (timeUp()) {
            // project the partition straight to the finest level (stays legal)
            for (int lj = li - 1; lj >= 0; lj--) {
                const HG& fn = (lj == 0) ? g0 : Ls[lj - 1];
                vector<uint8_t> f2((size_t)fn.n);
                for (int i = 0; i < fn.n; i++) f2[i] = side[maps[lj][i]];
                side = move(f2);
            }
            break;
        }
    }
    FM fm;
    fm.init(&g0);
    fm.setSide(side);
    if (!fm.legal()) return false;
    outSide = move(fm.side);
    outCut = fm.cut;
    return true;
}

// ---------------------------------------------------------------------- main

static string FILEBUF; // input bytes; cell names are string_views into this

int main(int argc, char** argv) {
    T0 = Clock::now();
    if (argc < 3) {
        fprintf(stderr, "usage: %s <input.txt> <output.out>\n", argv[0]);
        return 1;
    }

    { // read whole input
        FILE* fp = fopen(argv[1], "rb");
        if (!fp) { fprintf(stderr, "cannot open input %s\n", argv[1]); return 1; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        FILEBUF.resize((size_t)sz);
        if (sz > 0 && fread(FILEBUF.data(), 1, (size_t)sz, fp) != (size_t)sz) {
            fprintf(stderr, "read error\n");
            fclose(fp);
            return 1;
        }
        fclose(fp);
    }

    size_t pos = 0, SZ = FILEBUF.size();
    auto token = [&]() -> string_view {
        while (pos < SZ && (unsigned char)FILEBUF[pos] <= ' ') pos++;
        size_t st = pos;
        while (pos < SZ && (unsigned char)FILEBUF[pos] > ' ') pos++;
        return string_view(FILEBUF).substr(st, pos - st);
    };
    auto tokI = [&]() -> ll {
        string_view t = token();
        ll v = 0;
        from_chars(t.data(), t.data() + t.size(), v);
        return v;
    };
    auto tokD = [&]() -> double {
        string_view t = token();
        double v = 0;
        from_chars(t.data(), t.data() + t.size(), v);
        return v;
    };

    // ---- parse (contracts/io-format.md grammar)
    token(); // NumTechs
    int nt = (int)tokI();
    unordered_map<string_view, int> libId, techId;
    vector<vector<double>> techArea((size_t)nt);
    for (int t = 0; t < nt; t++) {
        token(); // Tech
        techId[token()] = t;
        int k = (int)tokI();
        for (int j = 0; j < k; j++) {
            token(); // LibCell
            string_view ln = token();
            double w = tokD(), h = tokD();
            auto it = libId.try_emplace(ln, (int)libId.size()).first;
            if ((int)techArea[t].size() <= it->second) techArea[t].resize(it->second + 1, 0.0);
            techArea[t][it->second] = w * h;
        }
    }
    token(); // DieSize
    double dieW = tokD(), dieH = tokD();
    token(); // DieA
    int techA = techId[token()];
    double capA = tokD() / 100.0 * dieW * dieH;
    token(); // DieB
    int techB = techId[token()];
    double capB = tokD() / 100.0 * dieW * dieH;
    token(); // NumCells
    int nCells = (int)tokI();
    vector<string_view> cellName((size_t)nCells);
    vector<int> cellLib((size_t)nCells);
    unordered_map<string_view, int> cellId;
    cellId.reserve((size_t)nCells * 2);
    for (int i = 0; i < nCells; i++) {
        token(); // Cell
        cellName[i] = token();
        cellId[cellName[i]] = i;
        string_view lb = token();
        cellLib[i] = libId[lb];
    }
    token(); // NumNets
    int nNets = (int)tokI();
    vector<int> rawOff;
    rawOff.reserve((size_t)nNets + 1);
    rawOff.push_back(0);
    vector<int> rawPins;
    rawPins.reserve((size_t)nNets * 3);
    for (int e = 0; e < nNets; e++) {
        token(); // Net
        token(); // name (unused)
        int d = (int)tokI();
        for (int j = 0; j < d; j++) {
            token(); // Cell
            rawPins.push_back(cellId[token()]);
        }
        rawOff.push_back((int)rawPins.size());
    }

    HG g;
    g.n = nCells;
    g.capA = capA;
    g.capB = capB;
    g.aA.resize((size_t)nCells);
    g.aB.resize((size_t)nCells);
    for (int i = 0; i < nCells; i++) {
        g.aA[i] = techArea[techA][cellLib[i]];
        g.aB[i] = techArea[techB][cellLib[i]];
    }
    {
        vector<int> ident((size_t)nCells);
        for (int i = 0; i < nCells; i++) ident[i] = i;
        contractNets(rawOff, rawPins, nullptr, ident, g);
        rawOff.clear(); rawOff.shrink_to_fit();
        rawPins.clear(); rawPins.shrink_to_fit();
    }
    fprintf(stderr, "[parse] cells=%d rawNets=%d nets=%d pins=%zu  %.2fs\n",
            nCells, nNets, g.m, g.nPins.size(), elapsedS());

    // ---- solve
    vector<uint8_t> bestSide;
    ll bestCut = LLONG_MAX;
    double totA = 0, totB = 0;
    for (int i = 0; i < nCells; i++) { totA += g.aA[i]; totB += g.aB[i]; }
    if (totA <= capA) { // everything fits on one die -> cut 0
        bestSide.assign((size_t)nCells, 0);
        bestCut = 0;
    } else if (totB <= capB) {
        bestSide.assign((size_t)nCells, 1);
        bestCut = 0;
    } else if (nCells <= 3 * COARSE_N) {
        // flat parallel multi-start FM + parallel ILS chains
        int K = nCells <= 3000 ? 128 : 64;
        if (multistart(g, bestSide, 1, K, 60)) {
            FM fm;
            fm.init(&g);
            fm.setSide(bestSide);
            bestCut = fm.cut;
            const int C = 8;
            int rounds = nCells <= 3000 ? 600 : 300;
            vector<ll> cuts((size_t)C, LLONG_MAX);
            vector<vector<uint8_t>> sides((size_t)C);
#pragma omp parallel for schedule(dynamic, 1)
            for (int ch = 0; ch < C; ch++) {
                vector<uint8_t> s = bestSide;
                ll c = bestCut;
                ils(g, s, c, rounds, 1000u + (uint32_t)ch);
                cuts[ch] = c;
                sides[ch] = move(s);
            }
            for (int ch = 0; ch < C; ch++)
                if (cuts[ch] < bestCut) { bestCut = cuts[ch]; bestSide = sides[ch]; }
        }
    } else {
        ll P = (ll)g.nPins.size();
        int runs = P > 2000000 ? 12 : P > 500000 ? 20 : 32;
        for (int r = 0; r < runs; r++) {
            if (timeUp()) break;
            vector<uint8_t> s;
            ll c;
            if (!mlCycle(g, (uint32_t)r * 131u + 1u, nullptr, s, c)) continue;
            int fails = 0; // per-run V-cycles until 3 consecutive non-improvements
            for (int v = 0; v < 8 && fails < 3 && !timeUp(); v++) {
                vector<uint8_t> s2;
                ll c2;
                if (!mlCycle(g, (uint32_t)(r * 131 + 1000 + v * 17), &s, s2, c2)) break;
                if (c2 < c) { c = c2; s = move(s2); fails = 0; } else fails++;
            }
            fprintf(stderr, "[ML run %d] cut=%lld  %.2fs\n", r, c, elapsedS());
            if (c < bestCut) { bestCut = c; bestSide = move(s); }
        }
        int fails = 0; // final V-cycles on the global best
        for (int v = 0; v < 10 && fails < 3 && !timeUp(); v++) {
            vector<uint8_t> s2;
            ll c2;
            if (!mlCycle(g, (uint32_t)(5000 + v * 23), &bestSide, s2, c2)) break;
            if (c2 < bestCut) { bestCut = c2; bestSide = move(s2); fails = 0; } else fails++;
        }
        fprintf(stderr, "[ML final] cut=%lld  %.2fs\n", bestCut, elapsedS());
    }

    if (bestSide.empty()) { // last-resort legal output
        if (!greedyInit(g, bestSide, 0)) {
            fprintf(stderr, "FATAL: no feasible assignment found\n");
            return 1;
        }
    }

    // ---- verify + finalize (scorer semantics)
    double uA = 0, uB = 0;
    for (int i = 0; i < nCells; i++) {
        if (bestSide[i]) uB += g.aB[i]; else uA += g.aA[i];
    }
    if (uA > capA || uB > capB) {
        fprintf(stderr, "[warn] best illegal (uA=%.3f/%.3f uB=%.3f/%.3f); greedy fallback\n",
                uA / (dieW * dieH), capA / (dieW * dieH), uB / (dieW * dieH), capB / (dieW * dieH));
        if (!greedyInit(g, bestSide, 0)) return 1;
    }
    { // recompute exact cut from final side[]
        FM fm;
        fm.init(&g);
        fm.setSide(bestSide);
        bestCut = fm.cut;
    }

    // ---- write output (contracts/io-format.md)
    {
        error_code ec;
        filesystem::path op(argv[2]);
        if (op.has_parent_path()) filesystem::create_directories(op.parent_path(), ec);
        string out;
        out.reserve((size_t)nCells * 8 + 64);
        ll nA = 0;
        for (int i = 0; i < nCells; i++) nA += bestSide[i] == 0;
        out += "CutSize ";  out += to_string(bestCut); out += '\n';
        out += "DieA ";     out += to_string(nA);      out += '\n';
        for (int i = 0; i < nCells; i++)
            if (!bestSide[i]) { out.append(cellName[i]); out += '\n'; }
        out += "DieB ";     out += to_string((ll)nCells - nA); out += '\n';
        for (int i = 0; i < nCells; i++)
            if (bestSide[i]) { out.append(cellName[i]); out += '\n'; }
        FILE* fo = fopen(argv[2], "wb");
        if (!fo) { fprintf(stderr, "cannot open output %s\n", argv[2]); return 1; }
        fwrite(out.data(), 1, out.size(), fo);
        fclose(fo);
    }
    fprintf(stderr, "[done] cut=%lld utilA=%.4f utilB=%.4f  %.2fs\n",
            bestCut, uA / (dieW * dieH), uB / (dieW * dieH), elapsedS());
    return 0;
}
