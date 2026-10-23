#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <limits>
#include <vector>

class ccPointCloud;

namespace solis
{
//! One persisted Crown Exposure summary, flattened for tabular export.
struct CrownExposureExportRecord
{
    int schemaVersion = 0;
    QString pluginVersion;
    QString entityName;
    QString cloudName;
    unsigned cloudUniqueId = 0;
    QString runId;
    QString createdUtc;

    double cei = std::numeric_limits<double>::quiet_NaN();
    double skyMean = std::numeric_limits<double>::quiet_NaN();
    double sunMean = std::numeric_limits<double>::quiet_NaN();
    double radiativeMean = std::numeric_limits<double>::quiet_NaN();
    double reiP10 = std::numeric_limits<double>::quiet_NaN();
    double reiP50 = std::numeric_limits<double>::quiet_NaN();
    double reiP90 = std::numeric_limits<double>::quiet_NaN();
    double exposedFraction = std::numeric_limits<double>::quiet_NaN();
    double threshold = std::numeric_limits<double>::quiet_NaN();

    QString weightingMethod;
    double voxelSize = std::numeric_limits<double>::quiet_NaN();
    unsigned occupiedVoxelCount = 0;
    double representedSurfaceArea = std::numeric_limits<double>::quiet_NaN();
    unsigned pointOrVertexCount = 0;

    QString occlusionContext;
    unsigned occluderCount = 0;
    QStringList occluderEntities;
    QStringList sourceExposureResultIds;

    double latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double longitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double elevationM = std::numeric_limits<double>::quiet_NaN();
    double linkeTurbidity = std::numeric_limits<double>::quiet_NaN();
    QString timeMode;
    double utcOffsetHours = std::numeric_limits<double>::quiet_NaN();

    double dayOfYear = std::numeric_limits<double>::quiet_NaN();
    QString temporalMode;
    double durationHours = std::numeric_limits<double>::quiet_NaN();
    double timestepMinutes = std::numeric_limits<double>::quiet_NaN();

    int resolution = 0;
    unsigned skyRayCount = 0;
    bool acceleratedDirectSampling = false;
    double directAngularToleranceDeg = std::numeric_limits<double>::quiet_NaN();
};

//! Reads persistent qSOLIS Crown Exposure metadata and exports stable CSV tables.
class CrownExposureExport
{
public:
    //! Parses Crown Exposure records from an arbitrary metadata map. This QtCore-only path
    //! is also used by unit tests so they do not need to instantiate CloudCompare DB objects.
    static std::vector<CrownExposureExportRecord> RecordsFromMetadata(const QVariantMap& metadata,
                                                                       const QString& cloudName = QString(),
                                                                       unsigned cloudUniqueId = 0);

    //! Returns the most recent record from an arbitrary metadata map.
    static bool LatestRecordFromMetadata(const QVariantMap& metadata,
                                         CrownExposureExportRecord& record,
                                         const QString& cloudName = QString(),
                                         unsigned cloudUniqueId = 0);

    //! Returns all valid qSOLIS/crownExposure/* records stored on a cloud.
    static std::vector<CrownExposureExportRecord> Records(const ccPointCloud* cloud);

    //! Returns the most recently created crown summary stored on a cloud.
    static bool LatestRecord(const ccPointCloud* cloud, CrownExposureExportRecord& record);

    //! CSV serialization helpers. Numeric output always uses the C locale.
    static QString CsvHeader();
    static QString CsvLine(const CrownExposureExportRecord& record);
    static QByteArray CsvDocument(const std::vector<CrownExposureExportRecord>& records);

    //! Writes a UTF-8 CSV file, replacing an existing file.
    static bool WriteCsv(const QString& path,
                         const std::vector<CrownExposureExportRecord>& records,
                         QString* errorMessage = nullptr);
};
}
