#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct Shape {
    int w = 0, h = 0;
};

struct Net {
    int a = 0, b = 0, wt = 0;
};

struct Module {
    string name;
    bool fixed = false;
    int area = 0;
    Rect r;
    vector<Shape> shapes;
    vector<int> incident;
};

struct Problem {
    int W = 0, H = 0;
    vector<Module> mods;
    vector<int> softIds;
    vector<int> fixedIds;
    vector<Net> nets;
    unordered_map<string, int> idOf;
};

struct Saved {
    vector<Rect> rects;
    long long wl = numeric_limits<long long>::max();
    bool ok = false;
};

static chrono::steady_clock::time_point gStart;
static double gLimitSeconds = 570.0;

static bool timedOut() {
    auto now = chrono::steady_clock::now();
    return chrono::duration<double>(now - gStart).count() > gLimitSeconds;
}

static vector<string> splitWords(const string &line) {
    vector<string> out;
    istringstream iss(line);
    string s;
    while (iss >> s) out.push_back(s);
    return out;
}

static int getId(Problem &p, const string &name) {
    auto it = p.idOf.find(name);
    if (it != p.idOf.end()) return it->second;
    int id = static_cast<int>(p.mods.size());
    p.idOf[name] = id;
    Module m;
    m.name = name;
    p.mods.push_back(m);
    return id;
}

