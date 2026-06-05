#include "sa.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <vector>

struct ModuleState { int x, y, w, h; };

static bool has_overlap(int x, int y, int w, int h,
                        const std::vector<SoftModule*>& soft,
                        const std::vector<FixedModule*>& fixed,
                        int exclude) {
    for (int i = 0; i < (int)soft.size(); i++) {
        if (i == exclude) continue;
        const SoftModule* s = soft[i];
        if (x < s->x + s->w && s->x < x + w && y < s->y + s->h && s->y < y + h)
            return true;
    }
    for (const auto* f : fixed)
        if (x < f->x + f->w && f->x < x + w && y < f->y + f->h && f->y < y + h)
            return true;
    return false;
}

static int weighted_select(const std::vector<SoftModule*>& soft,
                            const std::vector<Net>& nets, std::mt19937& rng) {
    int n = (int)soft.size();
    if (n <= 1) return 0;
    std::vector<double> w(n, 0);
    for (const auto& net : nets)
        for (int i = 0; i < n; i++)
            if (soft[i]->name == net.module_a || soft[i]->name == net.module_b)
                w[i] += net.weight;
    double t = 0; for (double v : w) t += v;
    if (t <= 0) return rng() % n;
    double r = ((double)(rng() % 1000000) / 1000000.0) * t;
    double c = 0;
    for (int i = 0; i < n; i++) { c += w[i]; if (r <= c) return i; }
    return n - 1;
}

static bool op_move(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F,
                    const Chip& C, std::mt19937& rng, int idx) {
    SoftModule* m = S[idx]; m->backup();
    int max_step = std::max(C.width, C.height) / 20;
    if (max_step < 1) max_step = 1;
    for (int att = 0; att < 40; att++) {
        int dir = rng() % 4;
        int step = rng() % max_step + 1;
        int nx = m->x, ny = m->y;
        if (dir == 0) nx -= step; else if (dir == 1) nx += step;
        else if (dir == 2) ny -= step; else ny += step;
        nx = std::max(0, std::min(nx, C.width - m->w));
        ny = std::max(0, std::min(ny, C.height - m->h));
        if (!has_overlap(nx, ny, m->w, m->h, S, F, idx)) { m->x = nx; m->y = ny; return true; }
    }
    m->restore(); return false;
}

static bool op_attract(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F,
                        const std::vector<Net>& nets, const Chip& C, int idx, std::mt19937& rng) {
    SoftModule* m = S[idx]; m->backup();
    int bt = -1, bw = 0;
    for (const auto& net : nets) {
        int t = -1;
        if (net.module_a == m->name) for (int i = 0; i < (int)S.size(); i++) if (S[i]->name == net.module_b) { t = i; break; }
        if (net.module_b == m->name) for (int i = 0; i < (int)S.size(); i++) if (S[i]->name == net.module_a) { t = i; break; }
        if (t >= 0 && net.weight > bw) { bt = t; bw = net.weight; }
    }
    if (bt < 0) { m->restore(); return false; }
    SoftModule* tgt = S[bt];
    for (int att = 0; att < 30; att++) {
        int nx = m->x, ny = m->y;
        int step = rng() % 10 + 1;
        if (m->x + m->w/2 < tgt->x + tgt->w/2) nx += step; else nx -= step;
        if (m->y + m->h/2 < tgt->y + tgt->h/2) ny += step; else ny -= step;
        nx = std::max(0, std::min(nx, C.width - m->w));
        ny = std::max(0, std::min(ny, C.height - m->h));
        if (!has_overlap(nx, ny, m->w, m->h, S, F, idx)) { m->x = nx; m->y = ny; return true; }
    }
    m->restore(); return false;
}

