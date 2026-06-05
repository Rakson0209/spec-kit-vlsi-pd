#include "placer.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <vector>
#include <numeric>

static bool overlaps_with(int x, int y, int w, int h,
                          const std::vector<SoftModule*>& placed_soft,
                          const std::vector<FixedModule*>& fixed) {
    for (const auto* s : placed_soft) {
        if (x < s->x + s->w && s->x < x + w && y < s->y + s->h && s->y < y + h)
            return true;
    }
    for (const auto* f : fixed) {
        if (x < f->x + f->w && f->x < x + w && y < f->y + f->h && f->y < y + h)
            return true;
    }
    return false;
}

// Find the lowest y at a given x where the module fits, jumping over obstacles
static int find_lowest_y(int x, int w, int h, int cw, int ch,
                         const std::vector<SoftModule*>& placed,
                         const std::vector<FixedModule*>& fixed) {
    int y = 0;
    int iterations = 0;
    const int max_iter = (int)placed.size() + (int)fixed.size() + 1;

    while (y + h <= ch && iterations <= max_iter) {
        iterations++;
        if (!overlaps_with(x, y, w, h, placed, fixed))
            return y;
        // Jump to the top edge of the first module that blocks us
        int jump_to = ch + 1; // fallback: no jump found
        for (const auto* s : placed) {
            if (s->x < x + w && x < s->x + s->w && // x-overlap
                s->y <= y && s->y + s->h > y) {    // y-overlap at current y
                if (s->y + s->h < jump_to)
                    jump_to = s->y + s->h;
            }
        }
        for (const auto* f : fixed) {
            if (f->x < x + w && x < f->x + f->w &&
                f->y <= y && f->y + f->h > y) {
                if (f->y + f->h < jump_to)
                    jump_to = f->y + f->h;
            }
        }
        if (jump_to == ch + 1) break; // no blocking module found, should not happen
        y = jump_to;
    }
    return ch + 1; // not found
}

void initial_placement(const Chip& chip,
                       std::vector<SoftModule*>& soft,
                       const std::vector<FixedModule*>& fixed,
                       const std::vector<Net>& nets) {
    int cw = chip.width;
    int ch = chip.height;

    // Sort by min_area descending (place large modules first)
    std::sort(soft.begin(), soft.end(),
              [](SoftModule* a, SoftModule* b) { return a->min_area > b->min_area; });

    std::vector<SoftModule*> placed;

    for (auto* mod : soft) {
        bool placed_mod = false;

        // Sort shapes: prefer more square shapes
        std::vector<std::pair<int,int>> shapes = mod->shapes;
        std::sort(shapes.begin(), shapes.end(), [](const std::pair<int,int>& a, const std::pair<int,int>& b) {
            double ra = (double)a.first / a.second;
            double rb = (double)b.first / b.second;
            return std::abs(ra - 1.0) < std::abs(rb - 1.0);
        });

        // Collect x candidates: 0, left/right edge of each placed and fixed module
        std::vector<int> x_cands;
        x_cands.reserve((int)placed.size() * 2 + (int)fixed.size() * 2 + 1);
        x_cands.push_back(0);
        for (const auto* s : placed) {
            x_cands.push_back(s->x);
            x_cands.push_back(s->x + s->w);
        }
        for (const auto* f : fixed) {
            x_cands.push_back(f->x);
            x_cands.push_back(f->x + f->w);
        }
        std::sort(x_cands.begin(), x_cands.end());
        x_cands.erase(std::unique(x_cands.begin(), x_cands.end()), x_cands.end());

        // Also add intermediate positions: between candidate x and x+w, check if there's room
        // For each candidate x, find the lowest valid y
        int best_x = -1, best_y = -1, best_y_val = ch + 1;

        for (int xc : x_cands) {
            for (const auto& shape : shapes) {
                int w = shape.first;
                int h = shape.second;

                if (w * h < mod->min_area) continue;
                if (w <= 0 || h <= 0) continue;
                double ratio = (double)h / w;
                if (ratio < 0.5 - 1e-9 || ratio > 2.0 + 1e-9) continue;
                if (xc + w > cw) continue;

                int y = find_lowest_y(xc, w, h, cw, ch, placed, fixed);
                if (y <= ch - h && y < best_y_val) {
                    best_y_val = y;
                    best_x = xc;
                    best_y = y;
                    mod->w = w; mod->h = h;
                }
            }
        }

        if (best_x >= 0) {
            mod->x = best_x;
            mod->y = best_y;
            placed.push_back(mod);
            placed_mod = true;
        }

        // Fallback: scan with step
        if (!placed_mod) {
            int fw = (int)std::sqrt(mod->min_area);
            int fh = (int)std::ceil((double)mod->min_area / fw);
            int step = std::max(1, std::max(cw, ch) / 200);
            for (int y = 0; y + fh <= ch && !placed_mod; y += step) {
                for (int x = 0; x + fw <= cw && !placed_mod; x += step) {
                    int cy = y;
                    while (cy > 0 && !overlaps_with(x, cy - 1, fw, fh, placed, fixed)) cy--;
                    if (!overlaps_with(x, cy, fw, fh, placed, fixed)) {
                        mod->x = x; mod->y = cy; mod->w = fw; mod->h = fh;
                        placed.push_back(mod);
                        placed_mod = true;
                    }
                }
            }
        }

        if (!placed_mod) {
            fprintf(stderr, "Warning: could not place %s, placing at origin\n", mod->name.c_str());
            mod->x = 0; mod->y = 0;
            int w = (int)std::sqrt(mod->min_area);
            int h = (int)std::ceil((double)mod->min_area / w);
            mod->w = w; mod->h = h;
            placed.push_back(mod);
        }
    }
}
