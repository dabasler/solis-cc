#include "SOLISExposureCommand.h"

#include "SOLISBackend.h"
#include "SOLISCrownExposure.h"
#include "SOLISExposure.h"
#include "SOLISResults.h"
#include "SOLISVersion.h"

#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>
#include <ccScalarField.h>

#include <QTime>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
constexpr char COMMAND_SOLIS_EXPOSURE[] = "SOLIS_EXPOSURE";

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
    {
        return cmd.error(QObject::tr("Invalid numeric value '%1' after -%2")
                             .arg(token)
                             .arg(QString::fromLatin1(option)));
    }

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
    if (!ok || parsed > std::numeric_limits<unsigned>::max())
    {
        return cmd.error(QObject::tr("Invalid unsigned integer '%1' after -%2")
                             .arg(token)
                             .arg(QString::fromLatin1(option)));
    }

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

QString optionalFraction(double value)
{
    return std::isfinite(value) ? QString::number(value, 'f', 4) : QStringLiteral("n/a");
}

bool writeField(ccPointCloud* cloud,
                const QString& baseName,
                const std::vector<double>& values,
                solis::ResultField& field,
                QString* errorMessage)
{
    if (!cloud || values.size() != cloud->size()
        || !solis::ResultManager::CreateNamedResultField(cloud, baseName, field))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("failed to allocate exposure scalar field");
        return false;
    }

    auto sf = cloud->getScalarField(field.index);
    if (!sf)
    {
        cloud->deleteScalarField(field.index);
        field = solis::ResultField{};
        if (errorMessage)
            *errorMessage = QStringLiteral("failed to access exposure scalar field");
        return false;
    }

    for (unsigned i = 0; i < cloud->size(); ++i)
        sf->setValue(i, static_cast<ScalarType>(values[i]));

    solis::ResultManager::FinalizeField(cloud, field.index);
    return true;
}

struct CommandExposureEntity
{
    ccPointCloud* cloud = nullptr;
    ccGenericMesh* mesh = nullptr;
    QString name;
};

void appendCommandOccluder(const CommandExposureEntity& source,
                           std::vector<SOLISOccluder>& occluders,
                           QStringList& names)
{
    if (!source.cloud)
        return;
    const auto existing = std::find_if(occluders.begin(), occluders.end(),
                                       [&](const SOLISOccluder& item)
                                       {
                                           return item.cloud == source.cloud && item.mesh == source.mesh;
                                       });
    if (existing != occluders.end())
        return;
    occluders.push_back({source.cloud, source.mesh});
    names.push_back(source.name);
}

void commandContextForTarget(const CommandExposureEntity& target,
                             const std::vector<CommandExposureEntity>& loadedEntities,
                             solis::ExposureOcclusionContext mode,
                             std::vector<SOLISOccluder>& occluders,
                             solis::ExposureContextInfo& info)
{
    occluders.clear();
    info = solis::ExposureContextInfo{};
    info.mode = mode;
    if (mode == solis::ExposureOcclusionContext::TargetOnly)
    {
        appendCommandOccluder(target, occluders, info.entityNames);
        return;
    }
    for (const CommandExposureEntity& source : loadedEntities)
        appendCommandOccluder(source, occluders, info.entityNames);
    appendCommandOccluder(target, occluders, info.entityNames);
}

