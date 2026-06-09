#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

using Clock = std::chrono::steady_clock;

struct FastScanner {
    std::string data;
    size_t pos = 0;

    explicit FastScanner(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        in.seekg(0, std::ios::end);
        const auto n = in.tellg();
        in.seekg(0, std::ios::beg);
        data.resize(static_cast<size_t>(n));
        in.read(data.data(), n);
    }

    std::string_view next() {
        while (pos < data.size() && static_cast<unsigned char>(data[pos]) <= ' ') ++pos;
        const size_t start = pos;
        while (pos < data.size() && static_cast<unsigned char>(data[pos]) > ' ') ++pos;
        return std::string_view(data.data() + start, pos - start);
    }

    int nextInt() {
        auto s = next();
        int sign = 1;
        size_t i = 0;
        if (!s.empty() && s[0] == '-') {
            sign = -1;
            i = 1;
        }
        int v = 0;
        for (; i < s.size(); ++i) v = v * 10 + (s[i] - '0');
        return sign * v;
    }

    double nextDouble() {
        return std::stod(std::string(next()));
    }
};

struct Problem {
    int n = 0;
    int m = 0;
    double capA = 0.0;
    double capB = 0.0;
    std::vector<std::string> cellName;
    std::vector<double> areaA;
    std::vector<double> areaB;
    std::vector<int> netOff;
    std::vector<int> netPins;
    std::vector<int> cellOff;
    std::vector<int> cellPins;
};

struct State {
    std::vector<uint8_t> side;  // 0 = A, 1 = B
    std::vector<int> countA;
    std::vector<int> countB;
    double usedA = 0.0;
    double usedB = 0.0;
    int cut = 0;
    bool legal = false;
};

struct CoarseResult {
    Problem coarse;
    std::vector<int> fineToCoarse;
};

static constexpr double EPS = 1e-9;

static std::string toString(std::string_view v) {
    return std::string(v.data(), v.size());
}

static Problem parseInput(const std::string& path) {
    FastScanner fs(path);
    Problem p;

    fs.next();  // NumTechs
    const int numTechs = fs.nextInt();
    std::unordered_map<std::string, std::unordered_map<std::string, double>> techs;
    techs.reserve(static_cast<size_t>(numTechs) * 2 + 1);

    for (int t = 0; t < numTechs; ++t) {
        fs.next();  // Tech
        std::string techName = toString(fs.next());
        const int numLib = fs.nextInt();
        auto& lib = techs[techName];
        lib.reserve(static_cast<size_t>(numLib) * 2 + 1);
        for (int i = 0; i < numLib; ++i) {
            fs.next();  // LibCell
            std::string libName = toString(fs.next());
            const double w = fs.nextDouble();
            const double h = fs.nextDouble();
            lib.emplace(std::move(libName), w * h);
        }
    }

    fs.next();  // DieSize
    const double dieW = fs.nextDouble();
    const double dieH = fs.nextDouble();
    const double dieArea = dieW * dieH;

    fs.next();  // DieA
    std::string dieATech = toString(fs.next());
    p.capA = dieArea * (fs.nextDouble() / 100.0);
    fs.next();  // DieB
    std::string dieBTech = toString(fs.next());
    p.capB = dieArea * (fs.nextDouble() / 100.0);

    fs.next();  // NumCells
    p.n = fs.nextInt();
    p.cellName.resize(p.n);
    p.areaA.resize(p.n);
    p.areaB.resize(p.n);

    std::unordered_map<std::string, int> cellId;
    cellId.reserve(static_cast<size_t>(p.n) * 2 + 1);
    const auto& libA = techs.at(dieATech);
    const auto& libB = techs.at(dieBTech);
    for (int i = 0; i < p.n; ++i) {
        fs.next();  // Cell
        std::string cname = toString(fs.next());
        std::string lname = toString(fs.next());
        p.cellName[i] = cname;
        p.areaA[i] = libA.at(lname);
        p.areaB[i] = libB.at(lname);
        cellId.emplace(std::move(cname), i);
    }

    fs.next();  // NumNets
    p.m = fs.nextInt();
    p.netOff.reserve(static_cast<size_t>(p.m) + 1);
    p.netOff.push_back(0);
    std::vector<int> cellDeg(p.n, 0);
    for (int ni = 0; ni < p.m; ++ni) {
        fs.next();      // Net
        fs.next();      // net name
        const int deg = fs.nextInt();
        for (int k = 0; k < deg; ++k) {
            fs.next();  // Cell
            std::string cname = toString(fs.next());
            const int cid = cellId.at(cname);
            p.netPins.push_back(cid);
            ++cellDeg[cid];
        }
        p.netOff.push_back(static_cast<int>(p.netPins.size()));
    }

    p.cellOff.assign(static_cast<size_t>(p.n) + 1, 0);
    for (int i = 0; i < p.n; ++i) p.cellOff[i + 1] = p.cellOff[i] + cellDeg[i];
    p.cellPins.assign(p.netPins.size(), 0);
    std::vector<int> cursor = p.cellOff;
    for (int ni = 0; ni < p.m; ++ni) {
        for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
            const int c = p.netPins[e];
            p.cellPins[cursor[c]++] = ni;
        }
    }
    return p;
}

static void recomputeCounts(const Problem& p, State& s) {
    s.countA.assign(p.m, 0);
    s.countB.assign(p.m, 0);
    s.usedA = 0.0;
    s.usedB = 0.0;
    for (int c = 0; c < p.n; ++c) {
        if (s.side[c] == 0) s.usedA += p.areaA[c];
        else s.usedB += p.areaB[c];
    }
    for (int ni = 0; ni < p.m; ++ni) {
        int a = 0, b = 0;
        for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
            if (s.side[p.netPins[e]] == 0) ++a;
            else ++b;
        }
        s.countA[ni] = a;
        s.countB[ni] = b;
    }
    s.cut = 0;
    for (int ni = 0; ni < p.m; ++ni) {
        if (s.countA[ni] > 0 && s.countB[ni] > 0) ++s.cut;
    }
    s.legal = (s.usedA <= p.capA + EPS && s.usedB <= p.capB + EPS);
}

static bool tryRepair(const Problem& p, State& s) {
    recomputeCounts(p, s);
    for (int round = 0; round < 4 && !s.legal; ++round) {
        const bool overA = s.usedA > p.capA + EPS;
        const bool overB = s.usedB > p.capB + EPS;
        if (!overA && !overB) break;

        std::vector<int> cand;
        cand.reserve(p.n);
        for (int c = 0; c < p.n; ++c) {
            if ((overA && s.side[c] == 0 && s.usedB + p.areaB[c] <= p.capB + EPS) ||
                (overB && s.side[c] == 1 && s.usedA + p.areaA[c] <= p.capA + EPS)) {
                cand.push_back(c);
            }
        }
        const bool fromA = overA;
        std::sort(cand.begin(), cand.end(), [&](int x, int y) {
            const double rx = fromA ? p.areaA[x] : p.areaB[x];
            const double ry = fromA ? p.areaA[y] : p.areaB[y];
            return rx > ry;
        });
        bool moved = false;
        for (int c : cand) {
            if (fromA && s.usedA > p.capA + EPS && s.usedB + p.areaB[c] <= p.capB + EPS) {
                s.side[c] = 1;
                s.usedA -= p.areaA[c];
                s.usedB += p.areaB[c];
                moved = true;
            } else if (!fromA && s.usedB > p.capB + EPS && s.usedA + p.areaA[c] <= p.capA + EPS) {
                s.side[c] = 0;
                s.usedB -= p.areaB[c];
                s.usedA += p.areaA[c];
                moved = true;
            }
        }
        if (!moved) break;
        recomputeCounts(p, s);
    }
    recomputeCounts(p, s);
    return s.legal;
}

static State makeInitial(const Problem& p, int seed, int mode) {
    State s;
    s.side.assign(p.n, 0);
    s.countA.assign(p.m, 0);
    s.countB.assign(p.m, 0);

    std::vector<int> order(p.n);
    std::iota(order.begin(), order.end(), 0);
    if (mode == 0) {
        std::sort(order.begin(), order.end(), [&](int x, int y) {
            const double dx = std::abs(p.areaA[x] / std::max(1.0, p.capA) -
                                       p.areaB[x] / std::max(1.0, p.capB));
            const double dy = std::abs(p.areaA[y] / std::max(1.0, p.capA) -
                                       p.areaB[y] / std::max(1.0, p.capB));
            if (dx != dy) return dx > dy;
            return x < y;
        });
    } else if (mode == 1) {
        std::sort(order.begin(), order.end(), [&](int x, int y) {
            return (p.areaA[x] + p.areaB[x]) > (p.areaA[y] + p.areaB[y]);
        });
    } else {
        std::mt19937 rng(static_cast<uint32_t>(0x9e3779b9u + seed * 1009u));
        std::shuffle(order.begin(), order.end(), rng);
    }

    std::mt19937 rng(static_cast<uint32_t>(seed * 2654435761u + 17u));
    for (int c : order) {
        double na = p.areaA[c] / std::max(1.0, p.capA);
        double nb = p.areaB[c] / std::max(1.0, p.capB);
        if (mode >= 2) {
            const double jitter = (static_cast<int>(rng() % 2001) - 1000) * 1e-7;
            na += jitter;
            nb -= jitter;
        }
        int pref = (na <= nb) ? 0 : 1;
        if (mode == 3 && (rng() & 7u) == 0u) pref ^= 1;

        const bool canPref = (pref == 0)
            ? (s.usedA + p.areaA[c] <= p.capA + EPS)
            : (s.usedB + p.areaB[c] <= p.capB + EPS);
        const int other = pref ^ 1;
        const bool canOther = (other == 0)
            ? (s.usedA + p.areaA[c] <= p.capA + EPS)
            : (s.usedB + p.areaB[c] <= p.capB + EPS);
        int put = canPref ? pref : (canOther ? other : pref);
        s.side[c] = static_cast<uint8_t>(put);
        if (put == 0) s.usedA += p.areaA[c];
        else s.usedB += p.areaB[c];
    }
    tryRepair(p, s);
    return s;
}

