#include "qSOLIS.h"
#include "ccSolisDlg.h"
#include "SOLIS.h"
#include "SOLISBatch.h"
#include "SOLISCommand.h"
#include "SOLISCleanupCommand.h"
#include "SOLISCleanupDlg.h"
#include "SOLISBackend.h"
#include "SOLISResults.h"
#include "SOLISVersion.h"

#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

#include <QAction>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <vector>

namespace
{
solis::CalculationOptions defaultGuiOptions()
{
    solis::CalculationOptions options;
    options.site.latitudeDeg = 45.0;
    options.site.longitudeDeg = 10.0;
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
    return options;
}

solis::BatchOptions defaultGuiBatchOptions()
{
    solis::BatchOptions options;
    options.enabled = false;
    options.endDayOfYear = 172.75; // DOY 172, 18:00
    options.stepMinutes = 60.0;
    return options;
}

solis::CalculationOptions s_options = defaultGuiOptions();
solis::BatchOptions s_batchOptions = defaultGuiBatchOptions();
bool s_calculateDirect = true;
bool s_calculateDiffuse = false;
bool s_advancedVisible = false;
int s_qualityPreset = 1; // Normal

constexpr unsigned BatchConfirmationThreshold = 100;

ccPointCloud* pointCloudForObject(ccHObject* obj, ccGenericMesh** meshOut = nullptr)
{
    if (meshOut)
        *meshOut = nullptr;
    if (!obj)
        return nullptr;

    if (obj->isA(CC_TYPES::POINT_CLOUD))
        return ccHObjectCaster::ToPointCloud(obj);

    if (obj->isKindOf(CC_TYPES::MESH))
    {
        ccGenericMesh* mesh = ccHObjectCaster::ToGenericMesh(obj);
        if (meshOut)
            *meshOut = mesh;
        return mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
    }

    return nullptr;
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

} // namespace

qSOLIS::qSOLIS(QObject* parent)
    : QObject(parent)
    , ccStdPluginInterface(":/CC/plugin/qSolis/info.json")
{
}

void qSOLIS::onNewSelection(const ccHObject::Container& selectedEntities)
{
    bool eligible = false;
    for (ccHObject* obj : selectedEntities)
    {
        if (obj && (obj->isKindOf(CC_TYPES::POINT_CLOUD) || obj->isKindOf(CC_TYPES::MESH)))
        {
            eligible = true;
            break;
        }
    }
    if (m_action)
        m_action->setEnabled(eligible);
    if (m_cleanupAction)
        m_cleanupAction->setEnabled(eligible);
}

QList<QAction*> qSOLIS::getActions()
{
    if (!m_action)
    {
        m_action = new QAction(tr("SOLIS Calculate..."), this);
        m_action->setToolTip(getDescription());
        m_action->setIcon(getIcon());
        connect(m_action, &QAction::triggered, this, &qSOLIS::doAction);
    }
    if (!m_cleanupAction)
    {
        m_cleanupAction = new QAction(tr("SOLIS Cleanup..."), this);
        m_cleanupAction->setToolTip(tr("Remove qSOLIS result scalar fields from the selected cloud or mesh"));
        m_cleanupAction->setIcon(getIcon());
        connect(m_cleanupAction, &QAction::triggered, this, &qSOLIS::doCleanupAction);
    }
    return {m_action, m_cleanupAction};
}

void qSOLIS::doAction()
{
    if (!m_app)
        return;

    ccHObject::Container candidates;
    bool hasMeshes = false;
    bool allHaveNormals = true;

    for (ccHObject* obj : m_app->getSelectedEntities())
    {
        if (!obj)
            continue;

        ccGenericMesh* mesh = nullptr;
        ccPointCloud* cloud = pointCloudForObject(obj, &mesh);
        if (!cloud)
            continue;

        candidates.push_back(obj);
        hasMeshes = hasMeshes || (mesh != nullptr);
        allHaveNormals = allHaveNormals && cloud->hasNormals();
    }

    if (candidates.empty())
        return;

    ccSolisDlg dlg(m_app->getMainWindow());
    dlg.setCalculationOptions(s_options);
    dlg.setBatchOptions(s_batchOptions);
    dlg.setComponents(s_calculateDirect, s_calculateDiffuse);
    dlg.setHasMeshes(hasMeshes);
    dlg.setSurfaceModeAvailable(
        allHaveNormals,
        tr("Surface irradiance is unavailable: every selected cloud/mesh vertex cloud must already have normals."));
    dlg.setQualityPresetIndex(s_qualityPreset);
    dlg.setAdvancedVisible(s_advancedVisible);

    if (!dlg.exec())
        return;

    const bool calculateDirect = dlg.calculateDirect();
    const bool calculateDiffuse = dlg.calculateDiffuse();
    if (!calculateDirect && !calculateDiffuse)
    {
        QMessageBox::warning(m_app->getMainWindow(), tr("SOLIS"), tr("Select Direct and/or Diffuse."));
        return;
    }

    solis::CalculationOptions baseOptions = dlg.calculationOptions();
    baseOptions.render.meshIsClosed = hasMeshes && baseOptions.render.meshIsClosed;
    solis::BatchOptions batchOptions = dlg.batchOptions();

    // Keep the dialog state for the next invocation in this CloudCompare process.
    s_options = baseOptions;
    s_batchOptions = batchOptions;
    s_calculateDirect = calculateDirect;
    s_calculateDiffuse = calculateDiffuse;
    s_advancedVisible = dlg.advancedVisible();
    s_qualityPreset = dlg.qualityPresetIndex();

    if (baseOptions.spatialMode == solis::SpatialMode::SurfaceIrradiance && !allHaveNormals)
    {
        QMessageBox::warning(m_app->getMainWindow(),
                             tr("SOLIS"),
                             tr("Surface irradiance requires existing normals on every selected point/vertex cloud."));
        return;
    }

    QString validationError;
    if (!SOLIS::ValidateOptions(baseOptions, &validationError))
    {
        QMessageBox::warning(m_app->getMainWindow(), tr("SOLIS"), tr("Invalid calculation options: %1").arg(validationError));
        return;
    }

    std::vector<double> batchTimes;
    QString batchError;
    if (!solis::BuildBatchTimes(baseOptions, batchOptions, batchTimes, &batchError))
    {
        QMessageBox::warning(m_app->getMainWindow(), tr("SOLIS batch"), batchError);
        return;
    }

    if (batchTimes.size() > BatchConfirmationThreshold)
    {
        unsigned fieldsPerTime = 0;
        if (calculateDirect)
            ++fieldsPerTime;
        if (calculateDiffuse)
            ++fieldsPerTime;
        if (calculateDirect && calculateDiffuse && baseOptions.spatialMode != solis::SpatialMode::Visibility)
            ++fieldsPerTime; // Total

        const auto reply = QMessageBox::question(
            m_app->getMainWindow(),
            tr("SOLIS batch"),
            tr("This batch will create %1 time points and up to %2 scalar fields per selected entity. Continue?")
                .arg(static_cast<qulonglong>(batchTimes.size()))
                .arg(static_cast<qulonglong>(batchTimes.size() * fieldsPerTime)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    std::vector<CCVector3d> diffuseRays;
    if (calculateDiffuse && !SOLIS::GenerateDiffuseRays(baseOptions.sampling.diffuseRayCount, diffuseRays))
    {
        m_app->dispToConsole("SOLIS: failed to generate diffuse rays", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
        return;
    }

    const bool isBatch = batchTimes.size() > 1;
    const QString batchId = isBatch ? solis::ResultManager::NewRunId() : QString();

    for (ccHObject* obj : candidates)
    {
        ccGenericMesh* mesh = nullptr;
        ccPointCloud* cloud = pointCloudForObject(obj, &mesh);
        if (!cloud)
            continue;

        std::vector<CCVector3d> normals;
        const std::vector<CCVector3d>* normalsPtr = nullptr;
        if (baseOptions.spatialMode == solis::SpatialMode::SurfaceIrradiance)
        {
            if (!extractNormals(cloud, normals))
            {
                m_app->dispToConsole(
                    QStringLiteral("SOLIS: surface irradiance requires valid normals on '%1'").arg(obj->getName()),
                    ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                continue;
            }
            normalsPtr = &normals;
        }

        solis::CalculationOptions entityBaseOptions = baseOptions;
        entityBaseOptions.render.meshIsClosed = (mesh != nullptr) && baseOptions.render.meshIsClosed;

        std::unique_ptr<SOLISAccumulationBackend> backend = CreateSOLISAccumulationBackend();
        if (!backend || !backend->init(entityBaseOptions.render.resolution,
                                       entityBaseOptions.render.resolution,
                                       cloud,
                                       mesh,
                                       entityBaseOptions.render.meshIsClosed,
                                       entityBaseOptions.render.depthBias,
                                       entityBaseOptions.render.parallelCpuProjection,
                                       entityBaseOptions.render.cpuThreadCount))
        {
            m_app->dispToConsole(QStringLiteral("SOLIS: renderer initialization failed for '%1'").arg(obj->getName()),
                                 ccMainAppInterface::ERR_CONSOLE_MESSAGE);
            continue;
        }

        solis::BatchContext batchContext;
        if (isBatch)
        {
            batchContext.batchId = batchId;
            batchContext.count = static_cast<unsigned>(batchTimes.size());
            batchContext.startDayOfYear = batchTimes.front();
            batchContext.endDayOfYear = batchOptions.endDayOfYear;
            batchContext.stepMinutes = batchOptions.stepMinutes;
            solis::ResultManager::StoreBatchMetadata(cloud, batchContext, obj->getName());
        }

        std::unique_ptr<ccProgressDialog> progress = std::make_unique<ccProgressDialog>(true, m_app->getMainWindow());
        progress->setAutoClose(false);

        for (size_t batchIndex = 0; batchIndex < batchTimes.size(); ++batchIndex)
        {
            solis::CalculationOptions options = entityBaseOptions;
            options.time.dayOfYear = batchTimes[batchIndex];

            std::vector<solis::RaySample> directSamples;
            if (calculateDirect && !SOLIS::GenerateDirectSamples(options, directSamples))
            {
                m_app->dispToConsole(QStringLiteral("SOLIS: failed to generate direct samples for batch time %1/%2")
                                         .arg(static_cast<qulonglong>(batchIndex + 1))
                                         .arg(static_cast<qulonglong>(batchTimes.size())),
                                     ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                return;
            }

            const double diffuseIrradiance = calculateDiffuse
                                           ? SOLIS::AggregateDiffuseIrradiance(options)
                                           : 0.0;
            if (calculateDiffuse && !std::isfinite(diffuseIrradiance))
            {
                m_app->dispToConsole("SOLIS: failed to calculate diffuse irradiance", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                return;
            }

            const QString runId = solis::ResultManager::NewRunId();
            if (isBatch)
                batchContext.index = static_cast<unsigned>(batchIndex);

            const solis::BatchContext* batchPtr = isBatch ? &batchContext : nullptr;
            solis::ResultField directResult;
            solis::ResultField diffuseResult;
            solis::ResultField totalResult;

            if (calculateDirect)
            {
                if (!solis::ResultManager::CreateResultField(cloud, solis::ResultComponent::Direct, options, directResult))
                {
                    m_app->dispToConsole(QStringLiteral("SOLIS: failed to allocate Direct scalar field for '%1'").arg(obj->getName()),
                                         ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                    return;
                }

                if (!SOLIS::LaunchDirect(directSamples, options, cloud, mesh, normalsPtr, progress.get(), obj->getName(), backend.get()))
                {
                    cloud->deleteScalarField(directResult.index);
                    if (progress->wasCanceled())
                        m_app->dispToConsole("SOLIS: batch canceled by user", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
                    else
                        m_app->dispToConsole("SOLIS: Direct calculation failed", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                    return;
                }

                solis::ResultManager::FinalizeField(cloud, directResult.index);
                solis::ResultManager::StoreMetadata(cloud, directResult, solis::ResultComponent::Direct,
                                                    options, runId, obj->getName(), batchPtr);
                m_app->dispToConsole(QStringLiteral("SOLIS: created scalar field '%1'").arg(directResult.name),
                                     ccMainAppInterface::STD_CONSOLE_MESSAGE);
            }

            if (calculateDiffuse)
            {
                if (!solis::ResultManager::CreateResultField(cloud, solis::ResultComponent::Diffuse, options, diffuseResult))
                {
                    m_app->dispToConsole(QStringLiteral("SOLIS: failed to allocate Diffuse scalar field for '%1'").arg(obj->getName()),
                                         ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                    return;
                }

                if (!SOLIS::LaunchDiffuse(diffuseRays, diffuseIrradiance, options, cloud, mesh, normalsPtr,
                                          progress.get(), obj->getName(), backend.get()))
                {
                    cloud->deleteScalarField(diffuseResult.index);
                    if (progress->wasCanceled())
                        m_app->dispToConsole("SOLIS: batch canceled by user", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
                    else
                        m_app->dispToConsole("SOLIS: Diffuse calculation failed", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                    return;
                }

                solis::ResultManager::FinalizeField(cloud, diffuseResult.index);
                solis::ResultManager::StoreMetadata(cloud, diffuseResult, solis::ResultComponent::Diffuse,
                                                    options, runId, obj->getName(), batchPtr);
                m_app->dispToConsole(QStringLiteral("SOLIS: created scalar field '%1'").arg(diffuseResult.name),
                                     ccMainAppInterface::STD_CONSOLE_MESSAGE);
            }

            // Total irradiance/energy is meaningful only for irradiance modes.
            if (calculateDirect && calculateDiffuse && options.spatialMode != solis::SpatialMode::Visibility)
            {
                if (!solis::ResultManager::CreateTotalField(cloud, directResult.index, diffuseResult.index, options, totalResult))
                {
                    m_app->dispToConsole(QStringLiteral("SOLIS: failed to create Total scalar field for '%1'").arg(obj->getName()),
                                         ccMainAppInterface::WRN_CONSOLE_MESSAGE);
                }
                else
                {
                    solis::ResultManager::StoreMetadata(cloud, totalResult, solis::ResultComponent::Total,
                                                        options, runId, obj->getName(), batchPtr);
                    m_app->dispToConsole(QStringLiteral("SOLIS: created scalar field '%1'").arg(totalResult.name),
                                         ccMainAppInterface::STD_CONSOLE_MESSAGE);
                }
            }
        }

        obj->showSF(true);
        obj->prepareDisplayForRefresh_recursive();
    }

    m_app->updateUI();
    m_app->refreshAll();
}

void qSOLIS::doCleanupAction()
{
    if (!m_app)
        return;

    struct Target
    {
        ccHObject* entity = nullptr;
        ccPointCloud* cloud = nullptr;
    };

    std::vector<Target> targets;
    QSet<ccPointCloud*> seenClouds;
    for (ccHObject* obj : m_app->getSelectedEntities())
    {
        if (!obj)
            continue;

        ccPointCloud* cloud = nullptr;
        if (obj->isA(CC_TYPES::POINT_CLOUD))
        {
            cloud = ccHObjectCaster::ToPointCloud(obj);
        }
        else if (obj->isKindOf(CC_TYPES::MESH))
        {
            ccGenericMesh* mesh = ccHObjectCaster::ToGenericMesh(obj);
            cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        }

        if (cloud && !seenClouds.contains(cloud))
        {
            seenClouds.insert(cloud);
            targets.push_back({obj, cloud});
        }
    }

    if (targets.empty())
        return;

    unsigned managedCount = 0;
    unsigned legacyCount = 0;
    unsigned lastRunCount = 0;
    unsigned lastBatchCount = 0;

    struct BatchAggregate
    {
        solis::CleanupBatchChoice choice;
        QString createdUtc;
    };
    QMap<QString, BatchAggregate> batchMap;

    for (const Target& target : targets)
    {
        const std::vector<solis::ResultRecord> results = solis::ResultManager::Results(target.cloud);
        managedCount += static_cast<unsigned>(results.size());
        legacyCount += solis::ResultManager::LegacyFieldCount(target.cloud);

        const QString lastRunId = solis::ResultManager::LastRunId(target.cloud);
        const QString lastBatchId = solis::ResultManager::LastBatchId(target.cloud);
        for (const solis::ResultRecord& result : results)
        {
            if (!lastRunId.isEmpty() && result.runId == lastRunId)
                ++lastRunCount;
            if (!lastBatchId.isEmpty() && result.batchId == lastBatchId)
                ++lastBatchCount;
        }

        for (const solis::BatchRecord& batch : solis::ResultManager::Batches(target.cloud))
        {
            if (batch.batchId.isEmpty() || batch.storedResultCount == 0)
                continue;
            BatchAggregate& aggregate = batchMap[batch.batchId];
            aggregate.choice.batchId = batch.batchId;
            aggregate.choice.resultCount += batch.storedResultCount;
            if (aggregate.createdUtc.isEmpty() || batch.createdUtc > aggregate.createdUtc)
                aggregate.createdUtc = batch.createdUtc;
        }
    }

    std::vector<solis::CleanupBatchChoice> batchChoices;
    batchChoices.reserve(static_cast<size_t>(batchMap.size()));
    for (auto it = batchMap.constBegin(); it != batchMap.constEnd(); ++it)
    {
        solis::CleanupBatchChoice choice = it.value().choice;
        const QString shortId = choice.batchId.left(8);
        const QString created = it.value().createdUtc.isEmpty() ? tr("unknown time") : it.value().createdUtc;
        choice.label = tr("%1 — %2 — %3 fields").arg(created).arg(shortId).arg(choice.resultCount);
        batchChoices.push_back(choice);
    }
    std::sort(batchChoices.begin(), batchChoices.end(), [](const auto& a, const auto& b)
    {
        return a.label > b.label;
    });

    solis::CleanupDialog dlg(static_cast<unsigned>(targets.size()),
                             managedCount,
                             legacyCount,
                             lastRunCount,
                             lastBatchCount,
                             batchChoices,
                             m_app->getMainWindow());
    if (!dlg.exec())
        return;

    solis::CleanupStats total;
    for (const Target& target : targets)
    {
        solis::CleanupStats stats;
        switch (dlg.choice())
        {
        case solis::CleanupChoice::LastRun:
            stats = solis::ResultManager::RemoveRun(target.cloud, solis::ResultManager::LastRunId(target.cloud));
            break;
        case solis::CleanupChoice::LastBatch:
            stats = solis::ResultManager::RemoveBatch(target.cloud, solis::ResultManager::LastBatchId(target.cloud));
            break;
        case solis::CleanupChoice::SelectedBatch:
            stats = solis::ResultManager::RemoveBatch(target.cloud, dlg.selectedBatchId());
            break;
        case solis::CleanupChoice::AllManaged:
            stats = solis::ResultManager::RemoveAll(target.cloud);
            break;
        case solis::CleanupChoice::LegacyNamed:
            stats = solis::ResultManager::RemoveLegacyNamedFields(target.cloud);
            break;
        }

        total.fieldsRemoved += stats.fieldsRemoved;
        total.metadataRecordsRemoved += stats.metadataRecordsRemoved;
        total.missingFields += stats.missingFields;
        total.legacyFieldsRemoved += stats.legacyFieldsRemoved;

        if (target.entity)
        {
            target.entity->showSF(target.cloud->hasScalarFields());
            target.entity->prepareDisplayForRefresh_recursive();
        }
    }

    m_app->dispToConsole(
        tr("SOLIS cleanup: removed %1 scalar field(s) and %2 metadata record(s); %3 stale metadata field reference(s)")
            .arg(total.fieldsRemoved)
            .arg(total.metadataRecordsRemoved)
            .arg(total.missingFields),
        ccMainAppInterface::STD_CONSOLE_MESSAGE);

    m_app->updatePropertiesView();
    m_app->updateUI();
    m_app->refreshAll();
}

void qSOLIS::registerCommands(ccCommandLineInterface* cmd)
{
    if (!cmd)
        return;
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCommand));
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCleanupCommand));
}
