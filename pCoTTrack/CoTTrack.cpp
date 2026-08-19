/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTTrack.cpp                                    */
/*    DATE: August 2026                                     */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include "CoTTrack.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTTrack::CoTTrack()
{
  m_node_prefix     = "atak_";
  m_node_type       = "swimmer";   // pMarineViewer body closest to a person
  m_node_group      = "";

  // ATAK's team palette folded onto the two game teams. Only colors an
  // operator would plausibly pick to mean red/blue are mapped; anything
  // else (green, yellow, white...) falls back to node_group.
  m_group_from_team        = true;
  m_team_map["red"]        = "red";
  m_team_map["maroon"]     = "red";
  m_team_map["magenta"]    = "red";
  m_team_map["blue"]       = "blue";
  m_team_map["dark blue"]  = "blue";
  m_team_map["cyan"]       = "blue";
  m_team_map["teal"]       = "blue";
  m_node_color      = "yellow";
  m_node_length     = 2.0;
  m_lowercase_names = true;

  // Default ingest set — ground UNIT PLI in all four affiliations.
  // Prefix matching means a-f-G-U also claims a-f-G-U-C-I etc.
  m_track_types.push_back("a-f-G-U");
  m_track_types.push_back("a-h-G-U");
  m_track_types.push_back("a-n-G-U");
  m_track_types.push_back("a-u-G-U");

  // pCoTContact stamps every contact it sends with this prefix.
  m_ignore_uids.push_back("surveyor-");

  m_max_tracks = 20;

  m_publish_node_report = true;
  m_publish_view_marker = false;
  m_marker_type         = "triangle";
  m_marker_color        = "yellow";
  m_marker_width        = 6.0;

  m_post_interval = 0.0;    // post on every accepted event
  m_stale_timeout = 30.0;

  m_derive_motion_only = false;

  m_debug = false;

  m_cot_received   = 0;
  m_cot_accepted   = 0;
  m_cot_loopback   = 0;
  m_cot_not_listed = 0;
  m_reports_posted = 0;
  m_tracks_dropped = 0;
  m_geo_failures   = 0;
}


// ============================================================
// debugLog()
// ============================================================

void CoTTrack::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTTrack::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
// ============================================================

