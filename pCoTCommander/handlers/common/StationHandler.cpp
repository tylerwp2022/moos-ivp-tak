/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StationHandler.cpp                              */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "StationHandler.h"

namespace common {

StationHandler::StationHandler()
  : m_count(0),
    m_reject_count(0)
{}


// ============================================================
// chatKeywords()
// ============================================================

std::vector<std::string> StationHandler::chatKeywords() const
{
  return { "station", "hold" };
}


std::string StationHandler::helpLine() const
{
  return "station | hold    -- station-keep at current position";
}


// ============================================================
// handleChat()
// ============================================================
//
// Faithful port of the "station" / "hold" branch in the
// pre-refactor CoTCommander::handleChatCommand().

bool StationHandler::handleChat(const ChatMessage& msg,
                                 CommanderContext& ctx)
{
  if(msg.cmd != "station" && msg.cmd != "hold") {
    ctx.dm("Usage: station  or  hold   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Station-keep assertion.
  ctx.publish("STATION_KEEP" + msg.sfx, "true");

  // Exit ATAK mode so waypt_atak yields.
  ctx.publish("ATAK_MODE"         + msg.sfx, "false");
  ctx.publish("ATAK_WAYPT_ACTIVE" + msg.sfx, "false");
  // Ground-truth mirrors (see AtakHandler comment).
  ctx.publish("ATAK_MODE_STATE"         + msg.sfx, "false");
  ctx.publish("ATAK_WAYPT_ACTIVE_STATE" + msg.sfx, "false");

  // Preserve pre-refactor DM phrasing.
  std::string subject = (msg.sfx == "_ALL") ? "All vehicles"
                                            : msg.target_label;
  ctx.dm(subject + " holding position.", msg.reply_to);

  // Diagnostics
  m_count++;
  m_last_station = msg.target_label;
  if(!msg.callsign.empty())
    m_last_station += " (from " + msg.callsign + ")";

  ctx.dlog("StationHandler: STATION_KEEP" + msg.sfx + "=true");
  return true;
}


void StationHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_station.empty())
    report += "  Last:       " + m_last_station + "\n";
}

} // namespace common
