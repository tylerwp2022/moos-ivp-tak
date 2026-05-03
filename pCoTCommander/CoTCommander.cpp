/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander.cpp                                */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include "CoTCommander.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTCommander::CoTCommander()
{
  m_geodesy_initialized    = false;
  m_waypoint_update_var    = "ATAK_WPT_UPDATE";
  m_capture_radius         = 15.0;
  m_operator_uid_filter    = "";
  m_enable_waypoint_control = true;
  m_enable_chat_commands   = true;
  m_command_chatroom       = "AQUATICUS-SHORE";
  m_fleet_mode             = true;
  m_debug                  = false;

  m_cot_received    = 0;
  m_cot_handled     = 0;
  m_cot_ignored     = 0;
  m_waypoint_commands = 0;
  m_chat_commands   = 0;
  m_last_command    = "none";

  m_last_sender_callsign = "";
  m_last_wpt_lat         = 0.0;
  m_last_wpt_lon         = 0.0;
  m_deployed             = false;
  m_wpt_reached_sent     = false;
}


// ============================================================
// debugLog()
// ============================================================

void CoTCommander::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTCommander::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
//
// ProcessConfig = pCoTCommander
// {
//   AppTick   = 10
//   CommsTick = 10
//
//   // Waypoint control
//   enable_waypoint_control = true
//   waypoint_update_var     = ATAK_WPT_UPDATE
//   capture_radius          = 15.0
//
//   // Optional: restrict to one ATAK device UID
//   // operator_uid_filter = ANDROID-abc123
//
//   // Geodesy — same options as pCoTBridge
//   use_nav_fallback = false
//
//   debug = false
// }
// ============================================================

