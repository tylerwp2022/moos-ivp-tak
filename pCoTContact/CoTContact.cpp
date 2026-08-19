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

  m_immediate          = false;
  m_affiliation        = "f";  // default: friendly
  m_affiliation_explicit = false;
  m_team_color         = "";   // default: no __group (map-only if unset)
  m_hostile_team_color = "";   // default: hostiles render as diamonds

  m_stealth_integration   = false;
  m_reveal_state_received = false;

  m_hide_tagged            = false;
  m_own_friendly           = true;
  m_contact_alerts         = false;
  m_contact_alert_duration = 3.0;
  m_cancel_repeat          = 5.0;

  m_debug              = false;
  m_pos_cot_sent       = 0;
  m_pos_cot_suppressed = 0;
  m_alert_cot_sent     = 0;
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
      m_affiliation = value;
      m_affiliation_explicit = true;
      if(m_affiliation != "f" && m_affiliation != "h" &&
         m_affiliation != "n" && m_affiliation != "u") {
        reportConfigWarning("pCoTContact: invalid affiliation '" +
                            m_affiliation + "' — use f/h/n/u. Defaulting to f.");
        m_affiliation = "f";
      }
      debugLog("Config: affiliation = " + m_affiliation);
    }
    else if(param == "team_color") {
      // Preserve case — ATAK team color names are capitalized
      string v = orig; biteStringX(v, '=');
      m_team_color = stripBlankEnds(v);
      debugLog("Config: team_color = " + m_team_color);
    }
    else if(param == "hostile_team_color") {
      // Preserve case — ATAK team color names are capitalized
      string v = orig; biteStringX(v, '=');
      m_hostile_team_color = stripBlankEnds(v);
      debugLog("Config: hostile_team_color = " + m_hostile_team_color);
    }
    else if(param == "immediate") {
      setBooleanOnString(m_immediate, value);
      debugLog("Config: immediate = " + boolToString(m_immediate));
    }
    else if(param == "own_role") {
      // Single-vehicle mode only: the vehicle's game role, so the
      // HVT features work on a boat that reports itself to TAK.
      //   friendly (default) — always reports; gets In Contact
      //     alerts; hidden by hide_tagged while tagged.
      //   hostile — hidden-group member: with stealth_integration,
      //     reports NO CoT unless HVT_REVEAL_STATE (bridged from
      //     the shoreside) lists it as revealed. Fail-safe: no
      //     state received = no leak. Uses hostile CoT symbology.
      string v = tolower(stripBlankEnds(value));
      if((v == "hostile") || (v == "red"))
        m_own_friendly = false;
      else if((v == "friendly") || (v == "blue"))
        m_own_friendly = true;
      else
        reportConfigWarning("pCoTContact: bad own_role: " + value);
      debugLog("Config: own_role = " +
               string(m_own_friendly ? "friendly" : "hostile"));
    }
    else if(param == "stealth_integration") {
      setBooleanOnString(m_stealth_integration, value);
      debugLog("Config: stealth_integration = " +
               boolToString(m_stealth_integration));
    }
    else if(param == "hide_tagged") {
      setBooleanOnString(m_hide_tagged, value);
      debugLog("Config: hide_tagged = " + boolToString(m_hide_tagged));
    }
    else if(param == "contact_alerts") {
      setBooleanOnString(m_contact_alerts, value);
      debugLog("Config: contact_alerts = " + boolToString(m_contact_alerts));
    }
    else if(param == "contact_alert_duration") {
      m_contact_alert_duration = atof(value.c_str());
      debugLog("Config: contact_alert_duration = " +
               doubleToStringX(m_contact_alert_duration) + "s");
    }
    else if(param == "contact_alert_cancel_repeat") {
      m_cancel_repeat = atof(value.c_str());
      debugLog("Config: contact_alert_cancel_repeat = " +
               doubleToStringX(m_cancel_repeat) + "s");
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
  if(m_stealth_integration)
    Register("HVT_REVEAL_STATE", 0);
  if(m_hide_tagged)
    Register("TAGGED_VEHICLES", 0);
  if(m_contact_alerts)
    Register("HVT_REVEAL_EVENT", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTContact::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key = msg.m_sKey;
    if(key == "HVT_REVEAL_STATE") {
      handleRevealState(msg.m_sVal);
    }
    else if(key == "TAGGED_VEHICLES") {
      handleTaggedVehicles(msg.m_sVal);
    }
    else if(key == "HVT_REVEAL_EVENT") {
      handleRevealEvent(msg.m_sVal);
    }
    else if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      if(!parseNodeReport(msg.m_sVal)) continue;
      if(!m_immediate) continue;
      string name;
      for(auto& tok : parseString(msg.m_sVal, ',')) {
        string t = tok;
        if(toupper(biteStringX(t, '=')) == "NAME") { name = t; break; }
      }
      auto it = m_vehicles.find(name);
      if(it != m_vehicles.end() && it->second.valid) {
        if(isHidden(name) || isTagSuppressed(name)) {
          m_pos_cot_suppressed++;
          continue;
        }
        Notify("COT_OUTBOUND", buildPositionCoT(it->second));
        it->second.last_sent = m_curr_time;
        m_pos_cot_sent++;
        debugLog("OnNewMail: immediate CoT sent for " + name);
      }
    }
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

    if(!m_immediate) {
      bool   moving   = (vs.speed > m_speed_threshold);
      double interval = moving ? m_moving_send_interval
                               : m_stationary_send_interval;
      if((m_curr_time - vs.last_sent) < interval) continue;
    }

    // Stealth/tagged: withhold this send slot; last_sent still advances
    // so the suppressed counter ticks at the send cadence, not AppTick.
    if(isHidden(vs.name) || isTagSuppressed(vs.name)) {
      vs.last_sent = m_curr_time;
      m_pos_cot_suppressed++;
      continue;
    }

    Notify("COT_OUTBOUND", buildPositionCoT(vs));
    vs.last_sent = m_curr_time;
    m_pos_cot_sent++;
    debugLog("Iterate: pos CoT sent for " + vs.name +
             string(m_immediate ? " [immediate]" :
                    (vs.speed > m_speed_threshold ? " [moving]" : " [static]")));
  }

  if(m_contact_alerts)
    processActiveAlerts();

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
    return m_own_friendly; // single-vehicle mode: own_role config
}