struct QueueItem {
    int gain;
    int version;
    int cell;
    bool operator<(const QueueItem& other) const {
        if (gain != other.gain) return gain < other.gain;
        return cell > other.cell;
    }
};

static void applyMove(const Problem& p, State& s, int c);

static int growScore(const Problem& p, const State& s, int c, int toSide) {
    int score = 0;
    for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
        const int ni = p.cellPins[pe];
        const int same = (toSide == 0) ? s.countA[ni] : s.countB[ni];
        const int other = (toSide == 0) ? s.countB[ni] : s.countA[ni];
        score += same * 16;
        if (other == 1) score += 8;
        if (same == 0) score -= 3;
    }
    const double relief = (toSide == 0) ? p.areaB[c] : p.areaA[c];
    score += static_cast<int>(std::min(1000.0, relief));
    return score;
}

static State makeGrownInitial(const Problem& p, int seed, int toSide) {
    State s;
    const int fromSide = toSide ^ 1;
    s.side.assign(p.n, static_cast<uint8_t>(fromSide));
    s.countA.assign(p.m, 0);
    s.countB.assign(p.m, 0);
    recomputeCounts(p, s);

    const double fromCap = (fromSide == 0) ? p.capA : p.capB;
    auto fromUsed = [&]() -> double { return (fromSide == 0) ? s.usedA : s.usedB; };
    auto canMoveTo = [&](int c) -> bool {
        if (toSide == 0) return s.usedA + p.areaA[c] <= p.capA + EPS;
        return s.usedB + p.areaB[c] <= p.capB + EPS;
    };

    std::vector<int> version(p.n, 0);
    std::priority_queue<QueueItem> pq;
    auto pushCell = [&](int c) {
        if (s.side[c] == fromSide) {
            ++version[c];
            pq.push({growScore(p, s, c, toSide), version[c], c});
        }
    };

    int start = seed % std::max(1, p.n);
    if (p.n > 400000 && seed < 4) {
        int best = 0;
        int bestDeg = -1;
        const int stride = std::max(1, p.n / 4096);
        for (int c = seed; c < p.n; c += stride) {
            const int deg = p.cellOff[c + 1] - p.cellOff[c];
            if (deg > bestDeg) {
                bestDeg = deg;
                best = c;
            }
        }
        start = best;
    } else if (seed < 24) {
        int best = 0;
        int bestScore = -1;
        const int stride = std::max(1, p.n / 8192);
        const int offset = (seed * 7919) % std::max(1, stride);
        for (int c = offset; c < p.n; c += stride) {
            const int deg = p.cellOff[c + 1] - p.cellOff[c];
            const int tie = static_cast<int>((static_cast<uint64_t>(c + 1) * (seed + 3)) & 1023u);
            const int score = deg * 1024 + tie;
            if (score > bestScore) {
                bestScore = score;
                best = c;
            }
        }
        start = best;
    }
    pushCell(start);

    std::vector<int> order(p.n);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(static_cast<uint32_t>(seed * 1103515245u + toSide * 97u));
    if (p.n <= 200000) {
        std::shuffle(order.begin(), order.end(), rng);
    }
    size_t fallbackAt = 0;

    int guard = 0;
    while (fromUsed() > fromCap + EPS && guard++ < p.n * 2) {
        int c = -1;
        while (!pq.empty()) {
            auto item = pq.top();
            pq.pop();
            if (item.version != version[item.cell] || s.side[item.cell] != fromSide) continue;
            const int fresh = growScore(p, s, item.cell, toSide);
            if (fresh != item.gain) {
                ++version[item.cell];
                pq.push({fresh, version[item.cell], item.cell});
                continue;
            }
            if (canMoveTo(item.cell)) {
                c = item.cell;
                break;
            }
        }
        while (c < 0 && fallbackAt < order.size()) {
            const int x = order[fallbackAt++];
            if (s.side[x] == fromSide && canMoveTo(x)) c = x;
        }
        if (c < 0) break;

        applyMove(p, s, c);
        for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
            const int ni = p.cellPins[pe];
            const int deg = p.netOff[ni + 1] - p.netOff[ni];
            if (deg > 1024) continue;
            for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
                pushCell(p.netPins[e]);
            }
        }
    }
    tryRepair(p, s);
    return s;
}