bool processEntity(ccCommandLineInterface& cmd,
                   ccPointCloud* cloud,
                   ccGenericMesh* mesh,
                   const QString& entityName,
                   const solis::ExposureOptions& baseOptions,
                   const std::vector<SOLISOccluder>& occluders,
                   const solis::ExposureContextInfo& contextInfo)
{
    if (!cloud)
        return cmd.error(QObject::tr("SOLIS Exposure: entity has no point/vertex cloud"));

    solis::ExposureOptions options = baseOptions;
    options.calculation.render.meshIsClosed = mesh && baseOptions.calculation.render.meshIsClosed;

    std::unique_ptr<SOLISAccumulationBackend> backend = CreateSOLISAccumulationBackend();
    if (!backend
        || !backend->setOcclusionContext(occluders)
        || !backend->init(options.calculation.render.resolution,
                          options.calculation.render.resolution,
                          cloud,
                          mesh,
                          options.calculation.render.meshIsClosed,
                          options.calculation.render.depthBias,
                          options.calculation.render.parallelCpuProjection,
                          options.calculation.render.cpuThreadCount))
    {
        return cmd.error(QObject::tr("SOLIS Exposure: renderer initialization failed for '%1'")
                             .arg(entityName));
    }

    cmd.print(QObject::tr("SOLIS Exposure '%1': context=%2, occluding entities=%3")
                  .arg(entityName, solis::ExposureCalculator::OcclusionContextName(contextInfo.mode))
                  .arg(contextInfo.entityNames.size()));

    solis::ExposureResult exposure;
    QString error;
    if (!solis::ExposureCalculator::Calculate(options,
                                               cloud,
                                               mesh,
                                               exposure,
                                               nullptr,
                                               entityName,
                                               backend.get(),
                                               &error))
    {
        return cmd.error(QObject::tr("SOLIS Exposure: %1: %2")
                             .arg(entityName)
                             .arg(error));
    }

    const QString runId = solis::ResultManager::NewRunId();
    QStringList resultIds;

    auto store = [&](const QString& metric,
                     const QString& baseName,
                     const std::vector<double>& values) -> bool
    {
        solis::ResultField field;
        if (!writeField(cloud, baseName, values, field, &error))
            return false;

        solis::ResultManager::StoreExposureMetadata(cloud,
                                                    field,
                                                    metric,
                                                    options,
                                                    exposure.reference,
                                                    contextInfo,
                                                    runId,
                                                    entityName);
        resultIds.push_back(field.resultId);
        cmd.print(QObject::tr("SOLIS Exposure: created scalar field '%1'").arg(field.name));
        return true;
    };

    if (solis::ExposureCalculator::HasMetric(options, solis::ExposureSky)
        && !store(QStringLiteral("sky"),
                  solis::ExposureCalculator::SkyFieldBaseName(options),
                  exposure.sky))
    {
        solis::ResultManager::RemoveRun(cloud, runId);
        return cmd.error(error);
    }

    if (solis::ExposureCalculator::HasMetric(options, solis::ExposureSun)
        && !store(QStringLiteral("sun"),
                  solis::ExposureCalculator::SunFieldBaseName(options),
                  exposure.sun))
    {
        solis::ResultManager::RemoveRun(cloud, runId);
        return cmd.error(error);
    }

    if (solis::ExposureCalculator::HasMetric(options, solis::ExposureRadiative)
        && !store(QStringLiteral("radiative"),
                  solis::ExposureCalculator::RadiativeFieldBaseName(options),
                  exposure.radiative))
    {
        solis::ResultManager::RemoveRun(cloud, runId);
        return cmd.error(error);
    }

    if (options.crownSummary)
    {
        solis::CrownExposureSummary summary;
        if (!solis::CrownExposure::Summarize(cloud, mesh, exposure, options, summary, &error))
        {
            solis::ResultManager::RemoveRun(cloud, runId);
            return cmd.error(QObject::tr("SOLIS Exposure crown summary: %1").arg(error));
        }

        solis::ResultManager::StoreCrownExposureMetadata(cloud,
                                                         summary,
                                                         options,
                                                         contextInfo,
                                                         runId,
                                                         entityName,
                                                         resultIds);

        QString weightingDetail;
        if (summary.weightingMethod == QLatin1String("occupied-voxel"))
            weightingDetail = QObject::tr(", voxel=%1 m").arg(summary.voxelSize, 0, 'g', 6);
        else
            weightingDetail = QObject::tr(", area=%1").arg(summary.representedSurfaceArea, 0, 'g', 8);

        cmd.print(
            QObject::tr("SOLIS Crown Exposure '%1': CEI=%2, SkyMean=%3, SunMean=%4, "
                        "REI_P10=%5, REI_P50=%6, REI_P90=%7, REI>=%8=%9%, weighting=%10%11")
                .arg(entityName)
                .arg(summary.cei, 0, 'f', 4)
                .arg(optionalFraction(summary.skyMean))
                .arg(optionalFraction(summary.sunMean))
                .arg(summary.reiP10, 0, 'f', 4)
                .arg(summary.reiP50, 0, 'f', 4)
                .arg(summary.reiP90, 0, 'f', 4)
                .arg(summary.threshold, 0, 'f', 2)
                .arg(summary.exposedFraction * 100.0, 0, 'f', 1)
                .arg(summary.weightingMethod)
                .arg(weightingDetail));
    }

    return true;
}

} // namespace

