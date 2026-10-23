#include "SOLISCrownExport.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <utility>

namespace solis
{
namespace
{
double jsonNumber(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

unsigned jsonUnsigned(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return 0;
    const double numeric = value.toDouble();
    if (!std::isfinite(numeric) || numeric <= 0.0)
        return 0;
    return static_cast<unsigned>(numeric);
}

QStringList jsonStringArray(const QJsonObject& object, const QString& key)
{
    QStringList values;
    const QJsonArray array = object.value(key).toArray();
    values.reserve(array.size());
    for (const QJsonValue& value : array)
    {
        if (value.isString())
            values.push_back(value.toString());
    }
    return values;
}

QString csvEscape(QString value)
{
    const bool quote = value.contains(QLatin1Char(','))
                    || value.contains(QLatin1Char('"'))
                    || value.contains(QLatin1Char('\n'))
                    || value.contains(QLatin1Char('\r'));
    if (!quote)
        return value;

    value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

QString csvNumber(double value)
{
    if (!std::isfinite(value))
        return QString();
    return QLocale::c().toString(value, 'g', 15);
}

QString csvInteger(qulonglong value)
{
    return QLocale::c().toString(value);
}

QString csvBoolean(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QDateTime parseDateTime(const QString& text)
{
    QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODate);
    return dt;
}

bool isNewer(const CrownExposureExportRecord& lhs, const CrownExposureExportRecord& rhs)
{
    const QDateTime lhsTime = parseDateTime(lhs.createdUtc);
    const QDateTime rhsTime = parseDateTime(rhs.createdUtc);
    if (lhsTime.isValid() && rhsTime.isValid() && lhsTime != rhsTime)
        return lhsTime > rhsTime;
    if (lhs.createdUtc != rhs.createdUtc)
        return lhs.createdUtc > rhs.createdUtc;
    return lhs.runId > rhs.runId;
}

CrownExposureExportRecord parseRecord(const QString& cloudName,
                                      unsigned cloudUniqueId,
                                      const QString& key,
                                      const QVariant& value)
{
    CrownExposureExportRecord record;
    const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8());
    if (!document.isObject())
        return record;

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("resultFamily")).toString() != QLatin1String("crownExposure"))
        return record;

    record.schemaVersion = object.value(QStringLiteral("schemaVersion")).toInt();
    record.pluginVersion = object.value(QStringLiteral("pluginVersion")).toString();
    record.runId = object.value(QStringLiteral("runId")).toString();
    if (record.runId.isEmpty() && key.startsWith(QStringLiteral("qSOLIS/crownExposure/")))
        record.runId = key.mid(QStringLiteral("qSOLIS/crownExposure/").size());
    record.entityName = object.value(QStringLiteral("entityName")).toString();
    record.createdUtc = object.value(QStringLiteral("createdUtc")).toString();

    record.cloudName = cloudName;
    record.cloudUniqueId = cloudUniqueId;
    if (record.entityName.isEmpty())
        record.entityName = record.cloudName;

    record.cei = jsonNumber(object, QStringLiteral("cei"));
    record.skyMean = jsonNumber(object, QStringLiteral("skyMean"));
    record.sunMean = jsonNumber(object, QStringLiteral("sunMean"));
    record.radiativeMean = jsonNumber(object, QStringLiteral("radiativeMean"));
    record.reiP10 = jsonNumber(object, QStringLiteral("reiP10"));
    record.reiP50 = jsonNumber(object, QStringLiteral("reiP50"));
    record.reiP90 = jsonNumber(object, QStringLiteral("reiP90"));
    record.exposedFraction = jsonNumber(object, QStringLiteral("exposedFraction"));
    record.threshold = jsonNumber(object, QStringLiteral("threshold"));

