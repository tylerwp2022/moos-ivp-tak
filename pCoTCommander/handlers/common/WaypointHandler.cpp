/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: WaypointHandler.cpp                             */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Faithful port of CoTCommander::handleWaypointCoT() and  */
/*  the ATAK_WPT_REACHED branch of OnNewMail() from the     */
/*  pre-refactor code.                                      */
/************************************************************/

#include <cstdlib>      // atof
#include <string>

#include "MBUtils.h"           // doubleToStringX
#include "CoTGeodesy.h"        // ctx.geodesy methods

#include "WaypointHandler.h"
#include "../../CoTUtils.h"    // cot::extractAttr

namespace common {

WaypointHandler::WaypointHandler()
  : m_capture_radius(15.0),
    m_waypoint_update_var("ATAK_WPT_UPDATE"),
    m_enabled(true),
    m_speed_stop_threshold(0.1),
    m_settle_duration(3.0),
    m_post_capture_timeout(30.0),
    m_wpt_reached_sent(false),
    m_post_capture_pending(false),
    m_post_capture_start_time(0.0),
    m_low_speed_start_time(-1.0),
    m_last_speed(0.0),
    m_count(0),
    m_rejected_not_deployed(0),
    m_rejected_no_geodesy(0)
{}


// ============================================================
// CoT claim -- b-m-p-w-GOTO with a position
// ============================================================

bool WaypointHandler::claimsCoT(const ParsedCoT& evt) const
{
  return m_enabled
      && evt.type == "b-m-p-w-GOTO"
      && evt.has_position;
}


// ============================================================
// handleCoT() -- process an operator Go-To
// ============================================================
//
// Faithful port of pre-refactor handleWaypointCoT(). See
// the header for the publication sequence and the geodesy
// + deployment preconditions.

bool WaypointHandler::handleCoT(const ParsedCoT& evt,
                                 CommanderContext& ctx)
{
  // ----------------------------------------------------------
  // Sender callsign (used for ack DM and as a shared
  // scratchpad value other handlers can read).
  // ----------------------------------------------------------
  std::string sender = cot::extractAttr(evt.raw_xml, "parent_callsign");
  std::string chat_dest = sender.empty() ? "All Chat Rooms" : sender;

  // ----------------------------------------------------------
  // Reject if not deployed. Operator must press Deploy
  // before any waypoint can be accepted -- otherwise the
  // helm is parked and the update would just queue.
  // ----------------------------------------------------------
  if(!ctx.deployed) {
    ctx.dm("Deploy robots before sending waypoints.", chat_dest);
    m_rejected_not_deployed++;
    ctx.dlog("WaypointHandler: rejected -- DEPLOY=false");
    return false;
  }

  // ----------------------------------------------------------
  // Reject if geodesy isn't ready. First NODE_REPORT
  // establishes the LatOrigin; before that we can't convert
  // lat/lon to local XY.
  // ----------------------------------------------------------
  if(!ctx.geodesy_ready || !ctx.geodesy) {
    ctx.dm("Waypoint received but geodesy not ready. Waiting for GPS fix.",
           chat_dest);
    m_rejected_no_geodesy++;
    ctx.dlog("WaypointHandler: rejected -- geodesy not ready");
    return false;
  }

  // ----------------------------------------------------------
  // Convert to local XY. CoTGeodesy returns false on
  // failure (e.g. lat/lon out of the projection's valid
  // range).
  // ----------------------------------------------------------
  double x = 0.0, y = 0.0;
  if(!ctx.geodesy->latLonToLocalXY(evt.lat, evt.lon, x, y)) {
    ctx.dm("Waypoint conversion failed -- could not compute local XY.",
           chat_dest);
    m_rejected_no_geodesy++;
    ctx.dlog("WaypointHandler: latLonToLocalXY failed");
    return false;
  }

  // ----------------------------------------------------------
  // Update shared scratchpad so other handlers (e.g.
  // FlagPursuitHandler) can DM the same operator.
  // ----------------------------------------------------------
  if(!sender.empty())
    ctx.last_operator_callsign = sender;

  // New waypoint -> reset both phase latches AND the low-
  // speed timer so the next capture fires a fresh phase 1
  // -> phase 2 sequence. If the operator sends a new
  // waypoint while we're still in post-capture pending state
  // (boat hasn't fully settled yet), the new mission takes
  // priority -- we abandon the old one's settlement tracking.
  m_wpt_reached_sent      = false;
  m_post_capture_pending  = false;
  m_low_speed_start_time  = -1.0;

  // ----------------------------------------------------------
  // Build the BHV_Waypoint update line and publish the
  // activation trio.
  // ----------------------------------------------------------
  std::string update = "points="            + doubleToStringX(x, 2) +
                       ","                  + doubleToStringX(y, 2) +
                       " # capture_radius=" + doubleToStringX(m_capture_radius, 1);

  ctx.publish("ATAK_MODE",            "true");
  ctx.publish("ATAK_WAYPT_ACTIVE",    "true");
  // Vehicle-side bare _STATE mirrors. uFldNodeBroker bridges
  // these up to shore as ATAK_*_STATE_<VNAME_UPPER>, where
  // shore StatusHandler reads them per-vehicle. Without this,
  // operator-driven Go-To events on this vehicle would be
  // invisible to shore-side status queries by other operators.
  ctx.publish("ATAK_MODE_STATE",         "true");
  ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "true");
  ctx.publish(m_waypoint_update_var,  update);

