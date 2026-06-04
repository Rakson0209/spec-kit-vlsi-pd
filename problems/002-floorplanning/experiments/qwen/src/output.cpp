#include "output.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void OutputWriter::write(const Solution& sol, const std::string& filename) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        throw std::runtime_error("Cannot open output file: " + filename);
    }

    outfile << "Wirelength " << sol.wirelength << std::endl;
    outfile << std::endl;
    outfile << "NumSoftModules " << sol.soft_modules.size() << std::endl;
    for (const auto& m : sol.soft_modules) {
        outfile << m.name << " " << m.x << " " << m.y << " " << m.width << " " << m.height << std::endl;
    }

    outfile.close();
}
