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
/*    5. Resolve effective waypoint mode:                   */
/*       (a) Parse <remarks>...</remarks> for #fast /       */
/*           #precise / #hold tags. If found, use that      */
/*           mode for this one waypoint only (per-shot      */
/*           override).                                     */
/*       (b) Otherwise use m_current_mode, last set by      */
/*           WptModeHandler via WPT_MODE mail.              */
/*       (c) Otherwise use m_default_mode (config).         */
/*    6. Build BHV_Waypoint update string based on mode:    */
/*         fast:    speed=<default>                          */
/*         precise: speed=<default>  (will be reduced on    */
/*                                   approach by Iterate)   */
/*         hold:    same as precise                          */
/*    7. Publish ATAK_MODE, ATAK_WAYPT_ACTIVE, ATAK_WPT_UPDATE. */
/*    8. DM ack mentioning the active mode.                 */
/*                                                          */
/*  =======================================================  */
/*  THE THREE WAYPOINT MODES                                */
/*  =======================================================  */
/*  fast:                                                   */
/*    Transit at default speed. On first capture            */
/*    (ATAK_WPT_REACHED=true), immediately clear            */
/*    ATAK_WAYPT_ACTIVE. Behavior deactivates, boat coasts. */
/*    DM "Waypoint reached." right away.                    */
/*                                                          */
/*  precise:                                                */
/*    Transit at default speed. When boat enters            */
/*    (capture_radius + approach_buffer), update the bhv    */
/*    speed to precise_approach_speed. Behavior continues   */
/*    to drive the boat (station-keep via perpetual=true).  */
/*    Once NAV_SPEED stays sub-threshold continuously for   */
/*    settle_duration seconds, deactivate the behavior and  */
/*    DM "Waypoint reached.". Boat parks at the pin.        */
/*                                                          */
/*  hold:                                                   */
/*    Same approach phase as precise (slow-down + settle    */
/*    detection). But after settlement, the behavior is NOT */
/*    deactivated -- ATAK_WAYPT_ACTIVE stays true and the   */
/*    bhv keeps actively returning the boat to the pin if   */
/*    wind/current displaces it. Operator must type         */
/*    'resume' to release.                                  */
/*                                                          */
/*  Per-shot override tags in CoT remarks:                  */
/*    "#fast"     -- force fast for this one waypoint       */
/*    "#precise"  -- force precise                          */
/*    "#hold"     -- force hold                             */
/*  Tags are case-insensitive, '#' optional. Without a tag, */
/*  the sticky WPT_MODE applies.                            */
/*                                                          */
/*  =======================================================  */
/*  STATION-KEEP-UNTIL-SETTLED LOGIC (precise/hold)         */
/*  =======================================================  */
/*    Phase 1: First capture (entered capture_radius).      */
/*       BHV_Waypoint posts ATAK_WPT_REACHED=true. We       */
/*       record the time and start watching NAV_SPEED. The  */
/*       waypoint behavior STAYS ACTIVE (perpetual=true     */
/*       drives the boat back if it overshoots).            */
/*       No DM yet -- the boat may still be oscillating.    */
/*                                                          */
/*    Phase 2: Sustained settlement.                        */
/*       Watch NAV_SPEED. When it stays below                */
/*       speed_stop_threshold (default 0.1 m/s)             */
/*       CONTINUOUSLY for settle_duration seconds (default  */
/*       3.0), the boat has truly settled at the waypoint.  */
/*         precise: clear ATAK_WAYPT_ACTIVE (release).       */
/*         hold:    leave ATAK_WAYPT_ACTIVE=true (keep      */
/*                  station-keeping; operator must resume). */
/*       DM "Waypoint reached." in both cases.              */
/*                                                          */
/*    Phase 2 timeout: if the boat won't decelerate within  */
/*       post_capture_timeout seconds total, force a        */
/*       release with a different DM noting the timeout.    */
/*                                                          */
/*  =======================================================  */
/*  APPROACH SLOWDOWN (precise/hold only)                   */
/*  =======================================================  */
/*  The handler subscribes to NAV_X / NAV_Y. On every       */
/*  NAV_X mail (or NAV_Y -- both arrive together each       */
/*  iteration), it computes distance to the current target. */
/*  When distance < (capture_radius + approach_buffer) and  */
/*  the boat is currently in "fast approach" sub-state, the */
/*  handler republishes ATAK_WPT_UPDATE with the lower      */
/*  speed (precise_approach_speed). BHV_Waypoint reads the  */
/*  update via its 'updates' parameter and decelerates.     */
/*                                                          */
/*  This is closed-loop slowdown done outside the behavior. */
/*  BHV_Waypoint has no built-in deceleration model -- it   */
/*  runs at the configured speed for the whole leg. The     */
/*  handler does the math and updates the speed in time.    */
/*                                                          */
/*  =======================================================  */
/*  CONFIGURATION (.moos ProcessConfig keys)                */
/*  =======================================================  */
/*    capture_radius          = 15.0     // meters          */
/*    waypoint_update_var     = ATAK_WPT_UPDATE             */
/*    enable_waypoint_control = true     // master switch   */
/*    speed_stop_threshold    = 0.1      // m/s             */
/*    settle_duration         = 3.0      // sec, sustained  */
/*    post_capture_timeout    = 30.0     // sec, safety net */
/*    default_wpt_mode        = fast     // before WPT_MODE  */
/*    default_speed           = 2.0      // m/s, normal cruise*/
/*    precise_approach_speed  = 1.0      // m/s, in approach */
/*    approach_buffer         = 5.0      // m, slowdown trigger:*/
/*                                       //   range = cap_radius + buffer*/
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
/*                        phase 2 transition.               */
/*    NAV_X, NAV_Y     -- vehicle position. Triggers        */
/*                        approach slowdown.                */
/*    WPT_MODE         -- sticky mode set by WptModeHandler.*/
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
  // ---- Helpers ----
  // Resolve effective mode: per-shot override > sticky >
  // default. Returns one of "fast" / "precise" / "hold".
  std::string resolveMode(const std::string& raw_xml) const;

  // Extract the per-shot override tag from CoT remarks.
  // Returns "fast" / "precise" / "hold" if a tag is found,
  // empty string if not. Searches for #fast / #precise /
  // #hold (case-insensitive, # optional). The remarks body
  // is <remarks>...</remarks> in the CoT XML.
  std::string extractModeOverride(const std::string& raw_xml) const;

  // Build the ATAK_WPT_UPDATE string. speed parameter is the
  // m/s to embed; pass m_default_speed for cruise,
  // m_precise_approach_speed for slowdown.
  std::string buildUpdate(double x, double y, double speed) const;

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

  // Default mode used when no chat command has set WPT_MODE
  // and no per-shot override tag is present in the CoT.
  // Should be "fast" / "precise" / "hold".
  std::string m_default_mode;

  // Normal cruise speed (m/s) sent in the initial
  // ATAK_WPT_UPDATE for all modes. The .bhv default is 3.5
  // m/s but explicit speed= in the update overrides that.
  double      m_default_speed;

  // Speed applied when within approach range (precise/hold
  // modes only). Default 1.0 m/s. Lower for tighter parking
  // accuracy; higher for less overshoot tolerance.
  double      m_precise_approach_speed;

  // Slowdown trigger distance is (m_capture_radius + this).
  // With cap_radius=15 and buffer=5, slowdown engages 20m
  // from the pin. Scales naturally with how precise the
  // operator already configured the capture radius.
  double      m_approach_buffer;

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

  // Most recent NAV_X / NAV_Y (for approach slowdown calc).
  double      m_nav_x;
  double      m_nav_y;
  bool        m_nav_valid;   // false until first NAV_X & NAV_Y arrive

  // Current sticky mode (updated by WPT_MODE mail). The
  // effective mode for any given waypoint is this OR a per-
  // shot override tag in the CoT remarks.
  std::string m_current_mode;

  // Effective mode for the currently-active waypoint
  // (resolved at CoT receipt time from override-or-sticky).
  // Reset to "" between waypoints. Different from m_current_mode
  // because per-shot override only affects one waypoint.
  std::string m_active_mode;

  // Target waypoint in local XY for approach distance calc.
  // Valid while m_active_mode is non-empty.
  double      m_target_x;
  double      m_target_y;

  // Approach slowdown latch -- true once we've published the
  // slow-speed update for the current waypoint. Prevents
  // repeated publication on every NAV_X mail. Reset on new
  // waypoint.
  bool        m_approach_slowdown_applied;

  // ---- Diagnostics ----
  unsigned int m_count;
  unsigned int m_rejected_not_deployed;
  unsigned int m_rejected_no_geodesy;
  std::string  m_last_waypoint;   // "lat,lon -> x,y  sender=X"
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_WAYPOINT_HANDLER_HEADER
