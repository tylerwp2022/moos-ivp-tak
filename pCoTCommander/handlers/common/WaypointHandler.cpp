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

#include <algorithm>    // std::transform
#include <cctype>       // std::tolower
#include <cmath>        // sqrt for approach distance
#include <cstdlib>      // atof
#include <string>

#include "MBUtils.h"           // doubleToStringX
#include "CoTGeodesy.h"        // ctx.geodesy methods
#include "MOOS/libMOOS/Utils/MOOSGenLibGlobalHelper.h"  // MOOSTime

#include "WaypointHandler.h"
#include "../../CoTUtils.h"    // cot::extractAttr

namespace common {

namespace {
// File-local helper. Lowercase a copy of the input.
std::string toLowerStr(const std::string& s)
{
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return out;
}
} // anonymous

WaypointHandler::WaypointHandler()
  : m_capture_radius(15.0),
    m_waypoint_update_var("ATAK_WPT_UPDATE"),
    m_enabled(true),
    m_speed_stop_threshold(0.1),
    m_settle_duration(3.0),
    m_post_capture_timeout(30.0),
    m_default_mode("fast"),
    m_default_speed(2.0),
    m_precise_approach_speed(1.0),
    m_approach_buffer(5.0),
    m_wpt_reached_sent(false),
    m_post_capture_pending(false),
    m_post_capture_start_time(0.0),
    m_low_speed_start_time(-1.0),
    m_last_speed(0.0),
    m_nav_x(0.0),
    m_nav_y(0.0),
    m_nav_valid(false),
    m_current_mode("fast"),
    m_target_x(0.0),
    m_target_y(0.0),
    m_approach_slowdown_applied(false),
    m_count(0),
    m_rejected_not_deployed(0),
    m_rejected_no_geodesy(0)
{}


// ============================================================
// extractModeOverride() -- search remarks for per-shot tag
// ============================================================
//
// Pulls the body of <remarks>...</remarks> from the CoT XML
// and searches for "fast" / "precise" / "hold" tokens (case-
// insensitive, optional leading '#'). Returns the matched
// mode string or empty.
//
// We don't use cot::extractAttr() because remarks is an
// element body, not an attribute. Hand-rolled substring
// search keeps the dependency surface flat.

std::string WaypointHandler::extractModeOverride(const std::string& raw_xml) const
{
  // Find <remarks ...>BODY</remarks>. The remarks element
  // may have attributes (source, time, etc.) so we look for
  // the closing '>' of the opening tag.
  size_t open_start = raw_xml.find("<remarks");
  if(open_start == std::string::npos) return "";
  size_t open_end = raw_xml.find('>', open_start);
  if(open_end == std::string::npos) return "";
  size_t close_start = raw_xml.find("</remarks>", open_end);
  if(close_start == std::string::npos) return "";

  std::string body = raw_xml.substr(open_end + 1,
                                     close_start - open_end - 1);
  std::string lower = toLowerStr(body);

  // Order matters: check "precise" before "fast" so "fastest"
  // (contains "fast") doesn't false-match. Currently no other
  // substrings collide but the order is defensive.
  if(lower.find("precise") != std::string::npos) return "precise";
  if(lower.find("hold")    != std::string::npos) return "hold";
  if(lower.find("fast")    != std::string::npos) return "fast";
  return "";
}


// ============================================================
// resolveMode() -- effective mode for a given CoT
// ============================================================
//
// Priority order: per-shot CoT override > sticky WPT_MODE >
// default from config.

std::string WaypointHandler::resolveMode(const std::string& raw_xml) const
{
  std::string override_mode = extractModeOverride(raw_xml);
  if(!override_mode.empty()) return override_mode;
  if(!m_current_mode.empty()) return m_current_mode;
  return m_default_mode;
}


// ============================================================
// buildUpdate() -- format ATAK_WPT_UPDATE string
// ============================================================
//
// Per BHV_Waypoint docs: "points=X,Y # speed=S # capture_radius=R"
// is a valid dynamic update. The '#' separator yields a list
// of behavior-parameter overrides for the next iterate.

std::string WaypointHandler::buildUpdate(double x,
                                          double y,
                                          double speed) const
{
  return "points="            + doubleToStringX(x, 2) +
         ","                  + doubleToStringX(y, 2) +
         " # speed="          + doubleToStringX(speed, 2) +
         " # capture_radius=" + doubleToStringX(m_capture_radius, 1);
}


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

