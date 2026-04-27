/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGraphics.cpp                                 */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <ctime>
#include "CoTGraphics.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTGraphics::CoTGraphics()
{
  m_geodesy_initialized      = false;
  m_publish_view_points      = true;
  m_publish_view_seglists    = true;
  m_immediate_view_points    = true;
  m_stationary_send_interval = 3.0;
  m_cot_stale_offset         = 86400.0; // 24 hours — graphics persist
  m_debug                    = false;

  m_vp_cot_sent     = 0;
  m_vsl_cot_sent    = 0;
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
//
// ProcessConfig = pCoTGraphics
// {
//   AppTick   = 10
//   CommsTick = 10
//
//   publish_view_points    = true
//   publish_view_seglists  = true
//   immediate_view_points  = true
//   stationary_send_interval = 3.0
//
//   use_nav_fallback = false
//   debug = false
// }
// ============================================================

bool CoTGraphics::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug flag
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

    if(param == "debug") {
      setBooleanOnString(m_debug, value);
    }
    else if(param == "publish_view_points") {
      setBooleanOnString(m_publish_view_points, value);
      debugLog("Config: publish_view_points = " +
               boolToString(m_publish_view_points));
    }
    else if(param == "publish_view_seglists") {
      setBooleanOnString(m_publish_view_seglists, value);
      debugLog("Config: publish_view_seglists = " +
               boolToString(m_publish_view_seglists));
    }
    else if(param == "immediate_view_points") {
      setBooleanOnString(m_immediate_view_points, value);
      debugLog("Config: immediate_view_points = " +
               boolToString(m_immediate_view_points));
    }
    else if(param == "stationary_send_interval") {
      m_stationary_send_interval = atof(value.c_str());
      debugLog("Config: stationary_send_interval = " +
               doubleToStringX(m_stationary_send_interval) + "s");
    }
    else if(param == "use_nav_fallback") {
      setBooleanOnString(use_nav_fallback, value);
      debugLog("Config: use_nav_fallback = " + boolToString(use_nav_fallback));
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Initialize geodesy
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
    else {
      reportRunWarning("pCoTGraphics: geodesy initialisation failed");
    }
  }
  else {
    reportEvent("pCoTGraphics: LatOrigin/LongOrigin not found — "
                "will use NAV anchor for coordinate conversion");
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
  Register("VIEW_POINT",        0);
  Register("VIEW_SEGLIST",      0);
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
        string t_copy = tok;
        string k = toupper(biteStringX(t_copy, '='));
        string v = t_copy;
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
    // VIEW_POINT — single point marker
    // --------------------------------------------------------
    else if(key == "VIEW_POINT" && m_publish_view_points) {
      debugLog("OnNewMail: VIEW_POINT = " + sval);
      ViewPoint vp;
      if(parseViewPoint(sval, vp)) {
        bool is_new = (m_view_points.find(vp.label) == m_view_points.end());
        if(!is_new) vp.last_sent = m_view_points[vp.label].last_sent;
        m_view_points[vp.label] = vp;
        debugLog("OnNewMail: VIEW_POINT " +
                 string(is_new ? "[NEW]" : "[UPDATE]") +
                 " label=" + vp.label +
                 " lat=" + doubleToStringX(vp.lat, 6) +
                 " lon=" + doubleToStringX(vp.lon, 6));

        // Send immediately if configured
        if(m_immediate_view_points) {
          string cot = buildViewPointCoT(m_view_points[vp.label]);
          Notify("COT_OUTBOUND", cot);
          m_view_points[vp.label].last_sent = m_curr_time;
          m_vp_cot_sent++;
          debugLog("OnNewMail: VIEW_POINT immediate send → COT_OUTBOUND");
        }
      }
      // parseViewPoint returns false for active=false — delete already sent
    }

    // --------------------------------------------------------
    // VIEW_SEGLIST — polyline
    // --------------------------------------------------------
    else if(key == "VIEW_SEGLIST" && m_publish_view_seglists) {
      debugLog("OnNewMail: VIEW_SEGLIST = " + sval);
      ViewSegList vsl;
      if(parseViewSegList(sval, vsl)) {
        bool is_new = (m_view_seglists.find(vsl.label) == m_view_seglists.end());
        if(!is_new) vsl.last_sent = m_view_seglists[vsl.label].last_sent;
        m_view_seglists[vsl.label] = vsl;
        debugLog("OnNewMail: VIEW_SEGLIST " +
                 string(is_new ? "[NEW]" : "[UPDATE]") +
                 " label=" + vsl.label +
                 " vertices=" + intToString((int)vsl.vertices.size()));
      }
    }
  }

  return true;
}


// ============================================================
// Iterate()
//
// Sends throttled VIEW_POINT and VIEW_SEGLIST CoT updates.
// Immediate VIEW_POINTs are sent in OnNewMail — skipped here.
// ============================================================

