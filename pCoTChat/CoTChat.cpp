/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTChat.cpp                                     */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <algorithm>
#include "CoTChat.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTChat::CoTChat()
{
  m_own_callsign = "";
  m_own_uid      = "";
  m_nav_lat      = 0.0;
  m_nav_lon      = 0.0;
  m_nav_valid    = false;
  m_echo_filter  = true;
  m_debug        = false;

  m_chat_in_received  = 0;
  m_chat_in_published = 0;
  m_chat_out_sent     = 0;
  m_contacts_tracked  = 0;

  // Known TAK team colors
  m_team_colors = {"Cyan","Red","Blue","Green","Yellow",
                   "Orange","Purple","Maroon","White","Dark Blue"};

  // Known TAK roles — extend via config if needed
  m_roles = {"HQ","Team Lead","K9","Forward Observer",
             "Sniper","Medic","RTO","XO","Ops","Intel"};
}


// ============================================================
// debugLog()
// ============================================================

void CoTChat::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTChat::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
//
// ProcessConfig = pCoTChat
// {
//   AppTick   = 4
//   CommsTick = 10
//
//   own_callsign = alpha
//   own_uid      = surveyor-alpha   // optional, default = surveyor-{callsign}
//
//   echo_filter  = true     // suppress own messages echoed back from TAK
//   debug        = false
// }
// ============================================================

bool CoTChat::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  for(auto& orig : sParams) {
    string line  = tolower(orig);
    string param = biteStringX(line, '=');
    string value = line;
    bool handled = true;

    if(param == "debug") {
      setBooleanOnString(m_debug, value);
    }
    else if(param == "own_callsign") {
      string v = orig; biteStringX(v, '=');
      m_own_callsign = stripBlankEnds(v);
      debugLog("Config: own_callsign = " + m_own_callsign);
    }
    else if(param == "own_uid") {
      string v = orig; biteStringX(v, '=');
      m_own_uid = stripBlankEnds(v);
      debugLog("Config: own_uid = " + m_own_uid);
    }
    else if(param == "echo_filter") {
      setBooleanOnString(m_echo_filter, value);
      debugLog("Config: echo_filter = " + boolToString(m_echo_filter));
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Default UID if not explicitly configured
  if(m_own_uid.empty() && !m_own_callsign.empty())
    m_own_uid = "surveyor-" + m_own_callsign;

  if(m_own_callsign.empty())
    reportConfigWarning("pCoTChat: own_callsign not set — "
                        "outbound messages will have empty sender");

  // Event-driven — all work in OnNewMail
  SetIterateMode(COMMS_DRIVEN_ITERATE_AND_MAIL);

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
// ============================================================

void CoTChat::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("COT_INBOUND",    0);
  Register("ATAK_CHAT_OUT",  0);
  Register("NODE_REPORT",    0);
  Register("NODE_REPORT_LOCAL", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTChat::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key  = msg.m_sKey;
    string sval = msg.m_sVal;

    // --------------------------------------------------------
    // COT_INBOUND — raw CoT XML from pCoTBridge.
    // Two jobs: update contact table, handle inbound chat.
    // --------------------------------------------------------
    if(key == "COT_INBOUND") {
      string type = extractAttr(sval, "type");

      // SA contact — update callsign→UID table
      if((type.size() >= 3 && type.substr(0, 3) == "a-f") ||
         (type.size() >= 3 && type.substr(0, 3) == "a-h")) {
        updateContactTable(sval);
      }

      // GeoChat — parse inbound message
      if(type == "b-t-f")
        handleInboundChat(sval);
    }

    // --------------------------------------------------------
    // ATAK_CHAT_OUT — outbound message request from MOOS.
    // Format: message=hello team,chatroom=All Chat Rooms
    // --------------------------------------------------------
    else if(key == "ATAK_CHAT_OUT") {
      debugLog("OnNewMail: ATAK_CHAT_OUT = " + sval);
      handleOutboundChat(sval);
    }

    // --------------------------------------------------------
    // NODE_REPORT — own vehicle position for outbound CoT
    // --------------------------------------------------------
    else if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      vector<string> tokens = parseString(sval, ',');
      for(auto& tok : tokens) {
        string t = tok;
        string k = toupper(biteStringX(t, '='));
        string v = t;
        if     (k == "LAT") { m_nav_lat = atof(v.c_str()); m_nav_valid = true; }
        else if(k == "LON") { m_nav_lon = atof(v.c_str()); }
      }
    }
  }

  return true;
}


