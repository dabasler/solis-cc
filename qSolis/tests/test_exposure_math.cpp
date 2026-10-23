#include "SOLISExposureMath.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
bool near(double a,double b,double tol,const char* label){if(std::abs(a-b)<=tol)return true;std::cerr<<label<<": "<<a<<" != "<<b<<'\n';return false;}
}
int main()
{
    using namespace solis::exposure_math;
    bool ok=true;
    ok &= near(boundedFraction(1,1),1,1e-12,"all visible");
    ok &= near(boundedFraction(0,1),0,1e-12,"none visible");
    ok &= near(boundedFraction(0.5,1),0.5,1e-12,"half visible");
    bool valid=false;
    ok &= near(radiativeFraction(50,100,&valid),0.5,1e-12,"rei half") && valid;
    const double bad=radiativeFraction(1,0,&valid); ok &= !valid && std::isnan(bad);
    valid=false; ok &= near(radiativeFraction(1+5e-10,1,&valid),1,1e-12,"tiny clamp") && valid;
    valid=true; const double over=radiativeFraction(1.01,1,&valid); ok &= !valid && over>1;
    std::vector<double> v{0,0.5,1}; std::vector<double> w{1,2,1};
    ok &= near(weightedMean(v,w),0.5,1e-12,"weighted mean");
    ok &= near(weightedQuantile(v,w,0.5),0.5,1e-12,"weighted median");
    return ok?0:1;
}
