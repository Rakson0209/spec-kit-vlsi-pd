#include "evaluator.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>

static std::unordered_map<std::string, const Module*> build_center_map(
    const std::vector<SoftModule*>& soft,
    const std::vector<FixedModule*>& fixed) {
    std::unordered_map<std::string, const Module*> m;
    for (auto* s : soft) m[s->name] = s;
    for (auto* f : fixed) m[f->name] = f;
    return m;
}

int calculate_hpwl(const std::vector<SoftModule*>& soft,
                   const std::vector<FixedModule*>& fixed,
                   const std::vector<Net>& nets) {
    auto centers = build_center_map(soft, fixed);
    int total = 0;
    for (const auto& net : nets) {
        auto it_a = centers.find(net.module_a);
        auto it_b = centers.find(net.module_b);
        if (it_a == centers.end() || it_b == centers.end()) continue;
        int cx1 = it_a->second->center_x();
        int cy1 = it_a->second->center_y();
        int cx2 = it_b->second->center_x();
        int cy2 = it_b->second->center_y();
        total += net.weight * (std::abs(cx1 - cx2) + std::abs(cy1 - cy2));
    }
    return total;
}

static bool overlap(const Module& a, const Module& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

ValidationResult validate(const Chip& chip,
                          const std::vector<SoftModule*>& soft,
                          const std::vector<FixedModule*>& fixed) {
    ValidationResult result;
    result.valid = true;

    for (auto* m : soft) {
        // chip boundary
        if (m->x < 0 || m->y < 0 || m->x + m->w > chip.width || m->y + m->h > chip.height) {
            result.valid = false;
            result.violations.push_back(m->name + " out of chip");
        }
        // area
        if (!m->valid_area()) {
            result.valid = false;
            result.violations.push_back(m->name + " area too small");
        }
        // aspect ratio
        if (!m->valid_aspect_ratio()) {
            result.valid = false;
            result.violations.push_back(m->name + " aspect ratio violation");
        }
    }

    // check overlap between all modules (soft + fixed)
    std::vector<const Module*> all;
    for (auto* s : soft) all.push_back(s);
    for (auto* f : fixed) all.push_back(f);

    for (int i = 0; i < (int)all.size(); i++) {
        for (int j = i + 1; j < (int)all.size(); j++) {
            if (overlap(*all[i], *all[j])) {
                result.valid = false;
                result.violations.push_back(all[i]->name + " overlaps " + all[j]->name);
            }
        }
    }

    return result;
}
