/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/*    REV:  May 14, 2026 -- per-vehicle _STATE cache; no    */
/*                            more request/reply ping-pong  */
/************************************************************/

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "MBUtils.h"   // setBooleanOnString, tolower, biteString
#include "StatusHandler.h"

namespace common {

// ============================================================
// Local helpers
// ============================================================

namespace {

std::string toUpper(const std::string& s)
{
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c){ return std::toupper(c); });
  return out;
}

std::string toLower(const std::string& s)
{
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return out;
}

// Split "blue_one:blue_two:red_one" -> {"blue_one","blue_two","red_one"}.
// Tolerates spaces and commas as additional separators because
// the operator might have written it either way.
std::vector<std::string> splitNames(const std::string& csv)
{
  std::vector<std::string> out;
  std::string current;
  for(char c : csv) {
    if(c == ':' || c == ',' || c == ' ') {
      if(!current.empty()) { out.push_back(current); current.clear(); }
    } else {
      current.push_back(c);
    }
  }
  if(!current.empty()) out.push_back(current);
  return out;
}

// Strip a known prefix and return what's left (e.g.
// stripPrefix("ATAK_MODE_STATE_BLUE_ONE", "ATAK_MODE_STATE_")
// -> "BLUE_ONE"). Returns empty string if prefix didn't match.
std::string stripPrefix(const std::string& s, const std::string& prefix)
{
  if(s.size() <= prefix.size())              return "";
  if(s.compare(0, prefix.size(), prefix) != 0) return "";
  return s.substr(prefix.size());
}

} // anonymous


// ============================================================
// Construction
// ============================================================

StatusHandler::StatusHandler()
  : m_fleet_mode_resolved(false),
    m_fleet_mode(false),
    m_atak_waypt_active(false),
    m_atak_flag_pursuit(false),
    m_atak_auto_untag(true),
    m_has_flag(false),
    m_chat_queries(0),
    m_chat_fleet(0),
    m_cache_updates(0)
{}


std::vector<std::string> StatusHandler::chatKeywords() const
{
  return { "status" };
}


std::string StatusHandler::helpLine() const
{
  return "status [all]      -- show vehicle state (or fleet, on shore)";
}


// ============================================================
// configure() -- read fleet_mode and status_vehicles
// ============================================================
//
// The dispatcher calls configure() once per ProcessConfig
// line. We snoop on:
//   - fleet_mode: needed to choose vehicle vs shore subs in
//                  registerSubs(). The dispatcher also reads
//                  this; we re-read for our own use.
//   - status_vehicles: shore-side list of vehicles to cache.

void StatusHandler::configure(const std::string& key,
                               const std::string& value)
{
  std::string k = toLower(key);

  if(k == "fleet_mode") {
    setBooleanOnString(m_fleet_mode, value);
    m_fleet_mode_resolved = true;
    return;
  }

  if(k == "status_vehicles") {
    auto names = splitNames(value);
    for(const auto& name : names) {
      VehicleState v;
      v.vname_label = toLower(name);
      v.vname_upper = toUpper(name);
      if(m_vehicles.count(v.vname_upper)) continue; // dedupe
      m_vehicles[v.vname_upper] = v;
      m_vehicle_order.push_back(v.vname_upper);
    }
    return;
  }
}


// ============================================================
// registerSubs() -- mode-aware subscriptions
// ============================================================

void StatusHandler::registerSubs(std::vector<std::string>& subs)
{
  // AQUATICUS_GAME relevant in both modes.
  subs.push_back("AQUATICUS_GAME");

  if(!m_fleet_mode) {
    // ---- Vehicle mode: bare local names ----
    subs.push_back("ACTION");
    subs.push_back("ATAK_WAYPT_ACTIVE");
    subs.push_back("ATAK_FLAG_PURSUIT");
    subs.push_back("ATAK_AUTO_UNTAG");
    subs.push_back("HAS_FLAG");
    return;
  }

  // ---- Shore mode: per-vehicle suffixed names ----
  // status_vehicles must have been populated by configure().
  // If empty (mis-config), shore-mode status will return
  // empty caches but won't crash.
  for(const auto& upper : m_vehicle_order) {
    subs.push_back("DEPLOY_"                   + upper);
    subs.push_back("ATAK_MODE_STATE_"          + upper);
    subs.push_back("ATAK_WAYPT_ACTIVE_STATE_"  + upper);
    subs.push_back("ATAK_FLAG_PURSUIT_STATE_"  + upper);
    subs.push_back("TAGGED_"                   + upper);
    subs.push_back("HAS_FLAG_"                 + upper);
    subs.push_back("ATAK_AUTO_UNTAG_"          + upper);
    subs.push_back("ACTION_"                   + upper);
  }
}


