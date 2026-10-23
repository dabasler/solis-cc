#include "SOLISResults.h"
#include "SOLISVersion.h"

#include <ccPointCloud.h>
#include <ccScalarField.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace solis
{
namespace
{
QString formatCompactNumber(double value, int precision = 6)
{
    QString text = QString::number(value, 'f', precision);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

struct DayClock
{
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

DayClock splitDayOfYear(double dayOfYear)
{
    DayClock result;
    if (!std::isfinite(dayOfYear))
        return result;

    double dayFloor = std::floor(dayOfYear);
    result.day = std::max(1, std::min(366, static_cast<int>(dayFloor)));

    double fraction = dayOfYear - dayFloor;
    if (fraction < 0.0)
        fraction = 0.0;

    // Round to the nearest second. Carrying across midnight is intentionally
    // represented as 23:59:59 rather than changing DOY in a field label.
    int totalSeconds = static_cast<int>(std::lround(fraction * 86400.0));
    totalSeconds = std::max(0, std::min(86399, totalSeconds));
    result.hour = totalSeconds / 3600;
    result.minute = (totalSeconds % 3600) / 60;
    result.second = totalSeconds % 60;
    return result;
}

QString clockToken(const DayClock& clock)
{
    if (clock.second == 0)
    {
        return QStringLiteral("T%1%2")
            .arg(clock.hour, 2, 10, QLatin1Char('0'))
            .arg(clock.minute, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("T%1%2%3")
        .arg(clock.hour, 2, 10, QLatin1Char('0'))
        .arg(clock.minute, 2, 10, QLatin1Char('0'))
        .arg(clock.second, 2, 10, QLatin1Char('0'));
}

QString spatialToken(SpatialMode mode)
{
    switch (mode)
    {
    case SpatialMode::Visibility: return QStringLiteral("V");
    case SpatialMode::HorizontalIrradiance: return QStringLiteral("H");
    case SpatialMode::SurfaceIrradiance: return QStringLiteral("S");
    }
    return QStringLiteral("?");
}

QString metadataKey(const QString& resultId)
{
    return QStringLiteral("qSOLIS/result/%1").arg(resultId);
}

QJsonObject optionsJson(const CalculationOptions& options)
{
    QJsonObject site;
    site.insert(QStringLiteral("latitudeDeg"), options.site.latitudeDeg);
    site.insert(QStringLiteral("longitudeDeg"), options.site.longitudeDeg);
    site.insert(QStringLiteral("elevationM"), options.site.elevationM);
    site.insert(QStringLiteral("linkeTurbidity"), options.site.linkeTurbidity);
    site.insert(QStringLiteral("timeMode"), options.site.timeMode == TimeMode::UTC
                                                   ? QStringLiteral("UTC")
                                                   : QStringLiteral("LOCAL_FIXED_OFFSET"));
    site.insert(QStringLiteral("utcOffsetHours"), options.site.utcOffsetHours);

    QJsonObject time;
    time.insert(QStringLiteral("dayOfYear"), options.time.dayOfYear);
    time.insert(QStringLiteral("temporalMode"), ResultManager::TemporalModeName(options.time.mode));
    time.insert(QStringLiteral("durationHours"), options.time.durationHours);
    time.insert(QStringLiteral("timestepMinutes"), options.time.timestepMinutes);
    time.insert(QStringLiteral("energyUnit"), options.time.energyUnit == EnergyUnit::KWhPerM2
                                                  ? QStringLiteral("kWh/m2")
                                                  : QStringLiteral("Wh/m2"));

    QJsonObject render;
    render.insert(QStringLiteral("resolution"), static_cast<int>(options.render.resolution));
    render.insert(QStringLiteral("meshIsClosed"), options.render.meshIsClosed);
    render.insert(QStringLiteral("depthBias"), options.render.depthBias);
    render.insert(QStringLiteral("parallelCpuProjection"), options.render.parallelCpuProjection);
    render.insert(QStringLiteral("cpuThreadCount"), static_cast<int>(options.render.cpuThreadCount));

    QJsonObject sampling;
    sampling.insert(QStringLiteral("acceleratedDirectSampling"), options.sampling.acceleratedDirectSampling);
    sampling.insert(QStringLiteral("directAngularToleranceDeg"), options.sampling.directAngularToleranceDeg);
    sampling.insert(QStringLiteral("diffuseRayCount"), static_cast<int>(options.sampling.diffuseRayCount));

    QJsonObject root;
    root.insert(QStringLiteral("site"), site);
    root.insert(QStringLiteral("time"), time);
    root.insert(QStringLiteral("render"), render);
    root.insert(QStringLiteral("sampling"), sampling);
    root.insert(QStringLiteral("spatialMode"), ResultManager::SpatialModeName(options.spatialMode));
    return root;
}


bool parseJsonObject(const QVariant& value, QJsonObject& object)
{
    const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8());
    if (!document.isObject())
        return false;
    object = document.object();
    return true;
}

ResultRecord parseResultRecord(const ccPointCloud* cloud,
                               const QString& key,
                               const QVariant& value)
{
    ResultRecord record;
    record.metadataKey = key;

    QJsonObject object;
    if (!parseJsonObject(value, object))
        return record;

    record.resultId = object.value(QStringLiteral("resultId")).toString();
    record.fieldName = object.value(QStringLiteral("fieldName")).toString();
    record.runId = object.value(QStringLiteral("runId")).toString();
    record.component = object.value(QStringLiteral("component")).toString();
    record.unit = object.value(QStringLiteral("unit")).toString();
    record.createdUtc = object.value(QStringLiteral("createdUtc")).toString();

    const QJsonObject batch = object.value(QStringLiteral("batch")).toObject();
    record.batchId = batch.value(QStringLiteral("batchId")).toString();

    if (cloud && !record.fieldName.isEmpty())
        record.fieldIndex = cloud->getScalarFieldIndexByName(qPrintable(record.fieldName));

    return record;
}

BatchRecord parseBatchRecord(const QString& key, const QVariant& value)
{
    BatchRecord record;
    record.metadataKey = key;

    QJsonObject object;
    if (!parseJsonObject(value, object))
        return record;

    record.batchId = object.value(QStringLiteral("batchId")).toString();
    record.createdUtc = object.value(QStringLiteral("createdUtc")).toString();
    record.count = static_cast<unsigned>(std::max(0, object.value(QStringLiteral("count")).toInt()));
    record.startDayOfYear = object.value(QStringLiteral("startDayOfYear")).toDouble();
    record.endDayOfYear = object.value(QStringLiteral("endDayOfYear")).toDouble();
    record.stepMinutes = object.value(QStringLiteral("stepMinutes")).toDouble();
    return record;
}

void refreshTrackingMetadata(ccPointCloud* cloud)
{
    if (!cloud)
        return;

    const std::vector<ResultRecord> results = ResultManager::Results(cloud);

    QString newestRunId;
    QString newestRunTime;
    for (const ResultRecord& result : results)
    {
        if (!result.runId.isEmpty() && result.createdUtc >= newestRunTime)
        {
            newestRunTime = result.createdUtc;
            newestRunId = result.runId;
        }
    }
    if (newestRunId.isEmpty())
        cloud->removeMetaData(QStringLiteral("qSOLIS/lastRunId"));
    else
        cloud->setMetaData(QStringLiteral("qSOLIS/lastRunId"), newestRunId);

    const std::vector<BatchRecord> batches = ResultManager::Batches(cloud);
    QString newestBatchId;
    QString newestBatchTime;
    for (const BatchRecord& batch : batches)
    {
        if (batch.storedResultCount != 0 && !batch.batchId.isEmpty() && batch.createdUtc >= newestBatchTime)
        {
            newestBatchTime = batch.createdUtc;
            newestBatchId = batch.batchId;
        }
    }
    if (newestBatchId.isEmpty())
        cloud->removeMetaData(QStringLiteral("qSOLIS/lastBatchId"));
    else
        cloud->setMetaData(QStringLiteral("qSOLIS/lastBatchId"), newestBatchId);

    if (results.empty() && batches.empty())
    {
        cloud->removeMetaData(QStringLiteral("qSOLIS/schemaVersion"));
        cloud->removeMetaData(QStringLiteral("qSOLIS/pluginVersion"));
    }
}

void removeOrphanBatchMetadata(ccPointCloud* cloud, CleanupStats& stats)
{
    if (!cloud)
        return;

    const std::vector<ResultRecord> remaining = ResultManager::Results(cloud);
    QSet<QString> liveBatchIds;
    for (const ResultRecord& result : remaining)
    {
        if (!result.batchId.isEmpty())
            liveBatchIds.insert(result.batchId);
    }

    QStringList orphanKeys;
    const QVariantMap& metadata = cloud->metaData();
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("qSOLIS/batch/")))
            continue;
        const BatchRecord batch = parseBatchRecord(it.key(), it.value());
        if (batch.batchId.isEmpty() || !liveBatchIds.contains(batch.batchId))
            orphanKeys.push_back(it.key());
    }

    for (const QString& key : orphanKeys)
    {
        if (cloud->removeMetaData(key))
            ++stats.metadataRecordsRemoved;
    }
}

CleanupStats removeMatchingResults(ccPointCloud* cloud,
                                   const std::function<bool(const ResultRecord&)>& predicate)
{
    CleanupStats stats;
    if (!cloud)
        return stats;

    const std::vector<ResultRecord> records = ResultManager::Results(cloud);
    std::vector<std::pair<int, QString>> fields;
    QStringList metadataKeys;

    for (const ResultRecord& record : records)
    {
        if (!predicate(record))
            continue;

        metadataKeys.push_back(record.metadataKey);
        if (record.fieldIndex >= 0)
            fields.emplace_back(record.fieldIndex, record.fieldName);
        else
            ++stats.missingFields;
    }

    // Delete from highest index down so CloudCompare's compact scalar-field
    // array cannot invalidate an index we still plan to remove.
    std::sort(fields.begin(), fields.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& field : fields)
    {
        const int currentIndex = cloud->getScalarFieldIndexByName(qPrintable(field.second));
        if (currentIndex >= 0)
        {
            cloud->deleteScalarField(currentIndex);
            ++stats.fieldsRemoved;
        }
        else
        {
            ++stats.missingFields;
        }
    }

    for (const QString& key : metadataKeys)
    {
        if (cloud->removeMetaData(key))
            ++stats.metadataRecordsRemoved;
    }

    removeOrphanBatchMetadata(cloud, stats);
    refreshTrackingMetadata(cloud);
    return stats;
}

} // namespace

QString ResultManager::NewRunId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString ResultManager::ComponentName(ResultComponent component)
{
    switch (component)
    {
    case ResultComponent::Direct: return QStringLiteral("Direct");
    case ResultComponent::Diffuse: return QStringLiteral("Diffuse");
    case ResultComponent::Total: return QStringLiteral("Total");
    }
    return QStringLiteral("Unknown");
}

QString ResultManager::TemporalModeName(TemporalMode mode)
{
    switch (mode)
    {
    case TemporalMode::Instantaneous: return QStringLiteral("Instantaneous");
    case TemporalMode::MeanIrradiance: return QStringLiteral("Mean");
    case TemporalMode::IntegratedEnergy: return QStringLiteral("Integrated");
    }
    return QStringLiteral("Unknown");
}

QString ResultManager::SpatialModeName(SpatialMode mode)
{
    switch (mode)
    {
    case SpatialMode::Visibility: return QStringLiteral("Visibility");
    case SpatialMode::HorizontalIrradiance: return QStringLiteral("Horizontal");
    case SpatialMode::SurfaceIrradiance: return QStringLiteral("Surface");
    }
    return QStringLiteral("Unknown");
}

QString ResultManager::BaseFieldName(ResultComponent component,
                                     const CalculationOptions& options)
{
    const DayClock clock = splitDayOfYear(options.time.dayOfYear);

    QString componentLabel = ComponentName(component);
    if (options.time.mode == TemporalMode::MeanIrradiance)
        componentLabel += QStringLiteral(" Mean");
    if (options.spatialMode == SpatialMode::Visibility)
        componentLabel += QStringLiteral(" Visibility");

    QString name = QStringLiteral("SOLIS %1 [%2] D%3 %4")
                       .arg(componentLabel)
                       .arg(SOLIS::OutputUnit(options))
                       .arg(clock.day)
                       .arg(clockToken(clock));

    if (options.site.timeMode == TimeMode::UTC)
    {
        name += QStringLiteral(" UTC");
    }
    else
    {
        const double offset = options.site.utcOffsetHours;
        name += QStringLiteral(" L%1%2")
                    .arg(offset >= 0.0 ? QStringLiteral("+") : QStringLiteral("-"))
                    .arg(formatCompactNumber(std::abs(offset), 2));
    }

    if (options.time.mode != TemporalMode::Instantaneous)
    {
        name += QStringLiteral(" dur%1h dt%2m")
                    .arg(formatCompactNumber(options.time.durationHours, 4))
                    .arg(formatCompactNumber(options.time.timestepMinutes, 4));
    }

    if (options.spatialMode != SpatialMode::Visibility)
        name += QStringLiteral(" %1").arg(spatialToken(options.spatialMode));

    name += QStringLiteral(" R%1").arg(options.render.resolution);
    return name;
}

QString ResultManager::UniqueFieldName(const ccPointCloud* cloud,
                                       const QString& baseName)
{
    if (!cloud || cloud->getScalarFieldIndexByName(qPrintable(baseName)) < 0)
        return baseName;

    for (unsigned suffix = 2; suffix < 1000000; ++suffix)
    {
        const QString candidate = QStringLiteral("%1 #%2").arg(baseName).arg(suffix);
        if (cloud->getScalarFieldIndexByName(qPrintable(candidate)) < 0)
            return candidate;
    }

    // Practically unreachable; UUID keeps the no-overwrite guarantee even in
    // an extremely pathological project.
    return QStringLiteral("%1 #%2")
        .arg(baseName)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

bool ResultManager::CreateResultField(ccPointCloud* cloud,
                                      ResultComponent component,
                                      const CalculationOptions& options,
                                      ResultField& result)
{
    result = ResultField{};
    if (!cloud)
        return false;

    result.name = UniqueFieldName(cloud, BaseFieldName(component, options));
    result.index = cloud->addScalarField(qPrintable(result.name));
    if (result.index < 0)
    {
        result = ResultField{};
        return false;
    }

    result.resultId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cloud->setCurrentScalarField(result.index);
    return true;
}

bool ResultManager::CreateTemporaryField(ccPointCloud* cloud,
                                         ResultComponent component,
                                         const QString& runId,
                                         ResultField& result)
{
    result = ResultField{};
    if (!cloud)
        return false;

    QString shortRun = runId;
    shortRun.remove(QLatin1Char('-'));
    shortRun = shortRun.left(12);
    const QString base = QStringLiteral("__SOLIS_TMP_%1_%2")
                             .arg(ComponentName(component), shortRun);
    result.name = UniqueFieldName(cloud, base);
    result.index = cloud->addScalarField(qPrintable(result.name));
    if (result.index < 0)
    {
        result = ResultField{};
        return false;
    }

    cloud->setCurrentScalarField(result.index);
    return true;
}

void ResultManager::FinalizeField(ccPointCloud* cloud, int index)
{
    if (!cloud || index < 0)
        return;

    auto sf = cloud->getScalarField(index);
    if (sf)
        sf->computeMinAndMax();
    cloud->setCurrentDisplayedScalarField(index);
    cloud->showSF(true);
}

bool ResultManager::CreateTotalField(ccPointCloud* cloud,
                                     int directIndex,
                                     int diffuseIndex,
                                     const CalculationOptions& options,
                                     ResultField& totalResult)
{
    totalResult = ResultField{};
    if (!cloud || directIndex < 0 || diffuseIndex < 0)
        return false;

    auto directSF = cloud->getScalarField(directIndex);
    auto diffuseSF = cloud->getScalarField(diffuseIndex);
    if (!directSF || !diffuseSF)
        return false;

    if (!CreateResultField(cloud, ResultComponent::Total, options, totalResult))
        return false;

    auto totalSF = cloud->getScalarField(totalResult.index);
    if (!totalSF)
    {
        cloud->deleteScalarField(totalResult.index);
        totalResult = ResultField{};
        return false;
    }

    for (unsigned i = 0; i < cloud->size(); ++i)
    {
        const double direct = static_cast<double>(directSF->getValue(i));
        const double diffuse = static_cast<double>(diffuseSF->getValue(i));
        totalSF->setValue(i, static_cast<ScalarType>(std::max(0.0, direct + diffuse)));
    }

    FinalizeField(cloud, totalResult.index);
    return true;
}

void ResultManager::StoreMetadata(ccPointCloud* cloud,
                                  const ResultField& result,
                                  ResultComponent component,
                                  const CalculationOptions& options,
                                  const QString& runId,
                                  const QString& entityName,
                                  const BatchContext* batchContext)
{
    if (!cloud || !result.isValid() || result.resultId.isEmpty())
        return;

    QJsonObject record;
    record.insert(QStringLiteral("schemaVersion"), solis::ResultMetadataSchemaVersion);
    record.insert(QStringLiteral("plugin"), QStringLiteral("qSOLIS"));
    record.insert(QStringLiteral("pluginVersion"), QString::fromLatin1(solis::Version));
    record.insert(QStringLiteral("runId"), runId);
    record.insert(QStringLiteral("resultId"), result.resultId);
    record.insert(QStringLiteral("fieldName"), result.name);
    record.insert(QStringLiteral("component"), ComponentName(component));
    record.insert(QStringLiteral("unit"), SOLIS::OutputUnit(options));
    record.insert(QStringLiteral("entityName"), entityName);
    record.insert(QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    record.insert(QStringLiteral("options"), optionsJson(options));

    if (batchContext && !batchContext->batchId.isEmpty())
    {
        QJsonObject batch;
        batch.insert(QStringLiteral("batchId"), batchContext->batchId);
        batch.insert(QStringLiteral("index"), static_cast<int>(batchContext->index));
        batch.insert(QStringLiteral("count"), static_cast<int>(batchContext->count));
        batch.insert(QStringLiteral("startDayOfYear"), batchContext->startDayOfYear);
        batch.insert(QStringLiteral("endDayOfYear"), batchContext->endDayOfYear);
        batch.insert(QStringLiteral("stepMinutes"), batchContext->stepMinutes);
        record.insert(QStringLiteral("batch"), batch);
    }

    const QByteArray json = QJsonDocument(record).toJson(QJsonDocument::Compact);
    cloud->setMetaData(metadataKey(result.resultId), QString::fromUtf8(json));
    cloud->setMetaData(QStringLiteral("qSOLIS/schemaVersion"), solis::ResultMetadataSchemaVersion);
    cloud->setMetaData(QStringLiteral("qSOLIS/pluginVersion"), QString::fromLatin1(solis::Version));
    cloud->setMetaData(QStringLiteral("qSOLIS/lastRunId"), runId);
}


void ResultManager::StoreBatchMetadata(ccPointCloud* cloud,
                                       const BatchContext& batchContext,
                                       const QString& entityName)
{
    if (!cloud || batchContext.batchId.isEmpty())
        return;

    QJsonObject record;
    record.insert(QStringLiteral("schemaVersion"), solis::ResultMetadataSchemaVersion);
    record.insert(QStringLiteral("plugin"), QStringLiteral("qSOLIS"));
    record.insert(QStringLiteral("pluginVersion"), QString::fromLatin1(solis::Version));
    record.insert(QStringLiteral("batchId"), batchContext.batchId);
    record.insert(QStringLiteral("entityName"), entityName);
    record.insert(QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    record.insert(QStringLiteral("count"), static_cast<int>(batchContext.count));
    record.insert(QStringLiteral("startDayOfYear"), batchContext.startDayOfYear);
    record.insert(QStringLiteral("endDayOfYear"), batchContext.endDayOfYear);
    record.insert(QStringLiteral("stepMinutes"), batchContext.stepMinutes);

    const QByteArray json = QJsonDocument(record).toJson(QJsonDocument::Compact);
    cloud->setMetaData(QStringLiteral("qSOLIS/batch/%1").arg(batchContext.batchId), QString::fromUtf8(json));
    cloud->setMetaData(QStringLiteral("qSOLIS/pluginVersion"), QString::fromLatin1(solis::Version));
    cloud->setMetaData(QStringLiteral("qSOLIS/lastBatchId"), batchContext.batchId);
}


std::vector<ResultRecord> ResultManager::Results(const ccPointCloud* cloud)
{
    std::vector<ResultRecord> results;
    if (!cloud)
        return results;

    const QVariantMap& metadata = cloud->metaData();
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("qSOLIS/result/")))
            continue;

        ResultRecord record = parseResultRecord(cloud, it.key(), it.value());
        if (!record.resultId.isEmpty() && !record.fieldName.isEmpty())
            results.push_back(std::move(record));
    }

    std::sort(results.begin(), results.end(), [](const ResultRecord& a, const ResultRecord& b)
    {
        if (a.createdUtc != b.createdUtc)
            return a.createdUtc < b.createdUtc;
        return a.fieldName < b.fieldName;
    });
    return results;
}

std::vector<BatchRecord> ResultManager::Batches(const ccPointCloud* cloud)
{
    std::vector<BatchRecord> batches;
    if (!cloud)
        return batches;

    const std::vector<ResultRecord> results = Results(cloud);
    const QVariantMap& metadata = cloud->metaData();
    QSet<QString> seen;

    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("qSOLIS/batch/")))
            continue;
        BatchRecord record = parseBatchRecord(it.key(), it.value());
        if (record.batchId.isEmpty())
            continue;

        for (const ResultRecord& result : results)
        {
            if (result.batchId == record.batchId)
                ++record.storedResultCount;
        }
        batches.push_back(record);
        seen.insert(record.batchId);
    }

    // Be tolerant of old/incomplete projects where per-result metadata survived
    // but the batch summary record did not.
    for (const ResultRecord& result : results)
    {
        if (result.batchId.isEmpty() || seen.contains(result.batchId))
            continue;
        BatchRecord inferred;
        inferred.batchId = result.batchId;
        inferred.createdUtc = result.createdUtc;
        for (const ResultRecord& other : results)
        {
            if (other.batchId == inferred.batchId)
            {
                ++inferred.storedResultCount;
                if (inferred.createdUtc.isEmpty() || (!other.createdUtc.isEmpty() && other.createdUtc < inferred.createdUtc))
                    inferred.createdUtc = other.createdUtc;
            }
        }
        batches.push_back(inferred);
        seen.insert(inferred.batchId);
    }

    std::sort(batches.begin(), batches.end(), [](const BatchRecord& a, const BatchRecord& b)
    {
        if (a.createdUtc != b.createdUtc)
            return a.createdUtc > b.createdUtc;
        return a.batchId < b.batchId;
    });
    return batches;
}

QString ResultManager::LastRunId(const ccPointCloud* cloud)
{
    if (!cloud)
        return QString();

    const QString stored = cloud->getMetaData(QStringLiteral("qSOLIS/lastRunId")).toString();
    const std::vector<ResultRecord> results = Results(cloud);
    if (!stored.isEmpty())
    {
        for (const ResultRecord& result : results)
        {
            if (result.runId == stored)
                return stored;
        }
    }

    QString newestId;
    QString newestTime;
    for (const ResultRecord& result : results)
    {
        if (!result.runId.isEmpty() && result.createdUtc >= newestTime)
        {
            newestTime = result.createdUtc;
            newestId = result.runId;
        }
    }
    return newestId;
}

QString ResultManager::LastBatchId(const ccPointCloud* cloud)
{
    if (!cloud)
        return QString();

    const QString stored = cloud->getMetaData(QStringLiteral("qSOLIS/lastBatchId")).toString();
    const std::vector<BatchRecord> batches = Batches(cloud);
    if (!stored.isEmpty())
    {
        for (const BatchRecord& batch : batches)
        {
            if (batch.batchId == stored && batch.storedResultCount != 0)
                return stored;
        }
    }

    for (const BatchRecord& batch : batches)
    {
        if (batch.storedResultCount != 0)
            return batch.batchId;
    }
    return QString();
}

