#include "GlobalPlacer.h"
#include "ExampleFunction.h"
#include "NumericalOptimizer.h"


GlobalPlacer::GlobalPlacer(wrapper::Placement &placement)
    : _placement(placement)
{
}

void GlobalPlacer::randomPlace(vector<double>& sol)
{
    srand(0);
    double coreWidth = _placement.boundryRight() - _placement.boundryLeft();
    double coreHeight = _placement.boundryTop() - _placement.boundryBottom();
    for (size_t i = 0; i < _placement.numModules(); ++i)
    {
        if (_placement.module(i).isFixed())
            continue;

        double width = _placement.module(i).width();
        double height = _placement.module(i).height();
        double x = rand() % static_cast<int>(coreWidth - width) + _placement.boundryLeft();
        double y = rand() % static_cast<int>(coreHeight - height) + _placement.boundryBottom();
        // center the module
        sol[2*i] = x + width / 2;
        sol[2*i+1] = y + height / 2;
        _placement.module(i).setPosition(x, y);
    }
}

void GlobalPlacer::place()
{
    ///////////////////////////////////////////////////////////////////
    // The following example is only for analytical methods.
    // if you use other methods, you can skip and delete it directly.
    //////////////////////////////////////////////////////////////////

    double start_time;
    start_time = clock();

    ExampleFunction ef(_placement); // require to define the object function and gradient function
    NumericalOptimizer no(ef);

    vector<double> sol(ef.dimension()); // solution vector, size: num_blocks*2
                                        // each 2 variables represent the X and Y dimensions of a block
    randomPlace(sol); // initialize the solution vector

    double boundryTop = _placement.boundryTop();
    double boundryRight = _placement.boundryRight();
    double boundryBottom = _placement.boundryBottom();
    double boundryLeft = _placement.boundryLeft();
           

    double alpha = 60000;

    no.setStepSizeBound((boundryRight-boundryLeft)*10);   // user-specified parameter
    for (int i = 0; i < 5 && ((clock() - start_time) / CLOCKS_PER_SEC <= 500); ++i)
    {

        if(i == 0) ef.beta = 0; // force on WL 
		else if(i == 1) ef.beta = 1; // inorder to get initial beta
		else ef.Increase_Lambda(alpha);

        no.setX(sol);
        if(i == 0) no.setNumIteration(150); 
		else if(i == 1) no.setNumIteration(1);  // iterate one time to get initial lambda
		else no.setNumIteration(30);

        no.solve(); // Conjugate Gradient solver
        for (size_t j = 0; j < _placement.numModules(); ++j)
        {

            double mX = no.x(2 * j), mY = no.x(2 * j + 1), mW = _placement.module(j).width(), mH = _placement.module(j).height();
            // mX is center position of module j
            // mY is center position of module j
            if (!_placement.module(j).isFixed())
            {
                mX = mX + mW/2 > boundryRight ? boundryRight - mW/2 : mX;
                mX = mX - mW/2 < boundryLeft ? boundryLeft + mW/2 : mX;

                mY = mY + mH/2 > boundryTop ? boundryTop - mH/2 : mY;
                mY = mY - mH/2 < boundryBottom ? boundryBottom + mH/2 : mY;
            
            }else{
                mX = _placement.module(j).centerX();
                mY = _placement.module(j).centerY();
            }
            
            if(!_placement.module(j).isFixed()) _placement.module(j).setCenterPosition(mX, mY);;
            
            sol[2*j] = mX;
            sol[2*j+1] = mY;
        }
        
    }

    cout << "Current solution:\n";
    for (unsigned i = 0; i < no.dimension(); i++)
    {
        cout << "x[" << i << "] = " << no.x(i) << "\n";
    }
    cout << "Objective: " << no.objective() << "\n";
    ////////////////////////////////////////////////////////////////

    /* @@@ TODO
     * 1. Understand above example and modify ExampleFunction.cpp to implement the analytical placement
     * 2. You can choose LSE or WA as the wirelength model, the former is easier to calculate the gradient
     * 3. For the bin density model, you could refer to the lecture notes
     * 4. You should first calculate the form of wirelength model and bin density model and the forms of their gradients ON YOUR OWN
     * 5. Replace the value of f in evaluateF() by the form like "f = alpha*WL() + beta*BinDensity()"
     * 6. Replace the form of g[] in evaluateG() by the form like "g = grad(WL()) + grad(BinDensity())"
     * 7. Set the initial vector x in place(), set step size, set #iteration, and call the solver like above example
     * */
}
