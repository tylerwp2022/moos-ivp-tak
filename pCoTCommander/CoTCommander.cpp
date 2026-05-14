/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander.cpp                                */
/*    DATE: April 2026                                      */
/*    REV:  May 13, 2026 -- handler-class refactor          */
/*                                                          */
/*  Slim dispatcher. All per-command logic lives in the     */
/*  handlers/ subdirectory; this file is lifecycle +        */
/*  dispatch + state mirror.                                */
/*                                                          */
/*  See CoTCommander.h for the dispatch flow and startup    */
/*  sequence in detail.                                     */
/************************************************************/

#include <cstdlib>      // atof
#include <list>
#include <set>
#include <string>
#include <vector>

#include "MBUtils.h"
#include "ColorParse.h"
#include "ACTable.h"

#include "CoTCommander.h"
#include "CoTUtils.h"
#include "CommandHandlerFactory.h"

// ============================================================
// Constructor
// ============================================================

CoTCommander::CoTCommander()
  : m_geodesy_initialized(false),
    m_fleet_mode(true),
    m_command_chatroom("AQUATICUS-SHORE"),
    m_enable_chat_commands(true),
    m_debug(false),
    m_cot_received(0),
    m_cot_handled(0),
    m_cot_ignored(0),
    m_chat_received(0),
    m_chat_handled(0),
    m_chat_unknown(0),
    m_last_dispatch("none")
{
  // Context defaults are set in CommanderContext.h's
  // member initializers. We populate runtime-bound fields
  // (publish/dm/dlog lambdas, geodesy pointer, mode flags)
  // in bindContext() during OnStartUp().
}


// ============================================================
// debugLog -- circular buffer for AppCast diagnostics
// ============================================================
//
// No-op when m_debug == false. Buffer caps at
// DEBUG_BUF_SIZE; oldest entry drops off the front.

void CoTCommander::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer
// ============================================================

bool CoTCommander::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp
// ============================================================
//
// 1. Read .moos ProcessConfig.
// 2. Pull dispatcher-level keys (fleet_mode, command_set,
//    mission, command_chatroom, operator_uid_filter,
//    debug, enable_chat_commands, custom enable_handler
//    entries).
// 3. Initialize geodesy from LatOrigin/LongOrigin if
//    present (otherwise wait for NODE_REPORT).
// 4. Build the handler bundle via CommandHandlerFactory.
// 5. Fan out configure(key,value) to every handler for
//    every cached config line.
// 6. Build the chat keyword index (fails fast on duplicate
//    keywords).
// 7. Bind m_ctx callbacks.
// 8. Register MOOS subscriptions (common + per-handler).

bool CoTCommander::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  std::list<std::string> sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // ---- Parse dispatcher-level config, collect custom_handlers ----
  std::vector<std::string> custom_handlers;
  std::string command_set, mission;
  bool use_nav_fallback = false;

  std::list<std::string>::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    std::string orig  = *p;
    std::string line  = *p;
    std::string param = tolower(biteStringX(line, '='));
    std::string value = line;

    if(param == "fleet_mode")
      setBooleanOnString(m_fleet_mode, value);
    else if(param == "command_set")
      command_set = tolower(value);
    else if(param == "mission")
      mission = tolower(value);
    else if(param == "command_chatroom")
      m_command_chatroom = value;
    else if(param == "operator_uid_filter")
      m_operator_uid_filter = value;
    else if(param == "debug")
      setBooleanOnString(m_debug, value);
    else if(param == "enable_chat_commands")
      setBooleanOnString(m_enable_chat_commands, value);
    else if(param == "enable_handler")
      custom_handlers.push_back(tolower(value));
    else if(param == "use_nav_fallback")
      setBooleanOnString(use_nav_fallback, value);
    // Other keys are passed through to handlers below.
  }

  m_command_set = command_set;  // empty -> factory defaults

  // ---- Initialize geodesy ----
  // Static LatOrigin/LongOrigin from the .moos preferred;
  // otherwise wait for NODE_REPORT to set the NAV anchor.
  m_geodesy.setNavFallback(use_nav_fallback);
  double lat_origin = 0.0, lon_origin = 0.0;
  bool got_lat = m_MissionReader.GetValue("LatOrigin",  lat_origin);
  bool got_lon = m_MissionReader.GetValue("LongOrigin", lon_origin);
  if(got_lat && got_lon) {
    if(m_geodesy.initialise(lat_origin, lon_origin)) {
      m_geodesy_initialized = true;
      debugLog("OnStartUp: geodesy initialized origin=(" +
               doubleToStringX(lat_origin, 6) + "," +
               doubleToStringX(lon_origin, 6) + ")");
    }
  } else {
    debugLog("OnStartUp: no LatOrigin/LongOrigin -- waiting for NODE_REPORT");
  }

  // ---- Build handler bundle via factory ----
  auto result = CommandHandlerFactory::build(m_command_set, mission,
                                              m_fleet_mode, custom_handlers);
  if(!result.ok) {
    reportConfigWarning("CommandHandlerFactory: " + result.error);
    return false;
  }
  m_handlers = std::move(result.handlers);

  // ---- Fan configure() out to every handler ----
  // Handlers consume the keys they recognize and ignore the
  // rest. Order of replay matches order of lines in .moos.
  for(auto& h : m_handlers) {
    for(p = sParams.begin(); p != sParams.end(); p++) {
      std::string line  = *p;
      std::string key   = biteStringX(line, '=');
      std::string value = line;
      h->configure(key, value);
    }
  }

  // ---- Build chat keyword index ----
  if(!buildChatIndex())
    return false;

  // ---- Bind context (callbacks + pointers + mode + chatroom) ----
  bindContext();

  // ---- Register MOOS subscriptions ----
  registerVariables();

  debugLog("OnStartUp: " + std::to_string(m_handlers.size()) +
           " handlers loaded; " + std::to_string(m_chat_index.size()) +
           " chat keywords indexed");
  return true;
}


