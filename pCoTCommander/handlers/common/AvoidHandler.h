/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AvoidHandler.h                                  */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "avoid on|off" chat command -- toggles      */
/*  collision avoidance behaviors in ATAK mode.             */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Toggles ATAK_AVOID_COLLISIONS+sfx. Gates                */
/*  BHV_AvdColregsV22 and BHV_AvoidCollision -- behaviors   */
/*  in the .bhv file condition on this variable so that     */
/*  collision avoidance can be selectively disabled when    */
/*  deliberately maneuvering in close quarters.             */
/*                                                          */
/*  Defaults on in waypt_atak / meta_surveyor configs;      */
/*  setting it off is an explicit operator decision and is  */
/*  scoped to the ATAK mode -- autonomous strategy mode is  */
/*  unaffected.                                             */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_AVOID_COLLISIONS+sfx  = true | false             */
/*    ATAK_CHAT_OUT (via dm)     = "Collision avoidance     */
/*                                  <state> for <target>."  */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    avoid on                                              */
/*    avoid off                                             */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_AVOID_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_AVOID_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class AvoidHandler : public CoTCommandHandler
{
public:
  AvoidHandler();
  ~AvoidHandler() override = default;

  std::string name() const override { return "Avoid"; }

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

#endif // MOOS_IVP_TAK_COMMON_AVOID_HANDLER_HEADER
