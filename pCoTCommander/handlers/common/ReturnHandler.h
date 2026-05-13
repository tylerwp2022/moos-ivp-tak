/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ReturnHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "return" / "rtb" chat command.              */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Orders the addressed vehicle(s) to return to base. The  */
/*  vehicle remains DEPLOY=true (helm active) and           */
/*  MOOS_MANUAL_OVERRIDE=false (helm in command); RETURN    */
/*  toggles the BHV_Waypoint behavior into its return       */
/*  configuration. As a SIDE EFFECT this also exits ATAK    */
/*  mode (ATAK_MODE=false, ATAK_WAYPT_ACTIVE=false) so the  */
/*  autonomous return is not blocked by waypt_atak holding  */
/*  the helm in place.                                      */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    DEPLOY+sfx               = true                       */
/*    MOOS_MANUAL_OVERRIDE+sfx = false                      */
/*    RETURN+sfx               = true                       */
/*    ATAK_MODE+sfx            = false  (side effect)       */
/*    ATAK_WAYPT_ACTIVE+sfx    = false  (side effect)       */
/*    ATAK_CHAT_OUT (via dm)   = "<subject> returning to    */
/*                                base."                    */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    return                                                */
/*    rtb     (alias)                                       */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_RETURN_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_RETURN_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class ReturnHandler : public CoTCommandHandler
{
public:
  ReturnHandler();
  ~ReturnHandler() override = default;

  std::string name() const override { return "Return"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_return;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_RETURN_HANDLER_HEADER