bool CoTGraphics::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Throttled VIEW_POINTs (immediate_view_points = false)
  if(m_publish_view_points && !m_immediate_view_points) {
    for(auto& kv : m_view_points) {
      ViewPoint& vp = kv.second;
      if(!vp.valid) continue;
      if((m_curr_time - vp.last_sent) < m_stationary_send_interval) continue;
      string cot = buildViewPointCoT(vp);
      Notify("COT_OUTBOUND", cot);
      vp.last_sent = m_curr_time;
      m_vp_cot_sent++;
      debugLog("Iterate: VIEW_POINT throttled send → " + vp.label);
    }
  }

  // Throttled VIEW_SEGLISTs
  if(m_publish_view_seglists) {
    for(auto& kv : m_view_seglists) {
      ViewSegList& vsl = kv.second;
      if(!vsl.valid) continue;
      if((m_curr_time - vsl.last_sent) < m_stationary_send_interval) continue;
      string cot = buildViewSegListCoT(vsl);
      Notify("COT_OUTBOUND", cot);
      vsl.last_sent = m_curr_time;
      m_vsl_cot_sent++;
      debugLog("Iterate: VIEW_SEGLIST throttled send → " + vsl.label);
    }
  }

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// parseViewPoint()
//
// Format: x=60,y=-53.18,label=alpha's trackpt,active=true,...
//
// Checks active= FIRST before converting XY — when active=false
// the x/y values are meaningless (set to 0,0) so we must not
// try to convert them. Sends delete CoT and erases map entry.
// ============================================================

bool CoTGraphics::parseViewPoint(const std::string& raw, ViewPoint& vp_out)
{
  double x = 0.0, y = 0.0;
  bool got_x = false, got_y = false, got_label = false;
  bool active = true;

  vector<string> tokens = parseString(raw, ',');
  for(auto& tok : tokens) {
    string t_copy = tok;
    string key = tolower(biteStringX(t_copy, '='));
    string val = t_copy;
    if     (key == "x")      { x = atof(val.c_str()); got_x     = true; }
    else if(key == "y")      { y = atof(val.c_str()); got_y     = true; }
    else if(key == "label")  { vp_out.label = val;    got_label = true; }
    else if(key == "active") { setBooleanOnString(active, val); }
  }

  // Handle deactivation BEFORE XY conversion
  if(!active) {
    debugLog("parseViewPoint: active=false label=" + vp_out.label +
             " — sending delete CoT");
    auto it = m_view_points.find(vp_out.label);
    if(it != m_view_points.end()) {
      string uid = "aquaticus-vp-" + sanitizeLabel(vp_out.label);
      string del = buildDeleteCoT(uid, it->second.lat, it->second.lon);
      Notify("COT_OUTBOUND", del);
      m_delete_cot_sent++;
      m_view_points.erase(vp_out.label);
    }
    return false;
  }

  if(!got_x || !got_y || !got_label) {
    debugLog("parseViewPoint: missing fields in: " + raw);
    return false;
  }

  if(!m_geodesy.localXYToLatLon(x, y, vp_out.lat, vp_out.lon)) {
    debugLog("parseViewPoint: geodesy not ready — cannot convert x=" +
             doubleToStringX(x, 2) + " y=" + doubleToStringX(y, 2));
    return false;
  }

  vp_out.valid = true;
  return true;
}


// ============================================================
// parseViewSegList()
//
// Format: pts={x,y:x,y:...},label=alpha_waypt_survey,active=true,...
//
// Checks active= BEFORE pts parsing — when active=false the pts
// block contains garbage data and must not be parsed.
// ============================================================

