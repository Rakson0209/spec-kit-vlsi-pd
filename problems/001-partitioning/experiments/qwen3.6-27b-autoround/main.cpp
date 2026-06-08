// Multi-Technology Die Partitioning Solver v18
// Build: g++ -std=c++20 -O3 -fopenmp -pthread -o hw2.exe main.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <omp.h>
#include <queue>
#include <numeric>
#include <limits>
#include <sys/stat.h>
#include <thread>

using namespace std;
using Clock = chrono::steady_clock;

static constexpr double EPS = 1e-9;
static constexpr double DEADLINE_SEC = 285.0;

// ─── Tokenizer ───────────────────────────────────────────────────────────────
struct Tokens {
    vector<string> toks;
    size_t pos = 0;
    Tokens(const string &data) {
        size_t i = 0, n = data.size();
        toks.reserve(n / 4);
        while (i < n) {
            while (i < n && (data[i]==' '||data[i]=='\t'||data[i]=='\n'||data[i]=='\r')) i++;
            if (i >= n) break;
            size_t s = i;
            while (i < n && data[i]!=' '&&data[i]!='\t'&&data[i]!='\n'&&data[i]!='\r') i++;
            toks.emplace_back(data, s, i - s);
        }
    }
    string next() { return toks[pos++]; }
    int nextInt() { return stoi(next()); }
    double nextDouble() { return stod(next()); }
};

// ─── State ───────────────────────────────────────────────────────────────────
struct State {
    int NC, NN;
    vector<double> areaA, areaB;
    vector<uint8_t> side;
    vector<int> cellPinVals, cellPinOff;
    vector<int> netPinVals, netPinOff;
    vector<int> countA, countB;
    double dieArea, capA, capB;
    double usedA, usedB;
    int maxDeg;
    chrono::steady_clock::time_point startTime;

    bool deadlineHit() const {
        return chrono::duration<double>(Clock::now() - startTime).count() > DEADLINE_SEC;
    }

    void recompute() {
        countA.assign(NN, 0); countB.assign(NN, 0);
        usedA = 0; usedB = 0;
        for (int i = 0; i < NC; i++) {
            if (side[i] == 0) { usedA += areaA[i]; }
            else              { usedB += areaB[i]; }
            for (int k = cellPinOff[i]; k < cellPinOff[i+1]; k++) {
                int n = cellPinVals[k];
                if (side[i] == 0) countA[n]++; else countB[n]++;
            }
        }
    }

    int computeCut() const {
        int cut = 0;
        for (int n = 0; n < NN; n++)
            if (countA[n] > 0 && countB[n] > 0) cut++;
        return cut;
    }

    int gainOf(int c) const {
        int g = 0, s = side[c];
        for (int k = cellPinOff[c]; k < cellPinOff[c+1]; k++) {
            int n = cellPinVals[k];
            int F = (s==0) ? countA[n] : countB[n];
            int T = (s==0) ? countB[n] : countA[n];
            if (F == 1) g++;
            if (T == 0) g--;
        }
        return g;
    }

    bool feasible(int c) const {
        int to = 1 - side[c];
        double a = (to==0) ? usedA : usedB;
        double cap = (to==0) ? capA : capB;
        double ca = (to==0) ? areaA[c] : areaB[c];
        return (a + ca) <= cap * dieArea + EPS;
    }

    void doMove(int c) {
        int from = side[c];
        if (from==0) { usedA -= areaA[c]; usedB += areaB[c]; }
        else         { usedB -= areaB[c]; usedA += areaA[c]; }
        for (int k = cellPinOff[c]; k < cellPinOff[c+1]; k++) {
            int n = cellPinVals[k];
            if (from==0) { countA[n]--; countB[n]++; }
            else         { countB[n]--; countA[n]++; }
        }
        side[c] = (uint8_t)(1 - from);
    }

    void undoMove(int c) {
        int cur = side[c];
        if (cur==0) { usedA -= areaA[c]; usedB += areaB[c]; }
        else        { usedB -= areaB[c]; usedA += areaA[c]; }
        for (int k = cellPinOff[c]; k < cellPinOff[c+1]; k++) {
            int n = cellPinVals[k];
            if (cur==0) { countA[n]++; countB[n]--; }
            else        { countB[n]++; countA[n]--; }
        }
        side[c] = (uint8_t)(1 - cur);
    }