// ============================================================
// buildChatIndex -- map<keyword, handler*>
// ============================================================
//
// Iterates every handler's chatKeywords() and inserts into
// m_chat_index. Duplicate keyword across handlers is a
// FATAL startup error -- caught here to prevent silent
// mis-dispatch from shipping.

bool CoTCommander::buildChatIndex()
{
  m_chat_index.clear();
  for(auto& h : m_handlers) {
    std::vector<std::string> kws = h->chatKeywords();
    for(const std::string& kw : kws) {
      auto inserted = m_chat_index.insert({kw, h.get()});
      if(!inserted.second) {
        reportConfigWarning(
          "Chat keyword conflict: '" + kw + "' claimed by both '" +
          inserted.first->second->name() + "' and '" + h->name() + "'");
        return false;
      }
    }
  }
  return true;
}


// ============================================================
// bindContext -- wire callbacks and pointers into m_ctx
// ============================================================
//
// Called once at OnStartUp after handlers are built and the
// chat index is constructed. Captures 'this' in the lambdas
// so handler ctx.publish/dm/dlog/help_lines calls route
// back into this CoTCommander instance.

void CoTCommander::bindContext()
{
  m_ctx.fleet_mode       = m_fleet_mode;
  m_ctx.command_chatroom = m_command_chatroom;
  m_ctx.geodesy          = &m_geodesy;
  m_ctx.geodesy_ready    = m_geodesy_initialized;

  m_ctx.publish = [this](const std::string& key,
                          const std::string& value) {
    Notify(key, value);
  };

  m_ctx.dm = [this](const std::string& msg,
                     const std::string& reply_to) {
    // pCoTChat parses ATAK_CHAT_OUT by searching for the
    // literal substring "|chatroom=", not by splitting on
    // any '|'. So pipes embedded in the message content
    // pass through correctly. Handlers may use '|' freely.
    //
    // What WILL break the DM (silently -- ATAK drops the
    // CoT and the operator sees nothing):
    //   * Raw '<' or '>' in message content. pCoTChat
    //     inserts the message text RAW into the CoT XML
    //     as the content of <remarks>...</remarks>. Angle
    //     brackets corrupt the XML structure and ATAK
    //     drops the malformed event.
    //   * Raw '&' that isn't part of a valid XML entity.
    //     Use the escape: &amp;.
    // Use "&#10;" for line breaks (HTML decimal LF entity);
    // raw '\n' breaks the XML the same way.
    Notify("ATAK_CHAT_OUT",
           "message=" + msg + "|chatroom=" + reply_to);
  };

  m_ctx.dlog = [this](const std::string& msg) {
    debugLog(msg);
  };

  m_ctx.help_lines = [this]() -> std::vector<std::string> {
    std::vector<std::string> lines;
    lines.reserve(m_handlers.size());
    for(auto& h : m_handlers) {
      std::string l = h->helpLine();
      if(!l.empty()) lines.push_back(l);
    }
    return lines;
  };
}


