/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*    REV:  May 14, 2026 -- expanded to report task & state */
/*                                                          */
/*  Handles the "status" chat command -- DMs a summary of   */
/*  the vehicle's current operational state and inferred    */
/*  task back to the operator. Read-only: no MOOS           */
/*  publications.                                           */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT REPORTS                                         */
/*  =======================================================  */
/*  Vehicle mode:                                           */
/*    <callsign>:                                           */
/*      Deployed: YES/NO                                    */
/*      Mode:     Autonomous | ATAK control | Parked        */
/*      Task:     <derived from state>                      */
/*      Action:   <raw ACTION value, when autonomous>       */
/*      Tagged:   YES/NO                                    */
/*      Has flag: YES/NO                                    */
/*                                                          */
/*  Shore mode: game state + usage hint                     */
/*    Shore can't easily see per-vehicle state without      */
/*    additional plumbing, so it reports AQUATICUS_GAME     */
/*    and suggests sending 'status' to the vehicle's        */
/*    chatroom for detailed state.                          */
/*                                                          */
/*  =======================================================  */
/*  TASK DERIVATION                                         */
/*  =======================================================  */
/*  Priority order (first match wins):                      */
/*    1. !deployed              -> "Idle (not deployed)"    */
/*    2. tagged && !auto_untag -> "Tagged, holding (untag   */
/*                                  off)"                   */
/*    3. tagged                 -> "Tagged, recovering"     */
/*    4. atak_mode && pursuit  -> "Pursuing flag (ATAK      */
/*                                  auto)"                  */
/*    5. atak_mode && waypt    -> "Navigating to operator   */
/*                                  waypoint"               */
/*    6. atak_mode             -> "Awaiting operator        */
/*                                  command"                */
/*    7. action=ATTACK_*       -> "Attacking flag"          */
/*    8. action=DEFEND_*       -> "Defending zone"          */
/*    9. else                  -> "Autonomous (no role      */
/*                                  assigned)"              */
/*                                                          */
/*  =======================================================  */
/*  MOOS SUBSCRIPTIONS                                      */
/*  =======================================================  */
/*    ACTION             -- current role (ATTACK_E, etc.)   */
/*    ATAK_WAYPT_ACTIVE  -- operator has live waypoint?     */
/*    ATAK_FLAG_PURSUIT  -- auto flag pursuit active?       */
/*    ATAK_AUTO_UNTAG    -- auto-untag enabled?             */
/*    HAS_FLAG           -- vehicle currently carrying flag?*/
/*    AQUATICUS_GAME     -- game state (shore mode only)    */
/*                                                          */
/*  Note: DEPLOY, ATAK_MODE, TAGGED, ATAK_RETRY come from   */
/*  ctx -- dispatcher already mirrors them.                 */
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

  // ---- Lifecycle: subscribe to operational state vars ----
  void registerSubs(std::vector<std::string>& subs) override;
  void onMail(const std::string& key,
               const std::string& value,
               CommanderContext& ctx) override;

  void appcast(std::string& report) const override;

private:
  // Tracked state (updated from MOOS mail). All initialized
  // in the constructor; we don't trust the subscribed
  // variable to be set before the first status query.
  std::string m_action;            // empty = no role assigned
  bool        m_atak_waypt_active;
  bool        m_atak_flag_pursuit;
  bool        m_atak_auto_untag;   // default true (matches handler)
  bool        m_has_flag;
  std::string m_aquaticus_game;    // empty | play | pause | stop

  // Diagnostic
  unsigned int m_query_count;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER
