#include "SOLISCrownExport.h"

#include <ccPointCloud.h>

namespace solis
{
std::vector<CrownExposureExportRecord> CrownExposureExport::Records(const ccPointCloud* cloud)
{
    if (!cloud)
        return {};

    return RecordsFromMetadata(cloud->metaData(), cloud->getName(), cloud->getUniqueID());
}

bool CrownExposureExport::LatestRecord(const ccPointCloud* cloud, CrownExposureExportRecord& record)
{
    if (!cloud)
        return false;

    return LatestRecordFromMetadata(cloud->metaData(), record, cloud->getName(), cloud->getUniqueID());
}
}
