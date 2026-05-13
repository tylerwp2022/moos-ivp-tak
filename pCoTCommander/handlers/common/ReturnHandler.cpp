/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ReturnHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "ReturnHandler.h"

namespace common {

ReturnHandler::ReturnHandler()
  : m_count(0),
    m_reject_count(0)
{}


// ============================================================
// chatKeywords() -- claim "return" and its "rtb" alias
// ============================================================
//
// Two first-word keywords routed to this handler. The
// dispatcher's m_chat_index will have both pointing here.

std::vector<std::string> ReturnHandler::chatKeywords() const
{
  return { "return", "rtb" };
}


std::string ReturnHandler::helpLine() const
{
  return "return | rtb      -- return to base";
}


// ============================================================
// handleChat() -- process a return-to-base command
// ============================================================
//
// Faithful port of the "return" branch in the pre-refactor
// CoTCommander::handleChatCommand().
//
// Both "return" and "rtb" are accepted as the bare command;
// trailing arguments are rejected. (Pre-refactor behavior
// matched these with strict equality.)
//
// Side effect: exits ATAK mode. This is intentional --
// returning autonomously needs the helm free of any
// operator-imposed waypoint.

bool ReturnHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  if(msg.cmd != "return" && msg.cmd != "rtb") {
    ctx.dm("Usage: return  or  rtb   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Deployment + return assertion.
  ctx.publish("DEPLOY"               + msg.sfx, "true");
  ctx.publish("MOOS_MANUAL_OVERRIDE" + msg.sfx, "false");
  ctx.publish("RETURN"               + msg.sfx, "true");

  // Exit ATAK mode so waypt_atak yields to the return
  // behavior.
  ctx.publish("ATAK_MODE"         + msg.sfx, "false");
  ctx.publish("ATAK_WAYPT_ACTIVE" + msg.sfx, "false");

  // Preserve pre-refactor DM phrasing: "All vehicles" is
  // capitalized when sfx == _ALL; vehicle and per-vehicle
  // targets use target_label as-is. Keep this matching the
  // current code so log diffs across the migration are
  // empty for the operator-facing text.
  std::string subject = (msg.sfx == "_ALL") ? "All vehicles"
                                            : msg.target_label;
  ctx.dm(subject + " returning to base.", msg.reply_to);

  // Diagnostics
  m_count++;
  m_last_return = msg.target_label;
  if(!msg.callsign.empty())
    m_last_return += " (from " + msg.callsign + ")";

  ctx.dlog("ReturnHandler: RETURN" + msg.sfx + "=true");
  return true;
}


void ReturnHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_return.empty())
    report += "  Last:       " + m_last_return + "\n";
}

} // namespace common