    record.weightingMethod = object.value(QStringLiteral("weightingMethod")).toString();
    record.voxelSize = jsonNumber(object, QStringLiteral("voxelSize"));
    record.occupiedVoxelCount = jsonUnsigned(object, QStringLiteral("occupiedVoxelCount"));
    record.representedSurfaceArea = jsonNumber(object, QStringLiteral("representedSurfaceArea"));
    record.pointOrVertexCount = jsonUnsigned(object, QStringLiteral("pointOrVertexCount"));

    record.occlusionContext = object.value(QStringLiteral("occlusionContext")).toString();
    record.occluderCount = jsonUnsigned(object, QStringLiteral("occluderCount"));
    record.occluderEntities = jsonStringArray(object, QStringLiteral("occluderEntities"));
    record.sourceExposureResultIds = jsonStringArray(object, QStringLiteral("sourceExposureResultIds"));

    const QJsonObject options = object.value(QStringLiteral("options")).toObject();
    const QJsonObject site = options.value(QStringLiteral("site")).toObject();
    const QJsonObject time = options.value(QStringLiteral("time")).toObject();
    const QJsonObject render = options.value(QStringLiteral("render")).toObject();
    const QJsonObject sampling = options.value(QStringLiteral("sampling")).toObject();

    record.latitudeDeg = jsonNumber(site, QStringLiteral("latitudeDeg"));
    record.longitudeDeg = jsonNumber(site, QStringLiteral("longitudeDeg"));
    record.elevationM = jsonNumber(site, QStringLiteral("elevationM"));
    record.linkeTurbidity = jsonNumber(site, QStringLiteral("linkeTurbidity"));
    record.timeMode = site.value(QStringLiteral("timeMode")).toString();
    record.utcOffsetHours = jsonNumber(site, QStringLiteral("utcOffsetHours"));

    record.dayOfYear = jsonNumber(time, QStringLiteral("dayOfYear"));
    record.temporalMode = time.value(QStringLiteral("temporalMode")).toString();
    record.durationHours = jsonNumber(time, QStringLiteral("durationHours"));
    record.timestepMinutes = jsonNumber(time, QStringLiteral("timestepMinutes"));

    record.resolution = render.value(QStringLiteral("resolution")).toInt();
    record.skyRayCount = jsonUnsigned(sampling, QStringLiteral("diffuseRayCount"));
    record.acceleratedDirectSampling = sampling.value(QStringLiteral("acceleratedDirectSampling")).toBool(false);
    record.directAngularToleranceDeg = jsonNumber(sampling, QStringLiteral("directAngularToleranceDeg"));
    return record;
}

QStringList csvFields(const CrownExposureExportRecord& record)
{
    QStringList fields;
    fields.reserve(38);
    fields
        << record.entityName
        << record.cloudName
        << csvInteger(record.cloudUniqueId)
        << record.runId
        << record.createdUtc
        << csvInteger(static_cast<unsigned>(std::max(0, record.schemaVersion)))
        << record.pluginVersion
        << csvNumber(record.cei)
        << csvNumber(record.skyMean)
        << csvNumber(record.sunMean)
        << csvNumber(record.radiativeMean)
        << csvNumber(record.reiP10)
        << csvNumber(record.reiP50)
        << csvNumber(record.reiP90)
        << csvNumber(record.threshold)
        << csvNumber(record.exposedFraction)
        << (std::isfinite(record.exposedFraction) ? csvNumber(record.exposedFraction * 100.0) : QString())
        << record.weightingMethod
        << (record.weightingMethod == QLatin1String("occupied-voxel") ? csvNumber(record.voxelSize) : QString())
        << (record.weightingMethod == QLatin1String("occupied-voxel") ? csvInteger(record.occupiedVoxelCount) : QString())
        << (record.weightingMethod == QLatin1String("surface-area") ? csvNumber(record.representedSurfaceArea) : QString())
        << csvInteger(record.pointOrVertexCount)
        << record.occlusionContext
        << csvInteger(record.occluderCount)
        << record.occluderEntities.join(QStringLiteral("; "))
        << csvNumber(record.latitudeDeg)
        << csvNumber(record.longitudeDeg)
        << csvNumber(record.elevationM)
        << csvNumber(record.linkeTurbidity)
        << record.timeMode
        << csvNumber(record.utcOffsetHours)
        << csvNumber(record.dayOfYear)
        << record.temporalMode
        << csvNumber(record.durationHours)
        << csvNumber(record.timestepMinutes)
        << csvInteger(static_cast<unsigned>(std::max(0, record.resolution)))
        << csvInteger(record.skyRayCount)
        << csvBoolean(record.acceleratedDirectSampling)
        << csvNumber(record.directAngularToleranceDeg)
        << record.sourceExposureResultIds.join(QStringLiteral("; "));
    return fields;
}
} // namespace

