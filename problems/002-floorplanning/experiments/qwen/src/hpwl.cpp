#include "hpwl.h"
#include <cmath>

int HPWL::compute(const Solution& sol) {
    int wl = 0;
    for (const auto& net : sol.nets) {
        if (net.idx_a < -1 || net.idx_b < -1) continue;
        const Module* ma = net.idx_a >= 0 ? &sol.soft_modules[net.idx_a]
                                          : &sol.fixed_modules[-1 - net.idx_a];
        const Module* mb = net.idx_b >= 0 ? &sol.soft_modules[net.idx_b]
                                          : &sol.fixed_modules[-1 - net.idx_b];
        if (!ma || !mb) continue;
        int cx1 = ma->center_x(), cy1 = ma->center_y();
        int cx2 = mb->center_x(), cy2 = mb->center_y();
        wl += net.weight * (std::abs(cx1 - cx2) + std::abs(cy1 - cy2));
    }
    return wl;
}
