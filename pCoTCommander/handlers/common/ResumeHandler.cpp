/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ResumeHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "ResumeHandler.h"

namespace common {

ResumeHandler::ResumeHandler()
  : m_count(0),
    m_reject_count(0)
{}


std::vector<std::string> ResumeHandler::chatKeywords() const
{
  return { "resume" };
}


std::string ResumeHandler::helpLine() const
{
  return "resume            -- exit ATAK mode, resume autonomous play";
}


// ============================================================
// handleChat() -- exit ATAK mode
// ============================================================
//
// Faithful port of the "resume" branch in the pre-refactor
// CoTCommander::handleChatCommand().

bool ResumeHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  if(msg.cmd != "resume") {
    ctx.dm("Usage: resume   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_MODE"         + msg.sfx, "false");
  ctx.publish("ATAK_WAYPT_ACTIVE" + msg.sfx, "false");
  // Ground-truth mirrors (see AtakHandler comment).
  ctx.publish("ATAK_MODE_STATE"         + msg.sfx, "false");
  ctx.publish("ATAK_WAYPT_ACTIVE_STATE" + msg.sfx, "false");

  // Preserve pre-refactor DM phrasing.
  std::string subject = (msg.sfx == "_ALL") ? "All vehicles"
                                            : msg.target_label;
  ctx.dm(subject + " resuming autonomous strategy.", msg.reply_to);

  m_count++;
  m_last_resume = msg.target_label;
  if(!msg.callsign.empty())
    m_last_resume += " (from " + msg.callsign + ")";

  ctx.dlog("ResumeHandler: ATAK_MODE" + msg.sfx + "=false");
  return true;
}


void ResumeHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_resume.empty())
    report += "  Last:       " + m_last_resume + "\n";
}

} // namespace common
