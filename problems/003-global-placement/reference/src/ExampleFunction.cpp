#include "ExampleFunction.h"

using namespace std;
// minimize 3*x^2 + 2*x*y + 2*y^2 + 7

ExampleFunction::ExampleFunction(wrapper::Placement &placement) : _placement(placement)
{
    boundryWidth = _placement.boundryRight() - _placement.boundryLeft();
    boundryHeight = _placement.boundryTop() - _placement.boundryBottom();
    numModules = _placement.numModules();
    alpha = boundryWidth / 10;
    binCut = 14;
    Wb = boundryWidth / binCut;
    Hb = boundryHeight / binCut;

    WL = new double[numModules * 4]();
    BinDensity = new double[numModules * 2]();

    numBins = binCut * binCut;
    binArea = Wb * Hb;
    binDensity = (double*)malloc(sizeof(double) * numBins);
    grad = (double*)malloc(sizeof(double) * numModules * 2);

    targetDensity = 0;
	for(int i = 0; i < numModules; ++i) targetDensity += _placement.module(i).area();
	targetDensity = targetDensity / (boundryWidth * boundryHeight);

}

void ExampleFunction::evaluateFG(const vector<double> &x, double &f, vector<double> &g)
{
    fill(g.begin(), g.end(), 0.0);
    f = 0.0;
    memset(WL, 0.0, sizeof(double) * numModules * 4);
    for(int i = 0; i < numModules; ++i)
    {
        WL[4*i] = exp(x[2*i] / alpha);
        WL[4*i+1] = exp(-x[2*i] / alpha);
        WL[4*i+2] = exp(x[2*i + 1] / alpha);
        WL[4*i+3] = exp(-x[2*i + 1] / alpha);
    }

    for(unsigned int i = 0; i <  _placement.numNets(); ++i)
    {
        double sumX1 = 0.0, sumX2 = 0.0, sumY1 = 0.0, sumY2 = 0.0;
        for(unsigned int j = 0; j < _placement.net(i).numPins(); ++j)
        {
            int mID =_placement.net(i).pin(j).moduleId();
            sumX1 += WL[4*mID];
            sumX2 += WL[4*mID+1];
            sumY1 += WL[4*mID+2];
            sumY2 += WL[4*mID+3];
        }
        f += alpha*(log(sumX1) + log(sumX2) + log(sumY1) + log(sumY2));

        for (unsigned int j = 0; j < _placement.net(i).numPins(); ++j)
        {
            int mID =_placement.net(i).pin(j).moduleId();
            g[2*mID] += (alpha * (WL[4*mID] / sumX1 - WL[4*mID+1] / sumX2));
            g[2*mID+1] += (alpha * (WL[4*mID+2] / sumY1 - WL[4*mID+3] / sumY2));
        }    
    }
    WLF = f;
    if (beta == 0) return;

    // Bin Density, bell-shaped smoothing function
    memset(binDensity, 0.0, sizeof(double) * numBins);
    memset(grad, 0.0, sizeof(double) * numModules * 2);
    double Wi = 0.0, Hi = 0.0, c = 0.0;
    double thetaX = 0.0, thetaY = 0.0, dX = 0.0, dY = 0.0, ABSdX = 0.0, ABSdY = 0.0, aX = 0.0, bX = 0.0, aY = 0.0, bY = 0.0;

    for (int a = 0; a < binCut; ++a)
    {
        for (int b = 0; b < binCut; ++b)
        {
            for (int i = 0; i < numModules; ++i)
            {
                Wi = _placement.module(i).width(); Hi = _placement.module(i).height();
                c = _placement.module(i).area()/binArea;
                // dX: center-to-center distance of the block v and the bin b in the x-direction.
                dX = x[2*i] - ((a + 0.5)*Wb + _placement.boundryLeft());
                // distance should be positive
                ABSdX = abs(dX);
                // dY: center-to-center distance of the block v and the bin b in the y-direction.
                dY = x[2*i + 1] - ((b + 0.5)*Hb + _placement.boundryBottom());
                // distance should be positive
                ABSdY = abs(dY);
                aX = 4 / ((Wb + Wi) * (2 * Wb + Wi));
                bX  = 4 / (Wb * (2 * Wb + Wi));
                aY = 4 / ((Hb + Hi) * (2 * Hb + Hi));
                bY  = 4 / (Hb * (2 * Hb + Hi));

                if(ABSdX <= Wi/2.0 + Wb/2.0) thetaX = (1 - aX * ABSdX * ABSdX);
                else if(ABSdX <= Wi/2.0 + Wb) thetaX = (bX * pow(ABSdX - (Wb + Wi / 2), 2 ));
                else thetaX = 0;
                
                if(ABSdY <= Hi/2.0 + Hb/2.0) thetaY = (1 - aY * ABSdY * ABSdY);
                else if(ABSdY <= Hi/2.0 + Hb) thetaY = (bY * pow(ABSdY - (Hb + Hi / 2), 2 ));
                else thetaY = 0;

                binDensity[a + binCut*b] += c * thetaX * thetaY;

                if(ABSdX <= Wi/2.0 + Wb/2.0) grad[2*i] = c * -2 * aX * dX * thetaY;
                else if(ABSdX <= Wi/2.0 + Wb){
                    if(dX >0) grad[2*i] = c * 2 * bX * (dX - (Wb + Wi / 2)) * thetaY;
                    else grad[2*i] = c * 2 * bX * (dX + (Wb + Wi / 2)) * thetaY;
                }
                else grad[2*i] = 0;
                
                if(ABSdY <= Hi/2.0 + Hb/2.0) grad[2*i + 1] = c * -2 * aY * dY * thetaX;
                else if(ABSdY <= Hi/2.0 + Hb){
                    if(dY >0) grad[2*i + 1] = c * 2 * bY * (dY - (Hb + Hi / 2)) * thetaX;
                    else grad[2*i + 1] = c * 2 * bY * (dY + (Hb + Hi / 2)) * thetaX;
                }
                else grad[2*i + 1] = 0;
            }
            f += beta * pow(binDensity[a + binCut*b] - targetDensity , 2);

            for (int i = 0; i < numModules; ++i)
            {
                g[2*i] += 2 * beta * (binDensity[a + binCut*b] - targetDensity) * grad[2*i];
                g[2*i + 1] += 2 * beta * (binDensity[a + binCut*b] - targetDensity) * grad[2*i + 1];
            }
        }
    }
    DF = f - WLF;
}

