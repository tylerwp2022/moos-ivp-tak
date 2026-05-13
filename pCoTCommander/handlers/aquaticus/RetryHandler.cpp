/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: RetryHandler.cpp                                */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Same on|off toggle idiom as common::AvoidHandler.       */
/*  See the header for the state-interaction note about     */
/*  ctx.atak_retry lagging the publish by one iterate.      */
/************************************************************/

#include <string>

#include "RetryHandler.h"

namespace aquaticus {

RetryHandler::RetryHandler()
  : m_count_on(0),
    m_count_off(0),
    m_reject_count(0)
{}


std::vector<std::string> RetryHandler::chatKeywords() const
{
  return { "retry" };
}


std::string RetryHandler::helpLine() const
{
  return "retry on|off      -- toggle waypoint retry after untag (CTF)";
}


bool RetryHandler::handleChat(const ChatMessage& msg,
                               CommanderContext& ctx)
{
  bool value;
  if      (msg.cmd == "retry on")  value = true;
  else if (msg.cmd == "retry off") value = false;
  else {
    ctx.dm("Usage: retry on|off. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_RETRY" + msg.sfx,
              value ? "true" : "false");

  std::string state = value ? "on" : "off";
  ctx.dm("Retry " + state + " for " +
         msg.target_label + ".", msg.reply_to);

  if(value) m_count_on++; else m_count_off++;
  m_last_toggle = state + " - " + msg.target_label;
  if(!msg.callsign.empty())
    m_last_toggle += " (from " + msg.callsign + ")";

  ctx.dlog("RetryHandler: ATAK_RETRY" + msg.sfx +
           "=" + (value ? "true" : "false"));
  return true;
}


void RetryHandler::appcast(std::string& report) const
{
  report += "  Turned on:  " + std::to_string(m_count_on)  + "\n";
  report += "  Turned off: " + std::to_string(m_count_off) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_toggle.empty())
    report += "  Last:       " + m_last_toggle + "\n";
}

} // namespace aquaticus