  // ----------------------------------------------------------
  // Acknowledgment DM with the lat/lon. ATAK clients use
  // this to confirm the boat got the order.
  // ----------------------------------------------------------
  std::string lat_str = doubleToStringX(evt.lat, 5);
  std::string lon_str = doubleToStringX(evt.lon, 5);
  ctx.dm("ATAK mode active. Moving to " + lat_str + ", " + lon_str + ".",
         chat_dest);

  m_count++;
  m_last_waypoint = lat_str + "," + lon_str +
                    " -> " + doubleToStringX(x, 2) + "," +
                             doubleToStringX(y, 2) +
                    "  sender=" + chat_dest;

  ctx.dlog("WaypointHandler: " + m_waypoint_update_var + "=" + update +
           " sender=" + chat_dest);
  return true;
}


// ============================================================
// configure() -- consume .moos ProcessConfig keys
// ============================================================

void WaypointHandler::configure(const std::string& key,
                                 const std::string& value)
{
  std::string k = tolower(key);
  if(k == "capture_radius") {
    double r = atof(value.c_str());
    if(r > 0.0) m_capture_radius = r;
  }
  else if(k == "waypoint_update_var") {
    if(!value.empty()) m_waypoint_update_var = value;
  }
  else if(k == "enable_waypoint_control") {
    m_enabled = (tolower(value) == "true");
  }
  else if(k == "speed_stop_threshold") {
    double v = atof(value.c_str());
    // Reject negative; allow 0.0 to "disable" the wait (DM
    // fires only on timeout). Sensible practical range is
    // 0.05 -- 1.0 m/s.
    if(v >= 0.0) m_speed_stop_threshold = v;
  }
  else if(k == "settle_duration") {
    double v = atof(value.c_str());
    // Reject negative; allow 0.0 to release on the first
    // sub-threshold sample (matches pre-May-14 behavior).
    // Typical range is 1-5 seconds.
    if(v >= 0.0) m_settle_duration = v;
  }
  else if(k == "post_capture_timeout") {
    double v = atof(value.c_str());
    if(v > 0.0) m_post_capture_timeout = v;
  }
  // Unknown keys are silently ignored -- other handlers may
  // own them.
}


