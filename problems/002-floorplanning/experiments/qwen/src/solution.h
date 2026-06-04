#pragma once

#include "module.h"
#include "net.h"
#include "grid.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <unordered_map>

struct Solution {
    std::vector<Module> soft_modules;
    std::vector<Module> fixed_modules;
    std::vector<Net> nets;
    int chip_width;
    int chip_height;
    int wirelength;

    Solution() : chip_width(0), chip_height(0), wirelength(0) {}

    // Get module pointer by net index
    const Module* get_module(int idx) const {
        if (idx >= 0) return idx < (int)soft_modules.size() ? &soft_modules[idx] : nullptr;
        if (idx <= -1) { int fi = -1 - idx; return fi < (int)fixed_modules.size() ? &fixed_modules[fi] : nullptr; }
        return nullptr;
    }

    Module* get_module_mut(int idx) {
        if (idx >= 0) return idx < (int)soft_modules.size() ? &soft_modules[idx] : nullptr;
        if (idx <= -1) { int fi = -1 - idx; return fi < (int)fixed_modules.size() ? &fixed_modules[fi] : nullptr; }
        return nullptr;
    }

    int compute_wirelength() const {
        int wl = 0;
        for (const auto& net : nets) {
            const Module* ma = get_module(net.idx_a);
            const Module* mb = get_module(net.idx_b);
            if (!ma || !mb) continue;
            wl += net.weight * (std::abs(ma->center_x() - mb->center_x()) +
                                std::abs(ma->center_y() - mb->center_y()));
        }
        return wl;
    }

    bool is_valid(int chip_w, int chip_h) const {
        for (const auto& m : soft_modules) {
            if (m.x < 0 || m.y < 0 || m.x + m.width > chip_w || m.y + m.height > chip_h)
                return false;
            if (m.width * m.height < m.min_area) return false;
            if (m.width <= 0 || m.height <= 0) return false;
            float ratio = static_cast<float>(m.height) / m.width;
            if (ratio < 0.5f - 1e-9f || ratio > 2.0f + 1e-9f) return false;
        }
        // Check overlaps between all module pairs
        std::vector<Rect> all;
        for (const auto& m : fixed_modules)
            all.push_back(Rect(m.x, m.y, m.width, m.height));
        for (const auto& m : soft_modules)
            all.push_back(Rect(m.x, m.y, m.width, m.height));
        for (size_t i = 0; i < all.size(); i++)
            for (size_t j = i + 1; j < all.size(); j++)
                if (all[i].overlaps(all[j])) return false;
        return true;
    }

    void backup_all_soft() {
        for (auto& m : soft_modules) m.backup();
    }

    void restore_all_soft() {
        for (auto& m : soft_modules) m.restore();
    }

    std::unordered_map<std::string, Module*> build_module_map() {
        std::unordered_map<std::string, Module*> map;
        for (auto& m : fixed_modules) map[m.name] = &m;
        for (auto& m : soft_modules) map[m.name] = &m;
        return map;
    }
};
