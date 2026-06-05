#ifndef PLACER_H
#define PLACER_H

#include "chip.h"
#include "module.h"
#include "net.h"
#include <vector>

// Net-aware initial placement: sort soft modules by net connectivity
// and place larger modules first, prioritizing modules connected by high-weight nets
void initial_placement(const Chip& chip,
                       std::vector<SoftModule*>& soft,
                       const std::vector<FixedModule*>& fixed,
                       const std::vector<Net>& nets);

#endif
