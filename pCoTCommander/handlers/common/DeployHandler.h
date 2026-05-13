/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: DeployHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "deploy" chat command.                      */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Starts (or restarts) vehicle motion. The MOOS           */
/*  publication trio below puts the addressed vehicle(s)    */
/*  into the deployed state with no station-keep, no        */
/*  return-to-base, and no manual override -- the helm is   */
/*  free to run mission behaviors.                          */
/*                                                          */
/*  This is a faithful port of the "deploy" branch from     */
/*  the pre-refactor CoTCommander::handleChatCommand().     */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*  Publication targets depend on the dispatcher-resolved   */
/*  ChatMessage::sfx:                                       */
/*    fleet mode, no target  -> sfx = "_ALL"                */
/*    fleet mode, vehicle X  -> sfx = "_<VEHICLE_X>"        */
/*    vehicle mode           -> sfx = "" (bare)             */
/*                                                          */
/*  Published variables (in order):                         */
/*    DEPLOY+sfx                  = true                    */
/*    MOOS_MANUAL_OVERRIDE+sfx    = false                   */
/*    RETURN+sfx                  = false                   */
/*    ATAK_CHAT_OUT (via ctx.dm)  = "Deploying <target>."   */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    deploy                                                */
/*                                                          */
/*  =======================================================  */
/*  EXAMPLES                                                */
/*  =======================================================  */
/*  Shore mode:                                             */
/*    operator types: "deploy"                              */
/*      -> DEPLOY_ALL=true                                  */
/*         MOOS_MANUAL_OVERRIDE_ALL=false                   */
/*         RETURN_ALL=false                                 */
/*         DM: "Deploying all vehicles."                    */
/*    operator types: "blue_one deploy"                     */
/*      -> DEPLOY_BLUE_ONE=true (etc.)                      */
/*         DM: "Deploying blue_one."                        */
/*                                                          */
/*  Vehicle mode:                                           */
/*    operator types: "deploy" in chatroom $(VNAME)         */
/*      -> DEPLOY=true (etc.) on the vehicle's MOOSDB       */
/*         DM: "Deploying vehicle."                         */
/*                                                          */
/*  =======================================================  */
/*  USAGE / INPUT VALIDATION                                */
/*  =======================================================  */
/*  The command takes no arguments. "deploy now" produces   */
/*  a usage DM rather than silently deploying -- this is    */
/*  stricter behavior than letting unknown-suffix variants  */
/*  pass through, but matches the pre-refactor strict       */
/*  equality check (cmd == "deploy") and gives the          */
/*  operator a clear hint when they mis-type.               */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_DEPLOY_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_DEPLOY_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class DeployHandler : public CoTCommandHandler
{
public:
  DeployHandler();
  ~DeployHandler() override = default;

  // ========================================================
  // Identification
  // ========================================================
  std::string name() const override { return "Deploy"; }

  // ========================================================
  // Chat dispatch
  // ========================================================
  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  // ========================================================
  // Status
  // ========================================================
  void appcast(std::string& report) const override;

private:
  // Count of successful deploy commands processed.
  unsigned int m_deploy_count;

  // Count of malformed deploy commands rejected
  // (e.g. "deploy now" with unexpected arguments).
  unsigned int m_reject_count;

  // Human-readable summary of the most recent successful
  // deploy (target_label + sender) for AppCast visibility.
  // Empty until the first successful command.
  std::string  m_last_deploy;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_DEPLOY_HANDLER_HEADER
