/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: FlagPursuitHandler.h                            */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the Aquaticus flag CoT (b-m-p-s-m with a        */
/*  matching uid). Automatically pursues the opponent's     */
/*  flag when its position is broadcast over CoT.           */
/*                                                          */
/*  VEHICLE-ONLY. Listed only in the Aquaticus vehicle      */
/*  bundle. Operates as an autonomous-pursuit assist on     */
/*  top of operator supervisory control: the operator can   */
/*  still send waypoints / resume / etc. and override the   */
/*  pursuit.                                                */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  When a b-m-p-s-m CoT arrives with uid matching the      */
/*  configured flag_uid (typically "aquaticus-flag-red"):   */
/*                                                          */
/*    1. Skip if marked as a self-broadcast                 */
/*       (<_aquaticus_graphics sa_broadcast="true"/>);      */
/*       these are map-display broadcasts from              */
/*       pCoTGraphics, not operator pursuit commands.       */
/*    2. Skip if the flag's uid contains this vehicle's     */
/*       own team name (we'd be pursuing our own flag).    */
/*    3. Skip if not deployed.                              */
/*    4. Skip if geodesy not ready.                         */
/*    5. Skip if the flag position hasn't changed enough    */
/*       since the last accepted update                     */
/*       (FLAG_POS_THRESHOLD, ~0.5m at Lake Popolopen).     */
/*    6. Otherwise:                                         */
/*         - Publish ATAK_MODE = true                       */
/*         - Publish ATAK_WAYPT_ACTIVE = true               */
/*         - Publish ATAK_FLAG_PURSUIT = true               */
/*         - Publish waypoint_update_var with the flag's    */
/*           local XY and flag_capture_radius (smaller      */
/*           than the general capture_radius -- we want     */
/*           to drive INTO the grab zone, not stop short).  */
/*         - DM the operator ONCE per pursuit (subsequent   */
/*           flag re-broadcasts are silent).                */
/*                                                          */
/*  Subscribes to the configured team_flag_vars list (e.g.  */
/*  HAS_FLAG_BLUE_ONE, HAS_FLAG_BLUE_TWO, ...). When any    */
/*  goes true, a teammate has secured the flag and pursuit  */
/*  ends silently -- m_pursuing reset to false, next        */
/*  position update will start a fresh pursuit.             */
/*                                                          */
/*  =======================================================  */
/*  WHY BYPASS THE OPERATOR-UID FILTER                      */
/*  =======================================================  */
/*  Flag-position CoT comes from pCoTGraphics or the        */
/*  Aquaticus referee, not an ATAK operator. The event uid  */
/*  won't match the operator_uid_filter substring. Override */
/*  bypassOperatorFilter() to true so this handler still    */
/*  sees the events.                                        */
/*                                                          */
/*  =======================================================  */
/*  CONFIGURATION (.moos ProcessConfig keys)                */
/*  =======================================================  */
/*    flag_pursuit_enabled = true                           */
/*    flag_uid             = aquaticus-flag-red             */
/*    flag_my_team         = blue   (lowercase team name)   */
/*    flag_capture_radius  = 5.0    (meters)                */
/*    team_flag_vars       = HAS_FLAG_BLUE_ONE,             */
/*                            HAS_FLAG_BLUE_TWO,            */
/*                            HAS_FLAG_BLUE_THREE           */
/*    waypoint_update_var  = ATAK_WPT_UPDATE  (shared with  */
/*                            WaypointHandler)              */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_FLAG_PURSUIT_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_FLAG_PURSUIT_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class FlagPursuitHandler : public CoTCommandHandler
{
public:
  FlagPursuitHandler();
  ~FlagPursuitHandler() override = default;

  std::string name() const override { return "FlagPursuit"; }

  // ---- CoT side ----
  bool claimsCoT(const ParsedCoT& evt) const override;
  bool handleCoT(const ParsedCoT& evt,
                  CommanderContext& ctx) override;
  bool bypassOperatorFilter() const override { return true; }

  // ---- Lifecycle ----
  void configure(const std::string& key,
                  const std::string& value) override;
  void registerSubs(std::vector<std::string>& subs) override;
  void onMail(const std::string& key,
               const std::string& value,
               CommanderContext& ctx) override;

  void appcast(std::string& report) const override;

private:
  // ---- Configuration ----
  bool        m_enabled;             // master switch
  std::string m_flag_uid;            // CoT uid to match (substring? no, exact)
  std::string m_my_team;             // lowercase, e.g. "blue"
  double      m_flag_capture_radius; // meters, default 5.0
  std::vector<std::string> m_team_flag_vars;  // HAS_FLAG_BLUE_ONE etc.
  std::string m_waypoint_update_var; // default ATAK_WPT_UPDATE

  // Minimum flag-position change (degrees) that triggers a
  // new waypoint update. ~0.5m at Lake Popolopen latitude.
  // Flag is stationary in normal play; this guards against
  // floating-point noise from repeated CoT broadcasts.
  static constexpr double kFlagPosThreshold = 0.000005;

  // ---- Runtime state ----
  bool        m_pursuing;            // currently chasing the flag
  bool        m_pursuit_notified;    // DM'd the operator this pursuit
  double      m_last_lat;
  double      m_last_lon;

  // ---- Diagnostics ----
  unsigned int m_pursuits_started;
  unsigned int m_pursuits_ended;     // teammate-captured terminations
  unsigned int m_updates_silent;     // position-changed-but-no-DM
  std::string  m_last_action;
};

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_FLAG_PURSUIT_HANDLER_HEADER
