/** @file solrad.c
 * 
 * @brief A C implemantation of the 'solrad' R package functions. see solrad.h for more a detailed description
 *
 * @par       
 * 2020  David Basler , based on 'solrad' R package <https://github.com/bnasr/solrad> by Bijan Seyednasrollah 
*/ 

 
#include <stdio.h>
#include <math.h>

#include "solrad.h"

/*! Day of year
 * @brief * @brief This function returns a continuous the day of year value (as double value 1:365) for a given date-time 'YYYY-mm-dd HH:MM:SS' format.
 * @param[ch] DateTime string
 * @return day of year with time fraction
*/
/*
double DayOfYear ( char * DateTime){
   char buff[6];
   //YEAR
   memcpy(buff, &buff[0], 4 );
   buff[4] = '\0';
   int year=atoi(buff);
   //MONTH
   memcpy(buff, &buff[5], 2 );
   buff[2] = '\0';
   int month=atoi(buff);
   //DAY
   memcpy(buff, &buff[8], 2 );
   buff[2] = '\0';
   int day=atoi(buff);
   //HOUR
   memcpy(buff, &buff[11], 2 );
   buff[2] = '\0';
   double hours=atof(buff);
   //MIN
   memcpy(buff, &buff[14], 2 );
   buff[2] = '\0';
   double mins=atof(buff);
   //SEC
   memcpy(buff, &buff[17], 2 );
   buff[2] = '\0';
   double secs=atof(buff);

   static const int days[2][13] = {
        {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
   int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
   return( days[leap][month] + day + hours/24 + mins/24/60 + secs/24/60/60 );   
}
*/

/*! Declination Angle
 * @brief This function calculates solar declination angle for a given day of year.
 * @param doy Day of year
*/
double Declination(double doy){
  return (23.45*sin(M_PI/180.0*360/365 * (284.0 + doy)));
}

/*! Solar Hour Angle
 * @brief This function returns solar hour angle for a given day of year, and location.
 * @param doy Day of year
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
*/
double HourAngle (double doy, double lon, double slon, double ds){
  double ast = AST(doy, lon, slon, ds);
  return((ast - 12*60.0)/4.0);
}

/*! Apparent Solar Time
 * @brief This function returns the apparent solar time (in minutes) for a given day of year and location.
 * @param doy Day of year
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
*/
double AST (double doy, double lon, double slon, double ds){
  double eot = EOT(doy);
  double lst = LST(doy);
  return (lst + eot + 4* (slon - lon) - ds);
}

/*! Equation of time
 * @brief This function approximates the value of equation of time for a given day of year
 * @param doy Day of year
*/
double EOT (double doy){
  double b = (doy - 81)*360/365.0;
  return (9.87*sin(M_PI/180.0*2*b)- 7.53*cos(M_PI/180.0*b)-1.5*sin(M_PI/180.0*b));
}

/*! Local Standard Time
 * @brief This function returns local standard time (in minutes) given a day of the year value.
 * @param doy Day of year
*/
double LST (double doy){
  return( fmod(doy*24*60, (24*60)));
}

/*! Solar Altitude Angle
 * @brief This function solar altitude angle (in degrees) for a given day of year and location.
 * @param doy Day of year
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 */
double Altitude (double doy, double lat, double lon, double slon, double ds){
  double delta = Declination(doy);
  double h  = HourAngle(doy, lon, slon, ds);
  return(180.0/M_PI*asin(sin(M_PI/180.0*lat)*sin(M_PI/180.0*delta)+cos(M_PI/180.0*lat)*cos(M_PI/180.0*delta)*cos(M_PI/180.0*h)));
}

/*! Sunset Time
 * @brief This function estimates sunset time (in continuous hour values) for a given day of year and latitude.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
*/
double Sunset (double doy, double lat){
  double delta = Declination(doy);
  return(180.0/M_PI*acos(-tan(M_PI/180.0*lat)*tan(M_PI/180.0*delta))/15.0);
}

/*! Sunrise Time
 * @brief This function estimates sunrise time (in continuous hour values) for a given day of year and latitude.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
*/
double Sunrise (double doy, double lat){
  double delta = Declination(doy);
  return(-180.0/M_PI*acos(-tan(M_PI/180.0*lat)* tan(M_PI/180.0*delta))/15.0);
}