unsigned ResultManager::LegacyFieldCount(const ccPointCloud* cloud)
{
    if (!cloud)
        return 0;

    QSet<QString> managedNames;
    for (const ResultRecord& result : Results(cloud))
        managedNames.insert(result.fieldName);

    unsigned count = 0;
    const unsigned sfCount = cloud->getNumberOfScalarFields();
    for (unsigned i = 0; i < sfCount; ++i)
    {
        const QString name = QString::fromStdString(cloud->getScalarFieldName(i));
        if (name.startsWith(QStringLiteral("SOLIS ")) && !managedNames.contains(name))
            ++count;
    }
    return count;
}

CleanupStats ResultManager::RemoveRun(ccPointCloud* cloud, const QString& runId)
{
    if (runId.isEmpty())
        return CleanupStats{};
    return removeMatchingResults(cloud, [&runId](const ResultRecord& result)
    {
        return result.runId == runId;
    });
}

CleanupStats ResultManager::RemoveBatch(ccPointCloud* cloud, const QString& batchId)
{
    if (batchId.isEmpty())
        return CleanupStats{};
    CleanupStats stats = removeMatchingResults(cloud, [&batchId](const ResultRecord& result)
    {
        return result.batchId == batchId;
    });

    const QString key = QStringLiteral("qSOLIS/batch/%1").arg(batchId);
    if (cloud && cloud->removeMetaData(key))
        ++stats.metadataRecordsRemoved;
    refreshTrackingMetadata(cloud);
    return stats;
}

