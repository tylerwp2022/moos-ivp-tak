/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: PauseHandler.h                                  */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "pause" chat command.                       */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Halts the addressed vehicle(s) by un-deploying and      */
/*  asserting manual override -- the helm yields, thruster  */
/*  output is gated, vehicle drifts. As a SIDE EFFECT exits */
/*  ATAK mode so the operator can issue a clean "deploy"    */
/*  to resume autonomous behavior. ATAK_MODE will need to   */
/*  be reasserted explicitly if the operator wants to       */
/*  resume supervisory control after unpausing.             */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    DEPLOY+sfx               = false                      */
/*    MOOS_MANUAL_OVERRIDE+sfx = true                       */
/*    ATAK_MODE+sfx            = false  (side effect)       */
/*    ATAK_WAYPT_ACTIVE+sfx    = false  (side effect)       */
/*    ATAK_CHAT_OUT (via dm)   = "<subject> paused."        */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    pause                                                 */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_PAUSE_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_PAUSE_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class PauseHandler : public CoTCommandHandler
{
public:
  PauseHandler();
  ~PauseHandler() override = default;

  std::string name() const override { return "Pause"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_pause;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_PAUSE_HANDLER_HEADER
