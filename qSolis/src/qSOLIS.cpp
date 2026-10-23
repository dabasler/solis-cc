#include "qSOLIS.h"
#include "ccSolisDlg.h"
#include "ccSolisExposureDlg.h"
#include "SOLIS.h"
#include "SOLISBatch.h"
#include "SOLISCommand.h"
#include "SOLISCleanupCommand.h"
#include "SOLISExposureCommand.h"
#include "SOLISExposure.h"
#include "SOLISCrownExposure.h"
#include "SOLISCrownExport.h"
#include "SOLISCrownExportCommand.h"
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
#include <QDir>
#include <QFileDialog>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <utility>
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

solis::ExposureOptions defaultExposureOptions()
{
    solis::ExposureOptions options;
    options.metrics = solis::ExposureAll;
    options.crownSummary = true;
    options.crownVoxelAuto = true;
    options.crownVoxelSize = 0.2;
    options.crownThreshold = 0.75;
    options.calculation = defaultGuiOptions();
    options.calculation.time.dayOfYear = 172.0;
    options.calculation.time.mode = solis::TemporalMode::IntegratedEnergy;
    options.calculation.time.durationHours = 24.0;
    options.calculation.time.timestepMinutes = 10.0;
    options.calculation.time.energyUnit = solis::EnergyUnit::WhPerM2;
    options.calculation.spatialMode = solis::SpatialMode::HorizontalIrradiance;
    return options;
}

solis::ExposureOptions s_exposureOptions = defaultExposureOptions();
bool s_exposureAdvancedVisible = false;
int s_exposureQualityPreset = 1;

bool writeExposureField(ccPointCloud* cloud,
                        const QString& baseName,
                        const std::vector<double>& values,
                        solis::ResultField& field)
{
    if (!cloud || values.size() != cloud->size()
        || !solis::ResultManager::CreateNamedResultField(cloud, baseName, field))
        return false;
    auto sf = cloud->getScalarField(field.index);
    if (!sf)
    {
        cloud->deleteScalarField(field.index);
        field = solis::ResultField{};
        return false;
    }
    for (unsigned i = 0; i < cloud->size(); ++i)
        sf->setValue(i, static_cast<ScalarType>(values[i]));
    solis::ResultManager::FinalizeField(cloud, field.index);
    return true;
}

QString optionalFraction(double value)
{
    return std::isfinite(value) ? QString::number(value, 'f', 4) : QStringLiteral("n/a");
}

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

struct ExposureEntity
{
    ccHObject* entity = nullptr;
    ccPointCloud* cloud = nullptr;
    ccGenericMesh* mesh = nullptr;
};

std::vector<ExposureEntity> selectedExposureTargets(const ccHObject::Container& selection)
{
    std::vector<ExposureEntity> targets;
    QSet<ccPointCloud*> seenClouds;

    // Prefer a selected mesh over its associated vertex cloud when both are selected.
    for (ccHObject* obj : selection)
    {
        if (!obj || !obj->isKindOf(CC_TYPES::MESH))
            continue;
        ccGenericMesh* mesh = nullptr;
        ccPointCloud* cloud = pointCloudForObject(obj, &mesh);
        if (cloud && mesh && !seenClouds.contains(cloud))
        {
            seenClouds.insert(cloud);
            targets.push_back({obj, cloud, mesh});
        }
    }
    for (ccHObject* obj : selection)
    {
        if (!obj || !obj->isA(CC_TYPES::POINT_CLOUD))
            continue;
        ccPointCloud* cloud = pointCloudForObject(obj);
        if (cloud && !seenClouds.contains(cloud))
        {
            seenClouds.insert(cloud);
            targets.push_back({obj, cloud, nullptr});
        }
    }
    return targets;
}

std::vector<ExposureEntity> visibleExposureGeometry(ccHObject* root)
{
    std::vector<ExposureEntity> geometry;
    if (!root)
        return geometry;

    QSet<ccPointCloud*> meshClouds;
    ccHObject::Container meshes;
    root->filterChildren(meshes, true, CC_TYPES::MESH);
    for (ccHObject* obj : meshes)
    {
        if (!obj || !obj->isDisplayed())
            continue;
        ccGenericMesh* mesh = nullptr;
        ccPointCloud* cloud = pointCloudForObject(obj, &mesh);
        if (cloud && mesh)
        {
            geometry.push_back({obj, cloud, mesh});
            meshClouds.insert(cloud);
        }
    }

    ccHObject::Container clouds;
    root->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD, true);
    for (ccHObject* obj : clouds)
    {
        if (!obj || !obj->isDisplayed())
            continue;
        ccPointCloud* cloud = pointCloudForObject(obj);
        if (cloud && !meshClouds.contains(cloud))
            geometry.push_back({obj, cloud, nullptr});
    }
    return geometry;
}

