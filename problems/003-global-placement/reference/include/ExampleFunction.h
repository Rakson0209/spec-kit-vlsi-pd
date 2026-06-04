#ifndef EXAMPLEFUNCTION_H
#define EXAMPLEFUNCTION_H

#include "NumericalOptimizerInterface.h"
#include "Wrapper.hpp"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>

class ExampleFunction : public NumericalOptimizerInterface
{
public:
    ExampleFunction(wrapper::Placement &placement);
    wrapper::Placement &_placement;

    double boundryWidth, boundryHeight, Wb, Hb, binArea, alpha, beta, targetDensity, WLF, DF;
    int numModules, binCut, numBins;
    double *WL, *BinDensity, *binDensity, *grad;

    void evaluateFG(const vector<double> &x, double &f, vector<double> &g);
    void evaluateF(const vector<double> &x, double &f);
    unsigned dimension();
    void Increase_Lambda(double rand);
    
};

#endif // EXAMPLEFUNCTION_H
