#include "SOLISExposure.h"

#include "SOLISBackend.h"
#include "SOLISSolarModel.h"
#include "SOLISExposureMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <new>

using namespace CCCoreLib;

namespace
{
constexpr double Pi = 3.1415926535897932384626433832795;
constexpr double FractionTolerance = 1.0e-9;

QString compactNumber(double value, int decimals = 6)
{
    QString text = QString::number(value, 'f', decimals);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

QString timeZoneToken(const solis::Site& site)
{
    if (site.timeMode == solis::TimeMode::UTC)
        return QStringLiteral("UTC");
    const double offset = site.utcOffsetHours;
    return QStringLiteral("L%1%2")
        .arg(offset >= 0.0 ? QStringLiteral("+") : QString())
        .arg(compactNumber(offset, 2));
}

QString timeToken(double dayOfYear)
{
    const double dayFloor = std::floor(dayOfYear);
    double hours = (dayOfYear - dayFloor) * 24.0;
    int seconds = static_cast<int>(std::lround(hours * 3600.0));
    seconds = std::max(0, std::min(86399, seconds));
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    return s == 0
        ? QStringLiteral("T%1%2").arg(h, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0'))
        : QStringLiteral("T%1%2%3").arg(h, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

QString temporalSuffix(const solis::ExposureOptions& options)
{
    const auto& time = options.calculation.time;
    const int day = static_cast<int>(std::floor(time.dayOfYear));
    QString suffix = QStringLiteral("D%1 %2 %3").arg(day).arg(timeToken(time.dayOfYear), timeZoneToken(options.calculation.site));
    if (time.mode != solis::TemporalMode::Instantaneous)
        suffix += QStringLiteral(" dur%1h dt%2m").arg(compactNumber(time.durationHours, 3), compactNumber(time.timestepMinutes, 3));
    return suffix;
}

double angularDistanceDeg(const CCVector3d& a, const CCVector3d& b)
{
    const double dot = std::max(-1.0, std::min(1.0, a.dot(b)));
    return std::acos(dot) * 180.0 / Pi;
}

void appendOrMerge(std::vector<solis::SolarExposureSample>& samples,
                   const solis::SolarExposureSample& sample,
                   double toleranceDeg)
{
    if (samples.empty() || toleranceDeg <= 0.0
        || angularDistanceDeg(samples.back().direction, sample.direction) > toleranceDeg)
    {
        samples.push_back(sample);
        return;
    }

    auto& target = samples.back();
    CCVector3d merged = target.direction * target.daylightWeightHours
                      + sample.direction * sample.daylightWeightHours;
    if (merged.norm2() > 1.0e-20)
        merged.normalize();
    else
        merged = target.direction;
    target.direction = merged;
    target.daylightWeightHours += sample.daylightWeightHours;
    target.directHorizontalPotential += sample.directHorizontalPotential;
}

bool finiteUnit(double value)
{
    return std::isfinite(value) && value >= -FractionTolerance && value <= 1.0 + FractionTolerance;
}

} // namespace

namespace solis
{

bool ExposureCalculator::ValidateOptions(const ExposureOptions& options, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    switch (options.occlusionContext)
    {
    case ExposureOcclusionContext::TargetOnly:
    case ExposureOcclusionContext::SelectedEntities:
    case ExposureOcclusionContext::AllVisibleEntities:
        break;
    default:
        return fail(QStringLiteral("unknown exposure occlusion context"));
    }

    const uint32_t knownMetrics = static_cast<uint32_t>(ExposureAll);
    if ((options.metrics & knownMetrics) == 0u)
        return fail(QStringLiteral("select at least one exposure metric"));
    if ((options.metrics & ~knownMetrics) != 0u)
        return fail(QStringLiteral("unknown exposure metric flag"));

    const auto& calc = options.calculation;
    const bool needsSky = HasMetric(options, ExposureSky) || HasMetric(options, ExposureRadiative);
    const bool needsSolarGeometry = HasMetric(options, ExposureSun) || HasMetric(options, ExposureRadiative);
    const bool needsAtmosphere = HasMetric(options, ExposureRadiative);

    // Rendering is shared by all exposure metrics.
    if (calc.render.resolution == 0)
        return fail(QStringLiteral("render resolution must be greater than zero"));
    if (!std::isfinite(calc.render.depthBias)
        || calc.render.depthBias < 0.0
        || calc.render.depthBias > 0.05)
    {
        return fail(QStringLiteral("depth bias must be in [0, 0.05]"));
    }

    // SKY is intentionally independent of location/date/atmosphere. Only its
    // geometric sky quadrature and renderer settings are validated here.
    if (needsSky
        && (calc.sampling.diffuseRayCount == 0 || calc.sampling.diffuseRayCount > 65536))
    {
        return fail(QStringLiteral("sky ray count must be in [1, 65536]"));
    }

    if (needsSolarGeometry)
    {
        if (!std::isfinite(calc.time.dayOfYear)
            || calc.time.dayOfYear < 1.0
            || calc.time.dayOfYear >= 367.0)
        {
            return fail(QStringLiteral("day-of-year must be >= 1 and < 367 (fractional part is time-of-day)"));
        }
        if (!std::isfinite(calc.site.latitudeDeg)
            || calc.site.latitudeDeg < -90.0
            || calc.site.latitudeDeg > 90.0)
        {
            return fail(QStringLiteral("latitude must be in [-90, 90] degrees"));
        }
        if (!std::isfinite(calc.site.longitudeDeg)
            || calc.site.longitudeDeg < -180.0
            || calc.site.longitudeDeg > 180.0)
        {
            return fail(QStringLiteral("longitude must be in [-180, 180] degrees (east positive)"));
        }
        if (!std::isfinite(calc.site.elevationM))
            return fail(QStringLiteral("elevation must be finite"));
        if (calc.site.timeMode == TimeMode::LocalFixedOffset
            && (!std::isfinite(calc.site.utcOffsetHours)
                || calc.site.utcOffsetHours < -14.0
                || calc.site.utcOffsetHours > 14.0))
        {
            return fail(QStringLiteral("UTC offset must be in [-14, 14] hours"));
        }
        if (calc.time.mode != TemporalMode::Instantaneous)
        {
            if (!std::isfinite(calc.time.durationHours) || calc.time.durationHours <= 0.0)
                return fail(QStringLiteral("duration must be greater than zero for interval exposure"));
            if (!std::isfinite(calc.time.timestepMinutes) || calc.time.timestepMinutes <= 0.0)
                return fail(QStringLiteral("timestep must be greater than zero for interval exposure"));
        }
        if (!std::isfinite(calc.sampling.directAngularToleranceDeg)
            || calc.sampling.directAngularToleranceDeg < 0.0
            || calc.sampling.directAngularToleranceDeg > 180.0)
        {
            return fail(QStringLiteral("direct angular tolerance must be in [0, 180] degrees"));
        }
    }

    if (needsAtmosphere
        && (!std::isfinite(calc.site.linkeTurbidity)
            || calc.site.linkeTurbidity < 1.0
            || calc.site.linkeTurbidity > 12.0))
    {
        return fail(QStringLiteral("Linke turbidity must be in [1, 12]"));
    }

    if (!std::isfinite(options.crownThreshold)
        || options.crownThreshold < 0.0
        || options.crownThreshold > 1.0)
    {
        return fail(QStringLiteral("crown exposure threshold must be in [0, 1]"));
    }
    if (!options.crownVoxelAuto
        && (!std::isfinite(options.crownVoxelSize) || options.crownVoxelSize <= 0.0))
    {
        return fail(QStringLiteral("explicit crown voxel size must be greater than zero"));
    }
    if (options.crownSummary && !HasMetric(options, ExposureRadiative))
        return fail(QStringLiteral("Crown Exposure Index requires Radiative Exposure Index"));

    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool ExposureCalculator::GenerateSolarSamples(const ExposureOptions& options,
                                              std::vector<SolarExposureSample>& samples,
                                              ExposureReference& reference,
                                              QString* errorMessage)
{
    samples.clear();
    reference = ExposureReference{};

    QString validationError;
    if (!ValidateOptions(options, &validationError))
    {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }

    const auto& calc = options.calculation;
    const bool instantaneous = calc.time.mode == TemporalMode::Instantaneous;

    try
    {
        if (instantaneous)
        {
            const SolarPosition pos = SolarModel::position(calc.time.dayOfYear, calc.site);
            if (pos.altitudeDeg > 0.0)
            {
                const ClearSkyIrradiance cs = SolarModel::clearSky(calc.time.dayOfYear, calc.site, pos);
                const double cosZenith = std::max(0.0, -pos.directionToScene.z);
                samples.push_back({pos.directionToScene, 1.0, cs.dniWm2 * cosZenith});
                reference.daylightHours = 1.0;
            }
            return true;
        }

        const double stepHours = calc.time.timestepMinutes / 60.0;
        double elapsed = 0.0;
        while (elapsed < calc.time.durationHours - 1.0e-12)
        {
            const double dt = std::min(stepHours, calc.time.durationHours - elapsed);
            const double sampleDoy = calc.time.dayOfYear + (elapsed + 0.5 * dt) / 24.0;
            const SolarPosition pos = SolarModel::position(sampleDoy, calc.site);
            if (pos.altitudeDeg > 0.0)
            {
                const ClearSkyIrradiance cs = SolarModel::clearSky(sampleDoy, calc.site, pos);
                const double cosZenith = std::max(0.0, -pos.directionToScene.z);
                const SolarExposureSample sample{pos.directionToScene, dt, cs.dniWm2 * cosZenith * dt};
                if (calc.sampling.acceleratedDirectSampling)
                    appendOrMerge(samples, sample, calc.sampling.directAngularToleranceDeg);
                else
                    samples.push_back(sample);
                reference.daylightHours += dt;
            }
            elapsed += dt;
        }
    }
    catch (const std::bad_alloc&)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("out of memory while generating solar exposure samples");
        return false;
    }

    return true;
}

double ExposureCalculator::SkyFraction(double visibleWeight, double totalWeight)
{
    return exposure_math::boundedFraction(visibleWeight, totalWeight);
}

double ExposureCalculator::SunFraction(double visibleDaylightHours, double daylightHours)
{
    return exposure_math::boundedFraction(visibleDaylightHours, daylightHours);
}

double ExposureCalculator::RadiativeFraction(double receivedPotential, double openPotential, bool* valid)
{
    return exposure_math::radiativeFraction(receivedPotential, openPotential, valid);
}

bool ExposureCalculator::Calculate(const ExposureOptions& options,
                                   GenericCloud* vertices,
                                   GenericMesh* mesh,
                                   ExposureResult& result,
                                   GenericProgressCallback* progressCb,
                                   const QString& entityName,
                                   SOLISAccumulationBackend* backend,
                                   QString* errorMessage)
{
    result = ExposureResult{};
    QString validationError;
    if (!ValidateOptions(options, &validationError))
    {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    if (!vertices || vertices->size() == 0)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("selected entity has no points/vertices");
        return false;
    }

    const bool needSky = HasMetric(options, ExposureSky) || HasMetric(options, ExposureRadiative);
    const bool needSun = HasMetric(options, ExposureSun) || HasMetric(options, ExposureRadiative);
    const unsigned pointCount = vertices->size();

    try
    {
        if (HasMetric(options, ExposureSky))
            result.sky.assign(pointCount, 0.0);
        if (HasMetric(options, ExposureSun))
            result.sun.assign(pointCount, 0.0);
        if (HasMetric(options, ExposureRadiative))
            result.radiative.assign(pointCount, 0.0);
    }
    catch (const std::bad_alloc&)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("out of memory allocating exposure result arrays");
        return false;
    }

    std::unique_ptr<SOLISAccumulationBackend> ownedBackend;
    if (!backend)
    {
        // Multi-entity context is an orchestration concern: callers must resolve
        // which scene entities participate and configure the backend before the
        // point-level exposure calculation starts.  Falling back silently to the
        // target alone here would make Selected/All-visible calculations appear
        // successful while actually computing self-exposure only.
        if (options.occlusionContext != ExposureOcclusionContext::TargetOnly)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("multi-entity occlusion context requires a preconfigured renderer context");
            return false;
        }

        ownedBackend = CreateSOLISAccumulationBackend();
        backend = ownedBackend.get();
        if (!backend
            || !backend->setOcclusionContext({SOLISOccluder{vertices, mesh}})
            || !backend->init(options.calculation.render.resolution,
                              options.calculation.render.resolution,
                              vertices,
                              mesh,
                              options.calculation.render.meshIsClosed,
                              options.calculation.render.depthBias,
                              options.calculation.render.parallelCpuProjection,
                              options.calculation.render.cpuThreadCount))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("renderer initialization failed");
            return false;
        }
    }

    std::vector<CCVector3d> skyRays;
    if (needSky && !SOLIS::GenerateDiffuseRays(options.calculation.sampling.diffuseRayCount, skyRays))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("failed to generate sky rays");
        return false;
    }

    std::vector<SolarExposureSample> solarSamples;
    if (needSun && !GenerateSolarSamples(options, solarSamples, result.reference, errorMessage))
        return false;

    if (HasMetric(options, ExposureSun) && result.reference.daylightHours <= 0.0)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Sun Exposure is undefined because the selected period contains no daylight");
        return false;
    }