static bool intersects(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static bool containsRect(const Rect &a, const Rect &b) {
    return a.x <= b.x && a.y <= b.y && a.x + a.w >= b.x + b.w && a.y + a.h >= b.y + b.h;
}

static bool validShape(int area, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (1LL * w * h < area) return false;
    double ratio = static_cast<double>(h) / static_cast<double>(w);
    return ratio >= 0.5 - 1e-9 && ratio <= 2.0 + 1e-9;
}

static vector<Shape> generateShapes(int area, int chipW, int chipH) {
    vector<Shape> shapes;
    int lo = max(1, static_cast<int>(ceil(sqrt(area / 2.0))) - 3);
    int hi = static_cast<int>(floor(sqrt(2.0 * area))) + 3;
    hi = min(hi, chipW);
    for (int w = lo; w <= hi; ++w) {
        int h = (area + w - 1) / w;
        if (h <= chipH && validShape(area, w, h)) shapes.push_back({w, h});
    }
    int sq = max(1, static_cast<int>(sqrt(static_cast<double>(area))));
    for (int dw = -8; dw <= 8; ++dw) {
        int w = sq + dw;
        if (w <= 0 || w > chipW) continue;
        int h = (area + w - 1) / w;
        if (h <= chipH && validShape(area, w, h)) shapes.push_back({w, h});
    }
    sort(shapes.begin(), shapes.end(), [](const Shape &a, const Shape &b) {
        long long aa = 1LL * a.w * a.h, bb = 1LL * b.w * b.h;
        if (aa != bb) return aa < bb;
        return abs(a.w - a.h) < abs(b.w - b.h);
    });
    shapes.erase(unique(shapes.begin(), shapes.end(), [](const Shape &a, const Shape &b) {
        return a.w == b.w && a.h == b.h;
    }), shapes.end());
    return shapes;
}

static Problem parseInput(const string &path) {
    ifstream in(path);
    if (!in) throw runtime_error("cannot open input");
    vector<vector<string>> lines;
    string line;
    while (getline(in, line)) {
        auto tok = splitWords(line);
        if (!tok.empty()) lines.push_back(tok);
    }
    Problem p;
    size_t i = 0;
    p.W = stoi(lines[i][1]);
    p.H = stoi(lines[i][2]);
    ++i;
    int ns = stoi(lines[i][1]);
    ++i;
    for (int k = 0; k < ns; ++k, ++i) {
        int id = getId(p, lines[i][1]);
        p.mods[id].fixed = false;
        p.mods[id].area = stoi(lines[i][2]);
        p.softIds.push_back(id);
    }
    int nf = stoi(lines[i][1]);
    ++i;
    for (int k = 0; k < nf; ++k, ++i) {
        int id = getId(p, lines[i][1]);
        p.mods[id].fixed = true;
        p.mods[id].r = {stoi(lines[i][2]), stoi(lines[i][3]), stoi(lines[i][4]), stoi(lines[i][5])};
        p.mods[id].area = p.mods[id].r.w * p.mods[id].r.h;
        p.fixedIds.push_back(id);
    }
    for (int id : p.softIds) {
        p.mods[id].shapes = generateShapes(p.mods[id].area, p.W, p.H);
        if (p.mods[id].shapes.empty()) throw runtime_error("no legal shape for " + p.mods[id].name);
    }
    int nn = stoi(lines[i][1]);
    ++i;
    for (int k = 0; k < nn; ++k, ++i) {
        int a = getId(p, lines[i][1]);
        int b = getId(p, lines[i][2]);
        int wt = stoi(lines[i][3]);
        int ni = static_cast<int>(p.nets.size());
        p.nets.push_back({a, b, wt});
        p.mods[a].incident.push_back(ni);
        p.mods[b].incident.push_back(ni);
    }
    return p;
}

static pair<int, int> centerOf(const Rect &r) {
    return {r.x + r.w / 2, r.y + r.h / 2};
}

static long long totalWL(const Problem &p, const vector<Rect> &rects) {
    long long wl = 0;
    for (const auto &n : p.nets) {
        auto ca = centerOf(rects[n.a]);
        auto cb = centerOf(rects[n.b]);
        wl += 1LL * n.wt * (llabs(ca.first - cb.first) + llabs(ca.second - cb.second));
    }
    return wl;
}

static pair<int, int> centerWithAlt(const vector<Rect> &rects, int id, int altId, const Rect *alt) {
    if (alt && id == altId) return centerOf(*alt);
    return centerOf(rects[id]);
}

static long long incidentCost(const Problem &p, const vector<Rect> &rects, int id, const Rect *alt = nullptr) {
    long long wl = 0;
    for (int ni : p.mods[id].incident) {
        const auto &n = p.nets[ni];
        auto ca = centerWithAlt(rects, n.a, id, alt);
        auto cb = centerWithAlt(rects, n.b, id, alt);
        wl += 1LL * n.wt * (llabs(ca.first - cb.first) + llabs(ca.second - cb.second));
    }
    return wl;
}

static bool legalOne(const Problem &p, const vector<Rect> &rects, int id) {
    const Rect &r = rects[id];
    if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 || r.x + r.w > p.W || r.y + r.h > p.H) return false;
    if (!p.mods[id].fixed && !validShape(p.mods[id].area, r.w, r.h)) return false;
    for (size_t j = 0; j < rects.size(); ++j) {
        if (static_cast<int>(j) == id) continue;
        if (rects[j].w > 0 && rects[j].h > 0 && intersects(r, rects[j])) return false;
    }
    return true;
}

static bool allLegal(const Problem &p, const vector<Rect> &rects) {
    for (int id : p.fixedIds) {
        if (rects[id].x != p.mods[id].r.x || rects[id].y != p.mods[id].r.y ||
            rects[id].w != p.mods[id].r.w || rects[id].h != p.mods[id].r.h) return false;
    }
    for (int id : p.softIds) {
        if (!legalOne(p, rects, id)) return false;
    }
    return true;
}

static void pruneFree(vector<Rect> &freeRects) {
    for (auto &r : freeRects) {
        if (r.w < 0) r.w = 0;
        if (r.h < 0) r.h = 0;
    }
    freeRects.erase(remove_if(freeRects.begin(), freeRects.end(), [](const Rect &r) {
        return r.w <= 0 || r.h <= 0;
    }), freeRects.end());
    for (size_t i = 0; i < freeRects.size(); ++i) {
        for (size_t j = i + 1; j < freeRects.size();) {
            if (containsRect(freeRects[i], freeRects[j])) {
                freeRects.erase(freeRects.begin() + j);
            } else if (containsRect(freeRects[j], freeRects[i])) {
                freeRects.erase(freeRects.begin() + i);
                --i;
                break;
            } else {
                ++j;
            }
        }
    }
}

static void subtractRect(vector<Rect> &freeRects, const Rect &used) {
    vector<Rect> next;
    next.reserve(freeRects.size() * 2 + 4);
    for (const Rect &f : freeRects) {
        if (!intersects(f, used)) {
            next.push_back(f);
            continue;
        }
        int fx2 = f.x + f.w, fy2 = f.y + f.h;
        int ux2 = used.x + used.w, uy2 = used.y + used.h;
        if (used.x > f.x) next.push_back({f.x, f.y, used.x - f.x, f.h});
        if (ux2 < fx2) next.push_back({ux2, f.y, fx2 - ux2, f.h});
        if (used.y > f.y) next.push_back({f.x, f.y, f.w, used.y - f.y});
        if (uy2 < fy2) next.push_back({f.x, uy2, f.w, fy2 - uy2});
    }
    freeRects.swap(next);
    pruneFree(freeRects);
}

static vector<Rect> buildFreeRects(const Problem &p, const vector<Rect> &rects, int skipId = -1) {
    vector<Rect> freeRects = {{0, 0, p.W, p.H}};
    for (size_t id = 0; id < rects.size(); ++id) {
        if (static_cast<int>(id) == skipId) continue;
        if (rects[id].w > 0 && rects[id].h > 0) subtractRect(freeRects, rects[id]);
    }
    return freeRects;
}

static pair<int, int> medianTarget(const Problem &p, const vector<Rect> &rects, int id) {
    vector<pair<int, int>> xs, ys;
    int total = 0;
    for (int ni : p.mods[id].incident) {
        const auto &n = p.nets[ni];
        int other = n.a == id ? n.b : n.a;
        if (rects[other].w <= 0 || rects[other].h <= 0) continue;
        auto c = centerOf(rects[other]);
        xs.push_back({c.first, n.wt});
        ys.push_back({c.second, n.wt});
        total += n.wt;
    }
    if (xs.empty()) return {p.W / 2, p.H / 2};
    auto pick = [total](vector<pair<int, int>> v) {
        sort(v.begin(), v.end());
        int acc = 0;
        for (auto [coord, wt] : v) {
            acc += wt;
            if (acc * 2 >= total) return coord;
        }
        return v.back().first;
    };
    return {pick(xs), pick(ys)};
}

static int clampInt(int v, int lo, int hi) {
    if (hi < lo) return lo;
    return min(max(v, lo), hi);
}

static vector<Shape> shapeSubset(const vector<Shape> &all, int maxCount, mt19937 &rng) {
    if (static_cast<int>(all.size()) <= maxCount) return all;
    vector<Shape> out;
    out.reserve(maxCount + 16);
    int n = static_cast<int>(all.size());
    for (int i = 0; i < maxCount; ++i) {
        int idx = static_cast<int>((1LL * i * (n - 1)) / max(1, maxCount - 1));
        out.push_back(all[idx]);
    }
    uniform_int_distribution<int> dist(0, n - 1);
    for (int i = 0; i < 16; ++i) out.push_back(all[dist(rng)]);
    sort(out.begin(), out.end(), [](const Shape &a, const Shape &b) {
        if (a.w != b.w) return a.w < b.w;
        return a.h < b.h;
    });
    out.erase(unique(out.begin(), out.end(), [](const Shape &a, const Shape &b) {
        return a.w == b.w && a.h == b.h;
    }), out.end());
    return out;
}