// ============================================================
// handleRevealState()
//
// Parses HVT_REVEAL_STATE from uFldNodeCommsHVT:
//   mode=3,red_one=hidden,red_two=revealed
// Vehicles listed are the hidden-group members; anything not
// listed is not subject to hiding and always reports.
// ============================================================

void CoTContact::handleRevealState(const std::string& spec)
{
  map<string, bool> new_map;
  for(auto& tok : parseString(spec, ',')) {
    string t   = tok;
    string key = tolower(biteStringX(t, '='));
    string val = tolower(stripBlankEnds(t));
    if(key == "mode") continue;
    new_map[key] = (val == "hidden");
  }

  for(auto& kv : new_map) {
    auto it = m_hidden_map.find(kv.first);
    if(it == m_hidden_map.end() || it->second != kv.second)
      debugLog("RevealState: " + kv.first + " -> " +
               string(kv.second ? "hidden" : "revealed"));
  }

  m_hidden_map = new_map;
  m_reveal_state_received = true;
}


// ============================================================
// isHidden()
//
// True if the vehicle's CoT should be withheld from TAK.
// Only ever true when stealth_integration=true.
//
// A hostile not (yet) listed in HVT_REVEAL_STATE is treated as
// hidden. The hidden roster builds up as vehicles' node reports
// reach uFldNodeCommsHVT's ledger, so during startup a hostile
// may be tracked here before it appears in the reveal state —
// defaulting to visible would leak it to TAK in that window.
// Consequence: stealth_integration assumes the hidden group
// covers the hostile vehicles (true for the HVT missions).
// ============================================================

bool CoTContact::isHidden(const std::string& name) const
{
  if(!m_stealth_integration)
    return false;

  auto it = m_hidden_map.find(tolower(name));
  if(it != m_hidden_map.end())
    return it->second;

  return !isFriendly(name);
}


// ============================================================
// handleTaggedVehicles()
//
// Parses TAGGED_VEHICLES from uFldTagManager — a comma list of
// currently tagged vehicle names, republished on every change
// (empty string when nobody is tagged).
// ============================================================

void CoTContact::handleTaggedVehicles(const std::string& val)
{
  set<string> new_set;
  for(auto& tok : parseString(val, ',')) {
    string vname = tolower(stripBlankEnds(tok));
    if(!vname.empty())
      new_set.insert(vname);
  }

  for(auto& vname : new_set)
    if(m_tagged_set.count(vname) == 0)
      debugLog("Tagged: " + vname);
  for(auto& vname : m_tagged_set)
    if(new_set.count(vname) == 0)
      debugLog("Untagged: " + vname);

  m_tagged_set = new_set;
}


// ============================================================
// isTagSuppressed()
//
// True if the vehicle's CoT should be withheld because it has
// been tagged/exploded. Applies to friendly vehicles only —
// hostile visibility is governed by the stealth integration.
// ============================================================

bool CoTContact::isTagSuppressed(const std::string& name) const
{
  if(!m_hide_tagged)
    return false;
  if(!isFriendly(name))
    return false;
  return (m_tagged_set.count(tolower(name)) > 0);
}


