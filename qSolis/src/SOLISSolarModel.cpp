#include "SOLISSolarModel.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double Pi = 3.1415926535897932384626433832795;
constexpr double SolarConstant = 1361.0;

double degToRad(double v) { return v * Pi / 180.0; }
double radToDeg(double v) { return v * 180.0 / Pi; }

double clampUnit(double v)
{
    return std::max(-1.0, std::min(1.0, v));
}

double wrapMinutes(double minutes)
{
    minutes = std::fmod(minutes, 1440.0);
    if (minutes < 0.0)
        minutes += 1440.0;
    return minutes;
}
} // namespace

namespace solis
{

SolarPosition SolarModel::position(double dayOfYear, const Site& site)
{
    SolarPosition result;

    const double localIntegerDay = std::floor(dayOfYear);
    const double localFractionalDay = dayOfYear - localIntegerDay;
    const double clockMinutes = localFractionalDay * 1440.0;

    // The fractional-year terms (declination and equation of time) describe the physical
    // instant, so they must be evaluated in UTC. The civil clock below remains local when
    // LocalFixedOffset is selected. Keeping these two notions separate makes, for example,
    // 12:00 UTC and 14:00 UTC+2 exactly equivalent without changing UTC-mode results.
    const double utcOffsetHours = (site.timeMode == TimeMode::UTC ? 0.0 : site.utcOffsetHours);
    const double utcDayOfYear = dayOfYear - utcOffsetHours / 24.0;
    const double utcIntegerDay = std::floor(utcDayOfYear);
    const double utcFractionalDay = utcDayOfYear - utcIntegerDay;
    const double utcClockHours = utcFractionalDay * 24.0;

    // NOAA fractional-year approximation. A 365-day year is deliberately used because the
    // public SOLIS interface is day-of-year based and does not carry a calendar year.
    const double gamma = 2.0 * Pi / 365.0 * (utcIntegerDay - 1.0 + (utcClockHours - 12.0) / 24.0);
    const double eqTime = 229.18 * (0.000075
                                   + 0.001868 * std::cos(gamma)
                                   - 0.032077 * std::sin(gamma)
                                   - 0.014615 * std::cos(2.0 * gamma)
                                   - 0.040849 * std::sin(2.0 * gamma));

    const double decl = 0.006918
                      - 0.399912 * std::cos(gamma)
                      + 0.070257 * std::sin(gamma)
                      - 0.006758 * std::cos(2.0 * gamma)
                      + 0.000907 * std::sin(2.0 * gamma)
                      - 0.002697 * std::cos(3.0 * gamma)
                      + 0.001480 * std::sin(3.0 * gamma);

    const double trueSolarMinutes = wrapMinutes(clockMinutes + eqTime + 4.0 * site.longitudeDeg - 60.0 * utcOffsetHours);
    double hourAngleDeg = trueSolarMinutes / 4.0 - 180.0;
    if (hourAngleDeg < -180.0)
        hourAngleDeg += 360.0;

    const double latitude = degToRad(site.latitudeDeg);
    const double hourAngle = degToRad(hourAngleDeg);
    const double cosZenith = clampUnit(std::sin(latitude) * std::sin(decl)
                                     + std::cos(latitude) * std::cos(decl) * std::cos(hourAngle));
    const double zenith = std::acos(cosZenith);
    result.altitudeDeg = 90.0 - radToDeg(zenith);

    if (result.altitudeDeg <= -90.0)
        return result;

    // atan2 form gives azimuth clockwise from north and is stable near the zenith.
    const double azimuth = std::atan2(std::sin(hourAngle),
                                      std::cos(hourAngle) * std::sin(latitude) - std::tan(decl) * std::cos(latitude));
    result.azimuthDegFromNorth = std::fmod(radToDeg(azimuth) + 180.0 + 360.0, 360.0);

    const double alt = degToRad(result.altitudeDeg);
    const double az = degToRad(result.azimuthDegFromNorth);

    // +X east, +Y north, +Z up. directionToScene points from the sun toward the scene.
    const CCVector3d surfaceToSun(std::cos(alt) * std::sin(az),
                                  std::cos(alt) * std::cos(az),
                                  std::sin(alt));
    result.directionToScene = -surfaceToSun;
    return result;
}

double SolarModel::extraterrestrialNormalIrradiance(double dayOfYear)
{
    const double b = 2.0 * Pi * (dayOfYear - 1.0) / 365.0;
    const double eccentricity = 1.00011
                              + 0.034221 * std::cos(b)
                              + 0.001280 * std::sin(b)
                              + 0.000719 * std::cos(2.0 * b)
                              + 0.000077 * std::sin(2.0 * b);
    return SolarConstant * eccentricity;
}

ClearSkyIrradiance SolarModel::clearSky(double dayOfYear, const Site& site, const SolarPosition& pos)
{
    ClearSkyIrradiance result;
    if (pos.altitudeDeg <= 0.0)
        return result;

    const double zenithDeg = 90.0 - pos.altitudeDeg;
    const double cosZenith = std::max(0.0, std::cos(degToRad(zenithDeg)));
    if (cosZenith <= 0.0 || zenithDeg >= 90.0)
        return result;

    // Kasten-Young relative optical air mass.
    const double relativeAirMass = 1.0 / (cosZenith + 0.50572 * std::pow(96.07995 - zenithDeg, -1.6364));
    // Standard-atmosphere pressure approximation, making air mass absolute.
    const double pressureRatio = std::exp(-site.elevationM / 8434.5);
    const double airMass = relativeAirMass * pressureRatio;

    const double tl = std::max(1.0, site.linkeTurbidity);
    const double fh1 = std::exp(-site.elevationM / 8000.0);
    const double fh2 = std::exp(-site.elevationM / 1250.0);
    const double cg1 = 5.09e-5 * site.elevationM + 0.868;
    const double cg2 = 3.92e-5 * site.elevationM + 0.0387;
    // Extraterrestrial irradiance also depends on the physical instant rather than the
    // displayed local clock. This keeps clear-sky results invariant under UTC/local
    // representations of the same instant.
    const double utcOffsetHours = (site.timeMode == TimeMode::UTC ? 0.0 : site.utcOffsetHours);
    const double dniExtra = extraterrestrialNormalIrradiance(dayOfYear - utcOffsetHours / 24.0);

    // Ineichen/Perez clear-sky GHI and DNI approximation.
    result.ghiWm2 = std::max(0.0,
                            cg1 * dniExtra * cosZenith
                            * std::exp(-cg2 * airMass * (fh1 + fh2 * (tl - 1.0))));

    const double b = 0.664 + 0.163 / std::max(1.0e-6, fh1);
    const double bnci = b * dniExtra * std::exp(-0.09 * airMass * (tl - 1.0));
    const double correctionDenom = 0.1 + 0.882 / std::max(1.0e-6, fh1);
    const double bnci2 = result.ghiWm2
                       * (1.0 - (0.1 - 0.2 * std::exp(-tl)) / correctionDenom)
                       / std::max(1.0e-6, cosZenith);
    result.dniWm2 = std::max(0.0, std::min(bnci, bnci2));
    result.dhiWm2 = std::max(0.0, result.ghiWm2 - result.dniWm2 * cosZenith);
    return result;
}

} // namespace solis
