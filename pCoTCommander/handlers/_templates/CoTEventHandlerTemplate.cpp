/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  CoTEventHandlerTemplate.cpp                    */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- see header file for renaming checklist.     */
/************************************************************/

#include <cstdlib>      // atof
#include <string>

#include "MBUtils.h"           // doubleToStringX, tolower
#include "CoTGeodesy.h"

#include "CoTEventHandlerTemplate.h"
#include "../../CoTUtils.h"    // cot::extractAttr

namespace templ {

CoTEventHandlerTemplate::CoTEventHandlerTemplate()
  : m_my_param(0.0),
    m_count(0),
    m_reject_not_deployed(0),
    m_reject_no_geodesy(0)
{}


// ============================================================
// claimsCoT -- fast predicate, called for every inbound CoT
// ============================================================
//
// Keep this LIGHTWEIGHT. The dispatcher iterates every
// handler's claimsCoT for every inbound event. Don't:
//   - touch ctx (you don't have it)
//   - check deployment / geodesy / other state
//   - do XML parsing beyond what ParsedCoT already gives you
//
// Do check:
//   - evt.type (always)
//   - evt.uid  (if you want a specific uid or substring)
//   - evt.has_position (if you require lat/lon)
//
// First-claim-wins: if you return true, no later handler
// sees this event. Order handlers in CommandHandlerFactory
// so more specific claims come first.

bool CoTEventHandlerTemplate::claimsCoT(const ParsedCoT& evt) const
{
  // Example: claim every spot marker with a position.
  if(evt.type != "b-m-p-s-m") return false;
  if(!evt.has_position)       return false;

  // Optional: narrow on uid if a config key is set.
  // Comment out if your handler matches by type alone.
  if(!m_my_uid_match.empty()) {
    if(evt.uid.find(m_my_uid_match) == std::string::npos)
      return false;
  }

  return true;
}


// ============================================================
// handleCoT -- the actual work
// ============================================================
//
// All state-dependent checks (deployed? geodesy ready?)
// happen here, not in claimsCoT. Returning false from
// handleCoT still counts as "consumed" -- the dispatcher
// won't offer the event to later handlers.
//
// Return true if you actually did something useful.
// Diagnostic counters in the dispatcher distinguish
// "handled" (true) from "ignored" (no handler claimed).

bool CoTEventHandlerTemplate::handleCoT(const ParsedCoT& evt,
                                         CommanderContext& ctx)
{
  // ----------------------------------------------------------
  // Pull operator callsign for DMs.
  // <link parent_callsign="..."/> in the CoT XML.
  // ----------------------------------------------------------
  std::string sender = cot::extractAttr(evt.raw_xml, "parent_callsign");
  std::string chat_dest = sender.empty() ? "All Chat Rooms" : sender;

  // ----------------------------------------------------------
  // State checks. Use ctx mirrored state. NEVER subscribe
  // to DEPLOY / ATAK_MODE / TAGGED yourself -- those are
  // dispatcher-managed and reading from ctx is the right
  // way.
  // ----------------------------------------------------------
  if(!ctx.deployed) {
    ctx.dm("Not deployed -- ignoring CoT.", chat_dest);
    m_reject_not_deployed++;
    ctx.dlog("Template: rejected -- not deployed");
    return false;
  }

  // ----------------------------------------------------------
  // If the event has a position and you need local XY,
  // convert via ctx.geodesy. Geodesy is initialized from
  // LatOrigin/LongOrigin or the first NODE_REPORT.
  // ----------------------------------------------------------
  double x = 0.0, y = 0.0;
  if(evt.has_position) {
    if(!ctx.geodesy_ready || !ctx.geodesy) {
      ctx.dm("Geodesy not ready -- waiting for GPS fix.", chat_dest);
      m_reject_no_geodesy++;
      ctx.dlog("Template: rejected -- geodesy not ready");
      return false;
    }
    if(!ctx.geodesy->latLonToLocalXY(evt.lat, evt.lon, x, y)) {
      ctx.dlog("Template: latLonToLocalXY failed");
      return false;
    }
  }

  // ----------------------------------------------------------
  // Optionally update shared scratchpad so OTHER handlers
  // (or this handler's onMail) can DM the same operator
  // later. Only WaypointHandler currently does this, but
  // it's the right pattern for any operator-driven CoT.
  // ----------------------------------------------------------
  if(!sender.empty())
    ctx.last_operator_callsign = sender;

  // ----------------------------------------------------------
  // Do the actual work. Publish whatever MOOS variables
  // your behaviors need to see. Use ctx.publish (NOT MOOS
  // Notify) so dispatcher routing applies.
  // ----------------------------------------------------------
  std::string update = "points="            + doubleToStringX(x, 2) +
                       ","                  + doubleToStringX(y, 2);

  ctx.publish("TEMPLATE_COT_ACTIVE", "true");
  // ctx.publish("TEMPLATE_TARGET", update);

  // ----------------------------------------------------------
  // Confirm to operator.
  // ----------------------------------------------------------
  std::string lat_str = doubleToStringX(evt.lat, 5);
  std::string lon_str = doubleToStringX(evt.lon, 5);
  ctx.dm("Template CoT received at " + lat_str + ", " + lon_str + ".",
         chat_dest);

  m_count++;
  m_last_event = "uid=" + evt.uid +
                 " @ " + lat_str + "," + lon_str;
  ctx.dlog("Template: handled " + evt.type + " uid=" + evt.uid);
  return true;
}


// ============================================================
// configure -- consume .moos ProcessConfig keys
// ============================================================
//
// Called once per .moos config line. Ignore keys you don't
// recognize -- they're for other handlers (or the
// dispatcher). The dispatcher already consumes well-known
// keys like fleet_mode, command_chatroom, debug, etc.

void CoTEventHandlerTemplate::configure(const std::string& key,
                                         const std::string& value)
{
  std::string k = tolower(key);

  if(k == "template_uid_match") {
    m_my_uid_match = value;
  }
  else if(k == "template_param") {
    double v = atof(value.c_str());
    if(v > 0.0) m_my_param = v;
  }
  // Add more keys here. Unknown keys silently ignored.
}


// ============================================================
// registerSubs -- handler-specific MOOS subscriptions
// ============================================================
//
// Add variable names you want to receive in onMail().
// Don't include DEPLOY / ATAK_MODE / TAGGED / ATAK_RETRY --
// those are dispatcher-managed and exposed via ctx.

void CoTEventHandlerTemplate::registerSubs(std::vector<std::string>& subs)
{
  // Example:
  // subs.push_back("MY_TERMINATION_SIGNAL");
  (void)subs;  // suppress unused-param warning if no subs
}


// ============================================================
// onMail -- react to subscribed variables
// ============================================================
//
// Self-filter by key. Cheap dispatch: called on EVERY mail
// item, but the default base impl is a no-op so handlers
// that don't override pay nothing.

void CoTEventHandlerTemplate::onMail(const std::string& key,
                                      const std::string& value,
                                      CommanderContext& ctx)
{
  // Example pattern:
  //
  // if(key == "MY_TERMINATION_SIGNAL" && value == "true") {
  //   ctx.publish("TEMPLATE_COT_ACTIVE", "false");
  //   ctx.dm("Template event ended.",
  //          ctx.last_operator_callsign.empty()
  //            ? ctx.command_chatroom
  //            : ctx.last_operator_callsign);
  //   ctx.dlog("Template: terminated on MY_TERMINATION_SIGNAL");
  // }

  (void)key; (void)value; (void)ctx;  // suppress warnings
}


// ============================================================
// appcast
// ============================================================

void CoTEventHandlerTemplate::appcast(std::string& report) const
{
  report += "  Handled:    " + std::to_string(m_count) + "\n";
  report += "  Not deploy: " + std::to_string(m_reject_not_deployed) + "\n";
  report += "  No geodesy: " + std::to_string(m_reject_no_geodesy) + "\n";
  if(!m_my_uid_match.empty())
    report += "  Uid match:  " + m_my_uid_match + "\n";
  if(!m_last_event.empty())
    report += "  Last:       " + m_last_event + "\n";
}

} // namespace templ
