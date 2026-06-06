// Fixed-outline Floorplanning Optimizer — claude-opus-4-8
// 演算法：線長導向建構式擺放 + 無網格 HPWL-only 模擬退火（O(N^2) 重疊檢查）。
//   - cost 僅含加權 HPWL（固定輪廓下面積與目標無關，丟棄面積項）。
//   - 模組以絕對座標矩形表示；N(soft)<=28，每步重疊檢查 O(N^2) 近免費，可跑大量迭代。
//   - 合法性常數與 HPWL 定義對齊官方 verifier / scorer：
//       outline: 0<=x,0<=y,x+w<=W,y+h<=H；overlap: 矩形交集>0（觸邊不算）；
//       面積: w*h>=area（下限）；長寬比: h/w in [0.5,2]；中心: (x+w/2,y+h/2) 整數除法。
// 編譯：g++ -std=c++20 -O3 -o hw3 main.cpp
// 執行：hw3 <input.txt> <output.floorplan>   （可選 argv[3]=時間上限秒數，預設 580）
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <random>
#include <ctime>
#include <climits>

using namespace std;

struct Module {
    string name;
    long area;          // soft: 最小面積；fixed: 實際面積（未用）
    int x, y, w, h;
    bool fixed;
    vector<pair<int,int>> shapes; // soft 的合法 (w,h) 候選
};

struct Net { int a, b, weight; }; // a,b 為 module index

static int W, H;                       // chip outline
static vector<Module> mods;            // 全部模組（soft + fixed）
static vector<int> softIdx;            // soft 模組在 mods 的 index
static vector<Net> nets;
static mt19937 rng(20260605u);         // 固定 seed → 可重現

// ---------- 工具 ----------
static inline int ri(int n){ return (int)(rng() % (unsigned)n); }      // [0,n)
static inline double rd(){ return (double)rng() / (double)rng.max(); } // [0,1]

// 產生 soft module 的合法形狀候選：w*h>=area 且 h/w in [0.5,2]
static vector<pair<int,int>> genShapes(long area){
    vector<pair<int,int>> out;
    int wmin = max(1, (int)floor(sqrt((double)area / 2.0)));
    int wmax = max(wmin, (int)ceil(sqrt((double)area * 2.0)));
    for(int w = wmin; w <= wmax; ++w){
        int h = (int)ceil((double)area / (double)w);
        while((long)w * h < area) ++h;
        double r = (double)h / (double)w;
        if(r < 0.5 - 1e-9 || r > 2.0 + 1e-9) continue;
        out.push_back(make_pair(w, h));
    }
    // 保底近正方形
    int ws = max(1, (int)round(sqrt((double)area)));
    int hs = (int)ceil((double)area / (double)ws);
    while((long)ws * hs < area) ++hs;
    if((double)hs/ws >= 0.5-1e-9 && (double)hs/ws <= 2.0+1e-9)
        out.push_back(make_pair(ws, hs));
    if(out.empty()) out.push_back(make_pair(ws, hs)); // 最終退路
    // 去重
    sort(out.begin(), out.end());
    out.erase(unique(out.begin(), out.end()), out.end());
    return out;
}

static inline bool inOutline(int x, int y, int w, int h){
    return x >= 0 && y >= 0 && x + w <= W && y + h <= H;
}
static inline bool rectOverlap(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){
    return ax < bx+bw && bx < ax+aw && ay < by+bh && by < ay+ah;
}
// 模組 idx 以 (x,y,w,h) 擺放是否合法（在輪廓內且與其他所有模組不重疊）
static bool legalAt(int idx,int x,int y,int w,int h){
    if(!inOutline(x,y,w,h)) return false;
    for(size_t j=0;j<mods.size();++j){
        if((int)j==idx) continue;
        if(rectOverlap(x,y,w,h, mods[j].x,mods[j].y,mods[j].w,mods[j].h)) return false;
    }
    return true;
}

static inline long centerX(const Module&m){ return m.x + m.w/2; }
static inline long centerY(const Module&m){ return m.y + m.h/2; }