// ============================================================
// registerSubs() -- ATAK_WPT_REACHED endflag + NAV_SPEED
// ============================================================
//
// ATAK_WPT_REACHED: BHV_Waypoint's endflag, posted when the
//   boat enters capture_radius. Triggers phase 1 (deactivate).
//
// NAV_SPEED: vehicle speed in m/s, from uSimMarine (sim) or
//   iM1_8 (hardware). Triggers phase 2 (DM + release) when
//   it drops below m_speed_stop_threshold during pending stop.

void WaypointHandler::registerSubs(std::vector<std::string>& subs)
{
  subs.push_back("ATAK_WPT_REACHED");
  subs.push_back("NAV_SPEED");
}


// ============================================================
// onMail() -- station-keep-until-settled logic
// ============================================================
//
// Phase 1 (ATAK_WPT_REACHED=true, first time):
//   - Mark m_post_capture_pending so we begin watching speed.
//   - Record start time for the safety timeout.
//   - Reset ATAK_WPT_REACHED to false so the next waypoint
//     can trigger phase 1 again. (BHV_Waypoint with
//     perpetual=true posts the endflag every iterate while
//     the boat is inside capture_radius -- the
//     m_wpt_reached_sent latch prevents repeated processing.)
//   - DO NOT deactivate the behavior. waypt_atak keeps
//     driving (and station-keeping via perpetual=true) so
//     the boat stays on the pin while it settles.
//
// Phase 2 (NAV_SPEED, while m_post_capture_pending=true):
//   Sustained-low check:
//     - speed < threshold AND timer not started: start timer.
//     - speed < threshold AND timer >= settle_duration:
//       settle now (deactivate, DM, reset state).
//     - speed >= threshold: reset timer (brief dips don't
//       count toward settlement).
//   Safety timeout:
//     - If elapsed-since-capture > post_capture_timeout,
//       force the release with a different DM noting the
//       boat didn't fully settle.

