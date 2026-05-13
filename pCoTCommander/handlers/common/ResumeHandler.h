/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ResumeHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "resume" chat command -- exits ATAK mode    */
/*  and resumes autonomous play.                            */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Clears ATAK_MODE and ATAK_WAYPT_ACTIVE. Game behaviors  */
/*  (attack/defend/loiter) resume on the next pHelmIvP      */
/*  iterate tick. Any pending operator-supplied waypoint    */
/*  is dropped along with the mode -- a follow-up "atak" or */
/*  "Send To" is required to re-enter supervisory control.  */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_MODE+sfx          = false                        */
/*    ATAK_WAYPT_ACTIVE+sfx  = false                        */
/*    ATAK_CHAT_OUT (via dm) = "<subject> resuming           */
/*                             autonomous strategy."        */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    resume                                                */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_RESUME_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_RESUME_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class ResumeHandler : public CoTCommandHandler
{
public:
  ResumeHandler();
  ~ResumeHandler() override = default;

  std::string name() const override { return "Resume"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_resume;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_RESUME_HANDLER_HEADER