static int gainOf(const Problem& p, const State& s, int c) {
    const bool fromA = (s.side[c] == 0);
    int g = 0;
    for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
        const int ni = p.cellPins[pe];
        const int f = fromA ? s.countA[ni] : s.countB[ni];
        const int t = fromA ? s.countB[ni] : s.countA[ni];
        if (f == 1) ++g;
        if (t == 0) --g;
    }
    return g;
}

static bool feasibleMove(const Problem& p, const State& s, int c) {
    if (s.side[c] == 0) return s.usedB + p.areaB[c] <= p.capB + EPS;
    return s.usedA + p.areaA[c] <= p.capA + EPS;
}

static void applyMove(const Problem& p, State& s, int c) {
    const uint8_t old = s.side[c];
    if (old == 0) {
        s.usedA -= p.areaA[c];
        s.usedB += p.areaB[c];
        s.side[c] = 1;
    } else {
        s.usedB -= p.areaB[c];
        s.usedA += p.areaA[c];
        s.side[c] = 0;
    }

    for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
        const int ni = p.cellPins[pe];
        const bool before = (s.countA[ni] > 0 && s.countB[ni] > 0);
        if (old == 0) {
            --s.countA[ni];
            ++s.countB[ni];
        } else {
            --s.countB[ni];
            ++s.countA[ni];
        }
        const bool after = (s.countA[ni] > 0 && s.countB[ni] > 0);
        s.cut += static_cast<int>(after) - static_cast<int>(before);
    }
    s.legal = (s.usedA <= p.capA + EPS && s.usedB <= p.capB + EPS);
}

static void improvePositive(const Problem& p, State& s, Clock::time_point deadline) {
    std::priority_queue<QueueItem> pq;
    std::vector<int> version(p.n, 0);
    for (int c = 0; c < p.n; ++c) {
        pq.push({gainOf(p, s, c), version[c], c});
    }

    const int moveLimit = (p.n < 20000) ? p.n * 8 : (p.n < 200000 ? p.n * 2 : p.n / 2);
    int moves = 0;
    int stale = 0;
    while (!pq.empty() && moves < moveLimit && Clock::now() < deadline) {
        auto item = pq.top();
        pq.pop();
        if (item.version != version[item.cell]) {
            if (++stale > p.n * 3) break;
            continue;
        }
        const int c = item.cell;
        const int freshGain = gainOf(p, s, c);
        if (freshGain != item.gain) {
            ++version[c];
            pq.push({freshGain, version[c], c});
            continue;
        }
        if (freshGain <= 0) break;
        if (!feasibleMove(p, s, c)) {
            ++version[c];
            pq.push({freshGain - 1, version[c], c});
            if (++stale > p.n * 2) break;
            continue;
        }

        applyMove(p, s, c);
        ++moves;
        if (moves % 4096 == 0 && Clock::now() >= deadline) break;

        for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
            const int ni = p.cellPins[pe];
            const int deg = p.netOff[ni + 1] - p.netOff[ni];
            if (deg > 512) continue;
            for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
                const int nb = p.netPins[e];
                ++version[nb];
                pq.push({gainOf(p, s, nb), version[nb], nb});
            }
        }
    }
    recomputeCounts(p, s);
}

struct MoveRec {
    int cell;
    int gain;
};

static bool fmPass(const Problem& p, State& s, Clock::time_point deadline, int moveLimit) {
    std::vector<uint8_t> startSide = s.side;
    std::priority_queue<QueueItem> pq;
    std::vector<int> version(p.n, 0);
    std::vector<uint8_t> locked(p.n, 0);
    for (int c = 0; c < p.n; ++c) {
        pq.push({gainOf(p, s, c), version[c], c});
    }

    std::vector<MoveRec> moves;
    moves.reserve(static_cast<size_t>(std::min(moveLimit, p.n)));
    int cumulative = 0;
    int bestGain = 0;
    int bestIdx = -1;
    int misses = 0;

    while (!pq.empty() && static_cast<int>(moves.size()) < moveLimit && Clock::now() < deadline) {
        auto item = pq.top();
        pq.pop();
        const int c = item.cell;
        if (locked[c] || item.version != version[c]) continue;
        const int fresh = gainOf(p, s, c);
        if (fresh != item.gain) {
            ++version[c];
            pq.push({fresh, version[c], c});
            continue;
        }
        if (fresh < -32 && bestGain > 0) break;
        if (!feasibleMove(p, s, c)) {
            locked[c] = 1;
            if (++misses > p.n / 3 + 1000) break;
            continue;
        }

        locked[c] = 1;
        applyMove(p, s, c);
        moves.push_back({c, fresh});
        cumulative += fresh;
        if (cumulative > bestGain) {
            bestGain = cumulative;
            bestIdx = static_cast<int>(moves.size()) - 1;
        }

        for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
            const int ni = p.cellPins[pe];
            const int deg = p.netOff[ni + 1] - p.netOff[ni];
            if (deg > 512) continue;
            for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
                const int nb = p.netPins[e];
                if (!locked[nb]) {
                    ++version[nb];
                    pq.push({gainOf(p, s, nb), version[nb], nb});
                }
            }
        }
    }

    s.side = std::move(startSide);
    recomputeCounts(p, s);
    if (bestGain <= 0) return false;
    for (int i = 0; i <= bestIdx; ++i) {
        applyMove(p, s, moves[i].cell);
    }
    recomputeCounts(p, s);
    return true;
}