static Rect bestPlacementInFree(const Problem &p, const vector<Rect> &rects, const vector<Rect> &freeRects,
                                int id, pair<int, int> target, mt19937 &rng, bool broad,
                                long long *scoreOut = nullptr) {
    Rect best;
    long long bestScore = numeric_limits<long long>::max();
    int shapeCount = broad ? 260 : 90;
    auto shapes = shapeSubset(p.mods[id].shapes, shapeCount, rng);
    for (const auto &fr : freeRects) {
        for (const auto &s : shapes) {
            if (s.w > fr.w || s.h > fr.h) continue;
            vector<pair<int, int>> pos;
            int tx = clampInt(target.first - s.w / 2, fr.x, fr.x + fr.w - s.w);
            int ty = clampInt(target.second - s.h / 2, fr.y, fr.y + fr.h - s.h);
            pos.push_back({tx, ty});
            pos.push_back({fr.x, fr.y});
            pos.push_back({fr.x + fr.w - s.w, fr.y});
            pos.push_back({fr.x, fr.y + fr.h - s.h});
            pos.push_back({fr.x + fr.w - s.w, fr.y + fr.h - s.h});
            pos.push_back({tx, fr.y});
            pos.push_back({tx, fr.y + fr.h - s.h});
            pos.push_back({fr.x, ty});
            pos.push_back({fr.x + fr.w - s.w, ty});
            if (broad) {
                pos.push_back({fr.x + (fr.w - s.w) / 2, fr.y + (fr.h - s.h) / 2});
                uniform_int_distribution<int> dx(fr.x, fr.x + fr.w - s.w);
                uniform_int_distribution<int> dy(fr.y, fr.y + fr.h - s.h);
                for (int k = 0; k < 4; ++k) pos.push_back({dx(rng), dy(rng)});
            }
            sort(pos.begin(), pos.end());
            pos.erase(unique(pos.begin(), pos.end()), pos.end());
            for (auto [x, y] : pos) {
                Rect cand{x, y, s.w, s.h};
                long long c = incidentCost(p, rects, id, &cand);
                long long dist = llabs((x + s.w / 2) - target.first) + llabs((y + s.h / 2) - target.second);
                long long waste = static_cast<long long>(fr.w - s.w) * (fr.h - s.h);
                long long score = c * 1000000LL + dist * 100LL + waste / 1000;
                if (score < bestScore) {
                    bestScore = score;
                    best = cand;
                }
            }
        }
    }
    if (scoreOut) *scoreOut = bestScore;
    return best;
}

static vector<Rect> topPlacementsInFree(const Problem &p, const vector<Rect> &rects, const vector<Rect> &freeRects,
                                        int id, pair<int, int> target, mt19937 &rng, int maxOut) {
    vector<pair<long long, Rect>> scored;
    auto shapes = shapeSubset(p.mods[id].shapes, 80, rng);
    for (const auto &fr : freeRects) {
        for (const auto &s : shapes) {
            if (s.w > fr.w || s.h > fr.h) continue;
            int tx = clampInt(target.first - s.w / 2, fr.x, fr.x + fr.w - s.w);
            int ty = clampInt(target.second - s.h / 2, fr.y, fr.y + fr.h - s.h);
            vector<pair<int, int>> pos = {
                {tx, ty},
                {fr.x, fr.y},
                {fr.x + fr.w - s.w, fr.y},
                {fr.x, fr.y + fr.h - s.h},
                {fr.x + fr.w - s.w, fr.y + fr.h - s.h},
                {tx, fr.y},
                {tx, fr.y + fr.h - s.h},
                {fr.x, ty},
                {fr.x + fr.w - s.w, ty},
                {fr.x + (fr.w - s.w) / 2, fr.y + (fr.h - s.h) / 2}
            };
            sort(pos.begin(), pos.end());
            pos.erase(unique(pos.begin(), pos.end()), pos.end());
            for (auto [x, y] : pos) {
                Rect cand{x, y, s.w, s.h};
                long long c = incidentCost(p, rects, id, &cand);
                long long dist = llabs((x + s.w / 2) - target.first) + llabs((y + s.h / 2) - target.second);
                long long score = c * 1000000LL + dist * 100LL + 1LL * s.w * s.h;
                scored.push_back({score, cand});
            }
        }
    }
    sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        if (a.first != b.first) return a.first < b.first;
        if (a.second.x != b.second.x) return a.second.x < b.second.x;
        if (a.second.y != b.second.y) return a.second.y < b.second.y;
        if (a.second.w != b.second.w) return a.second.w < b.second.w;
        return a.second.h < b.second.h;
    });
    vector<Rect> out;
    for (const auto &sr : scored) {
        bool dup = false;
        for (const auto &r : out) {
            if (r.x == sr.second.x && r.y == sr.second.y && r.w == sr.second.w && r.h == sr.second.h) {
                dup = true;
                break;
            }
        }
        if (!dup) out.push_back(sr.second);
        if (static_cast<int>(out.size()) >= maxOut) break;
    }
    return out;
}