bool CoTGraphics::parseViewSegList(const std::string& raw, ViewSegList& vsl_out)
{
  vsl_out.vertices.clear();

  // Pass 1: extract label and active from key=value after pts block
  bool active = true;
  size_t brace_end = raw.find("}");
  string kv_region = (brace_end != string::npos)
    ? raw.substr(brace_end + 1)
    : raw;

  vector<string> kv_tokens = parseString(kv_region, ',');
  for(auto& tok : kv_tokens) {
    string t_copy = tok;
    string key = tolower(biteStringX(t_copy, '='));
    string val = t_copy;
    if(key == "label")  vsl_out.label = val;
    if(key == "active") setBooleanOnString(active, val);
  }

  // Handle deactivation BEFORE pts parsing
  if(!active) {
    debugLog("parseViewSegList: active=false label=" + vsl_out.label +
             " — sending delete CoT");
    auto it = m_view_seglists.find(vsl_out.label);
    if(it != m_view_seglists.end()) {
      string uid = "aquaticus-vsl-" + sanitizeLabel(vsl_out.label);
      double lat = it->second.vertices.empty() ? 0.0
                                               : it->second.vertices[0].first;
      double lon = it->second.vertices.empty() ? 0.0
                                               : it->second.vertices[0].second;
      string del = buildDeleteCoT(uid, lat, lon);
      Notify("COT_OUTBOUND", del);
      m_delete_cot_sent++;
      m_view_seglists.erase(vsl_out.label);
    }
    return false;
  }

  // Pass 2: parse pts={...} for active seglist
  size_t pts_start = raw.find("pts={");
  if(pts_start == string::npos || brace_end == string::npos) {
    debugLog("parseViewSegList: no pts={} block in: " + raw);
    return false;
  }

  string pts_content = raw.substr(pts_start + 5, brace_end - pts_start - 5);
  vector<string> vertex_strs = parseString(pts_content, ':');
  if(vertex_strs.size() < 2) {
    debugLog("parseViewSegList: fewer than 2 vertices");
    return false;
  }

  for(unsigned int i = 0; i < vertex_strs.size(); i++) {
    vector<string> xy = parseString(vertex_strs[i], ',');
    if(xy.size() < 2) {
      debugLog("parseViewSegList: bad vertex: " + vertex_strs[i]);
      return false;
    }
    double x = atof(xy[0].c_str());
    double y = atof(xy[1].c_str());
    double lat = 0.0, lon = 0.0;
    if(!m_geodesy.localXYToLatLon(x, y, lat, lon)) {
      debugLog("parseViewSegList: geodesy not ready for vertex " +
               intToString(i));
      return false;
    }
    vsl_out.vertices.push_back(make_pair(lat, lon));
  }

  if(vsl_out.label.empty())
    vsl_out.label = "seglist-" + intToString((int)m_view_seglists.size());

  vsl_out.valid = true;
  return true;
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
// buildViewPointCoT() — spot marker (b-m-p-s-m)
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
    "<event"
      " version=\"2.0\""
      " uid=\""    + uid    + "\""
      " type=\"b-m-p-s-m\""
      " how=\"m-g\""
      " time=\""   + t_now  + "\""
      " start=\""  + t_now  + "\""
      " stale=\""  + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(vp.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vp.lon, 7) + "\""
      " hae=\"0.0\" ce=\"9999999.0\" le=\"9999999.0\"/>"
    + detail +
    "</event>";
}


// ============================================================
// buildViewSegListCoT() — polyline (u-d-f)
//
// Uses <link point="lat,lon,hae"/> elements per vertex —
// confirmed from live ATAK polyline capture.
// ============================================================

string CoTGraphics::buildViewSegListCoT(const ViewSegList& vsl)
{
  if(vsl.vertices.empty()) return "";

  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_offset);
  string uid     = "aquaticus-vsl-" + sanitizeLabel(vsl.label);

  double anchor_lat = vsl.vertices[0].first;
  double anchor_lon = vsl.vertices[0].second;

  string link_xml;
  for(auto& v : vsl.vertices) {
    link_xml +=
      "<link point=\""
      + doubleToStringX(v.first,  7) + ","
      + doubleToStringX(v.second, 7) + ","
      + "0.0\"/>";
  }

  string detail =
    "<detail>"
      "<color argb=\"-1\" value=\"-1\"/>"
      "<fillColor value=\"-1\"/>"
      "<strokeColor value=\"-1\"/>"
      "<strokeWeight value=\"3.0\"/>"
      "<remarks/>"
      "<archive/>"
      "<contact callsign=\"" + vsl.label + "\"/>"
      + link_xml +
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""    + uid     + "\""
      " type=\"u-d-f\""
      " how=\"m-g\""
      " time=\""   + t_now   + "\""
      " start=\""  + t_now   + "\""
      " stale=\""  + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(anchor_lat, 7) + "\""
      " lon=\"" + doubleToStringX(anchor_lon, 7) + "\""
      " hae=\"0.0\" ce=\"9999999.0\" le=\"9999999.0\"/>"
    + detail +
    "</event>";
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
    "<event"
      " version=\"2.0\""
      " uid=\""    + cmd_uid  + "\""
      " type=\"t-x-d-d\""
      " how=\"m-g\""
      " time=\""   + t_now    + "\""
      " start=\""  + t_now    + "\""
      " stale=\""  + t_stale  + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(lat, 7) + "\""
      " lon=\"" + doubleToStringX(lon, 7) + "\""
      " hae=\"0.0\" ce=\"9999999.0\" le=\"9999999.0\"/>"
    + detail +
    "</event>";
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
  m_msgs << endl;

  m_msgs << "CoT sent: vp=" << m_vp_cot_sent
         << "  vsl=" << m_vsl_cot_sent
         << "  delete=" << m_delete_cot_sent << endl;
  m_msgs << endl;

  m_msgs << "VIEW_POINTs  (" << m_view_points.size() << ")"
         << (m_publish_view_points ? "" : " [disabled]")
         << (m_immediate_view_points ? " immediate" : " throttled") << ":";
  for(auto& kv : m_view_points)
    m_msgs << " " << kv.first;
  m_msgs << endl;

  m_msgs << "VIEW_SEGLISTs(" << m_view_seglists.size() << ")"
         << (m_publish_view_seglists ? "" : " [disabled]") << ":";
  for(auto& kv : m_view_seglists)
    m_msgs << " " << kv.first
           << "(" << kv.second.vertices.size() << "pts)";
  m_msgs << endl;

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