static long long hpwl(){
    long long wl = 0;
    for(const Net&n : nets){
        const Module&A = mods[n.a]; const Module&B = mods[n.b];
        wl += (long long)n.weight * (llabs(centerX(A)-centerX(B)) + llabs(centerY(A)-centerY(B)));
    }
    return wl;
}

// 增量計算用：每模組所屬 net 清單、單一 net 線長、受影響 net 去重收集
static vector<vector<int>> adj;
static inline long long netCost(int ni){
    const Net&n=nets[ni]; const Module&A=mods[n.a]; const Module&B=mods[n.b];
    return (long long)n.weight*(llabs(centerX(A)-centerX(B))+llabs(centerY(A)-centerY(B)));
}
static vector<int> g_aff;              // 本次移動受影響的 net（去重）
static vector<int> netStamp; static int g_stamp=0;
static void collectAff(const int*idxs,int k){
    ++g_stamp; g_aff.clear();
    for(int i=0;i<k;++i){ for(int ni: adj[idxs[i]]) if(netStamp[ni]!=g_stamp){ netStamp[ni]=g_stamp; g_aff.push_back(ni);} }
}
static inline long long affCost(){ long long s=0; for(int ni:g_aff) s+=netCost(ni); return s; }

// 角點候選擺放：在其他模組邊界形成的候選點中，找一個合法位置。
// mode=0：bottom-left（最小化 (y,x)）；mode=1：最靠近目標中心 (tcx,tcy)。
static bool findPos(int idx,int w,int h,long tcx,long tcy,int mode,int&bx,int&by){
    vector<int> xs, ys;
    xs.push_back(0); ys.push_back(0);
    for(size_t j=0;j<mods.size();++j){
        if((int)j==idx) continue;
        xs.push_back(mods[j].x); xs.push_back(mods[j].x+mods[j].w);
        ys.push_back(mods[j].y); ys.push_back(mods[j].y+mods[j].h);
    }
    if(mode==1){ // 也試把中心對準目標
        xs.push_back((int)(tcx - w/2));
        ys.push_back((int)(tcy - h/2));
    }
    sort(xs.begin(),xs.end()); xs.erase(unique(xs.begin(),xs.end()),xs.end());
    sort(ys.begin(),ys.end()); ys.erase(unique(ys.begin(),ys.end()),ys.end());
    bool found=false; long long best=LLONG_MAX;
    for(int x : xs){
        if(x < 0 || x + w > W) continue;
        for(int y : ys){
            if(y < 0 || y + h > H) continue;
            if(!legalAt(idx,x,y,w,h)) continue;
            long long key;
            if(mode==0) key = (long long)y * (W+1) + x;
            else { long long ccx=x+w/2, ccy=y+h/2; key = llabs(ccx-tcx)+llabs(ccy-tcy); }
            if(key < best){ best=key; bx=x; by=y; found=true; }
        }
    }
    return found;
}

// ---------- 解析 ----------
static vector<string> tok(const string&s){
    vector<string> v; istringstream ss(s); string w; while(ss>>w) v.push_back(w); return v;
}
static void parse(const string&path){
    ifstream f(path);
    string line; vector<string> v;
    unordered_map<string,int> name2idx;
    auto nextTokens=[&](void)->vector<string>{
        while(getline(f,line)){ v=tok(line); if(!v.empty()) return v; }
        return vector<string>();
    };
    v=nextTokens(); W=stoi(v[1]); H=stoi(v[2]);
    v=nextTokens(); int ns=stoi(v[1]);
    for(int i=0;i<ns;++i){
        v=nextTokens();
        Module m; m.name=v[1]; m.area=stol(v[2]); m.fixed=false;
        m.x=m.y=m.w=m.h=0; m.shapes=genShapes(m.area);
        name2idx[m.name]=(int)mods.size(); mods.push_back(m);
    }
    v=nextTokens(); int nf=stoi(v[1]);
    for(int i=0;i<nf;++i){
        v=nextTokens();
        Module m; m.name=v[1]; m.fixed=true;
        m.x=stoi(v[2]); m.y=stoi(v[3]); m.w=stoi(v[4]); m.h=stoi(v[5]);
        m.area=(long)m.w*m.h;
        name2idx[m.name]=(int)mods.size(); mods.push_back(m);
    }
    v=nextTokens(); int nn=stoi(v[1]);
    for(int i=0;i<nn;++i){
        v=nextTokens();
        Net n; n.a=name2idx[v[1]]; n.b=name2idx[v[2]]; n.weight=stoi(v[3]);
        nets.push_back(n);
    }
    for(size_t j=0;j<mods.size();++j) if(!mods[j].fixed) softIdx.push_back((int)j);
}

