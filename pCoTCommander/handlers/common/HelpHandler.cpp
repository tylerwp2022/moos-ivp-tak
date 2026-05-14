/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: HelpHandler.cpp                                 */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>
#include <vector>

#include "HelpHandler.h"

namespace common {

HelpHandler::HelpHandler()
  : m_query_count(0)
{}


std::vector<std::string> HelpHandler::chatKeywords() const
{
  return { "help" };
}


std::string HelpHandler::helpLine() const
{
  return "help              -- show this list";
}


// ============================================================
// handleChat() -- assemble and DM the help text
// ============================================================
//
// Calls ctx.help_lines() to get every active handler's
// helpLine(). Filters empty strings (handlers that opt out
// of help) and joins with newlines.
//
// In fleet mode, appends the per-vehicle syntax hint so
// the operator knows about the "blue_one <command>" form.

bool HelpHandler::handleChat(const ChatMessage& msg,
                              CommanderContext& ctx)
{
  m_query_count++;

  if(!ctx.help_lines) {
    // Defensive: factory didn't bind the callback. This is
    // a bug, not an operator error, but fail informative.
    ctx.dm("Help is unavailable (registry callback not bound). "
           "This is a configuration error.", msg.reply_to);
    ctx.dlog("HelpHandler: ctx.help_lines unbound!");
    return false;
  }

  std::vector<std::string> lines = ctx.help_lines();

  // ATAK GeoChat renders &#10; (HTML decimal entity for line
  // feed) as a newline. A raw '\n' character embedded in
  // the CoT XML body breaks the message -- the TAK server
  // either drops it or truncates at the first newline,
  // resulting in nothing appearing in ATAK. Use the entity.
  static const char* NL = "&#10;";

  std::string help_text = "Available commands:";
  help_text += NL;
  for(const std::string& line : lines) {
    if(line.empty()) continue;
    help_text += line;
    help_text += NL;
  }

  // Fleet-mode-only hint about the vehicle-prefix syntax.
  // The dispatcher handles this transparently, but operators
  // need to know it exists.
  if(ctx.fleet_mode) {
    help_text += "<vehicle> <command>  -- per-vehicle "
                 "(e.g. blue_one attack)";
    help_text += NL;
  }

  ctx.dm(help_text, msg.reply_to);
  ctx.dlog("HelpHandler: replied to " + msg.reply_to);
  return true;
}


// ============================================================
// appcast()
// ============================================================

void HelpHandler::appcast(std::string& report) const
{
  report += "  Queries:    " + std::to_string(m_query_count) + "\n";
}

} // namespace common