bool CoTCommander::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug flag first
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
    else if(param == "enable_waypoint_control") {
      setBooleanOnString(m_enable_waypoint_control, value);
      debugLog("Config: enable_waypoint_control = " +
               boolToString(m_enable_waypoint_control));
    }
    else if(param == "waypoint_update_var") {
      string v = orig; biteStringX(v, '=');
      m_waypoint_update_var = stripBlankEnds(v);
      debugLog("Config: waypoint_update_var = " + m_waypoint_update_var);
    }
    else if(param == "capture_radius") {
      m_capture_radius = atof(value.c_str());
      debugLog("Config: capture_radius = " +
               doubleToStringX(m_capture_radius) + "m");
    }
    else if(param == "operator_uid_filter") {
      string v = orig; biteStringX(v, '=');
      m_operator_uid_filter = stripBlankEnds(v);
      debugLog("Config: operator_uid_filter = " + m_operator_uid_filter);
    }
    else if(param == "enable_chat_commands") {
      setBooleanOnString(m_enable_chat_commands, value);
      debugLog("Config: enable_chat_commands = " +
               boolToString(m_enable_chat_commands));
    }
    else if(param == "command_chatroom") {
      string v = orig; biteStringX(v, '=');
      m_command_chatroom = stripBlankEnds(v);
      debugLog("Config: command_chatroom = " + m_command_chatroom);
    }
    else if(param == "fleet_mode") {
      setBooleanOnString(m_fleet_mode, value);
      debugLog("Config: fleet_mode = " + boolToString(m_fleet_mode));
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

  // --------------------------------------------------------
  // Initialize geodesy from mission LatOrigin / LongOrigin
  // --------------------------------------------------------
  m_geodesy.setNavFallback(use_nav_fallback);

  double lat_origin = 0.0, lon_origin = 0.0;
  bool got_lat = m_MissionReader.GetValue("LatOrigin",  lat_origin);
  bool got_lon = m_MissionReader.GetValue("LongOrigin", lon_origin);

  if(got_lat && got_lon) {
    if(m_geodesy.initialise(lat_origin, lon_origin)) {
      m_geodesy_initialized = true;
      debugLog("OnStartUp: geodesy initialized [" +
               m_geodesy.getModeString() + "] origin=(" +
               doubleToStringX(lat_origin, 6) + "," +
               doubleToStringX(lon_origin, 6) + ")");
    }
    else {
      reportRunWarning("pCoTCommander: geodesy initialisation failed");
    }
  }
  else {
    // NAV fallback will be used — geodesy anchors from NODE_REPORT
    reportEvent("pCoTCommander: LatOrigin/LongOrigin not found — "
                "will use NAV anchor for coordinate conversion");
    debugLog("OnStartUp: no mission origin — waiting for NAV anchor");
  }

  // pCoTCommander is purely event-driven — all work happens in
  // OnNewMail(). Skip Iterate() ticks when no mail has arrived.
  // This eliminates unnecessary CPU usage vs a fixed tick rate.
  SetIterateMode(COMMS_DRIVEN_ITERATE_AND_MAIL);

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
// ============================================================

void CoTCommander::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();

  // Raw CoT XML from pCoTBridge
  Register("COT_INBOUND", 0);

  // GeoChat commands from ATAK operator via pCoTChat
  Register("ATAK_CHAT_IN", 0);

  // NODE_REPORT — for updating the geodesy NAV anchor.
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);

  // Posted by waypt_atak behavior endflag when waypoint is captured.
  // Triggers "waypoint reached" acknowledgment chat to operator.
  Register("ATAK_WPT_REACHED", 0);

  // Track deployment state — waypoints rejected if not deployed
  Register("DEPLOY", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTCommander::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key  = msg.m_sKey;
    string sval = msg.m_sVal;

    // --------------------------------------------------------
    // COT_INBOUND — raw CoT XML from pCoTBridge.
    // Dispatch to handler based on CoT type.
    // --------------------------------------------------------
    if(key == "COT_INBOUND") {
      m_cot_received++;
      debugLog("OnNewMail: COT_INBOUND (" +
               intToString((int)sval.size()) + " bytes)");
      if(!dispatchInboundCoT(sval))
        m_cot_ignored++;
    }

    // --------------------------------------------------------
    // ATAK_CHAT_IN — GeoChat command from ATAK operator.
    // Format: callsign=X,chatroom=Y,message=Z
    // Filtered on m_command_chatroom in handleChatCommand().
    // --------------------------------------------------------
    else if(key == "ATAK_CHAT_IN") {
      debugLog("OnNewMail: ATAK_CHAT_IN = " + sval);
      if(m_enable_chat_commands)
        handleChatCommand(sval);
    }

    // --------------------------------------------------------
    // NODE_REPORT — update geodesy NAV anchor.
    // Extract X, Y, LAT, LON to anchor flat-earth conversions
    // when MOOSGeodesy is unavailable.
    // --------------------------------------------------------
    else if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
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
          debugLog("OnNewMail: geodesy anchor set from NODE_REPORT — " +
                   m_geodesy.getModeString());
        }
      }
    }

    // --------------------------------------------------------
    // ATAK_WPT_REACHED — posted by waypt_atak endflag when
    // the robot captures the waypoint. Send "waypoint reached"
    // acknowledgment chat back to the operator who sent it.
    // --------------------------------------------------------
    // --------------------------------------------------------
    // DEPLOY — track deployment state.
    // Waypoints are rejected with a chat message if not deployed.
    // --------------------------------------------------------
    else if(key == "DEPLOY") {
      bool prev = m_deployed;
      setBooleanOnString(m_deployed, sval);
      if(m_deployed != prev)
        debugLog("OnNewMail: DEPLOY = " + boolToString(m_deployed));
    }

    else if(key == "ATAK_WPT_REACHED" && sval == "true") {
      // Only send once per waypoint — perpetual=true causes the
      // behavior to re-complete every tick while idling at the
      // capture point, firing this endflag repeatedly.
      if(m_wpt_reached_sent) {
        debugLog("OnNewMail: ATAK_WPT_REACHED — already sent, ignoring");
      }
      else {
        debugLog("OnNewMail: ATAK_WPT_REACHED — notifying operator");

        string chat_dest = m_last_sender_callsign.empty()
                           ? "All Chat Rooms"
                           : m_last_sender_callsign;

        Notify("ATAK_CHAT_OUT",
               "message=Waypoint reached.|chatroom=" + chat_dest);

        m_wpt_reached_sent = true;
        debugLog("OnNewMail: sent waypoint reached → chatroom=" + chat_dest);
      }

      // Reset flag so pHelmIvP doesn't see it as permanently true
      Notify("ATAK_WPT_REACHED", "false");
    }
  }

  return true;
}


