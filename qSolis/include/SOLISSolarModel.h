#pragma once

#include <CCGeom.h>

namespace solis
{
enum class TimeMode { UTC, LocalFixedOffset };

struct Site
{
    double latitudeDeg = 45.0;
    double longitudeDeg = 10.0;
    double elevationM = 0.0;
    double linkeTurbidity = 3.0;
    TimeMode timeMode = TimeMode::UTC;
    double utcOffsetHours = 0.0;
};

struct SolarPosition
{
    double altitudeDeg = -90.0;
    double azimuthDegFromNorth = 0.0;
    CCVector3d directionToScene{0.0, 0.0, -1.0};
};

struct ClearSkyIrradiance
{
    double dniWm2 = 0.0;
    double dhiWm2 = 0.0;
    double ghiWm2 = 0.0;
};

class SolarModel
{
public:
    static SolarPosition position(double dayOfYear, const Site& site);
    static double extraterrestrialNormalIrradiance(double dayOfYear);
    static ClearSkyIrradiance clearSky(double dayOfYear, const Site& site, const SolarPosition& pos);
};
}
