#pragma once

#include "solution.h"
#include <string>

class OutputWriter {
public:
    void write(const Solution& sol, const std::string& filename);
};
