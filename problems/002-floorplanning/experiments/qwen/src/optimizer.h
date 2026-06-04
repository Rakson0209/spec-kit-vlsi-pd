#pragma once

#include "solution.h"
#include "grid.h"
#include <random>
#include <vector>
#include <ctime>

class Optimizer {
public:
    Optimizer(Solution& sol, Grid& grid, std::mt19937& rng);
    bool run(double timeout_secs = 540.0);
    const std::vector<Module>& get_best() const { return best_soft; }

private:
    Solution& sol;
    Grid& grid;
    std::mt19937& rng;
    std::vector<Module> best_soft;
    int best_wirelength;
    double best_cost;
    clock_t start_time;
    double timeout_secs_val;

    double cost_function(int wirelength);
    int compute_wirelength() const;
    void rebuild_grid();
};
