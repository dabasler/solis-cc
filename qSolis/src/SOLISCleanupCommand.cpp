#include "SOLISCleanupCommand.h"

#include "SOLISResults.h"

#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>

#include <QSet>

namespace
{
constexpr char COMMAND_SOLIS_CLEAN[] = "SOLIS_CLEAN";

enum class CleanupMode
{
    LastRun,
    LastBatch,
    Batch,
    All,
    Legacy
};

bool cleanupCloud(ccCommandLineInterface& cmd,
                  ccPointCloud* cloud,
                  const QString& entityName,
                  CleanupMode mode,
                  const QString& batchId)
{
    if (!cloud)
        return true;

    solis::CleanupStats stats;
    QString target;

    switch (mode)
    {
    case CleanupMode::LastRun:
    {
        const QString runId = solis::ResultManager::LastRunId(cloud);
        if (runId.isEmpty())
        {
            cmd.warning(QObject::tr("SOLIS cleanup: '%1' has no metadata-backed SOLIS run").arg(entityName));
            return true;
        }
        stats = solis::ResultManager::RemoveRun(cloud, runId);
        target = QStringLiteral("last run %1").arg(runId);
        break;
    }
    case CleanupMode::LastBatch:
    {
        const QString id = solis::ResultManager::LastBatchId(cloud);
        if (id.isEmpty())
        {
            cmd.warning(QObject::tr("SOLIS cleanup: '%1' has no metadata-backed SOLIS batch").arg(entityName));
            return true;
        }
        stats = solis::ResultManager::RemoveBatch(cloud, id);
        target = QStringLiteral("last batch %1").arg(id);
        break;
    }
    case CleanupMode::Batch:
        stats = solis::ResultManager::RemoveBatch(cloud, batchId);
        target = QStringLiteral("batch %1").arg(batchId);
        break;
    case CleanupMode::All:
        stats = solis::ResultManager::RemoveAll(cloud);
        target = QStringLiteral("all metadata-backed results");
        break;
    case CleanupMode::Legacy:
        stats = solis::ResultManager::RemoveLegacyNamedFields(cloud);
        target = QStringLiteral("legacy SOLIS-named fields");
        break;
    }

    cloud->prepareDisplayForRefresh_recursive();
    cmd.print(QObject::tr("SOLIS cleanup: '%1' %2 -> removed %3 field(s), %4 metadata record(s), %5 missing field reference(s)")
                  .arg(entityName, target)
                  .arg(stats.fieldsRemoved)
                  .arg(stats.metadataRecordsRemoved)
                  .arg(stats.missingFields));
    return true;
}

} // namespace

SOLISCleanupCommand::SOLISCleanupCommand()
    : ccCommandLineInterface::Command("SOLIS cleanup", COMMAND_SOLIS_CLEAN)
{
}

bool SOLISCleanupCommand::process(ccCommandLineInterface& cmd)
{
    cmd.print("[SOLIS CLEANUP]");

    if (cmd.clouds().empty() && cmd.meshes().empty())
        return cmd.error(QObject::tr("SOLIS cleanup: no cloud or mesh is loaded"));
    if (cmd.arguments().empty())
        return cmd.error(QObject::tr("SOLIS cleanup: expected LAST_RUN|LAST_BATCH|BATCH <uuid>|ALL|LEGACY after -SOLIS_CLEAN"));

    const QString modeToken = cmd.arguments().takeFirst().toUpper();
    CleanupMode mode;
    QString batchId;

    if (modeToken == QLatin1String("LAST_RUN"))
        mode = CleanupMode::LastRun;
    else if (modeToken == QLatin1String("LAST_BATCH"))
        mode = CleanupMode::LastBatch;
    else if (modeToken == QLatin1String("ALL"))
        mode = CleanupMode::All;
    else if (modeToken == QLatin1String("LEGACY"))
        mode = CleanupMode::Legacy;
    else if (modeToken == QLatin1String("BATCH"))
    {
        mode = CleanupMode::Batch;
        if (cmd.arguments().empty())
            return cmd.error(QObject::tr("SOLIS cleanup: BATCH requires a batch UUID"));
        batchId = cmd.arguments().takeFirst();
        if (batchId.startsWith(QLatin1Char('-')) || batchId.trimmed().isEmpty())
            return cmd.error(QObject::tr("SOLIS cleanup: invalid/missing batch UUID after BATCH"));
    }
    else
    {
        return cmd.error(QObject::tr("SOLIS cleanup: unknown mode '%1' (LAST_RUN|LAST_BATCH|BATCH <uuid>|ALL|LEGACY)")
                             .arg(modeToken));
    }

    QSet<ccPointCloud*> processed;
    for (CLCloudDesc& desc : cmd.clouds())
    {
        if (!desc.pc || processed.contains(desc.pc))
            continue;
        processed.insert(desc.pc);
        if (!cleanupCloud(cmd, desc.pc, desc.pc->getName(), mode, batchId))
            return false;
    }

    for (CLMeshDesc& desc : cmd.meshes())
    {
        ccGenericMesh* mesh = desc.mesh;
        ccPointCloud* cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        if (!cloud || processed.contains(cloud))
            continue;
        processed.insert(cloud);
        if (!cleanupCloud(cmd, cloud, mesh ? mesh->getName() : QStringLiteral("mesh"), mode, batchId))
            return false;
    }

    if (cmd.autoSaveMode())
    {
        for (CLCloudDesc& desc : cmd.clouds())
        {
            const QString error = cmd.exportEntity(desc, QStringLiteral("SOLIS_CLEAN"));
            if (!error.isEmpty())
                return cmd.error(error);
        }
        for (CLMeshDesc& desc : cmd.meshes())
        {
            const QString error = cmd.exportEntity(desc, QStringLiteral("SOLIS_CLEAN"));
            if (!error.isEmpty())
                return cmd.error(error);
        }
    }

    return true;
}