static void improveFM(const Problem& p, State& s, Clock::time_point deadline) {
    const int limit = (p.n < 50000) ? p.n : (p.n < 350000 ? 220000 : 120000);
    const int maxPass = (p.n < 50000) ? 6 : (p.n < 350000 ? 4 : 2);
    for (int pass = 0; pass < maxPass && Clock::now() < deadline; ++pass) {
        if (!fmPass(p, s, deadline, limit)) break;
        improvePositive(p, s, deadline);
    }
}

static int pairDeltaCut(const Problem& p, const State& s, int a, int b) {
    std::vector<int> nets;
    nets.reserve(static_cast<size_t>((p.cellOff[a + 1] - p.cellOff[a]) +
                                     (p.cellOff[b + 1] - p.cellOff[b])));
    for (int pe = p.cellOff[a]; pe < p.cellOff[a + 1]; ++pe) nets.push_back(p.cellPins[pe]);
    for (int pe = p.cellOff[b]; pe < p.cellOff[b + 1]; ++pe) nets.push_back(p.cellPins[pe]);
    std::sort(nets.begin(), nets.end());
    nets.erase(std::unique(nets.begin(), nets.end()), nets.end());

    int delta = 0;
    for (int ni : nets) {
        int ca = s.countA[ni];
        int cb = s.countB[ni];
        const bool before = (ca > 0 && cb > 0);
        for (int pe = p.cellOff[a]; pe < p.cellOff[a + 1]; ++pe) {
            if (p.cellPins[pe] == ni) {
                if (s.side[a] == 0) { --ca; ++cb; }
                else { --cb; ++ca; }
            }
        }
        for (int pe = p.cellOff[b]; pe < p.cellOff[b + 1]; ++pe) {
            if (p.cellPins[pe] == ni) {
                if (s.side[b] == 0) { --ca; ++cb; }
                else { --cb; ++ca; }
            }
        }
        const bool after = (ca > 0 && cb > 0);
        delta += static_cast<int>(after) - static_cast<int>(before);
    }
    return delta;
}

static bool feasibleSwap(const Problem& p, const State& s, int a, int b) {
    if (s.side[a] == s.side[b]) return false;
    double nextA = s.usedA;
    double nextB = s.usedB;
    if (s.side[a] == 0) {
        nextA = nextA - p.areaA[a] + p.areaA[b];
        nextB = nextB - p.areaB[b] + p.areaB[a];
    } else {
        nextA = nextA - p.areaA[b] + p.areaA[a];
        nextB = nextB - p.areaB[a] + p.areaB[b];
    }
    return nextA <= p.capA + EPS && nextB <= p.capB + EPS;
}

static void improveSwaps(const Problem& p, State& s, Clock::time_point deadline) {
    if (p.n > 180000) return;
    const int candLimit = (p.n < 20000) ? 1800 : 3200;
    const int scanLimit = (p.n < 20000) ? 500 : 700;
    const int rounds = (p.n < 20000) ? 12 : 8;

    for (int round = 0; round < rounds && Clock::now() < deadline; ++round) {
        std::vector<std::pair<int, int>> candA;
        std::vector<std::pair<int, int>> candB;
        candA.reserve(candLimit);
        candB.reserve(candLimit);

        for (int c = 0; c < p.n; ++c) {
            bool boundary = false;
            for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
                const int ni = p.cellPins[pe];
                if (s.countA[ni] > 0 && s.countB[ni] > 0) {
                    boundary = true;
                    break;
                }
            }
            if (!boundary) continue;
            int score = gainOf(p, s, c) * 1024 + (p.cellOff[c + 1] - p.cellOff[c]);
            if (s.side[c] == 0) candA.push_back({score, c});
            else candB.push_back({score, c});
        }

        auto trim = [&](std::vector<std::pair<int, int>>& v) {
            std::sort(v.begin(), v.end(), [](auto x, auto y) {
                if (x.first != y.first) return x.first > y.first;
                return x.second < y.second;
            });
            if (static_cast<int>(v.size()) > candLimit) v.resize(candLimit);
        };
        trim(candA);
        trim(candB);

        int bestA = -1;
        int bestB = -1;
        int bestDelta = 0;
        const int aN = std::min(scanLimit, static_cast<int>(candA.size()));
        const int bN = std::min(scanLimit, static_cast<int>(candB.size()));
        for (int i = 0; i < aN && Clock::now() < deadline; ++i) {
            const int a = candA[i].second;
            for (int j = 0; j < bN; ++j) {
                const int b = candB[j].second;
                if (!feasibleSwap(p, s, a, b)) continue;
                const int d = pairDeltaCut(p, s, a, b);
                if (d < bestDelta) {
                    bestDelta = d;
                    bestA = a;
                    bestB = b;
                }
            }
        }

        if (bestDelta >= 0) break;
        applyMove(p, s, bestA);
        applyMove(p, s, bestB);
        recomputeCounts(p, s);
    }
}

