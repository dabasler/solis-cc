#include "SOLISCommand.h"

#include "SOLIS.h"
#include "SOLISBatch.h"
#include "SOLISBackend.h"
#include "SOLISResults.h"
#include "SOLISVersion.h"

#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

#include <QTime>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <vector>

namespace
{
constexpr char COMMAND_SOLIS[] = "SOLIS";

enum class ComponentMode
{
    Direct,
    Diffuse,
    Total,
    All
};

bool takeString(ccCommandLineInterface& cmd, const char* option, QString& value)
{
    if (cmd.arguments().empty())
        return cmd.error(QObject::tr("Missing value after -%1").arg(QString::fromLatin1(option)));
    value = cmd.arguments().takeFirst();
    return true;
}

bool takeDouble(ccCommandLineInterface& cmd, const char* option, double& value)
{
    QString token;
    if (!takeString(cmd, option, token))
        return false;

    bool ok = false;
    const double parsed = token.toDouble(&ok);
    if (!ok || !std::isfinite(parsed))
        return cmd.error(QObject::tr("Invalid numeric value '%1' after -%2")
                             .arg(token, QString::fromLatin1(option)));
    value = parsed;
    return true;
}

bool takeUnsigned(ccCommandLineInterface& cmd, const char* option, unsigned& value)
{
    QString token;
    if (!takeString(cmd, option, token))
        return false;

    bool ok = false;
    const qulonglong parsed = token.toULongLong(&ok);
    if (!ok || parsed > static_cast<qulonglong>(std::numeric_limits<unsigned>::max()))
        return cmd.error(QObject::tr("Invalid unsigned integer '%1' after -%2")
                             .arg(token, QString::fromLatin1(option)));
    value = static_cast<unsigned>(parsed);
    return true;
}

bool parseClock(const QString& value, double& clockHours)
{
    QTime time = QTime::fromString(value, QStringLiteral("H:mm:ss"));
    if (!time.isValid())
        time = QTime::fromString(value, QStringLiteral("H:mm"));
    if (!time.isValid())
        return false;

    clockHours = static_cast<double>(time.hour())
               + static_cast<double>(time.minute()) / 60.0
               + static_cast<double>(time.second()) / 3600.0;
    return true;
}

bool extractNormals(ccPointCloud* cloud, std::vector<CCVector3d>& normals)
{
    normals.clear();
    if (!cloud || !cloud->hasNormals())
        return false;

    try
    {
        normals.resize(cloud->size());
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }

    for (unsigned i = 0; i < cloud->size(); ++i)
    {
        const CCVector3& n = cloud->getPointNormal(i);
        CCVector3d nd(static_cast<double>(n.x), static_cast<double>(n.y), static_cast<double>(n.z));
        if (nd.norm2() > 1.0e-20)
            nd.normalize();
        normals[i] = nd;
    }
    return true;
}

QString componentName(ComponentMode mode)
{
    switch (mode)
    {
    case ComponentMode::Direct: return QStringLiteral("DIRECT");
    case ComponentMode::Diffuse: return QStringLiteral("DIFFUSE");
    case ComponentMode::Total: return QStringLiteral("TOTAL");
    case ComponentMode::All: return QStringLiteral("ALL");
    }
    return QStringLiteral("UNKNOWN");
}

QString temporalModeName(solis::TemporalMode mode)
{
    switch (mode)
    {
    case solis::TemporalMode::Instantaneous: return QStringLiteral("INSTANT");
    case solis::TemporalMode::MeanIrradiance: return QStringLiteral("MEAN");
    case solis::TemporalMode::IntegratedEnergy: return QStringLiteral("ENERGY");
    }
    return QStringLiteral("UNKNOWN");
}

QString spatialModeName(solis::SpatialMode mode)
{
    switch (mode)
    {
    case solis::SpatialMode::Visibility: return QStringLiteral("VISIBILITY");
    case solis::SpatialMode::HorizontalIrradiance: return QStringLiteral("HORIZONTAL");
    case solis::SpatialMode::SurfaceIrradiance: return QStringLiteral("SURFACE");
    }
    return QStringLiteral("UNKNOWN");
}

bool processEntityStep(ccCommandLineInterface& cmd,
                       ccPointCloud* cloud,
                       ccGenericMesh* mesh,
                       const QString& entityName,
                       ComponentMode component,
                       const solis::CalculationOptions& options,
                       const std::vector<solis::RaySample>& directSamples,
                       const std::vector<CCVector3d>& diffuseRays,
                       double diffuseIrradiance,
                       const QString& runId,
                       const solis::BatchContext* batchContext,
                       SOLISAccumulationBackend* backend,
                       const std::vector<CCVector3d>* normalsPtr,
                       ccProgressDialog* progress)
{
    const bool needDirect = component == ComponentMode::Direct
                         || component == ComponentMode::Total
                         || component == ComponentMode::All;
    const bool needDiffuse = component == ComponentMode::Diffuse
                          || component == ComponentMode::Total
                          || component == ComponentMode::All;
    const bool keepDirect = component == ComponentMode::Direct || component == ComponentMode::All;
    const bool keepDiffuse = component == ComponentMode::Diffuse || component == ComponentMode::All;
    const bool makeTotal = (component == ComponentMode::Total || component == ComponentMode::All)
                        && options.spatialMode != solis::SpatialMode::Visibility;

    solis::ResultField directResult;
    solis::ResultField diffuseResult;
    solis::ResultField totalResult;

    if (needDirect)
    {
        const bool allocated = keepDirect
            ? solis::ResultManager::CreateResultField(cloud, solis::ResultComponent::Direct, options, directResult)
            : solis::ResultManager::CreateTemporaryField(cloud, solis::ResultComponent::Direct, runId, directResult);
        if (!allocated)
            return cmd.error(QObject::tr("SOLIS: failed to allocate Direct scalar field on '%1'").arg(entityName));

        if (!SOLIS::LaunchDirect(directSamples, options, cloud, mesh, normalsPtr, progress, entityName, backend))
        {
            cloud->deleteScalarField(directResult.index);
            if (progress && progress->wasCanceled())
                return cmd.error(QObject::tr("SOLIS: calculation canceled by user"));
            return cmd.error(QObject::tr("SOLIS: Direct calculation failed for '%1'").arg(entityName));
        }

        solis::ResultManager::FinalizeField(cloud, directResult.index);
        if (keepDirect)
        {
            solis::ResultManager::StoreMetadata(cloud, directResult, solis::ResultComponent::Direct,
                                                options, runId, entityName, batchContext);
            cmd.print(QObject::tr("SOLIS: created scalar field '%1'").arg(directResult.name));
        }
    }

    if (needDiffuse)
    {
        const bool allocated = keepDiffuse
            ? solis::ResultManager::CreateResultField(cloud, solis::ResultComponent::Diffuse, options, diffuseResult)
            : solis::ResultManager::CreateTemporaryField(cloud, solis::ResultComponent::Diffuse, runId, diffuseResult);
        if (!allocated)
            return cmd.error(QObject::tr("SOLIS: failed to allocate Diffuse scalar field on '%1'").arg(entityName));

        if (!SOLIS::LaunchDiffuse(diffuseRays, diffuseIrradiance, options, cloud, mesh, normalsPtr,
                                  progress, entityName, backend))
        {
            cloud->deleteScalarField(diffuseResult.index);
            if (progress && progress->wasCanceled())
                return cmd.error(QObject::tr("SOLIS: calculation canceled by user"));
            return cmd.error(QObject::tr("SOLIS: Diffuse calculation failed for '%1'").arg(entityName));
        }

        solis::ResultManager::FinalizeField(cloud, diffuseResult.index);
        if (keepDiffuse)
        {
            solis::ResultManager::StoreMetadata(cloud, diffuseResult, solis::ResultComponent::Diffuse,
                                                options, runId, entityName, batchContext);
            cmd.print(QObject::tr("SOLIS: created scalar field '%1'").arg(diffuseResult.name));
        }
    }

    if (makeTotal)
    {
        if (!solis::ResultManager::CreateTotalField(cloud, directResult.index, diffuseResult.index, options, totalResult))
            return cmd.error(QObject::tr("SOLIS: failed to create Total scalar field on '%1'").arg(entityName));

        solis::ResultManager::StoreMetadata(cloud, totalResult, solis::ResultComponent::Total,
                                            options, runId, entityName, batchContext);
        cmd.print(QObject::tr("SOLIS: created scalar field '%1'").arg(totalResult.name));
    }

    // TOTAL requests use Direct and Diffuse only as private working fields.
    if (component == ComponentMode::Total)
    {
        int idx = cloud->getScalarFieldIndexByName(qPrintable(diffuseResult.name));
        if (idx >= 0)
            cloud->deleteScalarField(idx);
        idx = cloud->getScalarFieldIndexByName(qPrintable(directResult.name));
        if (idx >= 0)
            cloud->deleteScalarField(idx);
        totalResult.index = cloud->getScalarFieldIndexByName(qPrintable(totalResult.name));
    }

    if (makeTotal)
        solis::ResultManager::FinalizeField(cloud, totalResult.index);
    else if (keepDiffuse)
        solis::ResultManager::FinalizeField(cloud, diffuseResult.index);
    else if (keepDirect)
        solis::ResultManager::FinalizeField(cloud, directResult.index);

    return true;
}

bool processEntityBatch(ccCommandLineInterface& cmd,
                        ccPointCloud* cloud,
                        ccGenericMesh* mesh,
                        const QString& entityName,
                        ComponentMode component,
                        const solis::CalculationOptions& baseOptions,
                        const solis::BatchOptions& batchOptions,
                        const std::vector<double>& batchTimes,
                        const std::vector<CCVector3d>& diffuseRays,
                        const QString& batchId)
{
    if (!cloud)
        return cmd.error(QObject::tr("SOLIS: invalid point-cloud/mesh vertex cloud for '%1'").arg(entityName));
    if (cloud->size() == 0)
    {
        cmd.warning(QObject::tr("SOLIS: entity '%1' is empty; skipped").arg(entityName));
        return true;
    }

    const bool needDirect = component == ComponentMode::Direct
                         || component == ComponentMode::Total
                         || component == ComponentMode::All;
    const bool needDiffuse = component == ComponentMode::Diffuse
                          || component == ComponentMode::Total
                          || component == ComponentMode::All;

    std::vector<CCVector3d> normals;
    const std::vector<CCVector3d>* normalsPtr = nullptr;
    if (baseOptions.spatialMode == solis::SpatialMode::SurfaceIrradiance)
    {
        if (!extractNormals(cloud, normals))
        {
            return cmd.error(QObject::tr("SOLIS: surface irradiance requires existing point/vertex normals on '%1'")
                                 .arg(entityName));
        }
        normalsPtr = &normals;
    }

    std::unique_ptr<SOLISAccumulationBackend> backend = CreateSOLISAccumulationBackend();
    if (!backend || !backend->init(baseOptions.render.resolution,
                                   baseOptions.render.resolution,
                                   cloud,
                                   mesh,
                                   baseOptions.render.meshIsClosed,
                                   baseOptions.render.depthBias,
                                   baseOptions.render.parallelCpuProjection,
                                   baseOptions.render.cpuThreadCount))
    {
        return cmd.error(QObject::tr("SOLIS: renderer initialization failed for '%1'").arg(entityName));
    }

    std::unique_ptr<ccProgressDialog> progress;
    if (!cmd.silentMode())
    {
        progress = std::make_unique<ccProgressDialog>(true, cmd.widgetParent());
        progress->setAutoClose(false);
    }

    const bool isBatch = batchTimes.size() > 1;
    solis::BatchContext batchContext;
    if (isBatch)
    {
        batchContext.batchId = batchId;
        batchContext.count = static_cast<unsigned>(batchTimes.size());
        batchContext.startDayOfYear = batchTimes.front();
        batchContext.endDayOfYear = batchOptions.endDayOfYear;
        batchContext.stepMinutes = batchOptions.stepMinutes;
        solis::ResultManager::StoreBatchMetadata(cloud, batchContext, entityName);
    }

    for (size_t i = 0; i < batchTimes.size(); ++i)
    {
        solis::CalculationOptions options = baseOptions;
        options.time.dayOfYear = batchTimes[i];

        std::vector<solis::RaySample> directSamples;
        if (needDirect && !SOLIS::GenerateDirectSamples(options, directSamples))
            return cmd.error(QObject::tr("SOLIS: failed to generate direct solar samples at batch point %1").arg(static_cast<qulonglong>(i + 1)));

        double diffuseIrradiance = 0.0;
        if (needDiffuse)
        {
            diffuseIrradiance = SOLIS::AggregateDiffuseIrradiance(options);
            if (!std::isfinite(diffuseIrradiance))
                return cmd.error(QObject::tr("SOLIS: failed to calculate diffuse irradiance at batch point %1").arg(static_cast<qulonglong>(i + 1)));
        }

        const QString runId = solis::ResultManager::NewRunId();
        if (isBatch)
        {
            batchContext.index = static_cast<unsigned>(i);
            cmd.print(QObject::tr("SOLIS batch %1/%2, run ID: %3")
                          .arg(static_cast<qulonglong>(i + 1))
                          .arg(static_cast<qulonglong>(batchTimes.size()))
                          .arg(runId));
        }
        else
        {
            cmd.print(QObject::tr("SOLIS run ID: %1").arg(runId));
        }

        if (!processEntityStep(cmd,
                               cloud,
                               mesh,
                               entityName,
                               component,
                               options,
                               directSamples,
                               diffuseRays,
                               diffuseIrradiance,
                               runId,
                               isBatch ? &batchContext : nullptr,
                               backend.get(),
                               normalsPtr,
                               progress.get()))
        {
            return false;
        }
    }

    cmd.print(QObject::tr("SOLIS: '%1' complete; %2 time point(s), %3 point/vertex values per field")
                  .arg(entityName)
                  .arg(static_cast<qulonglong>(batchTimes.size()))
                  .arg(cloud->size()));
    return true;
}

} // namespace