SOLISExposureCommand::SOLISExposureCommand()
    : ccCommandLineInterface::Command("SOLIS canopy exposure", COMMAND_SOLIS_EXPOSURE)
{
}

bool SOLISExposureCommand::process(ccCommandLineInterface& cmd)
{
    cmd.print(QObject::tr("[SOLIS Exposure v%1]").arg(QString::fromLatin1(solis::Version)));

    solis::ExposureOptions options;
    options.metrics = solis::ExposureAll;
    options.occlusionContext = solis::ExposureOcclusionContext::SelectedEntities;
    options.crownSummary = false;
    options.crownVoxelAuto = true;
    options.crownThreshold = 0.75;

    auto& calculation = options.calculation;
    calculation.site.latitudeDeg = 45.0;
    calculation.site.longitudeDeg = 10.0;
    calculation.site.elevationM = 0.0;
    calculation.site.linkeTurbidity = 3.0;
    calculation.site.timeMode = solis::TimeMode::UTC;
    calculation.site.utcOffsetHours = 0.0;

    calculation.time.dayOfYear = 172.0;
    calculation.time.mode = solis::TemporalMode::IntegratedEnergy;
    calculation.time.durationHours = 24.0;
    calculation.time.timestepMinutes = 10.0;
    calculation.time.energyUnit = solis::EnergyUnit::WhPerM2;

    calculation.spatialMode = solis::SpatialMode::HorizontalIrradiance;
    calculation.render.resolution = 1024;
    calculation.render.depthBias = 1.0e-3;
    calculation.render.cpuThreadCount = 0;
    calculation.render.parallelCpuProjection = true;

    calculation.sampling.diffuseRayCount = 256;
    calculation.sampling.acceleratedDirectSampling = false;
    calculation.sampling.directAngularToleranceDeg = 0.1;

    bool explicitClock = false;
    double clockHours = 0.0;

    while (!cmd.arguments().empty())
    {
        const QString arg = cmd.arguments().front();

        if (ccCommandLineInterface::IsCommand(arg, "METRICS"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "METRICS", value))
                return false;

            value = value.toUpper();
            if (value == QLatin1String("SKY"))
                options.metrics = solis::ExposureSky;
            else if (value == QLatin1String("SUN"))
                options.metrics = solis::ExposureSun;
            else if (value == QLatin1String("RADIATIVE"))
                options.metrics = solis::ExposureRadiative;
            else if (value == QLatin1String("ALL"))
                options.metrics = solis::ExposureAll;
            else
            {
                return cmd.error(QObject::tr("SOLIS Exposure: invalid -METRICS '%1' "
                                             "(SKY|SUN|RADIATIVE|ALL)")
                                     .arg(value));
            }
        }
        else if (ccCommandLineInterface::IsCommand(arg, "CONTEXT"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "CONTEXT", value))
                return false;
            value = value.toUpper();
            if (value == QLatin1String("TARGET"))
                options.occlusionContext = solis::ExposureOcclusionContext::TargetOnly;
            else if (value == QLatin1String("LOADED") || value == QLatin1String("SELECTED"))
                options.occlusionContext = solis::ExposureOcclusionContext::SelectedEntities;
            else
                return cmd.error(QObject::tr("SOLIS Exposure: -CONTEXT must be TARGET or LOADED"));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DOY"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DOY", calculation.time.dayOfYear))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIME"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "TIME", value))
                return false;
            if (!parseClock(value, clockHours))
                return cmd.error(QObject::tr("SOLIS Exposure: invalid -TIME '%1'").arg(value));
            explicitClock = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DURATION"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DURATION", calculation.time.durationHours))
                return false;
            calculation.time.mode = calculation.time.durationHours <= 0.0
                                      ? solis::TemporalMode::Instantaneous
                                      : solis::TemporalMode::IntegratedEnergy;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIMESTEP"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "TIMESTEP", calculation.time.timestepMinutes))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LAT"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LAT", calculation.site.latitudeDeg))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LON"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LON", calculation.site.longitudeDeg))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "ELV"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "ELV", calculation.site.elevationM))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "TIME_MODE"))
        {
            cmd.arguments().pop_front();
            QString value;
            if (!takeString(cmd, "TIME_MODE", value))
                return false;

            value = value.toUpper();
            if (value == QLatin1String("UTC"))
                calculation.site.timeMode = solis::TimeMode::UTC;
            else if (value == QLatin1String("LOCAL"))
                calculation.site.timeMode = solis::TimeMode::LocalFixedOffset;
            else
                return cmd.error(QObject::tr("SOLIS Exposure: -TIME_MODE must be UTC or LOCAL"));
        }
        else if (ccCommandLineInterface::IsCommand(arg, "UTC_OFFSET"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "UTC_OFFSET", calculation.site.utcOffsetHours))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "LINKE_TURBIDITY"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "LINKE_TURBIDITY", calculation.site.linkeTurbidity))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "SKY_RAYS"))
        {
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, "SKY_RAYS", calculation.sampling.diffuseRayCount))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "RESOLUTION"))
        {
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, "RESOLUTION", calculation.render.resolution))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "DEPTH_BIAS"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "DEPTH_BIAS", calculation.render.depthBias))
                return false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "THREADS"))
        {
            cmd.arguments().pop_front();
            if (!takeUnsigned(cmd, "THREADS", calculation.render.cpuThreadCount))
                return false;
            calculation.render.parallelCpuProjection = calculation.render.cpuThreadCount != 1;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "IS_CLOSED"))
        {
            cmd.arguments().pop_front();
            calculation.render.meshIsClosed = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "ACCEL_ANGLE"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "ACCEL_ANGLE", calculation.sampling.directAngularToleranceDeg))
                return false;
            calculation.sampling.acceleratedDirectSampling = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "NO_ACCEL"))
        {
            cmd.arguments().pop_front();
            calculation.sampling.acceleratedDirectSampling = false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "CROWN_SUMMARY"))
        {
            cmd.arguments().pop_front();
            options.crownSummary = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "CROWN_VOXEL_AUTO"))
        {
            cmd.arguments().pop_front();
            options.crownVoxelAuto = true;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "CROWN_VOXEL_SIZE"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "CROWN_VOXEL_SIZE", options.crownVoxelSize))
                return false;
            options.crownVoxelAuto = false;
        }
        else if (ccCommandLineInterface::IsCommand(arg, "CROWN_THRESHOLD"))
        {
            cmd.arguments().pop_front();
            if (!takeDouble(cmd, "CROWN_THRESHOLD", options.crownThreshold))
                return false;
        }
        else
        {
            break;
        }
    }

    if (explicitClock)
        calculation.time.dayOfYear = std::floor(calculation.time.dayOfYear) + clockHours / 24.0;

    if (calculation.render.resolution < 64 || calculation.render.resolution > 16384)
        return cmd.error(QObject::tr("SOLIS Exposure: -RESOLUTION must be in [64,16384]"));
    if (calculation.sampling.diffuseRayCount < 1 || calculation.sampling.diffuseRayCount > 65536)
        return cmd.error(QObject::tr("SOLIS Exposure: -SKY_RAYS must be in [1,65536]"));
    if (calculation.render.cpuThreadCount > 1024)
        return cmd.error(QObject::tr("SOLIS Exposure: -THREADS must be in [0,1024]"));
    if (options.crownSummary
        && !solis::ExposureCalculator::HasMetric(options, solis::ExposureRadiative))
    {
        return cmd.error(QObject::tr("SOLIS Exposure: -CROWN_SUMMARY requires RADIATIVE or ALL metrics"));
    }

    QString validationError;
    if (!solis::ExposureCalculator::ValidateOptions(options, &validationError))
        return cmd.error(QObject::tr("SOLIS Exposure: invalid options: %1").arg(validationError));

    if (cmd.clouds().empty() && cmd.meshes().empty())
        return cmd.error(QObject::tr("SOLIS Exposure: no loaded point cloud or mesh to process"));

    cmd.print(QObject::tr("SOLIS Exposure settings: metrics=%1, context=%2, DOY=%3, duration=%4 h, "
                          "dt=%5 min, rays=%6, resolution=%7")
                  .arg(static_cast<unsigned>(options.metrics))
                  .arg(options.occlusionContext == solis::ExposureOcclusionContext::TargetOnly
                           ? QStringLiteral("TARGET")
                           : QStringLiteral("LOADED"))
                  .arg(calculation.time.dayOfYear, 0, 'g', 10)
                  .arg(calculation.time.mode == solis::TemporalMode::Instantaneous
                           ? 0.0
                           : calculation.time.durationHours,
                       0,
                       'g',
                       8)
                  .arg(calculation.time.timestepMinutes, 0, 'g', 8)
                  .arg(calculation.sampling.diffuseRayCount)
                  .arg(calculation.render.resolution));

    std::vector<CommandExposureEntity> loadedEntities;
    QSet<ccPointCloud*> meshClouds;
    for (CLMeshDesc& desc : cmd.meshes())
    {
        ccGenericMesh* mesh = desc.mesh;
        ccPointCloud* cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        if (cloud && mesh)
        {
            loadedEntities.push_back({cloud, mesh, mesh->getName()});
            meshClouds.insert(cloud);
        }
    }
    for (CLCloudDesc& desc : cmd.clouds())
    {
        if (desc.pc && !meshClouds.contains(desc.pc))
            loadedEntities.push_back({desc.pc, nullptr, desc.pc->getName()});
    }

    for (CLCloudDesc& desc : cmd.clouds())
    {
        if (desc.pc && meshClouds.contains(desc.pc))
            continue;
        CommandExposureEntity target{desc.pc, nullptr, desc.pc ? desc.pc->getName() : QStringLiteral("cloud")};
        std::vector<SOLISOccluder> occluders;
        solis::ExposureContextInfo contextInfo;
        commandContextForTarget(target, loadedEntities, options.occlusionContext, occluders, contextInfo);
        if (!processEntity(cmd, target.cloud, target.mesh, target.name, options, occluders, contextInfo))
            return false;
    }

    for (CLMeshDesc& desc : cmd.meshes())
    {
        ccGenericMesh* mesh = desc.mesh;
        ccPointCloud* cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        CommandExposureEntity target{cloud, mesh, mesh ? mesh->getName() : QStringLiteral("mesh")};
        std::vector<SOLISOccluder> occluders;
        solis::ExposureContextInfo contextInfo;
        commandContextForTarget(target, loadedEntities, options.occlusionContext, occluders, contextInfo);
        if (!processEntity(cmd, target.cloud, target.mesh, target.name, options, occluders, contextInfo))
            return false;
    }

    if (cmd.autoSaveMode())
    {
        for (CLCloudDesc& desc : cmd.clouds())
        {
            const QString error = cmd.exportEntity(desc, QStringLiteral("SOLIS_EXPOSURE"));
            if (!error.isEmpty())
                return cmd.error(error);
        }
        for (CLMeshDesc& desc : cmd.meshes())
        {
            const QString error = cmd.exportEntity(desc, QStringLiteral("SOLIS_EXPOSURE"));
            if (!error.isEmpty())
                return cmd.error(error);
        }
    }

    return true;
}