  // ----------------------------------------------------------
  // Resolve effective mode (per-shot override > sticky >
  // default). Store as m_active_mode so the rest of the
  // mission (capture, settle, release) knows which path to
  // take.
  // ----------------------------------------------------------
  m_active_mode = resolveMode(evt.raw_xml);

  // New waypoint -> reset every phase latch AND remember the
  // target XY for approach-distance calculation. If the
  // operator sends a new waypoint while we're still in
  // post-capture pending state, the new mission takes
  // priority -- we abandon the old one's settlement tracking.
  m_wpt_reached_sent          = false;
  m_post_capture_pending      = false;
  m_low_speed_start_time      = -1.0;
  m_target_x                  = x;
  m_target_y                  = y;
  m_approach_slowdown_applied = false;

  // ----------------------------------------------------------
  // Build the BHV_Waypoint update line and publish the
  // activation trio. Initial speed is the cruise default
  // regardless of mode -- approach slowdown (precise/hold)
  // republishes a lower speed later when within range.
  // ----------------------------------------------------------
  std::string update = buildUpdate(x, y, m_default_speed);

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
  // Acknowledgment DM with the lat/lon and active mode. ATAK
  // clients use this to confirm the boat got the order; the
  // mode tag tells the operator how the boat will behave at
  // the destination.
  // ----------------------------------------------------------
  std::string lat_str = doubleToStringX(evt.lat, 5);
  std::string lon_str = doubleToStringX(evt.lon, 5);
  ctx.dm("ATAK [" + m_active_mode + "]. Moving to " +
         lat_str + ", " + lon_str + ".",
         chat_dest);

  m_count++;
  m_last_waypoint = lat_str + "," + lon_str +
                    " -> " + doubleToStringX(x, 2) + "," +
                             doubleToStringX(y, 2) +
                    "  mode=" + m_active_mode +
                    "  sender=" + chat_dest;

