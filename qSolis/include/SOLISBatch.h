#pragma once

#include "SOLIS.h"

#include <QString>
#include <vector>

namespace solis
{

struct BatchOptions
{
    bool enabled = false;
    double endDayOfYear = 172.5;
    double stepMinutes = 60.0;
    unsigned maxTimePoints = 1000;
};

struct BatchContext
{
    QString batchId;
    unsigned index = 0;       //!< zero-based index
    unsigned count = 1;
    double startDayOfYear = 0.0;
    double endDayOfYear = 0.0;
    double stepMinutes = 0.0;

    bool isBatch() const { return !batchId.isEmpty() && count > 1; }
};

//! Builds the list of calculation start times for a single run or a batch.
/**
 * The end time is inclusive only when it falls on the requested step grid.
 * Batch times never wrap implicitly across a year boundary: specify an end
 * DOY >= the start DOY.  This keeps project result labels unambiguous.
 */
bool BuildBatchTimes(const CalculationOptions& baseOptions,
                     const BatchOptions& batch,
                     std::vector<double>& dayOfYearTimes,
                     QString* errorMessage = nullptr);

//! Number of time points that BuildBatchTimes would generate, or zero on error.
unsigned BatchTimePointCount(const CalculationOptions& baseOptions,
                             const BatchOptions& batch,
                             QString* errorMessage = nullptr);

} // namespace solis