void ExampleFunction::evaluateF(const vector<double> &x, double &f)
{
    f = 0.0; // objective cost function : LSE wirelength + Bin Density
    memset(WL, 0.0, sizeof(double) * numModules * 4);
    // LSE wirelength
    for(int i = 0; i < numModules; ++i)
    {
        WL[4*i] = exp(x[2*i] / alpha);
        WL[4*i+1] = exp(-x[2*i] / alpha);
        WL[4*i+2] = exp(x[2*i + 1] / alpha);
        WL[4*i+3] = exp(-x[2*i + 1] / alpha);
    }

    for(unsigned int i = 0; i <  _placement.numNets(); ++i)
    {
        double sumX1 = 0.0, sumX2 = 0.0, sumY1 = 0.0, sumY2 = 0.0;
        for(unsigned int j = 0; j < _placement.net(i).numPins(); ++j)
        {
            int mID =_placement.net(i).pin(j).moduleId();
            sumX1 += WL[4*mID];
            sumX2 += WL[4*mID+1];
            sumY1 += WL[4*mID+2];
            sumY2 += WL[4*mID+3];
        }
        f += alpha*(log(sumX1) + log(sumX2) + log(sumY1) + log(sumY2));
    }

    if(beta == 0) return; //first round -> return

    // Bin Density, bell-shaped smoothing function
    memset(binDensity, 0.0, sizeof(double) * numBins);
    double Wi = 0.0, Hi = 0.0, c = 0.0;
    double thetaX = 0.0, thetaY = 0.0, dX = 0.0, dY = 0.0, aX = 0.0, bX = 0.0, aY = 0.0, bY = 0.0, ABSdX = 0.0, ABSdY = 0.0;

    for (int a = 0; a < binCut; ++a){
        for (int b = 0; b < binCut; ++b){
            for (int i = 0; i < numModules; ++i){
                Wi = _placement.module(i).width(); Hi = _placement.module(i).height();
                c = _placement.module(i).area()/binArea;
                // dX: center-to-center distance of the block v and the bin b in the x-direction.
                dX = x[2*i] - ((a + 0.5)*Wb + _placement.boundryLeft());
                // distance should be positive
                ABSdX = abs(dX);
                // dY: center-to-center distance of the block v and the bin b in the y-direction.
                dY = x[2*i + 1] - ((b + 0.5)*Hb + _placement.boundryBottom());
                // distance should be positive
                ABSdY = abs(dY);
                aX = 4 / ((Wb + Wi) * (2 * Wb + Wi));
                bX  = 4 / (Wb * (2 * Wb + Wi));
                aY = 4 / ((Hb + Hi) * (2 * Hb + Hi));
                bY  = 4 / (Hb * (2 * Hb + Hi));

                if(ABSdX <= Wi/2.0 + Wb/2.0) thetaX = (1 - aX * ABSdX * ABSdX);
                else if(ABSdX <= Wi/2.0 + Wb) thetaX = (bX * pow(ABSdX - (Wb + Wi / 2), 2 ));
                else thetaX = 0;
                
                if(ABSdY <= Hi/2.0 + Hb/2.0) thetaY = (1 - aY * ABSdY * ABSdY);
                else if(ABSdY <= Hi/2.0 + Hb) thetaY = (bY * pow(ABSdY - (Hb + Hi / 2), 2 ));
                else thetaY = 0;

                binDensity[a + binCut*b] += c * thetaX * thetaY; 
            }
            f += beta * pow(binDensity[a + binCut*b] - targetDensity , 2);
        }
    }

}

void ExampleFunction::Increase_Lambda(double alpha)
{
    beta += DF * alpha;
}

unsigned ExampleFunction::dimension()
{
    return 2 * numModules;
    // each two dimension represent the X and Y dimensions of each block
}