// ============================================================
// findVehicle() -- case-tolerant cache lookup
// ============================================================

StatusHandler::VehicleState*
StatusHandler::findVehicle(const std::string& key_any_case)
{
  auto it = m_vehicles.find(toUpper(key_any_case));
  if(it == m_vehicles.end()) return nullptr;
  return &it->second;
}

const StatusHandler::VehicleState*
StatusHandler::findVehicle(const std::string& key_any_case) const
{
  auto it = m_vehicles.find(toUpper(key_any_case));
  if(it == m_vehicles.end()) return nullptr;
  return &it->second;
}


// ============================================================
// onMail() -- update local mirror or per-vehicle cache
// ============================================================
//
// In vehicle mode, we mirror local state vars into private
// members. In shore mode, the variable name carries the
// vehicle suffix, so we strip the prefix to identify which
// vehicle to update.

void StatusHandler::onMail(const std::string& key,
                            const std::string& value,
                            CommanderContext& /*ctx*/)
{
  // AQUATICUS_GAME is bare in both modes.
  if(key == "AQUATICUS_GAME") {
    m_aquaticus_game = value;
    return;
  }

  // ---- Vehicle-mode (local bare names) ----
  if(!m_fleet_mode) {
    if(key == "ACTION")              { m_action = value; return; }
    if(key == "ATAK_WAYPT_ACTIVE")   { setBooleanOnString(m_atak_waypt_active, value); return; }
    if(key == "ATAK_FLAG_PURSUIT")   { setBooleanOnString(m_atak_flag_pursuit, value); return; }
    if(key == "ATAK_AUTO_UNTAG")     { setBooleanOnString(m_atak_auto_untag, value); return; }
    if(key == "HAS_FLAG")            { setBooleanOnString(m_has_flag, value); return; }
    return;
  }

  // ---- Shore-mode (suffixed names) ----
  //
  // Test each prefix in declining length order so that
  // ATAK_MODE_STATE_<X> isn't matched as ATAK_MODE_<...>.

  std::string suffix;
  if(!(suffix = stripPrefix(key, "ATAK_MODE_STATE_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->atak_mode, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "ATAK_WAYPT_ACTIVE_STATE_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->atak_waypt_active, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "ATAK_FLAG_PURSUIT_STATE_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->atak_flag_pursuit, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "ATAK_AUTO_UNTAG_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->atak_auto_untag, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "DEPLOY_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->deployed, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "TAGGED_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->tagged, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "HAS_FLAG_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      setBooleanOnString(v->has_flag, value);
      m_cache_updates++;
    }
    return;
  }
  if(!(suffix = stripPrefix(key, "ACTION_")).empty()) {
    if(auto* v = findVehicle(suffix)) {
      v->action = value;
      m_cache_updates++;
    }
    return;
  }
}


// ============================================================
// deriveMode / deriveTask
// ============================================================

std::string StatusHandler::deriveMode(bool deployed,
                                       bool atak_mode) const
{
  if(!deployed)  return "Parked";
  if(atak_mode)  return "ATAK control";
  return "Autonomous";
}

std::string StatusHandler::deriveTask(bool deployed,
                                       bool tagged,
                                       bool atak_mode,
                                       bool atak_waypt_active,
                                       bool atak_flag_pursuit,
                                       bool atak_auto_untag,
                                       const std::string& action) const
{
  if(!deployed)                             return "Idle (not deployed)";
  if(tagged && !atak_auto_untag)            return "Tagged, holding (untag off)";
  if(tagged)                                return "Tagged, recovering";
  if(atak_mode && atak_flag_pursuit)        return "Pursuing flag (ATAK auto)";
  if(atak_mode && atak_waypt_active)        return "Navigating to operator waypoint";
  if(atak_mode)                              return "Awaiting operator command";
  if(action == "ATTACK_MED" || action == "ATTACK_E") return "Attacking flag";
  if(action == "DEFEND_MED" || action == "DEFEND_E") return "Defending zone";
  return "Autonomous (no role assigned)";
}


// ============================================================
// buildVehicleStatusFromCtx -- vehicle mode (path P1)
// ============================================================

std::string StatusHandler::buildVehicleStatusFromCtx(const CommanderContext& ctx) const
{
  static const char* NL = "&#10;";

  std::string mode = deriveMode(ctx.deployed, ctx.atak_mode);
  std::string task = deriveTask(ctx.deployed,
                                  ctx.tagged,
                                  ctx.atak_mode,
                                  m_atak_waypt_active,
                                  m_atak_flag_pursuit,
                                  m_atak_auto_untag,
                                  m_action);

  std::string status;
  status += ctx.command_chatroom + ":";
  status += NL + std::string("  Deployed: ") + (ctx.deployed ? "YES" : "NO");
  status += NL + std::string("  Mode:     ") + mode;
  status += NL + std::string("  Task:     ") + task;
  if(mode == "Autonomous" && !m_action.empty())
    status += NL + std::string("  Action:   ") + m_action;
  status += NL + std::string("  Tagged:   ") + (ctx.tagged ? "YES" : "NO");
  status += NL + std::string("  Has flag: ") + (m_has_flag ? "YES" : "NO");
  return status;
}


// ============================================================
// buildVehicleStatusFromCache -- shore mode (paths P3, P4)
// ============================================================

std::string StatusHandler::buildVehicleStatusFromCache(const VehicleState& v) const
{
  static const char* NL = "&#10;";

  std::string mode = deriveMode(v.deployed, v.atak_mode);
  std::string task = deriveTask(v.deployed,
                                  v.tagged,
                                  v.atak_mode,
                                  v.atak_waypt_active,
                                  v.atak_flag_pursuit,
                                  v.atak_auto_untag,
                                  v.action);

  std::string status;
  status += v.vname_label + ":";
  status += NL + std::string("  Deployed: ") + (v.deployed ? "YES" : "NO");
  status += NL + std::string("  Mode:     ") + mode;
  status += NL + std::string("  Task:     ") + task;
  if(mode == "Autonomous" && !v.action.empty())
    status += NL + std::string("  Action:   ") + v.action;
  status += NL + std::string("  Tagged:   ") + (v.tagged ? "YES" : "NO");
  status += NL + std::string("  Has flag: ") + (v.has_flag ? "YES" : "NO");
  return status;
}


// ============================================================
// buildShoreStatus -- game state + usage hint (path P2)
// ============================================================
//
// CAUTION: Message text is inserted RAW into the CoT
// <remarks>...</remarks> block by pCoTChat. Literal '<' or
// '>' characters open phantom XML tags and ATAK silently
// drops the whole CoT. Use [brackets] (not <angle> brackets)
// in any operator-facing text. Pipes, ampersands, slashes,
// quotes are all fine; only '<' and '>' break things.
// See CommanderContext.h ctx.dm docs for the full rule set.

std::string StatusHandler::buildShoreStatus() const
{
  static const char* NL = "&#10;";

  std::string status;
  status += "Shore:";
  status += NL;
  status += "  Game: " +
            (m_aquaticus_game.empty() ? std::string("(unset)")
                                       : m_aquaticus_game);
  status += NL;
  status += "  '[vehicle] status' queries one vehicle.";
  status += NL;
  status += "  'status all' queries the fleet.";

  if(!m_vehicles.empty()) {
    status += NL;
    status += "  Tracked: ";
    bool first = true;
    for(const auto& upper : m_vehicle_order) {
      auto it = m_vehicles.find(upper);
      if(it == m_vehicles.end()) continue;
      if(!first) status += ", ";
      status += it->second.vname_label;
      first = false;
    }
  }
  return status;
}


// ============================================================
// handleChat() -- dispatch P1/P2/P3/P4
// ============================================================

bool StatusHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  m_chat_queries++;

  // ----------------------------------------------------------
  // P1: vehicle mode -- DM local status to operator
  // ----------------------------------------------------------
  if(!ctx.fleet_mode) {
    std::string status = buildVehicleStatusFromCtx(ctx);
    ctx.dm(status, msg.reply_to);
    m_last_action = "P1 chat -> " + msg.reply_to;
    return true;
  }

  // ----------------------------------------------------------
  // Shore-mode parsing of "status [all]"
  // ----------------------------------------------------------
  std::string rest;
  size_t space = msg.cmd.find(' ');
  if(space != std::string::npos) {
    rest = msg.cmd.substr(space + 1);
    size_t f = rest.find_first_not_of(" \t");
    if(f != std::string::npos) rest = rest.substr(f);
  }

  // ----------------------------------------------------------
  // P4: shore + "status all" -- fleet rollcall
  // ----------------------------------------------------------
  if(rest == "all") {
    if(msg.sfx != "_ALL") {
      ctx.dm("Fleet query: send 'status all' without vehicle prefix.",
             msg.reply_to);
      return false;
    }
    if(m_vehicles.empty()) {
      ctx.dm("No vehicles configured. Set status_vehicles in "
             "plug_pCoTCommander_shore.moos.",
             msg.reply_to);
      return false;
    }

    // One DM per vehicle. ATAK GeoChat handles them as
    // separate messages in the shore thread, in iteration
    // order (predictable because m_vehicle_order is a vector).
    for(const auto& upper : m_vehicle_order) {
      auto it = m_vehicles.find(upper);
      if(it == m_vehicles.end()) continue;
      std::string status = buildVehicleStatusFromCache(it->second);
      ctx.dm(status, msg.reply_to);
    }

    m_chat_fleet++;
    m_last_action = "P4 fleet -> " + msg.reply_to;
    return true;
  }

  // ----------------------------------------------------------
  // P2: shore + plain "status" (no prefix, no "all")
  // ----------------------------------------------------------
  if(msg.sfx == "_ALL") {
    std::string status = buildShoreStatus();
    ctx.dm(status, msg.reply_to);
    m_last_action = "P2 shore -> " + msg.reply_to;
    return true;
  }

  // ----------------------------------------------------------
  // P3: shore + "<vehicle> status" -- cache lookup
  // ----------------------------------------------------------
  // msg.target_vehicle is lowercase ("blue_one"); cache is
  // keyed on uppercase. findVehicle() handles case folding.
  const VehicleState* v = findVehicle(msg.target_vehicle);
  if(!v) {
    ctx.dm("Vehicle '" + msg.target_label + "' not tracked. "
           "Check status_vehicles config on shore.",
           msg.reply_to);
    m_last_action = "P3 unknown vehicle " + msg.target_label;
    return false;
  }

  std::string status = buildVehicleStatusFromCache(*v);
  ctx.dm(status, msg.reply_to);
  m_last_action = "P3 cache hit " + msg.target_label;
  return true;
}


