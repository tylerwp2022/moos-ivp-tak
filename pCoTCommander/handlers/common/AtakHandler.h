/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AtakHandler.h                                   */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "atak" chat command -- enters ATAK mode.    */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Sets ATAK_MODE+sfx = true. This suppresses game         */
/*  behaviors (attack/defend/loiter) in pHelmIvP, since     */
/*  those condition on ATAK_MODE != true. The vehicle stays */
/*  ready for an operator-supplied waypoint (b-m-p-w-GOTO)  */
/*  or for "resume" to release control back to autonomous   */
/*  play.                                                   */
/*                                                          */
/*  Useful to pre-stage the vehicle into operator-           */
/*  supervised state before sending the first waypoint.     */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_MODE+sfx           = true                        */
/*    ATAK_CHAT_OUT (via dm)  = "<subject> in ATAK mode.    */
/*                              Send waypoint or 'resume'   */
/*                              to exit."                   */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    atak                                                  */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_ATAK_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_ATAK_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class AtakHandler : public CoTCommandHandler
{
public:
  AtakHandler();
  ~AtakHandler() override = default;

  std::string name() const override { return "Atak"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_entry;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_ATAK_HANDLER_HEADER