  ctx.dlog("WaypointHandler: " + m_waypoint_update_var + "=" + update +
           " mode=" + m_active_mode + " sender=" + chat_dest);
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
  else if(k == "default_wpt_mode") {
    std::string lv = toLowerStr(value);
    if(lv == "fast" || lv == "precise" || lv == "hold") {
      m_default_mode = lv;
      m_current_mode = lv;   // until WPT_MODE mail overrides
    }
  }
  else if(k == "default_speed") {
    double v = atof(value.c_str());
    if(v > 0.0) m_default_speed = v;
  }
  else if(k == "precise_approach_speed") {
    double v = atof(value.c_str());
    if(v > 0.0) m_precise_approach_speed = v;
  }
  else if(k == "approach_buffer") {
    double v = atof(value.c_str());
    // Allow 0 (slowdown engages exactly at capture_radius)
    // but not negative.
    if(v >= 0.0) m_approach_buffer = v;
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
  subs.push_back("NAV_X");
  subs.push_back("NAV_Y");
  subs.push_back("WPT_MODE");
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
  // WPT_MODE: sticky mode update from WptModeHandler.
  // Persists across waypoints. Per-shot CoT remarks tag can
  // still override on a single waypoint.
  // --------------------------------------------------------
  if(key == "WPT_MODE") {
    std::string lv = toLowerStr(value);
    if(lv == "fast" || lv == "precise" || lv == "hold") {
      m_current_mode = lv;
      ctx.dlog("WaypointHandler: WPT_MODE mail -> " + lv);
    }
    return;
  }

  // --------------------------------------------------------
  // NAV_X / NAV_Y: position updates for approach-distance
  // calculation. We track both so approach slowdown works
  // whether MOOS delivers them in either order.
  // --------------------------------------------------------
  if(key == "NAV_X") {
    m_nav_x = atof(value.c_str());
    m_nav_valid = true;
    // Fall through to approach slowdown check below.
  }
  else if(key == "NAV_Y") {
    m_nav_y = atof(value.c_str());
    m_nav_valid = true;
    // Fall through to approach slowdown check below.
  }

  // --------------------------------------------------------
  // Approach slowdown check -- only meaningful in precise/
  // hold modes, only before first capture, only once per
  // waypoint. Triggers on NAV_X / NAV_Y mail.
  // --------------------------------------------------------
  if((key == "NAV_X" || key == "NAV_Y") &&
     m_nav_valid &&
     !m_approach_slowdown_applied &&
     !m_wpt_reached_sent &&
     (m_active_mode == "precise" || m_active_mode == "hold"))
  {
    double dx = m_target_x - m_nav_x;
    double dy = m_target_y - m_nav_y;
    double dist = std::sqrt(dx*dx + dy*dy);
    double trigger = m_capture_radius + m_approach_buffer;

    if(dist <= trigger) {
      // Boat has entered the approach zone. Republish the
      // update with the precise approach speed. BHV_Waypoint
      // reads the new speed on its next iterate.
      std::string update = buildUpdate(m_target_x, m_target_y,
                                        m_precise_approach_speed);
      ctx.publish(m_waypoint_update_var, update);
      m_approach_slowdown_applied = true;

      ctx.dlog("WaypointHandler: approach slowdown @ "
               + doubleToStringX(dist, 1) + "m, speed="
               + doubleToStringX(m_precise_approach_speed, 2) + " m/s");
    }
    // Fall through -- NAV_X/Y is not the trigger for any
    // other state transition. Settlement uses NAV_SPEED.
    if(key == "NAV_X" || key == "NAV_Y") return;
  }
  if(key == "NAV_X" || key == "NAV_Y") return;

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
    m_low_speed_start_time    = -1.0;   // not yet sub-threshold

    // FAST mode: release immediately. No settle wait.
    if(m_active_mode == "fast") {
      std::string chat_dest = ctx.last_operator_callsign.empty()
                              ? std::string("All Chat Rooms")
                              : ctx.last_operator_callsign;
      ctx.publish("ATAK_WAYPT_ACTIVE",       "false");
      ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "false");
      ctx.publish("ATAK_WPT_REACHED",        "false");
      ctx.dm("Waypoint reached.", chat_dest);
      m_post_capture_pending = false;
      m_wpt_reached_sent     = false;  // ready for next
      m_active_mode          = "";

      ctx.dlog("WaypointHandler: fast mode -- released on capture, "
               "DM'd " + chat_dest);
      return;
    }

    // PRECISE / HOLD: enter pending state, station-keep
    // while waiting for sustained settlement.
    m_post_capture_pending    = true;
    m_post_capture_start_time = MOOSTime();

    // Reset the helm-side endflag so pHelmIvP doesn't see it
    // latched. The bhv will likely re-post it next iterate
    // while still in capture radius -- that's fine,
    // m_wpt_reached_sent guards against re-processing.
    ctx.publish("ATAK_WPT_REACHED", "false");

    // NOTE: ATAK_WAYPT_ACTIVE is NOT cleared here. The bhv
    // stays active and continues station-keeping on the pin
    // until Phase 2 fires (sustained settle or timeout).

