/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGraphics.cpp                                 */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "CoTGraphics.h"
#include "MBUtils.h"

using namespace std;

// CoT 'how' attribute values confirmed from live ATAK captures:


static const char* HOW_MG = "m-g";

// ATAK ARGB values for fully-opaque team colors (confirmed from live captures)
static const int ARGB_RED    = -65536;     // 0xFFFF0000
static const int ARGB_BLUE   = -16776961;  // 0xFF0000FF
static const int ARGB_YELLOW = -256;       // 0xFFFFFF00
static const int ARGB_WHITE  = -1;         // 0xFFFFFFFF


// ============================================================
// Constructor
// ============================================================

CoTGraphics::CoTGraphics()
{
  m_geodesy_initialized      = false;
  m_publish_view_points      = true;
  m_publish_view_seglists    = true;
  m_publish_view_polygons    = true;
  m_publish_view_circles     = true;
  m_publish_flag_markers     = true;
  m_publish_score_label      = true;
  m_immediate_view_points    = true;
  m_stationary_send_interval = 3.0;
  m_cot_stale_offset         = 86400.0;
  m_shoreside_mode           = false;
  m_debug                    = false;

  m_vp_cot_sent     = 0;
  m_vsl_cot_sent    = 0;
  m_poly_cot_sent   = 0;
  m_circle_cot_sent = 0;
  m_flag_cot_sent   = 0;
  m_text_cot_sent   = 0;
  m_delete_cot_sent = 0;
}


// ============================================================
// debugLog()
// ============================================================

void CoTGraphics::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTGraphics::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
// ============================================================

bool CoTGraphics::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug flag first so subsequent config logging works
  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

  bool use_nav_fallback = false;

  // Pass 2: all params
  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  for(auto& orig : sParams) {
    string line  = tolower(orig);
    string param = biteStringX(line, '=');
    string value = line;
    bool handled = true;

    if     (param == "debug")
      setBooleanOnString(m_debug, value);
    else if(param == "publish_view_points")
      setBooleanOnString(m_publish_view_points, value);
    else if(param == "publish_view_seglists")
      setBooleanOnString(m_publish_view_seglists, value);
    else if(param == "publish_view_polygons")
      setBooleanOnString(m_publish_view_polygons, value);
    else if(param == "publish_view_circles")
      setBooleanOnString(m_publish_view_circles, value);
    else if(param == "publish_flag_markers")
      setBooleanOnString(m_publish_flag_markers, value);
    else if(param == "publish_score_label")
      setBooleanOnString(m_publish_score_label, value);
    else if(param == "immediate_view_points")
      setBooleanOnString(m_immediate_view_points, value);
    else if(param == "stationary_send_interval")
      m_stationary_send_interval = atof(value.c_str());
    else if(param == "use_nav_fallback")
      setBooleanOnString(use_nav_fallback, value);
    else if(param == "shoreside") {
      setBooleanOnString(m_shoreside_mode, value);
      debugLog("Config: shoreside = " + boolToString(m_shoreside_mode));
    }
    else if(param == "vehicle_names") {
      // Colon-separated vehicle name list — matches $(VNAMES) from
      // launch_shoreside.sh: red_one:red_two:...:blue_one:blue_two:...
      // Any VIEW_* label containing a vehicle name is blocked when
      // shoreside=true.
      string v = orig; biteStringX(v, '=');
      for(auto& tok : parseString(stripBlankEnds(v), ':')) {
        string name = stripBlankEnds(tok);
        if(!name.empty()) m_vehicle_names.insert(name);
      }
      debugLog("Config: vehicle_names — " +
               intToString((int)m_vehicle_names.size()) + " vehicles");
    }
    else if(param == "label_block_contains") {
      // Legacy fallback: comma-separated substring patterns.
      // Prefer shoreside=true + vehicle_names instead.
      for(auto& tok : parseString(value, ',')) {
        string s = stripBlankEnds(tok);
        if(!s.empty()) m_label_block_contains.push_back(s);
      }
      debugLog("Config: label_block_contains — " +
               intToString((int)m_label_block_contains.size()) + " patterns");
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Initialize geodesy from LatOrigin/LongOrigin in mission file
  m_geodesy.setNavFallback(use_nav_fallback);
  double lat_origin = 0.0, lon_origin = 0.0;
  bool got_lat = m_MissionReader.GetValue("LatOrigin",  lat_origin);
  bool got_lon = m_MissionReader.GetValue("LongOrigin", lon_origin);

  if(got_lat && got_lon) {
    if(m_geodesy.initialise(lat_origin, lon_origin)) {
      m_geodesy_initialized = true;
      debugLog("OnStartUp: geodesy initialized [" +
               m_geodesy.getModeString() + "]");
    }
    else
      reportRunWarning("pCoTGraphics: geodesy initialisation failed");
  }
  else {
    reportEvent("pCoTGraphics: LatOrigin/LongOrigin not found — "
                "will use NAV anchor from NODE_REPORT");
  }

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
// ============================================================

void CoTGraphics::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();

  // Generic vehicle graphics from pHelmIvP — bridged to shore via pShare.
  // On the vehicle MOOSDB these are own-vehicle only.
  // On the shoreside MOOSDB these include all vehicles; use
  // shoreside=true + vehicle_names to filter vehicle-specific shapes.
  if(m_publish_view_points)   Register("VIEW_POINT",   0);
  if(m_publish_view_seglists) Register("VIEW_SEGLIST", 0);
  if(m_publish_view_polygons) {
    Register("VIEW_POLYGON",  0);
    Register("UTM_ZONE_ONE",  0);
    Register("UTM_ZONE_TWO",  0);
  }
  if(m_publish_view_circles)  Register("VIEW_CIRCLE",  0);

  // Flag markers and score label are shoreside-only variables
  // (uFldFlagManager, uFldTagManager). Only register when enabled.
  if(m_publish_flag_markers) {
    Register("FLAG_SUMMARY", 0);
    Register("VIEW_MARKER",  0);
  }
  if(m_publish_score_label)
    Register("VIEW_TEXTBOX", 0);

  // Geodesy anchor fallback — always needed
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTGraphics::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key  = msg.m_sKey;
    string sval = msg.m_sVal;

    // --------------------------------------------------------
    // NODE_REPORT — update geodesy NAV anchor
    // --------------------------------------------------------
    if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      double x = 0, y = 0, lat = 0, lon = 0;
      bool got_x = false, got_y = false;
      bool got_lat = false, got_lon = false;

      vector<string> tokens = parseString(sval, ',');
      for(auto& tok : tokens) {
        string t = tok;
        string k = toupper(biteStringX(t, '='));
        string v = t;
        if     (k == "X")   { x   = atof(v.c_str()); got_x   = true; }
        else if(k == "Y")   { y   = atof(v.c_str()); got_y   = true; }
        else if(k == "LAT") { lat = atof(v.c_str()); got_lat = true; }
        else if(k == "LON") { lon = atof(v.c_str()); got_lon = true; }
      }
      if(got_x && got_y && got_lat && got_lon) {
        m_geodesy.updateNavAnchor(x, y, lat, lon);
        if(!m_geodesy_initialized) {
          m_geodesy_initialized = true;
          debugLog("OnNewMail: geodesy anchor set from NODE_REPORT");
        }
      }
    }

    // --------------------------------------------------------
    // VIEW_POINT — generic point marker
    // --------------------------------------------------------
    else if(key == "VIEW_POINT" && m_publish_view_points) {
      ViewPoint vp;
      if(parseViewPoint(sval, vp)) {
        if(isLabelBlocked(vp.label)) {
          debugLog("VIEW_POINT: blocked label=" + vp.label);
          continue;
        }
        bool is_new = !m_view_points.count(vp.label);
        if(!is_new) vp.last_sent = m_view_points[vp.label].last_sent;
        m_view_points[vp.label] = vp;
        debugLog("VIEW_POINT " + string(is_new?"[NEW]":"[UPD]") +
                 " " + vp.label);
        if(m_immediate_view_points) {
          Notify("COT_OUTBOUND", buildViewPointCoT(m_view_points[vp.label]));
          m_view_points[vp.label].last_sent = m_curr_time;
          m_vp_cot_sent++;
        }
      }
    }

    // --------------------------------------------------------
    // VIEW_SEGLIST — open polyline
    // --------------------------------------------------------
    else if(key == "VIEW_SEGLIST" && m_publish_view_seglists) {
      ViewSegList vsl;
      if(parseViewSegList(sval, vsl)) {
        if(isLabelBlocked(vsl.label)) {
          debugLog("VIEW_SEGLIST: blocked label=" + vsl.label);
          continue;
        }
        bool is_new = !m_view_seglists.count(vsl.label);
        if(!is_new) vsl.last_sent = m_view_seglists[vsl.label].last_sent;
        m_view_seglists[vsl.label] = vsl;
        debugLog("VIEW_SEGLIST " + string(is_new?"[NEW]":"[UPD]") +
                 " " + vsl.label +
                 " (" + intToString((int)vsl.vertices.size()) + " pts)");
      }
    }

    // --------------------------------------------------------
    // VIEW_POLYGON — closed filled polygon (flag grab zones)
    // --------------------------------------------------------
    else if(key == "VIEW_POLYGON" && m_publish_view_polygons) {
      ViewPolygon vp;
      if(parseViewPolygon(sval, vp, "")) {
        if(isLabelBlocked(vp.label)) {
          debugLog("VIEW_POLYGON: blocked label=" + vp.label);
          continue;
        }
        bool is_new = !m_view_polygons.count(vp.label);
        if(!is_new) vp.last_sent = m_view_polygons[vp.label].last_sent;
        m_view_polygons[vp.label] = vp;
        debugLog("VIEW_POLYGON " + string(is_new?"[NEW]":"[UPD]") +
                 " " + vp.label);
      }
    }

    // --------------------------------------------------------
    // UTM_ZONE_ONE — red team zone boundary
    // map_key="zone_red" avoids collision with flag label "red"
    // --------------------------------------------------------
    else if(key == "UTM_ZONE_ONE" && m_publish_view_polygons) {
      ViewPolygon vp;
      if(parseViewPolygon(sval, vp, "zone_red")) {
        bool is_new = !m_view_polygons.count("zone_red");
        if(!is_new) vp.last_sent = m_view_polygons["zone_red"].last_sent;
        m_view_polygons["zone_red"] = vp;
        debugLog("UTM_ZONE_ONE " + string(is_new?"[NEW]":"[UPD]") +
                 " (" + intToString((int)vp.vertices.size()) + " pts)");
      }
    }

    // --------------------------------------------------------
    // UTM_ZONE_TWO — blue team zone boundary
    // --------------------------------------------------------
    else if(key == "UTM_ZONE_TWO" && m_publish_view_polygons) {
      ViewPolygon vp;
      if(parseViewPolygon(sval, vp, "zone_blue")) {
        bool is_new = !m_view_polygons.count("zone_blue");
        if(!is_new) vp.last_sent = m_view_polygons["zone_blue"].last_sent;
        m_view_polygons["zone_blue"] = vp;
        debugLog("UTM_ZONE_TWO " + string(is_new?"[NEW]":"[UPD]") +
                 " (" + intToString((int)vp.vertices.size()) + " pts)");
      }
    }

    // --------------------------------------------------------
    // VIEW_CIRCLE — circle (u-d-c-c)
    //
    // Format: x=...,y=...,radius=...,label=...,
    //         edge_color=...,fill_color=...,fill_transparency=...,
    //         active=true/false
    //
    // Radius is in meters (same as MOOS local XY units).
    // Center XY is converted to lat/lon via geodesy.
    // --------------------------------------------------------
    else if(key == "VIEW_CIRCLE" && m_publish_view_circles) {
      ViewCircle vc;
      if(parseViewCircle(sval, vc)) {
        if(isLabelBlocked(vc.label)) {
          debugLog("VIEW_CIRCLE: blocked label=" + vc.label);
          continue;
        }
        bool is_new = !m_view_circles.count(vc.label);
        if(!is_new) vc.last_sent = m_view_circles[vc.label].last_sent;
        m_view_circles[vc.label] = vc;
        debugLog("VIEW_CIRCLE " + string(is_new?"[NEW]":"[UPD]") +
                 " " + vc.label +
                 " r=" + doubleToStringX(vc.radius, 1) + "m");
      }
    }

    // --------------------------------------------------------
    // FLAG_SUMMARY — all flags '#'-delimited, send immediately
    // --------------------------------------------------------
    else if(key == "FLAG_SUMMARY" && m_publish_flag_markers) {
      debugLog("FLAG_SUMMARY received");
      parseFlagSummary(sval);
      for(auto& kv : m_view_marker_graphics) {
        ViewMarkerGraphic& vm = kv.second;
        if(!vm.valid) continue;
        Notify("COT_OUTBOUND", buildViewMarkerGraphicCoT(vm));
        vm.last_sent = m_curr_time;
        m_flag_cot_sent++;
      }
    }

    // --------------------------------------------------------
    // VIEW_MARKER — single flag state update, send immediately
    // --------------------------------------------------------
    else if(key == "VIEW_MARKER" && m_publish_flag_markers) {
      ViewMarkerGraphic vm;
      if(parseViewMarkerGraphic(sval, vm)) {
        bool is_new = !m_view_marker_graphics.count(vm.label);
        if(!is_new) vm.last_sent = m_view_marker_graphics[vm.label].last_sent;
        m_view_marker_graphics[vm.label] = vm;
        debugLog("VIEW_MARKER " + string(is_new?"[NEW]":"[UPD]") +
                 " " + vm.label);
        Notify("COT_OUTBOUND",
               buildViewMarkerGraphicCoT(m_view_marker_graphics[vm.label]));
        m_view_marker_graphics[vm.label].last_sent = m_curr_time;
        m_flag_cot_sent++;
      }
    }

    // --------------------------------------------------------
    // VIEW_TEXTBOX — score label, send immediately
    //
    // Rendered as a b-m-p-s-m/LABEL spot marker whose callsign
    // IS the display text. Placed at the XE position (east
    // midfield) by uFldFlagManager's post_score=$(XE) config.
    // --------------------------------------------------------
    else if(key == "VIEW_TEXTBOX" && m_publish_score_label) {
      ViewTextBox vtb;
      if(parseViewTextBox(sval, vtb)) {
        bool is_new = !m_view_textboxes.count(vtb.label);
        if(!is_new) vtb.last_sent = m_view_textboxes[vtb.label].last_sent;
        m_view_textboxes[vtb.label] = vtb;
        debugLog("VIEW_TEXTBOX " + string(is_new?"[NEW]":"[UPD]") +
                 " msg=" + vtb.msg);
        Notify("COT_OUTBOUND",
               buildViewTextBoxCoT(m_view_textboxes[vtb.label]));
        m_view_textboxes[vtb.label].last_sent = m_curr_time;
        m_text_cot_sent++;
      }
    }
  }

  return true;
}


// ============================================================
// Iterate() — throttled resends for all tracked graphics
// ============================================================

bool CoTGraphics::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Throttled VIEW_POINTs (when immediate_view_points = false)
  if(m_publish_view_points && !m_immediate_view_points) {
    for(auto& kv : m_view_points) {
      ViewPoint& vp = kv.second;
      if(!vp.valid) continue;
      if((m_curr_time - vp.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewPointCoT(vp));
      vp.last_sent = m_curr_time;
      m_vp_cot_sent++;
    }
  }

  // Throttled VIEW_SEGLISTs
  if(m_publish_view_seglists) {
    for(auto& kv : m_view_seglists) {
      ViewSegList& vsl = kv.second;
      if(!vsl.valid) continue;
      if((m_curr_time - vsl.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewSegListCoT(vsl));
      vsl.last_sent = m_curr_time;
      m_vsl_cot_sent++;
    }
  }

  // Throttled VIEW_POLYGONs + UTM_ZONE_*
  if(m_publish_view_polygons) {
    for(auto& kv : m_view_polygons) {
      ViewPolygon& vp = kv.second;
      if(!vp.valid) continue;
      if((m_curr_time - vp.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewPolygonCoT(vp));
      vp.last_sent = m_curr_time;
      m_poly_cot_sent++;
    }
  }

  // Throttled VIEW_CIRCLEs
  if(m_publish_view_circles) {
    for(auto& kv : m_view_circles) {
      ViewCircle& vc = kv.second;
      if(!vc.valid) continue;
      if((m_curr_time - vc.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewCircleCoT(vc));
      vc.last_sent = m_curr_time;
      m_circle_cot_sent++;
    }
  }

  // Throttled flag markers
  if(m_publish_flag_markers) {
    for(auto& kv : m_view_marker_graphics) {
      ViewMarkerGraphic& vm = kv.second;
      if(!vm.valid) continue;
      if((m_curr_time - vm.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewMarkerGraphicCoT(vm));
      vm.last_sent = m_curr_time;
      m_flag_cot_sent++;
    }
  }

  // Throttled score label
  if(m_publish_score_label) {
    for(auto& kv : m_view_textboxes) {
      ViewTextBox& vtb = kv.second;
      if(!vtb.valid) continue;
      if((m_curr_time - vtb.last_sent) < m_stationary_send_interval) continue;
      Notify("COT_OUTBOUND", buildViewTextBoxCoT(vtb));
      vtb.last_sent = m_curr_time;
      m_text_cot_sent++;
    }
  }

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// parsePtsBlock()
//
// Shared helper for parseViewSegList and parseViewPolygon.
// Extracts pts={x,y:x,y:...} and converts to (lat,lon) pairs.
// Returns false if block is missing, degenerate, or geodesy
// is not ready.
// ============================================================

bool CoTGraphics::parsePtsBlock(const std::string& raw,
                                 vector<pair<double,double>>& vertices_out)
{
  vertices_out.clear();

  size_t pts_start = raw.find("pts={");
  size_t brace_end = raw.find("}");
  if(pts_start == string::npos || brace_end == string::npos) {
    debugLog("parsePtsBlock: no pts={} block");
    return false;
  }

  string content = raw.substr(pts_start + 5, brace_end - pts_start - 5);
  vector<string> verts = parseString(content, ':');
  if(verts.size() < 2) {
    debugLog("parsePtsBlock: fewer than 2 vertices");
    return false;
  }

  for(unsigned int i = 0; i < verts.size(); i++) {
    vector<string> xy = parseString(verts[i], ',');
    if(xy.size() < 2) continue;
    double x = atof(xy[0].c_str());
    double y = atof(xy[1].c_str());
    double lat = 0.0, lon = 0.0;
    if(!m_geodesy.localXYToLatLon(x, y, lat, lon)) {
      debugLog("parsePtsBlock: geodesy not ready at vertex " + intToString(i));
      return false;
    }
    vertices_out.push_back(make_pair(lat, lon));
  }
  return true;
}


// ============================================================
// parseViewPoint()
//
// Checks active= BEFORE converting XY — active=false sets
// x=0,y=0 which is a valid-looking but incorrect coordinate.
// ============================================================

bool CoTGraphics::parseViewPoint(const std::string& raw, ViewPoint& vp_out)
{
  double x = 0.0, y = 0.0;
  bool got_x = false, got_y = false, got_label = false;
  bool active = true;

  vector<string> tokens = parseString(raw, ',');
  for(auto& tok : tokens) {
    string t = tok;
    string key = tolower(biteStringX(t, '='));
    string val = t;
    if     (key == "x")      { x = atof(val.c_str()); got_x     = true; }
    else if(key == "y")      { y = atof(val.c_str()); got_y     = true; }
    else if(key == "label")  { vp_out.label = val;    got_label = true; }
    else if(key == "active") { setBooleanOnString(active, val); }
  }

  if(!active) {
    auto it = m_view_points.find(vp_out.label);
    if(it != m_view_points.end()) {
      string uid = "aquaticus-vp-" + sanitizeLabel(vp_out.label);
      Notify("COT_OUTBOUND",
             buildDeleteCoT(uid, it->second.lat, it->second.lon));
      m_delete_cot_sent++;
      m_view_points.erase(vp_out.label);
      debugLog("parseViewPoint: deleted " + vp_out.label);
    }
    return false;
  }

  if(!got_x || !got_y || !got_label) return false;
  if(!m_geodesy.localXYToLatLon(x, y, vp_out.lat, vp_out.lon)) return false;

  vp_out.valid = true;
  return true;
}


// ============================================================
// parseViewSegList()
//
// Checks active= BEFORE parsing pts={} block.
// ============================================================

bool CoTGraphics::parseViewSegList(const std::string& raw, ViewSegList& vsl_out)
{
  vsl_out.vertices.clear();
  bool active = true;

  // Extract label and active from key=value after closing brace
  size_t brace_end = raw.find("}");
  string kv_region = (brace_end != string::npos)
    ? raw.substr(brace_end + 1) : raw;

  for(auto& tok : parseString(kv_region, ',')) {
    string t = tok;
    string key = tolower(biteStringX(t, '='));
    if(key == "label")  vsl_out.label = t;
    if(key == "active") setBooleanOnString(active, t);
  }

  if(!active) {
    auto it = m_view_seglists.find(vsl_out.label);
    if(it != m_view_seglists.end()) {
      string uid = "aquaticus-vsl-" + sanitizeLabel(vsl_out.label);
      double lat = it->second.vertices.empty() ? 0.0 : it->second.vertices[0].first;
      double lon = it->second.vertices.empty() ? 0.0 : it->second.vertices[0].second;
      Notify("COT_OUTBOUND", buildDeleteCoT(uid, lat, lon));
      m_delete_cot_sent++;
      m_view_seglists.erase(vsl_out.label);
    }
    return false;
  }

  if(!parsePtsBlock(raw, vsl_out.vertices)) return false;
  if(vsl_out.label.empty())
    vsl_out.label = "seglist-" + intToString((int)m_view_seglists.size());

  vsl_out.valid = true;
  return true;
}


// ============================================================
// parseViewPolygon()
//
// Parses VIEW_POLYGON and UTM_ZONE_* variables. Same pts={}
// format as VIEW_SEGLIST but additionally parses fill_color,
// edge_color, and fill_transparency for ATAK polygon styling.
//
// map_key: if non-empty, overrides the label from the string
//   as the storage key. Used for UTM_ZONE_* to avoid collision
//   with flag label names ("red" / "blue").
// ============================================================

bool CoTGraphics::parseViewPolygon(const std::string& raw,
                                    ViewPolygon& vp_out,
                                    const std::string& map_key)
{
  vp_out.vertices.clear();
  bool active = true;
  string fill_color  = "white";
  string edge_color  = "gray50";
  double fill_transp = 0.0;

  size_t brace_end = raw.find("}");
  string kv_region = (brace_end != string::npos)
    ? raw.substr(brace_end + 1) : raw;

  for(auto& tok : parseString(kv_region, ',')) {
    string t = tok;
    string key = tolower(biteStringX(t, '='));
    string val = t;
    if     (key == "label")            vp_out.label  = val;
    else if(key == "fill_color")       fill_color     = val;
    else if(key == "edge_color")       edge_color     = val;
    else if(key == "fill_transparency")fill_transp    = atof(val.c_str());
    else if(key == "active")           setBooleanOnString(active, val);
  }

  // Apply map_key override so UTM_ZONE_* don't collide with
  // flag marker labels in m_view_marker_graphics
  if(!map_key.empty())
    vp_out.label = map_key;
  else if(vp_out.label.empty())
    vp_out.label = "polygon-" + intToString((int)m_view_polygons.size());

  if(!active) {
    m_view_polygons.erase(vp_out.label);
    return false;
  }

  if(!parsePtsBlock(raw, vp_out.vertices)) return false;

  vp_out.fill_color_argb = moosColorToArgb(fill_color, fill_transp);
  vp_out.edge_color_argb = moosColorToArgb(edge_color, 0.0); // edges always opaque

  vp_out.valid = true;
  return true;
}


// ============================================================
// parseViewMarkerGraphic()
//
// Format: x=X,y=Y,width=W,range=R,primary_color=red,label=red
// ============================================================

bool CoTGraphics::parseViewMarkerGraphic(const std::string& raw,
                                          ViewMarkerGraphic& vm_out)
{
  double x = 0.0, y = 0.0;
  bool got_x = false, got_y = false, got_label = false;
  string primary_color = "white";

  for(auto& tok : parseString(raw, ',')) {
    string t = tok;
    string key = tolower(biteStringX(t, '='));
    string val = t;
    if     (key == "x")             { x = atof(val.c_str()); got_x     = true; }
    else if(key == "y")             { y = atof(val.c_str()); got_y     = true; }
    else if(key == "label")         { vm_out.label = val;    got_label = true; }
    else if(key == "primary_color") { primary_color = val; }
  }

  if(!got_x || !got_y || !got_label) return false;
  if(!m_geodesy.localXYToLatLon(x, y, vm_out.lat, vm_out.lon)) return false;

  // Use hardcoded ARGB constants for team colors to ensure exact
  // match with the values that appear in the ATAK iconsetpath.
  // moosColorToArgb("red") and ("blue") produce identical values,
  // but making this explicit avoids any floating-point rounding.
  string c = tolower(primary_color);
  if     (c == "red")  vm_out.color_argb = ARGB_RED;
  else if(c == "blue") vm_out.color_argb = ARGB_BLUE;
  else                 vm_out.color_argb = moosColorToArgb(primary_color, 0.0);

  vm_out.valid = true;
  return true;
}


// ============================================================
// parseFlagSummary()
//
// '#'-delimited list of VIEW_MARKER-format flag entries.
// '#' is the delimiter because each entry contains commas.
// ============================================================

bool CoTGraphics::parseFlagSummary(const std::string& raw)
{
  bool any = false;
  for(auto& entry : parseString(raw, '#')) {
    string trimmed = stripBlankEnds(entry);
    if(trimmed.empty()) continue;
    ViewMarkerGraphic vm;
    if(parseViewMarkerGraphic(trimmed, vm)) {
      bool is_new = !m_view_marker_graphics.count(vm.label);
      if(!is_new) vm.last_sent = m_view_marker_graphics[vm.label].last_sent;
      m_view_marker_graphics[vm.label] = vm;
      debugLog("parseFlagSummary: " + string(is_new?"[NEW]":"[UPD]") +
               " " + vm.label);
      any = true;
    }
  }
  return any;
}


// ============================================================
// parseViewTextBox()
//
// Format: x=X,y=Y,msg="RED:0 BLUE:0",fsize=20,mcolor=yellow
//
// The msg field may be double-quoted; strip quotes before use.
// Position (x,y) is the MOOS XY map coordinate of the label —
// in Aquaticus this is the XE (east midfield) position.
// ============================================================

bool CoTGraphics::parseViewTextBox(const std::string& raw, ViewTextBox& vtb_out)
{
  double x = 0.0, y = 0.0;
  bool got_x = false, got_y = false, got_msg = false;
  string mcolor = "yellow";

  for(auto& tok : parseString(raw, ',')) {
    string t = tok;
    string key = tolower(biteStringX(t, '='));
    string val = t;
    if     (key == "x")      { x = atof(val.c_str()); got_x   = true; }
    else if(key == "y")      { y = atof(val.c_str()); got_y   = true; }
    else if(key == "msg")    {
      // Strip surrounding double-quotes if present
      if(!val.empty() && val[0] == '"') val = val.substr(1);
      if(!val.empty() && val.back() == '"') val.pop_back();
      vtb_out.msg = val;
      got_msg = true;
    }
    else if(key == "mcolor") { mcolor = val; }
  }

  if(!got_x || !got_y || !got_msg) return false;
  if(!m_geodesy.localXYToLatLon(x, y, vtb_out.lat, vtb_out.lon)) return false;

  // Use yellow constant for the score label to match the
  // mcolor=yellow in the MOOS VIEW_TEXTBOX configuration
  string c = tolower(mcolor);
  if(c == "yellow") vtb_out.color_argb = ARGB_YELLOW;
  else              vtb_out.color_argb = moosColorToArgb(mcolor, 0.0);

  vtb_out.label = "score"; // fixed key — there's only ever one score label
  vtb_out.valid = true;
  return true;
}


// ============================================================
// moosColorToArgb()
//
// MOOS color name + fill_transparency → signed ATAK ARGB int
// ATAK format: 0xAARRGGBB as signed 32-bit int
// fill_transparency: 0.0=opaque, 1.0=transparent (MOOS convention)
// Alpha = round((1.0 - transparency) * 255)
// ============================================================

int CoTGraphics::moosColorToArgb(const std::string& color_name,
                                   double transparency)
{
  int alpha = (int)((1.0 - transparency) * 255.0);
  alpha = max(0, min(255, alpha));

  int r = 255, g = 255, b = 255; // default: white
  string c = tolower(color_name);

  if     (c == "red")                               { r=255; g=0;   b=0;   }
  else if(c == "blue")                              { r=0;   g=0;   b=255; }
  else if(c == "green")                             { r=0;   g=200; b=0;   }
  else if(c == "yellow")                            { r=255; g=255; b=0;   }
  else if(c == "white")                             { r=255; g=255; b=255; }
  else if(c == "black")                             { r=0;   g=0;   b=0;   }
  else if(c == "pink")                              { r=255; g=192; b=203; }
  else if(c == "light_blue"  || c == "lightblue")  { r=173; g=216; b=230; }
  else if(c == "dodger_blue" || c == "dodgerblue") { r=30;  g=144; b=255; }
  else if(c == "cyan")                              { r=0;   g=255; b=255; }
  else if(c == "orange")                            { r=255; g=165; b=0;   }
  else if(c == "gray50"  || c == "grey50")          { r=128; g=128; b=128; }
  else if(c == "gray70"  || c == "grey70")          { r=178; g=178; b=178; }
  else if(c == "gray90"  || c == "grey90")          { r=230; g=230; b=230; }
  else if(c == "gray"    || c == "grey")            { r=128; g=128; b=128; }
  else {
    debugLog("moosColorToArgb: unknown color '" + color_name + "' → white");
  }

  unsigned int argb_u =
    ((unsigned int)alpha << 24) |
    ((unsigned int)r     << 16) |
    ((unsigned int)g     <<  8) |
     (unsigned int)b;
  return (int)argb_u;
}


// ============================================================
// parseViewCircle()
//
// Format: x=...,y=...,radius=...,label=...,
//         edge_color=...,fill_color=...,fill_transparency=...,
//         active=true/false
//
// Radius is in meters (MOOS local XY coordinate units).
// Center XY is converted to lat/lon via geodesy.
// active=false removes the circle from tracking and sends delete.
// ============================================================

bool CoTGraphics::parseViewCircle(const std::string& raw, ViewCircle& vc_out)
{
  double x = 0.0, y = 0.0;
  bool got_x = false, got_y = false, got_r = false, got_label = false;
  bool active = true;
  string edge_color  = "white";
  string fill_color  = "white";
  double fill_transp = 1.0;   // default: transparent fill (outline only)

  for(auto& tok : parseString(raw, ',')) {
    string t   = tok;
    string key = tolower(biteStringX(t, '='));
    string val = t;
    if     (key == "x")                { x = atof(val.c_str()); got_x     = true; }
    else if(key == "y")                { y = atof(val.c_str()); got_y     = true; }
    else if(key == "radius")           { vc_out.radius = atof(val.c_str()); got_r = true; }
    else if(key == "label")            { vc_out.label  = val; got_label   = true; }
    else if(key == "edge_color")       { edge_color    = val; }
    else if(key == "fill_color")       { fill_color    = val; }
    else if(key == "fill_transparency"){ fill_transp   = atof(val.c_str()); }
    else if(key == "active")           { setBooleanOnString(active, val); }
  }

  if(!active) {
    auto it = m_view_circles.find(vc_out.label);
    if(it != m_view_circles.end()) {
      string uid = "aquaticus-circle-" + sanitizeLabel(vc_out.label);
      Notify("COT_OUTBOUND",
             buildDeleteCoT(uid, it->second.lat, it->second.lon));
      m_delete_cot_sent++;
      m_view_circles.erase(vc_out.label);
      debugLog("parseViewCircle: deleted " + vc_out.label);
    }
    return false;
  }

  if(!got_x || !got_y || !got_r || !got_label) return false;
  if(!m_geodesy.localXYToLatLon(x, y, vc_out.lat, vc_out.lon)) return false;

  vc_out.edge_color_argb = moosColorToArgb(edge_color, 0.0);
  vc_out.fill_color_argb = moosColorToArgb(fill_color, fill_transp);
  vc_out.valid = true;
  return true;
}


// ============================================================
// buildViewCircleCoT() — circle (u-d-c-c)
//
// ATAK renders circles using the ellipse element with equal
// major/minor radii and angle=360 for a full circle.
//
// COLOR ENCODING — two separate systems in one CoT:
//
// 1. KML Style block inside <shape><link type="b-x-KmlStyle">:
//    Colors are ABGR hex strings (KML convention, NOT ARGB):
//      AABBGGRR — alpha, blue, green, red
//      "ffffffff" = opaque white in ABGR
//      "00ffffff" = transparent white in ABGR
//    This is what actually renders in ATAK's map layer.
//
// 2. ATAK native <fillColor value="..."/> in <detail>:
//    Signed ARGB integer (ATAK convention):
//      16777215 = 0x00FFFFFF = transparent white
//      -1       = 0xFFFFFFFF = opaque white
//    This mirrors the KML fill color for ATAK's internal state.
//
// Both must be set consistently for correct rendering.
//
// uid format: "aquaticus-circle-{sanitized_label}"
// Style link uid: same + ".Style" (required by ATAK)
// ============================================================

string CoTGraphics::buildViewCircleCoT(const ViewCircle& vc)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-circle-" + sanitizeLabel(vc.label);
  string style_uid = uid + ".Style";

  // Convert ARGB integers to KML ABGR hex strings.
  // ARGB: 0xAARRGGBB → ABGR: 0xAABBGGRR
  auto argbToKml = [](int argb) -> string {
    unsigned int u  = (unsigned int)argb;
    unsigned int aa = (u >> 24) & 0xFF;
    unsigned int rr = (u >> 16) & 0xFF;
    unsigned int gg = (u >>  8) & 0xFF;
    unsigned int bb = (u >>  0) & 0xFF;
    unsigned int kml = (aa << 24) | (bb << 16) | (gg << 8) | rr;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", kml);
    return string(buf);
  };

  string edge_kml = argbToKml(vc.edge_color_argb);
  string fill_kml = argbToKml(vc.fill_color_argb);
  string fill_int = intToString(vc.fill_color_argb);
  string edge_int = intToString(vc.edge_color_argb);
  string radius_s = doubleToStringX(vc.radius, 6);
  string weight_s = doubleToStringX(vc.stroke_weight, 1);

  // KML Style block — controls actual ATAK circle rendering
  string style_link =
    "<link uid=\"" + style_uid + "\""
          " type=\"b-x-KmlStyle\""
          " relation=\"p-c\">"
      "<Style>"
        "<LineStyle>"
          "<color>" + edge_kml + "</color>"
          "<width>" + weight_s + "</width>"
        "</LineStyle>"
        "<PolyStyle>"
          "<color>" + fill_kml + "</color>"
        "</PolyStyle>"
      "</Style>"
    "</link>";

  string detail =
    "<detail>"
      "<shape>"
        "<ellipse"
          " major=\"" + radius_s + "\""
          " minor=\"" + radius_s + "\""
          " angle=\"360\"/>"
        + style_link +
      "</shape>"
      "<__shapeExtras cpvis=\"false\"/>"
      "<contact callsign=\"" + vc.label + "\"/>"
      "<archive/>"
      "<remarks/>"
      "<strokeColor value=\""  + edge_int  + "\"/>"
      "<strokeWeight value=\"" + weight_s  + "\"/>"
      "<strokeStyle value=\"solid\"/>"
      "<fillColor value=\""    + fill_int  + "\"/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid    + "\""
      " type=\"u-d-c-c\""
      " how=\"h-e\""
      " time=\""   + t_now  + "\""
      " start=\""  + t_now  + "\""
      " stale=\""  + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(vc.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vc.lon, 7) + "\""
      " hae=\"0.0\" ce=\"9999999\" le=\"9999999\"/>"
    + detail + "</event>";
}


// ============================================================
// isLabelBlocked()
//
// Returns true if the label should be suppressed.
//
// In shoreside mode (shoreside=true): checks if the label
// contains any vehicle name from m_vehicle_names. This catches
// all vehicle-specific graphics regardless of behavior type:
//   "red_three_loiter_passive" → contains "red_three" → blocked
//   "blue_one:recover:opreg"   → contains "blue_one"  → blocked
//
// Legacy fallback: checks m_label_block_contains patterns.
// Both checks run if both are configured.
// ============================================================

bool CoTGraphics::isLabelBlocked(const std::string& label) const
{
  // Primary: shoreside mode — block any label containing a vehicle name
  if(m_shoreside_mode) {
    for(const auto& vname : m_vehicle_names) {
      if(label.find(vname) != string::npos) {
        return true;
      }
    }
  }

  // Legacy: explicit substring patterns
  for(const auto& pattern : m_label_block_contains) {
    if(label.find(pattern) != string::npos) {
      return true;
    }
  }

  return false;
}


// ============================================================
// sanitizeLabel() — spaces and apostrophes → underscores
// ============================================================

string CoTGraphics::sanitizeLabel(const std::string& label)
{
  string s = label;
  for(char& c : s)
    if(c == ' ' || c == '\'') c = '_';
  return s;
}


// ============================================================
// buildViewPointCoT() — generic spot marker (b-m-p-s-m)
// ============================================================

string CoTGraphics::buildViewPointCoT(const ViewPoint& vp)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-vp-" + sanitizeLabel(vp.label);

  string detail =
    "<detail>"
      "<contact callsign=\"" + vp.label + "\"/>"
      "<precisionlocation geopointsrc=\"GPS\" altsrc=\"GPS\"/>"
      "<status readiness=\"true\"/>"
      "<archive/>"
      "<usericon iconsetpath=\"COT_MAPPING_SPOTMAP/b-m-p-s-m/-1\"/>"
      "<color argb=\"-1\"/>"
      "<remarks/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"b-m-p-s-m\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(vp.lat, 7) + "\""
           " lon=\"" + doubleToStringX(vp.lon, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// buildViewSegListCoT() — open polyline (u-d-f)
// ============================================================

string CoTGraphics::buildViewSegListCoT(const ViewSegList& vsl)
{
  if(vsl.vertices.empty()) return "";

  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-vsl-" + sanitizeLabel(vsl.label);

  string link_xml;
  for(auto& v : vsl.vertices)
    link_xml += "<link point=\"" +
      doubleToStringX(v.first, 7) + "," +
      doubleToStringX(v.second, 7) + ",0.0\"/>";

  string detail =
    "<detail>"
      "<contact callsign=\"" + vsl.label + "\"/>"
      "<strokeColor value=\"-1\"/>"
      "<fillColor value=\"-1\"/>"
      "<strokeWeight value=\"3.0\"/>"
      "<strokeStyle value=\"solid\"/>"
      "<clamped value=\"False\"/>"
      "<height value=\"0.00\"/>"
      "<height_unit value=\"4\"/>"
      "<remarks/>"
      "<archive/>"
      "<__shapeExtras cpvis=\"false\"/>"
      + link_xml +
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"u-d-f\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(vsl.vertices[0].first, 7) + "\""
           " lon=\"" + doubleToStringX(vsl.vertices[0].second, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// buildViewPolygonCoT() — closed filled polygon (u-d-f)
//
// Key differences from open polyline:
//   1. fillColor set from fill_color_argb
//   2. First vertex repeated at end to close the polygon
//      (ATAK requires explicit closure for filled shapes)
//   3. clamped/height/strokeStyle elements from live capture
// ============================================================

string CoTGraphics::buildViewPolygonCoT(const ViewPolygon& vp)
{
  if(vp.vertices.size() < 3) return "";

  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-poly-" + sanitizeLabel(vp.label);
  string fill    = intToString(vp.fill_color_argb);
  string edge    = intToString(vp.edge_color_argb);

  // Build vertex links, then close polygon by repeating vertex 0
  string link_xml;
  for(auto& v : vp.vertices)
    link_xml += "<link point=\"" +
      doubleToStringX(v.first, 7) + "," +
      doubleToStringX(v.second, 7) + ",0.0\"/>";
  // Closure vertex — ATAK requires last point == first point for fill
  link_xml += "<link point=\"" +
    doubleToStringX(vp.vertices[0].first, 7) + "," +
    doubleToStringX(vp.vertices[0].second, 7) + ",0.0\"/>";

  string detail =
    "<detail>"
      "<contact callsign=\"" + vp.label + "\"/>"
      "<strokeColor value=\"" + edge  + "\"/>"
      "<fillColor value=\""   + fill  + "\"/>"
      "<strokeWeight value=\"2.0\"/>"
      "<strokeStyle value=\"solid\"/>"
      "<clamped value=\"False\"/>"
      "<height value=\"0.00\"/>"
      "<height_unit value=\"4\"/>"
      "<remarks/>"
      "<archive/>"
      "<__shapeExtras cpvis=\"false\"/>"
      + link_xml +
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"u-d-f\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(vp.vertices[0].first, 7) + "\""
           " lon=\"" + doubleToStringX(vp.vertices[0].second, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// buildViewMarkerGraphicCoT() — colored flag spot marker
//
// The ARGB integer is embedded in BOTH the iconsetpath AND
// the <color> element — confirmed from live ATAK captures:
//   iconsetpath="COT_MAPPING_SPOTMAP/b-m-p-s-m/-65536"  (red)
//   iconsetpath="COT_MAPPING_SPOTMAP/b-m-p-s-m/-16776961" (blue)
// Using just <color> without the iconsetpath argb suffix does
// not produce the correct team-colored icon in ATAK.
// ============================================================

string CoTGraphics::buildViewMarkerGraphicCoT(const ViewMarkerGraphic& vm)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-flag-" + sanitizeLabel(vm.label);
  string argb    = intToString(vm.color_argb);

  // ARGB embedded in iconsetpath suffix — this is what produces
  // the colored icon in ATAK, not just the <color> element alone
  string iconpath = "COT_MAPPING_SPOTMAP/b-m-p-s-m/" + argb;

  string detail =
    "<detail>"
      "<contact callsign=\"" + vm.label + " flag\"/>"
      "<precisionlocation geopointsrc=\"GPS\" altsrc=\"GPS\"/>"
      "<status readiness=\"true\"/>"
      "<archive/>"
      "<usericon iconsetpath=\"" + iconpath + "\"/>"
      "<color argb=\""           + argb     + "\"/>"
      "<remarks/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"b-m-p-s-m\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(vm.lat, 7) + "\""
           " lon=\"" + doubleToStringX(vm.lon, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// buildViewTextBoxCoT() — text label on ATAK map (b-m-p-s-m/LABEL)
//
// Uses the special LABEL iconsetpath suffix which causes ATAK
// to render the callsign text as a map label rather than an icon.
// The score "RED:0 BLUE:0" becomes the callsign — ATAK displays
// it as a visible text annotation at the label's map position.
//
// Confirmed iconsetpath for text labels from live ATAK capture:
//   "COT_MAPPING_SPOTMAP/b-m-p-s-m/LABEL"
// ============================================================

string CoTGraphics::buildViewTextBoxCoT(const ViewTextBox& vtb)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-score";
  string argb    = intToString(vtb.color_argb);

  string detail =
    "<detail>"
      "<contact callsign=\"" + vtb.msg + "\"/>"
      "<precisionlocation geopointsrc=\"GPS\" altsrc=\"GPS\"/>"
      "<status readiness=\"true\"/>"
      "<archive/>"
      "<usericon iconsetpath=\"COT_MAPPING_SPOTMAP/b-m-p-s-m/LABEL\"/>"
      "<color argb=\""  + argb + "\"/>"
      "<remarks/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"b-m-p-s-m\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(vtb.lat, 7) + "\""
           " lon=\"" + doubleToStringX(vtb.lon, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// buildDeleteCoT() — t-x-d-d delete command
// ============================================================

string CoTGraphics::buildDeleteCoT(const std::string& target_uid,
                                    double lat, double lon)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, 60.0);
  string cmd_uid = "aquaticus-del-" + target_uid;

  string detail =
    "<detail>"
      "<link uid=\"" + target_uid + "\" relation=\"none\" type=\"none\"/>"
      "<__forcedelete/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""    + cmd_uid  + "\""
      " type=\"t-x-d-d\""
      " how=\""   + HOW_MG + "\""
      " time=\""  + t_now    + "\""
      " start=\"" + t_now    + "\""
      " stale=\"" + t_stale  + "\""
      " access=\"Undefined\">"
    "<point lat=\"" + doubleToStringX(lat, 7) + "\""
           " lon=\"" + doubleToStringX(lon, 7) + "\""
           " hae=\"0.0\" ce=\"0\" le=\"0\"/>"
    + detail + "</event>";
}


// ============================================================
// formatCoTTime()
// ============================================================

string CoTGraphics::formatCoTTime(double moos_time, double offset)
{
  time_t t = (time_t)(moos_time + offset);
  struct tm* utc = gmtime(&t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
  return string(buf);
}


// ============================================================
// buildReport()
// ============================================================

bool CoTGraphics::buildReport()
{
  m_msgs << "Geodesy: " << m_geodesy.getModeString()
         << (m_geodesy_initialized ? " [ready]" : " [NOT READY]")
         << "  debug=" << boolToString(m_debug) << endl;
  m_msgs << "Mode: " << (m_shoreside_mode ? "SHORESIDE" : "VEHICLE");
  if(m_shoreside_mode)
    m_msgs << " — filtering " << m_vehicle_names.size() << " vehicle names";
  m_msgs << endl;
  m_msgs << endl;

  m_msgs << "CoT sent:"
         << "  vp="     << m_vp_cot_sent
         << "  vsl="    << m_vsl_cot_sent
         << "  poly="   << m_poly_cot_sent
         << "  circle=" << m_circle_cot_sent
         << "  flag="   << m_flag_cot_sent
         << "  text="   << m_text_cot_sent
         << "  delete=" << m_delete_cot_sent << endl;
  m_msgs << endl;

  m_msgs << "VIEW_POINTs   (" << m_view_points.size() << ")"
         << (m_publish_view_points ? "" : " [disabled]")
         << (m_immediate_view_points ? " immediate" : " throttled") << ":";
  for(auto& kv : m_view_points)
    m_msgs << " " << kv.first;
  m_msgs << endl;

  m_msgs << "VIEW_SEGLISTs (" << m_view_seglists.size() << ")"
         << (m_publish_view_seglists ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_seglists)
    m_msgs << " " << kv.first
           << "(" << kv.second.vertices.size() << "pts)";
  m_msgs << endl;

  m_msgs << "VIEW_POLYGONs (" << m_view_polygons.size() << ")"
         << (m_publish_view_polygons ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_polygons)
    m_msgs << " " << kv.first
           << "(" << kv.second.vertices.size() << "pts)";
  m_msgs << endl;

  m_msgs << "VIEW_CIRCLEs  (" << m_view_circles.size() << ")"
         << (m_publish_view_circles ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_circles)
    m_msgs << " " << kv.first
           << "(r=" << doubleToStringX(kv.second.radius, 0) << "m)";
  m_msgs << endl;

  m_msgs << "Flag markers  (" << m_view_marker_graphics.size() << ")"
         << (m_publish_flag_markers ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_marker_graphics)
    m_msgs << " " << kv.first;
  m_msgs << endl;

  m_msgs << "Score label   (" << m_view_textboxes.size() << ")"
         << (m_publish_score_label ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_textboxes)
    m_msgs << " \"" << kv.second.msg << "\"";
  m_msgs << endl;

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
