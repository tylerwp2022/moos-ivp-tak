/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "StatusHandler.h"

namespace common {

StatusHandler::StatusHandler()
  : m_query_count(0)
{}


std::vector<std::string> StatusHandler::chatKeywords() const
{
  return { "status" };
}


std::string StatusHandler::helpLine() const
{
  return "status            -- show deployment state";
}


// ============================================================
// handleChat() -- DM a status summary
// ============================================================
//
// Faithful port of the "status" branch in the pre-refactor
// CoTCommander::handleChatCommand(). Read-only: builds a
// summary string from CommanderContext state and DMs it
// back to the operator. No MOOS publications.
//
// Permissive about extra arguments -- "status now" is
// treated the same as "status". This matches the
// pre-refactor strict-equality check but doesn't bother
// the operator with a usage DM for what is fundamentally
// a read.

bool StatusHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  std::string status = std::string("Deployed: ") +
                       (ctx.deployed ? "YES" : "NO");

  // Vehicle mode: prefix with the vehicle's own callsign so
  // the operator can tell which boat replied.
  if(!ctx.fleet_mode)
    status = ctx.command_chatroom + " -- " + status;

  ctx.dm(status, msg.reply_to);
  m_query_count++;
  ctx.dlog("StatusHandler: " + status);
  return true;
}


void StatusHandler::appcast(std::string& report) const
{
  report += "  Queries:    " + std::to_string(m_query_count) + "\n";
}

} // namespace common