// ---------- 建構式初始合法擺放 ----------
// 將 soft 暫移出畫面(以巨大座標標示未擺)，依面積遞減逐一以 bottom-left 角點落子。
static bool constructOnce(const vector<int>&order){
    // 先把所有 soft 標為「未擺」：放到不可能重疊的遠方並設 0 尺寸
    for(int idx : softIdx){ mods[idx].w=0; mods[idx].h=0; mods[idx].x=INT_MAX/2; mods[idx].y=INT_MAX/2; }
    for(int idx : order){
        bool placed=false;
        // 嘗試各形狀（近正方優先），找 bottom-left 合法位
        vector<pair<int,int>>&shp = mods[idx].shapes;
        // 依接近正方排序形狀，較易落子
        vector<int> sidx(shp.size()); for(size_t k=0;k<shp.size();++k) sidx[k]=(int)k;
        sort(sidx.begin(),sidx.end(),[&](int a,int b){
            double ra=fabs(log((double)shp[a].second/shp[a].first));
            double rb=fabs(log((double)shp[b].second/shp[b].first));
            return ra<rb;
        });
        for(int k : sidx){
            int w=shp[k].first, h=shp[k].second, bx, by;
            // 暫時把自己尺寸設為 0 不影響 findPos（findPos 跳過 idx）
            if(findPos(idx,w,h,0,0,0,bx,by)){
                mods[idx].x=bx; mods[idx].y=by; mods[idx].w=w; mods[idx].h=h;
                placed=true; break;
            }
        }
        if(!placed) return false;
    }
    return true;
}
static bool construct(){
    vector<int> order = softIdx;
    sort(order.begin(),order.end(),[&](int a,int b){ return mods[a].area>mods[b].area; });
    if(constructOnce(order)) return true;
    // 重試：擾動順序
    for(int attempt=0; attempt<2000; ++attempt){
        shuffle(order.begin(),order.end(),rng);
        if(constructOnce(order)) return true;
    }
    return false;
}

// ---------- 模擬退火 ----------
struct Snap { vector<int> x,y,w,h; };
static Snap snapshot(){
    Snap s; for(int idx:softIdx){ s.x.push_back(mods[idx].x); s.y.push_back(mods[idx].y);
        s.w.push_back(mods[idx].w); s.h.push_back(mods[idx].h);} return s;
}
static void restoreSnap(const Snap&s){
    for(size_t k=0;k<softIdx.size();++k){ int idx=softIdx[k];
        mods[idx].x=s.x[k]; mods[idx].y=s.y[k]; mods[idx].w=s.w[k]; mods[idx].h=s.h[k]; }
}

// 計算 soft module idx 的加權連線目標中心
static bool netTarget(int idx,long&tcx,long&tcy){
    double sx=0,sy=0,sw=0;
    for(int ni : adj[idx]){
        const Net&n=nets[ni];
        int other = (n.a==idx)? n.b : n.a;
        sx += (double)n.weight * centerX(mods[other]);
        sy += (double)n.weight * centerY(mods[other]);
        sw += n.weight;
    }
    if(sw<=0) return false;
    tcx=(long)llround(sx/sw); tcy=(long)llround(sy/sw); return true;
}