    // Diffuse irradiance in the same temporal units as directHorizontalPotential:
    // W/m2 for instantaneous; Wh/m2 for interval calculations.
    double aggregateDhi = 0.0;
    if (HasMetric(options, ExposureRadiative))
    {
        const auto& calc = options.calculation;
        if (calc.time.mode == TemporalMode::Instantaneous)
        {
            const SolarPosition pos = SolarModel::position(calc.time.dayOfYear, calc.site);
            aggregateDhi = SolarModel::clearSky(calc.time.dayOfYear, calc.site, pos).dhiWm2;
        }
        else
        {
            const double stepHours = calc.time.timestepMinutes / 60.0;
            double elapsed = 0.0;
            while (elapsed < calc.time.durationHours - 1.0e-12)
            {
                const double dt = std::min(stepHours, calc.time.durationHours - elapsed);
                const double sampleDoy = calc.time.dayOfYear + (elapsed + 0.5 * dt) / 24.0;
                const SolarPosition pos = SolarModel::position(sampleDoy, calc.site);
                aggregateDhi += SolarModel::clearSky(sampleDoy, calc.site, pos).dhiWm2 * dt;
                elapsed += dt;
            }
        }
    }

    if (needSun)
    {
        for (const SolarExposureSample& sample : solarSamples)
            result.reference.openDirectPotential += sample.directHorizontalPotential;
    }
    if (needSky && HasMetric(options, ExposureRadiative))
    {
        const double solidWeight = 1.0 / static_cast<double>(skyRays.size());
        for (const CCVector3d& ray : skyRays)
        {
            const double z = std::max(0.0, -ray.z);
            result.reference.openDiffusePotential += 2.0 * aggregateDhi * solidWeight * z;
        }
    }
    result.reference.openTotalPotential = result.reference.openDirectPotential + result.reference.openDiffusePotential;
    if (HasMetric(options, ExposureRadiative) && result.reference.openTotalPotential <= 1.0e-12)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Radiative Exposure is undefined because open-sky radiation potential is zero");
        return false;
    }

    const unsigned totalPasses = static_cast<unsigned>(skyRays.size() + solarSamples.size());
    NormalizedProgress progress(progressCb, std::max(1u, totalPasses));
    if (progressCb)
    {
        if (progressCb->textCanBeEdited())
        {
            progressCb->setMethodTitle("SOLIS - canopy exposure");
            const QString info = QStringLiteral("%1\nSky passes: %2\nSolar passes: %3")
                                     .arg(entityName)
                                     .arg(static_cast<qulonglong>(skyRays.size()))
                                     .arg(static_cast<qulonglong>(solarSamples.size()));
            progressCb->setInfo(qPrintable(info));
        }
        progressCb->update(0);
        progressCb->start();
    }

    std::vector<double> skyVisible;
    std::vector<double> directReceived;
    std::vector<double> diffuseReceived;
    std::vector<double> sunVisibleHours;
    std::vector<uint8_t> visible;
    try
    {
        if (HasMetric(options, ExposureSky))
            skyVisible.assign(pointCount, 0.0);
        if (HasMetric(options, ExposureSun))
            sunVisibleHours.assign(pointCount, 0.0);
        if (HasMetric(options, ExposureRadiative))
        {
            directReceived.assign(pointCount, 0.0);
            diffuseReceived.assign(pointCount, 0.0);
        }
    }
    catch (const std::bad_alloc&)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("out of memory allocating exposure accumulators");
        return false;
    }

    if (needSky)
    {
        const double solidWeight = 1.0 / static_cast<double>(skyRays.size());
        for (const CCVector3d& ray : skyRays)
        {
            if (backend->visibility(visible, ray) < 0)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("sky visibility rendering failed");
                return false;
            }
            const double z = std::max(0.0, -ray.z); // surface-to-sky z
            const double diffuseWeight = 2.0 * aggregateDhi * solidWeight * z;
            for (unsigned i = 0; i < pointCount; ++i)
            {
                if (!visible[i])
                    continue;
                if (HasMetric(options, ExposureSky))
                    skyVisible[i] += solidWeight;
                if (HasMetric(options, ExposureRadiative))
                    diffuseReceived[i] += diffuseWeight;
            }
            if (progressCb && !progress.oneStep())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("calculation canceled");
                return false;
            }
        }
    }

    if (needSun)
    {
        for (const SolarExposureSample& sample : solarSamples)
        {
            if (backend->visibility(visible, sample.direction) < 0)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("solar visibility rendering failed");
                return false;
            }
            for (unsigned i = 0; i < pointCount; ++i)
            {
                if (!visible[i])
                    continue;
                if (HasMetric(options, ExposureSun))
                    sunVisibleHours[i] += sample.daylightWeightHours;
                if (HasMetric(options, ExposureRadiative))
                    directReceived[i] += sample.directHorizontalPotential;
            }
            if (progressCb && !progress.oneStep())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("calculation canceled");
                return false;
            }
        }
    }

    for (unsigned i = 0; i < pointCount; ++i)
    {
        if (HasMetric(options, ExposureSky))
            result.sky[i] = SkyFraction(skyVisible[i], 1.0);
        if (HasMetric(options, ExposureSun))
            result.sun[i] = SunFraction(sunVisibleHours[i], result.reference.daylightHours);
        if (HasMetric(options, ExposureRadiative))
        {
            bool valid = false;
            result.radiative[i] = RadiativeFraction(directReceived[i] + diffuseReceived[i], result.reference.openTotalPotential, &valid);
            if (!valid || !finiteUnit(result.radiative[i]))
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Radiative Exposure produced a value outside [0,1]; check render settings and numerical stability");
                return false;
            }
        }
    }

    return true;
}