bool CoTTrack::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug first, so config parsing itself can be logged
  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

  // Track whether the operator overrode a list-valued param. The first
  // explicit entry clears the built-in default rather than appending to
  // it — otherwise "track_cot_types = a-f-G-U-C" would silently keep
  // accepting the three other affiliations too.
  bool types_overridden   = false;
  bool ignore_overridden  = false;
  bool teammap_overridden = false;

  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  for(auto& orig : sParams) {
    string line  = tolower(orig);
    string param = biteStringX(line, '=');
    string value = line;
    bool handled = true;

    if(param == "debug") {
      setBooleanOnString(m_debug, value);
    }
    else if(param == "node_prefix") {
      string v = orig; biteStringX(v, '=');
      m_node_prefix = stripBlankEnds(v);
      debugLog("Config: node_prefix = " + m_node_prefix);
    }
    else if(param == "node_type") {
      m_node_type = value;
      debugLog("Config: node_type = " + m_node_type);
    }
    else if(param == "node_group") {
      m_node_group = value;
      debugLog("Config: node_group = " + m_node_group);
    }
    else if(param == "group_from_team") {
      setBooleanOnString(m_group_from_team, value);
      debugLog("Config: group_from_team = " + boolToString(m_group_from_team));
    }
    else if(param == "team_map") {
      // e.g. team_map = red:red, maroon:red, cyan:blue, dark blue:blue
      if(!teammap_overridden) { m_team_map.clear(); teammap_overridden = true; }
      string log_str;
      for(auto& pair : parseString(value, ',')) {
        string color = stripBlankEnds(biteStringX(pair, ':'));
        string team  = stripBlankEnds(pair);
        if(color.empty() || team.empty()) continue;
        m_team_map[color] = team;
        log_str += color + ">" + team + " ";
      }
      debugLog("Config: team_map = { " + log_str + "}");
    }
    else if(param == "vname_map") {
      // e.g. vname_map = bark:blue_four — that operator's track posts
      // under the exact vehicle name (no node_prefix), so an ATAK user
      // can play a standard mission vehicle such as the HVT.
      string log_str;
      for(auto& pair : parseString(value, ',')) {
        string cs    = stripBlankEnds(biteStringX(pair, ':'));
        string vname = stripBlankEnds(pair);
        if(cs.empty() || vname.empty()) continue;
        m_vname_map[cs] = vname;
        log_str += cs + ">" + vname + " ";
      }
      debugLog("Config: vname_map = { " + log_str + "}");
    }
    else if(param == "node_color") {
      m_node_color = value;
      debugLog("Config: node_color = " + m_node_color);
    }
    else if(param == "node_length") {
      m_node_length = atof(value.c_str());
      debugLog("Config: node_length = " + doubleToStringX(m_node_length, 1));
    }
    else if(param == "lowercase_names") {
      setBooleanOnString(m_lowercase_names, value);
      debugLog("Config: lowercase_names = " + boolToString(m_lowercase_names));
    }
    else if(param == "track_cot_types") {
      // Case matters — CoT types are case sensitive (a-f-G-U-C).
      string v = orig; biteStringX(v, '=');
      if(!types_overridden) { m_track_types.clear(); types_overridden = true; }
      string log_str;
      for(auto& t : parseString(v, ',')) {
        string trimmed = stripBlankEnds(t);
        if(trimmed.empty()) continue;
        m_track_types.push_back(trimmed);
        log_str += trimmed + " ";
      }
      debugLog("Config: track_cot_types = { " + log_str + "}");
    }
    else if(param == "ignore_uid_prefix") {
      string v = orig; biteStringX(v, '=');
      if(!ignore_overridden) { m_ignore_uids.clear(); ignore_overridden = true; }
      string log_str;
      for(auto& u : parseString(v, ',')) {
        string trimmed = stripBlankEnds(u);
        if(trimmed.empty()) continue;
        m_ignore_uids.push_back(trimmed);
        log_str += trimmed + " ";
      }
      debugLog("Config: ignore_uid_prefix = { " + log_str + "}");
    }
    else if(param == "ignore_callsign") {
      string v = orig; biteStringX(v, '=');
      string log_str;
      for(auto& c : parseString(v, ',')) {
        string trimmed = stripBlankEnds(c);
        if(trimmed.empty()) continue;
        m_ignore_calls.push_back(trimmed);
        log_str += trimmed + " ";
      }
      debugLog("Config: ignore_callsign = { " + log_str + "}");
    }
    else if(param == "callsign_whitelist") {
      // Stored lowercase — ATAK callsigns are free text an operator types
      // on a phone, so "Delta 1" must match "delta 1".
      string log_str;
      for(auto& c : parseString(value, ',')) {
        string trimmed = stripBlankEnds(c);
        if(trimmed.empty()) continue;
        m_allow_calls.push_back(trimmed);
        log_str += trimmed + " ";
      }
      debugLog("Config: callsign_whitelist = { " + log_str + "}");
    }
    else if(param == "max_tracks") {
      int v = atoi(value.c_str());
      if(v > 0) m_max_tracks = (unsigned int)v;
      debugLog("Config: max_tracks = " + intToString((int)m_max_tracks));
    }
    else if(param == "publish_node_report") {
      setBooleanOnString(m_publish_node_report, value);
      debugLog("Config: publish_node_report = " +
               boolToString(m_publish_node_report));
    }
    else if(param == "publish_view_marker") {
      setBooleanOnString(m_publish_view_marker, value);
      debugLog("Config: publish_view_marker = " +
               boolToString(m_publish_view_marker));
    }
    else if(param == "marker_type") {
      m_marker_type = value;
      debugLog("Config: marker_type = " + m_marker_type);
    }
    else if(param == "marker_color") {
      m_marker_color = value;
      debugLog("Config: marker_color = " + m_marker_color);
    }
    else if(param == "marker_width") {
      m_marker_width = atof(value.c_str());
      debugLog("Config: marker_width = " + doubleToStringX(m_marker_width, 1));
    }
    else if(param == "post_interval") {
      m_post_interval = atof(value.c_str());
      debugLog("Config: post_interval = " +
               doubleToStringX(m_post_interval, 2) + "s");
    }
    else if(param == "stale_timeout") {
      m_stale_timeout = atof(value.c_str());
      debugLog("Config: stale_timeout = " +
               doubleToStringX(m_stale_timeout, 1) + "s");
    }
    else if(param == "derive_motion_only") {
      setBooleanOnString(m_derive_motion_only, value);
      debugLog("Config: derive_motion_only = " +
               boolToString(m_derive_motion_only));
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // --------------------------------------------------------
  // Geodesy — same LatOrigin/LongOrigin the rest of the
  // mission uses. Required: inbound CoT carries lat/lon and
  // NODE_REPORT needs X/Y in the mission's local grid.
  // --------------------------------------------------------
  double lat_origin = 0.0, lon_origin = 0.0;
  bool got_lat = m_MissionReader.GetValue("LatOrigin",  lat_origin);
  bool got_lon = m_MissionReader.GetValue("LongOrigin", lon_origin);

  if(got_lat && got_lon) {
    if(m_geodesy.initialise(lat_origin, lon_origin))
      debugLog("Geodesy initialised at " +
               doubleToStringX(lat_origin, 6) + ", " +
               doubleToStringX(lon_origin, 6));
    else
      reportConfigWarning("pCoTTrack: geodesy init failed for LatOrigin/LongOrigin");
  }
  else {
    reportConfigWarning("pCoTTrack: LatOrigin/LongOrigin not found — "
                        "falling back to NAV anchor from NODE_REPORT");
  }

  if(m_track_types.empty())
    reportConfigWarning("pCoTTrack: track_cot_types is empty — "
                        "no inbound CoT will ever be accepted");

  if(!m_publish_node_report && !m_publish_view_marker)
    reportConfigWarning("pCoTTrack: both publish_node_report and "
                        "publish_view_marker are false — app will do nothing");

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
//
// NODE_REPORT is subscribed for the geodesy NAV anchor only —
// it is the one place a simultaneous X/Y and LAT/LON pair is
// available, which is what the flat-earth fallback needs when
// MOOSGeodesy is not compiled in.
// ============================================================

void CoTTrack::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("COT_INBOUND",       0);
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTTrack::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key = msg.m_sKey;

    if(key == "COT_INBOUND") {
      m_cot_received++;
      handleInboundCoT(msg.m_sVal);
    }
    else if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      updateNavAnchor(msg.m_sVal);
    }
  }

  return true;
}