// 一次擾動：成功時狀態已變更、g_delta 為 HPWL 變化量、g_saved 記原狀態供還原；
//           失敗時自行還原並回傳 false（狀態不變）。
struct Saved{ int idx,x,y,w,h; };
static vector<Saved> g_saved;
static long long g_delta;
static inline void saveMod(int idx){ Saved s={idx,mods[idx].x,mods[idx].y,mods[idx].w,mods[idx].h}; g_saved.push_back(s); }
static inline void revertMove(){ for(size_t i=g_saved.size(); i-->0;){ const Saved&s=g_saved[i];
        mods[s.idx].x=s.x; mods[s.idx].y=s.y; mods[s.idx].w=s.w; mods[s.idx].h=s.h; } }

static bool perturb(){
    g_saved.clear();
    int op = ri(100);
    int idx = softIdx[ri((int)softIdx.size())];
    Module&m = mods[idx];

    if(op < 40){
        // 線長導向 relocate（可同時換形）：移到加權連線質心附近的合法角點
        long tcx,tcy;
        if(!netTarget(idx,tcx,tcy)){ tcx=centerX(m); tcy=centerY(m); }
        int sk = ri((int)m.shapes.size());
        int w=m.shapes[sk].first, h=m.shapes[sk].second, bx,by;
        int ids[1]={idx}; collectAff(ids,1); long long oldc=affCost();
        saveMod(idx);
        if(findPos(idx,w,h,tcx,tcy,1,bx,by)){            // findPos 跳過 idx，自身不擋自己
            m.x=bx; m.y=by; m.w=w; m.h=h;
            g_delta = affCost()-oldc; return true;
        }
        revertMove(); g_saved.clear(); return false;
    } else if(op < 60){
        // 換形（優先原位；不行找鄰近合法角點）
        int sk = ri((int)m.shapes.size());
        int w=m.shapes[sk].first, h=m.shapes[sk].second;
        if(w==m.w && h==m.h) return false;
        int ids[1]={idx}; collectAff(ids,1); long long oldc=affCost();
        saveMod(idx);
        if(legalAt(idx,m.x,m.y,w,h)){ m.w=w; m.h=h; g_delta=affCost()-oldc; return true; }
        int bx,by;
        if(findPos(idx,w,h,centerX(m),centerY(m),1,bx,by)){ m.x=bx;m.y=by;m.w=w;m.h=h; g_delta=affCost()-oldc; return true; }
        revertMove(); g_saved.clear(); return false;
    } else if(op < 80){
        // 局部平移：小幅隨機位移（廉價 O(N)）
        int range = max(50, max(W,H)/20);
        int nx = m.x + (ri(2*range+1)-range);
        int ny = m.y + (ri(2*range+1)-range);
        int ids[1]={idx}; collectAff(ids,1); long long oldc=affCost();
        saveMod(idx);
        if(legalAt(idx,nx,ny,m.w,m.h)){ m.x=nx; m.y=ny; g_delta=affCost()-oldc; return true; }
        revertMove(); g_saved.clear(); return false;
    } else {
        // 交換兩 soft 模組位置（各自保留形狀，置於對方中心對齊的合法角點）
        int idx2 = softIdx[ri((int)softIdx.size())];
        if(idx2==idx) return false;
        Module&m2 = mods[idx2];
        long c1x=centerX(m), c1y=centerY(m), c2x=centerX(m2), c2y=centerY(m2);
        int ids[2]={idx,idx2}; collectAff(ids,2); long long oldc=affCost();
        saveMod(idx); saveMod(idx2);
        int aw=m.w,ah=m.h,b2w=m2.w,b2h=m2.h;
        m.w=0;m.h=0; m2.w=0;m2.h=0;               // 兩者皆讓出空間，避免互擋
        int p1x,p1y,p2x,p2y;
        if(findPos(idx,aw,ah,c2x,c2y,1,p1x,p1y)){
            m.x=p1x; m.y=p1y; m.w=aw; m.h=ah;
            if(findPos(idx2,b2w,b2h,c1x,c1y,1,p2x,p2y)){
                m2.x=p2x; m2.y=p2y; m2.w=b2w; m2.h=b2h;
                g_delta = affCost()-oldc; return true;
            }
        }
        revertMove(); g_saved.clear(); return false;
    }
}

