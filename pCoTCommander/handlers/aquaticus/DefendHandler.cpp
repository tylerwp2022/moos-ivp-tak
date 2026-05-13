/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: DefendHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Direct mirror of AttackHandler.cpp -- if you change the */
/*  parsing or DM logic here, change it there too. The two  */
/*  handlers share a structural identity that we chose not  */
/*  to factor into a base class (see decision journal       */
/*  May 13, 2026 "One handler per command").                */
/************************************************************/

#include <string>

#include "DefendHandler.h"

namespace aquaticus {

DefendHandler::DefendHandler()
  : m_count_med(0),
    m_count_easy(0),
    m_reject_count(0)
{}


std::vector<std::string> DefendHandler::chatKeywords() const
{
  return { "defend", "defend_e", "defend_med" };
}


std::string DefendHandler::helpLine() const
{
  return "defend [easy|med] -- assign defender role (CTF)";
}


bool DefendHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  std::string action_val;
  if     (msg.cmd == "defend"      || msg.cmd == "defend_med") action_val = "DEFEND_MED";
  else if(msg.cmd == "defend med")                              action_val = "DEFEND_MED";
  else if(msg.cmd == "defend easy" || msg.cmd == "defend_e")    action_val = "DEFEND_E";

  if(action_val.empty()) {
    ctx.dm("Usage: defend [easy|med]. Got: \"" + msg.cmd + "\"",
           msg.reply_to);
    m_reject_count++;
    return false;
  }

  std::string var_name = "ACTION" + msg.sfx;
  ctx.publish(var_name, action_val);

  std::string dm_text = msg.target_label + " -> " + action_val + ".";
  if(!ctx.fleet_mode && ctx.atak_mode)
    dm_text += " (in ATAK mode -- role takes effect after 'resume')";
  ctx.dm(dm_text, msg.reply_to);

  if(action_val == "DEFEND_E") m_count_easy++;
  else                          m_count_med++;
  m_last_assignment = msg.target_label + " -> " + action_val;
  if(!msg.callsign.empty())
    m_last_assignment += " (from " + msg.callsign + ")";

  ctx.dlog("DefendHandler: " + var_name + "=" + action_val);
  return true;
}


void DefendHandler::appcast(std::string& report) const
{
  report += "  DEFEND_MED: " + std::to_string(m_count_med)  + "\n";
  report += "  DEFEND_E:   " + std::to_string(m_count_easy) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_assignment.empty())
    report += "  Last:       " + m_last_assignment + "\n";
}

} // namespace aquaticus
