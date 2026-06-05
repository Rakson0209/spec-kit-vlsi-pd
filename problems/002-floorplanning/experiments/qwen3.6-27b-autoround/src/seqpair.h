#ifndef SEQPAIR_H
#define SEQPAIR_H

#include "chip.h"
#include "module.h"
#include "net.h"
#include <vector>
#include <string>

// Sequence-pair encoding: two permutations of module names
struct SequencePair {
    std::vector<std::string> sigma_plus;   // horizontal ordering
    std::vector<std::string> sigma_minus;  // vertical ordering
    int n;

    SequencePair() : n(0) {}
    SequencePair(int nn) : n(nn) {
        sigma_plus.resize(nn);
        sigma_minus.resize(nn);
    }

    // Get position of module name in a sequence (-1 if not found)
    int pos_in(const std::string& name, const std::vector<std::string>& seq) const;

    // Encode from placed modules (sort by center coordinates)
    void encode(const std::vector<SoftModule*>& soft,
                const std::vector<FixedModule*>& fixed);

    // Decode to positions (with area and aspect ratio constraints)
    void decode(const Chip& chip,
                std::vector<SoftModule*>& soft,
                const std::vector<FixedModule*>& fixed);
};

#endif
