/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: RetryHandler.h                                  */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "retry on|off" chat command -- toggles      */
/*  automatic waypoint resumption after tag recovery.       */
/*                                                          */
/*  Mission-specific (Aquaticus): predicates on the         */
/*  tag/untag cycle of CTF play.                            */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Toggles ATAK_RETRY+sfx.                                 */
/*  "retry on"  (default) -- after BHV_HomeReturn completes */
/*    a tag-recovery cycle, waypt_atak reactivates          */
/*    automatically and the vehicle re-attempts the         */
/*    original ATAK waypoint.                               */
/*  "retry off" -- after tag recovery, ATAK_WAYPT_ACTIVE    */
/*    is cleared and the operator gets a DM. The vehicle    */
/*    holds at home until the operator sends a new          */
/*    waypoint. Use when reassessment is needed before      */
/*    committing again.                                     */
/*                                                          */
/*  =======================================================  */
/*  STATE INTERACTION                                       */
/*  =======================================================  */
/*  The pre-refactor code mirrored the new value into a     */
/*  local m_atak_retry member synchronously for the         */
/*  vehicle-side untagged-transition logic.                 */
/*                                                          */
/*  Post-refactor, that state lives in                      */
/*  CommanderContext::atak_retry, which the dispatcher      */
/*  updates when MOOSDB publishes the new ATAK_RETRY value  */
/*  back -- i.e. one iterate tick after the publish here.   */
/*  This is acceptable: the retry flag is consulted on the  */
/*  tagged->untagged transition, which occurs at game-event */
/*  rate (seconds), not at iterate rate (10 Hz). The one-   */
/*  tick lag is invisible.                                  */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_RETRY+sfx         = true | false                 */
/*    ATAK_CHAT_OUT (via dm) = "Retry <state> for           */
/*                              <target>."                  */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    retry on                                              */
/*    retry off                                             */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_RETRY_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_RETRY_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class RetryHandler : public CoTCommandHandler
{
public:
  RetryHandler();
  ~RetryHandler() override = default;

  std::string name() const override { return "Retry"; }

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

#endif // MOOS_IVP_TAK_AQUATICUS_RETRY_HANDLER_HEADER