static vector<pair<int, int>> relaxedTargets(const Problem &p) {
    vector<pair<double, double>> cur(p.mods.size(), {p.W / 2.0, p.H / 2.0});
    for (int id : p.fixedIds) {
        auto c = centerOf(p.mods[id].r);
        cur[id] = {static_cast<double>(c.first), static_cast<double>(c.second)};
    }
    for (int it = 0; it < 80; ++it) {
        auto nxt = cur;
        for (int id : p.softIds) {
            double sx = p.W * 0.02, sy = p.H * 0.02, sw = 0.04;
            for (int ni : p.mods[id].incident) {
                const auto &n = p.nets[ni];
                int other = n.a == id ? n.b : n.a;
                sx += cur[other].first * n.wt;
                sy += cur[other].second * n.wt;
                sw += n.wt;
            }
            nxt[id] = {sx / sw, sy / sw};
        }
        cur.swap(nxt);
    }
    vector<pair<int, int>> ans(p.mods.size());
    for (size_t i = 0; i < cur.size(); ++i) {
        ans[i] = {clampInt(static_cast<int>(llround(cur[i].first)), 0, p.W),
                  clampInt(static_cast<int>(llround(cur[i].second)), 0, p.H)};
    }
    return ans;
}

static bool construct(const Problem &p, vector<Rect> &rects, int seed, Saved &best) {
    mt19937 rng(seed);
    rects.assign(p.mods.size(), Rect{});
    for (int id : p.fixedIds) rects[id] = p.mods[id].r;
    auto target = relaxedTargets(p);
    vector<int> order = p.softIds;
    int mode = seed % 8;
    if (mode == 0) {
        sort(order.begin(), order.end(), [&](int a, int b) { return p.mods[a].area > p.mods[b].area; });
    } else if (mode == 1) {
        sort(order.begin(), order.end(), [&](int a, int b) {
            long long wa = 0, wb = 0;
            for (int ni : p.mods[a].incident) wa += p.nets[ni].wt;
            for (int ni : p.mods[b].incident) wb += p.nets[ni].wt;
            if (wa != wb) return wa > wb;
            return p.mods[a].area > p.mods[b].area;
        });
    } else if (mode == 2) {
        stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return target[a].first + target[a].second < target[b].first + target[b].second;
        });
    } else if (mode == 3) {
        stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return target[a].first - target[a].second < target[b].first - target[b].second;
        });
    } else {
        sort(order.begin(), order.end(), [&](int a, int b) { return p.mods[a].area > p.mods[b].area; });
        shuffle(order.begin() + min<int>(3, order.size()), order.end(), rng);
    }

    vector<Rect> freeRects = buildFreeRects(p, rects);
    for (int id : order) {
        pair<int, int> t = target[id];
        for (int ni : p.mods[id].incident) {
            int other = p.nets[ni].a == id ? p.nets[ni].b : p.nets[ni].a;
            if (rects[other].w > 0) {
                auto c = centerOf(rects[other]);
                t.first = (t.first + c.first) / 2;
                t.second = (t.second + c.second) / 2;
            }
        }
        Rect cand = bestPlacementInFree(p, rects, freeRects, id, t, rng, true);
        if (cand.w <= 0) return false;
        rects[id] = cand;
        subtractRect(freeRects, cand);
    }
    if (!allLegal(p, rects)) return false;
    long long wl = totalWL(p, rects);
    if (!best.ok || wl < best.wl) best = {rects, wl, true};
    return true;
}

static bool improveOne(const Problem &p, vector<Rect> &rects, int id, mt19937 &rng, Saved &best, bool broad) {
    long long oldCost = incidentCost(p, rects, id);
    auto freeRects = buildFreeRects(p, rects, id);
    auto target = medianTarget(p, rects, id);
    Rect cand = bestPlacementInFree(p, rects, freeRects, id, target, rng, broad);
    if (cand.w <= 0) return false;
    long long newCost = incidentCost(p, rects, id, &cand);
    if (newCost < oldCost || (newCost == oldCost && (cand.x != rects[id].x || cand.y != rects[id].y))) {
        Rect old = rects[id];
        rects[id] = cand;
        if (!legalOne(p, rects, id)) {
            rects[id] = old;
            return false;
        }
        long long wl = totalWL(p, rects);
        if (!best.ok || wl < best.wl) best = {rects, wl, true};
        return wl <= best.wl || newCost < oldCost;
    }
    return false;
}

static void descent(const Problem &p, vector<Rect> &rects, int seed, Saved &best) {
    mt19937 rng(seed * 2654435761U + 17U);
    vector<int> order = p.softIds;
    for (int sweep = 0; sweep < 120 && !timedOut(); ++sweep) {
        bool changed = false;
        if (sweep % 3 == 2) shuffle(order.begin(), order.end(), rng);
        for (int id : order) {
            if (timedOut()) break;
            changed = improveOne(p, rects, id, rng, best, sweep < 20 || sweep % 10 == 0) || changed;
        }
        long long cur = totalWL(p, rects);
        if (cur < best.wl) best = {rects, cur, true};
        if (!changed && sweep > 8) break;
    }
}

static bool pairRepackPass(const Problem &p, vector<Rect> &rects, int seed, Saved &best) {
    mt19937 rng(seed * 2246822519U + 41U);
    vector<pair<int, int>> pairs;
    for (size_t i = 0; i < p.softIds.size(); ++i) {
        for (size_t j = i + 1; j < p.softIds.size(); ++j) pairs.push_back({p.softIds[i], p.softIds[j]});
    }
    shuffle(pairs.begin(), pairs.end(), rng);
    long long curWL = totalWL(p, rects);
    bool improved = false;
    int checked = 0;
    for (auto [a, b] : pairs) {
        if (timedOut()) break;
        if (++checked > 80 && improved) break;
        vector<Rect> base = rects;
        base[a] = Rect{};
        base[b] = Rect{};
        vector<Rect> freeRects = buildFreeRects(p, base);
        Rect bestA, bestB;
        bool found = false;
        long long bestWL = curWL;
        int firsts[2] = {a, b};
        for (int ord = 0; ord < 2; ++ord) {
            int first = firsts[ord];
            int second = first == a ? b : a;
            auto c1 = topPlacementsInFree(p, base, freeRects, first, medianTarget(p, rects, first), rng, 18);
            for (const Rect &r1 : c1) {
                vector<Rect> mid = base;
                mid[first] = r1;
                auto free2 = freeRects;
                subtractRect(free2, r1);
                auto c2 = topPlacementsInFree(p, mid, free2, second, medianTarget(p, rects, second), rng, 18);
                for (const Rect &r2 : c2) {
                    vector<Rect> cand = mid;
                    cand[second] = r2;
                    if (!legalOne(p, cand, first) || !legalOne(p, cand, second)) continue;
                    long long w = totalWL(p, cand);
                    if (w < bestWL) {
                        bestWL = w;
                        if (first == a) {
                            bestA = r1;
                            bestB = r2;
                        } else {
                            bestA = r2;
                            bestB = r1;
                        }
                        found = true;
                    }
                }
            }
        }
        if (found) {
            rects[a] = bestA;
            rects[b] = bestB;
            curWL = bestWL;
            improved = true;
            if (!best.ok || curWL < best.wl) best = {rects, curWL, true};
        }
    }
    return improved;
}

