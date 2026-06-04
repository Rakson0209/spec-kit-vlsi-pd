#pragma once

#include "solution.h"
#include "grid.h"
#include <vector>
#include <random>

class Placer {
public:
    bool place(Solution& sol, Grid& grid, std::mt19937& rng);
private:
    bool try_place_one(Solution& sol, Grid& grid, int idx, std::mt19937& rng);
};
