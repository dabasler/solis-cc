#include "SOLISCrownExportCommand.h"

#include "SOLISCrownExport.h"

#include <ccGenericMesh.h>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>

#include <QSet>

#include <utility>

namespace
{
constexpr char COMMAND_SOLIS_EXPORT_CROWN[] = "SOLIS_EXPORT_CROWN";

void appendCloudRecords(ccPointCloud* cloud,
                        bool allRuns,
                        QSet<ccPointCloud*>& processed,
                        std::vector<solis::CrownExposureExportRecord>& records)
{
    if (!cloud || processed.contains(cloud))
        return;
    processed.insert(cloud);

    if (allRuns)
    {
        std::vector<solis::CrownExposureExportRecord> cloudRecords = solis::CrownExposureExport::Records(cloud);
        records.insert(records.end(), cloudRecords.begin(), cloudRecords.end());
        return;
    }

    solis::CrownExposureExportRecord record;
    if (solis::CrownExposureExport::LatestRecord(cloud, record))
        records.push_back(std::move(record));
}
}

SOLISCrownExportCommand::SOLISCrownExportCommand()
    : ccCommandLineInterface::Command("SOLIS Crown Exposure CSV export", COMMAND_SOLIS_EXPORT_CROWN)
{
}

bool SOLISCrownExportCommand::process(ccCommandLineInterface& cmd)
{
    cmd.print("[SOLIS EXPORT CROWN]");

    if (cmd.clouds().empty() && cmd.meshes().empty())
        return cmd.error(QObject::tr("SOLIS crown export: no cloud or mesh is loaded"));
    if (cmd.arguments().empty())
        return cmd.error(QObject::tr("SOLIS crown export: expected output CSV path after -SOLIS_EXPORT_CROWN"));

    QString outputPath = cmd.arguments().takeFirst();
    if (outputPath.startsWith(QLatin1Char('-')) || outputPath.trimmed().isEmpty())
        return cmd.error(QObject::tr("SOLIS crown export: invalid/missing output CSV path"));

    bool allRuns = false;
    if (!cmd.arguments().empty() && !cmd.arguments().front().startsWith(QLatin1Char('-')))
    {
        const QString mode = cmd.arguments().takeFirst().toUpper();
        if (mode == QLatin1String("ALL_RUNS"))
            allRuns = true;
        else if (mode != QLatin1String("LATEST"))
            return cmd.error(QObject::tr("SOLIS crown export: unknown mode '%1' (LATEST|ALL_RUNS)").arg(mode));
    }

    if (!outputPath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        outputPath += QStringLiteral(".csv");

    QSet<ccPointCloud*> processed;
    std::vector<solis::CrownExposureExportRecord> records;

    for (CLCloudDesc& desc : cmd.clouds())
        appendCloudRecords(desc.pc, allRuns, processed, records);

    for (CLMeshDesc& desc : cmd.meshes())
    {
        ccGenericMesh* mesh = desc.mesh;
        ccPointCloud* cloud = mesh ? ccHObjectCaster::ToPointCloud(mesh->getAssociatedCloud()) : nullptr;
        appendCloudRecords(cloud, allRuns, processed, records);
    }

    if (records.empty())
        return cmd.error(QObject::tr("SOLIS crown export: no stored Crown Exposure summaries were found"));

    QString error;
    if (!solis::CrownExposureExport::WriteCsv(outputPath, records, &error))
        return cmd.error(QObject::tr("SOLIS crown export: %1").arg(error));

    cmd.print(QObject::tr("SOLIS crown export: wrote %1 row(s) to '%2' (%3)")
                  .arg(static_cast<qulonglong>(records.size()))
                  .arg(outputPath, allRuns ? QStringLiteral("all stored runs") : QStringLiteral("latest run per entity")));
    return true;
}