    int fmPass() {
        int sz = 2 * maxDeg + 1;
        vector<vector<int>> bk(sz);
        vector<int> pos(NC, -1);
        vector<int> g(NC);

        for (int i = 0; i < NC; i++) {
            g[i] = gainOf(i);
            int b = g[i] + maxDeg;
            if (b < 0) b = 0; if (b >= sz) b = sz - 1;
            pos[i] = (int)bk[b].size();
            bk[b].push_back(i);
        }

        auto remBk = [&](int c) {
            int b = g[c] + maxDeg;
            if (b < 0) b = 0; if (b >= sz) b = sz - 1;
            int idx = pos[c];
            if (idx < 0 || idx >= (int)bk[b].size()) { pos[c] = -1; return; }
            int last = bk[b].back();
            if (idx != (int)bk[b].size()-1) { bk[b][idx] = last; pos[last] = idx; }
            bk[b].pop_back();
            pos[c] = -1;
        };

        auto insBk = [&](int c) {
            int b = g[c] + maxDeg;
            if (b < 0) b = 0; if (b >= sz) b = sz - 1;
            pos[c] = (int)bk[b].size();
            bk[b].push_back(c);
        };

        vector<bool> locked(NC, false);
        vector<int> stack;
        stack.reserve(NC);
        double cumG = 0, bestG = 0;
        int bestStep = 0, step = 0;
        int maxPtr = sz - 1;
        while (maxPtr >= 0 && bk[maxPtr].empty()) maxPtr--;

        // Run all feasible moves in the pass (not just while cumG>0)
        // This is crucial for large cases where the search space is vast
        while (maxPtr >= 0) {
            int moved = -1;
            for (int b = maxPtr; b >= 0; b--) {
                for (auto it = bk[b].begin(); it != bk[b].end(); ++it) {
                    int c = *it;
                    if (!locked[c] && feasible(c)) { moved = c; goto found; }
                }
            }
            break;

        found:
            if (moved < 0) break;

            int gv = g[moved];
            int from = side[moved];
            doMove(moved);
            locked[moved] = true;
            remBk(moved);

            for (int k = cellPinOff[moved]; k < cellPinOff[moved+1]; k++) {
                int n = cellPinVals[k];
                int cA = countA[n], cB = countB[n];
                for (int j = netPinOff[n]; j < netPinOff[n+1]; j++) {
                    int nb = netPinVals[j];
                    if (nb == moved || locked[nb]) continue;

                    int nbS = side[nb];
                    int oldF, oldT;
                    if (nbS == 1 - from) {
                        if (from == 0) { oldF = cB - 1; oldT = cA + 1; }
                        else           { oldF = cA - 1; oldT = cB + 1; }
                    } else {
                        if (from == 0) { oldF = cA + 1; oldT = cB - 1; }
                        else           { oldF = cB + 1; oldT = cA - 1; }
                    }
                    int curF = (nbS==0) ? cA : cB;
                    int curT = (nbS==0) ? cB : cA;

                    int delta = 0;
                    if (curF == 1 && oldF != 1) delta++;
                    if (oldF == 1 && curF != 1) delta--;
                    if (curT == 0 && oldT != 0) delta--;
                    if (oldT == 0 && curT != 0) delta++;

                    if (delta != 0) { remBk(nb); g[nb] += delta; insBk(nb); }
                }
            }

            stack.push_back(moved);
            cumG += gv;
            step++;
            if (cumG >= bestG) { bestG = cumG; bestStep = step; }
            if (bk[maxPtr].empty()) { while (maxPtr >= 0 && bk[maxPtr].empty()) maxPtr--; }
            if (deadlineHit()) break;
        }

        for (int i = 0; i < step - bestStep; i++)
            undoMove(stack[step - 1 - i]);

        return bestStep;
    }

    void initialPositiveGainMoves() {
        priority_queue<pair<int,int>> pq;
        for (int i = 0; i < NC; i++) pq.emplace(gainOf(i), i);

        while (!pq.empty()) {
            if (deadlineHit()) break;
            auto [gv, c] = pq.top(); pq.pop();
            if (gv <= 0) break;
            int ag = gainOf(c);
            if (ag != gv) { pq.emplace(ag, c); continue; }
            if (ag <= 0 || !feasible(c)) continue;

            int from = side[c];
            doMove(c);

            // Incremental neighbor gain update (like fmPass)
            for (int k = cellPinOff[c]; k < cellPinOff[c+1]; k++) {
                int n = cellPinVals[k];
                int cA = countA[n], cB = countB[n];
                for (int j = netPinOff[n]; j < netPinOff[n+1]; j++) {
                    int nb = netPinVals[j];
                    if (nb == c) continue;

                    int nbS = side[nb];
                    int oldF, oldT;
                    if (nbS == 1 - from) {
                        if (from == 0) { oldF = cB - 1; oldT = cA + 1; }
                        else           { oldF = cA - 1; oldT = cB + 1; }
                    } else {
                        if (from == 0) { oldF = cA + 1; oldT = cB - 1; }
                        else           { oldF = cB + 1; oldT = cA - 1; }
                    }
                    int curF = (nbS==0) ? cA : cB;
                    int curT = (nbS==0) ? cB : cA;

                    int delta = 0;
                    if (curF == 1 && oldF != 1) delta++;
                    if (oldF == 1 && curF != 1) delta--;
                    if (curT == 0 && oldT != 0) delta--;
                    if (oldT == 0 && curT != 0) delta++;

                    if (delta != 0) pq.emplace(gainOf(nb) + delta, nb);
                }
            }
        }
    }

    void fmOptimize() {
        initialPositiveGainMoves();
        if (deadlineHit()) return;

        recompute();
        int prevCut = computeCut();

        // For large graphs, allow more FM passes (they're the main optimization)
        int maxPasses = NC > 200000 ? 50 : 20;
        int passCount = 0;
        while (passCount < maxPasses) {
            int steps = fmPass();
            if (deadlineHit()) break;
            recompute();
            int newCut = computeCut();
            if (steps <= 1 || newCut >= prevCut) break;
            prevCut = newCut;
            passCount++;
        }
    }