// ============================================================
// registerVariables -- common + handler-specific
// ============================================================

void CoTCommander::registerVariables()
{
  // Common subscriptions (dispatcher-managed)
  AppCastingMOOSApp::RegisterVariables();

  Register("COT_INBOUND",       0);
  Register("ATAK_CHAT_IN",      0);
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);
  Register("DEPLOY",            0);

  // Vehicle-mode state mirrors -- shore doesn't see these
  // bare on its own MOOSDB so don't bother subscribing.
  if(!m_fleet_mode) {
    Register("ATAK_MODE",  0);
    Register("TAGGED",     0);
    Register("ATAK_RETRY", 0);
  }

  // ---- Handler-specific subscriptions ----
  // Aggregate, dedupe (in case two handlers want the same
  // variable), then register the union.
  std::vector<std::string> handler_subs;
  for(auto& h : m_handlers)
    h->registerSubs(handler_subs);

  std::set<std::string> deduped(handler_subs.begin(),
                                  handler_subs.end());
  for(const std::string& sub : deduped)
    Register(sub, 0);
}


// ============================================================
// OnNewMail -- top-level mail router
// ============================================================
//
// Routes by key. COT_INBOUND / ATAK_CHAT_IN go to their
// dedicated dispatchers. Common state mirrors update m_ctx
// directly. Geodesy mail updates the converter. Anything
// else fans out to every handler's onMail() -- handlers
// self-filter by checking the key. The default onMail()
// implementation is a no-op, so handlers that don't
// override pay only a virtual dispatch.

bool CoTCommander::OnNewMail(MOOSMSG_LIST& NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    std::string key  = msg.GetKey();
    std::string sval = msg.GetString();

    if(key == "COT_INBOUND") {
      dispatchInboundCoT(sval);
    }
    else if(key == "ATAK_CHAT_IN") {
      if(m_enable_chat_commands) dispatchChatCommand(sval);
    }
    else if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      updateGeodesy(sval);
    }
    else if(key == "DEPLOY") {
      bool prev = m_ctx.deployed;
      setBooleanOnString(m_ctx.deployed, sval);
      if(m_ctx.deployed != prev)
        debugLog("DEPLOY = " + boolToString(m_ctx.deployed));
    }
    else if(key == "ATAK_MODE") {
      setBooleanOnString(m_ctx.atak_mode, sval);
    }
    else if(key == "TAGGED") {
      setBooleanOnString(m_ctx.tagged, sval);
    }
    else if(key == "ATAK_RETRY") {
      setBooleanOnString(m_ctx.atak_retry, sval);
    }
    else {
      // Handler-specific subscriptions. Fan out -- each
      // handler self-filters on key. Cheap: only ~2 of 16
      // handlers override onMail; the rest no-op.
      for(auto& h : m_handlers)
        h->onMail(key, sval, m_ctx);
    }
  }
  return true;
}


// ============================================================
// Iterate -- no-op
// ============================================================
//
// pCoTCommander is purely event-driven. Iterate() is here
// only because AppCastingMOOSApp requires it for AppCast
// reporting cadence.

