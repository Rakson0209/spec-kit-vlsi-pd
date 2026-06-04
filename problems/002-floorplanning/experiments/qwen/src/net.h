#pragma once

#include <string>

struct Module;

struct Net {
    std::string name_a;
    std::string name_b;
    int weight;

    // Resolved indices (set after all modules are loaded)
    int idx_a;  // -1 = fixed module, >= 0 = soft module index
    int idx_b;

    Net() : weight(0), idx_a(-2), idx_b(-2) {}
    Net(const std::string& a, const std::string& b, int w)
        : name_a(a), name_b(b), weight(w), idx_a(-2), idx_b(-2) {}
};
