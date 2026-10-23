#include "SOLIS.h"
#include "SOLISBackend.h"
#include "SOLISSolarModel.h"

#include <CCMath.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <new>

using namespace CCCoreLib;

namespace
{
constexpr double Pi = 3.1415926535897932384626433832795;

double angularDistanceDeg(const CCVector3d& a, const CCVector3d& b)
{
    const double dot = std::max(-1.0, std::min(1.0, a.dot(b)));
    return std::acos(dot) * 180.0 / Pi;
}

void appendOrMergeDirectSample(std::vector<solis::RaySample>& samples,
                               const solis::RaySample& sample,
                               double toleranceDeg)
{
    if (samples.empty() || toleranceDeg <= 0.0
        || angularDistanceDeg(samples.back().direction, sample.direction) > toleranceDeg)
    {
        samples.push_back(sample);
        return;
    }

    solis::RaySample& target = samples.back();
    const double oldEnergyWeight = target.irradianceWm2 * target.weightHours;
    const double newEnergyWeight = sample.irradianceWm2 * sample.weightHours;
    const double totalEnergyWeight = oldEnergyWeight + newEnergyWeight;
    const double totalTime = target.weightHours + sample.weightHours;

    // Direction is time-weighted. Irradiance is energy/time weighted below. This also keeps
    // accelerated direct-visibility calculations unbiased by irradiance magnitude.
    CCVector3d mergedDirection = target.direction * target.weightHours + sample.direction * sample.weightHours;
    if (mergedDirection.norm2() > 1.0e-20)
        mergedDirection.normalize();
    else
        mergedDirection = target.direction;

    target.direction = mergedDirection;
    target.irradianceWm2 = (totalTime > 0.0 ? totalEnergyWeight / totalTime : 0.0);
    target.weightHours = totalTime;
}

double sampleDHI(double dayOfYear, const solis::CalculationOptions& options)
{
    const solis::SolarPosition pos = solis::SolarModel::position(dayOfYear, options.site);
    return solis::SolarModel::clearSky(dayOfYear, options.site, pos).dhiWm2;
}

bool initAccumulator(GenericCloud* vertices, std::vector<double>& accumulator)
{
    if (!vertices || !vertices->enableScalarField())
        return false;
    try
    {
        accumulator.assign(vertices->size(), 0.0);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    return true;
}

void writeAccumulator(GenericCloud* vertices, const std::vector<double>& accumulator, double scale)
{
    for (unsigned i = 0; i < vertices->size(); ++i)
    {
        const double value = std::max(0.0, accumulator[i] * scale);
        vertices->setPointScalarValue(i, static_cast<ScalarType>(value));
    }
}

void setupProgress(GenericProgressCallback* progressCb,
                   const QString& title,
                   const QString& entityName,
                   unsigned rayCount,
                   GenericMesh* mesh,
                   unsigned pointCount)
{
    if (!progressCb)
        return;

    if (progressCb->textCanBeEdited())
    {
        progressCb->setMethodTitle(qPrintable(title));
        QString info = entityName;
        if (!info.isEmpty())
            info += QLatin1Char('\n');
        info += QStringLiteral("Rays: %1").arg(rayCount);
        info += mesh ? QStringLiteral("\nFaces: %1").arg(mesh->size())
                     : QStringLiteral("\nVertices: %1").arg(pointCount);
        progressCb->setInfo(qPrintable(info));
    }
    progressCb->update(0);
    progressCb->start();
}
} // namespace


bool SOLIS::ValidateOptions(const solis::CalculationOptions& o, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    // The integer part is a 1-based day-of-year and the fractional part is
    // time-of-day. Therefore day 366 at 12:00 is represented by 366.5 and is
    // valid, while 367.0 is not.
    if (!std::isfinite(o.time.dayOfYear) || o.time.dayOfYear < 1.0 || o.time.dayOfYear >= 367.0)
        return fail(QStringLiteral("day-of-year must be >= 1 and < 367 (fractional part is time-of-day)"));
    if (!std::isfinite(o.site.latitudeDeg) || o.site.latitudeDeg < -90.0 || o.site.latitudeDeg > 90.0)
        return fail(QStringLiteral("latitude must be in [-90, 90] degrees"));
    if (!std::isfinite(o.site.longitudeDeg) || o.site.longitudeDeg < -180.0 || o.site.longitudeDeg > 180.0)
        return fail(QStringLiteral("longitude must be in [-180, 180] degrees (east positive)"));
    if (!std::isfinite(o.site.elevationM))
        return fail(QStringLiteral("elevation must be finite"));
    if (!std::isfinite(o.site.linkeTurbidity) || o.site.linkeTurbidity < 1.0 || o.site.linkeTurbidity > 12.0)
        return fail(QStringLiteral("Linke turbidity must be in [1, 12]"));
    if (o.site.timeMode == solis::TimeMode::LocalFixedOffset
        && (!std::isfinite(o.site.utcOffsetHours) || o.site.utcOffsetHours < -14.0 || o.site.utcOffsetHours > 14.0))
    {
        return fail(QStringLiteral("UTC offset must be in [-14, 14] hours"));
    }
    if (o.render.resolution == 0)
        return fail(QStringLiteral("render resolution must be greater than zero"));
    if (!std::isfinite(o.render.depthBias) || o.render.depthBias < 0.0 || o.render.depthBias > 0.05)
        return fail(QStringLiteral("depth bias must be in [0, 0.05]"));
    if (o.sampling.diffuseRayCount == 0)
        return fail(QStringLiteral("diffuse ray count must be greater than zero"));
    if (!std::isfinite(o.sampling.directAngularToleranceDeg)
        || o.sampling.directAngularToleranceDeg < 0.0
        || o.sampling.directAngularToleranceDeg > 180.0)
    {
        return fail(QStringLiteral("direct angular tolerance must be in [0, 180] degrees"));
    }
    if (o.time.mode != solis::TemporalMode::Instantaneous)
    {
        if (!std::isfinite(o.time.durationHours) || o.time.durationHours <= 0.0)
            return fail(QStringLiteral("duration must be greater than zero for mean/energy output"));
        if (!std::isfinite(o.time.timestepMinutes) || o.time.timestepMinutes <= 0.0)
            return fail(QStringLiteral("timestep must be greater than zero for mean/energy output"));
    }

    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool SOLIS::GenerateDirectSamples(const solis::CalculationOptions& options,
                                  std::vector<solis::RaySample>& samples)
{
    samples.clear();
    if (!ValidateOptions(options))
        return false;

    try
    {
        if (options.time.mode == solis::TemporalMode::Instantaneous)
        {
            const solis::SolarPosition pos = solis::SolarModel::position(options.time.dayOfYear, options.site);
            if (pos.altitudeDeg <= 0.0)
                return true;
            const solis::ClearSkyIrradiance cs = solis::SolarModel::clearSky(options.time.dayOfYear, options.site, pos);
            if (cs.dniWm2 > 0.0)
                samples.push_back({pos.directionToScene, cs.dniWm2, 1.0});
            return true;
        }

        const double stepHours = options.time.timestepMinutes / 60.0;
        const double duration = options.time.durationHours;
        const size_t estimated = static_cast<size_t>(std::ceil(duration / stepHours));
        samples.reserve(estimated);

        double elapsed = 0.0;
        while (elapsed < duration - 1.0e-12)
        {
            const double dt = std::min(stepHours, duration - elapsed);
            const double sampleDoy = options.time.dayOfYear + (elapsed + 0.5 * dt) / 24.0;
            const solis::SolarPosition pos = solis::SolarModel::position(sampleDoy, options.site);
            if (pos.altitudeDeg > 0.0)
            {
                const solis::ClearSkyIrradiance cs = solis::SolarModel::clearSky(sampleDoy, options.site, pos);
                if (cs.dniWm2 > 0.0)
                {
                    const solis::RaySample sample{pos.directionToScene, cs.dniWm2, dt};
                    if (options.sampling.acceleratedDirectSampling)
                        appendOrMergeDirectSample(samples, sample, options.sampling.directAngularToleranceDeg);
                    else
                        samples.push_back(sample);
                }
            }
            elapsed += dt;
        }
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    return true;
}

bool SOLIS::GenerateDiffuseRays(unsigned N, std::vector<CCVector3d>& rays)
{
    rays.clear();
    if (N == 0)
        return false;

    try
    {
        rays.resize(N);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }

    // Fibonacci lattice with z uniformly distributed in (0,1): equal solid-angle samples.
    const double goldenAngle = Pi * (3.0 - std::sqrt(5.0));
    for (unsigned i = 0; i < N; ++i)
    {
        const double z = (static_cast<double>(i) + 0.5) / static_cast<double>(N);
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double phi = goldenAngle * static_cast<double>(i);
        const CCVector3d surfaceToSky(r * std::cos(phi), r * std::sin(phi), z);
        rays[i] = -surfaceToSky;
    }
    return true;
}

double SOLIS::AggregateDiffuseIrradiance(const solis::CalculationOptions& options)
{
    if (!ValidateOptions(options))
        return std::numeric_limits<double>::quiet_NaN();

    if (options.time.mode == solis::TemporalMode::Instantaneous)
        return sampleDHI(options.time.dayOfYear, options);

    const double stepHours = options.time.timestepMinutes / 60.0;
    const double duration = options.time.durationHours;
    double integralWh = 0.0;
    double elapsed = 0.0;
    while (elapsed < duration - 1.0e-12)
    {
        const double dt = std::min(stepHours, duration - elapsed);
        const double sampleDoy = options.time.dayOfYear + (elapsed + 0.5 * dt) / 24.0;
        integralWh += sampleDHI(sampleDoy, options) * dt;
        elapsed += dt;
    }

    if (options.time.mode == solis::TemporalMode::MeanIrradiance)
        return integralWh / duration;

    return integralWh * energyUnitScale(options.time.energyUnit);
}

bool SOLIS::LaunchDirect(const std::vector<solis::RaySample>& samples,
                         const solis::CalculationOptions& options,
                         GenericCloud* vertices,
                         GenericMesh* mesh,
                         const std::vector<CCVector3d>* normals,
                         GenericProgressCallback* progressCb,
                         const QString& entityName,
                         SOLISAccumulationBackend* backend)
{
    if (!ValidateOptions(options) || !vertices)
        return false;
    if (options.spatialMode == solis::SpatialMode::SurfaceIrradiance
        && (!normals || normals->size() != vertices->size()))
        return false;

    std::vector<double> accumulator;
    if (!initAccumulator(vertices, accumulator))
        return false;

    setupProgress(progressCb, QStringLiteral("SOLIS - direct"), entityName,
                  static_cast<unsigned>(samples.size()), mesh, vertices->size());
    NormalizedProgress progress(progressCb, std::max<unsigned>(1u, static_cast<unsigned>(samples.size())));

    if (samples.empty())
    {
        writeAccumulator(vertices, accumulator, 1.0);
        return true; // sun below horizon for the complete interval is a valid zero result
    }

    std::unique_ptr<SOLISAccumulationBackend> ownedBackend;
    if (!backend)
    {
        ownedBackend = CreateSOLISAccumulationBackend();
        backend = ownedBackend.get();
        if (!backend || !backend->init(options.render.resolution,
                                       options.render.resolution,
                                       vertices,
                                       mesh,
                                       options.render.meshIsClosed,
                                       options.render.depthBias,
                                       options.render.parallelCpuProjection,
                                       options.render.cpuThreadCount))
            return false;
    }

    double totalTimeWeight = 0.0;
    for (const solis::RaySample& sample : samples)
        totalTimeWeight += sample.weightHours;

    for (const solis::RaySample& sample : samples)
    {
        double scalarWeight = 1.0;
        if (options.spatialMode == solis::SpatialMode::Visibility)
        {
            scalarWeight = (options.time.mode == solis::TemporalMode::Instantaneous)
                         ? 1.0
                         : sample.weightHours;
        }
        else
        {
            scalarWeight = sample.irradianceWm2;
            if (options.time.mode != solis::TemporalMode::Instantaneous)
                scalarWeight *= sample.weightHours;
        }

        const int visible = backend->accumulate(accumulator,
                                                sample.direction,
                                                scalarWeight,
                                                options.spatialMode,
                                                normals);
        if (visible < 0)
            return false;


        if (progressCb && !progress.oneStep())
            return false;
    }

    double scale = 1.0;
    if (options.spatialMode == solis::SpatialMode::Visibility)
    {
        if (options.time.mode != solis::TemporalMode::Instantaneous)
            scale = totalTimeWeight > 0.0 ? 1.0 / totalTimeWeight : 0.0;
    }
    else if (options.time.mode == solis::TemporalMode::MeanIrradiance)
    {
        scale = 1.0 / options.time.durationHours;
    }
    else if (options.time.mode == solis::TemporalMode::IntegratedEnergy)
    {
        scale = energyUnitScale(options.time.energyUnit);
    }


    writeAccumulator(vertices, accumulator, scale);
    return true;
}

bool SOLIS::LaunchDiffuse(const std::vector<CCVector3d>& rays,
                          double diffuseIrradiance,
                          const solis::CalculationOptions& options,
                          GenericCloud* vertices,
                          GenericMesh* mesh,
                          const std::vector<CCVector3d>* normals,
                          GenericProgressCallback* progressCb,
                          const QString& entityName,
                          SOLISAccumulationBackend* backend)
{
    if (!ValidateOptions(options) || rays.empty() || !vertices || !std::isfinite(diffuseIrradiance))
        return false;
    if (options.spatialMode == solis::SpatialMode::SurfaceIrradiance
        && (!normals || normals->size() != vertices->size()))
        return false;

    std::vector<double> accumulator;
    if (!initAccumulator(vertices, accumulator))
        return false;

    setupProgress(progressCb, QStringLiteral("SOLIS - diffuse"), entityName,
                  static_cast<unsigned>(rays.size()), mesh, vertices->size());
    NormalizedProgress progress(progressCb, static_cast<unsigned>(rays.size()));

    std::unique_ptr<SOLISAccumulationBackend> ownedBackend;
    if (!backend)
    {
        ownedBackend = CreateSOLISAccumulationBackend();
        backend = ownedBackend.get();
        if (!backend || !backend->init(options.render.resolution,
                                       options.render.resolution,
                                       vertices,
                                       mesh,
                                       options.render.meshIsClosed,
                                       options.render.depthBias,
                                       options.render.parallelCpuProjection,
                                       options.render.cpuThreadCount))
            return false;
    }

    // Uniform hemisphere samples have solid angle 2*pi/N. For isotropic radiance L=DHI/pi,
    // each irradiance sample therefore carries 2*DHI/N before cosine incidence.
    const double rayWeight = (options.spatialMode == solis::SpatialMode::Visibility)
                           ? 1.0 / static_cast<double>(rays.size())
                           : 2.0 * diffuseIrradiance / static_cast<double>(rays.size());

    for (const CCVector3d& ray : rays)
    {
        if (backend->accumulate(accumulator, ray, rayWeight, options.spatialMode, normals) < 0)
            return false;
        if (progressCb && !progress.oneStep())
            return false;
    }

    writeAccumulator(vertices, accumulator, 1.0);
    return true;
}

QString SOLIS::OutputUnit(const solis::CalculationOptions& options)
{
    if (options.spatialMode == solis::SpatialMode::Visibility)
        return QStringLiteral("fraction");
    if (options.time.mode == solis::TemporalMode::IntegratedEnergy)
        return options.time.energyUnit == solis::EnergyUnit::KWhPerM2
             ? QStringLiteral("kWh/m2")
             : QStringLiteral("Wh/m2");
    return QStringLiteral("W/m2");
}

double SOLIS::energyUnitScale(solis::EnergyUnit unit)
{
    return unit == solis::EnergyUnit::KWhPerM2 ? 0.001 : 1.0;
}
