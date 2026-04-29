/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTContact.cpp                                  */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <ctime>
#include "CoTContact.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTContact::CoTContact()
{
  m_multi_mode  = false;
  m_own_vehicle = "";

  m_moving_send_interval     = 1.0;
  m_stationary_send_interval = 3.0;
  m_speed_threshold          = 0.5;
  m_cot_stale_offset         = 10.0;

  m_affiliation = "f";   // default: friendly
  m_team_color  = "";    // default: no __group element (map-only if no team_color set)

  m_debug        = false;
  m_pos_cot_sent = 0;
}


// ============================================================
// debugLog()
// ============================================================

void CoTContact::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTContact::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
//
// Single-vehicle mode (hardware):
//   own_vehicle = blue_one
//
// Multi-vehicle mode (shoreside sim):
//   own_vehicles     = blue_one,blue_two,blue_three
//   hostile_vehicles = red_one,red_two,red_three
//
// If both own_vehicle and own_vehicles are set, own_vehicles
// takes precedence and multi-vehicle mode is used.
// ============================================================

bool CoTContact::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug first
  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

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
    else if(param == "own_vehicle") {
      // Single-vehicle mode
      string v = orig; biteStringX(v, '=');
      m_own_vehicle = stripBlankEnds(v);
      debugLog("Config: own_vehicle = " + m_own_vehicle);
    }
    else if(param == "own_vehicles") {
      // Multi-vehicle mode — comma-separated friendly names
      string v = orig; biteStringX(v, '=');
      string log_str;
      for(auto& n : parseString(v, ',')) {
        string trimmed = stripBlankEnds(n);
        m_own_set.insert(trimmed);
        log_str += trimmed + " ";
      }
      m_multi_mode = true;
      debugLog("Config: own_vehicles = { " + log_str + "}");
    }
    else if(param == "hostile_vehicles") {
      // Multi-vehicle mode — comma-separated hostile names
      string v = orig; biteStringX(v, '=');
      string log_str;
      for(auto& n : parseString(v, ',')) {
        string trimmed = stripBlankEnds(n);
        m_hostile_set.insert(trimmed);
        log_str += trimmed + " ";
      }
      m_multi_mode = true;
      debugLog("Config: hostile_vehicles = { " + log_str + "}");
    }
    else if(param == "moving_send_interval") {
      m_moving_send_interval = atof(value.c_str());
      debugLog("Config: moving_send_interval = " +
               doubleToStringX(m_moving_send_interval) + "s");
    }
    else if(param == "stationary_send_interval") {
      m_stationary_send_interval = atof(value.c_str());
      debugLog("Config: stationary_send_interval = " +
               doubleToStringX(m_stationary_send_interval) + "s");
    }
    else if(param == "speed_threshold") {
      m_speed_threshold = atof(value.c_str());
      debugLog("Config: speed_threshold = " +
               doubleToStringX(m_speed_threshold) + "m/s");
    }
    else if(param == "cot_stale_offset") {
      m_cot_stale_offset = atof(value.c_str());
      debugLog("Config: cot_stale_offset = " +
               doubleToStringX(m_cot_stale_offset) + "s");
    }
    else if(param == "affiliation") {
      // f=friendly, h=hostile, n=neutral, u=unknown
      // Used in single-vehicle mode to set the CoT type.
      // In multi-vehicle mode, affiliation is derived from
      // own_vehicles / hostile_vehicles membership.
      m_affiliation = value;
      if(m_affiliation != "f" && m_affiliation != "h" &&
         m_affiliation != "n" && m_affiliation != "u") {
        reportConfigWarning("pCoTContact: invalid affiliation '" +
                            m_affiliation + "' — use f/h/n/u. Defaulting to f.");
        m_affiliation = "f";
      }
      debugLog("Config: affiliation = " + m_affiliation);
    }
    else if(param == "team_color") {
      // Preserve original case — ATAK team color names are capitalized.
      // Only used for friendly contacts (<__group name="..."/>).
      // Valid values: Cyan, Blue, Red, Green, Yellow, White, Magenta, Orange
      string v = orig; biteStringX(v, '=');
      m_team_color = stripBlankEnds(v);
      debugLog("Config: team_color = " + m_team_color);
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Validate
  if(m_multi_mode) {
    // own_vehicle + own_vehicles both set? Merge single into set
    if(!m_own_vehicle.empty())
      m_own_set.insert(m_own_vehicle);
    if(m_own_set.empty() && m_hostile_set.empty())
      reportConfigWarning("pCoTContact: multi-vehicle mode but no vehicles listed");
    debugLog("OnStartUp: MULTI-VEHICLE mode — own=" +
             intToString((int)m_own_set.size()) +
             " hostile=" + intToString((int)m_hostile_set.size()));
  }
  else {
    if(m_own_vehicle.empty())
      reportConfigWarning("pCoTContact: own_vehicle not set — "
                          "will auto-learn from first NODE_REPORT");
    debugLog("OnStartUp: SINGLE-VEHICLE mode — vehicle=" + m_own_vehicle);
  }

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
// ============================================================

void CoTContact::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTContact::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key = msg.m_sKey;
    if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL")
      parseNodeReport(msg.m_sVal);
  }

  return true;
}