CleanupStats ResultManager::RemoveAll(ccPointCloud* cloud)
{
    CleanupStats stats = removeMatchingResults(cloud, [](const ResultRecord&) { return true; });
    if (!cloud)
        return stats;

    QStringList extraKeys;
    const QVariantMap& metadata = cloud->metaData();
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
    {
        if (it.key().startsWith(QStringLiteral("qSOLIS/batch/")))
            extraKeys.push_back(it.key());
    }
    extraKeys << QStringLiteral("qSOLIS/lastRunId")
              << QStringLiteral("qSOLIS/lastBatchId")
              << QStringLiteral("qSOLIS/schemaVersion")
              << QStringLiteral("qSOLIS/pluginVersion");

    for (const QString& key : extraKeys)
    {
        if (cloud->removeMetaData(key))
            ++stats.metadataRecordsRemoved;
    }
    return stats;
}

CleanupStats ResultManager::RemoveLegacyNamedFields(ccPointCloud* cloud)
{
    CleanupStats stats;
    if (!cloud)
        return stats;

    QSet<QString> managedNames;
    for (const ResultRecord& result : Results(cloud))
        managedNames.insert(result.fieldName);

    QStringList legacyNames;
    const unsigned sfCount = cloud->getNumberOfScalarFields();
    for (unsigned i = 0; i < sfCount; ++i)
    {
        const QString name = QString::fromStdString(cloud->getScalarFieldName(i));
        if (name.startsWith(QStringLiteral("SOLIS ")) && !managedNames.contains(name))
            legacyNames.push_back(name);
    }

    for (auto it = legacyNames.crbegin(); it != legacyNames.crend(); ++it)
    {
        const int index = cloud->getScalarFieldIndexByName(qPrintable(*it));
        if (index >= 0)
        {
            cloud->deleteScalarField(index);
            ++stats.fieldsRemoved;
            ++stats.legacyFieldsRemoved;
        }
    }
    return stats;
}

} // namespace solis
