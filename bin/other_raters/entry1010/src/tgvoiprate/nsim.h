#pragma once

#include "vector_of_columns.h"
#include "spectorgram.h"

#include <iostream>

namespace tgvoiprate
{

int sign(double val)
{
    return (0.0 < val) - (val < 0.0);
}

double NSIM(VectorOfColumns& r, VectorOfColumns& d)
{
    assert(r.Length() == d.Length());
    assert(r.RowsCount() == d.RowsCount());

    double L = 160;
    double k[] = {0.1, 0.3};
    double c1 = pow(k[0] * L, 2);
    double c2 = pow(k[1] * L, 2) / 2;

    VectorOfColumns gaussianWindow({0.0113, 0.0838, 0.0113, 0.0838, 0.6193, 0.0838, 0.0113, 0.0838, 0.0113}, 3);

    auto mu_r = r.Convolve(gaussianWindow);
    auto mu_d = d.Convolve(gaussianWindow);

    auto mu_r_sq = mu_r * mu_r;
    auto mu_d_sq = mu_d * mu_d;
    auto mu_r_mu_d = mu_r * mu_d;

    auto sigma_r = ((r * r).Convolve(gaussianWindow) - mu_r_sq)
        .ForEach([](double& x){ x = sign(x) * sqrt(abs(x)); });

    auto sigma_d = ((d * d).Convolve(gaussianWindow) - mu_d_sq)
        .ForEach([](double& x){ x = sign(x) * sqrt(abs(x)); });

    auto sigma_r_d = (r * d).Convolve(gaussianWindow) - mu_r_mu_d;

    auto L_r_d = mu_r_mu_d.ForEach([c1](double& x){ x = 2 * x + c1; })
        / (mu_r_sq + mu_d_sq).ForEach([c1](double& x){ x = x + c1; });

    auto S_r_d = sigma_r_d.ForEach([c2](double& x){ x = x + c2; })
        / (sigma_r * sigma_d).ForEach([c2](double& x){ x = x + c2; });

    return (L_r_d + S_r_d).Mean();
}

}
