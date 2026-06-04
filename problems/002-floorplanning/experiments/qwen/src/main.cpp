#include <iostream>
#include <string>
#include <cstdlib>
#include <random>
#include <chrono>

#include "parser.h"
#include "placement.h"
#include "optimizer.h"
#include "output.h"
#include "hpwl.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.txt> <output.floorplan> [seed]" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];
    unsigned int seed = 42;
    if (argc >= 4)
        seed = static_cast<unsigned int>(std::stoi(argv[3]));

    auto t_start = std::chrono::steady_clock::now();
    std::mt19937 rng(seed);
    std::cerr << "Seed: " << seed << std::endl;

    // Parse
    Parser parser;
    Solution sol;
    try {
        sol = parser.parse(input_file);
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    }

    std::cerr << "Chip: " << sol.chip_width << " x " << sol.chip_height << std::endl;
    std::cerr << "Soft: " << sol.soft_modules.size()
              << " Fixed: " << sol.fixed_modules.size()
              << " Nets: " << sol.nets.size() << std::endl;

    // Grid (rectangle-based)
    Grid grid(sol.chip_width, sol.chip_height);

    // Place
    std::cerr << "Placing..." << std::endl;
    Placer placer;
    bool placed = placer.place(sol, grid, rng);

    sol.wirelength = HPWL::compute(sol);
    std::cerr << "Initial wl: " << sol.wirelength << std::endl;

    bool valid = sol.is_valid(sol.chip_width, sol.chip_height);
    std::cerr << "Valid: " << (valid ? "yes" : "no") << std::endl;

    // Save initial placement as fallback
    std::vector<Module> initial_soft = sol.soft_modules;
    Grid initial_grid = grid;
    int initial_wl = sol.wirelength;

    // Optimize
    if (sol.soft_modules.size() > 1) {
        std::cerr << "SA optimize (540s)..." << std::endl;
        Optimizer opt(sol, grid, rng);
        opt.run(540.0);
        sol.wirelength = HPWL::compute(sol);
        valid = sol.is_valid(sol.chip_width, sol.chip_height);
        std::cerr << "Final wl: " << sol.wirelength
                  << " valid: " << (valid ? "yes" : "no") << std::endl;

        // Fallback to initial placement if SA produced invalid result
        if (!valid) {
            std::cerr << "WARNING: SA produced invalid result, using initial placement" << std::endl;
            sol.soft_modules = initial_soft;
            grid = initial_grid;
            sol.wirelength = initial_wl;
        }
    }

    // Output
    OutputWriter writer;
    try {
        writer.write(sol, output_file);
    } catch (const std::exception& e) {
        std::cerr << "Output error: " << e.what() << std::endl;
        return 1;
    }

    auto t_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_end - t_start);
    std::cerr << "Time: " << elapsed.count() << "s" << std::endl;
    return 0;
}