QString ExposureCalculator::OcclusionContextName(ExposureOcclusionContext mode)
{
    switch (mode)
    {
    case ExposureOcclusionContext::TargetOnly: return QStringLiteral("target-only");
    case ExposureOcclusionContext::SelectedEntities: return QStringLiteral("selected-entities");
    case ExposureOcclusionContext::AllVisibleEntities: return QStringLiteral("all-visible-entities");
    }
    return QStringLiteral("unknown");
}

QString ExposureCalculator::SkyFieldBaseName(const ExposureOptions& options)
{
    return QStringLiteral("SOLIS Sky Exposure [fraction] rays%1 R%2")
        .arg(options.calculation.sampling.diffuseRayCount)
        .arg(options.calculation.render.resolution);
}

QString ExposureCalculator::SunFieldBaseName(const ExposureOptions& options)
{
    return QStringLiteral("SOLIS Sun Exposure [fraction] %1 R%2")
        .arg(temporalSuffix(options))
        .arg(options.calculation.render.resolution);
}

QString ExposureCalculator::RadiativeFieldBaseName(const ExposureOptions& options)
{
    return QStringLiteral("SOLIS Radiative Exposure [fraction] %1 H R%2")
        .arg(temporalSuffix(options))
        .arg(options.calculation.render.resolution);
}

} // namespace solis
