#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

#include "chip.h"
#include "module.h"
#include "net.h"
#include "parser.h"
#include "evaluator.h"
#include "placer.h"
#include "sa.h"

static void write_output(const std::string& path,
                         const std::vector<SoftModule*>& soft,
                         int wirelength) {
    std::ofstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "Error: cannot write %s\n", path.c_str());
        exit(1);
    }

    f << "Wirelength " << wirelength << "\n";
    f << "\n";
    f << "NumSoftModules " << soft.size() << "\n";
    for (auto* s : soft) {
        f << s->name << " " << s->x << " " << s->y << " " << s->w << " " << s->h << "\n";
    }
    f.close();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.txt> <output.floorplan>\n", argv[0]);
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    srand(time(nullptr));

    // Parse input
    Chip chip;
    std::vector<SoftModule*> soft;
    std::vector<FixedModule*> fixed;
    std::vector<Net> nets;

    parse_input(input_path, chip, soft, fixed, nets);

    fprintf(stderr, "Parsed: chip=%dx%d, soft=%zu, fixed=%zu, nets=%zu\n",
            chip.width, chip.height, soft.size(), fixed.size(), nets.size());

    auto start = std::chrono::steady_clock::now();

    // Phase 1: Initial placement
    initial_placement(chip, soft, fixed, nets);

    int init_hpwl = calculate_hpwl(soft, fixed, nets);
    auto init_vr = validate(chip, soft, fixed);
    fprintf(stderr, "Initial placement: HPWL=%d, valid=%s\n",
            init_hpwl, init_vr.valid ? "yes" : "no");
    if (!init_vr.valid) {
        for (auto& v : init_vr.violations)
            fprintf(stderr, "  violation: %s\n", v.c_str());
    }

    // Phase 2: SA optimization
    // Time budget: 590s for SA, leave 10s margin
    double time_budget = 590.0;
    run_sa(chip, soft, fixed, nets, time_budget);

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    // Final evaluation
    int final_hpwl = calculate_hpwl(soft, fixed, nets);
    auto final_vr = validate(chip, soft, fixed);

    fprintf(stderr, "Final: HPWL=%d, valid=%s, time=%.2fs\n",
            final_hpwl, final_vr.valid ? "yes" : "no", elapsed);
    if (!final_vr.valid) {
        for (auto& v : final_vr.violations)
            fprintf(stderr, "  violation: %s\n", v.c_str());
    }

    // Write output
    write_output(output_path, soft, final_hpwl);

    // Cleanup
    for (auto* s : soft) delete s;
    for (auto* f : fixed) delete f;

    return 0;
}
