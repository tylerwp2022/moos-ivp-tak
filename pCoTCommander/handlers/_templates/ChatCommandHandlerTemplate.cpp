/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  ChatCommandHandlerTemplate.cpp                 */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- see header file for renaming checklist.     */
/************************************************************/

#include <string>

#include "ChatCommandHandlerTemplate.h"

namespace templ {

ChatCommandHandlerTemplate::ChatCommandHandlerTemplate()
  : m_count(0),
    m_reject_count(0)
{}


std::vector<std::string> ChatCommandHandlerTemplate::chatKeywords() const
{
  return { "template" };   // <-- change to your command
}


std::string ChatCommandHandlerTemplate::helpLine() const
{
  return "template          -- example single-action handler";
}


// ============================================================
// handleChat -- the actual command logic
// ============================================================
//
// Pattern for a single-action command:
//   1. (Optional) Validate against context state.
//   2. Publish the MOOS variable(s) the .bhv / pHelmIvP needs.
//   3. DM a confirmation back to the operator.
//   4. Update diagnostic counters.
//
// IMPORTANT: msg.sfx is automatic.
//   * Shore mode, no vehicle prefix: msg.sfx == "_ALL"
//   * Shore mode + "blue_one X":     msg.sfx == "_BLUE_ONE"
//   * Vehicle mode:                  msg.sfx == ""
// Appending it to the variable name produces the right
// target automatically.

bool ChatCommandHandlerTemplate::handleChat(const ChatMessage& msg,
                                             CommanderContext& ctx)
{
  // ----------------------------------------------------------
  // OPTIONAL PRECONDITIONS.
  // Common checks. Uncomment the ones that apply.
  //
  // - Most actions require deployment first:
  //
  //   if(!ctx.deployed) {
  //     ctx.dm("Cannot " + msg.first_word +
  //            " -- vehicle is not deployed.", msg.reply_to);
  //     m_reject_count++;
  //     return false;
  //   }
  //
  // - Some commands shouldn't fire in ATAK mode (vehicle):
  //
  //   if(!ctx.fleet_mode && ctx.atak_mode) {
  //     ctx.dm("In ATAK mode -- send 'resume' first.",
  //            msg.reply_to);
  //     m_reject_count++;
  //     return false;
  //   }
  //
  // - Some commands are fleet-wide only (play/stop):
  //
  //   if(msg.sfx != "_ALL") {
  //     ctx.dm("Fleet-wide command -- omit vehicle name.",
  //            msg.reply_to);
  //     m_reject_count++;
  //     return false;
  //   }
  // ----------------------------------------------------------

  // ----------------------------------------------------------
  // PUBLISH.
  // Build the variable name with msg.sfx appended. Adjust
  // the variable name and value to your command.
  // ----------------------------------------------------------
  std::string var_name = "TEMPLATE_VAR" + msg.sfx;
  ctx.publish(var_name, "true");

  // If your command needs to publish multiple variables, just
  // call ctx.publish again:
  //   ctx.publish("ANOTHER_VAR" + msg.sfx, "value");

  // ----------------------------------------------------------
  // DM CONFIRMATION.
  // msg.target_label is the natural-language target
  // ("all vehicles", "blue_one", "vehicle").
  // ----------------------------------------------------------
  ctx.dm(msg.target_label + " -> template fired.", msg.reply_to);

  // ----------------------------------------------------------
  // DIAGNOSTICS.
  // ----------------------------------------------------------
  m_count++;
  m_last_action = msg.target_label;
  if(!msg.callsign.empty())
    m_last_action += " (from " + msg.callsign + ")";

  ctx.dlog("ChatCommandHandlerTemplate: " + var_name + "=true");
  return true;
}


// ============================================================
// appcast
// ============================================================

void ChatCommandHandlerTemplate::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_action.empty())
    report += "  Last:       " + m_last_action + "\n";
}

} // namespace templ