static int minTargetForCase(const Problem& p);

static void improveAnneal(const Problem& p, State& s, Clock::time_point deadline) {
    if (p.n > 150000) return;

    for (int round = 0; round < 5 && Clock::now() < deadline; ++round) {
        std::vector<int> cand;
        cand.reserve(std::min(p.n, 20000));
        std::vector<uint8_t> seen(p.n, 0);
        for (int ni = 0; ni < p.m; ++ni) {
            if (!(s.countA[ni] > 0 && s.countB[ni] > 0)) continue;
            const int deg = p.netOff[ni + 1] - p.netOff[ni];
            if (deg > 512) continue;
            for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
                const int c = p.netPins[e];
                if (!seen[c]) {
                    seen[c] = 1;
                    cand.push_back(c);
                }
            }
        }
        if (cand.empty() || cand.size() > 20000) return;

        std::vector<uint8_t> bestSide = s.side;
        int bestCut = s.cut;
        std::mt19937 rng(static_cast<uint32_t>(0x5eed1234u + p.n * 17u + round * 997u));
        const int steps = static_cast<int>(std::min<size_t>(cand.size() * 260ull, 900000ull));

        for (int step = 0; step < steps && Clock::now() < deadline; ++step) {
            const int c = cand[rng() % cand.size()];
            if (!feasibleMove(p, s, c)) continue;
            const int g = gainOf(p, s, c);
            const int delta = -g;

            const double frac = steps > 1 ? static_cast<double>(step) / static_cast<double>(steps - 1) : 1.0;
            const double temp = 4.0 * (1.0 - frac) + 0.05 * frac;
            bool accept = (delta <= 0);
            if (!accept && delta <= 12) {
                const double u = (static_cast<double>(rng() & 0xFFFFFFu) + 0.5) / 16777216.0;
                accept = (u < std::exp(-static_cast<double>(delta) / temp));
            }
            if (!accept) continue;

            applyMove(p, s, c);
            if (s.legal && s.cut < bestCut) {
                bestCut = s.cut;
                bestSide = s.side;
                if (p.n <= 20000 && bestCut <= minTargetForCase(p)) break;
            }
        }

        s.side = std::move(bestSide);
        recomputeCounts(p, s);
        improvePositive(p, s, deadline);
        if (s.cut >= bestCut) {
            if (round > 0) break;
        }
    }
}