// ============================================================
// Iterate() — nothing to do, event-driven
// ============================================================

bool CoTChat::Iterate()
{
  AppCastingMOOSApp::Iterate();
  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// extractAttr()
// ============================================================

string CoTChat::extractAttr(const std::string& xml,
                             const std::string& attr)
{
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
// extractTagContent()
//
// Extracts text content between opening and closing tags.
// Example: extractTagContent(xml, "remarks") → "test"
// Handles <remarks ...>content</remarks>
// ============================================================

string CoTChat::extractTagContent(const std::string& xml,
                                   const std::string& tag)
{
  // Find opening tag (may have attributes)
  string open_search = "<" + tag;
  size_t open_pos = xml.find(open_search);
  if(open_pos == string::npos) return "";

  // Find end of opening tag
  size_t tag_end = xml.find(">", open_pos);
  if(tag_end == string::npos) return "";

  // Self-closing tag?
  if(xml[tag_end - 1] == '/') return "";

  size_t content_start = tag_end + 1;
  string close_tag = "</" + tag + ">";
  size_t close_pos = xml.find(close_tag, content_start);
  if(close_pos == string::npos) return "";

  return xml.substr(content_start, close_pos - content_start);
}


// ============================================================
// updateContactTable()
//
// Parses a SA contact CoT (a-f-* or a-h-*) and updates the
// callsign→UID lookup table. Two sources for callsign:
//   <contact callsign="X"/> — standard contact element
//   <uid Droid="X"/>        — fallback if contact missing
// ============================================================

void CoTChat::updateContactTable(const std::string& xml)
{
  string uid      = extractAttr(xml, "uid");
  string callsign = extractAttr(xml, "callsign");

  // Fallback to Droid attribute if contact callsign missing
  if(callsign.empty())
    callsign = extractAttr(xml, "Droid");

  if(uid.empty() || callsign.empty()) return;

  // Skip own vehicle
  if(uid == m_own_uid || callsign == m_own_callsign) return;

  bool is_new = (m_contacts_by_callsign.find(callsign) ==
                 m_contacts_by_callsign.end());

  ContactInfo ci;
  ci.uid       = uid;
  ci.callsign  = callsign;
  ci.last_seen = m_curr_time;

  m_contacts_by_callsign[callsign] = ci;
  m_contacts_by_uid[uid]           = ci;

  if(is_new) {
    m_contacts_tracked++;
    debugLog("updateContactTable: new contact " + callsign +
             " uid=" + uid);
  }
}


// ============================================================
// handleInboundChat()
//
// Parses a b-t-f GeoChat CoT and publishes to ATAK_CHAT_IN.
// Extracts:
//   senderCallsign — from <__chat senderCallsign="X">
//   chatroom       — from <__chat chatroom="Y">
//   message        — from <remarks>content</remarks>
//
// Echo filter: skip messages from own_uid or own_callsign.
// ============================================================

void CoTChat::handleInboundChat(const std::string& xml)
{
  m_chat_in_received++;

  string sender   = extractAttr(xml, "senderCallsign");
  string chatroom = extractAttr(xml, "chatroom");
  string message  = extractTagContent(xml, "remarks");

  if(sender.empty() || message.empty()) {
    debugLog("handleInboundChat: missing sender or message — skipping");
    return;
  }

  // Echo filter — skip own messages echoed back from TAK server
  if(m_echo_filter) {
    if(sender == m_own_callsign) {
      debugLog("handleInboundChat: echo filter — skipping own message");
      return;
    }
  }

  // Build ATAK_CHAT_IN string
  string chat_in = "callsign=" + sender +
                   ",chatroom=" + chatroom +
                   ",message="  + message;

  Notify("ATAK_CHAT_IN", chat_in);
  m_chat_in_published++;

  reportEvent("pCoTChat: [IN] " + sender +
              " → " + chatroom + ": " + message);
  debugLog("handleInboundChat: published ATAK_CHAT_IN = " + chat_in);
}


// ============================================================
// handleOutboundChat()
//
// Parses ATAK_CHAT_OUT = "message=X,chatroom=Y" and builds
// the appropriate GeoChat CoT based on destination type.
// ============================================================

void CoTChat::handleOutboundChat(const std::string& moos_val)
{
  string message  = "";
  string chatroom = "All Chat Rooms";

  // --------------------------------------------------------
  // Parse ATAK_CHAT_OUT using '|' as the field separator.
  // '|' is used instead of ',' because message content can
  // freely contain commas (e.g. coordinates, lists).
  //
  // Format: message=<text with any chars except |>|chatroom=<dest>
  // --------------------------------------------------------

  string chatroom_key = "|chatroom=";
  size_t cr_pos = moos_val.find(chatroom_key);
  if(cr_pos != string::npos) {
    chatroom = moos_val.substr(cr_pos + chatroom_key.size());
  }

  string msg_key = "message=";
  size_t msg_pos = moos_val.find(msg_key);
  if(msg_pos != string::npos) {
    size_t msg_start = msg_pos + msg_key.size();
    size_t msg_end   = (cr_pos != string::npos) ? cr_pos : moos_val.size();
    message = moos_val.substr(msg_start, msg_end - msg_start);
  }

  if(message.empty()) {
    reportRunWarning("pCoTChat: ATAK_CHAT_OUT missing message field — " +
                     moos_val);
    return;
  }

  ChatDestType dest = resolveDestType(chatroom);
  string cot;

  switch(dest) {
    case CHAT_ALL_ROOMS:   cot = buildAllRoomsCoT(message);               break;
    case CHAT_ALL_GROUPS:  cot = buildAllGroupsCoT(message);              break;
    case CHAT_ALL_TEAMS:   cot = buildAllTeamsCoT(message);               break;
    case CHAT_TEAM_COLOR:  cot = buildTeamColorCoT(chatroom, message);    break;
    case CHAT_ROLE:        cot = buildRoleCoT(chatroom, message);         break;
    case CHAT_GROUP:       cot = buildGroupCoT(chatroom, message);        break;
    case CHAT_DIRECT:      cot = buildDirectCoT(chatroom, message);       break;
  }

  if(cot.empty()) {
    reportRunWarning("pCoTChat: failed to build CoT for chatroom=" + chatroom);
    return;
  }

  Notify("COT_OUTBOUND", cot);
  m_chat_out_sent++;

  reportEvent("pCoTChat: [OUT] → " + chatroom + ": " + message);
  debugLog("handleOutboundChat: published COT_OUTBOUND (" +
           intToString((int)cot.size()) + " bytes)");
}


// ============================================================
// resolveDestType()
//
// Maps a chatroom string to the appropriate ChatDestType.
// Checked in order of specificity:
//   1. Exact keyword matches (All Chat Rooms, Groups, Teams)
//   2. Known team colors
//   3. Known roles
//   4. Known group names (from m_groups table)
//   5. Fallback: direct message to callsign
// ============================================================

ChatDestType CoTChat::resolveDestType(const std::string& chatroom) const
{
  if(chatroom == "All Chat Rooms")
    return CHAT_ALL_ROOMS;
  if(chatroom == "Groups" || chatroom == "UserGroups")
    return CHAT_ALL_GROUPS;
  if(chatroom == "Teams" || chatroom == "TeamGroups")
    return CHAT_ALL_TEAMS;
  if(m_team_colors.count(chatroom))
    return CHAT_TEAM_COLOR;
  if(m_roles.count(chatroom))
    return CHAT_ROLE;
  if(m_groups.count(chatroom))
    return CHAT_GROUP;
  return CHAT_DIRECT;
}


// ============================================================
// generateMsgId()
//
// Generates a simple unique message ID from current time.
// Not a true UUID but sufficient for TAK message deduplication.
// ============================================================

string CoTChat::generateMsgId()
{
  time_t t = (time_t)m_curr_time;
  struct tm* utc = gmtime(&t);
  char buf[64];
  // Format: YYYYMMDDHHMMSS-{milliseconds}
  strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", utc);
  int ms = (int)((m_curr_time - (int)m_curr_time) * 1000);
  return string(buf) + "-" + intToString(ms);
}


// ============================================================
// formatCoTTime()
// ============================================================

string CoTChat::formatCoTTime(double moos_time, double offset)
{
  time_t t = (time_t)(moos_time + offset);
  struct tm* utc = gmtime(&t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
  return string(buf);
}


// ============================================================
// assembleCoT()
//
// Builds the full <event> wrapper for a GeoChat message.
// All chat CoT types share the same outer structure:
//   type="b-t-f", how="h-g-i-g-o", 24hr stale
//   <point> uses own vehicle position if available
// ============================================================

string CoTChat::assembleCoT(const std::string& uid,
                              const std::string& detail,
                              const std::string& chatroom)
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, 86400.0); // 24hr

  double lat = m_nav_valid ? m_nav_lat : 0.0;
  double lon = m_nav_valid ? m_nav_lon : 0.0;

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event version=\"2.0\""
      " uid=\""   + uid     + "\""
      " type=\"b-t-f\""
      " how=\"h-g-i-g-o\""
      " time=\""  + t_now   + "\""
      " start=\"" + t_now   + "\""
      " stale=\"" + t_stale + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\"" + doubleToStringX(lat, 7) + "\""
      " lon=\"" + doubleToStringX(lon, 7) + "\""
      " hae=\"0\" ce=\"9999999.0\" le=\"9999999.0\"/>"
    + detail +
    "</event>";
}


// ============================================================
// buildAllRoomsCoT()
// chatroom="All Chat Rooms", parent="RootContactGroup"
// ============================================================

string CoTChat::buildAllRoomsCoT(const std::string& message)
{
  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + ".All Chat Rooms." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"RootContactGroup\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id           + "\""
        " chatroom=\"All Chat Rooms\""
        " id=\"All Chat Rooms\""
        " senderCallsign=\"" + m_own_callsign   + "\">"
        "<chatgrp"
          " uid0=\"" + m_own_uid + "\""
          " uid1=\"All Chat Rooms\""
          " id=\"All Chat Rooms\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " to=\"All Chat Rooms\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, "All Chat Rooms");
}


// ============================================================
// buildAllGroupsCoT()
// chatroom="Groups", id="UserGroups", parent="RootContactGroup"
// ============================================================

string CoTChat::buildAllGroupsCoT(const std::string& message)
{
  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + ".UserGroups." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"RootContactGroup\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\"Groups\""
        " id=\"UserGroups\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp uid0=\"" + m_own_uid + "\" id=\"UserGroups\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, "Groups");
}


// ============================================================
// buildAllTeamsCoT()
// chatroom="Teams", id="TeamGroups", parent="RootContactGroup"
// ============================================================

string CoTChat::buildAllTeamsCoT(const std::string& message)
{
  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + ".TeamGroups." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"RootContactGroup\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\"Teams\""
        " id=\"TeamGroups\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp uid0=\"" + m_own_uid + "\" id=\"TeamGroups\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, "Teams");
}


// ============================================================
// buildTeamColorCoT()
// chatroom=color (e.g. "Cyan"), parent="TeamGroups"
// ============================================================

string CoTChat::buildTeamColorCoT(const std::string& chatroom,
                                   const std::string& message)
{
  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + "." + chatroom + "." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"TeamGroups\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\""       + chatroom       + "\""
        " id=\""             + chatroom       + "\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp uid0=\"" + m_own_uid + "\" id=\"" + chatroom + "\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, chatroom);
}


// ============================================================
// buildRoleCoT()
// chatroom=role (e.g. "HQ"), parent="RootContactGroup"
// ============================================================

string CoTChat::buildRoleCoT(const std::string& chatroom,
                               const std::string& message)
{
  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + "." + chatroom + "." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"RootContactGroup\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\""       + chatroom       + "\""
        " id=\""             + chatroom       + "\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp uid0=\"" + m_own_uid + "\" id=\"" + chatroom + "\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, chatroom);
}


// ============================================================
// buildGroupCoT()
//
// Named group message with <hierarchy> block.
// Looks up member UIDs from m_contacts_by_callsign and
// m_groups table to build the chatgrp and hierarchy elements.
// ============================================================

string CoTChat::buildGroupCoT(const std::string& group_name,
                               const std::string& message)
{
  auto git = m_groups.find(group_name);
  if(git == m_groups.end()) {
    reportRunWarning("pCoTChat: group '" + group_name +
                     "' not found in group table — sending as All Chat Rooms");
    return buildAllRoomsCoT(message);
  }

  string msg_id  = generateMsgId();
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string group_id = msg_id; // use message ID as group session ID
  string uid      = "GeoChat." + m_own_uid + "." + group_id + "." + msg_id;

  // Build chatgrp attributes (uid0=sender, uid1..N=members)
  string chatgrp_attrs = "uid0=\"" + m_own_uid + "\"";
  string hierarchy_contacts;
  int member_idx = 1;

  const vector<string>& members = git->second;
  for(const auto& callsign : members) {
    auto cit = m_contacts_by_callsign.find(callsign);
    string member_uid = (cit != m_contacts_by_callsign.end())
                        ? cit->second.uid
                        : callsign; // fallback: use callsign as uid
    chatgrp_attrs += " uid" + intToString(member_idx) +
                     "=\"" + member_uid + "\"";
    hierarchy_contacts +=
      "<contact uid=\"" + member_uid + "\" name=\"" + callsign + "\"/>";
    member_idx++;
  }

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"UserGroups\""
        " groupOwner=\"true\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\""       + group_name     + "\""
        " id=\""             + group_id       + "\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp " + chatgrp_attrs + " id=\"" + group_id + "\"/>"
        "<hierarchy>"
          "<group uid=\"UserGroups\" name=\"Groups\">"
            "<group uid=\"" + group_id + "\" name=\"" + group_name + "\">"
              "<contact uid=\"" + m_own_uid + "\""
                " name=\"" + m_own_callsign + "\"/>"
              + hierarchy_contacts +
            "</group>"
          "</group>"
        "</hierarchy>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " time=\"" + t_now + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, group_name);
}