void WaypointHandler::onMail(const std::string& key,
                              const std::string& value,
                              CommanderContext& ctx)
{
  // --------------------------------------------------------
  // Phase 1: capture detected -> begin watching speed.
  // Behavior stays active during this phase -- waypt_atak
  // continues to drive the boat back if it overshoots.
  // --------------------------------------------------------
  if(key == "ATAK_WPT_REACHED") {
    if(value != "true") return;

    // BHV_Waypoint with perpetual=true re-fires the endflag
    // every iterate the boat is inside capture_radius. Only
    // the first capture per waypoint matters for state init.
    if(m_wpt_reached_sent) {
      ctx.dlog("WaypointHandler: ATAK_WPT_REACHED already sent, ignoring");
      return;
    }
    m_wpt_reached_sent        = true;
    m_post_capture_pending    = true;
    m_post_capture_start_time = ctx.now();
    m_low_speed_start_time    = -1.0;   // not yet sub-threshold

    // Reset the helm-side endflag so pHelmIvP doesn't see it
    // latched. Note: the bhv will likely re-post it next
    // iterate while still in capture radius -- that's fine,
    // m_wpt_reached_sent guards against re-processing.
    ctx.publish("ATAK_WPT_REACHED", "false");

    // NOTE: ATAK_WAYPT_ACTIVE is NOT cleared here. The bhv
    // stays active and continues station-keeping on the pin
    // until Phase 2 fires (sustained settle or timeout).

    ctx.dlog("WaypointHandler: phase 1 capture -- station-keeping, "
             "watching for settle");
    return;
  }

  // --------------------------------------------------------
  // Phase 2: speed update -- track sustained low speed or
  // safety timeout. Only acts while post-capture is pending.
  // --------------------------------------------------------
  if(key == "NAV_SPEED") {
    m_last_speed = atof(value.c_str());
    if(!m_post_capture_pending) return;

    std::string chat_dest = ctx.last_operator_callsign.empty()
                            ? std::string("All Chat Rooms")
                            : ctx.last_operator_callsign;

    // ------ Sustained low-speed check ------
    if(m_last_speed < m_speed_stop_threshold) {
      // Boat is currently slow. Either start or check timer.
      if(m_low_speed_start_time < 0.0) {
        // First sub-threshold sample since last reset.
        m_low_speed_start_time = ctx.now();
        ctx.dlog("WaypointHandler: low-speed timer started at "
                 + doubleToStringX(m_last_speed, 3) + " m/s");
      } else {
        double low_elapsed = ctx.now() - m_low_speed_start_time;
        if(low_elapsed >= m_settle_duration) {
          // SETTLED. Deactivate behavior, DM, reset state.
          ctx.publish("ATAK_WAYPT_ACTIVE",       "false");
          ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "false");
          ctx.dm("Waypoint reached.", chat_dest);

          m_post_capture_pending  = false;
          m_wpt_reached_sent      = false;
          m_low_speed_start_time  = -1.0;

          ctx.dlog("WaypointHandler: settled -- sustained "
                   + doubleToStringX(low_elapsed, 1) + "s below "
                   + doubleToStringX(m_speed_stop_threshold, 2)
                   + " m/s, deactivated + DM'd " + chat_dest);
          return;
        }
        // Still timing -- keep waiting.
      }
    } else {
      // Speed exceeds threshold -- reset the timer. Brief
      // dips below threshold don't count.
      if(m_low_speed_start_time >= 0.0) {
        ctx.dlog("WaypointHandler: low-speed timer reset (speed "
                 + doubleToStringX(m_last_speed, 3) + " m/s)");
      }
      m_low_speed_start_time = -1.0;
    }

    // ------ Safety timeout ------
    // If we've been in pending state too long without ever
    // sustaining a settled period, release anyway. Prevents
    // the handler from getting stuck if wind or current
    // keeps the boat in continuous motion.
    double total_elapsed = ctx.now() - m_post_capture_start_time;
    if(total_elapsed > m_post_capture_timeout) {
      ctx.publish("ATAK_WAYPT_ACTIVE",       "false");
      ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "false");
      ctx.dm("Waypoint reached (boat did not fully settle).",
             chat_dest);

      m_post_capture_pending  = false;
      m_wpt_reached_sent      = false;
      m_low_speed_start_time  = -1.0;

      ctx.dlog("WaypointHandler: timeout after "
               + doubleToStringX(total_elapsed, 1) + "s "
               "(speed still " + doubleToStringX(m_last_speed, 3)
               + " m/s), released anyway, DM'd " + chat_dest);
      return;
    }
    return;
  }
}


// ============================================================
// appcast()
// ============================================================

void WaypointHandler::appcast(std::string& report) const
{
  report += "  Accepted:    " + std::to_string(m_count) + "\n";
  report += "  Not deploy:  " + std::to_string(m_rejected_not_deployed) + "\n";
  report += "  No geodesy:  " + std::to_string(m_rejected_no_geodesy)   + "\n";
  if(!m_last_waypoint.empty())
    report += "  Last:        " + m_last_waypoint + "\n";
  report += "  cap_radius:  " + doubleToStringX(m_capture_radius, 1) + " m\n";
  report += "  stop_thresh: " + doubleToStringX(m_speed_stop_threshold, 3) + " m/s\n";
  report += "  settle_dur:  " + doubleToStringX(m_settle_duration, 1)     + " s\n";
  report += "  timeout:     " + doubleToStringX(m_post_capture_timeout, 1) + " s\n";
  report += "  enabled:     " + std::string(m_enabled ? "true" : "false") + "\n";
  report += "  phase:       ";
  if(m_post_capture_pending) {
    report += "STATION-KEEP (last_speed="
            + doubleToStringX(m_last_speed, 3) + " m/s";
    if(m_low_speed_start_time >= 0.0)
      report += ", timer_armed";
    else
      report += ", timer_reset";
    report += ")\n";
  } else if(m_wpt_reached_sent) {
    // Shouldn't normally hit -- pending should be true while
    // reached is true. Show as anomaly.
    report += "(reached latched, pending cleared)\n";
  } else {
    report += "idle / navigating\n";
  }
}

} // namespace common
