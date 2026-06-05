#ifndef PARSER_H
#define PARSER_H

#include "chip.h"
#include "module.h"
#include "net.h"
#include <vector>
#include <string>

void parse_input(const std::string& path,
                 Chip& chip,
                 std::vector<SoftModule*>& soft_modules,
                 std::vector<FixedModule*>& fixed_modules,
                 std::vector<Net>& nets);

#endif