std::vector<CrownExposureExportRecord> CrownExposureExport::RecordsFromMetadata(const QVariantMap& metadata,
                                                                                 const QString& cloudName,
                                                                                 unsigned cloudUniqueId)
{
    std::vector<CrownExposureExportRecord> records;
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("qSOLIS/crownExposure/")))
            continue;

        CrownExposureExportRecord record = parseRecord(cloudName, cloudUniqueId, it.key(), it.value());
        if (record.runId.isEmpty() || !std::isfinite(record.cei))
            continue;
        records.push_back(std::move(record));
    }

    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b)
    {
        return isNewer(a, b);
    });
    return records;
}

bool CrownExposureExport::LatestRecordFromMetadata(const QVariantMap& metadata,
                                                   CrownExposureExportRecord& record,
                                                   const QString& cloudName,
                                                   unsigned cloudUniqueId)
{
    const std::vector<CrownExposureExportRecord> records = RecordsFromMetadata(metadata, cloudName, cloudUniqueId);
    if (records.empty())
        return false;
    record = records.front();
    return true;
}

QString CrownExposureExport::CsvHeader()
{
    return QStringLiteral(
        "entity_name,cloud_name,cloud_unique_id,run_id,created_utc,schema_version,plugin_version,"
        "cei,sky_mean,sun_mean,radiative_mean,rei_p10,rei_p50,rei_p90,"
        "exposure_threshold,exposed_fraction,exposed_percent,weighting_method,voxel_size_m,"
        "occupied_voxel_count,represented_surface_area_m2,point_or_vertex_count,"
        "occlusion_context,occluder_count,occluder_entities,"
        "latitude_deg,longitude_deg,elevation_m,linke_turbidity,time_mode,utc_offset_hours,"
        "day_of_year,temporal_mode,duration_hours,timestep_minutes,resolution,sky_ray_count,"
        "accelerated_direct_sampling,direct_angular_tolerance_deg,source_exposure_result_ids");
}

QString CrownExposureExport::CsvLine(const CrownExposureExportRecord& record)
{
    QStringList fields = csvFields(record);
    for (QString& field : fields)
        field = csvEscape(field);
    return fields.join(QLatin1Char(','));
}

QByteArray CrownExposureExport::CsvDocument(const std::vector<CrownExposureExportRecord>& records)
{
    QString text = CsvHeader();
    text += QLatin1Char('\n');
    for (const CrownExposureExportRecord& record : records)
    {
        text += CsvLine(record);
        text += QLatin1Char('\n');
    }
    return text.toUtf8();
}

bool CrownExposureExport::WriteCsv(const QString& path,
                                   const std::vector<CrownExposureExportRecord>& records,
                                   QString* errorMessage)
{
    if (path.trimmed().isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Export path is empty");
        return false;
    }
    if (records.empty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("No Crown Exposure summaries are available to export");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open '%1' for writing: %2").arg(path, file.errorString());
        return false;
    }

    const QByteArray data = CsvDocument(records);
    if (file.write(data) != data.size())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed while writing '%1': %2").arg(path, file.errorString());
        return false;
    }
    if (!file.flush())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed while flushing '%1': %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
}
