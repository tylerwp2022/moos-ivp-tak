/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StopHandler.cpp                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Direct mirror of PlayHandler.cpp -- same fleet-wide-    */
/*  only rejection logic, publishes "pause" instead of      */
/*  "play".                                                 */
/************************************************************/

#include <string>

#include "StopHandler.h"

namespace aquaticus {

StopHandler::StopHandler()
  : m_count(0),
    m_reject_count(0)
{}


std::vector<std::string> StopHandler::chatKeywords() const
{
  return { "stop" };
}


std::string StopHandler::helpLine() const
{
  return "stop              -- pause the Aquaticus game (fleet-wide)";
}


bool StopHandler::handleChat(const ChatMessage& msg,
                              CommanderContext& ctx)
{
  if(msg.cmd != "stop") {
    ctx.dm("Usage: stop   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  if(msg.sfx != "_ALL") {
    ctx.dm("Game control is fleet-wide only -- omit vehicle name (just: stop).",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("AQUATICUS_GAME_ALL", "pause");
  ctx.dm("Game stopped.", msg.reply_to);

  m_count++;
  m_last_stop = "stopped";
  if(!msg.callsign.empty())
    m_last_stop += " (from " + msg.callsign + ")";

  ctx.dlog("StopHandler: AQUATICUS_GAME_ALL=pause");
  return true;
}


void StopHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_stop.empty())
    report += "  Last:       " + m_last_stop + "\n";
}

} // namespace aquaticus
