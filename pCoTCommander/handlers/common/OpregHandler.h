/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: OpregHandler.h                                  */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "opreg on|off" chat command -- toggles      */
/*  OpRegion boundary recovery in ATAK mode.                */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Toggles ATAK_OPREG_RECOVER+sfx. Gates                   */
/*  BHV_OpRegionRecover -- the boundary enforcement         */
/*  behavior that pulls the vehicle back when it leaves     */
/*  the operating region (e.g. after a waypoint placed near */
/*  the edge of the field).                                 */
/*                                                          */
/*  Defaults on. Scoped to ATAK mode -- recovery always     */
/*  runs in autonomous strategy mode regardless of this     */
/*  setting.                                                */
/*                                                          */
/*  IMPORTANT: turning opreg OFF emits a WARNING DM         */
/*  instead of the usual confirmation, since the vehicle    */
/*  can then leave the field entirely.                      */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_OPREG_RECOVER+sfx  = true | false                */
/*    ATAK_CHAT_OUT (via dm)  = "Boundary recovery on for   */
/*                              <target>."                  */
/*                            OR                            */
/*                            = "WARNING: Boundary          */
/*                              recovery off for <target>.  */
/*                              Robot may leave the field." */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    opreg on                                              */
/*    opreg off                                             */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_OPREG_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_OPREG_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class OpregHandler : public CoTCommandHandler
{
public:
  OpregHandler();
  ~OpregHandler() override = default;

  std::string name() const override { return "Opreg"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count_on;
  unsigned int m_count_off;
  unsigned int m_reject_count;
  std::string  m_last_toggle;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_OPREG_HANDLER_HEADER
