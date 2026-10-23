#pragma once

#include "SOLIS.h"

#include <GenericCloud.h>
#include <GenericMesh.h>
#include <GenericProgressCallback.h>

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

class SOLISAccumulationBackend;

namespace solis
{
enum ExposureMetric : uint32_t
{
    ExposureNone      = 0u,
    ExposureSky       = 1u << 0,
    ExposureSun       = 1u << 1,
    ExposureRadiative = 1u << 2,
    ExposureAll       = ExposureSky | ExposureSun | ExposureRadiative
};

enum class ExposureOcclusionContext
{
    TargetOnly,
    SelectedEntities,
    AllVisibleEntities
};

struct ExposureContextInfo
{
    ExposureOcclusionContext mode = ExposureOcclusionContext::SelectedEntities;
    QStringList entityNames;
};

struct ExposureOptions
{
    uint32_t metrics = ExposureAll;
    ExposureOcclusionContext occlusionContext = ExposureOcclusionContext::SelectedEntities;
    CalculationOptions calculation;
    bool crownSummary = true;
    bool crownVoxelAuto = true;
    double crownVoxelSize = 0.0;
    double crownThreshold = 0.75;
};

struct ExposureReference
{
    double daylightHours = 0.0;
    double openDirectPotential = 0.0;
    double openDiffusePotential = 0.0;
    double openTotalPotential = 0.0;
};

struct ExposureResult
{
    std::vector<double> sky;
    std::vector<double> sun;
    std::vector<double> radiative;
    ExposureReference reference;
};

struct SolarExposureSample
{
    CCVector3d direction;
    double daylightWeightHours = 0.0;
    double directHorizontalPotential = 0.0;
};

class ExposureCalculator
{
public:
    static bool ValidateOptions(const ExposureOptions& options, QString* errorMessage = nullptr);

    //! Generates daylight samples. For interval calculations directHorizontalPotential
    //! is Wh/m2; for instantaneous calculations it is W/m2. Grouped samples preserve
    //! summed daylight and radiation weights while sharing one visibility pass.
    static bool GenerateSolarSamples(const ExposureOptions& options,
                                     std::vector<SolarExposureSample>& samples,
                                     ExposureReference& reference,
                                     QString* errorMessage = nullptr);

    //! Computes all selected per-point metrics using one FBO visibility pass per sky
    //! direction and one per solar direction. The supplied backend may be shared with
    //! the caller; if null, a backend is created and initialized internally.
    static bool Calculate(const ExposureOptions& options,
                          CCCoreLib::GenericCloud* vertices,
                          CCCoreLib::GenericMesh* mesh,
                          ExposureResult& result,
                          CCCoreLib::GenericProgressCallback* progressCb = nullptr,
                          const QString& entityName = QString(),
                          SOLISAccumulationBackend* backend = nullptr,
                          QString* errorMessage = nullptr);

    static bool HasMetric(const ExposureOptions& options, ExposureMetric metric)
    {
        return (options.metrics & static_cast<uint32_t>(metric)) != 0u;
    }

    static QString OcclusionContextName(ExposureOcclusionContext mode);
    static QString SkyFieldBaseName(const ExposureOptions& options);
    static QString SunFieldBaseName(const ExposureOptions& options);
    static QString RadiativeFieldBaseName(const ExposureOptions& options);

    // Pure helpers used by unit tests and by Calculate.
    static double SkyFraction(double visibleWeight, double totalWeight);
    static double SunFraction(double visibleDaylightHours, double daylightHours);
    static double RadiativeFraction(double receivedPotential, double openPotential, bool* valid = nullptr);
};

} // namespace solis
