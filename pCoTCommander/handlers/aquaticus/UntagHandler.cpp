/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: UntagHandler.cpp                                */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Same on|off toggle idiom as common::AvoidHandler.       */
/*  Aquaticus-specific because tagging is a CTF concept.    */
/************************************************************/

#include <string>

#include "UntagHandler.h"

namespace aquaticus {

UntagHandler::UntagHandler()
  : m_count_on(0),
    m_count_off(0),
    m_reject_count(0)
{}


std::vector<std::string> UntagHandler::chatKeywords() const
{
  return { "untag" };
}


std::string UntagHandler::helpLine() const
{
  return "untag on|off      -- toggle auto tag-recovery (CTF)";
}


bool UntagHandler::handleChat(const ChatMessage& msg,
                               CommanderContext& ctx)
{
  bool value;
  if      (msg.cmd == "untag on")  value = true;
  else if (msg.cmd == "untag off") value = false;
  else {
    ctx.dm("Usage: untag on|off. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("ATAK_AUTO_UNTAG" + msg.sfx,
              value ? "true" : "false");

  std::string state = value ? "on" : "off";
  ctx.dm("Auto-untag " + state + " for " +
         msg.target_label + ".", msg.reply_to);

  if(value) m_count_on++; else m_count_off++;
  m_last_toggle = state + " - " + msg.target_label;
  if(!msg.callsign.empty())
    m_last_toggle += " (from " + msg.callsign + ")";

  ctx.dlog("UntagHandler: ATAK_AUTO_UNTAG" + msg.sfx +
           "=" + (value ? "true" : "false"));
  return true;
}


void UntagHandler::appcast(std::string& report) const
{
  report += "  Turned on:  " + std::to_string(m_count_on)  + "\n";
  report += "  Turned off: " + std::to_string(m_count_off) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_toggle.empty())
    report += "  Last:       " + m_last_toggle + "\n";
}

} // namespace aquaticus
