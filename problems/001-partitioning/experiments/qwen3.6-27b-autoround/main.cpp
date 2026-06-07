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

        // For large cases: stop when cumG<=0 (like reference). For small: run all feasible moves.
        while (maxPtr >= 0 && (NC > 100000 ? (step == 0 || cumG > 0) : true)) {
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
            auto [gv, c] = pq.top(); pq.pop();
            if (gv <= 0) break;
            int ag = gainOf(c);
            if (ag != gv) { pq.emplace(ag, c); continue; }
            if (ag <= 0 || !feasible(c)) continue;

            int from = side[c];
            doMove(c);

            for (int k = cellPinOff[c]; k < cellPinOff[c+1]; k++) {
                int n = cellPinVals[k];
                for (int j = netPinOff[n]; j < netPinOff[n+1]; j++) {
                    int nb = netPinVals[j];
                    if (nb != c) pq.emplace(gainOf(nb), nb);
                }
            }
        }
    }

    void fmOptimize() {
        initialPositiveGainMoves();
        if (deadlineHit()) return;

        recompute();
        int prevCut = computeCut();

        while (true) {
            int steps = fmPass();
            if (deadlineHit()) break;
            recompute();
            int newCut = computeCut();
            if (steps <= 1 || newCut >= prevCut) break;
            prevCut = newCut;
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

        bool isLarge = (NC > 200000);

        // Two-phase strategy for large cases:
        //   Phase 1: parallel exploration (use 30% of time budget)
        //   Phase 2: sequential deep refinement on best result (use 70%)
        double phase1Budget = isLarge ? timeBudget * 0.30 : timeBudget;
        int refineTime = isLarge ? (int)(timeBudget * 0.70) : 0;

        // Calibrated per-start time estimates
        double estPerStart = 0.0000004 * NC;
        if (estPerStart < 0.005) estPerStart = 0.005;
        if (estPerStart > 50) estPerStart = 50;
        int maxStarts = (int)(phase1Budget / estPerStart);
        if (maxStarts < 1) maxStarts = 1;
        if (maxStarts > 1000) maxStarts = 1000;
        int numStarts = min(maxStarts, maxTh * 16);

        // For very large cases, limit starts AND threads to avoid OOM
        if (isLarge) {
            numStarts = min(numStarts, 32);
            maxTh = min(maxTh, 8);
        }

        fprintf(stderr, "Multi-start: %d starts (phase1=%.1fs, est=%.3fs/start, refine=%ds)\n",
                numStarts, phase1Budget, estPerStart, (int)refineTime);

        omp_set_num_threads(maxTh);

        vector<uint8_t> bestSide(NC);
        int bestCut=INT_MAX;
        double bestUA=0, bestUB=0;
        bool bestLegal=false;

#pragma omp parallel
        {
            bool done=false;
            int tid=omp_get_thread_num();
#pragma omp for schedule(dynamic,1)
            for (int s=0;s<numStarts;s++) {
                if(done)continue;
                if(Clock::now()-startTime>chrono::duration<double>(phase1Budget)){done=true;continue;}

                State st=initState();

                int strategy = s % 4;
                if (strategy == 0) st.greedyInit((unsigned int)(s+1));
                else if (strategy == 1) {
                    st.side.assign(NC,0); st.usedA=st.usedB=0;
                    vector<int>order(NC); iota(order.begin(),order.end(),0);
                    unsigned int sv=(unsigned int)(s+1)*2654435761u;
                    for(int i=NC-1;i>0;i--){sv=sv*48271u;int j=sv%(i+1);swap(order[i],order[j]);}
                    sort(order.begin(),order.end(),[&st](int a,int b){return st.areaA[a]+st.areaB[a]>st.areaA[b]+st.areaB[b];});
                    for(int idx:order){
                        if(st.usedA+st.areaA[idx]<=st.capA*st.dieArea+EPS){st.side[idx]=0;st.usedA+=st.areaA[idx];}
                        else if(st.usedB+st.areaB[idx]<=st.capB*st.dieArea+EPS){st.side[idx]=1;st.usedB+=st.areaB[idx];}
                        else{st.side[idx]=0;st.usedA+=st.areaA[idx];}
                    }
                    st.recompute();
                } else if (strategy == 2) {
                    st.side.assign(NC,0); st.usedA=st.usedB=0;
                    unsigned int sv=(unsigned int)(s+1)*2654435761u;
                    for(int i=0;i<NC;i++){
                        sv=sv*48271u;
                        int r=sv%2;
                        if(r==0){
                            if(st.usedA+st.areaA[i]<=st.capA*st.dieArea+EPS){st.side[i]=0;st.usedA+=st.areaA[i];}
                            else{st.side[i]=1;st.usedB+=st.areaB[i];}
                        }else{
                            if(st.usedB+st.areaB[i]<=st.capB*st.dieArea+EPS){st.side[i]=1;st.usedB+=st.areaB[i];}
                            else{st.side[i]=0;st.usedA+=st.areaA[i];}
                        }
                    }
                    st.recompute();
                } else {
                    st.side.assign(NC,0); st.usedA=st.usedB=0;
                    vector<int>order(NC); iota(order.begin(),order.end(),0);
                    sort(order.begin(),order.end(),[&st](int a,int b){return st.areaA[a]+st.areaB[a]>st.areaA[b]+st.areaB[b];});
                    int toggle=0;
                    for(int idx:order){
                        if(toggle==0){
                            if(st.usedA+st.areaA[idx]<=st.capA*st.dieArea+EPS){st.side[idx]=0;st.usedA+=st.areaA[idx];toggle=1;}
                            else{st.side[idx]=1;st.usedB+=st.areaB[idx];}
                        }else{
                            if(st.usedB+st.areaB[idx]<=st.capB*st.dieArea+EPS){st.side[idx]=1;st.usedB+=st.areaB[idx];toggle=0;}
                            else{st.side[idx]=0;st.usedA+=st.areaA[idx];}
                        }
                    }
                    st.recompute();
                }

                if(Clock::now()-startTime<=chrono::duration<double>(phase1Budget)) {
                    if (isLarge) {
                        st.initialPositiveGainMoves();
                    } else {
                        st.fmOptimize();
                    }
                }

                st.recompute();
                int cut=st.computeCut();
                bool legal=(st.usedA<=capA*dieArea+EPS)&&(st.usedB<=capB*dieArea+EPS);
                if(!legal)continue;

#pragma omp critical
                {
                    if(cut<bestCut||!bestLegal){
                        bestCut=cut; bestSide=st.side; bestUA=st.usedA; bestUB=st.usedB; bestLegal=true;
                        double el=chrono::duration<double>(Clock::now()-startTime).count();
                        fprintf(stderr,"  Start %d(t%d): cut=%d (%.1fs)\n",s,tid,cut,el);
                    }
                }
            }
        }

        if(!bestLegal){
            fprintf(stderr,"WARNING: No legal result\n");
            State st=initState(); st.greedyInit(0);
            bestSide=st.side; bestUA=st.usedA; bestUB=st.usedB; bestCut=st.computeCut();
        }

        // Final accurate cut computation
        {
            vector<int>fA(NN,0),fB(NN,0);
            for(int i=0;i<NC;i++)for(int k=cellPinOff[i];k<cellPinOff[i+1];k++){int n=cellPinVals[k];if(bestSide[i]==0)fA[n]++;else fB[n]++;}
            bestCut=0;for(int n=0;n<NN;n++)if(fA[n]>0&&fB[n]>0)bestCut++;
        }

        mkdirParent(out);
        vector<int>dA,dB;
        for(int i=0;i<NC;i++)if(bestSide[i]==0)dA.push_back(i);else dB.push_back(i);

        FILE*fp=fopen(out.c_str(),"w");
        if(!fp){fprintf(stderr,"Error: cannot open %s\n",out.c_str());return;}
        fprintf(fp,"CutSize %d\n",bestCut);
        fprintf(fp,"DieA %d\n",(int)dA.size());
        for(int i:dA)fprintf(fp,"%s\n",cellName.at(i).c_str());
        fprintf(fp,"DieB %d\n",(int)dB.size());
        for(int i:dB)fprintf(fp,"%s\n",cellName.at(i).c_str());
        fclose(fp);

        double el=chrono::duration<double>(Clock::now()-startTime).count();
        fprintf(stderr,"Done: cut=%d, uA=%.6f/%.6f, uB=%.6f/%.6f, t=%.2fs\n",
                bestCut,bestUA/dieArea,capA,bestUB/dieArea,capB,el);
    }
};

int main(int argc, char**argv) {
    if(argc<3){fprintf(stderr,"Usage: %s <in> <out>\n",argv[0]);return 1;}
    Solver s; s.solve(argv[1],argv[2]);
    return 0;
}
