/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: WaypointHandler.h                               */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "Go To" CoT (type b-m-p-w-GOTO) from ATAK.  */
/*  Converts lat/lon to local XY via CoTGeodesy and         */
/*  activates the waypt_atak behavior with the target.      */
/*                                                          */
/*  Lives in common/ because waypoint dispatch is a generic */
/*  supervisory primitive -- any future mission with        */
/*  point-to-point operator override wants the same         */
/*  mechanism.                                              */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  On b-m-p-w-GOTO with lat/lon:                           */
/*    1. Reject if not deployed (DM the operator).          */
/*    2. Reject if geodesy not ready (warning).             */
/*    3. Convert lat/lon to local XY.                       */
/*    4. Parse parent_callsign from CoT for the ack DM.     */
/*       Store as ctx.last_operator_callsign so other       */
/*       handlers (FlagPursuit) can DM the same operator.   */
/*    5. Publish:                                           */
/*         ATAK_MODE         = true                         */
/*         ATAK_WAYPT_ACTIVE = true                         */
/*         ATAK_WPT_UPDATE   = "points=x,y # capture_radius=r"*/
/*    6. DM ack to the operator: "ATAK mode active. Moving  */
/*       to <lat>, <lon>."                                  */
/*                                                          */
/*  Also subscribes to ATAK_WPT_REACHED and NAV_SPEED to     */
/*  implement a station-keep-until-settled completion model: */
/*                                                          */
/*    Phase 1: First capture (entered capture_radius).      */
/*       BHV_Waypoint posts ATAK_WPT_REACHED=true. We       */
/*       record the time and start watching NAV_SPEED. The  */
/*       waypoint behavior STAYS ACTIVE (perpetual=true     */
/*       drives the boat back if it overshoots the slip     */
/*       radius -- effectively a station-keep on the pin).  */
/*       No DM yet -- the boat may still be oscillating.    */
/*                                                          */
/*    Phase 2: Sustained settlement.                        */
/*       Watch NAV_SPEED. When it stays below                */
/*       speed_stop_threshold (default 0.1 m/s)             */
/*       CONTINUOUSLY for settle_duration seconds (default  */
/*       3.0), the boat has truly settled at the waypoint.  */
/*       NOW we deactivate ATAK_WAYPT_ACTIVE, DM the        */
/*       operator "Waypoint reached.", reset latches, and   */
/*       return to ATAK idle state. ATAK_MODE remains true  */
/*       (operator still has control), only                 */
/*       ATAK_WAYPT_ACTIVE goes false. The bhv stops        */
/*       firing; the boat sits where it settled.            */
/*                                                          */
/*    Phase 2 timeout: if the boat won't decelerate within  */
/*       post_capture_timeout seconds total (wind/current/  */
/*       sensor noise keeping NAV_SPEED above threshold     */
/*       continuously), force a release with a different DM */
/*       noting the timeout. Prevents the handler from      */
/*       getting stuck in pending state forever.            */
/*                                                          */
/*  Rationale: The previous design (deactivate on first     */
/*  capture, let boat coast) left the boat drifting          */
/*  wherever momentum carried it -- often well outside       */
/*  capture_radius. The station-keep-until-settled design   */
/*  keeps the boat ON the pin during oscillation, so by the */
/*  time we deactivate, the boat is actually parked there.  */
/*  Better for ATAK operator workflow: when you drop a Go-  */
/*  To, the boat ends up at that point, not 10m past it.    */
/*                                                          */
/*  =======================================================  */
/*  CONFIGURATION (.moos ProcessConfig keys)                */
/*  =======================================================  */
/*    capture_radius        = 5.0        // meters          */
/*    waypoint_update_var   = ATAK_WPT_UPDATE               */
/*    enable_waypoint_control = true     // master switch   */
/*    speed_stop_threshold  = 0.1        // m/s             */
/*    settle_duration       = 3.0        // sec, sustained  */
/*    post_capture_timeout  = 30.0       // sec, safety net */
/*                                                          */
/*  Set settle_duration = 0 to release on first             */
/*  sub-threshold sample (matches the pre-May-14 behavior). */
/*                                                          */
/*  =======================================================  */
/*  CoT TYPES CLAIMED                                       */
/*  =======================================================  */
/*    b-m-p-w-GOTO                                          */
/*                                                          */
/*  =======================================================  */
/*  MOOS SUBSCRIPTIONS (handler-specific)                   */
/*  =======================================================  */
/*    ATAK_WPT_REACHED -- endflag from waypt_atak behavior. */
/*                        Triggers phase 1 transition.      */
/*    NAV_SPEED        -- vehicle speed in m/s. Triggers    */
/*                        phase 2 transition when below     */
/*                        threshold.                        */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_WAYPOINT_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_WAYPOINT_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class WaypointHandler : public CoTCommandHandler
{
public:
  WaypointHandler();
  ~WaypointHandler() override = default;

  std::string name() const override { return "Waypoint"; }

  // ---- CoT side ----
  bool claimsCoT(const ParsedCoT& evt) const override;
  bool handleCoT(const ParsedCoT& evt,
                  CommanderContext& ctx) override;

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
  // Capture-radius parameter appended to the BHV_Waypoint
  // update string. Default 15m.
  double      m_capture_radius;

  // Target MOOS variable name for the waypt_atak behavior
  // update line. Must match 'updates = <name>' in the .bhv
  // file. Default: ATAK_WPT_UPDATE.
  std::string m_waypoint_update_var;

  // Master switch -- when false, claimsCoT() returns false
  // for everything (handler is effectively disabled).
  // Useful for shore configs that should NOT receive
  // waypoint CoT (the shore bundle currently excludes this
  // handler entirely, but the switch is here for explicit
  // .moos-time control).
  bool        m_enabled;

  // Phase 2 trigger: when boat speed drops below this
  // threshold (m/s) after a capture event, the deceleration
  // timer starts. Default 0.1 m/s. Set higher if wave action
  // keeps the boat bobbing above this value indefinitely;
  // set lower for very calm water.
  double      m_speed_stop_threshold;

  // Phase 2 trigger duration: NAV_SPEED must stay below the
  // threshold CONTINUOUSLY for this many seconds before we
  // consider the waypoint truly settled. Default 3.0 sec.
  // Set to 0 to release on the first sub-threshold sample
  // (matches the pre-May-14 behavior).
  double      m_settle_duration;

  // Phase 2 safety net: if the boat never sustains a sub-
  // threshold speed for the full settle_duration within this
  // many seconds after the first capture, force the release.
  // Prevents the handler from getting stuck in post-capture
  // state if speed measurements are noisy or currents push
  // the boat continuously. Default 30 seconds.
  double      m_post_capture_timeout;

  // ---- Runtime state ----
  // Guards the "first capture" path so the post-capture
  // pending state initializes once per waypoint. Reset to
  // false on settled (or timeout) so the next waypoint can
  // trigger it again.
  bool        m_wpt_reached_sent;

  // True between Phase 1 (capture detected) and Phase 2
  // (sustained settle or timeout). While true,
  // onMail(NAV_SPEED) tracks the low-speed timer.
  bool        m_post_capture_pending;

  // MOOSTime() at which Phase 1 fired. Used to detect the
  // post_capture_timeout deadline.
  double      m_post_capture_start_time;

  // MOOSTime() at which NAV_SPEED first dropped below the
  // threshold during the current pending state. Negative
  // (< 0) means "not currently below threshold". When NAV_SPEED
  // exceeds threshold while pending, this is reset to -1 so
  // a brief dip doesn't count toward settlement.
  double      m_low_speed_start_time;

  // Most recent NAV_SPEED value (for appcast / debugging).
  double      m_last_speed;

  // ---- Diagnostics ----
  unsigned int m_count;
  unsigned int m_rejected_not_deployed;
  unsigned int m_rejected_no_geodesy;
  std::string  m_last_waypoint;   // "lat,lon -> x,y  sender=X"
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_WAYPOINT_HANDLER_HEADER
