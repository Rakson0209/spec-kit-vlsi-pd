#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "chip.h"
#include "module.h"
#include "net.h"
#include <vector>
#include <string>

struct ValidationResult {
    bool valid;
    std::vector<std::string> violations;
};

int calculate_hpwl(const std::vector<SoftModule*>& soft,
                   const std::vector<FixedModule*>& fixed,
                   const std::vector<Net>& nets);

ValidationResult validate(const Chip& chip,
                          const std::vector<SoftModule*>& soft,
                          const std::vector<FixedModule*>& fixed);

#endif
