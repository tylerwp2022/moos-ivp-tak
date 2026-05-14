/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/*    REV:  May 14, 2026 -- expanded to report task & state */
/************************************************************/

#include <string>

#include "MBUtils.h"   // setBooleanOnString, tolower
#include "StatusHandler.h"

namespace common {

// ============================================================
// Construction
// ============================================================
//
// Default-init every tracked flag. ATAK_AUTO_UNTAG defaults
// to true to match UntagHandler's default (auto-recovery on);
// the others default false because the safe assumption when
// the variable hasn't been set yet is "this state isn't
// active".

StatusHandler::StatusHandler()
  : m_atak_waypt_active(false),
    m_atak_flag_pursuit(false),
    m_atak_auto_untag(true),
    m_has_flag(false),
    m_query_count(0)
{}


std::vector<std::string> StatusHandler::chatKeywords() const
{
  return { "status" };
}


std::string StatusHandler::helpLine() const
{
  return "status            -- show current state and task";
}


// ============================================================
// MOOS subscriptions
// ============================================================
//
// Variables that StatusHandler tracks directly. Mirrored
// state (DEPLOY/ATAK_MODE/TAGGED/ATAK_RETRY) comes from ctx
// and doesn't need a subscription here.
//
// All these variables exist on the vehicle MOOSDB (most
// arrive via uFldShoreBroker qbridges from shore). On the
// shore MOOSDB, only AQUATICUS_GAME is meaningful -- the
// others are vehicle-local. Subscribing to all of them on
// shore is harmless; the mail simply never arrives.

void StatusHandler::registerSubs(std::vector<std::string>& subs)
{
  subs.push_back("ACTION");
  subs.push_back("ATAK_WAYPT_ACTIVE");
  subs.push_back("ATAK_FLAG_PURSUIT");
  subs.push_back("ATAK_AUTO_UNTAG");
  subs.push_back("HAS_FLAG");
  subs.push_back("AQUATICUS_GAME");
}


// ============================================================
// onMail -- update tracked state
// ============================================================

void StatusHandler::onMail(const std::string& key,
                            const std::string& value,
                            CommanderContext& /*ctx*/)
{
  if(key == "ACTION") {
    m_action = value;
  }
  else if(key == "ATAK_WAYPT_ACTIVE") {
    setBooleanOnString(m_atak_waypt_active, value);
  }
  else if(key == "ATAK_FLAG_PURSUIT") {
    setBooleanOnString(m_atak_flag_pursuit, value);
  }
  else if(key == "ATAK_AUTO_UNTAG") {
    setBooleanOnString(m_atak_auto_untag, value);
  }
  else if(key == "HAS_FLAG") {
    setBooleanOnString(m_has_flag, value);
  }
  else if(key == "AQUATICUS_GAME") {
    m_aquaticus_game = value;
  }
}


// ============================================================
// handleChat -- assemble status DM
// ============================================================
//
// Two paths: vehicle (detailed local state) and shore
// (game state + hint).
//
// Multi-line output uses "&#10;" -- the HTML decimal entity
// for LF -- which ATAK GeoChat renders as a newline. Raw
// '\n' would break the CoT XML; see CommanderContext.h
// documentation on ctx.dm for the full rule set.

bool StatusHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  m_query_count++;
  static const char* NL = "&#10;";

  // ----------------------------------------------------------
  // Shore mode -- game state + usage hint
  // ----------------------------------------------------------
  if(ctx.fleet_mode) {
    std::string status;
    status += "Shore:";
    status += NL;
    status += "  Game: " +
              (m_aquaticus_game.empty() ? std::string("(unset)")
                                         : m_aquaticus_game);
    status += NL;
    status += "  For per-vehicle state: send 'status' directly to ";
    status += "the vehicle's chatroom, or '[vehicle] status' here.";

    ctx.dm(status, msg.reply_to);
    ctx.dlog("StatusHandler: shore status replied to " + msg.reply_to);
    return true;
  }

  // ----------------------------------------------------------
  // Vehicle mode -- full local state
  // ----------------------------------------------------------

  // ---- Mode ----
  std::string mode;
  if(!ctx.deployed)        mode = "Parked";
  else if(ctx.atak_mode)   mode = "ATAK control";
  else                      mode = "Autonomous";

  // ---- Task derivation (priority order) ----
  std::string task;
  if(!ctx.deployed) {
    task = "Idle (not deployed)";
  }
  else if(ctx.tagged && !m_atak_auto_untag) {
    task = "Tagged, holding (untag off)";
  }
  else if(ctx.tagged) {
    task = "Tagged, recovering";
  }
  else if(ctx.atak_mode && m_atak_flag_pursuit) {
    task = "Pursuing flag (ATAK auto)";
  }
  else if(ctx.atak_mode && m_atak_waypt_active) {
    task = "Navigating to operator waypoint";
  }
  else if(ctx.atak_mode) {
    task = "Awaiting operator command";
  }
  else if(m_action == "ATTACK_MED" || m_action == "ATTACK_E") {
    task = "Attacking flag";
  }
  else if(m_action == "DEFEND_MED" || m_action == "DEFEND_E") {
    task = "Defending zone";
  }
  else {
    task = "Autonomous (no role assigned)";
  }

  // ---- Assemble multi-line DM ----
  std::string status;
  status += ctx.command_chatroom + ":";
  status += NL + std::string("  Deployed: ") + (ctx.deployed ? "YES" : "NO");
  status += NL + std::string("  Mode:     ") + mode;
  status += NL + std::string("  Task:     ") + task;

  // Show raw ACTION only when running autonomously and a
  // role has been assigned -- otherwise it's redundant or
  // empty.
  if(mode == "Autonomous" && !m_action.empty()) {
    status += NL + std::string("  Action:   ") + m_action;
  }

  status += NL + std::string("  Tagged:   ") + (ctx.tagged ? "YES" : "NO");
  status += NL + std::string("  Has flag: ") + (m_has_flag ? "YES" : "NO");

  ctx.dm(status, msg.reply_to);
  ctx.dlog("StatusHandler: vehicle status replied to " + msg.reply_to +
           " (mode=" + mode + " task=" + task + ")");
  return true;
}


// ============================================================
// appcast
// ============================================================

void StatusHandler::appcast(std::string& report) const
{
  report += "  Queries:    " + std::to_string(m_query_count) + "\n";
  if(!m_action.empty())
    report += "  ACTION:     " + m_action + "\n";
  report += "  WaytActive: " + std::string(m_atak_waypt_active ? "true" : "false") + "\n";
  report += "  FlagPurs:   " + std::string(m_atak_flag_pursuit ? "true" : "false") + "\n";
  report += "  AutoUntag:  " + std::string(m_atak_auto_untag  ? "true" : "false") + "\n";
  report += "  HasFlag:    " + std::string(m_has_flag         ? "true" : "false") + "\n";
  if(!m_aquaticus_game.empty())
    report += "  Game:       " + m_aquaticus_game + "\n";
}

} // namespace common