// ============================================================
// Iterate()
//
// Two jobs: expire tracks that have gone quiet, and re-post
// NODE_REPORTs for the rest.
//
// Re-posting matters even when nothing new arrives. Consumers
// downstream (uFldNodeComms, pContactMgrV20) judge freshness by
// NODE_REPORT arrival, so a track that only posted on inbound
// CoT would be declared stale during any lull in the ATAK
// client's reporting. Posting each iteration with a current
// TIME keeps the contact alive right up until stale_timeout
// deliberately drops it.
// ============================================================

bool CoTTrack::Iterate()
{
  AppCastingMOOSApp::Iterate();

  vector<string> expired;

  for(auto& kv : m_tracks) {
    TakTrack& tr = kv.second;

    if((m_curr_time - tr.last_rx) > m_stale_timeout) {
      expired.push_back(kv.first);
      continue;
    }

    if(!tr.xy_valid) continue;

    if((m_curr_time - tr.last_post) < m_post_interval) continue;

    postTrack(tr);
  }

  for(auto& uid : expired) {
    auto it = m_tracks.find(uid);
    if(it == m_tracks.end()) continue;
    retractMarker(it->second);
    debugLog("Iterate: dropped stale track " + it->second.node_name +
             " (" + doubleToStringX(m_curr_time - it->second.last_rx, 1) +
             "s since last CoT)");
    m_tracks.erase(it);
    m_tracks_dropped++;
  }

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// handleInboundCoT()
//
// Parses one CoT event off COT_INBOUND and, if it is a ground
// unit PLI that survives the loopback filters, creates or
// updates the corresponding TakTrack.
//
// Returns true if the event was accepted.
// ============================================================

bool CoTTrack::handleInboundCoT(const std::string& xml)
{
  string type = extractElemAttr(xml, "event", "type");
  if(type.empty()) return false;

  // Cheap filter first — most inbound traffic is not PLI.
  if(!matchesTrackType(type))
    return false;

  string uid = extractElemAttr(xml, "event", "uid");
  if(uid.empty()) {
    debugLog("handleInboundCoT: PLI with no uid — dropped");
    return false;
  }

  if(isIgnoredUid(uid)) {
    m_cot_loopback++;
    debugLog("handleInboundCoT: loopback uid dropped: " + uid);
    return false;
  }

  // Position is mandatory — a PLI without one is not usable as a contact.
  string lat_s = extractElemAttr(xml, "point", "lat");
  string lon_s = extractElemAttr(xml, "point", "lon");
  if(lat_s.empty() || lon_s.empty()) {
    debugLog("handleInboundCoT: " + uid + " has no point lat/lon — dropped");
    return false;
  }

  double lat = atof(lat_s.c_str());
  double lon = atof(lon_s.c_str());

  // Reject the null island / unset-GPS case. ATAK clients without a fix
  // sometimes emit 0,0, which would otherwise plant a contact in the
  // Gulf of Guinea and drag every range calculation with it.
  if((lat == 0.0) && (lon == 0.0)) {
    debugLog("handleInboundCoT: " + uid + " reported 0,0 (no GPS fix) — dropped");
    return false;
  }

  // Callsign is what the operator sees; fall back to the uid if absent.
  string callsign = extractElemAttr(xml, "contact", "callsign");
  if(callsign.empty()) callsign = uid;

  if(isIgnoredCallsign(callsign)) {
    m_cot_loopback++;
    debugLog("handleInboundCoT: ignored callsign dropped: " + callsign);
    return false;
  }

  if(!isWhitelistedCallsign(callsign)) {
    m_cot_not_listed++;
    debugLog("handleInboundCoT: callsign not on whitelist — dropped: " +
             callsign);
    return false;
  }

  // New track? Enforce the cap before inserting.
  bool is_new = (m_tracks.find(uid) == m_tracks.end());
  if(is_new && (m_tracks.size() >= m_max_tracks)) {
    reportRunWarning("pCoTTrack: max_tracks (" +
                     intToString((int)m_max_tracks) +
                     ") reached — ignoring new track " + callsign);
    return false;
  }

  TakTrack& tr = m_tracks[uid];

  if(is_new) {
    tr.uid      = uid;
    tr.first_rx = m_curr_time;
    debugLog("handleInboundCoT: new track " + callsign + " uid=" + uid);
  }

  // Roll the previous fix forward before overwriting — deriveMotion
  // needs both endpoints.
  if(tr.xy_valid) {
    tr.prev_x     = tr.x;
    tr.prev_y     = tr.y;
    tr.prev_rx    = tr.last_rx;
    tr.prev_valid = true;
  }

  // A callsign change on a known uid is the operator renaming themselves
  // in ATAK. Keeping the uid as the key means the track survives, but the
  // MOOS node name changes — which downstream looks like the old node
  // vanishing and a new one appearing. Worth surfacing, not worth blocking.
  string new_name = m_node_prefix + sanitizeName(callsign);
  auto vm = m_vname_map.find(tolower(stripBlankEnds(callsign)));
  if(vm != m_vname_map.end())
    new_name = vm->second;
  if(!is_new && (new_name != tr.node_name)) {
    reportEvent("pCoTTrack: " + tr.node_name + " renamed to " + new_name);
    retractMarker(tr);
  }

  tr.callsign  = callsign;
  tr.node_name = new_name;
  tr.cot_type  = type;
  tr.lat       = lat;
  tr.lon       = lon;
  tr.last_rx   = m_curr_time;
  tr.updates++;

  string hae_s = extractElemAttr(xml, "point", "hae");
  if(!hae_s.empty()) {
    double hae = atof(hae_s.c_str());
    // 9999999 is the CoT "unknown" sentinel — do not propagate it.
    if(hae < 9999999.0) tr.hae = hae;
  }

  tr.team_color = extractElemAttr(xml, "__group", "name");
  tr.cot_time   = parseCoTTime(extractElemAttr(xml, "event", "time"));

  // <track speed course> when the client supplies it, derived otherwise.
  // derive_motion_only skips the element entirely — some clients report a
  // GPS course that is stale or noisy at walking speed, and deriving from
  // fixes is then the more trustworthy source.
  tr.motion_from_cot = false;
  if(!m_derive_motion_only) {
    string spd_s = extractElemAttr(xml, "track", "speed");
    string crs_s = extractElemAttr(xml, "track", "course");
    if(!spd_s.empty() && !crs_s.empty()) {
      tr.speed           = atof(spd_s.c_str());
      tr.course          = atof(crs_s.c_str());
      tr.motion_from_cot = true;
    }
  }

  // lat/lon → local grid
  double x = 0.0, y = 0.0;
  if(m_geodesy.latLonToLocalXY(lat, lon, x, y)) {
    tr.x        = x;
    tr.y        = y;
    tr.xy_valid = true;
  }
  else {
    m_geo_failures++;
    debugLog("handleInboundCoT: geodesy not ready — " + callsign +
             " held without XY");
  }

  if(!tr.motion_from_cot)
    deriveMotion(tr);

  m_cot_accepted++;

  debugLog("handleInboundCoT: " + tr.node_name +
           " lat=" + doubleToStringX(lat, 6) +
           " lon=" + doubleToStringX(lon, 6) +
           " x="   + doubleToStringX(tr.x, 1) +
           " y="   + doubleToStringX(tr.y, 1));

  // post_interval == 0 means "as fast as it arrives" — post here rather
  // than waiting for the next Iterate, so an operator's movement shows in
  // pMarineViewer without a tick of latency.
  if(tr.xy_valid && (m_post_interval <= 0.0))
    postTrack(tr);

  return true;
}


// ============================================================
// deriveMotion()
//
// Estimates speed and course from the last two fixes, for
// clients that omit <track>. Without this an ATAK user shows a
// permanent heading of 0 and never triggers a speed-dependent
// behavior.
//
// Skipped when the two fixes are too close in time (noise gets
// amplified into an absurd speed) or the movement is inside GPS
// jitter — in that case the operator is standing still and the
// previous course is the better answer than a random one.
// ============================================================

void CoTTrack::deriveMotion(TakTrack& tr)
{
  if(!tr.prev_valid || !tr.xy_valid) return;

  double dt = tr.last_rx - tr.prev_rx;
  if(dt < 0.5) return;          // too short to be meaningful

  double dx = tr.x - tr.prev_x;
  double dy = tr.y - tr.prev_y;
  double dist = hypot(dx, dy);

  if(dist < 1.0) {              // inside handheld GPS noise
    tr.speed = 0.0;
    return;                     // keep the previous course
  }

  tr.speed = dist / dt;

  // MOOS heading: degrees clockwise from north (+Y), so atan2(dx, dy).
  double hdg = atan2(dx, dy) * 180.0 / M_PI;
  if(hdg < 0.0) hdg += 360.0;
  tr.course = hdg;
}


// ============================================================
// postTrack()
//
// TakTrack → NODE_REPORT (and optionally VIEW_MARKER).
//
// TIME uses the local MOOS clock, NOT the CoT event time.
// Downstream consumers compare NODE_REPORT TIME against their
// own MOOSTime to judge staleness; an Android device's clock —
// even NTP-synced, and certainly under a simulated MOOS time
// base — is a different reference. Using it would make contacts
// read as permanently stale or impossibly fresh. The sender's
// clock is kept in TakTrack::cot_time for the AppCast only.
// ============================================================

void CoTTrack::postTrack(TakTrack& tr)
{
  if(m_publish_node_report) {
    string report;
    report += "NAME="   + tr.node_name;
    report += ",TYPE="  + m_node_type;
    report += ",TIME="  + doubleToStringX(m_curr_time, 2);
    report += ",X="     + doubleToStringX(tr.x, 2);
    report += ",Y="     + doubleToStringX(tr.y, 2);
    report += ",LAT="   + doubleToStringX(tr.lat, 6);
    report += ",LON="   + doubleToStringX(tr.lon, 6);
    report += ",SPD="   + doubleToStringX(tr.speed, 2);
    report += ",HDG="   + doubleToStringX(tr.course, 1);
    report += ",DEP=0";
    report += ",LENGTH=" + doubleToStringX(m_node_length, 1);
    report += ",MODE=TAK";
    report += ",VSOURCE=pCoTTrack";

    if(!m_node_color.empty())
      report += ",COLOR=" + m_node_color;

    // GROUP is a team name to uFldTagManager and a comms filter to
    // uFldNodeComms (apply_groups=true). Omitted when neither the team
    // mapping nor node_group yields one.
    string group = groupForTrack(tr);
    if(!group.empty())
      report += ",GROUP=" + group;

    Notify("NODE_REPORT", report);
    m_reports_posted++;
  }

  if(m_publish_view_marker) {
    string marker;
    marker += "x="       + doubleToStringX(tr.x, 2);
    marker += ",y="      + doubleToStringX(tr.y, 2);
    marker += ",label="  + tr.node_name;
    marker += ",type="   + m_marker_type;
    marker += ",color="  + m_marker_color;
    marker += ",width="  + doubleToStringX(m_marker_width, 1);
    Notify("VIEW_MARKER", marker);
  }

  tr.last_post = m_curr_time;
}


// ============================================================
// retractMarker()
//
// Erases a marker from pMarineViewer. Markers are keyed by
// label and persist until explicitly deactivated, so a dropped
// track would otherwise leave its icon behind forever.
//
// NODE_REPORT needs no equivalent — pMarineViewer and
// pContactMgrV20 age nodes out on their own once the reports
// stop arriving.
// ============================================================

void CoTTrack::retractMarker(const TakTrack& tr)
{
  if(!m_publish_view_marker) return;
  if(tr.node_name.empty())   return;
  Notify("VIEW_MARKER", "x=0,y=0,active=false,label=" + tr.node_name);
}


// ============================================================
// updateNavAnchor()
//
// Feeds lib_cot_geodesy a simultaneous XY/LatLon pair so the
// flat-earth fallback has a reference point when MOOSGeodesy
// is unavailable.
//
// Reports produced by this app are skipped: their lat/lon was
// computed from the anchor in the first place, so re-anchoring
// on them would close a loop that slowly walks the reference
// point away from ground truth.
// ============================================================

void CoTTrack::updateNavAnchor(const std::string& report)
{
  bool got_x = false, got_y = false, got_lat = false, got_lon = false;
  double x = 0, y = 0, lat = 0, lon = 0;
  string name;

  for(auto& tok : parseString(report, ',')) {
    string t   = tok;
    string key = toupper(biteStringX(t, '='));
    if     (key == "NAME") { name = t;                }
    else if(key == "X")    { x   = atof(t.c_str()); got_x   = true; }
    else if(key == "Y")    { y   = atof(t.c_str()); got_y   = true; }
    else if(key == "LAT")  { lat = atof(t.c_str()); got_lat = true; }
    else if(key == "LON")  { lon = atof(t.c_str()); got_lon = true; }
  }

  if(!got_x || !got_y || !got_lat || !got_lon)
    return;

  if(!m_node_prefix.empty() && strBegins(name, m_node_prefix))
    return;   // one of ours — do not anchor on it

  m_geodesy.updateNavAnchor(x, y, lat, lon);
}


// ============================================================
// matchesTrackType()
//
// Prefix match against track_cot_types. Prefix rather than
// exact so a single entry claims the whole MIL-STD 2525C
// subtree beneath it: "a-f-G-U" covers a-f-G-U-C (combat),
// a-f-G-U-C-I (infantry), a-f-G-U-T (tactical), and so on.
// ============================================================

bool CoTTrack::matchesTrackType(const std::string& type) const
{
  for(const auto& prefix : m_track_types) {
    if(type.compare(0, prefix.size(), prefix) == 0)
      return true;
  }
  return false;
}


// ============================================================
// isIgnoredUid()
// ============================================================

bool CoTTrack::isIgnoredUid(const std::string& uid) const
{
  for(const auto& prefix : m_ignore_uids) {
    if(uid.compare(0, prefix.size(), prefix) == 0)
      return true;
  }
  return false;
}


// ============================================================
// isIgnoredCallsign()
// ============================================================

bool CoTTrack::isIgnoredCallsign(const std::string& cs) const
{
  for(const auto& c : m_ignore_calls) {
    if(cs == c) return true;
  }
  return false;
}


// ============================================================
// isWhitelistedCallsign()
//
// An empty whitelist means the feature is off and every
// callsign is admitted — the pre-whitelist behavior. When set,
// only listed callsigns pass, compared case-insensitively
// (entries are stored lowercase at config time).
// ============================================================

bool CoTTrack::isWhitelistedCallsign(const std::string& cs) const
{
  if(m_allow_calls.empty()) return true;

  string lc = tolower(cs);
  for(const auto& c : m_allow_calls) {
    if(lc == c) return true;
  }
  return false;
}


// ============================================================
// sanitizeName()
//
// ATAK callsigns are free text. NODE_REPORT is a comma/equals
// delimited format and MOOS variable suffixes are built by
// upper-casing node names, so anything outside [A-Za-z0-9_-]
// becomes an underscore. Runs are collapsed and the ends
// trimmed so "Delta 1 (TL)" reads as "delta_1_tl", not
// "delta_1__tl_".
// ============================================================

std::string CoTTrack::sanitizeName(const std::string& raw) const
{
  string out;
  bool last_us = false;

  for(char c : raw) {
    bool ok = ((c >= 'a') && (c <= 'z')) ||
              ((c >= 'A') && (c <= 'Z')) ||
              ((c >= '0') && (c <= '9')) ||
              (c == '_') || (c == '-');
    if(ok) {
      out += c;
      last_us = false;
    }
    else if(!last_us) {
      out += '_';
      last_us = true;
    }
  }

  while(!out.empty() && (out[0] == '_'))
    out.erase(0, 1);
  while(!out.empty() && (out[out.size()-1] == '_'))
    out.erase(out.size()-1);

  if(out.empty()) out = "unnamed";

  return m_lowercase_names ? tolower(out) : out;
}


// ============================================================
// groupForTrack()
//
// The operator's ATAK team color, folded onto a game team via
// team_map. Looked up per-post rather than cached so an operator
// who switches teams mid-game moves to the new team on their
// next report. Unmapped colors fall back to node_group — never
// the raw color, which uFldTagManager would reject as an
// unknown team on every single report.
// ============================================================

std::string CoTTrack::groupForTrack(const TakTrack& tr) const
{
  if(m_group_from_team && !tr.team_color.empty()) {
    auto it = m_team_map.find(tolower(tr.team_color));
    if(it != m_team_map.end())
      return it->second;
  }
  return m_node_group;
}


// ============================================================
// extractAttr()
//
// Returns the value of the first attr="..." in the string.
// Same logic as cot::extractAttr in pCoTCommander — duplicated
// rather than shared because each pCoT* app is self-contained
// except for lib_cot_geodesy.
// ============================================================

std::string CoTTrack::extractAttr(const std::string& xml,
                                   const std::string& attr)
{
  string needle = attr + "=";
  size_t pos = xml.find(needle);
  if(pos == string::npos) return "";

  pos += needle.size();
  if(pos >= xml.size()) return "";

  char quote = xml[pos];
  if((quote != '\'') && (quote != '"')) return "";   // malformed

  size_t start = pos + 1;
  size_t end   = xml.find(quote, start);
  if(end == string::npos) return "";

  return xml.substr(start, end - start);
}


// ============================================================
// extractElemAttr()
//
// Returns attr from within a specific element's opening tag.
//
// Scoping to the element matters here in a way it does not for
// pCoTCommander's flat extractAttr. A PLI carries "name" on
// <__group> and again on other detail children, and both
// <point> and <takv> can carry attributes with overlapping
// names — an unscoped search returns whichever appears first in
// the document, which is not necessarily the one asked for.
// ============================================================

std::string CoTTrack::extractElemAttr(const std::string& xml,
                                       const std::string& elem,
                                       const std::string& attr)
{
  string open = "<" + elem;
  size_t pos  = xml.find(open);

  while(pos != string::npos) {
    // Reject a longer element name that merely starts with the one
    // requested (<track> vs <tracklog>) and keep looking, rather than
    // giving up — the real element may still be further along.
    size_t after = pos + open.size();
    bool   exact = true;
    if(after < xml.size()) {
      char c = xml[after];
      if((c != ' ') && (c != '>') && (c != '/') && (c != '\t') &&
         (c != '\n') && (c != '\r'))
        exact = false;
    }

    if(exact) {
      size_t close = xml.find('>', pos);
      if(close == string::npos) return "";
      return extractAttr(xml.substr(pos, close - pos + 1), attr);
    }

    pos = xml.find(open, pos + 1);
  }

  return "";
}


// ============================================================
// parseCoTTime()
//
// ISO8601 UTC ("2026-08-03T14:22:31.123Z") → epoch seconds.
// Diagnostic use only — see the note in postTrack about why
// this never reaches NODE_REPORT.
// ============================================================

double CoTTrack::parseCoTTime(const std::string& iso)
{
  if(iso.empty()) return 0.0;

  int    yr = 0, mo = 0, dy = 0, hr = 0, mn = 0;
  double sc = 0.0;

  if(sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%lf",
            &yr, &mo, &dy, &hr, &mn, &sc) != 6)
    return 0.0;

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = yr - 1900;
  tmv.tm_mon  = mo - 1;
  tmv.tm_mday = dy;
  tmv.tm_hour = hr;
  tmv.tm_min  = mn;
  tmv.tm_sec  = (int)sc;

  // timegm interprets the struct as UTC. CoT timestamps are always UTC
  // (trailing Z), so mktime — which assumes local time — would be wrong
  // by the host's timezone offset.
  time_t t = timegm(&tmv);
  if(t == (time_t)-1) return 0.0;

  return (double)t + (sc - (double)((int)sc));
}


