#pragma once

#include "SOLISExposure.h"

#include <ccGenericMesh.h>
#include <ccPointCloud.h>

#include <QString>

#include <limits>

#include <vector>

namespace solis
{
struct CrownExposureSummary
{
    double cei = std::numeric_limits<double>::quiet_NaN();
    double skyMean = std::numeric_limits<double>::quiet_NaN();
    double sunMean = std::numeric_limits<double>::quiet_NaN();
    double radiativeMean = std::numeric_limits<double>::quiet_NaN();
    double reiP10 = std::numeric_limits<double>::quiet_NaN();
    double reiP50 = std::numeric_limits<double>::quiet_NaN();
    double reiP90 = std::numeric_limits<double>::quiet_NaN();
    double exposedFraction = 0.0;
    double threshold = 0.75;
    QString weightingMethod;
    double voxelSize = 0.0;
    unsigned occupiedVoxelCount = 0;
    double representedSurfaceArea = 0.0;
    unsigned pointOrVertexCount = 0;
};

class CrownExposure
{
public:
    static double AutomaticVoxelSize(const ccPointCloud* cloud);

    static bool Summarize(const ccPointCloud* cloud,
                          const ccGenericMesh* mesh,
                          const ExposureResult& exposure,
                          const ExposureOptions& options,
                          CrownExposureSummary& summary,
                          QString* errorMessage = nullptr);

    static double WeightedMean(const std::vector<double>& values,
                               const std::vector<double>& weights);
    static double WeightedQuantile(const std::vector<double>& values,
                                   const std::vector<double>& weights,
                                   double quantile);
};
}
