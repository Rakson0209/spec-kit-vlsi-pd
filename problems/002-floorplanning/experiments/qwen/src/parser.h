#pragma once

#include "solution.h"
#include <string>

class Parser {
public:
    Solution parse(const std::string& filename);
private:
    std::vector<std::string> tokenize(const std::string& line);
};