SOLISCommand::SOLISCommand()
    : ccCommandLineInterface::Command("SOLIS", COMMAND_SOLIS)
{
}

bool SOLISCommand::process(ccCommandLineInterface& cmd)
{
    cmd.print(QObject::tr("[SOLIS v%1]").arg(QString::fromLatin1(solis::Version)));

    if (cmd.clouds().empty() && cmd.meshes().empty())
        return cmd.error(QObject::tr("SOLIS: no cloud or mesh is loaded"));

    solis::CalculationOptions options;
    options.site.latitudeDeg = 45.0;
    options.site.longitudeDeg = 0.0;
    options.site.elevationM = 0.0;
    options.site.linkeTurbidity = 3.0;
    options.site.timeMode = solis::TimeMode::UTC;
    options.site.utcOffsetHours = 0.0;
    options.time.dayOfYear = 172.5;
    options.time.mode = solis::TemporalMode::IntegratedEnergy;
    options.time.durationHours = 24.0;
    options.time.timestepMinutes = 5.0;
    options.time.energyUnit = solis::EnergyUnit::WhPerM2;
    options.spatialMode = solis::SpatialMode::HorizontalIrradiance;
    options.render.resolution = 1024;
    options.render.meshIsClosed = false;
    options.render.depthBias = 1.0e-3;
    options.render.parallelCpuProjection = true;
    options.render.cpuThreadCount = 0;
    options.sampling.diffuseRayCount = 256;
    options.sampling.acceleratedDirectSampling = false;
    options.sampling.directAngularToleranceDeg = 0.1;

    ComponentMode component = ComponentMode::All;
    bool explicitClockTime = false;
    double explicitClockHours = 0.0;

    bool batchEnabled = false;
    bool batchEndDoySet = false;
    bool batchEndTimeSet = false;
    bool batchEndSpecified = false;
    double batchEndDoy = 0.0;
    double batchEndClockHours = 0.0;
    solis::BatchOptions batchOptions;
    batchOptions.stepMinutes = 60.0;

    while (!cmd.arguments().empty())
    {
        const QString arg = cmd.arguments().front();

        if (ccCommandLineInterface::IsCommand(arg, "COMPONENT")
            || ccCommandLineInterface::IsCommand(arg, "TYPE"))
        {
            const char* option = ccCommandLineInterface::IsCommand(arg, "COMPONENT") ? "COMPONENT" : "TYPE";
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, option, value)) return false;
            value = value.toUpper();
            if (value == QLatin1String("DIRECT")) component = ComponentMode::Direct;
            else if (value == QLatin1String("DIFFUSE")) component = ComponentMode::Diffuse;
            else if (value == QLatin1String("TOTAL")) component = ComponentMode::Total;
            else if (value == QLatin1String("ALL") || value == QLatin1String("BOTH")) component = ComponentMode::All;
            else return cmd.error(QObject::tr("SOLIS: invalid -%1 value '%2' (DIRECT|DIFFUSE|TOTAL|ALL)")
                                      .arg(QString::fromLatin1(option), value));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DOY"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DOY", options.time.dayOfYear)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIME"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "TIME", value)) return false;
            if (!parseClock(value, explicitClockHours))
                return cmd.error(QObject::tr("SOLIS: invalid -TIME '%1' (expected HH:MM or HH:MM:SS)").arg(value));
            explicitClockTime = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DURATION"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DURATION", options.time.durationHours)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "INT"))
        {
            cmd.arguments().pop_front();
            double legacyDuration = 0.0;
            if (!takeDouble(cmd, "INT", legacyDuration)) return false;
            if (legacyDuration < 0.0)
                options.time.mode = solis::TemporalMode::Instantaneous;
            else
            {
                options.time.durationHours = legacyDuration;
                options.time.mode = solis::TemporalMode::IntegratedEnergy;
            }
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIMESTEP")
                 || ccCommandLineInterface::IsCommand(arg, "TS"))
        {
            const char* option = ccCommandLineInterface::IsCommand(arg, "TIMESTEP") ? "TIMESTEP" : "TS";
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, option, options.time.timestepMinutes)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LAT"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LAT", options.site.latitudeDeg)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LON"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LON", options.site.longitudeDeg)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "ELV"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "ELV", options.site.elevationM)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DIFFUSE_RAYS")
                 || ccCommandLineInterface::IsCommand(arg, "NRAYS"))
        {
            const char* option = ccCommandLineInterface::IsCommand(arg, "DIFFUSE_RAYS") ? "DIFFUSE_RAYS" : "NRAYS";
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, option, options.sampling.diffuseRayCount)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "RESOLUTION"))
        {
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, "RESOLUTION", options.render.resolution)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "OUTPUT_MODE"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "OUTPUT_MODE", value)) return false;
            value = value.toUpper();
            if (value == QLatin1String("INSTANT") || value == QLatin1String("INSTANTANEOUS"))
                options.time.mode = solis::TemporalMode::Instantaneous;
            else if (value == QLatin1String("MEAN") || value == QLatin1String("AVERAGE"))
                options.time.mode = solis::TemporalMode::MeanIrradiance;
            else if (value == QLatin1String("ENERGY") || value == QLatin1String("INTEGRATED"))
                options.time.mode = solis::TemporalMode::IntegratedEnergy;
            else
                return cmd.error(QObject::tr("SOLIS: invalid -OUTPUT_MODE '%1' (INSTANT|MEAN|ENERGY)").arg(value));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "SPATIAL_MODE"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "SPATIAL_MODE", value)) return false;
            value = value.toUpper();
            if (value == QLatin1String("VISIBILITY")) options.spatialMode = solis::SpatialMode::Visibility;
            else if (value == QLatin1String("HORIZONTAL")) options.spatialMode = solis::SpatialMode::HorizontalIrradiance;
            else if (value == QLatin1String("SURFACE")) options.spatialMode = solis::SpatialMode::SurfaceIrradiance;
            else return cmd.error(QObject::tr("SOLIS: invalid -SPATIAL_MODE '%1' (VISIBILITY|HORIZONTAL|SURFACE)").arg(value));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "ENERGY_UNIT"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "ENERGY_UNIT", value)) return false;
            value = value.toUpper();
            if (value == QLatin1String("WH") || value == QLatin1String("WH/M2"))
                options.time.energyUnit = solis::EnergyUnit::WhPerM2;
            else if (value == QLatin1String("KWH") || value == QLatin1String("KWH/M2"))
                options.time.energyUnit = solis::EnergyUnit::KWhPerM2;
            else return cmd.error(QObject::tr("SOLIS: invalid -ENERGY_UNIT '%1' (WH|KWH)").arg(value));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIME_MODE"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "TIME_MODE", value)) return false;
            value = value.toUpper();
            if (value == QLatin1String("UTC")) options.site.timeMode = solis::TimeMode::UTC;
            else if (value == QLatin1String("LOCAL")) options.site.timeMode = solis::TimeMode::LocalFixedOffset;
            else return cmd.error(QObject::tr("SOLIS: invalid -TIME_MODE '%1' (UTC|LOCAL)").arg(value));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "UTC_OFFSET"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "UTC_OFFSET", options.site.utcOffsetHours)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LINKE_TURBIDITY"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LINKE_TURBIDITY", options.site.linkeTurbidity)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "ACCEL_ANGLE"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "ACCEL_ANGLE", options.sampling.directAngularToleranceDeg)) return false;
            options.sampling.acceleratedDirectSampling = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "NO_ACCEL"))
        {
            cmd.arguments().pop_front();
            options.sampling.acceleratedDirectSampling = false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DEPTH_BIAS"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DEPTH_BIAS", options.render.depthBias)) return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "THREADS"))
        {
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, "THREADS", options.render.cpuThreadCount)) return false;
            options.render.parallelCpuProjection = options.render.cpuThreadCount != 1;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "IS_CLOSED"))
        {
            cmd.arguments().pop_front();
            options.render.meshIsClosed = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "BATCH_END_DOY"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "BATCH_END_DOY", batchEndDoy)) return false;
            batchEnabled = batchEndDoySet = batchEndSpecified = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "BATCH_END_TIME"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "BATCH_END_TIME", value)) return false;
            if (!parseClock(value, batchEndClockHours))
                return cmd.error(QObject::tr("SOLIS: invalid -BATCH_END_TIME '%1' (expected HH:MM or HH:MM:SS)").arg(value));
            batchEnabled = batchEndTimeSet = batchEndSpecified = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "BATCH_STEP"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "BATCH_STEP", batchOptions.stepMinutes)) return false;
            batchEnabled = true;
        }
        else
        {
            break;
        }
    }

    if (explicitClockTime)
        options.time.dayOfYear = std::floor(options.time.dayOfYear) + explicitClockHours / 24.0;

    if (options.render.resolution < 64 || options.render.resolution > 16384)
        return cmd.error(QObject::tr("SOLIS: -RESOLUTION must be in [64, 16384]"));
    if (options.sampling.diffuseRayCount < 1 || options.sampling.diffuseRayCount > 65536)
        return cmd.error(QObject::tr("SOLIS: diffuse ray count must be in [1, 65536]"));
    if (options.render.cpuThreadCount > 1024)
        return cmd.error(QObject::tr("SOLIS: -THREADS must be in [0, 1024]"));
    if (options.spatialMode == solis::SpatialMode::Visibility && component == ComponentMode::Total)
        return cmd.error(QObject::tr("SOLIS: TOTAL is undefined for VISIBILITY mode; use DIRECT, DIFFUSE, or ALL"));

    QString validationError;
    if (!SOLIS::ValidateOptions(options, &validationError))
        return cmd.error(QObject::tr("SOLIS: invalid options: %1").arg(validationError));

    if (batchEnabled && !batchEndSpecified)
        return cmd.error(QObject::tr("SOLIS: batch mode requires -BATCH_END_TIME and/or -BATCH_END_DOY"));

    if (batchEnabled)
    {
        const double startDay = std::floor(options.time.dayOfYear);
        const double startClockHours = (options.time.dayOfYear - startDay) * 24.0;
        const double endDay = batchEndDoySet ? std::floor(batchEndDoy) : startDay;
        const double endClock = batchEndTimeSet ? batchEndClockHours : startClockHours;
        batchOptions.enabled = true;
        batchOptions.endDayOfYear = endDay + endClock / 24.0;
    }
    else
    {
        batchOptions.enabled = false;
        batchOptions.endDayOfYear = options.time.dayOfYear;
    }

    std::vector<double> batchTimes;
    QString batchError;
    if (!solis::BuildBatchTimes(options, batchOptions, batchTimes, &batchError))
        return cmd.error(QObject::tr("SOLIS: invalid batch: %1").arg(batchError));

    const bool needDirect = component == ComponentMode::Direct
                         || component == ComponentMode::Total
                         || component == ComponentMode::All;
    const bool needDiffuse = component == ComponentMode::Diffuse
                          || component == ComponentMode::Total
                          || component == ComponentMode::All;

    std::vector<CCVector3d> diffuseRays;
    if (needDiffuse && !SOLIS::GenerateDiffuseRays(options.sampling.diffuseRayCount, diffuseRays))
        return cmd.error(QObject::tr("SOLIS: failed to generate diffuse sky rays"));

    cmd.print(QObject::tr("SOLIS settings: component=%1, temporal=%2, spatial=%3, DOY=%4, lat=%5, lon=%6, resolution=%7")
                  .arg(componentName(component))
                  .arg(temporalModeName(options.time.mode))
                  .arg(spatialModeName(options.spatialMode))
                  .arg(options.time.dayOfYear, 0, 'g', 10)
                  .arg(options.site.latitudeDeg, 0, 'g', 10)
                  .arg(options.site.longitudeDeg, 0, 'g', 10)
                  .arg(options.render.resolution));
    if (needDiffuse)
        cmd.print(QObject::tr("SOLIS: %1 diffuse sky ray(s)").arg(static_cast<qulonglong>(diffuseRays.size())));

    const bool isBatch = batchTimes.size() > 1;
    const QString batchId = isBatch ? solis::ResultManager::NewRunId() : QString();
    if (isBatch)
    {
        cmd.print(QObject::tr("SOLIS batch: %1 time points, step=%2 min, batch ID=%3")
                      .arg(static_cast<qulonglong>(batchTimes.size()))
                      .arg(batchOptions.stepMinutes, 0, 'g', 10)
                      .arg(batchId));
    }

    for (CLCloudDesc& desc : cmd.clouds())
    {
        if (!processEntityBatch(cmd,
                                desc.pc,
                                nullptr,
                                desc.pc ? desc.pc->getName() : QStringLiteral("cloud"),
                                component,
                                options,
                                batchOptions,
                                batchTimes,
                                diffuseRays,
                                batchId))
        {
            return false;
        }
    }

    for (CLMeshDesc& desc : cmd.meshes())
    {
        ccGenericMesh* mesh = desc.mesh;
        ccPointCloud* cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        if (!processEntityBatch(cmd,
                                cloud,
                                mesh,
                                mesh ? mesh->getName() : QStringLiteral("mesh"),
                                component,
                                options,
                                batchOptions,
                                batchTimes,
                                diffuseRays,
                                batchId))
        {
            return false;
        }
    }

    if (cmd.autoSaveMode())
    {
        const QString suffix = isBatch
            ? QStringLiteral("SOLIS_%1_BATCH").arg(componentName(component))
            : QStringLiteral("SOLIS_%1").arg(componentName(component));
        for (CLCloudDesc& desc : cmd.clouds())
        {
            const QString error = cmd.exportEntity(desc, suffix);
            if (!error.isEmpty())
                return cmd.error(error);
        }
        for (CLMeshDesc& desc : cmd.meshes())
        {
            const QString error = cmd.exportEntity(desc, suffix);
            if (!error.isEmpty())
                return cmd.error(error);
        }
    }

    return true;
}