bool CoTCommander::Iterate()
{
  AppCastingMOOSApp::Iterate();
  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// dispatchInboundCoT -- parse XML, filter, route to handler
// ============================================================

void CoTCommander::dispatchInboundCoT(const std::string& xml)
{
  m_cot_received++;

  ParsedCoT evt;
  evt.uid     = cot::extractAttr(xml, "uid");
  evt.type    = cot::extractAttr(xml, "type");
  evt.raw_xml = xml;

  std::string lat_str = cot::extractAttr(xml, "lat");
  std::string lon_str = cot::extractAttr(xml, "lon");
  if(!lat_str.empty() && !lon_str.empty()) {
    evt.lat = atof(lat_str.c_str());
    evt.lon = atof(lon_str.c_str());
    evt.has_position = true;
  }

  if(evt.uid.empty() || evt.type.empty()) {
    debugLog("dispatchInboundCoT: missing uid or type");
    m_cot_ignored++;
    return;
  }

  // Own-echo suppression. The TAK server loops our own SA
  // broadcasts back to us; they're not commands.
  if(evt.uid.find("surveyor-") == 0) {
    debugLog("dispatchInboundCoT: own echo uid=" + evt.uid);
    m_cot_ignored++;
    return;
  }

  // ---- Offer to handlers, first claim wins ----
  for(auto& h : m_handlers) {
    // Operator-UID cross-cutting filter. Handlers that
    // bypassOperatorFilter() (e.g. FlagPursuitHandler
    // receiving from pCoTGraphics) skip this check.
    if(!m_operator_uid_filter.empty() && !h->bypassOperatorFilter()) {
      if(evt.uid.find(m_operator_uid_filter) == std::string::npos)
        continue;  // filtered out for this handler
    }

    if(h->claimsCoT(evt)) {
      bool ok = h->handleCoT(evt, m_ctx);
      if(ok) {
        m_cot_handled++;
        m_last_dispatch = h->name() + ": CoT " + evt.type;
        reportEvent("pCoTCommander: [COT] " + h->name() +
                    " type=" + evt.type + " uid=" + evt.uid);
      }
      return;
    }
  }

  // No handler claimed the event.
  m_cot_ignored++;
  debugLog("dispatchInboundCoT: unhandled type=" + evt.type +
           " uid=" + evt.uid);
}


// ============================================================
// dispatchChatCommand -- parse, resolve prefix, route
// ============================================================

void CoTCommander::dispatchChatCommand(const std::string& moos_val)
{
  m_chat_received++;

  // ---- Parse "callsign=X,chatroom=Y,message=Z" ----
  // Message text can contain commas, so split on the
  // structured field separators rather than naively on ','.
  size_t cr_pos  = moos_val.find(",chatroom=");
  size_t msg_pos = moos_val.find(",message=");
  if(cr_pos == std::string::npos || msg_pos == std::string::npos) {
    debugLog("dispatchChatCommand: malformed -- " + moos_val);
    return;
  }

  ChatMessage msg;
  size_t cs_pos = moos_val.find("callsign=");
  if(cs_pos != std::string::npos)
    msg.callsign = moos_val.substr(cs_pos + 9, cr_pos - cs_pos - 9);

  msg.chatroom = moos_val.substr(cr_pos + 10, msg_pos - cr_pos - 10);
  std::string raw_message = moos_val.substr(msg_pos + 9);

  // ---- Chatroom filter ----
  if(msg.chatroom != m_command_chatroom) {
    debugLog("dispatchChatCommand: chatroom=" + msg.chatroom +
             " != " + m_command_chatroom);
    return;
  }

  msg.reply_to = msg.callsign.empty() ? "All Chat Rooms" : msg.callsign;

  // ---- Normalize: lowercase, trim ----
  std::string cmd = tolower(raw_message);
  size_t f = cmd.find_first_not_of(" \t\r\n");
  if(f == std::string::npos) return;
  cmd = cmd.substr(f);
  size_t l = cmd.find_last_not_of(" \t\r\n");
  if(l != std::string::npos) cmd = cmd.substr(0, l + 1);

  msg.cmd = cmd;

  size_t space  = cmd.find(' ');
  msg.first_word = (space != std::string::npos) ? cmd.substr(0, space) : cmd;

  // ---- Default suffix and label ----
  msg.sfx          = m_fleet_mode ? "_ALL" : "";
  msg.target_label = m_fleet_mode ? "all vehicles" : "vehicle";

  // ---- Vehicle-prefix resolution (fleet mode only) ----
  if(m_fleet_mode &&
     m_chat_index.find(msg.first_word) == m_chat_index.end()) {
    // First word is not a known keyword. If there's
    // remaining text after it, treat as "<vehicle> <command>".
    if(space != std::string::npos) {
      std::string vehicle = msg.first_word;
      std::string remaining = cmd.substr(space + 1);
      size_t rs = remaining.find_first_not_of(" \t");
      if(rs != std::string::npos) remaining = remaining.substr(rs);

      msg.target_vehicle = vehicle;
      msg.cmd            = remaining;
      size_t rsp = remaining.find(' ');
      msg.first_word     = (rsp != std::string::npos) ?
                            remaining.substr(0, rsp) : remaining;
      msg.sfx            = "_" + toupper(vehicle);
      msg.target_label   = vehicle;
    }
    // else: no second word -- fall through to unknown-command
  }

  // ---- Look up handler ----
  auto it = m_chat_index.find(msg.first_word);
  if(it == m_chat_index.end()) {
    m_chat_unknown++;
    // Route through m_ctx.dm so pipe-sanitization applies
    // (operator-typed raw_message could contain '|').
    m_ctx.dm("Unknown command: \"" + raw_message + "\". Try 'help'.",
             msg.reply_to);
    debugLog("dispatchChatCommand: unknown first_word=" + msg.first_word);
    return;
  }

  // ---- Dispatch ----
  CoTCommandHandler* h = it->second;
  bool ok = h->handleChat(msg, m_ctx);
  if(ok) {
    m_chat_handled++;
    m_last_dispatch = h->name() + ": " + msg.cmd;
    reportEvent("pCoTCommander: [CHAT] " + h->name() +
                " from " + msg.callsign);
  }
}


// ============================================================
// updateGeodesy -- set NAV anchor from NODE_REPORT
// ============================================================
//
// NODE_REPORT contains both X/Y and LAT/LON for a vehicle.
// CoTGeodesy uses the pair to fix its projection. Once the
// first valid report arrives, geodesy is "ready" and
// handlers needing position conversion can use it.

void CoTCommander::updateGeodesy(const std::string& node_report)
{
  double x = 0, y = 0, lat = 0, lon = 0;
  bool got_x = false, got_y = false;
  bool got_lat = false, got_lon = false;

  std::vector<std::string> tokens = parseString(node_report, ',');
  for(auto& tok : tokens) {
    std::string t_copy = tok;
    std::string k = toupper(biteStringX(t_copy, '='));
    std::string v = t_copy;
    if     (k == "X")   { x   = atof(v.c_str()); got_x   = true; }
    else if(k == "Y")   { y   = atof(v.c_str()); got_y   = true; }
    else if(k == "LAT") { lat = atof(v.c_str()); got_lat = true; }
    else if(k == "LON") { lon = atof(v.c_str()); got_lon = true; }
  }

  if(got_x && got_y && got_lat && got_lon) {
    m_geodesy.updateNavAnchor(x, y, lat, lon);
    if(!m_geodesy_initialized) {
      m_geodesy_initialized = true;
      m_ctx.geodesy_ready   = true;
      debugLog("geodesy anchor set from NODE_REPORT");
    }
  }
}


// ============================================================
// buildReport -- AppCast output
// ============================================================
//
// Top section: dispatcher state (geodesy, mode, counters).
// Then one section per handler: emit a header line and
// call handler->appcast() for the body.

bool CoTCommander::buildReport()
{
  m_msgs << "Geodesy: "
         << (m_geodesy_initialized ? "ready" : "NOT READY")
         << "  debug=" << boolToString(m_debug) << std::endl;

  m_msgs << "Mode:    " << (m_fleet_mode ? "fleet (_ALL)"
                                          : "vehicle (direct)")
         << "  chatroom=" << m_command_chatroom << std::endl;

  if(!m_operator_uid_filter.empty())
    m_msgs << "UID filter:  " << m_operator_uid_filter << std::endl;

  m_msgs << std::endl;

  m_msgs << "State:   deployed=" << boolToString(m_ctx.deployed);
  if(!m_fleet_mode) {
    m_msgs << "  atak_mode="     << boolToString(m_ctx.atak_mode)
           << "  tagged="        << boolToString(m_ctx.tagged)
           << "  retry="         << boolToString(m_ctx.atak_retry);
  }
  m_msgs << std::endl << std::endl;

  m_msgs << "CoT:     received=" << m_cot_received
         << "  handled=" << m_cot_handled
         << "  ignored=" << m_cot_ignored << std::endl;
  m_msgs << "Chat:    received=" << m_chat_received
         << "  handled=" << m_chat_handled
         << "  unknown=" << m_chat_unknown << std::endl;

  m_msgs << "Last dispatch: " << m_last_dispatch << std::endl << std::endl;

  m_msgs << "Handlers (" << m_handlers.size() << "):" << std::endl;
  for(auto& h : m_handlers) {
    m_msgs << "==== " << h->name() << " ====" << std::endl;
    std::string section;
    h->appcast(section);
    m_msgs << section;
  }

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << std::endl << "-- debug --" << std::endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << std::endl;
  }

  return true;
}
