/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: UntagHandler.h                                  */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "untag on|off" chat command -- toggles      */
/*  automatic tag-recovery behavior in ATAK mode.           */
/*                                                          */
/*  Mission-specific: the concept of being "tagged" is      */
/*  Aquaticus CTF terminology. The handler lives in the     */
/*  aquaticus/ subfolder and is included only in the        */
/*  Aquaticus bundles.                                      */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Toggles ATAK_AUTO_UNTAG+sfx.                            */
/*  "untag on"  (default) -- when tagged in ATAK mode, the  */
/*    vehicle automatically returns to its home flag zone   */
/*    to get untagged, then resumes the ATAK waypoint.      */
/*  "untag off" -- the vehicle ignores tag events and       */
/*    stays on the ATAK waypoint. Use when you want full    */
/*    manual control over tag recovery.                     */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_AUTO_UNTAG+sfx    = true | false                 */
/*    ATAK_CHAT_OUT (via dm) = "Auto-untag <state> for      */
/*                             <target>."                   */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    untag on                                              */
/*    untag off                                             */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_UNTAG_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_UNTAG_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class UntagHandler : public CoTCommandHandler
{
public:
  UntagHandler();
  ~UntagHandler() override = default;

  std::string name() const override { return "Untag"; }

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

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_UNTAG_HANDLER_HEADER