// ============================================================
// handleRevealEvent()
//
// Parses HVT_REVEAL_EVENT from uFldNodeCommsHVT:
//   vname=red_one,observer=blue_one,why=seen by blue_one at 179.8m
// A discovery puts the whole friendly team "In Contact": raises
// an alert on every tracked friendly vehicle for
// contact_alert_duration seconds. processActiveAlerts() keeps
// each alert at its boat's position and cancels it on expiry.
// ============================================================

void CoTContact::handleRevealEvent(const std::string& spec)
{
  string observer;
  for(auto& tok : parseString(spec, ',')) {
    string t = tok;
    if(tolower(biteStringX(t, '=')) == "observer") {
      observer = stripBlankEnds(t);
      break;
    }
  }

  unsigned int raised = 0;
  for(auto& kv : m_vehicles) {
    VehicleState& vs = kv.second;
    if(!vs.valid || !vs.friendly)
      continue;
    if(m_alert_until.find(vs.name) == m_alert_until.end()) {
      Notify("COT_OUTBOUND", buildAlertCoT(vs));
      m_alert_cot_sent++;
    }
    m_alert_until[vs.name] = m_curr_time + m_contact_alert_duration;
    // A pending cancel resend would clear the fresh alert (same uid).
    m_cancel_until.erase(vs.name);
    raised++;
  }

  debugLog("RevealEvent: In Contact (by " +
           (observer.empty() ? string("?") : observer) + ") — " +
           intToString(raised) + " alerts raised");
}


// ============================================================
// processActiveAlerts()
//
// Called each Iterate. Active alerts are re-sent at the boat's
// current position (same uid, so ATAK moves the alert with the
// boat). Expired alerts get an explicit cancel CoT — ATAK keeps
// emergency alerts on screen until cancelled.
// ============================================================

void CoTContact::processActiveAlerts()
{
  for(auto it = m_alert_until.begin(); it != m_alert_until.end(); ) {
    auto vit = m_vehicles.find(it->first);
    if(vit == m_vehicles.end()) {
      it = m_alert_until.erase(it);
      continue;
    }
    // A boat that went invalid (stale, tagged) still has its alert on
    // TAK screens — cancel at the last known position rather than
    // dropping the entry with the alert stranded.
    if(!vit->second.valid || m_curr_time >= it->second) {
      Notify("COT_OUTBOUND", buildAlertCancelCoT(vit->second));
      debugLog("Alert cancelled for " + it->first);
      m_cancel_until[it->first] = m_curr_time + m_cancel_repeat;
      it = m_alert_until.erase(it);
    }
    else {
      Notify("COT_OUTBOUND", buildAlertCoT(vit->second));
      ++it;
    }
  }

  // Keep re-sending each cancel until its window closes — see
  // m_cancel_repeat in the header.
  for(auto it = m_cancel_until.begin(); it != m_cancel_until.end(); ) {
    if(m_curr_time >= it->second) {
      it = m_cancel_until.erase(it);
      continue;
    }
    auto vit = m_vehicles.find(it->first);
    if(vit != m_vehicles.end())
      Notify("COT_OUTBOUND", buildAlertCancelCoT(vit->second));
    ++it;
  }
}


// ============================================================
// buildAlertCoT()
//
// ATAK "In Contact" emergency alert (b-a-o-opn) at the boat's
// current position. The <link> uid references the vehicle's SA
// contact (surveyor-<name>) so ATAK associates the alert with
// the plotted contact. Re-sends reuse the same uid
// (<name>-9-1-1), so the alert follows the boat. The stale time
// is only a fallback — removal is via buildAlertCancelCoT().
// ============================================================

string CoTContact::buildAlertCoT(const VehicleState& vs)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_contact_alert_duration + 10.0);

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""   + vs.name + "-9-1-1\""
      " type=\"b-a-o-opn\""
      " how=\"h-e\""
      " time=\""  + t_now   + "\""
      " start=\"" + t_now   + "\""
      " stale=\"" + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(vs.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vs.lon, 7) + "\""
      " hae=\"0.0\""
      " ce=\"9999999.0\" le=\"9999999.0\"/>"
    "<detail>"
      "<contact callsign=\"" + vs.name + "-Alert\"/>"
      "<link uid=\"surveyor-" + vs.name + "\""
           " type=\"a-f-S-C-U-N\" relation=\"p-p\"/>"
      "<emergency type=\"In Contact\">" + vs.name + "</emergency>"
    "</detail>"
    "</event>";
}


// ============================================================
// buildAlertCancelCoT()
//
// Emergency cancel (b-a-o-can) for the vehicle's In Contact
// alert. Same uid as the alert; <emergency cancel="true"> is
// what actually removes the alert from ATAK.
// ============================================================