/*! Day Length
 * @brief This function estimates day length (in hours) for a given day of year and latitude.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
*/
double DayLength (double doy, double lat){
  return( Sunset(doy, lat) - Sunrise(doy, lat));
}

/*! Solar Azimuth Angle
 * @brief This function returns solar azimuth angle (in degrees) for a given day of year and location. The solar azimuth angle is the angle of sun's ray measured in the horizental plane from due south
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
*/
double Azimuth (double doy, double lat, double lon, double slon, double ds){
  double delta    = Declination(doy);
  double h        = HourAngle(doy, lon, slon, ds);
  double ast      = AST(doy, lon, slon, ds);
  double alpha    = Altitude(doy, lat, lon, slon, ds);
  double rhs      = cos(M_PI/180.0*delta)* sin(M_PI/180.0*h)/cos(M_PI/180.0*alpha);
  double azimuth1 = 180.0/M_PI*asin(rhs);
  double c1       = (ast < 12*60)*1;
  double c2       = (cos(M_PI/180.0*h)> tan(M_PI/180.0*delta)/tan(M_PI/180.0*lat))*1;
  double azimuth2 = c1*(-180.0 + fabs(azimuth1)) + (1.0-c1)*(180.0 - azimuth1);
  return (c2*azimuth1 + (1-c2)*azimuth2);
}

/*! Solar Incidence Angle
 * @brief This function returns solar incidence angle (in degrees) for a given day of year and location and site slope and aspect. The solar incidence angle is the angle between sun's ray and the normal on a surface.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param slope Site slope in degrees
 * @param aspect Site aspect with respect to the south in degrees
*/
double Incidence (double doy, double lat, double lon, double slon, double ds, double slope, double aspect){
  double delta = Declination(doy);
  double h  =   HourAngle(doy, lon, slon, ds);
  double theta = 180.0/M_PI*acos(sin(M_PI/180.0*lat)*sin(M_PI/180.0*delta)*cos(M_PI/180.0*slope) -
                        cos(M_PI/180.0*lat)*sin(M_PI/180.0*delta)* sin(M_PI/180.0*slope)* cos(M_PI/180.0*aspect) +
                        cos(M_PI/180.0*lat)*cos(M_PI/180.0*delta)* cos(M_PI/180.0*h)*cos(M_PI/180.0*slope)+
                        sin(M_PI/180.0*lat)*cos(M_PI/180.0*delta)* cos(M_PI/180.0*h)*sin(M_PI/180.0*slope)*cos(M_PI/180.0*aspect)+
                        cos(M_PI/180.0*delta)*sin(M_PI/180.0*h)*sin(M_PI/180.0*slope)*sin(M_PI/180.0*aspect));
  return(theta);
}

/*! Atmospheric Transmittance
 * @brief This function returns atmospheric transmittance coefficient for a given day of year and location.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
*/
double Transmittance (double doy, double  lat, double lon, double slon, double ds, double elevation){
  double a0     = 0.4237-0.00821* pow((6-elevation/1000.0),2);
  double a1     = 0.5055+0.00595*pow((6.5-elevation/1000.0),2);
  double k      = 0.2711+0.01858*pow((2.5-elevation/1000.0),2);
  double alpha  = Altitude(doy, lat, lon, slon, ds);
  return((a0+a1*exp(-k/sin(M_PI/180.0*alpha)))*(alpha>0));
}

/*! Atmospheric Diffusion Factor
 * @brief This function returns atmospheric diffusion factor for a given day of year, location and topography.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
*/
double DiffusionFactor (double doy, double lat,double lon, double slon, double ds, double elevation){
  double tb =  Transmittance(doy, lat, lon, slon, ds, elevation);
  return( 0.271-0.294*tb );
}

/*! Solar Extraterrestrial Radiation
 * @brief This function calculates solar extraterrestrial radiation (in W/m2) for a given day of year.
 * @param doy Day of year
*/
double Extraterrestrial (double doy){
  return (SOLAR_CONSTANT*(1+0.033*cos(M_PI/180.0*360*doy/365)));
}