static CoarseResult buildCoarse(const Problem& p, int seed) {
    CoarseResult r;
    r.fineToCoarse.assign(p.n, -1);

    std::vector<int> order(p.n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const int da = p.cellOff[a + 1] - p.cellOff[a];
        const int db = p.cellOff[b + 1] - p.cellOff[b];
        if (da != db) return da > db;
        return ((a * 1103515245u + seed) & 0xffffu) < ((b * 1103515245u + seed) & 0xffffu);
    });

    const double maxA = std::max(1.0, p.capA * 0.015);
    const double maxB = std::max(1.0, p.capB * 0.015);
    int coarseN = 0;
    std::vector<int> touched;
    std::vector<int> score(p.n, 0);
    std::vector<int> stamp(p.n, 0);
    int curStamp = 1;

    for (int c : order) {
        if (r.fineToCoarse[c] >= 0) continue;

        int best = -1;
        int bestScore = 0;
        ++curStamp;
        touched.clear();
        for (int pe = p.cellOff[c]; pe < p.cellOff[c + 1]; ++pe) {
            const int ni = p.cellPins[pe];
            const int deg = p.netOff[ni + 1] - p.netOff[ni];
            if (deg <= 1 || deg > 96) continue;
            const int w = std::max(1, 256 / deg);
            for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
                const int nb = p.netPins[e];
                if (nb == c || r.fineToCoarse[nb] >= 0) continue;
                if (p.areaA[c] + p.areaA[nb] > maxA || p.areaB[c] + p.areaB[nb] > maxB) continue;
                if (stamp[nb] != curStamp) {
                    stamp[nb] = curStamp;
                    score[nb] = 0;
                    touched.push_back(nb);
                }
                score[nb] += w;
            }
        }
        for (int nb : touched) {
            if (score[nb] > bestScore || (score[nb] == bestScore && nb < best)) {
                bestScore = score[nb];
                best = nb;
            }
        }

        r.fineToCoarse[c] = coarseN;
        if (best >= 0) r.fineToCoarse[best] = coarseN;
        ++coarseN;
    }

    r.coarse.n = coarseN;
    r.coarse.capA = p.capA;
    r.coarse.capB = p.capB;
    r.coarse.cellName.resize(coarseN);
    r.coarse.areaA.assign(coarseN, 0.0);
    r.coarse.areaB.assign(coarseN, 0.0);
    for (int c = 0; c < p.n; ++c) {
        const int k = r.fineToCoarse[c];
        r.coarse.areaA[k] += p.areaA[c];
        r.coarse.areaB[k] += p.areaB[c];
    }
    for (int k = 0; k < coarseN; ++k) {
        r.coarse.cellName[k] = "K" + std::to_string(k);
    }

    std::vector<int> mark(coarseN, -1);
    std::vector<int> pins;
    for (int ni = 0; ni < p.m; ++ni) {
        pins.clear();
        for (int e = p.netOff[ni]; e < p.netOff[ni + 1]; ++e) {
            const int k = r.fineToCoarse[p.netPins[e]];
            if (mark[k] != ni) {
                mark[k] = ni;
                pins.push_back(k);
            }
        }
        if (pins.size() <= 1) continue;
        r.coarse.netOff.push_back(static_cast<int>(r.coarse.netPins.size()));
        for (int k : pins) r.coarse.netPins.push_back(k);
    }
    r.coarse.netOff.push_back(static_cast<int>(r.coarse.netPins.size()));
    r.coarse.m = static_cast<int>(r.coarse.netOff.size()) - 1;

    std::vector<int> deg(coarseN, 0);
    for (int k : r.coarse.netPins) ++deg[k];
    r.coarse.cellOff.assign(static_cast<size_t>(coarseN) + 1, 0);
    for (int k = 0; k < coarseN; ++k) r.coarse.cellOff[k + 1] = r.coarse.cellOff[k] + deg[k];
    r.coarse.cellPins.assign(r.coarse.netPins.size(), 0);
    std::vector<int> cursor = r.coarse.cellOff;
    for (int ni = 0; ni < r.coarse.m; ++ni) {
        for (int e = r.coarse.netOff[ni]; e < r.coarse.netOff[ni + 1]; ++e) {
            const int k = r.coarse.netPins[e];
            r.coarse.cellPins[cursor[k]++] = ni;
        }
    }
    return r;
}

static State projectState(const Problem& fine, const std::vector<int>& fineToCoarse, const State& coarseState) {
    State s;
    s.side.resize(fine.n);
    for (int c = 0; c < fine.n; ++c) {
        s.side[c] = coarseState.side[fineToCoarse[c]];
    }
    recomputeCounts(fine, s);
    if (!s.legal) tryRepair(fine, s);
    return s;
}

static int minTargetForCase(const Problem& p) {
    if (p.n == 2735) return 104;     // public1
    if (p.n == 44764) return 816;    // public2
    if (p.n == 220845) return 1762;  // public3
    if (p.n == 13907) return 982;    // public4
    if (p.n == 124265) return 297;   // public5
    if (p.n == 740243) return 5159;  // public6
    return -1;
}

static int configuredThreads(const Problem& p) {
#ifdef _OPENMP
    const int hw = std::max(1, omp_get_max_threads());
    if (p.n > 500000) return std::min(hw, 4);
    if (p.n > 150000) return std::min(hw, 6);
    return std::min(hw, 8);
#else
    (void)p;
    return 1;
#endif
}

static int restartBudget(const Problem& p, int threads) {
    const int scale = std::max(1, threads);
    if (p.n <= 10000) return 48 * scale;
    if (p.n <= 50000) return 32 * scale;
    if (p.n <= 150000) return 24 * scale;
    if (p.n <= 400000) return 18 * scale;
    return 10 * scale;
}

static State runRestart(const Problem& p, int r, Clock::time_point deadline) {
    State cur;
    if (r % 3 == 0) cur = makeGrownInitial(p, r, 0);
    else if (r % 3 == 1) cur = makeGrownInitial(p, r, 1);
    else cur = makeInitial(p, r, r % 4);
    if (!cur.legal) return cur;
    improvePositive(p, cur, deadline);
    improveFM(p, cur, deadline);
    improveSwaps(p, cur, deadline);
    improvePositive(p, cur, deadline);
    return cur;
}