static bool op_swap(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F, int i, int j) {
    S[i]->backup(); S[j]->backup();
    // Try 1: swap positions only (keep shapes)
    bool ok = !has_overlap(S[j]->x, S[j]->y, S[i]->w, S[i]->h, S, F, i) &&
              !has_overlap(S[i]->x, S[i]->y, S[j]->w, S[j]->h, S, F, j);
    if (ok) {
        int tx = S[i]->x, ty = S[i]->y;
        S[i]->x = S[j]->x; S[i]->y = S[j]->y;
        S[j]->x = tx; S[j]->y = ty;
        return true;
    }
    // Try 2: swap positions AND shapes (validate area + aspect ratio)
    ok = !has_overlap(S[j]->x, S[j]->y, S[j]->w, S[j]->h, S, F, i) &&
         !has_overlap(S[i]->x, S[i]->y, S[i]->w, S[i]->h, S, F, j);
    if (ok) {
        // Validate: S[i] gets shape of S[j], S[j] gets shape of S[i]
        if (S[j]->w * S[j]->h >= S[i]->min_area && S[i]->w * S[i]->h >= S[j]->min_area) {
            double ar_i = (double)S[j]->h / S[j]->w; // S[i] gets S[j]'s shape
            double ar_j = (double)S[i]->h / S[i]->w; // S[j] gets S[i]'s shape
            if (ar_i >= 0.5 - 1e-9 && ar_i <= 2.0 + 1e-9 &&
                ar_j >= 0.5 - 1e-9 && ar_j <= 2.0 + 1e-9) {
                int tx = S[i]->x, ty = S[i]->y, tw = S[i]->w, th = S[i]->h;
                S[i]->x = S[j]->x; S[i]->y = S[j]->y; S[i]->w = S[j]->w; S[i]->h = S[j]->h;
                S[j]->x = tx; S[j]->y = ty; S[j]->w = tw; S[j]->h = th;
                return true;
            }
        }
    }
    S[i]->restore(); S[j]->restore();
    return false;
}

static bool op_shape(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F,
                     std::mt19937& rng, int idx) {
    SoftModule* m = S[idx]; m->backup();
    if (m->shapes.empty()) { m->restore(); return false; }
    for (int att = 0; att < (int)m->shapes.size(); att++) {
        int s = (int)(rng() % m->shapes.size());
        int nw = m->shapes[s].first, nh = m->shapes[s].second;
        if (nw <= 0 || nh <= 0) continue; // safety
        if (nw == m->w && nh == m->h) continue;
        // Validate area and aspect ratio
        if (nw * nh < m->min_area) continue;
        double ar = (double)nh / nw;
        if (ar < 0.5 - 1e-9 || ar > 2.0 + 1e-9) continue;
        if (!has_overlap(m->x, m->y, nw, nh, S, F, idx)) { m->w = nw; m->h = nh; return true; }
    }
    m->restore(); return false;
}

static bool op_shape_reposition(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F,
                                 const Chip& C, int idx, std::mt19937& rng) {
    SoftModule* m = S[idx]; m->backup();
    if (m->shapes.empty()) { m->restore(); return false; }
    int s = (int)(rng() % m->shapes.size());
    int nw = m->shapes[s].first, nh = m->shapes[s].second;
    // Validate area and aspect ratio + safety
    if (nw <= 0 || nh <= 0) { m->restore(); return false; }
    if (nw * nh < m->min_area) { m->restore(); return false; }
    double ar = (double)nh / nw;
    if (ar < 0.5 - 1e-9 || ar > 2.0 + 1e-9) { m->restore(); return false; }
    if (nw == m->w && nh == m->h) { m->restore(); return false; }
    if (!has_overlap(m->x, m->y, nw, nh, S, F, idx)) { m->w = nw; m->h = nh; return true; }
    int range = std::max(C.width, C.height) / 15;
    for (int att = 0; att < 80; att++) {
        int nx = std::max(0, std::min((int)(m->x + (int)(rng()%(range*2+1))-range), C.width - nw));
        int ny = std::max(0, std::min((int)(m->y + (int)(rng()%(range*2+1))-range), C.height - nh));
        if (!has_overlap(nx, ny, nw, nh, S, F, idx)) { m->x = nx; m->y = ny; m->w = nw; m->h = nh; return true; }
    }
    m->restore(); return false;
}