/*! Normal Extraterrestrial Solar Radiation
 * @brief This function calculates extraterrestrial solar radiation normal to surface (in W/m2) for a given day of year, location and topogrpahy.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param slope Site slope in degrees
 * @param aspect Site aspect with respect to the south in degrees
*/
double ExtraterrestrialNormal (double doy, double lat, double lon, double slon, double ds, double slope, double aspect){
  double theta  =   Incidence(doy, lat, lon, slon, ds, slope, aspect);
  double sextr =  Extraterrestrial(doy);
  double sextrNormal =  sextr*cos(M_PI/180.0*theta);
  if (sextrNormal<0) return (0);
  return (sextrNormal);
}

/*! Open Sky Solar Radiation
 * @brief This function returns open sky solar radiation (in W/m2) for a given day of year and location.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
*/
double OpenRadiation (double doy, double lat, double lon, double slon, double ds, double elevation){
  double tb =  Transmittance(doy, lat, lon, slon, ds, elevation);
  double sextr =  Extraterrestrial(doy);
  return( tb*sextr );
}

/*! Solar Direct Beam Radiation on Surface
* @brief This function returns solar open direct beam dadiation (in W/m2) for a given day of year, location and topography.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
 * @param slope Site slope in degrees
 * @param aspect Site aspect with respect to the south in degrees
*/
double DirectRadiation (double doy, double lat, double lon, double slon, double ds, double elevation, double slope, double aspect){
  double theta  =   Incidence(doy, lat, lon, slon, ds, slope, aspect);
  double sopen =  OpenRadiation(doy, lat, lon, slon, ds, elevation);
  return( sopen*cos(M_PI/180.0*theta) );
}

/*! Solar Diffuse Radiation on a Surface
* @brief This function returns solar diffuse dadiation (in W/m2) for a given day of year, location and topography.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
 * @param slope Site slope in degrees
*/
double DiffuseRadiation (double doy, double lat, double lon, double slon, double ds, double elevation, double slope){
  double alpha  = Altitude(doy, lat, lon, slon, ds);
  double sopen =  OpenRadiation(doy, lat, lon, slon, ds, elevation);
  double td =  DiffusionFactor(doy, lat, lon, slon, ds, elevation);
  return( sopen*td*pow(sin(M_PI/180.0*alpha),2)*pow(cos(M_PI/180.0*slope/2.),2) );
}

/*! Calculating Solar Variables
* @brief This function calculates solar variables including radiation components, solar angles and positions and day length.
 * @param doy Day of year
 * @param lat Latitude (in degrees)
 * @param lon Longitude in degrees
 * @param slon Standard longitude (based on time zone) in degrees
 * @param ds Daylight saving in minutes
 * @param elevation elevation of the site in meters
 * @param slope Site slope in degrees
 * @param aspect Site aspect with respect to the south in degrees
*/
/*
void Solar (double doy, double lat, double lon, double slon, double ds, double elevation, double slope, double aspect){
  // calculate solar radiation and related variables based on location, time and topographical conditions
  doy          = (doy+365)%365;
  *declination = Declination(doy);
  *h          = HourAngle(doy, lon, slon, ds);
  *altitude    = Altitude(doy, lat, lon, slon, ds);
  *sunset      = Sunset(doy, lat);
  *sunrise     = Sunrise(doy, lat);
  *dl          = DayLength(doy, lat);
  *azimuth     = Azimuth(doy, lat, lon, slon, ds);
  *incidence   = Incidence(doy, lat, lon, slon, ds, slope, aspect);
  *tb          = Transmittance(doy, lat, lon, slon, ds, elevation);
  *td          = DiffusionFactor(doy, lat, lon, slon, ds, elevation);
  *sextr       = Extraterrestrial(doy);
  *sextrNormal = ExtraterrestrialNormal(doy, lat, lon, slon, ds, slope, aspect);
  *sopen       = OpenRadiation(doy, lat, lon, slon, ds, elevation);
  *sdiropen    = DirectRadiation(doy, lat, lon, slon, ds, elevation, slope, aspect);
  *sdifopen    = DiffuseRadiation(doy, lat, lon, slon, ds, elevation, slope);
}
*/