void appendUniqueOccluder(std::vector<SOLISOccluder>& occluders,
                          QStringList& names,
                          const ExposureEntity& source)
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
    const QString name = source.entity ? source.entity->getName() : QStringLiteral("unnamed");
    const unsigned uniqueId = source.entity ? source.entity->getUniqueID() : 0u;
    names.push_back(QStringLiteral("%1 [id=%2]").arg(name).arg(uniqueId));
}

void buildExposureContext(const ExposureEntity& target,
                          const std::vector<ExposureEntity>& selectedTargets,
                          const std::vector<ExposureEntity>& visibleGeometry,
                          solis::ExposureOcclusionContext mode,
                          std::vector<SOLISOccluder>& occluders,
                          solis::ExposureContextInfo& info)
{
    occluders.clear();
    info = solis::ExposureContextInfo{};
    info.mode = mode;

    if (mode == solis::ExposureOcclusionContext::TargetOnly)
    {
        appendUniqueOccluder(occluders, info.entityNames, target);
        return;
    }

    const auto& sources = mode == solis::ExposureOcclusionContext::AllVisibleEntities
                        ? visibleGeometry
                        : selectedTargets;
    for (const ExposureEntity& source : sources)
        appendUniqueOccluder(occluders, info.entityNames, source);

    // The selected target always participates in self-occlusion even if hidden or
    // otherwise absent from the selected/visible context set.
    appendUniqueOccluder(occluders, info.entityNames, target);
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
    bool hasCrownSummary = false;
    for (ccHObject* obj : selectedEntities)
    {
        if (!obj || !(obj->isKindOf(CC_TYPES::POINT_CLOUD) || obj->isKindOf(CC_TYPES::MESH)))
            continue;

        eligible = true;
        ccPointCloud* cloud = pointCloudForObject(obj);
        solis::CrownExposureExportRecord record;
        if (cloud && solis::CrownExposureExport::LatestRecord(cloud, record))
            hasCrownSummary = true;
    }
    if (m_action)
        m_action->setEnabled(eligible);
    if (m_exposureAction)
        m_exposureAction->setEnabled(eligible);
    if (m_exportCrownAction)
        m_exportCrownAction->setEnabled(hasCrownSummary);
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
    if (!m_exposureAction)
    {
        m_exposureAction = new QAction(tr("SOLIS Exposure..."), this);
        m_exposureAction->setToolTip(tr("Calculate canopy sky, sun, radiative, and crown exposure metrics"));
        m_exposureAction->setIcon(getIcon());
        connect(m_exposureAction, &QAction::triggered, this, &qSOLIS::doExposureAction);
    }
    if (!m_exportCrownAction)
    {
        m_exportCrownAction = new QAction(tr("SOLIS Export Crown Exposure Summary..."), this);
        m_exportCrownAction->setToolTip(tr("Export the latest stored Crown Exposure summary for each selected tree/entity to CSV"));
        m_exportCrownAction->setIcon(getIcon());
        connect(m_exportCrownAction, &QAction::triggered, this, &qSOLIS::doExportCrownAction);
    }
    if (!m_cleanupAction)
    {
        m_cleanupAction = new QAction(tr("SOLIS Cleanup..."), this);
        m_cleanupAction->setToolTip(tr("Remove qSOLIS result scalar fields from the selected cloud or mesh"));
        m_cleanupAction->setIcon(getIcon());
        connect(m_cleanupAction, &QAction::triggered, this, &qSOLIS::doCleanupAction);
    }
    if (m_app)
        onNewSelection(m_app->getSelectedEntities());
    return {m_action, m_exposureAction, m_exportCrownAction, m_cleanupAction};
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

void qSOLIS::doExposureAction()
{
    if (!m_app)
        return;

    const std::vector<ExposureEntity> targets = selectedExposureTargets(m_app->getSelectedEntities());
    if (targets.empty())
        return;

    bool hasMeshes = false;
    bool hasPointClouds = false;
    for (const ExposureEntity& target : targets)
    {
        hasMeshes = hasMeshes || (target.mesh != nullptr);
        hasPointClouds = hasPointClouds || (target.mesh == nullptr);
    }

    ccSolisExposureDlg dlg(m_app->getMainWindow());
    dlg.setOptions(s_exposureOptions);
    dlg.setSelectionKinds(hasPointClouds, hasMeshes);
    dlg.setQualityPresetIndex(s_exposureQualityPreset);
    dlg.setAdvancedVisible(s_exposureAdvancedVisible);
    if (!dlg.exec())
        return;

    solis::ExposureOptions options = dlg.options();
    s_exposureOptions = options;
    s_exposureQualityPreset = dlg.qualityPresetIndex();
    s_exposureAdvancedVisible = dlg.advancedVisible();

    QString validationError;
    if (!solis::ExposureCalculator::ValidateOptions(options, &validationError))
    {
        QMessageBox::warning(m_app->getMainWindow(), tr("SOLIS Exposure"), validationError);
        return;
    }

    const std::vector<ExposureEntity> visibleGeometry =
        options.occlusionContext == solis::ExposureOcclusionContext::AllVisibleEntities
            ? visibleExposureGeometry(m_app->dbRootObject())
            : std::vector<ExposureEntity>{};

    for (const ExposureEntity& target : targets)
    {
        ccHObject* obj = target.entity;
        ccPointCloud* cloud = target.cloud;
        ccGenericMesh* mesh = target.mesh;
        if (!obj || !cloud)
            continue;

        solis::ExposureOptions entityOptions = options;
        entityOptions.calculation.render.meshIsClosed = mesh && options.calculation.render.meshIsClosed;

        std::vector<SOLISOccluder> occluders;
        solis::ExposureContextInfo contextInfo;
        buildExposureContext(target,
                             targets,
                             visibleGeometry,
                             entityOptions.occlusionContext,
                             occluders,
                             contextInfo);
        if (occluders.empty())
        {
            m_app->dispToConsole(tr("SOLIS Exposure: no occluding geometry available for '%1'").arg(obj->getName()),
                                 ccMainAppInterface::ERR_CONSOLE_MESSAGE);
            continue;
        }

        std::unique_ptr<SOLISAccumulationBackend> backend = CreateSOLISAccumulationBackend();
        if (!backend
            || !backend->setOcclusionContext(occluders)
            || !backend->init(entityOptions.calculation.render.resolution,
                              entityOptions.calculation.render.resolution,
                              cloud,
                              mesh,
                              entityOptions.calculation.render.meshIsClosed,
                              entityOptions.calculation.render.depthBias,
                              entityOptions.calculation.render.parallelCpuProjection,
                              entityOptions.calculation.render.cpuThreadCount))
        {
            m_app->dispToConsole(tr("SOLIS Exposure: renderer initialization failed for '%1'").arg(obj->getName()),
                                 ccMainAppInterface::ERR_CONSOLE_MESSAGE);
            continue;
        }

        m_app->dispToConsole(
            tr("SOLIS Exposure '%1': context=%2, occluding entities=%3")
                .arg(obj->getName(),
                     solis::ExposureCalculator::OcclusionContextName(contextInfo.mode))
                .arg(contextInfo.entityNames.size()),
            ccMainAppInterface::STD_CONSOLE_MESSAGE);

        std::unique_ptr<ccProgressDialog> progress = std::make_unique<ccProgressDialog>(true, m_app->getMainWindow());
        solis::ExposureResult exposure;
        QString error;
        if (!solis::ExposureCalculator::Calculate(entityOptions, cloud, mesh, exposure,
                                                   progress.get(), obj->getName(), backend.get(), &error))
        {
            const auto level = progress->wasCanceled() ? ccMainAppInterface::WRN_CONSOLE_MESSAGE
                                                       : ccMainAppInterface::ERR_CONSOLE_MESSAGE;
            m_app->dispToConsole(tr("SOLIS Exposure '%1': %2").arg(obj->getName(), error), level);
            if (progress->wasCanceled())
                return;
            continue;
        }

        const QString runId = solis::ResultManager::NewRunId();
        QStringList resultIds;
        bool fieldFailure = false;
        auto store = [&](const QString& metric, const QString& baseName, const std::vector<double>& values)
        {
            if (fieldFailure)
                return;
            solis::ResultField field;
            if (!writeExposureField(cloud, baseName, values, field))
            {
                fieldFailure = true;
                return;
            }
            solis::ResultManager::StoreExposureMetadata(cloud, field, metric, entityOptions,
                                                        exposure.reference, contextInfo, runId, obj->getName());
            resultIds.push_back(field.resultId);
            m_app->dispToConsole(tr("SOLIS Exposure: created scalar field '%1'").arg(field.name),
                                 ccMainAppInterface::STD_CONSOLE_MESSAGE);
        };

        if (solis::ExposureCalculator::HasMetric(entityOptions, solis::ExposureSky))
            store(QStringLiteral("sky"), solis::ExposureCalculator::SkyFieldBaseName(entityOptions), exposure.sky);
        if (solis::ExposureCalculator::HasMetric(entityOptions, solis::ExposureSun))
            store(QStringLiteral("sun"), solis::ExposureCalculator::SunFieldBaseName(entityOptions), exposure.sun);
        if (solis::ExposureCalculator::HasMetric(entityOptions, solis::ExposureRadiative))
            store(QStringLiteral("radiative"), solis::ExposureCalculator::RadiativeFieldBaseName(entityOptions), exposure.radiative);

        if (fieldFailure)
        {
            solis::ResultManager::RemoveRun(cloud, runId);
            m_app->dispToConsole(tr("SOLIS Exposure: failed to create result fields for '%1'").arg(obj->getName()),
                                 ccMainAppInterface::ERR_CONSOLE_MESSAGE);
            continue;
        }

        if (entityOptions.crownSummary)
        {
            solis::CrownExposureSummary summary;
            if (!solis::CrownExposure::Summarize(cloud, mesh, exposure, entityOptions, summary, &error))
            {
                solis::ResultManager::RemoveRun(cloud, runId);
                m_app->dispToConsole(tr("SOLIS Crown Exposure '%1': %2").arg(obj->getName(), error),
                                     ccMainAppInterface::ERR_CONSOLE_MESSAGE);
                continue;
            }
            solis::ResultManager::StoreCrownExposureMetadata(cloud, summary, entityOptions, contextInfo, runId,
                                                             obj->getName(), resultIds);
            const QString weightingDetail = summary.weightingMethod == QLatin1String("occupied-voxel")
                                                ? tr(", voxel=%1 m").arg(summary.voxelSize, 0, 'g', 6)
                                                : tr(", area=%1").arg(summary.representedSurfaceArea, 0, 'g', 8);
            m_app->dispToConsole(
                tr("SOLIS Crown Exposure '%1': CEI=%2, SkyMean=%3, SunMean=%4, REI P10/P50/P90=%5/%6/%7, >=%8=%9%, %10%11")
                    .arg(obj->getName())
                    .arg(summary.cei, 0, 'f', 4)
                    .arg(optionalFraction(summary.skyMean))
                    .arg(optionalFraction(summary.sunMean))
                    .arg(summary.reiP10, 0, 'f', 4)
                    .arg(summary.reiP50, 0, 'f', 4)
                    .arg(summary.reiP90, 0, 'f', 4)
                    .arg(summary.threshold, 0, 'f', 2)
                    .arg(summary.exposedFraction * 100.0, 0, 'f', 1)
                    .arg(summary.weightingMethod)
                    .arg(weightingDetail),
                ccMainAppInterface::STD_CONSOLE_MESSAGE);
        }

        obj->showSF(true);
        obj->prepareDisplayForRefresh_recursive();
    }

    m_app->updatePropertiesView();
    m_app->updateUI();
    m_app->refreshAll();
    onNewSelection(m_app->getSelectedEntities());
}

void qSOLIS::doExportCrownAction()
{
    if (!m_app)
        return;

    const ccHObject::Container& selection = m_app->getSelectedEntities();
    QSet<ccPointCloud*> processed;
    std::vector<solis::CrownExposureExportRecord> records;
    QStringList skipped;

    for (ccHObject* obj : selection)
    {
        if (!obj)
            continue;
        ccPointCloud* cloud = pointCloudForObject(obj);
        if (!cloud || processed.contains(cloud))
            continue;
        processed.insert(cloud);

        solis::CrownExposureExportRecord record;
        if (solis::CrownExposureExport::LatestRecord(cloud, record))
            records.push_back(std::move(record));
        else
            skipped.push_back(obj->getName());
    }

    if (records.empty())
    {
        QMessageBox::information(m_app->getMainWindow(),
                                 tr("SOLIS Crown Exposure Export"),
                                 tr("None of the selected entities has a stored Crown Exposure summary.\n\n"
                                    "Run SOLIS Exposure with Crown Exposure Index enabled first."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        m_app->getMainWindow(),
        tr("Export Crown Exposure Summary"),
        QDir::homePath() + QStringLiteral("/qSOLIS_crown_exposure_summary.csv"),
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    QString error;
    if (!solis::CrownExposureExport::WriteCsv(path, records, &error))
    {
        QMessageBox::critical(m_app->getMainWindow(),
                              tr("SOLIS Crown Exposure Export"),
                              error);
        return;
    }

    m_app->dispToConsole(
        tr("SOLIS Crown Exposure export: wrote %1 tree/entity row(s) to '%2'%3")
            .arg(static_cast<qulonglong>(records.size()))
            .arg(path)
            .arg(skipped.isEmpty()
                     ? QString()
                     : tr("; skipped %1 selected entity/entities without a crown summary")
                           .arg(static_cast<qulonglong>(skipped.size()))),
        ccMainAppInterface::STD_CONSOLE_MESSAGE);
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
    onNewSelection(m_app->getSelectedEntities());
}

void qSOLIS::registerCommands(ccCommandLineInterface* cmd)
{
    if (!cmd)
        return;
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCommand));
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISExposureCommand));
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCrownExportCommand));
    cmd->registerCommand(ccCommandLineInterface::Command::Shared(new SOLISCleanupCommand));
}
