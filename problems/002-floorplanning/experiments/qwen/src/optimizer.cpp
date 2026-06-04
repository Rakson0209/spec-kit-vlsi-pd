#include "optimizer.h"
#include "hpwl.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <climits>

Optimizer::Optimizer(Solution& s, Grid& g, std::mt19937& r)
    : sol(s), grid(g), rng(r), best_wirelength(INT_MAX), best_cost(1e18),
      start_time(clock()), timeout_secs_val(540.0) {}

bool Optimizer::run(double timeout_secs) {
    timeout_secs_val = timeout_secs;
    int n = (int)sol.soft_modules.size();
    if (n == 0) return false;

    best_wirelength = compute_wirelength();
    best_cost = cost_function(best_wirelength);
    best_soft = sol.soft_modules;

    double T = std::max(1000.0, static_cast<double>(best_wirelength) / std::log(std::max(2, n)));
    double T_MIN = 1.0;
    double T_DECAY = 0.95;
    int K = 20;
    int N = n * K;
    int DOUBLE_N = N * 2;

    std::uniform_real_distribution<double> dist01(0.0, 1.0);

    double current_cost = best_cost;
    int total_gen = 0, total_reject = 0;

    std::cerr << "  [SA] wl=" << best_wirelength << " T=" << (int)T
              << " n=" << n << std::endl;

    while (T >= T_MIN) {
        if (static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC > timeout_secs_val)
            break;

        int gen_cnt = 0, uphill_cnt = 0, reject_cnt = 0;

        while (uphill_cnt <= N && gen_cnt <= DOUBLE_N) {
            if (static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC > timeout_secs_val)
                goto sa_done;

            sol.backup_all_soft();
            int gen_before = (int)grid.placed.size();

            int op = (int)(dist01(rng) * 100);
            bool accepted = false;

            if (op < 35 && n >= 2) {
                // SWAP
                int i = (int)(dist01(rng) * n);
                int j = (int)(dist01(rng) * n);
                while (j == i) j = (int)(dist01(rng) * n);

                Module& a = sol.soft_modules[i];
                Module& b = sol.soft_modules[j];

                int pi = -1, pj = -1;
                for (int k = 0; k < (int)grid.placed_idx.size(); k++) {
                    if (grid.placed_idx[k] == i) pi = k;
                    if (grid.placed_idx[k] == j) pj = k;
                }
                if (pi < 0 || pj < 0) { gen_cnt++; reject_cnt++; continue; }

                if (pi > pj) { grid.remove_at(pi); grid.remove_at(pj); }
                else { grid.remove_at(pj); grid.remove_at(pi); }

                if (grid.can_place(a.x, a.y, b.width, b.height) &&
                    grid.can_place(b.x, b.y, a.width, a.height)) {
                    int tx = a.x, ty = a.y;
                    a.x = b.x; a.y = b.y;
                    b.x = tx;   b.y = ty;
                    grid.add_rect(a.x, a.y, a.width, a.height, i);
                    grid.add_rect(b.x, b.y, b.width, b.height, j);
                    if (a.is_valid_shape() && b.is_valid_shape())
                        accepted = true;
                }
            } else if (op < 70) {
                // MOVE
                int idx = (int)(dist01(rng) * n);
                Module& mod = sol.soft_modules[idx];

                int pi = -1;
                for (int k = 0; k < (int)grid.placed_idx.size(); k++) {
                    if (grid.placed_idx[k] == idx) { pi = k; break; }
                }
                if (pi < 0) { gen_cnt++; reject_cnt++; continue; }
                grid.remove_at(pi);

                int max_delta = std::max(10, std::min(mod.width, mod.height) / 2);
                int dx = (int)((double)rng() / (double)rng.max() * max_delta * 2) - max_delta;
                int dy = (int)((double)rng() / (double)rng.max() * max_delta * 2) - max_delta;

                int nx = mod.x + dx;
                int ny = mod.y + dy;
                nx = std::max(0, std::min(nx, sol.chip_width - mod.width));
                ny = std::max(0, std::min(ny, sol.chip_height - mod.height));

                if (grid.can_place(nx, ny, mod.width, mod.height)) {
                    auto c = grid.compact_up_left(nx, ny, mod.width, mod.height);
                    auto c2 = grid.compact_down_right(nx, ny, mod.width, mod.height);

                    int best_wl = INT_MAX;
                    int best_x = nx, best_y = ny;

                    auto eval_pos = [&](int px, int py) {
                        if (!grid.in_chip(px, py, mod.width, mod.height)) return;
                        if (!grid.can_place(px, py, mod.width, mod.height)) return;
                        int est_wl = 0;
                        for (const auto& net : sol.nets) {
                            if (net.idx_a != idx && net.idx_b != idx) continue;
                            const Module* other = nullptr;
                            if (net.idx_a == idx) other = sol.get_module(net.idx_b);
                            else other = sol.get_module(net.idx_a);
                            if (!other) continue;
                            est_wl += net.weight * (std::abs(px + mod.width/2 - other->center_x()) +
                                                    std::abs(py + mod.height/2 - other->center_y()));
                        }
                        if (est_wl < best_wl) { best_wl = est_wl; best_x = px; best_y = py; }
                    };

                    eval_pos(nx, ny);
                    eval_pos(c.first, c.second);
                    eval_pos(c2.first, c2.second);

                    mod.x = best_x; mod.y = best_y;
                    grid.add_rect(mod.x, mod.y, mod.width, mod.height, idx);
                    accepted = true;
                }
            } else {
                // RESHAPE
                int idx = (int)(dist01(rng) * n);
                Module& mod = sol.soft_modules[idx];

                if (mod.shapes.size() > 1) {
                    int pi = -1;
                    for (int k = 0; k < (int)grid.placed_idx.size(); k++) {
                        if (grid.placed_idx[k] == idx) { pi = k; break; }
                    }
                    if (pi >= 0) grid.remove_at(pi);

                    int max_si = (int)mod.shapes.size() - 1;
                    int attempts = std::min((int)mod.shapes.size(), 10);

                    for (int att = 0; att < attempts && !accepted; att++) {
                        int si = (int)((double)rng() / (double)rng.max() * (max_si + 1));
                        si = std::min(si, max_si);
                        int nw = mod.shapes[si].first;
                        int nh = mod.shapes[si].second;
                        if (nw == mod.width && nh == mod.height) continue;

                        if (grid.can_place(mod.x, mod.y, nw, nh)) {
                            mod.width = nw; mod.height = nh;
                            grid.add_rect(mod.x, mod.y, mod.width, mod.height, idx);
                            if (mod.is_valid_shape()) { accepted = true; break; }
                        }

                        int range = std::max(5, std::min(mod.width, mod.height) / 2);
                        for (int t = 0; t < 5 && !accepted; t++) {
                            int px = mod.x + (int)((double)rng() / (double)rng.max() * range * 2) - range;
                            int py = mod.y + (int)((double)rng() / (double)rng.max() * range * 2) - range;
                            px = std::max(0, std::min(px, sol.chip_width - nw));
                            py = std::max(0, std::min(py, sol.chip_height - nh));
                            if (grid.can_place(px, py, nw, nh)) {
                                mod.x = px; mod.y = py;
                                mod.width = nw; mod.height = nh;
                                grid.add_rect(mod.x, mod.y, mod.width, mod.height, idx);
                                if (mod.is_valid_shape()) accepted = true;
                                break;
                            }
                        }
                    }
                }
            }

            gen_cnt++;
            if (accepted) {
                // Validate: reject if any overlap detected (safety net)
                if (!sol.is_valid(sol.chip_width, sol.chip_height)) {
                    sol.restore_all_soft();
                    rebuild_grid();
                    reject_cnt++;
                    accepted = false;
                }
            }

            if (accepted) {
                int new_wl = compute_wirelength();
                double new_cost = cost_function(new_wl);
                double delta_cost = new_cost - current_cost;

                if (delta_cost < 0) {
                    current_cost = new_cost;
                    uphill_cnt++;
                    if (new_cost < best_cost) {
                        best_cost = new_cost;
                        best_wirelength = new_wl;
                        best_soft = sol.soft_modules;
                    }
                } else {
                    double ap = std::exp(-delta_cost / (T + 1e-9));
                    if (dist01(rng) < ap) {
                        current_cost = new_cost;
                        uphill_cnt++;
                    } else {
                        sol.restore_all_soft();
                        rebuild_grid();
                        reject_cnt++;
                    }
                }
            } else {
                sol.restore_all_soft();
                rebuild_grid();
                reject_cnt++;
            }

            total_gen++;
            if (!accepted || (gen_cnt > 0 && uphill_cnt == 0)) total_reject++;

            if (gen_cnt > 100 && gen_cnt > 0 &&
                static_cast<double>(reject_cnt) / gen_cnt > 0.95) break;
        }

        if (total_gen > 200 && static_cast<double>(total_reject) / total_gen > 0.90)
            break;

        T *= T_DECAY;

        if ((int)(std::log(T) / std::log(T_DECAY)) % 20 == 0)
            std::cerr << "  [SA] T=" << (int)T << " wl=" << best_wirelength
                      << " gen=" << total_gen << std::endl;
    }

sa_done:
    sol.soft_modules = best_soft;
    sol.wirelength = best_wirelength;
    rebuild_grid();

    std::cerr << "  [SA] Done. wl=" << best_wirelength
              << " gen=" << total_gen << std::endl;
    return true;
}

void Optimizer::rebuild_grid() {
    grid.itree_x.clear();
    grid.itree_y.clear();
    grid.placed.clear();
    grid.placed_idx.clear();
    for (auto& fm : sol.fixed_modules)
        grid.add_rect(fm.x, fm.y, fm.width, fm.height, -1);
    for (int i = 0; i < (int)sol.soft_modules.size(); i++) {
        const auto& sm = sol.soft_modules[i];
        if (sm.width > 0 && sm.height > 0)
            grid.add_rect(sm.x, sm.y, sm.width, sm.height, i);
    }
}

double Optimizer::cost_function(int wirelength) {
    double area = 0;
    for (const auto& m : sol.soft_modules) area += m.area();
    return 0.05 * area + 0.95 * wirelength;
}

int Optimizer::compute_wirelength() const {
    return HPWL::compute(sol);
}
