#include "SOLISCrownExposureMath.h"
#include "SOLISExposureMath.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
bool near(double actual, double expected, double tolerance, const char* label)
{
    if (std::abs(actual - expected) <= tolerance)
        return true;
    std::cerr << label << ": actual=" << actual << " expected=" << expected << '\n';
    return false;
}
}

int main()
{
    using solis::crown_math::VoxelAggregation;
    using solis::crown_math::aggregateOccupiedVoxels;
    using solis::exposure_math::weightedMean;

    bool ok = true;

    ok &= near(solis::crown_math::automaticVoxelSizeFromDiagonal(20.0), 0.2, 1.0e-12,
               "automatic voxel size");
    ok &= near(solis::crown_math::automaticVoxelSizeFromDiagonal(0.0), 1.0, 1.0e-12,
               "degenerate automatic voxel size");

    std::vector<double> meshWeights(4, 0.0);
    ok &= solis::crown_math::addTriangleAreaWeight(meshWeights, 0, 1, 2, 3.0);
    ok &= solis::crown_math::addTriangleAreaWeight(meshWeights, 0, 2, 3, 3.0);
    ok &= near(meshWeights[0], 2.0, 1.0e-12, "mesh vertex area weight 0");
    ok &= near(meshWeights[1], 1.0, 1.0e-12, "mesh vertex area weight 1");
    ok &= near(meshWeights[2], 2.0, 1.0e-12, "mesh vertex area weight 2");
    ok &= near(meshWeights[3], 1.0, 1.0e-12, "mesh vertex area weight 3");

    // Two occupied voxels: the first is deliberately oversampled. Equal voxel
    // weighting must produce (0.2 + 0.8)/2 = 0.5 rather than a point-density mean.
    std::vector<std::array<double, 3>> points{
        {0.1, 0.1, 0.1},
        {0.2, 0.1, 0.1},
        {0.3, 0.1, 0.1},
        {1.1, 0.1, 0.1}};
    std::vector<double> rei{0.2, 0.2, 0.2, 0.8};
    std::vector<double> sky{0.3, 0.3, 0.3, 0.9};
    std::vector<double> sun{0.4, 0.4, 0.4, 1.0};

    auto getter = [&points](std::size_t i) { return points[i]; };
    VoxelAggregation aggregation;
    ok &= aggregateOccupiedVoxels(points.size(),
                                  getter,
                                  &sky,
                                  &sun,
                                  rei,
                                  {0.0, 0.0, 0.0},
                                  1.0,
                                  aggregation);
    ok &= aggregation.radiative.size() == 2;
    ok &= aggregation.occupiedVoxelCount == 2;

    const std::vector<double> equalWeights(aggregation.radiative.size(), 1.0);
    ok &= near(weightedMean(aggregation.radiative, equalWeights), 0.5, 1.0e-12,
               "voxel-weighted CEI");
    ok &= near(weightedMean(aggregation.sky, equalWeights), 0.6, 1.0e-12,
               "voxel-weighted sky mean");
    ok &= near(weightedMean(aggregation.sun, equalWeights), 0.7, 1.0e-12,
               "voxel-weighted sun mean");

    // Duplicate the dense first-voxel samples many more times. CEI must stay unchanged.
    for (int i = 0; i < 20; ++i)
    {
        points.push_back({0.25, 0.2, 0.2});
        rei.push_back(0.2);
        sky.push_back(0.3);
        sun.push_back(0.4);
    }

    VoxelAggregation duplicated;
    ok &= aggregateOccupiedVoxels(points.size(),
                                  getter,
                                  &sky,
                                  &sun,
                                  rei,
                                  {0.0, 0.0, 0.0},
                                  1.0,
                                  duplicated);
    const std::vector<double> duplicatedWeights(duplicated.radiative.size(), 1.0);
    ok &= near(weightedMean(duplicated.radiative, duplicatedWeights), 0.5, 1.0e-12,
               "point-density invariant CEI");

    return ok ? 0 : 1;
}
