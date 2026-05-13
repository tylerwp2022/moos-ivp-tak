/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: FlagPursuitHandler.cpp                          */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Faithful port of CoTCommander::handleFlagCoT() plus     */
/*  the team_flag_vars subscription logic that stops        */
/*  pursuit when any teammate captures the flag.            */
/************************************************************/

#include <cmath>        // fabs
#include <cstdlib>      // atof
#include <string>
#include <algorithm>    // for std::find on team_flag_vars

#include "MBUtils.h"          // doubleToStringX, tolower, parseString
#include "CoTGeodesy.h"

#include "FlagPursuitHandler.h"
#include "../../CoTUtils.h"   // cot::extractAttr

namespace aquaticus {

FlagPursuitHandler::FlagPursuitHandler()
  : m_enabled(true),
    m_flag_uid("aquaticus-flag-red"),
    m_flag_capture_radius(5.0),
    m_waypoint_update_var("ATAK_WPT_UPDATE"),
    m_pursuing(false),
    m_pursuit_notified(false),
    m_last_lat(0.0),
    m_last_lon(0.0),
    m_pursuits_started(0),
    m_pursuits_ended(0),
    m_updates_silent(0)
{}


// ============================================================
// CoT claim
// ============================================================
//
// b-m-p-s-m is a generic spot marker -- we narrow on uid
// matching the configured flag_uid. The dispatcher's
// operator-UID filter is bypassed (we expect non-operator
// uids from pCoTGraphics).

bool FlagPursuitHandler::claimsCoT(const ParsedCoT& evt) const
{
  if(!m_enabled)                  return false;
  if(evt.type != "b-m-p-s-m")     return false;
  if(evt.uid  != m_flag_uid)      return false;
  if(!evt.has_position)           return false;
  return true;
}


// ============================================================
// handleCoT() -- start or update flag pursuit
// ============================================================

bool FlagPursuitHandler::handleCoT(const ParsedCoT& evt,
                                    CommanderContext& ctx)
{
  // ----------------------------------------------------------
  // Vehicle-only -- shore has no flag-pursuit semantics.
  // The bundle factory shouldn't put this in the shore set,
  // but defend in depth.
  // ----------------------------------------------------------
  if(ctx.fleet_mode) {
    ctx.dlog("FlagPursuit: fleet_mode -- skipping (vehicle-only handler)");
    return false;
  }

  // ----------------------------------------------------------
  // SA-broadcast filter: pCoTGraphics adds the
  // <_aquaticus_graphics sa_broadcast="true"/> element to
  // every flag-position update it sends for map display.
  // These are NOT operator pursuit commands; skip silently.
  // ----------------------------------------------------------
  if(evt.raw_xml.find("sa_broadcast=\"true\"") != std::string::npos) {
    ctx.dlog("FlagPursuit: sa_broadcast -- skipping");
    return false;
  }

  // ----------------------------------------------------------
  // Team-filter: don't pursue our own flag. Belt-and-
  // suspenders against shared plug files that don't change
  // the flag_uid per team.
  // ----------------------------------------------------------
  if(!m_my_team.empty()) {
    std::string uid_lower = tolower(evt.uid);
    if(uid_lower.find(m_my_team) != std::string::npos) {
      ctx.dlog("FlagPursuit: flag uid contains my_team (" +
               m_my_team + ") -- skipping own flag");
      return false;
    }
  }

  if(!ctx.deployed) {
    ctx.dlog("FlagPursuit: not deployed -- ignoring");
    return false;
  }

  if(!ctx.geodesy_ready || !ctx.geodesy) {
    ctx.dlog("FlagPursuit: geodesy not ready -- ignoring");
    return false;
  }

  double x = 0.0, y = 0.0;
  if(!ctx.geodesy->latLonToLocalXY(evt.lat, evt.lon, x, y)) {
    ctx.dlog("FlagPursuit: latLonToLocalXY failed");
    return false;
  }

  // ----------------------------------------------------------
  // Position-change check. Flag is stationary in normal
  // play; repeated CoT broadcasts of the same position
  // would otherwise repeatedly re-publish the waypoint.
  // ----------------------------------------------------------
  bool pos_changed = (std::fabs(evt.lat - m_last_lat) > kFlagPosThreshold ||
                      std::fabs(evt.lon - m_last_lon) > kFlagPosThreshold);

  if(!pos_changed && m_pursuing) {
    ctx.dlog("FlagPursuit: already pursuing, position unchanged");
    return false;
  }

  m_last_lat = evt.lat;
  m_last_lon = evt.lon;

  // Build waypoint update with the flag-specific capture
  // radius (smaller than the general 15m so we drive INTO
  // the Aquaticus grab zone rather than stopping short).
  std::string update = "points="             + doubleToStringX(x, 2) +
                       ","                   + doubleToStringX(y, 2) +
                       " # capture_radius=" + doubleToStringX(m_flag_capture_radius, 1);

  ctx.publish("ATAK_MODE",            "true");
  ctx.publish("ATAK_WAYPT_ACTIVE",    "true");
  ctx.publish("ATAK_FLAG_PURSUIT",    "true");
  ctx.publish(m_waypoint_update_var,  update);

  // DM destination preference:
  //   1. The most recent operator who sent a waypoint
  //      (so the pursuit announcement goes to whoever's
  //      been driving).
  //   2. This vehicle's own command_chatroom (NOT "All
  //      Chat Rooms" -- that would spam every ATAK client
  //      on every flag broadcast).
  std::string chat_dest = ctx.last_operator_callsign.empty()
                          ? ctx.command_chatroom
                          : ctx.last_operator_callsign;

  std::string lat_str = doubleToStringX(evt.lat, 5);
  std::string lon_str = doubleToStringX(evt.lon, 5);

  if(!m_pursuit_notified) {
    m_pursuit_notified = true;
    m_pursuits_started++;
    ctx.dm("Pursuing red flag at " + lat_str + ", " + lon_str +
           ". Will stop when flag is secured by any teammate.",
           chat_dest);
    m_last_action = "started pursuit @ " + lat_str + "," + lon_str;
  } else if(pos_changed) {
    m_updates_silent++;
    m_last_action = "updated pursuit @ " + lat_str + "," + lon_str;
  }

  m_pursuing = true;
  ctx.dlog("FlagPursuit: pursuit active -- " + update);
  return true;
}


// ============================================================
// configure() -- consume .moos ProcessConfig keys
// ============================================================

void FlagPursuitHandler::configure(const std::string& key,
                                    const std::string& value)
{
  std::string k = tolower(key);
  if(k == "flag_pursuit_enabled") {
    m_enabled = (tolower(value) == "true");
  }
  else if(k == "flag_uid") {
    if(!value.empty()) m_flag_uid = value;
  }
  else if(k == "flag_my_team") {
    m_my_team = tolower(value);
  }
  else if(k == "flag_capture_radius") {
    double r = atof(value.c_str());
    if(r > 0.0) m_flag_capture_radius = r;
  }
  else if(k == "team_flag_vars") {
    // Comma-separated list, e.g.
    //   "HAS_FLAG_BLUE_ONE,HAS_FLAG_BLUE_TWO,HAS_FLAG_BLUE_THREE"
    m_team_flag_vars.clear();
    std::vector<std::string> parts = parseString(value, ',');
    for(std::string& p : parts) {
      // trim
      while(!p.empty() && (p.front() == ' ' || p.front() == '\t'))
        p.erase(p.begin());
      while(!p.empty() && (p.back()  == ' ' || p.back()  == '\t'))
        p.pop_back();
      if(!p.empty()) m_team_flag_vars.push_back(p);
    }
  }
  else if(k == "waypoint_update_var") {
    // Shared with WaypointHandler -- both must end up with
    // the same value. configure() runs against the same
    // line for every handler, so they will.
    if(!value.empty()) m_waypoint_update_var = value;
  }
}


// ============================================================
// registerSubs() -- HAS_FLAG_* for pursuit termination
// ============================================================

void FlagPursuitHandler::registerSubs(std::vector<std::string>& subs)
{
  for(const std::string& v : m_team_flag_vars)
    subs.push_back(v);
}


// ============================================================
// onMail() -- watch for teammate flag captures
// ============================================================
//
// When any team_flag_vars variable goes true, a teammate
// (possibly us) has secured the flag. Stop pursuit and
// release the ATAK overrides; the operator can resume
// supervisory control with no further input needed.

void FlagPursuitHandler::onMail(const std::string& key,
                                 const std::string& value,
                                 CommanderContext& ctx)
{
  // Only care about our subscribed flag vars.
  if(std::find(m_team_flag_vars.begin(), m_team_flag_vars.end(), key)
       == m_team_flag_vars.end())
    return;

  bool has_flag = (tolower(value) == "true");
  if(!has_flag) return;

  if(!m_pursuing) return;  // wasn't pursuing; nothing to stop

  // Pursuit ends. Reset state and release the ATAK overrides.
  m_pursuing         = false;
  m_pursuit_notified = false;
  m_pursuits_ended++;

  ctx.publish("ATAK_FLAG_PURSUIT", "false");
  ctx.publish("ATAK_WAYPT_ACTIVE", "false");
  ctx.publish("ATAK_MODE",         "false");

  std::string chat_dest = ctx.last_operator_callsign.empty()
                          ? ctx.command_chatroom
                          : ctx.last_operator_callsign;
  ctx.dm("Flag secured (" + key + "). Pursuit ended.", chat_dest);

  m_last_action = "ended pursuit on " + key + "=true";
  ctx.dlog("FlagPursuit: ended on " + key + "=true");
}


// ============================================================
// appcast()
// ============================================================

void FlagPursuitHandler::appcast(std::string& report) const
{
  report += "  Pursuing:   " + std::string(m_pursuing ? "YES" : "no") + "\n";
  report += "  Started:    " + std::to_string(m_pursuits_started) + "\n";
  report += "  Ended:      " + std::to_string(m_pursuits_ended)   + "\n";
  report += "  Silent upd: " + std::to_string(m_updates_silent)   + "\n";
  report += "  Flag uid:   " + m_flag_uid + "\n";
  if(!m_my_team.empty())
    report += "  My team:    " + m_my_team + "\n";
  if(!m_last_action.empty())
    report += "  Last:       " + m_last_action + "\n";
}

} // namespace aquaticus
