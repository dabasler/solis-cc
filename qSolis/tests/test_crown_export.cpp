#include "SOLISCrownExport.h"

#include <ccPointCloud.h>

#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

namespace
{
bool require(bool condition, const char* message)
{
    if (condition)
        return true;
    std::cerr << message << '\n';
    return false;
}
}

int main()
{
    solis::CrownExposureExportRecord record;
    record.schemaVersion = 3;
    record.pluginVersion = QStringLiteral("1.1.0");
    record.entityName = QStringLiteral("Tree, \"A\"");
    record.cloudName = QStringLiteral("Tree_A_cloud");
    record.cloudUniqueId = 42;
    record.runId = QStringLiteral("run-123");
    record.createdUtc = QStringLiteral("2026-10-23T12:00:00.000Z");
    record.cei = 0.6125;
    record.skyMean = 0.7;
    record.sunMean = 0.5;
    record.radiativeMean = 0.6125;
    record.reiP10 = 0.2;
    record.reiP50 = 0.63;
    record.reiP90 = 0.91;
    record.threshold = 0.75;
    record.exposedFraction = 0.384;
    record.weightingMethod = QStringLiteral("occupied-voxel");
    record.voxelSize = 0.18;
    record.occupiedVoxelCount = 123;
    record.pointOrVertexCount = 4567;
    record.occlusionContext = QStringLiteral("ALL_VISIBLE_ENTITIES");
    record.occluderCount = 2;
    record.occluderEntities = {QStringLiteral("Tree_B [id=7]"), QStringLiteral("Terrain [id=9]")};
    record.latitudeDeg = 47.0;
    record.longitudeDeg = 8.0;
    record.elevationM = 500.0;
    record.linkeTurbidity = 3.0;
    record.timeMode = QStringLiteral("UTC");
    record.utcOffsetHours = 0.0;
    record.dayOfYear = 172.0;
    record.temporalMode = QStringLiteral("ENERGY");
    record.durationHours = 24.0;
    record.timestepMinutes = 10.0;
    record.resolution = 1024;
    record.skyRayCount = 256;
    record.acceleratedDirectSampling = true;
    record.directAngularToleranceDeg = 1.0;
    record.sourceExposureResultIds = {QStringLiteral("r1"), QStringLiteral("r2")};

    bool ok = true;
    const QString line = solis::CrownExposureExport::CsvLine(record);
    ok &= require(line.startsWith(QStringLiteral("\"Tree, \"\"A\"\"\",")),
                  "CSV quoting/escaping failed");
    ok &= require(line.contains(QStringLiteral(",0.6125,")), "CEI missing from CSV line");
    ok &= require(line.contains(QStringLiteral(",38.4,")), "exposed percent missing from CSV line");
    ok &= require(line.contains(QStringLiteral("Tree_B [id=7]; Terrain [id=9]")),
                  "occluder list missing from CSV line");

    ccPointCloud metadataCloud(QStringLiteral("Tree_Metadata"));
    metadataCloud.setMetaData(
        QStringLiteral("qSOLIS/crownExposure/run-old"),
        QStringLiteral("{\"schemaVersion\":3,\"pluginVersion\":\"1.1.0\",\"resultFamily\":\"crownExposure\",\"runId\":\"run-old\",\"entityName\":\"Tree_001\",\"createdUtc\":\"2026-10-22T12:00:00.000Z\",\"cei\":0.4,\"radiativeMean\":0.4}"));
    metadataCloud.setMetaData(
        QStringLiteral("qSOLIS/crownExposure/run-new"),
        QStringLiteral("{\"schemaVersion\":3,\"pluginVersion\":\"1.1.0\",\"resultFamily\":\"crownExposure\",\"runId\":\"run-new\",\"entityName\":\"Tree_001\",\"createdUtc\":\"2026-10-23T12:00:00.000Z\",\"cei\":0.8,\"radiativeMean\":0.8,\"occlusionContext\":\"ALL_VISIBLE_ENTITIES\"}"));

    const auto parsed = solis::CrownExposureExport::Records(&metadataCloud);
    ok &= require(parsed.size() == 2, "crown metadata parser should return two records");
    solis::CrownExposureExportRecord latest;
    ok &= require(solis::CrownExposureExport::LatestRecord(&metadataCloud, latest),
                  "latest crown metadata lookup failed");
    ok &= require(latest.runId == QStringLiteral("run-new"), "latest crown run selection failed");
    ok &= require(std::abs(latest.cei - 0.8) < 1.0e-12, "latest crown CEI parse failed");

    std::vector<solis::CrownExposureExportRecord> records{record};
    const QByteArray document = solis::CrownExposureExport::CsvDocument(records);
    ok &= require(document.startsWith("entity_name,cloud_name,"), "CSV header missing");
    ok &= require(document.count('\n') == 2, "CSV document should contain header plus one row");

    QTemporaryDir dir;
    ok &= require(dir.isValid(), "temporary directory creation failed");
    if (dir.isValid())
    {
        const QString path = dir.path() + QStringLiteral("/summary.csv");
        QString error;
        ok &= require(solis::CrownExposureExport::WriteCsv(path, records, &error),
                      qPrintable(QStringLiteral("CSV write failed: %1").arg(error)));
        QFile file(path);
        ok &= require(file.open(QIODevice::ReadOnly), "CSV output could not be reopened");
        if (file.isOpen())
            ok &= require(file.readAll() == document, "written CSV differs from serialized document");
    }

    return ok ? 0 : 1;
}
