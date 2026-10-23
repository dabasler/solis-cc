#include "SOLISBatch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace solis
{
namespace
{
constexpr double MinutesPerDay = 1440.0;
constexpr double EpsilonDays = 1.0e-10;

bool fail(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}
}

bool BuildBatchTimes(const CalculationOptions& baseOptions,
                     const BatchOptions& batch,
                     std::vector<double>& dayOfYearTimes,
                     QString* errorMessage)
{
    dayOfYearTimes.clear();
    if (errorMessage)
        errorMessage->clear();

    const double start = baseOptions.time.dayOfYear;
    if (!std::isfinite(start) || start < 1.0 || start >= 367.0)
        return fail(errorMessage, QStringLiteral("batch start day/time must be >= 1 and < 367"));

    if (!batch.enabled)
    {
        dayOfYearTimes.push_back(start);
        return true;
    }

    if (!std::isfinite(batch.endDayOfYear) || batch.endDayOfYear < 1.0 || batch.endDayOfYear >= 367.0)
        return fail(errorMessage, QStringLiteral("batch end day/time must be >= 1 and < 367"));
    if (batch.endDayOfYear + EpsilonDays < start)
        return fail(errorMessage, QStringLiteral("batch end must not be earlier than batch start"));
    if (!std::isfinite(batch.stepMinutes) || batch.stepMinutes <= 0.0)
        return fail(errorMessage, QStringLiteral("batch step must be greater than zero minutes"));
    if (batch.maxTimePoints == 0)
        return fail(errorMessage, QStringLiteral("batch maximum time-point count must be greater than zero"));

    const double stepDays = batch.stepMinutes / MinutesPerDay;
    const double spanDays = std::max(0.0, batch.endDayOfYear - start);
    const double rawCount = std::floor((spanDays + EpsilonDays) / stepDays) + 1.0;
    if (!std::isfinite(rawCount) || rawCount < 1.0)
        return fail(errorMessage, QStringLiteral("invalid batch time range"));
    if (rawCount > static_cast<double>(batch.maxTimePoints))
    {
        return fail(errorMessage,
                    QStringLiteral("batch would create %1 time points; maximum is %2")
                        .arg(static_cast<qulonglong>(rawCount))
                        .arg(batch.maxTimePoints));
    }

    const size_t count = static_cast<size_t>(rawCount);
    try
    {
        dayOfYearTimes.reserve(count);
    }
    catch (const std::bad_alloc&)
    {
        return fail(errorMessage, QStringLiteral("not enough memory to create batch schedule"));
    }

    for (size_t i = 0; i < count; ++i)
    {
        const double t = start + static_cast<double>(i) * stepDays;
        if (t > batch.endDayOfYear + EpsilonDays)
            break;
        dayOfYearTimes.push_back(t);
    }

    if (dayOfYearTimes.empty())
        dayOfYearTimes.push_back(start);
    return true;
}

unsigned BatchTimePointCount(const CalculationOptions& baseOptions,
                             const BatchOptions& batch,
                             QString* errorMessage)
{
    std::vector<double> times;
    if (!BuildBatchTimes(baseOptions, batch, times, errorMessage))
        return 0;
    if (times.size() > static_cast<size_t>(std::numeric_limits<unsigned>::max()))
        return 0;
    return static_cast<unsigned>(times.size());
}

} // namespace solis
