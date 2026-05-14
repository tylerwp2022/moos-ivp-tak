/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  ChatToggleHandlerTemplate.cpp                  */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- see header file for renaming checklist.     */
/************************************************************/

#include <string>

#include "ChatToggleHandlerTemplate.h"

namespace templ {

ChatToggleHandlerTemplate::ChatToggleHandlerTemplate()
  : m_state(true),         // default: on
    m_on_count(0),
    m_off_count(0),
    m_reject_count(0)
{}


// ============================================================
// chatKeywords -- which words trigger this handler
// ============================================================

std::vector<std::string> ChatToggleHandlerTemplate::chatKeywords() const
{
  return { "template" };   // <-- change to your command
}


// ============================================================
// helpLine -- shown by 'help' command
// ============================================================

std::string ChatToggleHandlerTemplate::helpLine() const
{
  return "template on|off   -- example toggle handler";
}


// ============================================================
// handleChat -- the actual command logic
// ============================================================
//
// Parsing pattern for an on/off toggle:
//   "template on"  -> m_state = true
//   "template off" -> m_state = false
//   anything else  -> reject with usage DM
//
// All publications go via ctx.publish(). All DMs go via
// ctx.dm(). Never call MOOS Notify() directly.

bool ChatToggleHandlerTemplate::handleChat(const ChatMessage& msg,
                                            CommanderContext& ctx)
{
  // ----------------------------------------------------------
  // Parse the argument. msg.cmd has the full lowercased
  // command (e.g. "template on"), msg.first_word is just
  // the keyword. Extract anything after the keyword.
  // ----------------------------------------------------------
  std::string arg;
  size_t space = msg.cmd.find(' ');
  if(space != std::string::npos) {
    arg = msg.cmd.substr(space + 1);
    // trim leading whitespace from arg
    size_t f = arg.find_first_not_of(" \t");
    if(f != std::string::npos) arg = arg.substr(f);
  }

  // ----------------------------------------------------------
  // Decide new state. Reject anything that isn't on/off.
  // ----------------------------------------------------------
  bool new_state;
  if(arg == "on" || arg == "true") {
    new_state = true;
  }
  else if(arg == "off" || arg == "false") {
    new_state = false;
  }
  else {
    ctx.dm("Usage: template on|off. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  // ----------------------------------------------------------
  // Apply state and publish. msg.sfx is "_ALL" in fleet mode,
  // "_<VEHICLE>" when a vehicle was prefixed, or "" in
  // vehicle mode. Appending it produces the correct target
  // variable name automatically.
  // ----------------------------------------------------------
  m_state = new_state;
  std::string var_name  = "TEMPLATE_VAR" + msg.sfx;  // <-- rename
  std::string var_value = m_state ? "true" : "false";
  ctx.publish(var_name, var_value);

  // ----------------------------------------------------------
  // Confirm to operator. Use msg.target_label so the DM
  // reads naturally:
  //   shore + no prefix: "all vehicles -> template on."
  //   shore + prefix:    "blue_one -> template on."
  //   vehicle mode:      "vehicle -> template on."
  // ----------------------------------------------------------
  std::string dm = msg.target_label + " -> template " + arg + ".";
  ctx.dm(dm, msg.reply_to);

  // ----------------------------------------------------------
  // Update diagnostics for AppCast.
  // ----------------------------------------------------------
  if(m_state) m_on_count++; else m_off_count++;
  m_last_command = msg.target_label + " -> " + arg;
  if(!msg.callsign.empty())
    m_last_command += " (from " + msg.callsign + ")";

  ctx.dlog("ChatToggleHandlerTemplate: " + var_name + "=" + var_value);
  return true;
}


// ============================================================
// appcast -- diagnostic section in AppCast output
// ============================================================

void ChatToggleHandlerTemplate::appcast(std::string& report) const
{
  report += "  State:      " + std::string(m_state ? "on" : "off") + "\n";
  report += "  On count:   " + std::to_string(m_on_count) + "\n";
  report += "  Off count:  " + std::to_string(m_off_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_command.empty())
    report += "  Last:       " + m_last_command + "\n";
}

} // namespace templ
