/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: DefendHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "defend" chat command for the Aquaticus     */
/*  CTF mission. Direct mirror of AttackHandler -- same     */
/*  parsing shape, same state interactions, just publishes  */
/*  DEFEND_MED / DEFEND_E instead of ATTACK_MED / ATTACK_E. */
/*                                                          */
/*  See AttackHandler.h for the rationale on accepted       */
/*  forms, the ATAK-mode warning, and the underscore        */
/*  back-compat aliases (defend_e, defend_med).             */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ACTION+sfx              = "DEFEND_MED" | "DEFEND_E"   */
/*    ATAK_CHAT_OUT (via dm)  = confirmation/warning        */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    defend           -> DEFEND_MED                        */
/*    defend easy      -> DEFEND_E                          */
/*    defend med       -> DEFEND_MED (explicit)             */
/*    defend_med       -> DEFEND_MED (back-compat)          */
/*    defend_e         -> DEFEND_E   (back-compat)          */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_DEFEND_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_DEFEND_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class DefendHandler : public CoTCommandHandler
{
public:
  DefendHandler();
  ~DefendHandler() override = default;

  std::string name() const override { return "Defend"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count_med;
  unsigned int m_count_easy;
  unsigned int m_reject_count;
  std::string  m_last_assignment;
};

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_DEFEND_HANDLER_HEADER