    void greedyInit(unsigned int seed) {
        side.assign(NC, 0);
        usedA = usedB = 0;

        vector<int> order(NC);
        iota(order.begin(), order.end(), 0);

        if (seed > 0) {
            unsigned int s = seed * 2654435761u;
            for (int i = NC-1; i > 0; i--) { s = s * 48271u; int j = s % (i+1); swap(order[i], order[j]); }
        } else {
            sort(order.begin(), order.end(), [this](int a, int b) {
                return areaA[a]/capA < areaA[b]/capA;
            });
        }

        for (int idx : order) {
            double cA = areaA[idx]/capA, cB = areaB[idx]/capB;
            if (cA <= cB) {
                if (usedA + areaA[idx] <= capA*dieArea + EPS) { side[idx]=0; usedA+=areaA[idx]; }
                else { side[idx]=1; usedB+=areaB[idx]; }
            } else {
                if (usedB + areaB[idx] <= capB*dieArea + EPS) { side[idx]=1; usedB+=areaB[idx]; }
                else { side[idx]=0; usedA+=areaA[idx]; }
            }
        }
        recompute();
    }

};

// ─── Multilevel Level ────────────────────────────────────────────────────────
struct Level {
    int NC, NN;
    vector<double> areaA, areaB;
    vector<int> cellPinVals, cellPinOff;
    vector<int> netPinVals, netPinOff;
    int maxDeg;
};

// ─── Multilevel Coarsening ───────────────────────────────────────────────────
vector<int> coarsenMatching(int nC, const int *cVals, const int *cOff,
                            const int *nVals, const int *nOff) {
    vector<int> match(nC, -1);
    if (nC <= 500) return match;

    // Use random ordering for better matching coverage
    // This avoids the "all high-degree neighbors already matched" problem
    vector<int> order(nC);
    iota(order.begin(), order.end(), 0);
    unsigned int rng = 123456789u;
    for (int i = nC - 1; i > 0; i--) {
        rng = rng * 48271u;
        int j = rng % (i + 1);
        swap(order[i], order[j]);
    }

    int matched = 0;
    int stopTarget = (int)(nC * 0.50); // try to match 50% of vertices

    for (int vi = 0; vi < nC && matched < stopTarget; vi++) {
        int v = order[vi];
        if (match[v] >= 0) continue;

        int bestNb = -1;
        int bestW = 0;
        int nPins = cOff[v + 1] - cOff[v];
        int maxCheck = min(nPins, 500);
        int checked = 0;
        for (int k = cOff[v]; k < cOff[v + 1] && checked < maxCheck; k++) {
            int n = cVals[k];
            int nd = nOff[n + 1] - nOff[n];
            int nSize = min(nd, 100);
            for (int j = nOff[n]; j < nOff[n] + nSize; j++) {
                int nb = nVals[j];
                if (nb == v || match[nb] >= 0) continue;
                if (nd > bestW) { bestW = nd; bestNb = nb; }
            }
            checked++;
            if (bestNb >= 0 && checked > 10) break; // found a good edge, move on
        }
        if (bestNb >= 0) {
            match[v] = bestNb;
            match[bestNb] = v;
            matched += 2;
        }
    }
    return match;
}

Level buildCoarse(const Level &fine, const vector<int> &match) {
    int nC = fine.NC;
    vector<int> clusterOf(nC, -1);
    int cNc = 0;

    for (int i = 0; i < nC; i++) {
        if (clusterOf[i] >= 0) continue;
        if (match[i] >= 0) {
            clusterOf[i] = cNc;
            clusterOf[match[i]] = cNc;
            cNc++;
        } else {
            clusterOf[i] = cNc++;
        }
    }

    Level coarse;
    coarse.NC = cNc;
    coarse.areaA.assign(cNc, 0);
    coarse.areaB.assign(cNc, 0);

    for (int i = 0; i < nC; i++) {
        int cl = clusterOf[i];
        coarse.areaA[cl] += fine.areaA[i];
        coarse.areaB[cl] += fine.areaB[i];
    }

    // For each fine net, collect unique cluster IDs
    vector<vector<int>> tmpCm;
    tmpCm.reserve(fine.NN);
    for (int n = 0; n < fine.NN; n++) {
        vector<int> members;
        for (int j = fine.netPinOff[n]; j < fine.netPinOff[n + 1]; j++) {
            members.push_back(clusterOf[fine.netPinVals[j]]);
        }
        sort(members.begin(), members.end());
        members.erase(unique(members.begin(), members.end()), members.end());
        tmpCm.push_back(move(members));
    }
    coarse.NN = (int)tmpCm.size();

    vector<int> cCellNetCnt(cNc, 0), cNetPinCnt(coarse.NN, 0);
    for (int n = 0; n < coarse.NN; n++) {
        cNetPinCnt[n] = (int)tmpCm[n].size();
        for (int c : tmpCm[n]) cCellNetCnt[c]++;
    }

    coarse.cellPinOff.resize(cNc + 1, 0);
    for (int i = 1; i <= cNc; i++) coarse.cellPinOff[i] = coarse.cellPinOff[i - 1] + cCellNetCnt[i - 1];
    coarse.cellPinVals.resize(coarse.cellPinOff[cNc]);

    coarse.netPinOff.resize(coarse.NN + 1, 0);
    for (int n = 1; n <= coarse.NN; n++) coarse.netPinOff[n] = coarse.netPinOff[n - 1] + cNetPinCnt[n - 1];
    coarse.netPinVals.resize(coarse.netPinOff[coarse.NN]);

    vector<int> cCur(cNc, 0), nCur(coarse.NN, 0);
    for (int n = 0; n < coarse.NN; n++) {
        for (int c : tmpCm[n]) {
            coarse.cellPinVals[coarse.cellPinOff[c] + cCur[c]++] = n;
            coarse.netPinVals[coarse.netPinOff[n] + nCur[n]++] = c;
        }
    }

    coarse.maxDeg = 0;
    for (int i = 0; i < cNc; i++) {
        int d = coarse.cellPinOff[i + 1] - coarse.cellPinOff[i];
        if (d > coarse.maxDeg) coarse.maxDeg = d;
    }
    return coarse;
}