// 預設時間預算（秒）。可由 argv[3] 覆寫。設 120s：在 600s 硬上限內、兼顧 public2/3/4 的 ≤120s 目標，
// 且 public1 在此預算內即已超越 Min（更長時間對各測資邊際效益遞減）。
static double TIME_LIMIT = 120.0;
static double startClock;
static inline double elapsed(){ return (double)(clock()-startClock)/CLOCKS_PER_SEC; }

static void anneal(){
    // 以隨機擾動採樣估計初溫尺度（用增量 g_delta）
    double sumAbs=0; int cnt=0;
    for(int t=0;t<2000 && cnt<500;++t){
        if(perturb()){ sumAbs+=fabs((double)g_delta); cnt++; revertMove(); }
    }
    double avgDelta = (cnt>0)? (sumAbs/cnt) : 1000.0;
    if(avgDelta<1) avgDelta=1;

    long long cur = hpwl();
    long long best = cur;
    Snap bestSnap = snapshot();

    double Tmin  = avgDelta / 20000.0 + 1e-6;
    double decay = 0.99;
    long long L = max((long long)500, (long long)softIdx.size() * 200); // 每溫度層擾動數

    // 外層 reheat-intensify：由全域最佳反覆再加熱退火，耗盡時間預算（保留全域最佳）。
    int round = 0;
    while(elapsed() < TIME_LIMIT){
        restoreSnap(bestSnap); cur = best;
        double T = avgDelta * (round==0 ? 4.0 : 1.5);
        ++round;
        while(T > Tmin && elapsed() < TIME_LIMIT){
            for(long long it=0; it<L; ++it){
                if((it & 1023)==0 && elapsed() >= TIME_LIMIT) break;
                if(!perturb()) continue;             // 失敗移動：狀態未變
                long long d = g_delta;
                if(d <= 0 || rd() < exp(-(double)d / T)){
                    cur += d;
                    if(cur < best){ best=cur; bestSnap=snapshot(); }
                } else {
                    revertMove();                    // 拒絕：還原本次移動
                }
            }
            T *= decay;
        }
    }
    restoreSnap(bestSnap);
}

// ---------- 輸出 ----------
static void writeOut(const string&path){
    ofstream f(path);
    f << "Wirelength " << hpwl() << "\n\n";
    f << "NumSoftModules " << softIdx.size() << "\n";
    for(int idx : softIdx){
        const Module&m=mods[idx];
        f << m.name << " " << m.x << " " << m.y << " " << m.w << " " << m.h << "\n";
    }
}

int main(int argc,char**argv){
    if(argc < 3){ cerr<<"usage: hw3 <input> <output> [time_limit_s]\n"; return 1; }
    if(argc >= 4) TIME_LIMIT = atof(argv[3]);
    startClock = clock();

    parse(argv[1]);

    // 建立增量計算結構：每模組所屬 net、net 受影響戳記
    adj.assign(mods.size(), vector<int>());
    for(size_t ni=0; ni<nets.size(); ++ni){ adj[nets[ni].a].push_back((int)ni); adj[nets[ni].b].push_back((int)ni); }
    netStamp.assign(nets.size(), 0);

    if(softIdx.empty()){ writeOut(argv[2]); return 0; } // 無 soft module

    if(!construct()){
        // 萬一建構失敗，退而求其次：大規模隨機重試 bottom-left（仍應成功）
        bool ok=false;
        vector<int> order=softIdx;
        for(int a=0;a<20000 && !ok;++a){ shuffle(order.begin(),order.end(),rng); ok=constructOnce(order); }
        if(!ok){ cerr<<"[warn] construction failed; emitting best-effort.\n"; }
    }

    anneal();
    writeOut(argv[2]);
    return 0;
}
