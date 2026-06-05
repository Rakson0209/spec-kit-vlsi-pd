#include "parser.h"
#include "chip.h"
#include "module.h"
#include "net.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string word;
    while (ss >> word) out.push_back(word);
    return out;
}

void parse_input(const std::string& path,
                 Chip& chip,
                 std::vector<SoftModule*>& soft_modules,
                 std::vector<FixedModule*>& fixed_modules,
                 std::vector<Net>& nets) {
    std::ifstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "Error: cannot open %s\n", path.c_str());
        exit(1);
    }

    std::string line;

    // ChipSize W H
    std::getline(f, line);
    while (line.empty()) std::getline(f, line);
    auto v = tokenize(line);
    chip.width = std::stoi(v[1]);
    chip.height = std::stoi(v[2]);

    // blank line
    std::getline(f, line);

    // NumSoftModules n
    std::getline(f, line);
    v = tokenize(line);
    int ns = std::stoi(v[1]);
    for (int i = 0; i < ns; i++) {
        std::getline(f, line);
        v = tokenize(line);
        soft_modules.push_back(new SoftModule(v[1], std::stoi(v[2])));
    }

    // blank line
    std::getline(f, line);

    // NumFixedModules n
    std::getline(f, line);
    v = tokenize(line);
    int nf = std::stoi(v[1]);
    for (int i = 0; i < nf; i++) {
        std::getline(f, line);
        v = tokenize(line);
        fixed_modules.push_back(new FixedModule(v[1], std::stoi(v[2]), std::stoi(v[3]),
                                                 std::stoi(v[4]), std::stoi(v[5])));
    }

    // blank line
    std::getline(f, line);

    // NumNets n
    std::getline(f, line);
    v = tokenize(line);
    int nn = std::stoi(v[1]);
    for (int i = 0; i < nn; i++) {
        std::getline(f, line);
        v = tokenize(line);
        Net net;
        net.module_a = v[1];
        net.module_b = v[2];
        net.weight = std::stoi(v[3]);
        nets.push_back(net);
    }

    f.close();
}
