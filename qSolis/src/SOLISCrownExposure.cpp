#include "SOLISCrownExposure.h"

#include "SOLISCrownExposureMath.h"
#include "SOLISExposureMath.h"

#include <GenericIndexedMesh.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace
{
bool validOptionalMetricSize(const std::vector<double>& values, unsigned count)
{
    return values.empty() || values.size() == count;
}

bool cloudBoundingBox(const ccPointCloud* cloud, CCVector3& bbMin, CCVector3& bbMax)
{
    if (!cloud || cloud->size() == 0)
        return false;

    const CCVector3* first = cloud->getPoint(0);
    bbMin = *first;
    bbMax = *first;

    for (unsigned i = 1; i < cloud->size(); ++i)
    {
        const CCVector3* point = cloud->getPoint(i);
        bbMin.x = std::min(bbMin.x, point->x);
        bbMin.y = std::min(bbMin.y, point->y);
        bbMin.z = std::min(bbMin.z, point->z);
        bbMax.x = std::max(bbMax.x, point->x);
        bbMax.y = std::max(bbMax.y, point->y);
        bbMax.z = std::max(bbMax.z, point->z);
    }

    return true;
}

double triangleArea(const CCVector3d& a, const CCVector3d& b, const CCVector3d& c)
{
    return 0.5 * (b - a).cross(c - a).norm();
}
}

