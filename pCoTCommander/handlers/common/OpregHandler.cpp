/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: OpregHandler.cpp                                */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Follows the on|off toggle idiom established in          */
/*  AvoidHandler.cpp, with one variation: the "off" case    */
/*  emits a WARNING DM instead of a plain confirmation,     */
/*  because turning off boundary recovery is a deliberately */
/*  scary operation. Preserves pre-refactor phrasing.       */
/************************************************************/

#include <string>

#include "OpregHandler.h"

namespace common {

OpregHandler::OpregHandler()
  : m_count_on(0),
    m_count_off(0),
    m_reject_count(0)
{}


std::vector<std::string> OpregHandler::chatKeywords() const
{
  return { "opreg" };
}


std::string OpregHandler::helpLine() const
{
  return "opreg on|off      -- toggle boundary recovery (ATAK mode)";
}


bool OpregHandler::handleChat(const ChatMessage& msg,
                               CommanderContext& ctx)
{
  bool value;
  if      (msg.cmd == "opreg on")  value = true;
  else if (msg.cmd == "opreg off") value = false;
  else {
    ctx.dm("Usage: opreg on|off. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_OPREG_RECOVER" + msg.sfx,
              value ? "true" : "false");

  // Asymmetric DM: confirmation when on, WARNING when off.
  // Operators are expected to read the warning before
  // committing to the off state.
  if(value) {
    ctx.dm("Boundary recovery on for " + msg.target_label + ".",
           msg.reply_to);
  } else {
    ctx.dm("WARNING: Boundary recovery off for " +
           msg.target_label + ". Robot may leave the field.",
           msg.reply_to);
  }

  std::string state = value ? "on" : "off";
  if(value) m_count_on++; else m_count_off++;
  m_last_toggle = state + " - " + msg.target_label;
  if(!msg.callsign.empty())
    m_last_toggle += " (from " + msg.callsign + ")";

  ctx.dlog("OpregHandler: ATAK_OPREG_RECOVER" + msg.sfx +
           "=" + (value ? "true" : "false"));
  return true;
}


void OpregHandler::appcast(std::string& report) const
{
  report += "  Turned on:  " + std::to_string(m_count_on)  + "\n";
  report += "  Turned off: " + std::to_string(m_count_off) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_toggle.empty())
    report += "  Last:       " + m_last_toggle + "\n";
}

} // namespace common
