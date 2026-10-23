#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace solis::exposure_math
{
inline double boundedFraction(double numerator, double denominator)
{
    if (!(denominator > 0.0) || !std::isfinite(numerator) || !std::isfinite(denominator))
        return std::numeric_limits<double>::quiet_NaN();
    return std::max(0.0, std::min(1.0, numerator / denominator));
}

inline double radiativeFraction(double received, double openPotential, bool* valid = nullptr)
{
    constexpr double tolerance = 1.0e-9;
    const bool denominatorValid = std::isfinite(received) && std::isfinite(openPotential) && openPotential > 1.0e-12;
    if (!denominatorValid)
    {
        if (valid) *valid = false;
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double raw = received / openPotential;
    const bool rangeValid = raw >= -tolerance && raw <= 1.0 + tolerance;
    if (valid) *valid = rangeValid;
    if (!rangeValid)
        return raw;
    return std::max(0.0, std::min(1.0, raw));
}

inline double weightedMean(const std::vector<double>& values, const std::vector<double>& weights)
{
    if (values.size() != weights.size() || values.empty())
        return std::numeric_limits<double>::quiet_NaN();
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (!std::isfinite(values[i]) || !std::isfinite(weights[i]) || weights[i] <= 0.0)
            continue;
        num += values[i] * weights[i]; den += weights[i];
    }
    return den > 0.0 ? num / den : std::numeric_limits<double>::quiet_NaN();
}

inline double weightedQuantile(const std::vector<double>& values, const std::vector<double>& weights, double q)
{
    if (values.size() != weights.size() || values.empty() || !std::isfinite(q))
        return std::numeric_limits<double>::quiet_NaN();
    q = std::max(0.0, std::min(1.0, q));
    std::vector<std::pair<double,double>> pairs;
    pairs.reserve(values.size());
    double total = 0.0;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (std::isfinite(values[i]) && std::isfinite(weights[i]) && weights[i] > 0.0)
        { pairs.emplace_back(values[i], weights[i]); total += weights[i]; }
    }
    if (pairs.empty() || total <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    const double target = q * total;
    double cumulative = 0.0;
    for (const auto& p : pairs)
    { cumulative += p.second; if (cumulative >= target) return p.first; }
    return pairs.back().first;
}
}
