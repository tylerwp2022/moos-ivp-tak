/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AtakHandler.cpp                                 */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "AtakHandler.h"

namespace common {

AtakHandler::AtakHandler()
  : m_count(0),
    m_reject_count(0)
{}


std::vector<std::string> AtakHandler::chatKeywords() const
{
  return { "atak" };
}


std::string AtakHandler::helpLine() const
{
  return "atak              -- enter ATAK mode (suppress game behaviors)";
}


// ============================================================
// handleChat() -- enter ATAK mode
// ============================================================
//
// Faithful port of the "atak" branch in the pre-refactor
// CoTCommander::handleChatCommand().
//
// No side effects on other state -- WAYPT_ACTIVE remains
// whatever it was, deployment state untouched. The operator
// is expected to follow with a "Send To" waypoint from
// ATAK; the b-m-p-w-GOTO will populate ATAK_WPT_UPDATE and
// flip ATAK_WAYPT_ACTIVE on.

bool AtakHandler::handleChat(const ChatMessage& msg,
                              CommanderContext& ctx)
{
  if(msg.cmd != "atak") {
    ctx.dm("Usage: atak   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_MODE" + msg.sfx, "true");
  // Ground-truth mirror -- same shore variable that vehicle
  // bridges write to via _STATE bridge. Last writer wins
  // (chat command wins over vehicle echo if simultaneous).
  ctx.publish("ATAK_MODE_STATE" + msg.sfx, "true");

  // Preserve pre-refactor DM phrasing -- capitalized "All
  // vehicles" for sfx==_ALL, target_label otherwise.
  std::string subject = (msg.sfx == "_ALL") ? "All vehicles"
                                            : msg.target_label;
  ctx.dm(subject + " in ATAK mode. Send waypoint or 'resume' to exit.",
         msg.reply_to);

  m_count++;
  m_last_entry = msg.target_label;
  if(!msg.callsign.empty())
    m_last_entry += " (from " + msg.callsign + ")";

  ctx.dlog("AtakHandler: ATAK_MODE" + msg.sfx + "=true");
  return true;
}


void AtakHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_entry.empty())
    report += "  Last:       " + m_last_entry + "\n";
}

} // namespace common