namespace solis
{

double CrownExposure::AutomaticVoxelSize(const ccPointCloud* cloud)
{
    if (!cloud || cloud->size() == 0)
        return 1.0;

    CCVector3 bbMin;
    CCVector3 bbMax;
    if (!cloudBoundingBox(cloud, bbMin, bbMax))
        return 1.0;
    const double diagonal = static_cast<double>((bbMax - bbMin).norm());
    return crown_math::automaticVoxelSizeFromDiagonal(diagonal);
}

double CrownExposure::WeightedMean(const std::vector<double>& values,
                                   const std::vector<double>& weights)
{
    return exposure_math::weightedMean(values, weights);
}

double CrownExposure::WeightedQuantile(const std::vector<double>& values,
                                       const std::vector<double>& weights,
                                       double quantile)
{
    return exposure_math::weightedQuantile(values, weights, quantile);
}

bool CrownExposure::Summarize(const ccPointCloud* cloud,
                              const ccGenericMesh* mesh,
                              const ExposureResult& exposure,
                              const ExposureOptions& options,
                              CrownExposureSummary& summary,
                              QString* errorMessage)
{
    summary = CrownExposureSummary{};
    summary.threshold = options.crownThreshold;

    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (!cloud || cloud->size() == 0)
        return fail(QStringLiteral("crown summary requires a non-empty point/vertex cloud"));

    const unsigned count = cloud->size();
    if (exposure.radiative.size() != count
        || !validOptionalMetricSize(exposure.sky, count)
        || !validOptionalMetricSize(exposure.sun, count))
    {
        return fail(QStringLiteral("crown summary exposure arrays do not match the entity vertex count"));
    }

    summary.pointOrVertexCount = count;

    std::vector<double> skyValues;
    std::vector<double> sunValues;
    std::vector<double> radiativeValues;
    std::vector<double> weights;

    if (mesh)
    {
        auto* indexed = dynamic_cast<const CCCoreLib::GenericIndexedMesh*>(mesh);
        if (!indexed)
            return fail(QStringLiteral("surface-area crown weighting requires an indexed mesh"));

        weights.assign(count, 0.0);
        auto* mutableIndexed = const_cast<CCCoreLib::GenericIndexedMesh*>(indexed);
        mutableIndexed->placeIteratorAtBeginning();

        for (unsigned triangleIndex = 0; triangleIndex < indexed->size(); ++triangleIndex)
        {
            const CCCoreLib::VerticesIndexes* triangle = mutableIndexed->getNextTriangleVertIndexes();
            if (!triangle
                || triangle->i1 >= count
                || triangle->i2 >= count
                || triangle->i3 >= count)
            {
                continue;
            }

            const CCVector3d a = cloud->getPoint(triangle->i1)->toDouble();
            const CCVector3d b = cloud->getPoint(triangle->i2)->toDouble();
            const CCVector3d c = cloud->getPoint(triangle->i3)->toDouble();
            const double area = triangleArea(a, b, c);
            if (!(area > 0.0) || !std::isfinite(area))
                continue;

            if (crown_math::addTriangleAreaWeight(weights, triangle->i1, triangle->i2, triangle->i3, area))
                summary.representedSurfaceArea += area;
        }

        if (!(summary.representedSurfaceArea > 0.0))
            return fail(QStringLiteral("mesh crown summary contains no positive-area triangles"));

        radiativeValues = exposure.radiative;
        skyValues = exposure.sky;
        sunValues = exposure.sun;
        summary.weightingMethod = QStringLiteral("surface-area");
    }
    else
    {
        const double voxelSize = options.crownVoxelAuto
                                     ? AutomaticVoxelSize(cloud)
                                     : options.crownVoxelSize;
        if (!(voxelSize > 0.0) || !std::isfinite(voxelSize))
            return fail(QStringLiteral("invalid crown voxel size"));

        CCVector3 bbMin;
        CCVector3 bbMax;
        if (!cloudBoundingBox(cloud, bbMin, bbMax))
            return fail(QStringLiteral("failed to determine crown point-cloud bounds"));

        crown_math::VoxelAggregation aggregation;
        const std::vector<double>* sky = exposure.sky.empty() ? nullptr : &exposure.sky;
        const std::vector<double>* sun = exposure.sun.empty() ? nullptr : &exposure.sun;
        const std::array<double, 3> origin{
            static_cast<double>(bbMin.x),
            static_cast<double>(bbMin.y),
            static_cast<double>(bbMin.z)};

        const auto pointAt = [cloud](std::size_t index)
        {
            const CCVector3* point = cloud->getPoint(static_cast<unsigned>(index));
            return std::array<double, 3>{
                static_cast<double>(point->x),
                static_cast<double>(point->y),
                static_cast<double>(point->z)};
        };

        if (!crown_math::aggregateOccupiedVoxels(count,
                                                  pointAt,
                                                  sky,
                                                  sun,
                                                  exposure.radiative,
                                                  origin,
                                                  voxelSize,
                                                  aggregation))
        {
            return fail(QStringLiteral("crown summary has no valid occupied voxels"));
        }

        summary.voxelSize = voxelSize;
        summary.occupiedVoxelCount = aggregation.occupiedVoxelCount;
        summary.weightingMethod = QStringLiteral("occupied-voxel");

        radiativeValues = std::move(aggregation.radiative);
        skyValues = std::move(aggregation.sky);
        sunValues = std::move(aggregation.sun);
        weights.assign(radiativeValues.size(), 1.0);
    }

    if (weights.empty() || radiativeValues.empty())
        return fail(QStringLiteral("crown summary has no positive spatial weights"));

    summary.radiativeMean = WeightedMean(radiativeValues, weights);
    summary.cei = summary.radiativeMean;
    if (!skyValues.empty())
        summary.skyMean = WeightedMean(skyValues, weights);
    if (!sunValues.empty())
        summary.sunMean = WeightedMean(sunValues, weights);

    summary.reiP10 = WeightedQuantile(radiativeValues, weights, 0.10);
    summary.reiP50 = WeightedQuantile(radiativeValues, weights, 0.50);
    summary.reiP90 = WeightedQuantile(radiativeValues, weights, 0.90);

    if (!std::isfinite(summary.cei)
        || !std::isfinite(summary.reiP10)
        || !std::isfinite(summary.reiP50)
        || !std::isfinite(summary.reiP90))
    {
        return fail(QStringLiteral("crown summary statistics are undefined"));
    }

    double exposedWeight = 0.0;
    double totalWeight = 0.0;
    for (std::size_t i = 0; i < radiativeValues.size(); ++i)
    {
        if (!std::isfinite(radiativeValues[i])
            || !std::isfinite(weights[i])
            || weights[i] <= 0.0)
        {
            continue;
        }

        totalWeight += weights[i];
        if (radiativeValues[i] >= options.crownThreshold)
            exposedWeight += weights[i];
    }

    if (!(totalWeight > 0.0))
        return fail(QStringLiteral("crown summary has no finite weighted REI values"));

    summary.exposedFraction = exposedWeight / totalWeight;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

} // namespace solis
