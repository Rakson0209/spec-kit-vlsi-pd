#include "parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <stdexcept>

std::vector<std::string> Parser::tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string word;
    while (ss >> word) out.push_back(word);
    return out;
}

Solution Parser::parse(const std::string& filename) {
    Solution sol;
    std::ifstream infile(filename);
    if (!infile.is_open())
        throw std::runtime_error("Cannot open: " + filename);

    std::unordered_map<std::string, int> soft_map;  // name -> index in soft_modules
    std::unordered_map<std::string, int> fixed_map; // name -> index in fixed_modules
    std::string line;

    // ChipSize
    std::getline(infile, line);
    while (line.find_first_not_of(" \t\r\n") == std::string::npos)
        std::getline(infile, line);
    {
        auto t = tokenize(line);
        sol.chip_width = std::stoi(t[1]);
        sol.chip_height = std::stoi(t[2]);
    }

    // Soft modules
    std::getline(infile, line);
    while (line.find_first_not_of(" \t\r\n") == std::string::npos)
        std::getline(infile, line);
    {
        auto t = tokenize(line);
        int n = std::stoi(t[1]);
        sol.soft_modules.reserve(n);
        for (int i = 0; i < n; i++) {
            std::getline(infile, line);
            auto t = tokenize(line);
            Module m(t[1], std::stoi(t[2]));
            m.generate_shapes();
            soft_map[t[1]] = i;
            sol.soft_modules.push_back(m);
        }
    }

    // Fixed modules
    std::getline(infile, line);
    while (line.find_first_not_of(" \t\r\n") == std::string::npos)
        std::getline(infile, line);
    {
        auto t = tokenize(line);
        int n = std::stoi(t[1]);
        sol.fixed_modules.reserve(n);
        for (int i = 0; i < n; i++) {
            std::getline(infile, line);
            auto t = tokenize(line);
            Module m(t[1], std::stoi(t[2]), std::stoi(t[3]),
                     std::stoi(t[4]), std::stoi(t[5]));
            fixed_map[t[1]] = i;
            sol.fixed_modules.push_back(m);
        }
    }

    // Nets
    std::getline(infile, line);
    while (line.find_first_not_of(" \t\r\n") == std::string::npos)
        std::getline(infile, line);
    {
        auto t = tokenize(line);
        int n = std::stoi(t[1]);
        sol.nets.reserve(n);
        for (int i = 0; i < n; i++) {
            std::getline(infile, line);
            auto t = tokenize(line);
            sol.nets.emplace_back(t[1], t[2], std::stoi(t[3]));
        }
    }

    infile.close();

    // Resolve net indices
    for (auto& net : sol.nets) {
        if (soft_map.count(net.name_a))
            net.idx_a = soft_map[net.name_a];
        else if (fixed_map.count(net.name_a))
            net.idx_a = -1 - fixed_map[net.name_a];  // negative offset for fixed
        else
            net.idx_a = -2;  // not found

        if (soft_map.count(net.name_b))
            net.idx_b = soft_map[net.name_b];
        else if (fixed_map.count(net.name_b))
            net.idx_b = -1 - fixed_map[net.name_b];
        else
            net.idx_b = -2;
    }

    return sol;
}
