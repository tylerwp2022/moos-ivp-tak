/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AttackHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "AttackHandler.h"

namespace aquaticus {

// ============================================================
// Constructor
// ============================================================

AttackHandler::AttackHandler()
  : m_count_med(0),
    m_count_easy(0),
    m_reject_count(0)
{}


// ============================================================
// chatKeywords() -- claim the "attack" word
// ============================================================
//
// Single first-word keyword. Modifiers ("easy", "med") and
// underscore variants ("attack_e", "attack_med") are parsed
// inside handleChat() rather than each being its own
// dispatch entry -- they're the same command with different
// arguments, not different commands.
//
// Note: "attack_e" and "attack_med" appear as a single
// "token" with no internal whitespace, so their first_word
// matches "attack_e" / "attack_med" exactly -- NOT "attack".
// That means the dispatcher would NOT route them to this
// handler via the first-word map lookup.
//
// The pre-refactor code handled this by string-matching the
// full normalized command. For the refactor, we have two
// options:
//   (a) claim {"attack", "attack_e", "attack_med"} as
//       separate first-word keys, all routed to this
//       handler. Cleanest from the dispatcher's POV.
//   (b) drop the underscore variants. They're historical
//       compatibility shims that the operator probably
//       never typed.
// Going with (a) -- preserves existing behavior at no cost.

std::vector<std::string> AttackHandler::chatKeywords() const
{
  return { "attack", "attack_e", "attack_med" };
}


// ============================================================
// helpLine() -- one-liner for the help command
// ============================================================
//
// Show the two human-facing forms. The "attack_e" / "attack_med"
// variants are not advertised -- they're back-compat shims.

std::string AttackHandler::helpLine() const
{
  return "attack [easy|med] -- assign attacker role (CTF)";
}


// ============================================================
// handleChat() -- process an attack assignment
// ============================================================
//
// Faithful port of the role-assignment branch in
// CoTCommander::handleChatCommand() for the attack family.
//
// ACCEPTED FORMS
//   "attack"            -> ACTION+sfx = ATTACK_MED
//   "attack med"        -> ACTION+sfx = ATTACK_MED
//   "attack easy"       -> ACTION+sfx = ATTACK_E
//   "attack_med"        -> ACTION+sfx = ATTACK_MED (back-compat)
//   "attack_e"          -> ACTION+sfx = ATTACK_E   (back-compat)
//
// REJECTED
//   "attack hard", "attack foo", etc. -> usage DM
//
// STATE INTERACTION
//   When ctx.atak_mode == true (vehicle mode only), game
//   role behaviors are suppressed by their condition lines
//   in the .bhv file. We still publish ACTION so the role
//   takes effect when the operator sends "resume", but we
//   append a warning to the confirmation DM so the operator
//   isn't surprised by the lack of immediate behavior change.

bool AttackHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  // Resolve the action value from the command text.
  std::string action_val;
  if     (msg.cmd == "attack"      || msg.cmd == "attack_med") action_val = "ATTACK_MED";
  else if(msg.cmd == "attack med")                              action_val = "ATTACK_MED";
  else if(msg.cmd == "attack easy" || msg.cmd == "attack_e")    action_val = "ATTACK_E";

  if(action_val.empty()) {
    ctx.dm("Usage: attack [easy|med].  Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Publish ACTION+sfx. The handler does not second-guess
  // sfx -- the dispatcher resolved it from the vehicle
  // prefix (or absence thereof).
  std::string var_name = "ACTION" + msg.sfx;
  ctx.publish(var_name, action_val);

  // Confirmation DM. Mention the resolved ACTION value so
  // the operator can verify the underlying assignment.
  std::string dm_text = msg.target_label + " -> " + action_val + ".";

  // ATAK-mode warning. Game behaviors only run when
  // ATAK_MODE != true; this assignment will take effect
  // on the next "resume". Vehicle mode only.
  if(!ctx.fleet_mode && ctx.atak_mode)
    dm_text += " (in ATAK mode -- role takes effect after 'resume')";

  ctx.dm(dm_text, msg.reply_to);

  // Diagnostics
  if(action_val == "ATTACK_E") m_count_easy++;
  else                          m_count_med++;
  m_last_assignment = msg.target_label + " -> " + action_val;
  if(!msg.callsign.empty())
    m_last_assignment += " (from " + msg.callsign + ")";

  ctx.dlog("AttackHandler: " + var_name + "=" + action_val);
  return true;
}


// ============================================================
// appcast() -- handler section in the AppCast report
// ============================================================

void AttackHandler::appcast(std::string& report) const
{
  report += "  ATTACK_MED: " + std::to_string(m_count_med)  + "\n";
  report += "  ATTACK_E:   " + std::to_string(m_count_easy) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_assignment.empty())
    report += "  Last:       " + m_last_assignment + "\n";
}

} // namespace aquaticus
