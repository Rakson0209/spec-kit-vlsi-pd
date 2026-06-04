#include "placement.h"
#include <algorithm>
#include <iostream>

bool Placer::place(Solution& sol, Grid& grid, std::mt19937& rng) {
    // Sort by area descending
    std::vector<int> order(sol.soft_modules.size());
    for (int i = 0; i < (int)order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(),
        [&](int a, int b) { return sol.soft_modules[a].min_area > sol.soft_modules[b].min_area; });

    grid.init(sol.chip_width, sol.chip_height);
    for (auto& fm : sol.fixed_modules)
        grid.add_rect(fm.x, fm.y, fm.width, fm.height, -1);

    int max_retries = 200;
    for (int attempt = 0; attempt < max_retries; attempt++) {
        grid.itree_x.clear();
        grid.itree_y.clear();
        grid.placed.clear();
        grid.placed_idx.clear();
        for (auto& fm : sol.fixed_modules)
            grid.add_rect(fm.x, fm.y, fm.width, fm.height, -1);
        for (auto& sm : sol.soft_modules) {
            sm.x = 0; sm.y = 0; sm.width = 0; sm.height = 0;
        }

        bool all_placed = true;
        for (int oi = 0; oi < (int)order.size(); oi++) {
            if (!try_place_one(sol, grid, order[oi], rng)) {
                all_placed = false;
                break;
            }
        }
        if (all_placed) return true;
    }

    std::cerr << "Warning: could not place all modules" << std::endl;
    return false;
}

bool Placer::try_place_one(Solution& sol, Grid& grid, int idx, std::mt19937& rng) {
    Module& mod = sol.soft_modules[idx];
    if (mod.shapes.empty()) mod.generate_shapes();

    std::uniform_real_distribution<double> ur01(0.0, 1.0);

    for (int att = 0; att < 10000; att++) {
        for (auto& shape : mod.shapes) {
            int w = shape.first;
            int h = shape.second;
            int max_x = std::max(0, sol.chip_width - w);
            int max_y = std::max(0, sol.chip_height - h);
            if (max_x < 0 || max_y < 0) continue;

            int x = (int)(ur01(rng) * (max_x + 1));
            int y = (int)(ur01(rng) * (max_y + 1));
            x = std::min(x, max_x);
            y = std::min(y, max_y);

            if (grid.can_place(x, y, w, h)) {
                auto c = grid.compact_up_left(x, y, w, h);
                if (grid.can_place(c.first, c.second, w, h)) {
                    x = c.first; y = c.second;
                }
                // Double-check no overlap
                if (!grid.has_overlap(x, y, w, h)) {
                    mod.x = x; mod.y = y;
                    mod.width = w; mod.height = h;
                    grid.add_rect(x, y, w, h, idx);
                    return true;
                }
            }
        }
    }
    return false;
}
