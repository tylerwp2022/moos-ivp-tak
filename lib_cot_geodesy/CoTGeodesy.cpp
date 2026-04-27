/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGeodesy.cpp                                  */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cmath>    // cos, M_PI
#include "CoTGeodesy.h"

// ============================================================
// Constructor
// ============================================================

CoTGeodesy::CoTGeodesy()
{
  m_initialised       = false;
  m_use_nav_fallback  = false;
  m_lat_origin        = 0.0;
  m_lon_origin        = 0.0;

  m_nav_anchor_valid  = false;
  m_nav_x             = 0.0;
  m_nav_y             = 0.0;
  m_nav_lat           = 0.0;
  m_nav_lon           = 0.0;
}


// ============================================================
// initialise()
//
// Sets up the geodesy converter with the mission origin.
// Must be called in OnStartUp() with LatOrigin/LongOrigin
// from the .moos file before any conversions are attempted.
// ============================================================

bool CoTGeodesy::initialise(double lat_origin, double lon_origin)
{
  m_lat_origin = lat_origin;
  m_lon_origin = lon_origin;

#ifdef USE_MOOSGEODESY
  if(!m_use_nav_fallback) {
    if(!m_geodesy.Initialise(lat_origin, lon_origin))
      return false;
  }
#endif

  m_initialised = true;
  return true;
}


// ============================================================
// updateNavAnchor()
//
// Updates the flat-earth reference point with the vehicle's
// most recent simultaneously-known XY + LatLon position.
//
// Call this every time a NODE_REPORT with X/Y/LAT/LON arrives
// in OnNewMail(). The NAV anchor is used in two ways:
//
//   1. As the primary conversion method when m_use_nav_fallback=true
//      or USE_MOOSGEODESY is not defined.
//
//   2. As a validity check — latLonToLocalXY() requires a valid
//      anchor even in MOOSGeodesy mode when the origin hasn't
//      been set from the mission file.
// ============================================================

void CoTGeodesy::updateNavAnchor(double x, double y,
                                  double lat, double lon)
{
  m_nav_x            = x;
  m_nav_y            = y;
  m_nav_lat          = lat;
  m_nav_lon          = lon;
  m_nav_anchor_valid = true;
}


// ============================================================
// localXYToLatLon()
//
// Converts local XY (meters from mission origin) to WGS84.
//
// Mode 1 — CMOOSGeodesy (USE_MOOSGEODESY defined, no fallback):
//   Full UTM projection. Call signature:
//   LocalGrid2LatLong(East/X, North/Y, &lat, &lon)
//
// Mode 2 — flat-earth NAV fallback:
//   Anchored at the most recent NAV position.
//   delta_x = x - nav_x  (meters East of anchor)
//   delta_y = y - nav_y  (meters North of anchor)
//   lat = nav_lat + delta_y / 111111
//   lon = nav_lon + delta_x / (111111 * cos(nav_lat_rad))
//
//   Accurate to <1m at Aquaticus field scales (~200m).
//
// Returns false if required state is not available.
// ============================================================

bool CoTGeodesy::localXYToLatLon(double x, double y,
                                  double& lat, double& lon) const
{
#ifdef USE_MOOSGEODESY
  if(!m_use_nav_fallback) {
    if(!m_initialised)
      return false;
    // CMOOSGeodesy::LocalGrid2LatLong takes (East/X, North/Y, &lat, &lon)
    const_cast<CMOOSGeodesy&>(m_geodesy).LocalGrid2LatLong(x, y, lat, lon);
    return true;
  }
#endif

  // Flat-earth NAV fallback
  if(!m_nav_anchor_valid)
    return false;

  const double METERS_PER_DEG_LAT = 111111.0;
  const double nav_lat_rad        = m_nav_lat * M_PI / 180.0;
  const double meters_per_deg_lon = METERS_PER_DEG_LAT * cos(nav_lat_rad);

  lat = m_nav_lat + (y - m_nav_y) / METERS_PER_DEG_LAT;
  lon = m_nav_lon + (x - m_nav_x) / meters_per_deg_lon;
  return true;
}


// ============================================================
// latLonToLocalXY()
//
// Converts WGS84 lat/lon to local XY (meters from origin).
// Reverse of localXYToLatLon().
//
// Mode 1 — CMOOSGeodesy:
//   LatLong2LocalGrid(lat, lon, &north/Y, &east/X)
//   Note the argument order: north first, then east.
//
// Mode 2 — flat-earth NAV fallback:
//   x = nav_x + (lon - nav_lon) * meters_per_deg_lon
//   y = nav_y + (lat - nav_lat) * 111111
// ============================================================

bool CoTGeodesy::latLonToLocalXY(double lat, double lon,
                                  double& x, double& y) const
{
#ifdef USE_MOOSGEODESY
  if(!m_use_nav_fallback) {
    if(!m_initialised)
      return false;
    double north = 0.0, east = 0.0;
    // LatLong2LocalGrid returns north (Y) first, then east (X)
    const_cast<CMOOSGeodesy&>(m_geodesy).LatLong2LocalGrid(lat, lon,
                                                            north, east);
    x = east;
    y = north;
    return true;
  }
#endif

  // Flat-earth NAV fallback
  if(!m_nav_anchor_valid)
    return false;

  const double METERS_PER_DEG_LAT = 111111.0;
  const double nav_lat_rad        = m_nav_lat * M_PI / 180.0;
  const double meters_per_deg_lon = METERS_PER_DEG_LAT * cos(nav_lat_rad);

  x = m_nav_x + (lon - m_nav_lon) * meters_per_deg_lon;
  y = m_nav_y + (lat - m_nav_lat) * METERS_PER_DEG_LAT;
  return true;
}


// ============================================================
// getModeString()
//
// Returns a human-readable string describing the active mode.
// Used by host app buildReport() to show geodesy status.
// ============================================================

std::string CoTGeodesy::getModeString() const
{
#ifdef USE_MOOSGEODESY
  if(!m_use_nav_fallback)
    return m_initialised ? "MOOSGeodesy-UTM" : "MOOSGeodesy-UNINIT";
#endif
  return m_nav_anchor_valid ? "NAV-fallback" : "NAV-fallback(no anchor)";
}