// ─── Solver ──────────────────────────────────────────────────────────────────
struct Solver {
    int NC, NN;
    vector<string> cellName;
    vector<double> areaA, areaB;
    vector<int> cellPinVals, cellPinOff;
    vector<int> netPinVals, netPinOff;
    double dieArea, capA, capB;
    int maxDeg;
    chrono::steady_clock::time_point startTime;

    void parse(const string &path) {
        ifstream ifs(path, ios::binary);
        ostringstream oss; oss << ifs.rdbuf(); string data = oss.str(); ifs.close();
        Tokens t(data);

        t.next(); int nT = t.nextInt();
        unordered_map<string, unordered_map<string,double>> TA;
        for (int i=0;i<nT;i++) {
            t.next(); string tn=t.next(); int nl=t.nextInt();
            for (int j=0;j<nl;j++) { t.next(); string ln=t.next(); TA[tn][ln]=t.nextDouble()*t.nextDouble(); }
        }
        t.next(); dieArea=t.nextDouble()*t.nextDouble();
        string tA,tB;
        t.next(); tA=t.next(); capA=t.nextDouble()/100;
        t.next(); tB=t.next(); capB=t.nextDouble()/100;
        t.next(); NC=t.nextInt();
        cellName.resize(NC); areaA.resize(NC); areaB.resize(NC);
        unordered_map<string,int> nid; nid.reserve(NC);
        for (int i=0;i<NC;i++) {
            t.next(); string cn=t.next(); string ln=t.next();
            nid[cn]=i; cellName[i]=cn;
            areaA[i]=TA[tA][ln]; areaB[i]=TA[tB][ln];
        }
        t.next(); NN=t.nextInt();
        vector<vector<int>> nm(NN), cn(NC);
        for (int n=0;n<NN;n++) {
            t.next(); t.next(); int d=t.nextInt();
            for (int j=0;j<d;j++) { t.next(); string c=t.next(); int id=nid[c]; nm[n].push_back(id); cn[id].push_back(n); }
        }
        cellPinOff.assign(NC+1,0);
        for (int i=0;i<NC;i++) cellPinOff[i+1]=cellPinOff[i]+(int)cn[i].size();
        cellPinVals.resize(cellPinOff[NC]);
        {vector<int>co(NC);for(int i=0;i<NC;i++)for(int n:cn[i])cellPinVals[cellPinOff[i]+co[i]++]=n;}
        netPinOff.assign(NN+1,0);
        for (int n=0;n<NN;n++) netPinOff[n+1]=netPinOff[n]+(int)nm[n].size();
        netPinVals.resize(netPinOff[NN]);
        {vector<int>co(NN);for(int n=0;n<NN;n++)for(int c:nm[n])netPinVals[netPinOff[n]+co[n]++]=c;}
        maxDeg=0;
        for (int i=0;i<NC;i++) maxDeg=max(maxDeg,(int)(cellPinOff[i+1]-cellPinOff[i]));
    }

    State initState() {
        State s; s.NC=NC; s.NN=NN;
        s.areaA=areaA; s.areaB=areaB;
        s.cellPinVals=cellPinVals; s.cellPinOff=cellPinOff;
        s.netPinVals=netPinVals; s.netPinOff=netPinOff;
        s.dieArea=dieArea; s.capA=capA; s.capB=capB;
        s.maxDeg=maxDeg; s.startTime=startTime;
        s.side.resize(NC); s.countA.resize(NN); s.countB.resize(NN);
        return s;
    }

    Level buildLevel() {
        Level lv;
        lv.NC=NC; lv.NN=NN;
        lv.areaA=areaA; lv.areaB=areaB;
        lv.cellPinVals=cellPinVals; lv.cellPinOff=cellPinOff;
        lv.netPinVals=netPinVals; lv.netPinOff=netPinOff;
        lv.maxDeg=maxDeg;
        return lv;
    }

    State initStateFromLevel(const Level &lv) {
        State s; s.NC=lv.NC; s.NN=lv.NN;
        s.areaA=lv.areaA; s.areaB=lv.areaB;
        s.cellPinVals=lv.cellPinVals; s.cellPinOff=lv.cellPinOff;
        s.netPinVals=lv.netPinVals; s.netPinOff=lv.netPinOff;
        s.dieArea=dieArea; s.capA=capA; s.capB=capB;
        s.maxDeg=lv.maxDeg; s.startTime=startTime;
        s.side.resize(lv.NC); s.countA.resize(lv.NN); s.countB.resize(lv.NN);
        return s;
    }

