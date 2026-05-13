/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: PlayHandler.cpp                                 */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "PlayHandler.h"

namespace aquaticus {

PlayHandler::PlayHandler()
  : m_count(0),
    m_reject_count(0)
{}


std::vector<std::string> PlayHandler::chatKeywords() const
{
  return { "play" };
}


std::string PlayHandler::helpLine() const
{
  return "play              -- start the Aquaticus game (fleet-wide)";
}


// ============================================================
// handleChat() -- start the game
// ============================================================
//
// Faithful port of the "play" branch in the pre-refactor
// CoTCommander::handleChatCommand().
//
// Rejects vehicle-prefix forms ("blue_one play") because
// game state is not per-vehicle. Publishes the fixed
// fleet-wide variable AQUATICUS_GAME_ALL regardless of sfx.

bool PlayHandler::handleChat(const ChatMessage& msg,
                              CommanderContext& ctx)
{
  if(msg.cmd != "play") {
    ctx.dm("Usage: play   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Game control is fleet-wide only. sfx should be "_ALL"
  // (the shore-mode default). A "_<VEHICLE>" suffix means
  // the operator prefixed a vehicle name, which doesn't
  // make sense for game state.
  if(msg.sfx != "_ALL") {
    ctx.dm("Game control is fleet-wide only -- omit vehicle name (just: play).",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  ctx.publish("AQUATICUS_GAME_ALL", "play");
  ctx.dm("Game started.", msg.reply_to);

  m_count++;
  m_last_play = "started";
  if(!msg.callsign.empty())
    m_last_play += " (from " + msg.callsign + ")";

  ctx.dlog("PlayHandler: AQUATICUS_GAME_ALL=play");
  return true;
}


void PlayHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_play.empty())
    report += "  Last:       " + m_last_play + "\n";
}

} // namespace aquaticus
