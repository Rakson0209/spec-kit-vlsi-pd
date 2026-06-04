#ifndef GLOBALPLACER_H
#define GLOBALPLACER_H

#include "Wrapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <time.h>
#include <algorithm>
#include <cmath>

using namespace std;

class GlobalPlacer
{
public:
    GlobalPlacer(wrapper::Placement &placement);

    void randomPlace(vector<double>& sol); // An example of random placement implemented by TA
    void netFirstPlace(vector<double>& sol, unsigned seed);
    void place();

private:
    wrapper::Placement &_placement;
};

#endif // GLOBALPLACER_H