    ctx.dlog("WaypointHandler: phase 1 capture (" + m_active_mode +
             ") -- station-keeping, watching for settle");
    return;
  }

  // --------------------------------------------------------
  // Phase 2: speed update -- track sustained low speed or
  // safety timeout. Only acts while post-capture is pending.
  // (Precise / hold modes only -- fast mode releases at
  // Phase 1 and clears m_post_capture_pending.)
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
        m_low_speed_start_time = MOOSTime();
        ctx.dlog("WaypointHandler: low-speed timer started at "
                 + doubleToStringX(m_last_speed, 3) + " m/s");
      } else {
        double low_elapsed = MOOSTime() - m_low_speed_start_time;
        if(low_elapsed >= m_settle_duration) {
          // SETTLED.
          //   precise: deactivate bhv, DM, return to idle.
          //   hold:    leave bhv active (continued station-
          //            keep), DM, but don't reset m_active_mode.
          //            Operator must use 'resume' to release.
          if(m_active_mode == "hold") {
            ctx.dm("Waypoint reached. Holding position.", chat_dest);
            ctx.dlog("WaypointHandler: hold mode -- settled but "
                     "keeping bhv active for station-keep");
          } else {
            // precise (or anything else that fell through)
            ctx.publish("ATAK_WAYPT_ACTIVE",       "false");
            ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "false");
            ctx.dm("Waypoint reached.", chat_dest);
            ctx.dlog("WaypointHandler: precise mode -- settled, "
                     "deactivated, DM'd " + chat_dest);
          }

          m_post_capture_pending  = false;
          m_wpt_reached_sent      = false;
          m_low_speed_start_time  = -1.0;
          // hold-mode keeps m_active_mode set so the operator
          // can see in appcast what mode the boat is holding
          // in. It clears on 'resume' (handled by ResumeHandler)
          // or next CoT.
          if(m_active_mode != "hold") m_active_mode = "";
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
    double total_elapsed = MOOSTime() - m_post_capture_start_time;
    if(total_elapsed > m_post_capture_timeout) {
      ctx.publish("ATAK_WAYPT_ACTIVE",       "false");
      ctx.publish("ATAK_WAYPT_ACTIVE_STATE", "false");
      ctx.dm("Waypoint reached (boat did not fully settle).",
             chat_dest);

      m_post_capture_pending  = false;
      m_wpt_reached_sent      = false;
      m_low_speed_start_time  = -1.0;
      m_active_mode           = "";   // even hold gives up

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
  report += "  def_speed:   " + doubleToStringX(m_default_speed, 2) + " m/s\n";
  report += "  approach:    " + doubleToStringX(m_precise_approach_speed, 2)
         +  " m/s within " + doubleToStringX(m_capture_radius + m_approach_buffer, 1)
         +  " m\n";
  report += "  stop_thresh: " + doubleToStringX(m_speed_stop_threshold, 3) + " m/s\n";
  report += "  settle_dur:  " + doubleToStringX(m_settle_duration, 1)     + " s\n";
  report += "  timeout:     " + doubleToStringX(m_post_capture_timeout, 1) + " s\n";
  report += "  def_mode:    " + m_default_mode + "\n";
  report += "  cur_mode:    " + m_current_mode + "\n";
  report += "  enabled:     " + std::string(m_enabled ? "true" : "false") + "\n";
  report += "  phase:       ";
  if(m_post_capture_pending) {
    report += "STATION-KEEP " + m_active_mode +
              " (last_speed=" + doubleToStringX(m_last_speed, 3) + " m/s";
    if(m_low_speed_start_time >= 0.0)
      report += ", timer_armed";
    else
      report += ", timer_reset";
    report += ")\n";
  } else if(!m_active_mode.empty()) {
    // Either fast en-route, precise/hold approaching, or
    // hold settled (post-Phase2 with bhv still active).
    report += "ACTIVE " + m_active_mode;
    if(m_approach_slowdown_applied) report += " (slowdown applied)";
    report += "\n";
  } else if(m_wpt_reached_sent) {
    // Shouldn't normally hit -- pending should be true while
    // reached is true. Show as anomaly.
    report += "(reached latched, pending cleared)\n";
  } else {
    report += "idle\n";
  }
}

} // namespace common
