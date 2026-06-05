#ifndef SA_H
#define SA_H

#include "chip.h"
#include "module.h"
#include "net.h"
#include "evaluator.h"
#include <vector>
#include <ctime>

void run_sa(const Chip& chip,
            std::vector<SoftModule*>& soft,
            const std::vector<FixedModule*>& fixed,
            const std::vector<Net>& nets,
            double max_seconds);

#endif