// ============================================================
// buildReport()
// ============================================================

bool CoTTrack::buildReport()
{
  m_msgs << "Geodesy: " << m_geodesy.getModeString() << endl;

  string outputs;
  if(m_publish_node_report) outputs += "NODE_REPORT ";
  if(m_publish_view_marker) outputs += "VIEW_MARKER ";
  if(outputs.empty())       outputs = "(none)";
  m_msgs << "Publishing: " << outputs
         << "  node_prefix=" << m_node_prefix
         << "  type=" << m_node_type << endl;

  string types;
  for(const auto& t : m_track_types) types += t + " ";
  m_msgs << "Ingest types: " << types << endl;

  m_msgs << "Motion source: "
         << (m_derive_motion_only ? "derived from fixes only"
                                  : "CoT <track> when present, else derived")
         << endl;

  if(m_allow_calls.empty()) {
    m_msgs << "Callsign whitelist: (off — all callsigns admitted)" << endl;
  }
  else {
    string calls;
    for(const auto& c : m_allow_calls) calls += c + " ";
    m_msgs << "Callsign whitelist: " << calls << endl;
  }
  m_msgs << endl;

  m_msgs << "CoT inbound seen: " << m_cot_received
         << "   accepted: "      << m_cot_accepted
         << "   loopback dropped: " << m_cot_loopback
         << "   not whitelisted: " << m_cot_not_listed << endl;
  m_msgs << "Reports posted: "  << m_reports_posted
         << "   tracks dropped (stale): " << m_tracks_dropped
         << "   geodesy failures: " << m_geo_failures << endl;
  m_msgs << endl;

  m_msgs << "TAK tracks (" << m_tracks.size()
         << "/" << m_max_tracks << "):" << endl;

  if(m_tracks.empty()) {
    m_msgs << "  <none — no ground-unit PLI received yet>" << endl;
  }

  for(auto& kv : m_tracks) {
    const TakTrack& tr = kv.second;
    double age = m_curr_time - tr.last_rx;

    string group = groupForTrack(tr);
    string team_str;
    if(!tr.team_color.empty() || !group.empty()) {
      team_str = "  team=" + (tr.team_color.empty() ? "-" : tr.team_color);
      team_str += ">" + (group.empty() ? "(no group)" : group);
    }

    m_msgs << "  " << tr.node_name
           << "  [" << tr.callsign << "]"
           << "  " << tr.cot_type
           << team_str << endl;
    m_msgs << "      x="   << doubleToStringX(tr.x, 1)
           << " y="        << doubleToStringX(tr.y, 1)
           << "  lat="     << doubleToStringX(tr.lat, 6)
           << " lon="      << doubleToStringX(tr.lon, 6)
           << (tr.xy_valid ? "" : "  [NO XY]") << endl;
    m_msgs << "      spd=" << doubleToStringX(tr.speed, 2)
           << " hdg="      << doubleToStringX(tr.course, 1)
           << (tr.motion_from_cot ? " (cot)" : " (derived)")
           << "  age="     << doubleToStringX(age, 1) << "s"
           << "  updates=" << tr.updates << endl;
  }

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