static bool op_compact(std::vector<SoftModule*>& S, const std::vector<FixedModule*>& F,
                       std::mt19937& rng, int idx) {
    SoftModule* m = S[idx]; m->backup();
    bool mv = false;
    while (m->x > 0 && !has_overlap(m->x-1, m->y, m->w, m->h, S, F, idx)) { m->x--; mv = true; }
    while (m->y > 0 && !has_overlap(m->x, m->y-1, m->w, m->h, S, F, idx)) { m->y--; mv = true; }
    if (!mv) { m->restore(); return false; }
    return true;
}

static long long compute_cost(int hpwl, const ValidationResult& vr) {
    long long p = 0;
    for (const auto& v : vr.violations) {
        if (v.find("out of") != std::string::npos) p += 1000000000LL;
        else if (v.find("overlaps") != std::string::npos) p += 500000000LL;
        else if (v.find("area") != std::string::npos) p += 100000000LL;
        else if (v.find("aspect") != std::string::npos) p += 10000000LL;
    }
    return (long long)hpwl + p;
}

void run_sa(const Chip& chip,
            std::vector<SoftModule*>& soft,
            const std::vector<FixedModule*>& fixed,
            const std::vector<Net>& nets,
            double max_seconds) {

    auto t_start = std::chrono::steady_clock::now();
    auto elapsed = [&]() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
    };

    int n = (int)soft.size();
    std::mt19937 rng((unsigned int)time(nullptr));

    int hpwl = calculate_hpwl(soft, fixed, nets);
    auto vr = validate(chip, soft, fixed);

    // Pre-pass: repair initial overlaps - robust multi-module repair
    // If initial placement has overlaps, re-run a simple skyline placement for bad modules
    if (!vr.valid) {
        // Collect all soft modules involved in violations
        std::vector<int> bad_modules;
        for (int i = 0; i < n; i++) {
            for (const auto& v : vr.violations) {
                if (v.find(soft[i]->name) != std::string::npos) {
                    if ((int)bad_modules.size() == 0 || bad_modules.back() != i)
                        bad_modules.push_back(i);
                }
            }
        }

        // For each bad module, try to find a valid position
        for (int bi = 0; bi < (int)bad_modules.size(); bi++) {
            int i = bad_modules[bi];
            soft[i]->backup();
            int nx = soft[i]->x, ny = soft[i]->y;
            int nw = soft[i]->w, nh = soft[i]->h;
            bool moved = false;

            // Strategy 1: random placement
            for (int attempt = 0; attempt < 2000 && !moved; attempt++) {
                nx = (int)(rng() % (chip.width - nw + 1));
                ny = (int)(rng() % (chip.height - nh + 1));
                if (!has_overlap(nx, ny, nw, nh, soft, fixed, i)) moved = true;
            }

            // Strategy 2: try compacting from all 4 corners
            if (!moved) {
                int corners[][2] = {
                    {chip.width - nw, chip.height - nh},  // bottom-right
                    {chip.width - nw, 0},                 // top-right
                    {0, chip.height - nh},                // bottom-left
                    {0, 0}                                 // top-left
                };
                for (int ci = 0; ci < 4 && !moved; ci++) {
                    nx = corners[ci][0]; ny = corners[ci][1];
                    // Compact: if top-right, compact left and down; if bottom-left, compact right and up; etc.
                    if (ci == 0) { // bottom-right: compact up, then left
                        while (ny > 0 && !has_overlap(nx, ny-1, nw, nh, soft, fixed, i)) ny--;
                        while (nx > 0 && !has_overlap(nx-1, ny, nw, nh, soft, fixed, i)) nx--;
                    } else if (ci == 1) { // top-right: compact left, then down
                        while (nx > 0 && !has_overlap(nx-1, ny, nw, nh, soft, fixed, i)) nx--;
                        while (ny + nh < chip.height && !has_overlap(nx, ny+1, nw, nh, soft, fixed, i)) ny++;
                    } else if (ci == 2) { // bottom-left: compact right, then up
                        while (nx + nw < chip.width && !has_overlap(nx+1, ny, nw, nh, soft, fixed, i)) nx++;
                        while (ny > 0 && !has_overlap(nx, ny-1, nw, nh, soft, fixed, i)) ny--;
                    } else { // top-left: compact right, then down
                        while (nx + nw < chip.width && !has_overlap(nx+1, ny, nw, nh, soft, fixed, i)) nx++;
                        while (ny + nh < chip.height && !has_overlap(nx, ny+1, nw, nh, soft, fixed, i)) ny++;
                    }
                    if (!has_overlap(nx, ny, nw, nh, soft, fixed, i)) moved = true;
                }
            }

            // Strategy 3: try all 4 edges of each module (other soft + fixed)
            if (!moved) {
                auto try_4_edges = [&](int ox, int oy, int ow, int oh) {
                    int cands[][2] = {{ox+ow,oy},{ox,oy+oh},{ox-nw,oy},{ox,oy-nh}};
                    for (int c = 0; c < 4; c++) {
                        int cx = cands[c][0], cy = cands[c][1];
                        if (cx>=0 && cy>=0 && cx+nw<=chip.width && cy+nh<=chip.height)
                            if (!has_overlap(cx,cy,nw,nh,soft,fixed,i)) { nx=cx; ny=cy; moved=true; return; }
                    }
                };
                for (int j = 0; j < (int)soft.size() && !moved; j++) {
                    if (j == i) continue;
                    try_4_edges(soft[j]->x, soft[j]->y, soft[j]->w, soft[j]->h);
                }
                for (const auto* f : fixed) { if (!moved) try_4_edges(f->x, f->y, f->w, f->h); }
            }

            // Strategy 4: try different shapes + random + edge placement
            if (!moved && !soft[i]->shapes.empty()) {
                for (size_t si = 0; si < soft[i]->shapes.size() && !moved; si++) {
                    int tnw = soft[i]->shapes[si].first;
                    int tnh = soft[i]->shapes[si].second;
                    if (tnw > chip.width || tnh > chip.height) continue;
                    for (int att2 = 0; att2 < 1000 && !moved; att2++) {
                        int sx = (int)(rng() % (chip.width - tnw + 1));
                        int sy = (int)(rng() % (chip.height - tnh + 1));
                        if (!has_overlap(sx,sy,tnw,tnh,soft,fixed,i)) {
                            nx=sx; ny=sy; nw=tnw; nh=tnh; moved=true;
                        }
                    }
                    if (!moved) {
                        // Skyline compact with new shape from corner
                        int cx = chip.width - tnw, cy = chip.height - tnh;
                        while (cy > 0 && !has_overlap(cx, cy-1, tnw, tnh, soft, fixed, i)) cy--;
                        while (cx > 0 && !has_overlap(cx-1, cy, tnw, tnh, soft, fixed, i)) cx--;
                        if (!has_overlap(cx, cy, tnw, tnh, soft, fixed, i)) {
                            nx=cx; ny=cy; nw=tnw; nh=tnh; moved=true;
                        }
                    }
                    if (!moved) {
                        for (int j = 0; j < (int)soft.size() && !moved; j++) {
                            if (j == i) continue;
                            int cands[][2] = {{soft[j]->x+soft[j]->w,soft[j]->y},{soft[j]->x,soft[j]->y+soft[j]->h},
                                {soft[j]->x-tnw,soft[j]->y},{soft[j]->x,soft[j]->y-tnh}};
                            for (int c = 0; c < 4; c++) {
                                int cx = cands[c][0], cy = cands[c][1];
                                if (cx>=0 && cy>=0 && cx+tnw<=chip.width && cy+tnh<=chip.height)
                                    if (!has_overlap(cx,cy,tnw,tnh,soft,fixed,i)) {
                                        nx=cx; ny=cy; nw=tnw; nh=tnh; moved=true; goto done_shape;
                                    }
                            }
                        }
                        for (const auto* f : fixed) {
                            if (!moved) {
                                int cands[][2] = {{f->x+f->w,f->y},{f->x,f->y+f->h},{f->x-tnw,f->y},{f->x,f->y-tnh}};
                                for (int c = 0; c < 4; c++) {
                                    int cx = cands[c][0], cy = cands[c][1];
                                    if (cx>=0 && cy>=0 && cx+tnw<=chip.width && cy+tnh<=chip.height)
                                        if (!has_overlap(cx,cy,tnw,tnh,soft,fixed,i)) {
                                            nx=cx; ny=cy; nw=tnw; nh=tnh; moved=true; goto done_shape;
                                        }
                                }
                            }
                        }
                        done_shape:;
                    }
                }
            }

            // Strategy 5: full systematic scan at step 1 (for small chips) or step=max/500
            if (!moved) {
                int best_w = (int)std::sqrt(soft[i]->min_area);
                int best_h = (int)std::ceil((double)soft[i]->min_area / best_w);
                int scan_step = std::max(1, std::max(chip.width, chip.height) / 500);
                if (chip.width * chip.height <= 10000000) scan_step = 1;
                for (int y = 0; y + best_h <= chip.height && !moved; y += scan_step) {
                    for (int x = 0; x + best_w <= chip.width && !moved; x += scan_step) {
                        if (!has_overlap(x,y,best_w,best_h,soft,fixed,i)) {
                            nx=x; ny=y; nw=best_w; nh=best_h; moved=true;
                        }
                    }
                }
            }

            if (moved) { soft[i]->x = nx; soft[i]->y = ny; soft[i]->w = nw; soft[i]->h = nh; }
            else soft[i]->restore();
        }
    }

    hpwl = calculate_hpwl(soft, fixed, nets);
    vr = validate(chip, soft, fixed);

    // Overlap escape phase: directly relocate overlapping modules to valid positions
    if (!vr.valid) {
        for (int ri = 0; ri < 200; ri++) {
            vr = validate(chip, soft, fixed);
            if (vr.valid) break;
            bool found = false;
            for (const auto& v : vr.violations) {
                if (v.find("overlaps") != std::string::npos) {
                    std::string a_name = v.substr(0, v.find(" overlaps"));
                    for (int i = 0; i < n; i++) {
                        if (soft[i]->name == a_name) {
                            soft[i]->backup();
                            bool moved = false;
                            // Try 5000 random positions with current shape
                            for (int att = 0; att < 5000 && !moved; att++) {
                                int rx = (int)(rng() % (chip.width - soft[i]->w + 1));
                                int ry = (int)(rng() % (chip.height - soft[i]->h + 1));
                                if (!has_overlap(rx, ry, soft[i]->w, soft[i]->h, soft, fixed, i)) {
                                    soft[i]->x = rx; soft[i]->y = ry; moved = true;
                                }
                            }
                            // Try 4 corners with compaction
                            if (!moved) {
                                int corners[][2] = {{chip.width-soft[i]->w, chip.height-soft[i]->h},
                                    {chip.width-soft[i]->w, 0}, {0, chip.height-soft[i]->h}, {0, 0}};
                                for (int ci = 0; ci < 4 && !moved; ci++) {
                                    int cx = corners[ci][0], cy = corners[ci][1];
                                    if (ci == 0) { while(cy>0&& !has_overlap(cx,cy-1,soft[i]->w,soft[i]->h,soft,fixed,i))cy--; while(cx>0&&!has_overlap(cx-1,cy,soft[i]->w,soft[i]->h,soft,fixed,i))cx--; }
                                    else if (ci == 1) { while(cx>0&&!has_overlap(cx-1,cy,soft[i]->w,soft[i]->h,soft,fixed,i))cx--; while(cy+soft[i]->h<chip.height&&!has_overlap(cx,cy+1,soft[i]->w,soft[i]->h,soft,fixed,i))cy++; }
                                    else if (ci == 2) { while(cx+soft[i]->w<chip.width&&!has_overlap(cx+1,cy,soft[i]->w,soft[i]->h,soft,fixed,i))cx++; while(cy>0&&!has_overlap(cx,cy-1,soft[i]->w,soft[i]->h,soft,fixed,i))cy--; }
                                    else { while(cx+soft[i]->w<chip.width&&!has_overlap(cx+1,cy,soft[i]->w,soft[i]->h,soft,fixed,i))cx++; while(cy+soft[i]->h<chip.height&&!has_overlap(cx,cy+1,soft[i]->w,soft[i]->h,soft,fixed,i))cy++; }
                                    if (!has_overlap(cx, cy, soft[i]->w, soft[i]->h, soft, fixed, i)) {
                                        soft[i]->x = cx; soft[i]->y = cy; moved = true;
                                    }
                                }
                            }
                            // Try different shapes
                            if (!moved && !soft[i]->shapes.empty()) {
                                for (size_t si = 0; si < soft[i]->shapes.size() && !moved; si++) {
                                    int tnw = soft[i]->shapes[si].first, tnh = soft[i]->shapes[si].second;
                                    if (tnw > chip.width || tnh > chip.height) continue;
                                    for (int att2 = 0; att2 < 2000 && !moved; att2++) {
                                        int rx = (int)(rng() % (chip.width - tnw + 1));
                                        int ry = (int)(rng() % (chip.height - tnh + 1));
                                        if (!has_overlap(rx, ry, tnw, tnh, soft, fixed, i)) {
                                            soft[i]->x = rx; soft[i]->y = ry;
                                            soft[i]->w = tnw; soft[i]->h = tnh; moved = true;
                                        }
                                    }
                                }
                            }
                            if (!moved) soft[i]->restore();
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
            if (!found) break;
        }
    }

    hpwl = calculate_hpwl(soft, fixed, nets);
    vr = validate(chip, soft, fixed);
    long long best_cost = compute_cost(hpwl, vr);
    int best_wp = hpwl;

    std::vector<ModuleState> best_state(n);
    for (int i = 0; i < n; i++)
        best_state[i] = {soft[i]->x, soft[i]->y, soft[i]->w, soft[i]->h};

    fprintf(stderr, "SA: init_HPWL=%d valid=%s n=%d\n", hpwl, vr.valid?"yes":"no", n);

    // Adaptive parameters: use most of available time for optimization
    int total_n = n + (int)fixed.size();
    int evals_per_pass;
    if (total_n <= 20) evals_per_pass = 100000000;  // 100M for small
    else if (total_n <= 40) evals_per_pass = 50000000;
    else if (total_n <= 60) evals_per_pass = 30000000;
    else evals_per_pass = 20000000;

    int num_passes = 20;

    for (int pass = 0; pass < num_passes && elapsed() < max_seconds - 1.0; pass++) {
        double T0;
        if (pass == 0) T0 = 1e7 * std::sqrt((double)n);
        else if (pass <= 4) T0 = 1e6 * std::sqrt((double)n);
        else if (pass <= 8) T0 = 1e5 * std::sqrt((double)n);
        else if (pass <= 15) T0 = 1e4 * std::sqrt((double)n);
        else T0 = 500 * std::sqrt((double)n);

        // Restore best solution before each pass
        if (pass > 0) {
            for (int i = 0; i < n; i++) {
                soft[i]->x = best_state[i].x; soft[i]->y = best_state[i].y;
                soft[i]->w = best_state[i].w; soft[i]->h = best_state[i].h;
            }
        }

        long long cur_cost = best_cost;
        double T = T0;
        int evals = 0;
        double cooling = 0.99999;  // very slow cooling for maximum iterations per pass

        while (evals < evals_per_pass && T >= 0.1 && elapsed() < max_seconds) {
            evals++;

            int op = (int)(rng() % 100);
            bool ok = false;
            int idx = 0;
            int idx2 = -1; // for swap

            // Higher weight on net-aware and swap operations for better convergence
            if (op < 25) { idx = weighted_select(soft, nets, rng); ok = op_move(soft, fixed, chip, rng, idx); }
            else if (op < 40) { idx = weighted_select(soft, nets, rng); ok = op_attract(soft, fixed, nets, chip, idx, rng); }
            else if (op < 50) { int i=weighted_select(soft,nets,rng), j=weighted_select(soft,nets,rng); while(j==i)j=weighted_select(soft,nets,rng); ok = op_swap(soft, fixed, i, j); }
            else if (op < 58) { idx=(int)(rng()%n); idx2=(int)(rng()%n); while(idx2==idx)idx2=(int)(rng()%n); ok = op_swap(soft, fixed, idx, idx2); }
            else if (op < 64) { idx = (int)(rng() % n); ok = op_move(soft, fixed, chip, rng, idx); }
            else if (op < 72) { idx = weighted_select(soft, nets, rng); ok = op_shape_reposition(soft, fixed, chip, idx, rng); }
            else if (op < 78) { idx = (int)(rng() % n); ok = op_shape_reposition(soft, fixed, chip, idx, rng); }
            else if (op < 84) { idx = (int)(rng() % n); ok = op_shape(soft, fixed, rng, idx); }
            else if (op < 88) { idx = weighted_select(soft, nets, rng); ok = op_compact(soft, fixed, rng, idx); }
            else { idx = (int)(rng() % n); ok = op_compact(soft, fixed, rng, idx); }

            if (!ok) {
                soft[idx]->restore();
                if (idx2 >= 0 && idx2 != idx) soft[idx2]->restore();
                continue;
            }

            int new_hpwl = calculate_hpwl(soft, fixed, nets);

            // Fast cost estimate: since all ops check overlap before moving,
            // only check bounds and aspect ratio for affected modules. Full validate every 50 iters.
            long long penalty = 0;
            bool need_full_validate = (evals % 50 == 0);
            if (!need_full_validate) {
                // Quick check: bounds + aspect ratio for affected module(s)
                auto quick_check = [&](int mi) {
                    SoftModule* m = soft[mi];
                    if (m->x < 0 || m->y < 0 || m->x + m->w > chip.width || m->y + m->h > chip.height)
                        penalty += 1000000000LL;
                    double ar = (double)m->h / m->w;
                    if (ar < 0.5 - 1e-9 || ar > 2.0 + 1e-9) penalty += 10000000LL;
                    if (m->w * m->h < m->min_area) penalty += 100000000LL;
                };
                quick_check(idx);
                if (idx2 >= 0 && idx2 != idx) quick_check(idx2);
            }
            long long new_cost;
            ValidationResult new_vr;
            if (need_full_validate) {
                new_vr = validate(chip, soft, fixed);
                new_cost = compute_cost(new_hpwl, new_vr);
            } else {
                new_cost = (long long)new_hpwl + penalty;
            }

            long long delta = new_cost - cur_cost;
            bool accept = false;
            if (delta < 0) accept = true;
            else {
                double r = (double)(rng() % 1000000) / 1000000.0;
                if (r < std::exp((double)(-delta) / (T + 1e-9))) accept = true;
            }

            if (accept) {
                cur_cost = new_cost;
                hpwl = new_hpwl;
                if (new_cost < best_cost) {
                    // Always do full validation before saving as best
                    ValidationResult check_vr = validate(chip, soft, fixed);
                    long long check_cost = compute_cost(new_hpwl, check_vr);
                    if (check_cost < best_cost) {
                        best_cost = check_cost;
                        for (int i = 0; i < n; i++)
                            best_state[i] = {soft[i]->x, soft[i]->y, soft[i]->w, soft[i]->h};
                    }
                }
            } else {
                soft[idx]->restore();
                if (idx2 >= 0 && idx2 != idx) soft[idx2]->restore();
            }

            // Adaptive cooling: exponential decay
            T *= cooling;
        }

        fprintf(stderr, "SA pass %d: evals=%d best_HPWL=%d time=%.2fs\n",
                pass + 1, evals, (int)best_cost, elapsed());
        // Get actual HPWL from best solution (cost == hpwl when valid)
        best_wp = (best_cost < 1e8) ? (int)best_cost : calculate_hpwl(soft, fixed, nets);
    }

    // Restore best - validate each module has positive dimensions
    for (int i = 0; i < n; i++) {
        soft[i]->x = best_state[i].x; soft[i]->y = best_state[i].y;
        soft[i]->w = best_state[i].w; soft[i]->h = best_state[i].h;
        // Safety: if w or h is 0, restore from backup (should never happen)
        if (soft[i]->w <= 0 || soft[i]->h <= 0) {
            soft[i]->restore();
        }
    }

    hpwl = calculate_hpwl(soft, fixed, nets);
    fprintf(stderr, "SA done: HPWL=%d time=%.2fs\n", hpwl, elapsed());
}
