/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AvoidHandler.cpp                                */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  This file is the canonical implementation of the on|off */
/*  toggle handler idiom. The three sibling toggles         */
/*  (Opreg, Untag, Retry) follow exactly this shape with    */
/*  only the MOOS variable name and DM phrasing changing.   */
/*  If you fix a parsing bug here, audit all four.          */
/************************************************************/

#include <string>

#include "AvoidHandler.h"

namespace common {

AvoidHandler::AvoidHandler()
  : m_count_on(0),
    m_count_off(0),
    m_reject_count(0)
{}


std::vector<std::string> AvoidHandler::chatKeywords() const
{
  return { "avoid" };
}


std::string AvoidHandler::helpLine() const
{
  return "avoid on|off      -- toggle collision avoidance (ATAK mode)";
}


// ============================================================
// handleChat() -- toggle ATAK_AVOID_COLLISIONS
// ============================================================
//
// Faithful port of the "avoid on" / "avoid off" branches.
//
// IDIOM (repeated across the four on/off toggle handlers):
//   1. Parse "<keyword> on" or "<keyword> off". Anything
//      else -> usage DM, reject_count++.
//   2. Publish the boolean to the keyword's target MOOS
//      variable (with sfx).
//   3. Confirmation DM with the toggled state.
//   4. Per-state counters (count_on / count_off) for
//      AppCast visibility.

bool AvoidHandler::handleChat(const ChatMessage& msg,
                               CommanderContext& ctx)
{
  bool value;
  if      (msg.cmd == "avoid on")  value = true;
  else if (msg.cmd == "avoid off") value = false;
  else {
    ctx.dm("Usage: avoid on|off. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_AVOID_COLLISIONS" + msg.sfx,
              value ? "true" : "false");

  std::string state = value ? "on" : "off";
  ctx.dm("Collision avoidance " + state + " for " +
         msg.target_label + ".", msg.reply_to);

  if(value) m_count_on++; else m_count_off++;
  m_last_toggle = state + " - " + msg.target_label;
  if(!msg.callsign.empty())
    m_last_toggle += " (from " + msg.callsign + ")";

  ctx.dlog("AvoidHandler: ATAK_AVOID_COLLISIONS" + msg.sfx +
           "=" + (value ? "true" : "false"));
  return true;
}


void AvoidHandler::appcast(std::string& report) const
{
  report += "  Turned on:  " + std::to_string(m_count_on)  + "\n";
  report += "  Turned off: " + std::to_string(m_count_off) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_toggle.empty())
    report += "  Last:       " + m_last_toggle + "\n";
}

} // namespace common
