#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace solis::crown_math
{
inline double automaticVoxelSizeFromDiagonal(double diagonal)
{
    return std::isfinite(diagonal) && diagonal > 1.0e-9 ? diagonal / 100.0 : 1.0;
}

inline bool addTriangleAreaWeight(std::vector<double>& weights,
                                  std::size_t i1,
                                  std::size_t i2,
                                  std::size_t i3,
                                  double area)
{
    if (!std::isfinite(area) || area <= 0.0
        || i1 >= weights.size() || i2 >= weights.size() || i3 >= weights.size())
    {
        return false;
    }
    const double share = area / 3.0;
    weights[i1] += share;
    weights[i2] += share;
    weights[i3] += share;
    return true;
}

struct VoxelAggregation
{
    std::vector<double> sky;
    std::vector<double> sun;
    std::vector<double> radiative;
    unsigned occupiedVoxelCount = 0;
};

namespace detail
{
struct CellAccumulator
{
    double sky = 0.0;
    double sun = 0.0;
    double radiative = 0.0;
    unsigned skyCount = 0;
    unsigned sunCount = 0;
    unsigned radiativeCount = 0;
};
}

//! Aggregates point metrics into equally weighted occupied voxels.
/**
 * Only cells containing at least one finite radiative value are returned,
 * because REI is the required basis of CEI. Optional sky/sun vectors remain
 * aligned with radiative and carry NaN for a cell only when that optional
 * metric has no finite sample in that cell.
 */
template <typename PointGetter>
bool aggregateOccupiedVoxels(std::size_t count,
                             PointGetter pointAt,
                             const std::vector<double>* sky,
                             const std::vector<double>* sun,
                             const std::vector<double>& radiative,
                             const std::array<double, 3>& origin,
                             double voxelSize,
                             VoxelAggregation& output)
{
    output = VoxelAggregation{};
    if (!(voxelSize > 0.0) || !std::isfinite(voxelSize) || radiative.size() != count)
        return false;
    if (sky && sky->size() != count)
        return false;
    if (sun && sun->size() != count)
        return false;

    using Key = std::tuple<int64_t, int64_t, int64_t>;
    std::map<Key, detail::CellAccumulator> cells;

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::array<double, 3> point = pointAt(i);
        if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2]))
            continue;

        const int64_t ix = static_cast<int64_t>(std::floor((point[0] - origin[0]) / voxelSize));
        const int64_t iy = static_cast<int64_t>(std::floor((point[1] - origin[1]) / voxelSize));
        const int64_t iz = static_cast<int64_t>(std::floor((point[2] - origin[2]) / voxelSize));
        detail::CellAccumulator& cell = cells[Key(ix, iy, iz)];

        if (sky && std::isfinite((*sky)[i]))
        {
            cell.sky += (*sky)[i];
            ++cell.skyCount;
        }
        if (sun && std::isfinite((*sun)[i]))
        {
            cell.sun += (*sun)[i];
            ++cell.sunCount;
        }
        if (std::isfinite(radiative[i]))
        {
            cell.radiative += radiative[i];
            ++cell.radiativeCount;
        }
    }

    output.occupiedVoxelCount = static_cast<unsigned>(cells.size());
    output.radiative.reserve(cells.size());
    if (sky)
        output.sky.reserve(cells.size());
    if (sun)
        output.sun.reserve(cells.size());

    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto& item : cells)
    {
        const detail::CellAccumulator& cell = item.second;
        if (cell.radiativeCount == 0)
            continue;

        output.radiative.push_back(cell.radiative / static_cast<double>(cell.radiativeCount));
        if (sky)
        {
            output.sky.push_back(cell.skyCount != 0
                                     ? cell.sky / static_cast<double>(cell.skyCount)
                                     : nan);
        }
        if (sun)
        {
            output.sun.push_back(cell.sunCount != 0
                                     ? cell.sun / static_cast<double>(cell.sunCount)
                                     : nan);
        }
    }

    return !output.radiative.empty();
}

} // namespace solis::crown_math