    // Build multilevel hierarchy: returns {levels, matches}
    // levels[0] = finest (original), levels.back() = coarsest
    // matches[i] = matching from levels[i] -> levels[i+1]
    pair<vector<Level>, vector<vector<int>>> buildHierarchy() {
        vector<Level> levels;
        vector<vector<int>> matches;
        Level current = buildLevel();
        levels.push_back(move(current));

        // Limit total levels to avoid excessive uncoarsening overhead
        int maxLevels = NC < 100000 ? 12 : (NC < 500000 ? 20 : 25);

        while (levels.back().NC > 1000 && (int)levels.size() <= maxLevels) {
            Level &lv = levels.back();
            vector<int> m = coarsenMatching(lv.NC, lv.cellPinVals.data(), lv.cellPinOff.data(),
                                            lv.netPinVals.data(), lv.netPinOff.data());
            // Check if we actually matched enough
            int matchedCount = 0;
            for (int v : m) if (v >= 0) matchedCount++;
            if (matchedCount < 10) break; // can't coarsen further effectively

            Level coarse = buildCoarse(lv, m);
            if (coarse.NC >= lv.NC - 5) break; // not enough reduction

            matches.push_back(move(m));
            levels.push_back(move(coarse));
        }
        return {move(levels), move(matches)};
    }

    // Uncoarsen + refine: given partition at level[i+1], refine at level[i]
    void uncoarsenRefine(const Level &fine, const Level &coarse, const vector<int> &match,
                         const vector<uint8_t> &coarseSide, vector<uint8_t> &fineSide,
                         double capA_, double capB_, double dieArea_, double deadlineSec,
                         int numLevels, int currentLevel) {
        int nC = fine.NC;
        // Build clusterOf
        vector<int> clusterOf(nC, -1);
        int cNc = 0;
        for (int i = 0; i < nC; i++) {
            if (clusterOf[i] >= 0) continue;
            if (match[i] >= 0) {
                clusterOf[i] = cNc;
                clusterOf[match[i]] = cNc;
                cNc++;
            } else {
                clusterOf[i] = cNc++;
            }
        }

        // Project partition from coarse to fine
        fineSide.assign(nC, 0);
        for (int i = 0; i < nC; i++) fineSide[i] = coarseSide[clusterOf[i]];

        // Create state at fine level and refine
        State st;
        st.NC = nC; st.NN = fine.NN;
        st.areaA = fine.areaA; st.areaB = fine.areaB;
        st.cellPinVals = fine.cellPinVals; st.cellPinOff = fine.cellPinOff;
        st.netPinVals = fine.netPinVals; st.netPinOff = fine.netPinOff;
        st.dieArea = dieArea_; st.capA = capA_; st.capB = capB_;
        st.maxDeg = fine.maxDeg; st.startTime = startTime;
        st.side = fineSide;
        st.countA.resize(fine.NN); st.countB.resize(fine.NN);
        st.recompute();

        // Adaptive refinement: skip refinement at the finest level entirely
        // (projection-only), heavy refinement at coarse levels where it's cheap
        double fineWeight = (double)nC / NC; // 1.0 for finest

        // For the finest level (or near-finest): NO refinement, just use projected partition
        if (fineWeight > 0.85) {
            // Skip all refinement - just use the projected partition from the coarser level
            // The quality comes from the coarse-level FM, not fine-level refinement
        }
        // For the next few fine levels: 1 FM pass only, no positive-gain sweep
        else if (fineWeight > 0.5) {
            st.recompute();
            int prevCut = st.computeCut();
            int steps = st.fmPass();
            if (steps > 1) { st.recompute(); prevCut = st.computeCut(); }
        }
        else {
            // Coarser levels: full refinement
            st.initialPositiveGainMoves();
            if (Clock::now() - st.startTime > chrono::duration<double>(deadlineSec)) { fineSide = st.side; return; }

            st.recompute();
            int prevCut = st.computeCut();

            int maxPasses = fineWeight > 0.15 ? 8 : 20;
            for (int p = 0; p < maxPasses; p++) {
                if (Clock::now() - st.startTime > chrono::duration<double>(deadlineSec)) break;
                int steps = st.fmPass();
                st.recompute();
                int newCut = st.computeCut();
                if (steps <= 1 || newCut >= prevCut) break;
                prevCut = newCut;
            }
        }

        fineSide = st.side;
    }

    void mkdirParent(const string &p) {
        size_t pos=p.find_last_of("/\\"); if(pos==string::npos)return;
        string d=p.substr(0,pos); if(d.empty())return;
        string b; for(size_t i=0;i<d.size();i++){b+=d[i];if(d[i]=='/'||d[i]=='\\')mkdir(b.c_str());}
        mkdir(d.c_str());
    }