string CoTContact::buildAlertCancelCoT(const VehicleState& vs)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, 300.0);

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""   + vs.name + "-9-1-1\""
      " type=\"b-a-o-can\""
      " how=\"h-e\""
      " time=\""  + t_now   + "\""
      " start=\"" + t_now   + "\""
      " stale=\"" + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(vs.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vs.lon, 7) + "\""
      " hae=\"0.0\""
      " ce=\"9999999.0\" le=\"9999999.0\"/>"
    "<detail>"
      "<emergency cancel=\"true\">" + vs.name + "</emergency>"
    "</detail>"
    "</event>";
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
  string name, vsource;
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
    else if(key == "VSOURCE") { vsource = tolower(val); }
  }

  if(!got_name || !got_lat || !got_lon || !got_hdg || !got_spd || !got_time)
    return false;

  // Never echo a TAK-origin track back to TAK. Matters when a
  // pCoTTrack vname_map posts an ATAK operator under a mission
  // vehicle name (e.g. blue_four) that is in our roster — the
  // operator's own ATAK self-marker is already on every screen.
  if(vsource == "pcottrack") {
    debugLog("parseNodeReport: ignoring " + name + " (TAK-origin track)");
    return false;
  }

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
  // In single-vehicle mode, an explicit affiliation config wins;
  // otherwise it follows own_role (hostile boats report a-h-...).
  string affil;
  if(m_multi_mode)
    affil = vs.friendly ? "f" : "h";
  else if(m_affiliation_explicit)
    affil = m_affiliation;
  else
    affil = m_own_friendly ? "f" : "h";

  string cot_type = "a-" + affil + "-S-C-U-N";
  string uid      = "surveyor-" + vs.name;

  string t_now   = formatCoTTime(vs.timestamp, 0.0);
  string t_stale = formatCoTTime(vs.timestamp, m_cot_stale_offset);

  // Friendlies use team_color, hostiles use hostile_team_color.
  // ATAK's __group detail overrides type-based rendering, so an
  // empty color = no __group = the contact renders from its CoT
  // type (hostile diamond for a-h) and is map-only.
  string color = (affil == "f") ? m_team_color : m_hostile_team_color;
  string group_elem = "";
  if(!color.empty())
    group_elem = "<__group name=\"" + color + "\" role=\"Team Member\"/>";

  // Map-only contacts get no endpoint — an endpoint would list
  // them as messageable in ATAK's contacts list.
  string endpoint_attr = group_elem.empty() ? "" : " endpoint=\"*:-1:stcp\"";

  string detail =
    "<detail>"
      "<contact callsign=\"" + vs.name + "\"" + endpoint_attr + "/>"
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

string CoTContact::formatCoTTime(double /*moos_time*/, double offset)
{
  // Real wall-clock UTC, deliberately NOT the passed MOOS time:
  // under sim time warp MOOSTime runs decades fast (a warp-3
  // vehicle community stamps year-2139 events), and TAK clients
  // silently discard events with far-future times.
  time_t t = time(0) + (time_t)offset;
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
  if(m_stealth_integration)
    m_msgs << "Stealth integration: ON ("
           << (m_reveal_state_received ? "reveal state received"
                                       : "awaiting HVT_REVEAL_STATE — "
                                         "hostiles withheld")
           << ")" << endl;
  if(m_hide_tagged) {
    m_msgs << "Hide tagged: ON  tagged={";
    string sep = "";
    for(auto& vname : m_tagged_set) {
      m_msgs << sep << vname;
      sep = ",";
    }
    m_msgs << "}" << endl;
  }
  if(m_contact_alerts) {
    m_msgs << "Contact alerts: ON  raised: " << m_alert_cot_sent
           << "  (duration " << doubleToStringX(m_contact_alert_duration, 0)
           << "s)  active={";
    string sep = "";
    for(auto& kv : m_alert_until) {
      m_msgs << sep << kv.first;
      sep = ",";
    }
    m_msgs << "}" << endl;
  }
  m_msgs << "CoT sent: " << m_pos_cot_sent;
  if(m_stealth_integration || m_hide_tagged)
    m_msgs << "  suppressed: " << m_pos_cot_suppressed;
  m_msgs << endl;
  m_msgs << endl;

  m_msgs << "Tracked vehicles (" << m_vehicles.size() << "):" << endl;
  for(auto& kv : m_vehicles) {
    const VehicleState& vs = kv.second;
    double age   = m_curr_time - vs.timestamp;
    bool   stale = (age > 5.0);
    bool   moving = (vs.speed > m_speed_threshold);
    m_msgs << "  " << vs.name
           << (vs.friendly ? " [own]" : " [opp]")
           << (isHidden(vs.name)       ? " [HIDDEN]" : "")
           << (isTagSuppressed(vs.name) ? " [TAGGED]" : "")
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