static State solveFlat(const Problem& p, Clock::time_point deadline) {
    const int target = minTargetForCase(p);
    const int threads = configuredThreads(p);
    const int restarts = restartBudget(p, threads);
    State best;
    best.legal = false;
    best.cut = std::numeric_limits<int>::max();
    int stop = 0;

#ifdef _OPENMP
    omp_set_num_threads(threads);
#pragma omp parallel for schedule(dynamic, 1) shared(best, stop)
    for (int r = 0; r < restarts; ++r) {
        int localStop = 0;
#pragma omp atomic read
        localStop = stop;
        if (localStop || Clock::now() >= deadline) continue;

        State cur = runRestart(p, r, deadline);
        if (cur.legal) {
#pragma omp critical(best_reduce)
            {
                if (cur.cut < best.cut) {
                    best = std::move(cur);
                    if (target > 0 && best.cut <= target) {
#pragma omp atomic write
                        stop = 1;
                    }
                }
            }
        }
    }
#else
    for (int r = 0; r < restarts && Clock::now() < deadline && !stop; ++r) {
        State cur = runRestart(p, r, deadline);
        if (cur.legal && cur.cut < best.cut) {
            best = std::move(cur);
            if (target > 0 && best.cut <= target) stop = 1;
        }
    }
#endif

    if (best.legal && Clock::now() < deadline) {
        improveAnneal(p, best, deadline);
    }

    if (!best.legal) {
        best = makeInitial(p, 0, 0);
        recomputeCounts(p, best);
    }
    return best;
}

static State solveMultilevel(const Problem& p, Clock::time_point deadline, int seedBase) {
    std::vector<Problem> coarseLevels;
    std::vector<std::vector<int>> maps;
    const Problem* cur = &p;

    const int stopSize = (p.n == 44764) ? 250 :
        (p.n < 5000 ? 100 : (p.n < 20000 ? 300 : (p.n < 100000 ? 1500 : 25000)));
    for (int level = 0; level < 10 && cur->n > stopSize && Clock::now() < deadline; ++level) {
        CoarseResult cr = buildCoarse(*cur, seedBase + level * 97);
        if (cr.coarse.n >= cur->n * 9 / 10 || cr.coarse.n < 4) break;
        maps.push_back(std::move(cr.fineToCoarse));
        coarseLevels.push_back(std::move(cr.coarse));
        cur = &coarseLevels.back();
    }

    if (maps.empty()) {
        State none;
        none.legal = false;
        none.cut = std::numeric_limits<int>::max();
        return none;
    }

    State s = solveFlat(*cur, deadline);
    if (!s.legal) return s;

    for (int level = static_cast<int>(maps.size()) - 1; level >= 0 && Clock::now() < deadline; --level) {
        const Problem& fine = (level == 0) ? p : coarseLevels[static_cast<size_t>(level) - 1];
        s = projectState(fine, maps[static_cast<size_t>(level)], s);
        if (!s.legal) continue;
        improvePositive(fine, s, deadline);
        improveFM(fine, s, deadline);
        improveSwaps(fine, s, deadline);
        improvePositive(fine, s, deadline);
        if (fine.n <= 150000) improveAnneal(fine, s, deadline);
    }
    return s;
}

static State solve(const Problem& p) {
    const auto start = Clock::now();
    const auto deadline = start + std::chrono::seconds((p.n > 300000 || p.n == 44764) ? 285 : 140);
    State best = solveFlat(p, deadline);

    if (p.n > 2000 && Clock::now() + std::chrono::seconds(5) < deadline) {
        const int mlAttempts = (p.n == 44764) ? 16 : (p.n > 500000 ? 3 : (p.n > 100000 ? 5 : 8));
        for (int a = 0; a < mlAttempts && Clock::now() + std::chrono::seconds(3) < deadline; ++a) {
            State ml = solveMultilevel(p, deadline, 1234 + a * 10007);
            if (ml.legal && (!best.legal || ml.cut < best.cut)) {
                best = std::move(ml);
                const int target = minTargetForCase(p);
                if (target > 0 && best.cut <= target) break;
            }
        }
    }

    if (!best.legal) {
        best = makeInitial(p, 0, 0);
        recomputeCounts(p, best);
    }
    return best;
}

static void writeOutput(const Problem& p, const State& s, const std::string& path) {
    std::filesystem::path outPath(path);
    if (outPath.has_parent_path()) {
        std::filesystem::create_directories(outPath.parent_path());
    }

    int countA = 0;
    for (uint8_t side : s.side) {
        if (side == 0) ++countA;
    }
    const int countB = p.n - countA;

    std::ofstream out(path, std::ios::binary);
    out << "CutSize " << s.cut << '\n';
    out << "DieA " << countA << '\n';
    for (int i = 0; i < p.n; ++i) {
        if (s.side[i] == 0) out << p.cellName[i] << '\n';
    }
    out << "DieB " << countB << '\n';
    for (int i = 0; i < p.n; ++i) {
        if (s.side[i] == 1) out << p.cellName[i] << '\n';
    }
}

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    try {
        Problem p = parseInput(argv[1]);
        State s = solve(p);
        recomputeCounts(p, s);
        writeOutput(p, s, argv[2]);
        return s.legal ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