// ============================================================
// buildDirectCoT()
//
// Direct message to a specific callsign.
// Looks up the target's UID from the contact table.
// Falls back to using the callsign as the UID if not found.
// ============================================================

string CoTChat::buildDirectCoT(const std::string& callsign,
                                const std::string& message)
{
  // Look up target UID from contact table
  string target_uid;
  auto it = m_contacts_by_callsign.find(callsign);
  if(it != m_contacts_by_callsign.end()) {
    target_uid = it->second.uid;
  }
  else {
    // Not yet seen — use callsign as UID placeholder and warn
    target_uid = callsign;
    reportRunWarning("pCoTChat: DM to '" + callsign +
                     "' — UID unknown, using callsign as UID. "
                     "Wait for their SA CoT or check callsign spelling.");
  }

  string msg_id = generateMsgId();
  string t_now  = formatCoTTime(m_curr_time, 0.0);
  string uid    = "GeoChat." + m_own_uid + "." + target_uid + "." + msg_id;

  string detail =
    "<detail>"
      "<__chat"
        " parent=\"RootContactGroup\""
        " groupOwner=\"false\""
        " messageId=\""      + msg_id         + "\""
        " chatroom=\""       + callsign       + "\""
        " id=\""             + target_uid     + "\""
        " senderCallsign=\"" + m_own_callsign + "\">"
        "<chatgrp"
          " uid0=\"" + m_own_uid   + "\""
          " uid1=\"" + target_uid  + "\""
          " id=\""   + target_uid  + "\"/>"
      "</__chat>"
      "<link uid=\"" + m_own_uid + "\""
        " type=\"a-f-G-U-C\" relation=\"p-p\"/>"
      "<remarks"
        " source=\"BAO.F.ATAK." + m_own_uid + "\""
        " to=\""    + target_uid + "\""
        " time=\""  + t_now      + "\">"
        + message +
      "</remarks>"
    "</detail>";

  return assembleCoT(uid, detail, callsign);
}


