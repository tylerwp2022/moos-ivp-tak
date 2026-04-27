/************************************************************/
/*    FILE: test_cot_geodesy.cpp                            */
/*    Quick standalone test for CoTGeodesy library.         */
/*                                                          */
/*    Build (from lib_cot_geodesy/ directory):              */
/*      g++ -std=c++11 -o test_cot_geodesy \               */
/*          test_cot_geodesy.cpp CoTGeodesy.cpp -lm         */
/*                                                          */
/*    Run:                                                  */
/*      ./test_cot_geodesy                                  */
/************************************************************/

#include <iostream>
#include <cmath>
#include "CoTGeodesy.h"

using namespace std;

// Helper: check if two doubles are within tolerance
bool near(double a, double b, double tol = 0.001) {
  return fabs(a - b) < tol;
}

int pass_count = 0;
int fail_count = 0;

void check(const string& name, bool condition) {
  if(condition) {
    cout << "  [PASS] " << name << endl;
    pass_count++;
  } else {
    cout << "  [FAIL] " << name << endl;
    fail_count++;
  }
}

int main()
{
  cout << "======================================" << endl;
  cout << "CoTGeodesy Test" << endl;
  cout << "Mode: " <<
#ifdef USE_MOOSGEODESY
    "MOOSGeodesy-UTM"
#else
    "NAV flat-earth fallback"
#endif
  << endl;
  cout << "======================================" << endl;

  // --------------------------------------------------------
  // Test 1: Not initialised — conversions should fail
  // --------------------------------------------------------
  cout << "\nTest 1: uninitialised state" << endl;
  {
    CoTGeodesy geo;
    double lat, lon, x, y;
    check("localXYToLatLon fails before init",
          !geo.localXYToLatLon(0, 0, lat, lon));
    check("latLonToLocalXY fails before init",
          !geo.latLonToLocalXY(0, 0, x, y));
    check("isInitialised() = false", !geo.isInitialised());
    check("isNavAnchorValid() = false", !geo.isNavAnchorValid());
  }

  // --------------------------------------------------------
  // Test 2: Initialise with Forest Lake origin (alpha.moos)
  //   LatOrigin  = 43.825300
  //   LongOrigin = -70.330400
  // --------------------------------------------------------
  cout << "\nTest 2: initialise with Forest Lake origin" << endl;
  {
    CoTGeodesy geo;
    bool ok = geo.initialise(43.825300, -70.330400);
    check("initialise() succeeds", ok);
    check("isInitialised() = true", geo.isInitialised());
    cout << "  mode: " << geo.getModeString() << endl;
  }

  // --------------------------------------------------------
  // Test 3: NAV fallback mode — roundtrip XY → LatLon → XY
  //
  // Set a NAV anchor at the origin (x=0, y=0 = lat/lon origin).
  // Then convert a known offset and check it roundtrips.
  // --------------------------------------------------------
  cout << "\nTest 3: NAV fallback roundtrip" << endl;
  {
    CoTGeodesy geo;
    geo.setNavFallback(true);
    geo.initialise(43.825300, -70.330400);

    // Anchor at origin: x=0,y=0 corresponds to lat/lon origin
    geo.updateNavAnchor(0.0, 0.0, 43.825300, -70.330400);
    check("isNavAnchorValid() = true", geo.isNavAnchorValid());

    // Known test point from alpha.moos polygon: x=60, y=-40
    double lat, lon;
    bool ok = geo.localXYToLatLon(60.0, -40.0, lat, lon);
    check("localXYToLatLon(60,-40) succeeds", ok);

    // Roundtrip back
    double x, y;
    bool ok2 = geo.latLonToLocalXY(lat, lon, x, y);
    check("latLonToLocalXY roundtrip succeeds", ok2);
    check("x roundtrips to 60.0 (within 0.01m)", near(x, 60.0, 0.01));
    check("y roundtrips to -40.0 (within 0.01m)", near(y, -40.0, 0.01));

    cout << "  x=60,y=-40 → lat=" << lat << " lon=" << lon << endl;
    cout << "  roundtrip → x=" << x << " y=" << y << endl;
  }

  // --------------------------------------------------------
  // Test 4: Origin point converts to anchor lat/lon
  // --------------------------------------------------------
  cout << "\nTest 4: origin converts correctly" << endl;
  {
    CoTGeodesy geo;
    geo.setNavFallback(true);
    geo.initialise(43.825300, -70.330400);
    geo.updateNavAnchor(0.0, 0.0, 43.825300, -70.330400);

    double lat, lon;
    geo.localXYToLatLon(0.0, 0.0, lat, lon);
    check("x=0,y=0 → lat≈43.8253", near(lat, 43.825300, 0.0001));
    check("x=0,y=0 → lon≈-70.3304", near(lon, -70.330400, 0.0001));
  }

  // --------------------------------------------------------
  // Test 5: NAV anchor offset (anchor not at origin)
  //
  // Simulate a vehicle that has moved to x=60, y=-160
  // (one of the polygon vertices) and set the anchor there.
  // --------------------------------------------------------
  cout << "\nTest 5: NAV anchor at non-origin position" << endl;
  {
    CoTGeodesy geo;
    geo.setNavFallback(true);
    geo.initialise(43.825300, -70.330400);

    // First get the lat/lon of x=60,y=-160 using origin anchor
    geo.updateNavAnchor(0.0, 0.0, 43.825300, -70.330400);
    double anchor_lat, anchor_lon;
    geo.localXYToLatLon(60.0, -160.0, anchor_lat, anchor_lon);

    // Now update the anchor to the vehicle's new position
    geo.updateNavAnchor(60.0, -160.0, anchor_lat, anchor_lon);

    // Convert x=150, y=-160 — a neighboring polygon vertex
    double lat, lon;
    geo.localXYToLatLon(150.0, -160.0, lat, lon);

    // Roundtrip
    double x, y;
    geo.latLonToLocalXY(lat, lon, x, y);
    check("x roundtrips to 150.0 from offset anchor", near(x, 150.0, 0.1));
    check("y roundtrips to -160.0 from offset anchor", near(y, -160.0, 0.1));
    cout << "  x=150,y=-160 → lat=" << lat << " lon=" << lon << endl;
    cout << "  roundtrip → x=" << x << " y=" << y << endl;
  }

  // --------------------------------------------------------
  // Summary
  // --------------------------------------------------------
  cout << "\n======================================" << endl;
  cout << "Results: " << pass_count << " passed, "
       << fail_count << " failed" << endl;
  cout << "======================================" << endl;

  return (fail_count > 0) ? 1 : 0;
}
