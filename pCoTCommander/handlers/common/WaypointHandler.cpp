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
    m_wpt_reached_sent(false),
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

  // New waypoint -> reset the reached guard so the next
  // capture fires a fresh "Waypoint reached" DM.
  m_wpt_reached_sent = false;

  // ----------------------------------------------------------
  // Build the BHV_Waypoint update line and publish the
  // activation trio.
  // ----------------------------------------------------------
  std::string update = "points="            + doubleToStringX(x, 2) +
                       ","                  + doubleToStringX(y, 2) +
                       " # capture_radius=" + doubleToStringX(m_capture_radius, 1);

  ctx.publish("ATAK_MODE",            "true");
  ctx.publish("ATAK_WAYPT_ACTIVE",    "true");
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
  // Unknown keys are silently ignored -- other handlers may
  // own them.
}


// ============================================================
// registerSubs() -- ATAK_WPT_REACHED endflag
// ============================================================
//
// waypt_atak's endflag posts ATAK_WPT_REACHED=true when the
// vehicle captures the waypoint. We DM the operator and
// reset the flag for the next waypoint.

void WaypointHandler::registerSubs(std::vector<std::string>& subs)
{
  subs.push_back("ATAK_WPT_REACHED");
}


// ============================================================
// onMail() -- handle ATAK_WPT_REACHED
// ============================================================
//
// waypt_atak's endflag posts ATAK_WPT_REACHED=true when the
// vehicle captures the waypoint. We DM the operator who
// sent the original waypoint, then reset the helm-side flag
// to false so pHelmIvP doesn't see it as permanently true.

void WaypointHandler::onMail(const std::string& key,
                              const std::string& value,
                              CommanderContext& ctx)
{
  if(key != "ATAK_WPT_REACHED") return;
  if(value != "true")           return;

  // Behavior is perpetual=true and re-fires the endflag
  // every tick while idling at the capture point. Guard
  // against repeats.
  if(m_wpt_reached_sent) {
    ctx.dlog("WaypointHandler: ATAK_WPT_REACHED already sent, ignoring");
    return;
  }

  m_wpt_reached_sent = true;

  // DM the operator who sent the most recent waypoint.
  // Fall back to "All Chat Rooms" if no operator callsign
  // has been recorded yet.
  std::string chat_dest = ctx.last_operator_callsign.empty()
                          ? std::string("All Chat Rooms")
                          : ctx.last_operator_callsign;
  ctx.dm("Waypoint reached.", chat_dest);

  // Reset the helm-side flag so pHelmIvP doesn't see it
  // latched. Otherwise the next iterate would treat it as
  // still-true and never re-fire on the following capture.
  ctx.publish("ATAK_WPT_REACHED", "false");

  ctx.dlog("WaypointHandler: waypoint reached -- DM'd " + chat_dest);
}


// ============================================================
// appcast()
// ============================================================

void WaypointHandler::appcast(std::string& report) const
{
  report += "  Accepted:   " + std::to_string(m_count) + "\n";
  report += "  Not deploy: " + std::to_string(m_rejected_not_deployed) + "\n";
  report += "  No geodesy: " + std::to_string(m_rejected_no_geodesy)   + "\n";
  if(!m_last_waypoint.empty())
    report += "  Last:       " + m_last_waypoint + "\n";
  report += "  cap_radius: " + doubleToStringX(m_capture_radius, 1) + " m\n";
  report += "  enabled:    " + std::string(m_enabled ? "true" : "false") + "\n";
}

} // namespace common
