/** @file solrad.h
 * 
 * @brief A C implemantation of the 'solrad' R package functions.
 * These functions are meant be used in surface energy models and estimation of solar positions and 
 * components with varying topography, time and locations. The functions calculate solar top-of-atmosphere,
 * open, diffuse and direct components, atmospheric transmittance and diffuse factors, day length, sunrise
 * and sunset, solar azimuth, zenith, altitude, incidence, and hour angles, earth declination angle, equation
 * of time, and solar constant
 *
 * @par       
 * 2020  David Basler , based on 'solrad' by Bijan Seyednasrollah https://github.com/bnasr/solrad
 */ 
 
#ifndef SOLRAD_H_
#define SOLRAD_H_

#include <stdio.h>
#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
// CONSTANTS
#define SOLAR_CONSTANT 1366.1

// Day of year
//double DayOfYear ( char * DateTime);
// Declination Angle
double Declination(double doy);
// Solar Hour Angle
double HourAngle(double doy, double lon, double slon, double ds);
// Apparent Solar Time
double AST (double doy, double lon, double slon, double ds);
// Equation of time
double EOT (double doy);
// Local Standard Time
double LST (double doy);
// Solar Altitude Angle
double Altitude (double doy, double lat, double lon, double slon, double ds);
// Sunset Time
double Sunset (double doy, double lat);
// Sunrise Time
double Sunrise (double doy, double lat);
// Day Length
double DayLength (double doy, double lat);
// Solar Azimuth Angle
double Azimuth (double doy, double lat, double lon, double slon, double ds);
// Solar Incidence Angle
double Incidence (double doy, double lat, double lon, double slon, double ds, double slope, double aspect);
// Atmospheric Transmittance
double Transmittance (double doy, double  lat, double lon, double slon, double ds, double elevation);
// Atmospheric Diffusion Factor
double DiffusionFactor (double doy, double lat,double lon, double slon, double ds, double elevation);
// Solar Extraterrestrial Radiation
double Extraterrestrial (double doy);
// Normal Extraterrestrial Solar Radiation
double ExtraterrestrialNormal (double doy, double lat, double lon, double slon, double ds, double slope, double aspect);
// Open Sky Solar Radiation
double OpenRadiation (double doy, double lat, double lon, double slon, double ds, double elevation);
// Solar Direct Beam Radiation on Surface
double DirectRadiation (double doy, double lat, double lon, double slon, double ds, double elevation, double slope, double aspect);
// Solar Diffuse Radiation on a Surface
double DiffuseRadiation (double doy, double lat, double lon, double slon, double ds, double elevation, double slope);

#endif /* SOLRAD_H_ */

/*** end of file ***/