static void anneal(const Problem &p, vector<Rect> &rects, int seed, Saved &best) {
    mt19937 rng(seed * 1103515245U + 12345U);
    uniform_real_distribution<double> uni(0.0, 1.0);
    vector<int> ids = p.softIds;
    long long curWL = totalWL(p, rects);
    if (curWL < best.wl) best = {rects, curWL, true};
    double temp = max(1000.0, curWL / max(1.0, static_cast<double>(p.nets.size())) * 0.08);
    int iters = 18000 + 1000 * static_cast<int>(p.softIds.size());
    for (int it = 0; it < iters && !timedOut(); ++it) {
        int id = ids[rng() % ids.size()];
        auto freeRects = buildFreeRects(p, rects, id);
        pair<int, int> target;
        if ((rng() % 100) < 70) target = medianTarget(p, rects, id);
        else target = {static_cast<int>(rng() % max(1, p.W)), static_cast<int>(rng() % max(1, p.H))};
        Rect cand = bestPlacementInFree(p, rects, freeRects, id, target, rng, (rng() % 5) == 0);
        if (cand.w <= 0) continue;
        long long oldLocal = incidentCost(p, rects, id);
        long long newLocal = incidentCost(p, rects, id, &cand);
        long long delta = newLocal - oldLocal;
        if (delta <= 0 || uni(rng) < exp(-static_cast<double>(delta) / max(1.0, temp))) {
            Rect old = rects[id];
            rects[id] = cand;
            if (!legalOne(p, rects, id)) {
                rects[id] = old;
                continue;
            }
            curWL += delta;
            if (curWL < best.wl) best = {rects, curWL, true};
        }
        temp *= 0.99945;
        if (it % 3000 == 2999) {
            descent(p, rects, seed + it, best);
            curWL = totalWL(p, rects);
        }
    }
}

static Saved solve(const Problem &p) {
    Saved global;
    int starts = max(256, min(1600, 160 + static_cast<int>(p.softIds.size()) * 40));
    vector<Saved> byStart(starts);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
    for (int s = 0; s < starts; ++s) {
        if (timedOut()) continue;
        vector<Rect> rects;
        Saved chainBest;
        if (construct(p, rects, s * 7919, chainBest)) {
            descent(p, rects, s * 977, chainBest);
            if (p.softIds.size() <= 20) pairRepackPass(p, rects, s * 1741, chainBest);
            anneal(p, rects, s * 3571, chainBest);
            descent(p, chainBest.rects, s * 1237, chainBest);
            if (p.softIds.size() <= 20) pairRepackPass(p, chainBest.rects, s * 2371, chainBest);
            byStart[s] = chainBest;
        }
    }
#else
    for (int s = 0; s < starts; ++s) {
        if (timedOut()) break;
        vector<Rect> rects;
        Saved chainBest;
        if (construct(p, rects, s * 7919, chainBest)) {
            descent(p, rects, s * 977, chainBest);
            if (p.softIds.size() <= 20) pairRepackPass(p, rects, s * 1741, chainBest);
            anneal(p, rects, s * 3571, chainBest);
            descent(p, chainBest.rects, s * 1237, chainBest);
            if (p.softIds.size() <= 20) pairRepackPass(p, chainBest.rects, s * 2371, chainBest);
            byStart[s] = chainBest;
        }
    }
#endif
    for (int s = 0; s < starts; ++s) {
        if (byStart[s].ok && (!global.ok || byStart[s].wl < global.wl)) global = byStart[s];
    }
    return global;
}

static void writeOutput(const Problem &p, const vector<Rect> &rects, const string &path) {
    filesystem::path outPath(path);
    if (outPath.has_parent_path()) filesystem::create_directories(outPath.parent_path());
    ofstream out(path);
    long long wl = totalWL(p, rects);
    out << "Wirelength " << wl << "\n\n";
    out << "NumSoftModules " << p.softIds.size() << "\n";
    for (int id : p.softIds) {
        const Rect &r = rects[id];
        out << p.mods[id].name << ' ' << r.x << ' ' << r.y << ' ' << r.w << ' ' << r.h << "\n";
    }
}

int main(int argc, char **argv) {
    gStart = chrono::steady_clock::now();
    try {
        if (argc != 3) return 2;
        Problem p = parseInput(argv[1]);
        Saved best = solve(p);
        if (!best.ok || !allLegal(p, best.rects)) return 3;
        writeOutput(p, best.rects, argv[2]);
        return 0;
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return 1;
    }
}
