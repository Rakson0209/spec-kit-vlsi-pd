#include "seqpair.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <vector>

int SequencePair::pos_in(const std::string& name, const std::vector<std::string>& seq) const {
    for (int i = 0; i < (int)seq.size(); i++)
        if (seq[i] == name) return i;
    return -1;
}

void SequencePair::encode(const std::vector<SoftModule*>& soft,
                          const std::vector<FixedModule*>& fixed) {
    int ns = (int)soft.size();
    int nf = (int)fixed.size();
    n = ns + nf;
    sigma_plus.resize(n);
    sigma_minus.resize(n);

    struct E { int cx, cy; std::string name; };
    std::vector<E> ents; ents.reserve(n);
    for (auto* s : soft) ents.push_back({s->center_x(), s->center_y(), s->name});
    for (auto* f : fixed) ents.push_back({f->center_x(), f->center_y(), f->name});

    std::vector<E> bx = ents;
    std::sort(bx.begin(), bx.end(), [](const E& a, const E& b){ return a.cx < b.cx || (a.cx==b.cx && a.cy < b.cy); });
    std::vector<E> by = ents;
    std::sort(by.begin(), by.end(), [](const E& a, const E& b){ return a.cy < b.cy || (a.cy==b.cy && a.cx < b.cx); });

    for (int i = 0; i < n; i++) { sigma_plus[i] = bx[i].name; sigma_minus[i] = by[i].name; }
}

