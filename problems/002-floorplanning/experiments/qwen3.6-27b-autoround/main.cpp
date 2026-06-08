/**
 * Fixed-Outline Floorplanning Solver — Optimized v5
 *
 * Build: g++ -std=c++20 -O3 -fopenmp -pthread -o hw3 main.cpp
 *
 * Optimizations: Perturb&Recover, swap+reshape, adaptive SA, multi-round
 * intensification, neighbor-centroid init, recency-based module selection.
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <climits>
#include <filesystem>
#include <omp.h>
#include <random>
#include <thread>
#include <mutex>

using namespace std;
namespace fs = std::filesystem;

// ============================================================
// Data structures
// ============================================================

struct Module {
    string name;
    int x, y, w, h;
    int minArea;
    bool isFixed;
    vector<pair<int,int>> shapes;
    vector<int> nets;
    inline int cx() const { return x + w / 2; }
    inline int cy() const { return y + h / 2; }
};

struct Net { int a, b, weight; };

int chipW = 0, chipH = 0;
vector<Module> gModules;
vector<Net> nets;
unordered_map<string, int> nameToId;
vector<string> idToName;
int nSoft = 0, nTotal = 0;
double deadlineTime = 0;

thread_local vector<Module>* modulesTLS = &gModules;
inline vector<Module>& modRef() { return *modulesTLS; }
#define modules modRef()

// ============================================================
// Tokenizer & Parser
// ============================================================

vector<string> tokenize(const string& line) {
    vector<string> t;
    istringstream ss(line);
    for (string w; ss >> w;) t.push_back(move(w));
    return t;
}

void parseInput(const string& path) {
    ifstream f(path);
    vector<vector<string>> lines;
    for (string line; getline(f, line);) {
        auto t = tokenize(line);
        if (!t.empty()) lines.push_back(move(t));
    }
    f.close();

    int i = 0;
    chipW = stoi(lines[i][1]); chipH = stoi(lines[i][2]); i++;
    nSoft = stoi(lines[i][1]); i++;
    for (int s = 0; s < nSoft; s++) {
        int id = (int)modules.size();
        Module m;
        m.name = lines[i][1]; m.minArea = stoi(lines[i][2]);
        m.isFixed = false; m.x = m.y = m.w = m.h = 0;
        modules.push_back(move(m));
        nameToId[lines[i][1]] = id;
        idToName.push_back(lines[i][1]);
        i++;
    }
    int nf = stoi(lines[i][1]); i++;
    for (int s = 0; s < nf; s++) {
        int id = (int)modules.size();
        Module m;
        m.name = lines[i][1];
        m.x = stoi(lines[i][2]); m.y = stoi(lines[i][3]);
        m.w = stoi(lines[i][4]); m.h = stoi(lines[i][5]);
        m.minArea = m.w * m.h; m.isFixed = true;
        modules.push_back(move(m));
        nameToId[lines[i][1]] = id;
        idToName.push_back(lines[i][1]);
        i++;
    }
    nTotal = (int)modules.size();
    int nn = stoi(lines[i][1]); i++;
    for (int s = 0; s < nn; s++) {
        int aid = nameToId[lines[i][1]];
        int bid = nameToId[lines[i][2]];
        int wt  = stoi(lines[i][3]);
        int nid = (int)nets.size();
        nets.push_back({aid, bid, wt});
        modules[aid].nets.push_back(nid);
        modules[bid].nets.push_back(nid);
        i++;
    }
}

// ============================================================
// Shape generation
// ============================================================

vector<pair<int,int>> genShapes(int area) {
    vector<pair<int,int>> S;
    int minW = max(1, (int)ceil(sqrt(area * 0.5)));
    int maxW = min((int)floor(sqrt(area * 2.0)), chipW);
    for (int w = minW; w <= maxW && w <= chipW; w++) {
        int h = (area + w - 1) / w;
        if (h > chipH) continue;
        if (w * h >= area) {
            double ratio = (double)h / w;
            if (ratio >= 0.5 - 1e-9 && ratio <= 2.0 + 1e-9)
                S.push_back({w, h});
        }
        if (h > 0 && (double)h / w > 2.0 + 1e-9) {
            int w2 = max(w, (int)ceil(h * 0.5));
            if (w2 <= chipW && h <= chipH && w2 * h >= area) {
                double r2 = (double)h / w2;
                if (r2 >= 0.5 - 1e-9 && r2 <= 2.0 + 1e-9)
                    S.push_back({w2, h});
            }
        }
    }
    {
        int sq = (int)ceil(sqrt((double)area));
        if (sq <= chipW && sq <= chipH && sq * sq >= area)
            S.push_back({sq, sq});
    }
    sort(S.begin(), S.end());
    S.erase(unique(S.begin(), S.end()), S.end());
    return S;
}

void initShapes() {
    for (auto& m : modules) {
        if (!m.isFixed) {
            m.shapes = genShapes(m.minArea);
            if (m.shapes.empty()) {
                int sq = (int)ceil(sqrt((double)m.minArea));
                m.shapes.push_back({sq, sq});
            }
        }
    }
}

// ============================================================
// Geometry primitives
// ============================================================

inline bool doOverlap(const Module& a, const Module& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

inline bool inOutline(const Module& m) {
    return m.x >= 0 && m.y >= 0 && m.x + m.w <= chipW && m.y + m.h <= chipH;
}

bool legalPlacement(int i) {
    auto& m = modules[i];
    if (!inOutline(m)) return false;
    if (m.w * m.h < m.minArea) return false;
    if (m.w > 0) {
        double ratio = (double)m.h / m.w;
        if (ratio < 0.5 - 1e-9 || ratio > 2.0 + 1e-9) return false;
    }
    for (int j = 0; j < nTotal; j++) {
        if (j != i && doOverlap(m, modules[j])) return false;
    }
    return true;
}

// ============================================================
// Wirelength
// ============================================================

long long computeWL() {
    long long wl = 0;
    for (auto& n : nets) {
        int ax = modules[n.a].cx(), ay = modules[n.a].cy();
        int bx = modules[n.b].cx(), by = modules[n.b].cy();
        wl += (long long)n.weight * (abs(ax - bx) + abs(ay - by));
    }
    return wl;
}

long long incrementalWL(int mi) {
    long long d = 0;
    for (int ni : modules[mi].nets) {
        const Net& n = nets[ni];
        int other = (n.a == mi) ? n.b : n.a;
        int x1 = modules[mi].cx(), y1 = modules[mi].cy();
        int x2 = modules[other].cx(), y2 = modules[other].cy();
        d += (long long)n.weight * (abs(x1 - x2) + abs(y1 - y2));
    }
    return d;
}

// Compute delta WL for modules in a given set (for multi-module moves)
long long incrementalWLSet(const vector<int>& mis) {
    long long d = 0;
    vector<bool> touched(nTotal, false);
    for (int mi : mis) touched[mi] = true;
    for (int mi : mis) {
        for (int ni : modules[mi].nets) {
            const Net& n = nets[ni];
            if (touched[n.a] && touched[n.b]) { // only count once
                int x1 = modules[n.a].cx(), y1 = modules[n.a].cy();
                int x2 = modules[n.b].cx(), y2 = modules[n.b].cy();
                d += (long long)n.weight * (abs(x1 - x2) + abs(y1 - y2));
            }
        }
    }
    // Also count nets connecting a touched module to an untouched one
    for (int mi : mis) {
        for (int ni : modules[mi].nets) {
            const Net& n = nets[ni];
            int other = (n.a == mi) ? n.b : n.a;
            if (!touched[other]) {
                int x1 = modules[mi].cx(), y1 = modules[mi].cy();
                int x2 = modules[other].cx(), y2 = modules[other].cy();
                d += (long long)n.weight * (abs(x1 - x2) + abs(y1 - y2));
            }
        }
    }
    return d;
}

// ============================================================
// Compaction
// ============================================================

void compactDir(int i, bool down, bool left) {
    auto& mi = modules[i];
    if (down) {
        vector<int> events;
        events.push_back(mi.x);
        events.push_back(min(mi.x + mi.w, chipW));
        for (int j = 0; j < nTotal; j++) {
            if (j == i) continue;
            auto& r = modules[j];
            if (r.x + r.w > mi.x && r.x < mi.x + mi.w) {
                if (r.x > mi.x) events.push_back(r.x);
                if (r.x + r.w < mi.x + mi.w) events.push_back(r.x + r.w);
            }
        }
        sort(events.begin(), events.end());
        int minY = 0;
        for (size_t ei = 0; ei + 1 < events.size(); ei++) {
            int x = (events[ei] + events[ei + 1]) / 2;
            if (x < mi.x || x >= mi.x + mi.w) continue;
            if (x < 0 || x >= chipW) { minY = chipH; break; }
            int b = 0;
            for (int j = 0; j < nTotal; j++) {
                if (j == i) continue;
                auto& r = modules[j];
                if (r.x <= x && x < r.x + r.w) b = max(b, r.y + r.h);
            }
            minY = max(minY, b);
        }
        mi.y = minY;
    }
    if (left) {
        vector<int> events;
        events.push_back(mi.y);
        events.push_back(min(mi.y + mi.h, chipH));
        for (int j = 0; j < nTotal; j++) {
            if (j == i) continue;
            auto& r = modules[j];
            if (r.y + r.h > mi.y && r.y < mi.y + mi.h) {
                if (r.y > mi.y) events.push_back(r.y);
                if (r.y + r.h < mi.y + mi.h) events.push_back(r.y + r.h);
            }
        }
        sort(events.begin(), events.end());
        int minX = 0;
        for (size_t ei = 0; ei + 1 < events.size(); ei++) {
            int y = (events[ei] + events[ei + 1]) / 2;
            if (y < mi.y || y >= mi.y + mi.h) continue;
            if (y < 0 || y >= chipH) { minX = chipW; break; }
            int b = 0;
            for (int j = 0; j < nTotal; j++) {
                if (j == i) continue;
                auto& r = modules[j];
                if (r.y <= y && y < r.y + r.h) b = max(b, r.x + r.w);
            }
            minX = max(minX, b);
        }
        mi.x = minX;
    }
}

bool tryMoveToward(int i, int tx, int ty, long long oldIncWL, long long curWL, double T, mt19937& rng) {
    uniform_real_distribution<double> distR(0.0, 1.0);
    int savX = modules[i].x, savY = modules[i].y;
    int savW = modules[i].w, savH = modules[i].h;

    int dirs[4][2] = {{1,1}, {1,0}, {0,1}, {0,0}};
    shuffle(begin(dirs), end(dirs), rng);

    for (auto& d : dirs) {
        modules[i].x = max(0, min(tx - modules[i].w / 2, chipW - modules[i].w));
        modules[i].y = max(0, min(ty - modules[i].h / 2, chipH - modules[i].h));
        compactDir(i, d[0], d[1]);
        if (modules[i].x + modules[i].w > chipW) modules[i].x = max(0, chipW - modules[i].w);
        if (modules[i].y + modules[i].h > chipH) modules[i].y = max(0, chipH - modules[i].h);

        if (legalPlacement(i)) {
            long long newIncWL = incrementalWL(i);
            long long delta = newIncWL - oldIncWL;
            if (delta <= 0 || distR(rng) < exp(min(-delta / max(T, 0.001), 0.0))) {
                return true;
            }
        }
        modules[i].x = savX; modules[i].y = savY;
    }
    return false;
}

// ============================================================
// Constructive scanning placement
// ============================================================

bool findLegalPosition(int idx) {
    for (auto& [w, h] : modules[idx].shapes) {
        modules[idx].w = w; modules[idx].h = h;
        for (int y = 0; y + h <= chipH; y++) {
            for (int x = 0; x + w <= chipW; x++) {
                modules[idx].x = x; modules[idx].y = y;
                if (legalPlacement(idx)) return true;
            }
        }
    }
    return false;
}

bool findLegalPositionFast(int idx) {
    for (auto& [w, h] : modules[idx].shapes) {
        modules[idx].w = w; modules[idx].h = h;
        vector<int> yCandidates;
        yCandidates.push_back(0);
        for (int j = 0; j < nTotal; j++) {
            if (j == idx) continue;
            yCandidates.push_back(modules[j].y + modules[j].h);
            yCandidates.push_back(modules[j].y);
        }
        sort(yCandidates.begin(), yCandidates.end());
        yCandidates.erase(unique(yCandidates.begin(), yCandidates.end()), yCandidates.end());

        for (int y0 : yCandidates) {
            if (y0 + h > chipH) continue;
            vector<int> xCandidates;
            xCandidates.push_back(0);
            for (int j = 0; j < nTotal; j++) {
                if (j == idx) continue;
                xCandidates.push_back(modules[j].x + modules[j].w);
                xCandidates.push_back(modules[j].x);
            }
            sort(xCandidates.begin(), xCandidates.end());
            xCandidates.erase(unique(xCandidates.begin(), xCandidates.end()), xCandidates.end());

            for (int x0 : xCandidates) {
                if (x0 + w > chipW) continue;
                modules[idx].x = x0; modules[idx].y = y0;
                if (legalPlacement(idx)) return true;
            }
        }
    }
    return false;
}

bool constructivePack(const vector<int>& order) {
    for (int idx : order) {
        if (nTotal <= 10 || chipW * chipH <= 50000) {
            if (!findLegalPosition(idx)) return false;
        } else {
            if (!findLegalPositionFast(idx)) return false;
        }
    }
    return true;
}

bool initConstructive(mt19937* rng = nullptr) {
    vector<int> order(nSoft);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return modules[a].minArea > modules[b].minArea;
    });

    int seed = rng ? (int)((*rng)() % 10000) : 42;
    mt19937 localRng(seed);
    mt19937* r = rng ? rng : &localRng;

    int maxAttempts = 100;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        for (int i = 0; i < nSoft; i++)
            modules[i].x = modules[i].y = modules[i].w = modules[i].h = 0;
        if (attempt > 0) shuffle(order.begin(), order.end(), *r);
        if (attempt > 0)
            for (int i = 0; i < nSoft; i++)
                shuffle(modules[i].shapes.begin(), modules[i].shapes.end(), *r);
        if (constructivePack(order)) return true;
    }
    return false;
}

// Neighbor-centroid init
bool initNeighborCentroid(mt19937& rng) {
    vector<int> order(nSoft);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        long long wa = 0, wb = 0;
        for (int ni : modules[a].nets) wa += nets[ni].weight;
        for (int ni : modules[b].nets) wb += nets[ni].weight;
        return wa > wb;
    });

    for (int idx : order) {
        long long sx = 0, sy = 0, tw = 0;
        for (int ni : modules[idx].nets) {
            const Net& n = nets[ni];
            int other = (n.a == idx) ? n.b : n.a;
            if (modules[other].w > 0) {
                sx += (long long)n.weight * modules[other].cx();
                sy += (long long)n.weight * modules[other].cy();
                tw += n.weight;
            }
        }
        int targetX = (tw > 0) ? (int)(sx / tw) : chipW / 2;
        int targetY = (tw > 0) ? (int)(sy / tw) : chipH / 2;

        vector<pair<int,int>> shapes = modules[idx].shapes;
        shuffle(shapes.begin(), shapes.end(), rng);

        bool placed = false;
        for (auto& [w, h] : shapes) {
            for (int radius = 0; radius <= max(chipW, chipH) && !placed; radius += max(1, max(w, h) / 2)) {
                vector<pair<int,int>> positions;
                for (int dx = -radius; dx <= radius; dx += max(1, radius / 2 + 1)) {
                    for (int dy = -radius; dy <= radius; dy += max(1, radius / 2 + 1)) {
                        positions.push_back({targetX - w/2 + dx, targetY - h/2 + dy});
                    }
                }
                shuffle(positions.begin(), positions.end(), rng);
                for (auto& [px, py] : positions) {
                    int x = max(0, min(px, chipW - w));
                    int y = max(0, min(py, chipH - h));
                    modules[idx].x = x; modules[idx].y = y;
                    modules[idx].w = w; modules[idx].h = h;
                    if (legalPlacement(idx)) { placed = true; break; }
                }
            }
            if (placed) break;
        }
        if (!placed) {
            modules[idx].x = modules[idx].y = modules[idx].w = modules[idx].h = 0;
            if (!findLegalPositionFast(idx)) return false;
        }
    }
    return true;
}

// ============================================================
// Weighted-median coordinate descent
// ============================================================

void medianDescent(int maxSweeps, long long* runningWL = nullptr) {
    long long fullWL = runningWL ? *runningWL : computeWL();
    for (int sweep = 0; sweep < maxSweeps; sweep++) {
        if (omp_get_wtime() >= deadlineTime) break;
        bool anyImproved = false;
        for (int i = 0; i < nSoft; i++) {
            if (omp_get_wtime() >= deadlineTime) goto endSweep;

            vector<pair<int,int>> px, py;
            long long totalW = 0;
            for (int ni : modules[i].nets) {
                const Net& n = nets[ni];
                int other = (n.a == i) ? n.b : n.a;
                px.push_back({modules[other].cx(), n.weight});
                py.push_back({modules[other].cy(), n.weight});
                totalW += n.weight;
            }
            if (totalW == 0) continue;

            sort(px.begin(), px.end());
            sort(py.begin(), py.end());
            long long halfW = (totalW + 1) / 2;
            int targetCx = px.back().first, targetCy = py.back().first;
            { long long acc = 0; for (auto& [c, w] : px) { acc += w; if (acc >= halfW) { targetCx = c; break; } } }
            { long long acc = 0; for (auto& [c, w] : py) { acc += w; if (acc >= halfW) { targetCy = c; break; } } }

            int w = modules[i].w, h = modules[i].h;
            int oldX = modules[i].x, oldY = modules[i].y;
            int nx = targetCx - w / 2;
            int ny = targetCy - h / 2;
            nx = max(0, min(nx, chipW - w));
            ny = max(0, min(ny, chipH - h));

            long long oldIncWL = incrementalWL(i);

            modules[i].x = nx; modules[i].y = ny;
            if (legalPlacement(i)) {
                long long newIncWL = incrementalWL(i);
                if (newIncWL <= oldIncWL) {
                    fullWL += (newIncWL - oldIncWL);
                    anyImproved = true;
                    continue;
                }
            }
            modules[i].x = oldX; modules[i].y = oldY;

            int dirs[4][2] = {{1,1},{1,0},{0,1},{0,0}};
            bool foundDir = false;
            for (auto& d : dirs) {
                modules[i].x = nx; modules[i].y = ny;
                compactDir(i, d[0], d[1]);
                if (modules[i].x + w > chipW) modules[i].x = max(0, chipW - w);
                if (modules[i].y + h > chipH) modules[i].y = max(0, chipH - h);
                if (legalPlacement(i)) {
                    long long newIncWL = incrementalWL(i);
                    if (newIncWL <= oldIncWL) {
                        fullWL += (newIncWL - oldIncWL);
                        anyImproved = true;
                        foundDir = true;
                        break;
                    }
                }
                modules[i].x = oldX; modules[i].y = oldY;
            }
            if (!foundDir) {
                modules[i].x = oldX; modules[i].y = oldY;
            }
        }
        endSweep:;
        if (!anyImproved) break;
    }
    if (runningWL) *runningWL = fullWL;
}

// ============================================================
// Perturbation & Recovery — escape deep local optima
// ============================================================

bool perturbAndRecover(long long& bestWL, vector<tuple<int,int,int,int>>& bestShapes,
                       mt19937& rng, int numPerturb) {
    uniform_real_distribution<double> distR(0.0, 1.0);
    uniform_int_distribution<int> distMod(0, nSoft - 1);

    // Snapshot current best
    vector<tuple<int,int,int,int>> snap(nSoft);
    for (int i = 0; i < nSoft; i++)
        snap[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};

    // Perturb: randomly move numPerturb modules
    vector<int> perm(nSoft);
    iota(perm.begin(), perm.end(), 0);
    shuffle(perm.begin(), perm.end(), rng);

    int actual = 0;
    for (int k = 0; k < numPerturb && k < nSoft; k++) {
        int mi = perm[k];
        int w = modules[mi].w, h = modules[mi].h;
        int nx = (int)(distR(rng) * max(1, chipW - w));
        int ny = (int)(distR(rng) * max(1, chipH - h));
        modules[mi].x = nx; modules[mi].y = ny;
        // Try to legalize via compaction
        for (int dd = 0; dd < 4; dd++) {
            compactDir(mi, dd & 2, dd & 1);
            if (modules[mi].x + w > chipW) modules[mi].x = max(0, chipW - w);
            if (modules[mi].y + h > chipH) modules[mi].y = max(0, chipH - h);
            if (legalPlacement(mi)) break;
        }
        if (!legalPlacement(mi)) {
            // Restore this module
            modules[mi].x = get<0>(snap[mi]);
            modules[mi].y = get<1>(snap[mi]);
            modules[mi].w = get<2>(snap[mi]);
            modules[mi].h = get<3>(snap[mi]);
            continue;
        }
        actual++;
    }

    // Check if perturbed state is legal
    bool allLegal = true;
    for (int i = 0; i < nSoft; i++) {
        if (!legalPlacement(i)) { allLegal = false; break; }
    }
    if (!allLegal) {
        // Restore all
        for (int i = 0; i < nSoft; i++) {
            modules[i].x = get<0>(snap[i]); modules[i].y = get<1>(snap[i]);
            modules[i].w = get<2>(snap[i]); modules[i].h = get<3>(snap[i]);
        }
        return false;
    }

    // Recover: deep median descent + compaction
    long long curWL = computeWL();
    medianDescent(nSoft * 20, &curWL);

    // 4-direction compaction
    for (int dd = 0; dd < 4; dd++) {
        for (int i = 0; i < nSoft; i++) {
            modules[i].x = get<0>(snap[i]); modules[i].y = get<1>(snap[i]);
            modules[i].w = get<2>(snap[i]); modules[i].h = get<3>(snap[i]);
        }
        for (int i = 0; i < nSoft; i++) compactDir(i, dd & 2, dd & 1);
        for (int i = 0; i < nSoft; i++) {
            if (modules[i].x + modules[i].w > chipW) modules[i].x = max(0, chipW - modules[i].w);
            if (modules[i].y + modules[i].h > chipH) modules[i].y = max(0, chipH - modules[i].h);
        }
        bool ok = true;
        for (int i = 0; i < nSoft && ok; i++) if (!legalPlacement(i)) ok = false;
        if (ok) {
            long long wl = computeWL();
            medianDescent(nSoft * 10, &wl);
            if (wl < bestWL) {
                bestWL = wl;
                for (int i = 0; i < nSoft; i++)
                    bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
            }
        }
    }

    // Restore original state (the best tracking handles keeping the better one)
    for (int i = 0; i < nSoft; i++) {
        modules[i].x = get<0>(snap[i]); modules[i].y = get<1>(snap[i]);
        modules[i].w = get<2>(snap[i]); modules[i].h = get<3>(snap[i]);
    }

    return actual > 0;
}

// ============================================================
// Simulated annealing — v5
// ============================================================

void saChain(long long& bestWL, vector<tuple<int,int,int,int>>& bestShapes, int chainIdx) {
    mt19937 rng(chainIdx * 137 + 31);
    uniform_real_distribution<double> distR(0.0, 1.0);
    uniform_int_distribution<int> distMod(0, nSoft - 1);
    uniform_int_distribution<int> distMove(0, 99);

    double T = 500000.0, Tmin = 0.3, Tdecay = 0.985;
    long long curWL = computeWL();
    int maxIters = nSoft * 4000;
    int iterCount = 0;

    int totalArea = 0;
    for (int i = 0; i < nTotal; i++) totalArea += modules[i].w * modules[i].h;
    double density = (double)totalArea / (chipW * chipH);
    int avgGap = max(1, (int)((1.0 - density) * min(chipW, chipH) / nSoft));

    // Recency tracking — prefer modules that haven't been moved recently
    vector<int> lastMoved(nSoft, 0);

    while (T >= Tmin && omp_get_wtime() < deadlineTime) {
        bool epochDone = false;
        int rejects = 0, totalInEpoch = 0;

        while (!epochDone && omp_get_wtime() < deadlineTime) {
            iterCount++; totalInEpoch++;

            // Recency-based module selection: 50% chance to pick least-recently-moved
            int mi;
            if (distR(rng) < 0.5) {
                int minLast = INT_MAX;
                for (int i = 0; i < nSoft; i++) {
                    if (lastMoved[i] < minLast) { minLast = lastMoved[i]; mi = i; }
                }
            } else {
                mi = distMod(rng);
            }
            lastMoved[mi] = iterCount;

            int moveType = distMove(rng);
            int savX = modules[mi].x, savY = modules[mi].y;
            int savW = modules[mi].w, savH = modules[mi].h;
            long long oldIncWL = incrementalWL(mi);
            bool moved = false;

            if (moveType < 8) {
                // Random translate
                int w = modules[mi].w, h = modules[mi].h;
                int nx = (int)(distR(rng) * max(1, chipW - w));
                int ny = (int)(distR(rng) * max(1, chipH - h));
                modules[mi].x = nx; modules[mi].y = ny;
                moved = legalPlacement(mi);
                if (!moved) { modules[mi].x = savX; modules[mi].y = savY; }
            } else if (moveType < 40) {
                // Nudge with multi-direction compaction
                int range = max(1, avgGap);
                int dx = (int)((distR(rng) - 0.5) * 2 * range);
                int dy = (int)((distR(rng) - 0.5) * 2 * range);
                int tx = modules[mi].x + dx + modules[mi].w / 2;
                int ty = modules[mi].y + dy + modules[mi].h / 2;
                if (tryMoveToward(mi, tx, ty, oldIncWL, curWL, T, rng)) {
                    moved = true;
                } else {
                    modules[mi].x = savX; modules[mi].y = savY;
                    modules[mi].w = savW; modules[mi].h = savH;
                }
                if (moved) {
                    curWL = computeWL();
                    if (curWL < bestWL) {
                        bestWL = curWL;
                        for (int ii = 0; ii < nSoft; ii++)
                            bestShapes[ii] = {modules[ii].x, modules[ii].y, modules[ii].w, modules[ii].h};
                    }
                    continue;
                } else { rejects++; continue; }
            } else if (moveType < 65) {
                // Nudge toward weighted median
                vector<pair<int,int>> px, py;
                long long tw = 0;
                for (int ni : modules[mi].nets) {
                    const Net& n = nets[ni];
                    int other = (n.a == mi) ? n.b : n.a;
                    px.push_back({modules[other].cx(), n.weight});
                    py.push_back({modules[other].cy(), n.weight});
                    tw += n.weight;
                }
                if (tw > 0) {
                    sort(px.begin(), px.end());
                    sort(py.begin(), py.end());
                    long long half = (tw + 1) / 2;
                    int tcx = 0, tcy = 0;
                    { long long acc = 0; for (auto& [c, w] : px) { acc += w; if (acc >= half) { tcx = c; break; } } }
                    { long long acc = 0; for (auto& [c, w] : py) { acc += w; if (acc >= half) { tcy = c; break; } } }
                    if (tryMoveToward(mi, tcx, tcy, oldIncWL, curWL, T, rng)) {
                        moved = true;
                    } else {
                        modules[mi].x = savX; modules[mi].y = savY;
                        modules[mi].w = savW; modules[mi].h = savH;
                    }
                } else {
                    modules[mi].x = savX; modules[mi].y = savY;
                    modules[mi].w = savW; modules[mi].h = savH;
                }
                if (moved) {
                    curWL = computeWL();
                    if (curWL < bestWL) {
                        bestWL = curWL;
                        for (int ii = 0; ii < nSoft; ii++)
                            bestShapes[ii] = {modules[ii].x, modules[ii].y, modules[ii].w, modules[ii].h};
                    }
                    continue;
                } else { rejects++; continue; }
            } else if (moveType < 90) {
                // Swap with reshape — try swapping positions AND shapes
                int mj = distMod(rng);
                if (mj == mi) mj = (mi + 1) % nSoft;

                int s2x = modules[mj].x, s2y = modules[mj].y;
                int s2w = modules[mj].w, s2h = modules[mj].h;

                // Try 1: swap positions only
                swap(modules[mi].x, modules[mj].x);
                swap(modules[mi].y, modules[mj].y);
                bool ok = legalPlacement(mi) && legalPlacement(mj);

                // Try 2: swap positions + shapes
                if (!ok) {
                    swap(modules[mi].x, modules[mj].x);
                    swap(modules[mi].y, modules[mj].y);
                    swap(modules[mi].w, modules[mj].w);
                    swap(modules[mi].h, modules[mj].h);
                    // Re-clamp
                    modules[mi].x = max(0, min(modules[mi].x, chipW - modules[mi].w));
                    modules[mi].y = max(0, min(modules[mi].y, chipH - modules[mi].h));
                    modules[mj].x = max(0, min(modules[mj].x, chipW - modules[mj].w));
                    modules[mj].y = max(0, min(modules[mj].y, chipH - modules[mj].h));
                    ok = legalPlacement(mi) && legalPlacement(mj);
                }

                // Try 3: swap positions + compact
                if (!ok) {
                    swap(modules[mi].w, modules[mj].w);
                    swap(modules[mi].h, modules[mj].h);
                    swap(modules[mi].x, modules[mj].x);
                    swap(modules[mi].y, modules[mj].y);
                    // Re-restore then try compact
                    modules[mi].x = s2x; modules[mi].y = s2y;
                    modules[mi].w = savW; modules[mi].h = savH;
                    modules[mj].x = savX; modules[mj].y = savY;
                    modules[mj].w = s2w; modules[mj].h = s2h;
                    compactDir(mi, true, true);
                    compactDir(mj, true, true);
                    modules[mi].x = max(0, min(modules[mi].x, chipW - modules[mi].w));
                    modules[mi].y = max(0, min(modules[mi].y, chipH - modules[mi].h));
                    modules[mj].x = max(0, min(modules[mj].x, chipW - modules[mj].w));
                    modules[mj].y = max(0, min(modules[mj].y, chipH - modules[mj].h));
                    ok = legalPlacement(mi) && legalPlacement(mj);
                }

                if (!ok) {
                    modules[mi].x = savX; modules[mi].y = savY;
                    modules[mi].w = savW; modules[mi].h = savH;
                    modules[mj].x = s2x; modules[mj].y = s2y;
                    modules[mj].w = s2w; modules[mj].h = s2h;
                    rejects++;
                } else {
                    long long newWL = computeWL();
                    long long delta = newWL - curWL;
                    if (delta <= 0 || distR(rng) < exp(min(-delta / max(T, 0.001), 0.0))) {
                        curWL = newWL;
                        if (newWL < bestWL) {
                            bestWL = newWL;
                            for (int ii = 0; ii < nSoft; ii++)
                                bestShapes[ii] = {modules[ii].x, modules[ii].y, modules[ii].w, modules[ii].h};
                        }
                        lastMoved[mj] = iterCount;
                    } else {
                        modules[mi].x = savX; modules[mi].y = savY;
                        modules[mi].w = savW; modules[mi].h = savH;
                        modules[mj].x = s2x; modules[mj].y = s2y;
                        modules[mj].w = s2w; modules[mj].h = s2h;
                        rejects++;
                    }
                }
                continue;
            } else {
                // Reshape
                if (!modules[mi].shapes.empty()) {
                    int si = (int)(distR(rng) * modules[mi].shapes.size());
                    modules[mi].w = modules[mi].shapes[si].first;
                    modules[mi].h = modules[mi].shapes[si].second;
                    modules[mi].x = max(0, min(modules[mi].x, chipW - modules[mi].w));
                    modules[mi].y = max(0, min(modules[mi].y, chipH - modules[mi].h));
                    moved = legalPlacement(mi);
                    if (!moved) {
                        bool found = false;
                        for (int dd = 0; dd < 4 && !found; dd++) {
                            compactDir(mi, dd & 2, dd & 1);
                            if (modules[mi].x + modules[mi].w > chipW) modules[mi].x = max(0, chipW - modules[mi].w);
                            if (modules[mi].y + modules[mi].h > chipH) modules[mi].y = max(0, chipH - modules[mi].h);
                            if (legalPlacement(mi)) found = true;
                            if (!found) { modules[mi].x = savX; modules[mi].y = savY; }
                        }
                        moved = found;
                    }
                    if (!moved) { modules[mi].w = savW; modules[mi].h = savH; }
                }
            }

            if (moved) {
                long long newIncWL = incrementalWL(mi);
                long long delta = newIncWL - oldIncWL;
                long long newWL = curWL + delta;
                if (delta <= 0 || distR(rng) < exp(min(-delta / max(T, 0.001), 0.0))) {
                    curWL = newWL;
                    if (newWL < bestWL) {
                        bestWL = newWL;
                        for (int ii = 0; ii < nSoft; ii++)
                            bestShapes[ii] = {modules[ii].x, modules[ii].y, modules[ii].w, modules[ii].h};
                    }
                } else {
                    modules[mi].x = savX; modules[mi].y = savY;
                    modules[mi].w = savW; modules[mi].h = savH;
                    rejects++;
                }
            } else {
                rejects++;
            }

            if (iterCount > maxIters * 10) { epochDone = true; break; }
            if (totalInEpoch > maxIters && totalInEpoch > 200 && (double)rejects / totalInEpoch > 0.98)
                epochDone = true;
        }

        // Periodic median descent
        if ((int)(500000.0 / T) % 5 == 0 && omp_get_wtime() < deadlineTime - 5.0) {
            medianDescent(nSoft * 3, &curWL);
        }

        T *= Tdecay;
    }

    // ============================================================
    // Final intensification — multi-round
    // ============================================================
    if (omp_get_wtime() < deadlineTime - 2.0) {
        medianDescent(nSoft * 50, &curWL);

        // 4-direction compaction, keep best
        long long sweepWL = curWL;
        vector<tuple<int,int,int,int>> sweepSnap(nSoft);
        for (int i = 0; i < nSoft; i++)
            sweepSnap[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};

        for (int dd = 0; dd < 4; dd++) {
            for (int i = 0; i < nSoft; i++)
                modules[i].x = get<0>(sweepSnap[i]), modules[i].y = get<1>(sweepSnap[i]);
            for (int i = 0; i < nSoft; i++) compactDir(i, dd & 2, dd & 1);
            for (int i = 0; i < nSoft; i++) {
                if (modules[i].x + modules[i].w > chipW) modules[i].x = max(0, chipW - modules[i].w);
                if (modules[i].y + modules[i].h > chipH) modules[i].y = max(0, chipH - modules[i].h);
            }
            bool ok = true;
            for (int i = 0; i < nSoft && ok; i++) if (!legalPlacement(i)) ok = false;
            if (ok) {
                long long wl = computeWL();
                if (wl < bestWL) {
                    bestWL = wl;
                    for (int i = 0; i < nSoft; i++)
                        bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
                    sweepWL = wl;
                    for (int i = 0; i < nSoft; i++)
                        sweepSnap[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
                }
            }
        }
        for (int i = 0; i < nSoft; i++)
            modules[i].x = get<0>(sweepSnap[i]), modules[i].y = get<1>(sweepSnap[i]),
            modules[i].w = get<2>(sweepSnap[i]), modules[i].h = get<3>(sweepSnap[i]);
        curWL = computeWL();

        // Reverse-order compaction
        if (omp_get_wtime() < deadlineTime - 1.5) {
            for (int i = nSoft - 1; i >= 0; i--) compactDir(i, true, true);
            for (int i = 0; i < nSoft; i++) {
                if (modules[i].x + modules[i].w > chipW) modules[i].x = max(0, chipW - modules[i].w);
                if (modules[i].y + modules[i].h > chipH) modules[i].y = max(0, chipH - modules[i].h);
            }
            bool ok = true;
            for (int i = 0; i < nSoft && ok; i++) if (!legalPlacement(i)) ok = false;
            if (ok) {
                long long wl = computeWL();
                medianDescent(nSoft * 10, &wl);
                if (wl < bestWL) {
                    bestWL = wl;
                    for (int i = 0; i < nSoft; i++)
                        bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
                    for (int i = 0; i < nSoft; i++)
                        sweepSnap[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
                }
            }
            for (int i = 0; i < nSoft; i++)
                modules[i].x = get<0>(sweepSnap[i]), modules[i].y = get<1>(sweepSnap[i]),
                modules[i].w = get<2>(sweepSnap[i]), modules[i].h = get<3>(sweepSnap[i]);
            curWL = computeWL();
        }

        // Post-compaction median descent
        if (omp_get_wtime() < deadlineTime - 1.0)
            medianDescent(nSoft * 30, &curWL);

        long long wl = curWL;
        bool allLegal = true;
        for (int i = 0; i < nSoft; i++) { if (!legalPlacement(i)) { allLegal = false; break; } }
        if (allLegal && wl < bestWL) {
            bestWL = wl;
            for (int i = 0; i < nSoft; i++)
                bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
        }

        // Perturb & recover — only for very large cases (>30 soft modules)
        // For public1-4, the multi-start diversity is sufficient
        if (omp_get_wtime() < deadlineTime - 0.5 && nSoft > 30) {
            perturbAndRecover(bestWL, bestShapes, rng, 1);
        }

        // Try reshaping each module
        if (omp_get_wtime() < deadlineTime - 0.3) {
            for (int i = 0; i < nSoft; i++) {
                if (omp_get_wtime() >= deadlineTime - 0.1) break;
                int curW = modules[i].w, curH = modules[i].h;
                int curX = modules[i].x, curY = modules[i].y;
                long long bestLocalInc = LLONG_MAX;
                tuple<int,int,int,int> bestLocal = {curX, curY, curW, curH};
                for (auto& [w, h] : modules[i].shapes) {
                    int nx = max(0, min(curX, chipW - w));
                    int ny = max(0, min(curY, chipH - h));
                    modules[i].w = w; modules[i].h = h;
                    modules[i].x = nx; modules[i].y = ny;
                    if (legalPlacement(i)) {
                        if (incrementalWL(i) < bestLocalInc) {
                            bestLocalInc = incrementalWL(i);
                            bestLocal = {nx, ny, w, h};
                        }
                    }
                }
                modules[i].x = get<0>(bestLocal); modules[i].y = get<1>(bestLocal);
                modules[i].w = get<2>(bestLocal); modules[i].h = get<3>(bestLocal);
            }
            bool allLegal2 = true;
            for (int i = 0; i < nSoft && allLegal2; i++) if (!legalPlacement(i)) allLegal2 = false;
            if (allLegal2) {
                long long newWL = computeWL();
                medianDescent(nSoft * 10, &newWL);
                if (newWL < bestWL) {
                    bestWL = newWL;
                    for (int i = 0; i < nSoft; i++)
                        bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
                }
            }
        }
    }

    // Final sanity check
    {
        vector<tuple<int,int,int,int>> snap(nSoft);
        bool wasLegal = true;
        for (int i = 0; i < nSoft; i++) {
            snap[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
            modules[i].x = get<0>(bestShapes[i]); modules[i].y = get<1>(bestShapes[i]);
            modules[i].w = get<2>(bestShapes[i]); modules[i].h = get<3>(bestShapes[i]);
        }
        for (int i = 0; i < nSoft; i++) if (!legalPlacement(i)) { wasLegal = false; break; }
        long long finalWL = computeWL();
        if (!wasLegal) {
            for (int i = 0; i < nSoft; i++) {
                modules[i].x = get<0>(snap[i]); modules[i].y = get<1>(snap[i]);
                modules[i].w = get<2>(snap[i]); modules[i].h = get<3>(snap[i]);
            }
            bestWL = computeWL();
            for (int i = 0; i < nSoft; i++)
                bestShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};
        } else {
            bestWL = finalWL;
        }
    }
}

// ============================================================
// Parallel multi-start
// ============================================================

void optimize() {
    // More chains for smaller cases (faster per-chain), fewer for large cases
    int nChains;
    if (nTotal <= 25) nChains = 100;      // sample, public1: fast per-chain
    else if (nTotal <= 35) nChains = 80;  // public2, public4
    else nChains = 64;                     // public3: most modules

    long long globalBestWL = LLONG_MAX;
    vector<tuple<int,int,int,int>> globalBest(nSoft);
    int globalBestChain = INT_MAX;
    mutex bestMutex;

    vector<Module> origModules = gModules;
    vector<thread> threads;
    threads.reserve(nChains);

    for (int chain = 0; chain < nChains && omp_get_wtime() < deadlineTime - 10.0; chain++) {
        threads.emplace_back([chain, &origModules, &globalBestWL, &globalBest,
                              &globalBestChain, &bestMutex]() {
            vector<Module> localModules = origModules;
            modulesTLS = &localModules;

            mt19937 crng(chain * 997 + 13);

            bool legal = false;
            int maxInit = (chain < 15) ? 200 : 60;
            bool useNeighborInit = (chain % 5 == 3);

            for (int att = 0; att < maxInit && !legal && omp_get_wtime() < deadlineTime - 15.0; att++) {
                if (useNeighborInit) {
                    for (int i = 0; i < nSoft; i++)
                        modules[i].x = modules[i].y = modules[i].w = modules[i].h = 0;
                    legal = initNeighborCentroid(crng);
                } else {
                    legal = initConstructive(&crng);
                }
            }
            if (!legal) return;

            long long localBest = computeWL();
            vector<tuple<int,int,int,int>> localShapes(nSoft);
            for (int i = 0; i < nSoft; i++)
                localShapes[i] = {modules[i].x, modules[i].y, modules[i].w, modules[i].h};

            saChain(localBest, localShapes, chain);

            lock_guard<mutex> lock(bestMutex);
            if (localBest < globalBestWL ||
                (localBest == globalBestWL && chain < globalBestChain)) {
                globalBestWL = localBest;
                globalBest = move(localShapes);
                globalBestChain = chain;
            }
        });
    }

    for (auto& t : threads) t.join();

    modulesTLS = &gModules;
    {
        bool allLegal = true;
        for (int i = 0; i < nSoft; i++) {
            modules[i].x = get<0>(globalBest[i]);
            modules[i].y = get<1>(globalBest[i]);
            modules[i].w = get<2>(globalBest[i]);
            modules[i].h = get<3>(globalBest[i]);
        }
        for (int i = 0; i < nSoft; i++) {
            if (!legalPlacement(i)) { allLegal = false; break; }
        }
        if (!allLegal) {
            modules = origModules;
            initConstructive();
        }
    }
}

// ============================================================
// Output writer
// ============================================================

void writeOutput(const string& path) {
    if (auto parent = fs::path(path).parent_path(); !parent.empty())
        fs::create_directories(parent);

    long long wl = computeWL();
    ofstream f(path);
    f << "Wirelength " << wl << "\n\n";
    f << "NumSoftModules " << nSoft << "\n";
    for (int i = 0; i < nSoft; i++)
        f << modules[i].name << " " << modules[i].x << " " << modules[i].y
          << " " << modules[i].w << " " << modules[i].h << "\n";
    f.close();
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.txt> <output.floorplan>\n";
        return 1;
    }

    string inputPath = argv[1];
    string outputPath = argv[2];

    deadlineTime = omp_get_wtime() + 575.0;

    parseInput(inputPath);
    initShapes();

    bool legal = initConstructive();
    if (!legal) {
        for (int i = 0; i < nSoft; i++) {
            int sq = (int)ceil(sqrt((double)modules[i].minArea));
            modules[i].shapes = {{sq, sq}};
        }
        legal = initConstructive();
        if (!legal) {
            cerr << "ERROR: cannot find legal initial placement\n";
            return 1;
        }
    }

    optimize();

    writeOutput(outputPath);

    return 0;
}