// ============================================================
// Iterate()
// ============================================================

bool CoTCommander::Iterate()
{
  AppCastingMOOSApp::Iterate();
  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// extractAttr()
//
// Lightweight CoT XML attribute extractor.
// Scans for attr="value" or attr='value' in the XML string.
// No XML parser needed — CoT attributes are simple key=value.
// ============================================================

string CoTCommander::extractAttr(const std::string& xml,
                                  const std::string& attr)
{
  // Try double-quoted first, then single-quoted
  for(char quote : {'"', '\''}) {
    string search = attr + "=" + quote;
    size_t pos = xml.find(search);
    if(pos == string::npos) continue;

    size_t start = pos + search.size();
    size_t end   = xml.find(quote, start);
    if(end == string::npos) continue;

    return xml.substr(start, end - start);
  }
  return "";
}


// ============================================================
// dispatchInboundCoT()
//
// Extracts uid, type, lat, lon from the CoT XML and routes
// to the appropriate handler.
//
// Filtering:
//   - Skips malformed messages (missing required fields)
//   - Applies operator_uid_filter if configured
//   - Skips own echoes (uid starts with "surveyor-")
//
// Returns true if the message was handled, false if ignored.
//
// Adding a new command type:
//   1. Add an else-if block for the new type string
//   2. Add a handler method
//   3. No changes needed anywhere else
// ============================================================

bool CoTCommander::dispatchInboundCoT(const std::string& xml)
{
  string uid     = extractAttr(xml, "uid");
  string type    = extractAttr(xml, "type");
  string lat_str = extractAttr(xml, "lat");
  string lon_str = extractAttr(xml, "lon");

  if(uid.empty() || type.empty()) {
    debugLog("dispatchInboundCoT: missing uid or type — skipping");
    return false;
  }

  // Skip our own position CoT echoed back by the TAK server
  if(uid.find("surveyor-") == 0) {
    debugLog("dispatchInboundCoT: skipping own echo uid=" + uid);
    return false;
  }

  // Apply operator UID filter if configured.
  // The filter is a substring match against the event uid —
  // set it to the ATAK device UID prefix (e.g. "ANDROID-abc123")
  if(!m_operator_uid_filter.empty()) {
    if(uid.find(m_operator_uid_filter) == string::npos) {
      debugLog("dispatchInboundCoT: filtered uid=" + uid);
      return false;
    }
  }

  double lat = lat_str.empty() ? 0.0 : atof(lat_str.c_str());
  double lon = lon_str.empty() ? 0.0 : atof(lon_str.c_str());

  debugLog("dispatchInboundCoT: uid=" + uid +
           " type=" + type +
           " lat=" + doubleToStringX(lat, 6) +
           " lon=" + doubleToStringX(lon, 6));

  // ---- b-m-p-w-GOTO — ATAK "Go To" waypoint ----
  if(m_enable_waypoint_control && type == "b-m-p-w-GOTO") {
    if(lat_str.empty() || lon_str.empty()) {
      debugLog("dispatchInboundCoT: b-m-p-w-GOTO missing lat/lon — skipping");
      return false;
    }
    handleWaypointCoT(uid, lat, lon, xml);
    m_cot_handled++;
    return true;
  }

  // ---- Unhandled type — log and ignore ----
  // In debug mode this shows you what ATAK is sending so you
  // can decide whether to add a handler for it.
  debugLog("dispatchInboundCoT: unhandled type=" + type +
           " uid=" + uid);
  return false;
}


// ============================================================
// handleWaypointCoT()
//
// Handles a b-m-p-w-GOTO "Go To" waypoint from ATAK.
//
// Converts the CoT lat/lon to local XY via CoTGeodesy and
// publishes two MOOS variables:
//
//   ATAK_ACTIVE = true
//     Activates the waypt_atak behavior in pHelmIvP.
//     This behavior must be configured with:
//       condition = ATAK_ACTIVE = true
//       updates   = ATAK_WPT_UPDATE
//       pwt       = 150  (higher than survey, lower than const_speed)
//
//   ATAK_WPT_UPDATE = "points=x,y # capture_radius=r"
//     Updates the behavior's target point. The '#' separator
//     is MOOS-IvP's convention for multiple update params.
//     capture_radius is configurable via 'capture_radius' in .moos.
// ============================================================

void CoTCommander::handleWaypointCoT(const std::string& uid,
                                      double lat, double lon,
                                      const std::string& xml)
{
  // --------------------------------------------------------
  // Reject waypoint if robot is not deployed.
  // The operator must press Deploy in pMarineViewer first.
  // --------------------------------------------------------
  if(!m_deployed) {
    string sender    = extractAttr(xml, "parent_callsign");
    string chat_dest = sender.empty() ? "All Chat Rooms" : sender;
    Notify("ATAK_CHAT_OUT",
           "message=Deploy robots before sending waypoints.|chatroom=" +
           chat_dest);
    reportEvent("pCoTCommander: waypoint rejected — not deployed");
    debugLog("handleWaypointCoT: rejected — DEPLOY=false");
    return;
  }

  double x = 0.0, y = 0.0;

  // New waypoint — reset the reached guard so the next capture
  // triggers a fresh "waypoint reached" notification.
  m_wpt_reached_sent = false;

  if(!m_geodesy.latLonToLocalXY(lat, lon, x, y)) {
    reportRunWarning("pCoTCommander: waypoint received but geodesy not ready "
                     "— cannot convert lat/lon to local XY. "
                     "Waiting for NODE_REPORT to establish NAV anchor.");
    debugLog("handleWaypointCoT: FAILED — geodesy not ready");
    return;
  }

  // --------------------------------------------------------
  // Extract sender callsign from the <link parent_callsign>
  // element in the waypoint CoT. Used for the acknowledgment
  // DM back to the operator.
  //
  // Example CoT: <link uid="ANDROID-abc" parent_callsign="Tyler" .../>
  // --------------------------------------------------------
  string sender = extractAttr(xml, "parent_callsign");
  if(!sender.empty()) {
    m_last_sender_callsign = sender;
    debugLog("handleWaypointCoT: sender callsign = " + sender);
  }

  // Store lat/lon for the acknowledgment message
  m_last_wpt_lat = lat;
  m_last_wpt_lon = lon;

  string update = "points="             + doubleToStringX(x, 2)
                + ","                   + doubleToStringX(y, 2)
                + " # capture_radius=" + doubleToStringX(m_capture_radius, 1);

  Notify("ATAK_ACTIVE",         string("true"));
  Notify(m_waypoint_update_var, update);
  m_waypoint_commands++;

  // --------------------------------------------------------
  // Send acknowledgment chat back to the operator.
  // DM to whoever sent the waypoint (from parent_callsign).
  // Falls back to All Chat Rooms if sender unknown.
  // --------------------------------------------------------
  string chat_dest = sender.empty() ? "All Chat Rooms" : sender;

  // Format lat/lon to 5 decimal places — enough for ~1m precision
  string lat_str = doubleToStringX(lat, 5);
  string lon_str = doubleToStringX(lon, 5);

  Notify("ATAK_CHAT_OUT",
         "message=Waypoint received. Moving to " +
         lat_str + ", " + lon_str + ".|chatroom=" + chat_dest);

  m_last_command = "GOTO lat=" + doubleToStringX(lat, 6) +
                   " lon=" + doubleToStringX(lon, 6) +
                   " → x=" + doubleToStringX(x, 2) +
                   " y=" + doubleToStringX(y, 2) +
                   " sender=" + chat_dest;

  string event_msg = "pCoTCommander: waypoint → " +
                     m_waypoint_update_var + "=" + update +
                     " sender=" + chat_dest +
                     " (uid=" + uid + ")";
  reportEvent(event_msg);
  debugLog("handleWaypointCoT: " + event_msg);
}


// ============================================================
// buildReport()
// ============================================================

bool CoTCommander::buildReport()
{
  m_msgs << "Geodesy: " << m_geodesy.getModeString()
         << (m_geodesy_initialized ? " [ready]" : " [NOT READY]")
         << "  debug=" << boolToString(m_debug) << endl;
  m_msgs << endl;

  m_msgs << "COT_INBOUND: received=" << m_cot_received
         << "  handled=" << m_cot_handled
         << "  ignored=" << m_cot_ignored << endl;
  m_msgs << endl;

  m_msgs << "Commands dispatched:" << endl;
  m_msgs << "  waypoints=" << m_waypoint_commands
         << "  chat="      << m_chat_commands << endl;
  m_msgs << endl;

  m_msgs << "Last command: " << m_last_command << endl;

  if(!m_operator_uid_filter.empty())
    m_msgs << "UID filter:   " << m_operator_uid_filter << endl;

  m_msgs << "Chat mode:    " << (m_fleet_mode ? "fleet (_ALL)" : "vehicle (direct)")
         << "  chatroom=" << m_command_chatroom << endl;

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}



// ============================================================
// handleChatCommand()
//
// Parses ATAK_CHAT_IN = "callsign=X,chatroom=Y,message=Z"
// and dispatches fleet or per-vehicle commands when the
// chatroom matches m_command_chatroom.
//
// SUFFIX RESOLUTION (fleet mode):
//   Default sfx="_ALL" routes to all vehicles via uFldShoreBroker.
//   If the first word of the message is not a recognized command
//   keyword, it is treated as a vehicle name:
//     "blue_one deploy" → sfx="_BLUE_ONE" → DEPLOY_BLUE_ONE=true
//     "deploy"          → sfx="_ALL"      → DEPLOY_ALL=true
//     "blue_one attack" → sfx="_BLUE_ONE" → ACTION_BLUE_ONE=ATTACK_MED
//     "attack"          → sfx="_ALL"      → ACTION_ALL=ATTACK_MED
//   uFldShoreBroker's qbridge routes *_<VEHICLE> and *_ALL to the
//   appropriate vehicle MOOSDB as the bare variable name.
//
// VEHICLE MODE (fleet_mode=false):
//   sfx="" — posts directly on this vehicle's own MOOSDB.
//   Role commands use bare "attack|defend" (implicit vehicle).
//
// EXCEPTIONS:
//   play/stop are fleet-wide only (no per-vehicle game state).
//   status is always a reply-only query.
// ============================================================

void CoTCommander::handleChatCommand(const std::string& moos_val)
{
  // --------------------------------------------------------
  // Parse "callsign=X,chatroom=Y,message=Z"
  // message is always last and may contain commas.
  // --------------------------------------------------------
  string callsign, chatroom, message;

  size_t cr_pos  = moos_val.find(",chatroom=");
  size_t msg_pos = moos_val.find(",message=");
  if(cr_pos == string::npos || msg_pos == string::npos) {
    debugLog("handleChatCommand: malformed ATAK_CHAT_IN — " + moos_val);
    return;
  }

  size_t cs_pos = moos_val.find("callsign=");
  if(cs_pos != string::npos)
    callsign = moos_val.substr(cs_pos + 9, cr_pos - cs_pos - 9);

  chatroom = moos_val.substr(cr_pos  + 10, msg_pos - cr_pos  - 10);
  message  = moos_val.substr(msg_pos + 9);

  // --------------------------------------------------------
  // Only process messages directed at our command chatroom.
  // --------------------------------------------------------
  if(chatroom != m_command_chatroom) {
    debugLog("handleChatCommand: chatroom=" + chatroom +
             " != " + m_command_chatroom + " — ignored");
    return;
  }

  // Normalize: lowercase, strip surrounding whitespace
  string cmd = tolower(message);
  size_t f = cmd.find_first_not_of(" \t\r\n");
  if(f == string::npos) return;
  cmd = cmd.substr(f);
  size_t l = cmd.find_last_not_of(" \t\r\n");
  if(l != string::npos) cmd = cmd.substr(0, l + 1);

  debugLog("handleChatCommand: from=" + callsign +
           " chatroom=" + chatroom + " cmd=[" + cmd + "]");

  string reply_to = callsign.empty() ? "All Chat Rooms" : callsign;

  // --------------------------------------------------------
  // Resolve variable suffix and effective command.
  //
  // Fleet mode: sfx="_ALL" by default. If the first word is not
  // a recognized command keyword, treat it as a vehicle name and
  // set sfx="_<VEHICLE_UPPER>" for per-vehicle targeting.
  //
  // Vehicle mode: sfx="" — direct post on own MOOSDB.
  // --------------------------------------------------------
  string sfx = m_fleet_mode ? "_ALL" : "";

  if(m_fleet_mode) {
    // Keywords that are valid as the first (or only) word of a command.
    // Anything else is treated as a vehicle name prefix.
    static const set<string> cmd_keywords = {
      "deploy", "return", "rtb", "station", "hold", "pause",
      "play", "stop", "status", "attack", "defend", "help"
    };

    size_t space      = cmd.find(' ');
    string first_word = (space != string::npos) ? cmd.substr(0, space) : cmd;

    if(cmd_keywords.find(first_word) == cmd_keywords.end()) {
      // First word is not a command keyword — treat as vehicle name.
      if(space == string::npos) {
        Notify("ATAK_CHAT_OUT",
               "message=Unknown command. Use: deploy, return, station, "
               "pause, play, stop, status, attack, defend, or "
               "<vehicle> <command> (e.g. blue_one attack)."
               "|chatroom=" + reply_to);
        debugLog("handleChatCommand: unrecognized first word=" + first_word);
        return;
      }
      string vehicle = first_word;
      cmd = cmd.substr(space + 1);
      size_t rs = cmd.find_first_not_of(" \t");
      if(rs != string::npos) cmd = cmd.substr(rs);
      sfx = "_" + toupper(vehicle);
      debugLog("handleChatCommand: vehicle target=" + vehicle +
               " sfx=" + sfx + " cmd=" + cmd);
    }
  }

  // Human-readable target label for confirmation messages
  string target;
  if(sfx.empty())        target = "vehicle";
  else if(sfx == "_ALL") target = "all vehicles";
  else                   target = tolower(sfx.substr(1)); // e.g. "blue_one"

  // ========================================================
  // Deploy
  // ========================================================
  if(cmd == "deploy") {
    Notify("DEPLOY"               + sfx, "true");
    Notify("MOOS_MANUAL_OVERRIDE" + sfx, "false");
    Notify("RETURN"               + sfx, "false");
    Notify("ATAK_CHAT_OUT",
           "message=Deploying " + target + ".|chatroom=" + reply_to);
    m_last_command  = "DEPLOY" + sfx + " (from " + callsign + ")";
    m_chat_commands++;
    reportEvent("pCoTCommander: [CHAT] DEPLOY" + sfx +
                " from " + callsign);
  }

  // ========================================================
  // Return to base
  // ========================================================
  else if(cmd == "return" || cmd == "rtb") {
    Notify("DEPLOY"               + sfx, "true");
    Notify("MOOS_MANUAL_OVERRIDE" + sfx, "false");
    Notify("RETURN"               + sfx, "true");
    string verb = (sfx == "_ALL") ? "All vehicles returning"
                                  : (target + " returning");
    Notify("ATAK_CHAT_OUT",
           "message=" + verb + " to base.|chatroom=" + reply_to);
    m_last_command  = "RETURN" + sfx + " (from " + callsign + ")";
    m_chat_commands++;
    reportEvent("pCoTCommander: [CHAT] RETURN" + sfx +
                " from " + callsign);
  }

  // ========================================================
  // Station keep
  // ========================================================
  else if(cmd == "station" || cmd == "hold") {
    Notify("STATION_KEEP" + sfx, "true");
    string verb = (sfx == "_ALL") ? "All vehicles holding"
                                  : (target + " holding");
    Notify("ATAK_CHAT_OUT",
           "message=" + verb + " position.|chatroom=" + reply_to);
    m_last_command  = "STATION_KEEP" + sfx + " (from " + callsign + ")";
    m_chat_commands++;
    reportEvent("pCoTCommander: [CHAT] STATION_KEEP" + sfx +
                " from " + callsign);
  }

  // ========================================================
  // Pause (manual override)
  // ========================================================
  else if(cmd == "pause") {
    Notify("DEPLOY"               + sfx, "false");
    Notify("MOOS_MANUAL_OVERRIDE" + sfx, "true");
    string verb = (sfx == "_ALL") ? "All vehicles paused"
                                  : (target + " paused");
    Notify("ATAK_CHAT_OUT",
           "message=" + verb + ".|chatroom=" + reply_to);
    m_last_command  = "PAUSE / MOOS_MANUAL_OVERRIDE" + sfx +
                      " (from " + callsign + ")";
    m_chat_commands++;
    reportEvent("pCoTCommander: [CHAT] PAUSE" + sfx +
                " from " + callsign);
  }

  // ========================================================
  // Game control — fleet-wide only, no per-vehicle variant
  // ========================================================
  else if(cmd == "play" && m_fleet_mode) {
    if(sfx != "_ALL") {
      Notify("ATAK_CHAT_OUT",
             "message=Game control is fleet-wide only — "
             "omit vehicle name (just: play).|chatroom=" + reply_to);
    } else {
      Notify("AQUATICUS_GAME_ALL", "play");
      Notify("ATAK_CHAT_OUT",
             "message=Game started.|chatroom=" + reply_to);
      m_last_command  = "AQUATICUS_GAME_ALL=play (from " + callsign + ")";
      m_chat_commands++;
      reportEvent("pCoTCommander: [CHAT] PLAY from " + callsign);
    }
  }

  else if(cmd == "stop" && m_fleet_mode) {
    if(sfx != "_ALL") {
      Notify("ATAK_CHAT_OUT",
             "message=Game control is fleet-wide only — "
             "omit vehicle name (just: stop).|chatroom=" + reply_to);
    } else {
      Notify("AQUATICUS_GAME_ALL", "pause");
      Notify("ATAK_CHAT_OUT",
             "message=Game stopped.|chatroom=" + reply_to);
      m_last_command  = "AQUATICUS_GAME_ALL=pause (from " + callsign + ")";
      m_chat_commands++;
      reportEvent("pCoTCommander: [CHAT] STOP from " + callsign);
    }
  }

  // ========================================================
  // Status query — reply only, no MOOS posts
  // ========================================================
  else if(cmd == "status") {
    string status = string("Deployed: ") + (m_deployed ? "YES" : "NO");
    if(!m_fleet_mode)
      status = m_command_chatroom + " — " + status;
    Notify("ATAK_CHAT_OUT",
           "message=" + status + "|chatroom=" + reply_to);
    debugLog("handleChatCommand: status reply to " + reply_to);
  }

  // ========================================================
  // Help — list available commands
  // ========================================================
  else if(cmd == "help") {
    // Vehicle mode: only commands that apply to this vehicle.
    // Fleet mode: full set including per-vehicle prefix syntax.
    string help_msg;
    if(!m_fleet_mode) {
      help_msg =
        "Commands for this vehicle:&#10;"
        "deploy          - activate&#10;"
        "return / rtb    - return to base&#10;"
        "station / hold  - hold position&#10;"
        "pause           - manual override on&#10;"
        "atak            - enter ATAK operator control&#10;"
        "resume          - return to autonomous strategy&#10;"
        "status          - deployment state&#10;"
        "attack          - ATTACK_MED&#10;"
        "defend          - DEFEND_MED&#10;"
        "attack easy     - ATTACK_E&#10;"
        "defend easy     - DEFEND_E";
    } else {
      help_msg =
        "Commands (use underscores: blue_one, red_two):&#10;"
        "deploy          - activate vehicle(s)&#10;"
        "return / rtb    - send to base, exit ATAK mode&#10;"
        "station / hold  - hold position, exit ATAK mode&#10;"
        "pause           - manual override on, exit ATAK mode&#10;"
        "atak            - enter ATAK operator control&#10;"
        "resume          - exit ATAK mode, resume strategy&#10;"
        "play            - start game&#10;"
        "stop            - stop game&#10;"
        "status          - deployment state&#10;"
        "attack          - ATTACK_MED (all)&#10;"
        "defend          - DEFEND_MED (all)&#10;"
        "attack easy     - ATTACK_E (all)&#10;"
        "defend easy     - DEFEND_E (all)&#10;"
        "vehicle cmd     - target one vehicle&#10;"
        "  e.g. blue_one attack, red_two deploy";
    }
    Notify("ATAK_CHAT_OUT",
           "message=" + help_msg + "|chatroom=" + reply_to);
    debugLog("handleChatCommand: help reply to " + reply_to);
  }

  // ========================================================
  // Role assignment — ACTION[+sfx] = ATTACK/DEFEND_*
  // Works for all sfx variants: _ALL, _BLUE_ONE, or bare.
  // ========================================================
  else {
    string action_val;
    if     (cmd == "attack"      || cmd == "attack_med")  action_val = "ATTACK_MED";
    else if(cmd == "attack easy" || cmd == "attack_e")    action_val = "ATTACK_E";
    else if(cmd == "defend"      || cmd == "defend_med")  action_val = "DEFEND_MED";
    else if(cmd == "defend easy" || cmd == "defend_e")    action_val = "DEFEND_E";

    if(action_val.empty()) {
      string help = m_fleet_mode
        ? "message=Unknown command. Try: deploy, return, station, "
          "pause, play, stop, status, attack, defend, "
          "or <vehicle> <command> (e.g. blue_one attack)."
        : "message=Unknown command. Try: deploy, return, station, "
          "pause, status, attack, defend.";
      Notify("ATAK_CHAT_OUT", help + "|chatroom=" + reply_to);
      debugLog("handleChatCommand: unrecognized cmd=" + cmd);
      return;
    }

    string var_name = "ACTION" + sfx;  // ACTION_ALL, ACTION_BLUE_ONE, or ACTION
    Notify(var_name, action_val);
    Notify("ATAK_CHAT_OUT",
           "message=" + target + " -> " + action_val +
           ".|chatroom=" + reply_to);
    m_last_command  = var_name + "=" + action_val +
                      " (from " + callsign + ")";
    m_chat_commands++;
    reportEvent("pCoTCommander: [CHAT] " + var_name +
                "=" + action_val + " from " + callsign);
  }
}