// ============================================================
// appcast()
// ============================================================

void StatusHandler::appcast(std::string& report) const
{
  report += "  Mode:           " + std::string(m_fleet_mode ? "shore" : "vehicle") + "\n";
  report += "  Chat queries:   " + std::to_string(m_chat_queries)  + "\n";
  report += "  Fleet queries:  " + std::to_string(m_chat_fleet)    + "\n";

  if(m_fleet_mode) {
    report += "  Tracked vehs:   " + std::to_string(m_vehicles.size()) + "\n";
    report += "  Cache updates:  " + std::to_string(m_cache_updates)   + "\n";
    if(!m_vehicle_order.empty()) {
      report += "  Vehicles:\n";
      for(const auto& upper : m_vehicle_order) {
        auto it = m_vehicles.find(upper);
        if(it == m_vehicles.end()) continue;
        const auto& v = it->second;
        report += "    " + v.vname_label +
                  " dep=" + (v.deployed ? "Y" : "n") +
                  " atak=" + (v.atak_mode ? "Y" : "n") +
                  " wpt="  + (v.atak_waypt_active ? "Y" : "n") +
                  " pur="  + (v.atak_flag_pursuit ? "Y" : "n") +
                  " tag="  + (v.tagged ? "Y" : "n") +
                  " flg="  + (v.has_flag ? "Y" : "n");
        if(!v.action.empty()) report += " act=" + v.action;
        report += "\n";
      }
    }
  } else {
    if(!m_action.empty())
      report += "  ACTION:         " + m_action + "\n";
    report += "  WaytActive:     " + std::string(m_atak_waypt_active ? "true" : "false") + "\n";
    report += "  FlagPurs:       " + std::string(m_atak_flag_pursuit ? "true" : "false") + "\n";
    report += "  AutoUntag:      " + std::string(m_atak_auto_untag  ? "true" : "false") + "\n";
    report += "  HasFlag:        " + std::string(m_has_flag         ? "true" : "false") + "\n";
  }

  if(!m_aquaticus_game.empty())
    report += "  Game:           " + m_aquaticus_game + "\n";
  if(!m_last_action.empty())
    report += "  Last:           " + m_last_action + "\n";
}

} // namespace common
