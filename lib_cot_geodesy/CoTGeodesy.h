/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGeodesy.h                                    */
/*    DATE: April 2026                                      */
/*                                                          */
/*  Shared geodesy utility for the pCoT* app family.        */
/*                                                          */
/*  Provides XY local-grid ↔ WGS84 lat/lon conversion for  */
/*  use by pCoTGraphics and pCoTCommander. Wraps            */
/*  CMOOSGeodesy (full UTM projection) when available, and  */
/*  falls back to a flat-earth approximation anchored at    */
/*  the vehicle's live NAV position otherwise.              */
/*                                                          */
/*  This is a synchronous C++ library — NOT a MOOS app.    */
/*  Apps that need geodesy link against lib_cot_geodesy     */
/*  and call it directly. No MOOSDB round-trip needed.      */
/*                                                          */
/*  Usage:                                                  */
/*    CoTGeodesy geo;                                       */
/*    geo.initialise(lat_origin, lon_origin);  // OnStartUp */
/*    geo.updateNavAnchor(x, y, lat, lon);     // OnNewMail */
/*    geo.localXYToLatLon(x, y, lat, lon);     // convert   */
/*    geo.latLonToLocalXY(lat, lon, x, y);     // convert   */
/************************************************************/

#ifndef COT_GEODESY_HEADER
#define COT_GEODESY_HEADER

#include <string>

#ifdef USE_MOOSGEODESY
  #include "MOOS/libMOOSGeodesy/MOOSGeodesy.h"
#endif

class CoTGeodesy
{
public:
  CoTGeodesy();
  ~CoTGeodesy() {}

  // --------------------------------------------------------
  // Initialisation
  //
  // Call initialise() in OnStartUp with LatOrigin/LongOrigin
  // from the .moos file. Required for MOOSGeodesy mode.
  //
  // Call updateNavAnchor() in OnNewMail whenever a NODE_REPORT
  // with X/Y/LAT/LON arrives. Required for NAV fallback mode,
  // and improves accuracy of flat-earth conversions.
  // --------------------------------------------------------

  // Initialise from mission origin (LatOrigin/LongOrigin in .moos).
  // Returns true if geodesy was set up successfully.
  // If USE_MOOSGEODESY is defined: initialises CMOOSGeodesy.
  // Otherwise: stores origin for flat-earth fallback.
  bool initialise(double lat_origin, double lon_origin);

  // Update the NAV anchor from a live vehicle position.
  // x, y: local grid meters. lat, lon: WGS84 degrees.
  // Called every NODE_REPORT. Used as fallback reference point
  // when MOOSGeodesy is unavailable or use_nav_fallback=true.
  void updateNavAnchor(double x, double y, double lat, double lon);

  // Force NAV fallback mode regardless of build flags.
  void setNavFallback(bool use_nav) { m_use_nav_fallback = use_nav; }

  // --------------------------------------------------------
  // Conversion
  //
  // Both functions return false and do nothing if the required
  // state is not yet available (not initialised, no NAV anchor).
  // --------------------------------------------------------

  // Local XY (meters from mission origin) → WGS84 lat/lon.
  // Mode 1 (MOOSGeodesy): CMOOSGeodesy::LocalGrid2LatLong(x, y, lat, lon)
  // Mode 2 (NAV fallback): flat-earth anchored at updateNavAnchor position
  bool localXYToLatLon(double x, double y,
                        double& lat, double& lon) const;

  // WGS84 lat/lon → local XY (meters from mission origin).
  // Mode 1 (MOOSGeodesy): CMOOSGeodesy::LatLong2LocalGrid(lat, lon, y, x)
  // Mode 2 (NAV fallback): flat-earth reverse anchored at NAV position
  bool latLonToLocalXY(double lat, double lon,
                        double& x, double& y) const;

  // --------------------------------------------------------
  // Status queries — for buildReport() in host apps
  // --------------------------------------------------------
  bool        isInitialised()   const { return m_initialised; }
  bool        isNavAnchorValid() const { return m_nav_anchor_valid; }
  bool        isUsingNavFallback() const { return m_use_nav_fallback; }
  std::string getModeString()   const;

private:
  // --------------------------------------------------------
  // MOOSGeodesy instance (Mode 1)
  // Only compiled in when USE_MOOSGEODESY is defined.
  // --------------------------------------------------------
#ifdef USE_MOOSGEODESY
  CMOOSGeodesy m_geodesy;
#endif

  bool   m_initialised;        // true once initialise() succeeds
  bool   m_use_nav_fallback;   // true = skip MOOSGeodesy, use flat-earth
  double m_lat_origin;         // mission LatOrigin
  double m_lon_origin;         // mission LongOrigin

  // --------------------------------------------------------
  // NAV anchor (Mode 2 — flat-earth fallback)
  // Holds the most recent vehicle position as a local reference.
  // Using the live position rather than the mission origin reduces
  // flat-earth error because the reference point is always nearby.
  // --------------------------------------------------------
  bool   m_nav_anchor_valid;
  double m_nav_x;              // most recent NAV_X (local grid, meters)
  double m_nav_y;              // most recent NAV_Y (local grid, meters)
  double m_nav_lat;            // most recent NAV_LAT (WGS84 degrees)
  double m_nav_lon;            // most recent NAV_LON (WGS84 degrees)
};

#endif // COT_GEODESY_HEADER