// ============================================================
// buildReport()
// ============================================================

bool CoTChat::buildReport()
{
  m_msgs << "Own: callsign=" << m_own_callsign
         << "  uid=" << m_own_uid
         << "  nav=" << (m_nav_valid ? "valid" : "no fix")
         << "  debug=" << boolToString(m_debug) << endl;
  m_msgs << endl;

  m_msgs << "Chat IN:  received=" << m_chat_in_received
         << "  published=" << m_chat_in_published << endl;
  m_msgs << "Chat OUT: sent=" << m_chat_out_sent << endl;
  m_msgs << endl;

  m_msgs << "Contacts tracked (" << m_contacts_by_callsign.size() << "):" << endl;
  for(auto& kv : m_contacts_by_callsign) {
    double age = m_curr_time - kv.second.last_seen;
    m_msgs << "  " << kv.second.callsign
           << " → " << kv.second.uid
           << " (" << doubleToStringX(age, 0) << "s ago)" << endl;
  }

  if(!m_groups.empty()) {
    m_msgs << endl;
    m_msgs << "Groups (" << m_groups.size() << "):" << endl;
    for(auto& kv : m_groups) {
      m_msgs << "  " << kv.first << ": ";
      for(auto& m : kv.second) m_msgs << m << " ";
      m_msgs << endl;
    }
  }

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