// Robust decode using sequential placement respecting constraints from seqpair.
void SequencePair::decode(const Chip& chip,
                          std::vector<SoftModule*>& soft,
                          const std::vector<FixedModule*>& fixed) {
    int m = n;
    if (m == 0) return;

    // Build name -> index and position maps
    std::unordered_map<std::string, int> n2i;
    for (int i = 0; i < m; i++) n2i[sigma_plus[i]] = i;

    std::vector<int> pos_p(m), pos_m(m);
    for (int i = 0; i < m; i++) {
        pos_p[n2i[sigma_plus[i]]] = i;
        pos_m[n2i[sigma_minus[i]]] = i;
    }

    // Get module dimensions
    std::vector<int> W(m), HH(m);
    std::vector<bool> is_fixed(m);
    for (int i = 0; i < m; i++) {
        W[i] = 1; HH[i] = 1; is_fixed[i] = false;
        for (auto* s : soft) { if (s->name == sigma_plus[i]) { W[i]=s->w; HH[i]=s->h; break; } }
        for (auto* f : fixed) { if (f->name == sigma_plus[i]) { W[i]=f->w; HH[i]=f->h; is_fixed[i]=true; break; } }
    }

    // Fix fixed module dimensions and save original positions
    std::vector<int> fix_x(m, 0), fix_y(m, 0);
    for (auto* f : fixed) {
        int idx = n2i[f->name];
        fix_x[idx] = f->x; fix_y[idx] = f->y;
        W[idx] = f->w; HH[idx] = f->h;
    }

    // Build constraint graphs: for each pair (i,j), determine spatial relationship
    // If i is left of j: pos_p[i]<pos_p[j] && pos_m[i]<pos_m[j]
    // If i is below j: pos_p[i]<pos_p[j] && pos_m[i]>pos_m[j]
    // Use this to determine placement order
    std::vector<std::vector<int>> left_of(m), below_of(m);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            if (i == j) continue;
            if (pos_p[i] < pos_p[j] && pos_m[i] < pos_m[j])
                left_of[i].push_back(j); // i is left of j
            else if (pos_p[i] > pos_p[j] && pos_m[i] > pos_m[j])
                left_of[j].push_back(i); // j is left of i
            if (pos_p[i] < pos_p[j] && pos_m[i] > pos_m[j])
                below_of[j].push_back(i); // j is above i (i below j)
            else if (pos_p[i] > pos_p[j] && pos_m[i] < pos_m[j])
                below_of[i].push_back(j); // i is above j (j below i)
        }
    }

    // Compute in-degree for placement order (topological)
    std::vector<int> deg(m, 0);
    for (int i = 0; i < m; i++)
        for (int v : left_of[i]) deg[v]++;

    // Place in topological order of left_of (left to right)
    std::vector<int> X(m, 0), Y(m, 0);
    std::vector<int> q; q.reserve(m);
    for (int i = 0; i < m; i++) if (deg[i] == 0) q.push_back(i);

    // First pass: compute positions from constraint graph
    {
        std::vector<int> d = deg;
        int head = 0;
        while (head < (int)q.size()) {
            int u = q[head++];
            for (int v : left_of[u]) {
                X[v] = std::max(X[v], X[u] + W[u]);
                d[v]--;
                if (d[v] == 0) q.push_back(v);
            }
        }
    }

    // Compute Y from below_of
    {
        std::vector<int> deg_v(m, 0);
        for (int i = 0; i < m; i++)
            for (int v : below_of[i]) deg_v[v]++;
        std::vector<int> qv; qv.reserve(m);
        for (int i = 0; i < m; i++) if (deg_v[i] == 0) qv.push_back(i);
        int head = 0;
        while (head < (int)qv.size()) {
            int u = qv[head++];
            for (int v : below_of[u]) {
                Y[v] = std::max(Y[v], Y[u] + HH[u]);
                deg_v[v]--;
                if (deg_v[v] == 0) qv.push_back(v);
            }
        }
    }

    // Fix positions for fixed modules
    for (int i = 0; i < m; i++) {
        if (is_fixed[i]) {
            X[i] = fix_x[i];
            Y[i] = fix_y[i];
        }
    }

    // Check bounds
    int max_x = 0, max_y = 0;
    for (int i = 0; i < m; i++) {
        max_x = std::max(max_x, X[i] + W[i]);
        max_y = std::max(max_y, Y[i] + HH[i]);
    }

    // Scale soft modules if exceeding chip
    if (max_x > chip.width || max_y > chip.height) {
        double sx = (max_x > chip.width) ? (double)chip.width / max_x : 1.0;
        double sy = (max_y > chip.height) ? (double)chip.height / max_y : 1.0;
        double sc = std::min(sx, sy) * 0.9;
        for (int i = 0; i < m; i++) {
            if (is_fixed[i]) continue;
            int nw = std::max(1, (int)(W[i] * sc));
            int nh = std::max(1, (int)(HH[i] * sc));
            // Find valid shape
            for (auto* s : soft) {
                if (s->name == sigma_plus[i] && nw * nh < s->min_area) {
                    int ba = 1e9;
                    for (const auto& sh : s->shapes) {
                        if (sh.first * sh.second >= s->min_area && sh.first*sh.second < ba) {
                            nw = sh.first; nh = sh.second; ba = nw * nh;
                        }
                    }
                    if (nw * nh < s->min_area && !s->shapes.empty()) {
                        nw = s->shapes[0].first; nh = s->shapes[0].second;
                    }
                    break;
                }
            }
            // Aspect ratio
            while (nw > 0 && (double)nh / nw > 2.0) nw++;
            while (nw > 0 && (double)nh / nw < 0.5) nw = std::max(1, nw - 1);
            W[i] = nw; HH[i] = nh;
        }
        // Recompute positions
        std::fill(X.begin(), X.end(), 0);
        std::fill(Y.begin(), Y.end(), 0);
        {
            std::vector<int> d = deg;
            std::vector<int> qq; qq.reserve(m);
            for (int i = 0; i < m; i++) if (d[i] == 0) qq.push_back(i);
            int h = 0;
            while (h < (int)qq.size()) {
                int u = qq[h++];
                for (int v : left_of[u]) {
                    X[v] = std::max(X[v], X[u] + W[u]);
                    d[v]--; if (d[v] == 0) qq.push_back(v);
                }
            }
        }
        {
            std::vector<int> dv(m, 0);
            for (int i = 0; i < m; i++) for (int v : below_of[i]) dv[v]++;
            std::vector<int> qq; qq.reserve(m);
            for (int i = 0; i < m; i++) if (dv[i] == 0) qq.push_back(i);
            int h = 0;
            while (h < (int)qq.size()) {
                int u = qq[h++];
                for (int v : below_of[u]) {
                    Y[v] = std::max(Y[v], Y[u] + HH[u]);
                    dv[v]--; if (dv[v] == 0) qq.push_back(v);
                }
            }
        }
        for (int i = 0; i < m; i++)
            if (is_fixed[i]) { X[i] = fix_x[i]; Y[i] = fix_y[i]; }
    }

    // Final cleanup: shift soft modules to resolve any overlaps with fixed modules
    for (int i = 0; i < m; i++) {
        if (is_fixed[i]) continue;
        for (int attempt = 0; attempt < 1000; attempt++) {
            bool ok = true;
            for (int j = 0; j < m; j++) {
                if (is_fixed[j] && X[i] < X[j]+W[j] && X[j] < X[i]+W[i] &&
                           Y[i] < Y[j]+HH[j] && Y[j] < Y[i]+HH[i]) {
                    ok = false;
                    X[i] = X[j] + W[j]; // shift right
                    break;
                }
            }
            if (ok) break;
            if (X[i] + W[i] > chip.width) X[i] = std::max(0, chip.width - W[i]);
        }
    }

    // Resolve soft-soft overlaps
    for (int iter = 0; iter < 100; iter++) {
        bool any = false;
        for (int i = 0; i < m; i++) {
            if (is_fixed[i]) continue;
            for (int j = i+1; j < m; j++) {
                if (is_fixed[j]) continue;
                if (X[i] < X[j]+W[j] && X[j] < X[i]+W[i] &&
                    Y[i] < Y[j]+HH[j] && Y[j] < Y[i]+HH[i]) {
                    X[i] = X[j] + W[j];
                    any = true;
                    break;
                }
            }
            if (X[i] + W[i] > chip.width) X[i] = std::max(0, chip.width - W[i]);
        }
        if (!any) break;
    }

    // Assign to soft modules
    for (auto* s : soft) {
        int idx = n2i[s->name];
        s->x = X[idx]; s->y = Y[idx];
        s->w = W[idx]; s->h = HH[idx];
    }
}