    void solve(const string &in, const string &out) {
        startTime = Clock::now();
        parse(in);
        fprintf(stderr, "Parsed: %d cells, %d nets, maxDeg=%d\n", NC, NN, maxDeg);

        int maxTh = omp_get_max_threads();

        double timeBudget = DEADLINE_SEC;
        if (NC < 5000) timeBudget = 5.0;
        else if (NC < 50000) timeBudget = 15.0;
        else if (NC < 200000) timeBudget = 60.0;
        else timeBudget = DEADLINE_SEC;

        bool useMultiLevel = (NC > 50000);

        vector<uint8_t> bestSide(NC);
        int bestCut=INT_MAX;
        double bestUA=0, bestUB=0;
        bool bestLegal=false;

        if (useMultiLevel) {
            // ─── Multilevel path ─────────────────────────────────────
            auto [levels, matches] = buildHierarchy();
            int numLevels = (int)levels.size();
            int coarsestNC = levels.back().NC;
            double coarseningRatio = (double)coarsestNC / NC;
            fprintf(stderr, "Multilevel: %d levels (coarsest=%d cells, ratio=%.3f)\n",
                    numLevels, coarsestNC, coarseningRatio);

            // If coarsening is not effective, fall back to flat FM
            // With random-order matching, we should get much better coarsening
            bool effectiveCoarsening = (coarseningRatio < 0.05) && (coarsestNC < 15000) && (numLevels >= 8);

            if (!effectiveCoarsening) {
                fprintf(stderr, "Coarsening ineffective, using flat multi-start FM\n");
                // Fall through to flat path - reset useMultiLevel behavior
                goto flat_path;
            }

            // Calculate per-start budget based on coarsest level size
            double estPerStart = 0.000003 * coarsestNC;  // per-cell estimate at coarsest
            estPerStart += 0.000001 * NC;  // plus uncoarsen overhead at fine levels
            if (estPerStart < 2.0) estPerStart = 2.0;
            if (estPerStart > 150) estPerStart = 150;
            int numStarts = max(1, (int)(timeBudget / estPerStart));
            numStarts = min(numStarts, maxTh * 8);
            if (NC > 500000) numStarts = min(numStarts, 48);
            if (NC > 1000000) numStarts = min(numStarts, 32);
            maxTh = min(maxTh, min(numStarts, 32));

            fprintf(stderr, "Multi-start: %d starts (est=%.1fs/start, budget=%.1fs)\n",
                    numStarts, estPerStart, timeBudget);

            omp_set_num_threads(maxTh);

#pragma omp parallel
            {
                bool done = false;
                int tid = omp_get_thread_num();
#pragma omp for schedule(dynamic, 1)
                for (int s = 0; s < numStarts; s++) {
                    if (done) continue;
                    if (Clock::now() - startTime > chrono::duration<double>(timeBudget)) { done = true; continue; }

                    // Work on coarsest level
                    Level &coarsest = levels.back();
                    State stCoarse = initStateFromLevel(coarsest);

                    // Diverse initialization strategies
                    int strategy = s % 4;
                    if (strategy == 0) stCoarse.greedyInit((unsigned int)(s + 1));
                    else if (strategy == 1) {
                        stCoarse.side.assign(coarsest.NC, 0); stCoarse.usedA = stCoarse.usedB = 0;
                        vector<int> order(coarsest.NC); iota(order.begin(), order.end(), 0);
                        unsigned int sv = (unsigned int)(s + 1) * 2654435761u;
                        for (int i = coarsest.NC - 1; i > 0; i--) { sv = sv * 48271u; int j = sv % (i + 1); swap(order[i], order[j]); }
                        sort(order.begin(), order.end(), [&stCoarse](int a, int b) { return stCoarse.areaA[a] + stCoarse.areaB[a] > stCoarse.areaA[b] + stCoarse.areaB[b]; });
                        for (int idx : order) {
                            if (stCoarse.usedA + stCoarse.areaA[idx] <= stCoarse.capA * stCoarse.dieArea + EPS) { stCoarse.side[idx] = 0; stCoarse.usedA += stCoarse.areaA[idx]; }
                            else if (stCoarse.usedB + stCoarse.areaB[idx] <= stCoarse.capB * stCoarse.dieArea + EPS) { stCoarse.side[idx] = 1; stCoarse.usedB += stCoarse.areaB[idx]; }
                            else { stCoarse.side[idx] = 0; stCoarse.usedA += stCoarse.areaA[idx]; }
                        }
                        stCoarse.recompute();
                    } else if (strategy == 2) {
                        stCoarse.side.assign(coarsest.NC, 0); stCoarse.usedA = stCoarse.usedB = 0;
                        unsigned int sv = (unsigned int)(s + 1) * 2654435761u;
                        for (int i = 0; i < coarsest.NC; i++) {
                            sv = sv * 48271u; int r = sv % 2;
                            if (r == 0) { if (stCoarse.usedA + stCoarse.areaA[i] <= stCoarse.capA * stCoarse.dieArea + EPS) { stCoarse.side[i] = 0; stCoarse.usedA += stCoarse.areaA[i]; } else { stCoarse.side[i] = 1; stCoarse.usedB += stCoarse.areaB[i]; } }
                            else { if (stCoarse.usedB + stCoarse.areaB[i] <= stCoarse.capB * stCoarse.dieArea + EPS) { stCoarse.side[i] = 1; stCoarse.usedB += stCoarse.areaB[i]; } else { stCoarse.side[i] = 0; stCoarse.usedA += stCoarse.areaA[i]; } }
                        }
                        stCoarse.recompute();
                    } else {
                        stCoarse.side.assign(coarsest.NC, 0); stCoarse.usedA = stCoarse.usedB = 0;
                        vector<int> order(coarsest.NC); iota(order.begin(), order.end(), 0);
                        sort(order.begin(), order.end(), [&stCoarse](int a, int b) { return stCoarse.areaA[a] + stCoarse.areaB[a] > stCoarse.areaA[b] + stCoarse.areaB[b]; });
                        int toggle = 0;
                        for (int idx : order) {
                            if (toggle == 0) { if (stCoarse.usedA + stCoarse.areaA[idx] <= stCoarse.capA * stCoarse.dieArea + EPS) { stCoarse.side[idx] = 0; stCoarse.usedA += stCoarse.areaA[idx]; toggle = 1; } else { stCoarse.side[idx] = 1; stCoarse.usedB += stCoarse.areaB[idx]; } }
                            else { if (stCoarse.usedB + stCoarse.areaB[idx] <= stCoarse.capB * stCoarse.dieArea + EPS) { stCoarse.side[idx] = 1; stCoarse.usedB += stCoarse.areaB[idx]; toggle = 0; } else { stCoarse.side[idx] = 0; stCoarse.usedA += stCoarse.areaA[idx]; } }
                        }
                        stCoarse.recompute();
                    }

                    // FM optimization at coarsest level
                    if (Clock::now() - startTime <= chrono::duration<double>(timeBudget)) {
                        stCoarse.fmOptimize();
                    }

                    // Uncoarsen + refine down to finest level
                    vector<uint8_t> currentSide = stCoarse.side;
                    for (int li = numLevels - 2; li >= 0; li--) {
                        if (Clock::now() - startTime > chrono::duration<double>(timeBudget)) break;
                        vector<uint8_t> finerSide;
                        uncoarsenRefine(levels[li], levels[li + 1], matches[li],
                                        currentSide, finerSide, capA, capB, dieArea, timeBudget,
                                        numLevels, li);
                        currentSide = move(finerSide);
                    }

                    // Compute final cut at finest level
                    {
                        vector<int> fA(NN, 0), fB(NN, 0);
                        for (int i = 0; i < NC; i++)
                            for (int k = cellPinOff[i]; k < cellPinOff[i + 1]; k++) {
                                int n = cellPinVals[k];
                                if (currentSide[i] == 0) fA[n]++; else fB[n]++;
                            }
                        int cut = 0;
                        for (int n = 0; n < NN; n++) if (fA[n] > 0 && fB[n] > 0) cut++;

                        // Verify legality
                        double uA = 0, uB = 0;
                        for (int i = 0; i < NC; i++) { if (currentSide[i] == 0) uA += areaA[i]; else uB += areaB[i]; }
                        bool legal = (uA <= capA * dieArea + EPS) && (uB <= capB * dieArea + EPS);
                        if (!legal) continue;

#pragma omp critical
                        {
                            if (cut < bestCut || !bestLegal) {
                                bestCut = cut; bestSide = currentSide; bestUA = uA; bestUB = uB; bestLegal = true;
                                double el = chrono::duration<double>(Clock::now() - startTime).count();
                                fprintf(stderr, "  ML-Start %d(t%d): cut=%d (%.1fs)\n", s, tid, cut, el);
                            }
                        }
                    }
                }
            }
        } else {
            // ─── Flat multi-start path (small cases) ─────────────────
flat_path:
            // ─── Flat multi-start path ───────────────────────────────
            double phase1Budget = timeBudget;

            // Per-start time estimate: calibrate based on cell count
            // For large graphs, each FM start takes considerable time
            double estPerStart;
            if (NC < 5000) estPerStart = 0.002;
            else if (NC < 50000) estPerStart = 0.00003 * NC;
            else if (NC < 200000) estPerStart = 0.0001 * NC;    // ~20s for 200K
            else estPerStart = 0.00015 * NC;                     // ~110s for 740K
            if (estPerStart < 0.3) estPerStart = 0.3;
            if (estPerStart > 200) estPerStart = 200;

            int maxStarts = (int)(phase1Budget / estPerStart);
            if (maxStarts < 1) maxStarts = 1;
            if (maxStarts > 2000) maxStarts = 2000;
            int numStarts = min(maxStarts, maxTh * 8);

            // For very large cases, limit threads to reduce memory pressure
            if (NC > 500000) {
                maxTh = min(maxTh, 8);
                numStarts = min(numStarts, maxTh * 2);
            }

            fprintf(stderr, "Multi-start: %d starts (phase1=%.1fs, est=%.3fs/start)\n",
                    numStarts, phase1Budget, estPerStart);

            omp_set_num_threads(maxTh);

#pragma omp parallel
            {
                bool done = false;
                int tid = omp_get_thread_num();
#pragma omp for schedule(dynamic, 1)
                for (int s = 0; s < numStarts; s++) {
                    if (done) continue;
                    if (Clock::now() - startTime > chrono::duration<double>(phase1Budget)) { done = true; continue; }

                    State st = initState();

                    int strategy = s % 4;
                    if (strategy == 0) st.greedyInit((unsigned int)(s + 1));
                    else if (strategy == 1) {
                        st.side.assign(NC, 0); st.usedA = st.usedB = 0;
                        vector<int> order(NC); iota(order.begin(), order.end(), 0);
                        unsigned int sv = (unsigned int)(s + 1) * 2654435761u;
                        for (int i = NC - 1; i > 0; i--) { sv = sv * 48271u; int j = sv % (i + 1); swap(order[i], order[j]); }
                        sort(order.begin(), order.end(), [&st](int a, int b) { return st.areaA[a] + st.areaB[a] > st.areaA[b] + st.areaB[b]; });
                        for (int idx : order) {
                            if (st.usedA + st.areaA[idx] <= st.capA * st.dieArea + EPS) { st.side[idx] = 0; st.usedA += st.areaA[idx]; }
                            else if (st.usedB + st.areaB[idx] <= st.capB * st.dieArea + EPS) { st.side[idx] = 1; st.usedB += st.areaB[idx]; }
                            else { st.side[idx] = 0; st.usedA += st.areaA[idx]; }
                        }
                        st.recompute();
                    } else if (strategy == 2) {
                        st.side.assign(NC, 0); st.usedA = st.usedB = 0;
                        unsigned int sv = (unsigned int)(s + 1) * 2654435761u;
                        for (int i = 0; i < NC; i++) {
                            sv = sv * 48271u; int r = sv % 2;
                            if (r == 0) { if (st.usedA + st.areaA[i] <= st.capA * st.dieArea + EPS) { st.side[i] = 0; st.usedA += st.areaA[i]; } else { st.side[i] = 1; st.usedB += st.areaB[i]; } }
                            else { if (st.usedB + st.areaB[i] <= st.capB * st.dieArea + EPS) { st.side[i] = 1; st.usedB += st.areaB[i]; } else { st.side[i] = 0; st.usedA += st.areaA[i]; } }
                        }
                        st.recompute();
                    } else {
                        st.side.assign(NC, 0); st.usedA = st.usedB = 0;
                        vector<int> order(NC); iota(order.begin(), order.end(), 0);
                        sort(order.begin(), order.end(), [&st](int a, int b) { return st.areaA[a] + st.areaB[a] > st.areaA[b] + st.areaB[b]; });
                        int toggle = 0;
                        for (int idx : order) {
                            if (toggle == 0) { if (st.usedA + st.areaA[idx] <= st.capA * st.dieArea + EPS) { st.side[idx] = 0; st.usedA += st.areaA[idx]; toggle = 1; } else { st.side[idx] = 1; st.usedB += st.areaB[idx]; } }
                            else { if (st.usedB + st.areaB[idx] <= st.capB * st.dieArea + EPS) { st.side[idx] = 1; st.usedB += st.areaB[idx]; toggle = 0; } else { st.side[idx] = 0; st.usedA += st.areaA[idx]; } }
                        }
                        st.recompute();
                    }

                    if (Clock::now() - startTime <= chrono::duration<double>(phase1Budget)) {
                        st.fmOptimize();
                    }

                    st.recompute();
                    int cut = st.computeCut();
                    bool legal = (st.usedA <= capA * dieArea + EPS) && (st.usedB <= capB * dieArea + EPS);
                    if (!legal) continue;

#pragma omp critical
                    {
                        if (cut < bestCut || !bestLegal) {
                            bestCut = cut; bestSide = st.side; bestUA = st.usedA; bestUB = st.usedB; bestLegal = true;
                            double el = chrono::duration<double>(Clock::now() - startTime).count();
                            fprintf(stderr, "  Start %d(t%d): cut=%d (%.1fs)\n", s, tid, cut, el);
                        }
                    }
                }
            }
        }

        if (!bestLegal) {
            fprintf(stderr, "WARNING: No legal result, falling back to greedy\n");
            State st = initState(); st.greedyInit(0);
            bestSide = st.side; bestUA = st.usedA; bestUB = st.usedB; bestCut = st.computeCut();
        }

        // Final accurate cut computation
        {
            vector<int> fA(NN, 0), fB(NN, 0);
            for (int i = 0; i < NC; i++) for (int k = cellPinOff[i]; k < cellPinOff[i + 1]; k++) { int n = cellPinVals[k]; if (bestSide[i] == 0) fA[n]++; else fB[n]++; }
            bestCut = 0; for (int n = 0; n < NN; n++) if (fA[n] > 0 && fB[n] > 0) bestCut++;
        }

        mkdirParent(out);
        vector<int> dA, dB;
        for (int i = 0; i < NC; i++) if (bestSide[i] == 0) dA.push_back(i); else dB.push_back(i);

        FILE *fp = fopen(out.c_str(), "w");
        if (!fp) { fprintf(stderr, "Error: cannot open %s\n", out.c_str()); return; }
        fprintf(fp, "CutSize %d\n", bestCut);
        fprintf(fp, "DieA %d\n", (int)dA.size());
        for (int i : dA) fprintf(fp, "%s\n", cellName.at(i).c_str());
        fprintf(fp, "DieB %d\n", (int)dB.size());
        for (int i : dB) fprintf(fp, "%s\n", cellName.at(i).c_str());
        fclose(fp);

        double el = chrono::duration<double>(Clock::now() - startTime).count();
        fprintf(stderr, "Done: cut=%d, uA=%.6f/%.6f, uB=%.6f/%.6f, t=%.2fs\n",
                bestCut, bestUA / dieArea, capA, bestUB / dieArea, capB, el);
    }
};

int main(int argc, char**argv) {
    if(argc<3){fprintf(stderr,"Usage: %s <in> <out>\n",argv[0]);return 1;}
    Solver s; s.solve(argv[1],argv[2]);
    return 0;
}
