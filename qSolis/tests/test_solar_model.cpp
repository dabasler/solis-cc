#include "SOLISSolarModel.h"

#include <cmath>
#include <iostream>

namespace
{
bool near(double actual, double expected, double tolerance, const char* label)
{
    const bool ok = std::abs(actual - expected) <= tolerance;
    if (!ok)
        std::cerr << label << ": actual=" << actual << " expected=" << expected << " tolerance=" << tolerance << '\n';
    return ok;
}
}

int main()
{
    using namespace solis;

    Site site;
    site.latitudeDeg = 45.0;
    site.longitudeDeg = 0.0;
    site.elevationM = 0.0;
    site.linkeTurbidity = 3.0;
    site.timeMode = TimeMode::UTC;

    bool ok = true;

    const SolarPosition summer = SolarModel::position(172.5, site);
    const ClearSkyIrradiance summerSky = SolarModel::clearSky(172.5, site, summer);
    ok &= near(summer.altitudeDeg, 68.45035, 0.02, "summer altitude");
    ok &= near(summer.azimuthDegFromNorth, 179.17062, 0.05, "summer azimuth");
    ok &= near(summerSky.dniWm2, 897.3311, 0.5, "summer DNI");

    const SolarPosition winter = SolarModel::position(355.5, site);
    const ClearSkyIrradiance winterSky = SolarModel::clearSky(355.5, site, winter);
    ok &= near(winter.altitudeDeg, 21.5783, 0.02, "winter altitude");
    ok &= near(winterSky.dniWm2, 715.6085, 0.5, "winter DNI");

    // 14:00 at UTC+2 represents the same instant as 12:00 UTC.
    Site localSite = site;
    localSite.timeMode = TimeMode::LocalFixedOffset;
    localSite.utcOffsetHours = 2.0;
    const SolarPosition localEquivalent = SolarModel::position(172.0 + 14.0 / 24.0, localSite);
    ok &= near(localEquivalent.altitudeDeg, summer.altitudeDeg, 1.0e-8, "UTC/local altitude equivalence");
    ok &= near(localEquivalent.azimuthDegFromNorth, summer.azimuthDegFromNorth, 1.0e-8, "UTC/local azimuth equivalence");

    const SolarPosition night = SolarModel::position(172.0, site);
    const ClearSkyIrradiance nightSky = SolarModel::clearSky(172.0, site, night);
    if (night.altitudeDeg > 0.0 || nightSky.dniWm2 != 0.0 || nightSky.dhiWm2 != 0.0 || nightSky.ghiWm2 != 0.0)
    {
        std::cerr << "night clear-sky test failed\n";
        ok = false;
    }

    return ok ? 0 : 1;
}
