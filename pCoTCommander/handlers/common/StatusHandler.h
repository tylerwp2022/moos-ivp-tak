/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "status" chat command -- DMs a short        */
/*  summary of the current state back to the operator.      */
/*  Read-only: no MOOS publications.                        */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT REPORTS                                         */
/*  =======================================================  */
/*  Shore mode:                                             */
/*    "Deployed: YES" or "Deployed: NO"                     */
/*                                                          */
/*  Vehicle mode:                                           */
/*    "<vehicle> -- Deployed: YES/NO"                       */
/*                                                          */
/*  Future expansion: this handler could report             */
/*  ctx.atak_mode, ctx.tagged, ctx.atak_retry, etc. The     */
/*  current minimal form matches the pre-refactor behavior. */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_CHAT_OUT (via dm) = status summary               */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    status                                                */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class StatusHandler : public CoTCommandHandler
{
public:
  StatusHandler();
  ~StatusHandler() override = default;

  std::string name() const override { return "Status"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_query_count;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER
