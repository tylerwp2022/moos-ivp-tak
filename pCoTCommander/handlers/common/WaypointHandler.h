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
/*  Also subscribes to ATAK_WPT_REACHED. When the           */
/*  waypt_atak behavior captures the point, its endflag     */
/*  posts ATAK_WPT_REACHED=true; this handler DMs           */
/*  "Waypoint reached." to the operator and resets the      */
/*  guard so the next waypoint can fire it again.           */
/*                                                          */
/*  =======================================================  */
/*  CONFIGURATION (.moos ProcessConfig keys)                */
/*  =======================================================  */
/*    capture_radius        = 15.0       // meters          */
/*    waypoint_update_var   = ATAK_WPT_UPDATE               */
/*    enable_waypoint_control = true     // master switch   */
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
/*                        Triggers acknowledgment DM.       */
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

  // ---- Runtime state ----
  // Guards the "waypoint reached" DM so it doesn't repeat
  // while the vehicle idles at the capture point and
  // waypt_atak (perpetual=true) re-fires the endflag.
  // Reset to false on every new accepted waypoint.
  bool        m_wpt_reached_sent;

  // ---- Diagnostics ----
  unsigned int m_count;
  unsigned int m_rejected_not_deployed;
  unsigned int m_rejected_no_geodesy;
  std::string  m_last_waypoint;   // "lat,lon -> x,y  sender=X"
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_WAYPOINT_HANDLER_HEADER