// ============================================================
// Iterate() — throttled CoT send for each tracked vehicle
// ============================================================

bool CoTContact::Iterate()
{
  AppCastingMOOSApp::Iterate();

  for(auto& kv : m_vehicles) {
    VehicleState& vs = kv.second;
    if(!vs.valid) continue;

    bool   moving   = (vs.speed > m_speed_threshold);
    double interval = moving ? m_moving_send_interval
                             : m_stationary_send_interval;
    if((m_curr_time - vs.last_sent) < interval) continue;

    Notify("COT_OUTBOUND", buildPositionCoT(vs));
    vs.last_sent = m_curr_time;
    m_pos_cot_sent++;
    debugLog("Iterate: pos CoT sent for " + vs.name +
             " [" + string(moving ? "moving" : "static") + "]");
  }

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// shouldTrack()
// ============================================================

bool CoTContact::shouldTrack(const std::string& name) const
{
  if(m_multi_mode)
    return (m_own_set.count(name) > 0 || m_hostile_set.count(name) > 0);
  else
    return (name == m_own_vehicle);
}


// ============================================================
// isFriendly()
// ============================================================

bool CoTContact::isFriendly(const std::string& name) const
{
  if(m_multi_mode)
    return (m_own_set.count(name) > 0);
  else
    return true; // single-vehicle mode is always own vehicle
}


// ============================================================
// parseNodeReport()
//
// Parses NAME=...,LAT=...,LON=...,HDG=...,SPD=...,TIME=...
// Creates a new VehicleState entry on first receipt.
// In single-vehicle mode, auto-learns the vehicle name from
// the first NODE_REPORT if own_vehicle was not configured.
// ============================================================

bool CoTContact::parseNodeReport(const std::string& report)
{
  bool got_name = false, got_lat = false, got_lon = false;
  bool got_hdg  = false, got_spd = false, got_time = false;
  string name;
  double lat = 0, lon = 0, hdg = 0, spd = 0, t = 0;

  for(auto& tok : parseString(report, ',')) {
    string t_copy = tok;
    string key    = toupper(biteStringX(t_copy, '='));
    string val    = t_copy;
    if     (key == "NAME") { name = val;               got_name = true; }
    else if(key == "LAT")  { lat  = atof(val.c_str()); got_lat  = true; }
    else if(key == "LON")  { lon  = atof(val.c_str()); got_lon  = true; }
    else if(key == "HDG")  { hdg  = atof(val.c_str()); got_hdg  = true; }
    else if(key == "SPD")  { spd  = atof(val.c_str()); got_spd  = true; }
    else if(key == "TIME") { t    = atof(val.c_str()); got_time = true; }
  }

  if(!got_name || !got_lat || !got_lon || !got_hdg || !got_spd || !got_time)
    return false;

  // Auto-learn vehicle name in single-vehicle mode if not configured
  if(!m_multi_mode && m_own_vehicle.empty()) {
    m_own_vehicle = name;
    debugLog("parseNodeReport: auto-learned own_vehicle = " + name);
  }

  if(!shouldTrack(name)) {
    debugLog("parseNodeReport: ignoring " + name + " (not tracked)");
    return false;
  }

  // Create entry on first receipt
  if(m_vehicles.find(name) == m_vehicles.end()) {
    VehicleState vs;
    vs.name     = name;
    vs.friendly = isFriendly(name);
    m_vehicles[name] = vs;
    debugLog("parseNodeReport: new vehicle " + name +
             " [" + string(vs.friendly ? "friendly" : "hostile") + "]");
  }

  VehicleState& vs = m_vehicles[name];
  vs.lat       = lat;
  vs.lon       = lon;
  vs.heading   = hdg;
  vs.speed     = spd;
  vs.timestamp = t;
  vs.valid     = true;

  debugLog("parseNodeReport: updated " + name +
           " lat=" + doubleToStringX(lat, 6) +
           " lon=" + doubleToStringX(lon, 6));
  return true;
}


// ============================================================
// buildPositionCoT()
//
// MIL-STD 2525C SA contact for a surface vessel:
//   a-f-S-C-U-N — friendly surface combatant unit naval
//   a-h-S-C-U-N — hostile  surface combatant unit naval
//
// ce/le = 9999999 — GPS source but accuracy unknown from
// NODE_REPORT (no CEP field). Standard "accuracy unknown" value.
// ============================================================

string CoTContact::buildPositionCoT(const VehicleState& vs)
{
  // In multi-vehicle mode, affiliation comes from own_set/hostile_set.
  // In single-vehicle mode, it comes from the m_affiliation config param.
  string affil = m_multi_mode
    ? (vs.friendly ? "f" : "h")
    : m_affiliation;

  string cot_type = "a-" + affil + "-S-C-U-N";
  string uid      = "surveyor-" + vs.name;

  string t_now   = formatCoTTime(vs.timestamp, 0.0);
  string t_stale = formatCoTTime(vs.timestamp, m_cot_stale_offset);

  // __group element — included whenever team_color is configured.
  // Controls icon color and contacts-list visibility in ATAK:
  //   affiliation=f + team_color=Cyan → friendly cyan icon, IN contacts list
  //   affiliation=f + team_color=Red  → friendly red icon,  IN contacts list
  //   affiliation=h + team_color=Red  → hostile red icon,   IN contacts list
  //   affiliation=h (no team_color)   → hostile diamond,   MAP ONLY
  // Omit team_color entirely (leave empty) to suppress contacts list entry.
  string group_elem = "";
  if(!m_team_color.empty())
    group_elem = "<__group name=\"" + m_team_color +
                 "\" role=\"Team Member\"/>";

  string detail =
    "<detail>"
      "<contact callsign=\"" + vs.name + "\""
               " endpoint=\"*:-1:stcp\"/>"
    + group_elem +
      "<uid Droid=\""        + vs.name + "\"/>"
      "<takv"
        " device=\"SeaRobotics-Surveyor\""
        " platform=\"pCoTContact\""
        " os=\"0\""
        " version=\"1.0.0\"/>"
      "<precisionlocation geopointsrc=\"GPS\" altsrc=\"GPS\"/>"
      "<track"
        " speed=\""  + doubleToStringX(vs.speed,   2) + "\""
        " course=\"" + doubleToStringX(vs.heading,  1) + "\"/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""    + uid      + "\""
      " type=\""   + cot_type + "\""
      " how=\"m-g\""
      " time=\""   + t_now    + "\""
      " start=\""  + t_now    + "\""
      " stale=\""  + t_stale  + "\">"
    "<point"
      " lat=\"" + doubleToStringX(vs.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vs.lon, 7) + "\""
      " hae=\"0.0\""
      " ce=\"9999999\" le=\"9999999\"/>"
    + detail +
    "</event>";
}


// ============================================================
// formatCoTTime()
// ============================================================

string CoTContact::formatCoTTime(double moos_time, double offset)
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

bool CoTContact::buildReport()
{
  string mode = m_multi_mode
    ? "MULTI-VEHICLE (own=" + intToString((int)m_own_set.size()) +
      " hostile=" + intToString((int)m_hostile_set.size()) + ")"
    : "SINGLE-VEHICLE (" + m_own_vehicle + ")";
  m_msgs << "Mode: " << mode
         << "  debug=" << boolToString(m_debug) << endl;
  m_msgs << "CoT sent: " << m_pos_cot_sent << endl;
  m_msgs << endl;

  m_msgs << "Tracked vehicles (" << m_vehicles.size() << "):" << endl;
  for(auto& kv : m_vehicles) {
    const VehicleState& vs = kv.second;
    double age   = m_curr_time - vs.timestamp;
    bool   stale = (age > 5.0);
    bool   moving = (vs.speed > m_speed_threshold);
    m_msgs << "  " << vs.name
           << (vs.friendly ? " [own]" : " [opp]")
           << (moving      ? " MOV"   : " STA")
           << (stale ? " STALE(" + doubleToStringX(age, 1) + "s)" : "")
           << "  lat=" << doubleToStringX(vs.lat, 6)
           << " lon="  << doubleToStringX(vs.lon, 6)
           << " sent=" << doubleToStringX(m_curr_time - vs.last_sent, 1)
           << "s ago" << endl;
  }

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
