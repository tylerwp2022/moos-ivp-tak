/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: WptModeHandler.cpp                              */
/*    DATE: May 14, 2026                                    */
/************************************************************/

#include <algorithm>
#include <cctype>
#include <string>

#include "MBUtils.h"
#include "WptModeHandler.h"

namespace common {

namespace {

std::string toLower(const std::string& s)
{
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  return out;
}

} // anonymous


// ============================================================
// Construction
// ============================================================

WptModeHandler::WptModeHandler()
  : m_default_mode("fast"),
    m_current_mode("fast"),
    m_count_fast(0),
    m_count_precise(0),
    m_count_hold(0),
    m_count_query(0),
    m_reject_count(0)
{}


std::vector<std::string> WptModeHandler::chatKeywords() const
{
  return { "wpt_mode" };
}


std::string WptModeHandler::helpLine() const
{
  return "wpt_mode [fast|precise|hold]  -- set or query waypoint mode";
}


// ============================================================
// isValidMode
// ============================================================

bool WptModeHandler::isValidMode(const std::string& mode) const
{
  std::string m = toLower(mode);
  return (m == "fast" || m == "precise" || m == "hold");
}


// ============================================================
// configure() -- read default_wpt_mode
// ============================================================

void WptModeHandler::configure(const std::string& key,
                                const std::string& value)
{
  std::string k = toLower(key);

  if(k == "default_wpt_mode") {
    if(isValidMode(value)) {
      m_default_mode = toLower(value);
      m_current_mode = m_default_mode;
    }
    return;
  }
}


// ============================================================
// registerSubs() -- watch WPT_MODE for state mirror
// ============================================================
//
// Subscribed on both vehicle and shore. Vehicle sees the
// bridged value from shore (and its own self-publishes from
// direct DM). Shore sees the suffixed forms it writes itself.

void WptModeHandler::registerSubs(std::vector<std::string>& subs)
{
  subs.push_back("WPT_MODE");
}


// ============================================================
// onMail() -- update local mirror of current mode
// ============================================================

void WptModeHandler::onMail(const std::string& key,
                             const std::string& value,
                             CommanderContext& /*ctx*/)
{
  if(key == "WPT_MODE" && isValidMode(value)) {
    m_current_mode = toLower(value);
  }
}


// ============================================================
// handleChat() -- set or query mode
// ============================================================
//
// Two forms:
//   "wpt_mode <mode>"  -- set
//   "wpt_mode"         -- query (DM the current mode)
//
// Both forms route through dispatcher prefix resolution, so
// "blue_one wpt_mode precise" sets the mode for blue_one
// only by writing WPT_MODE_BLUE_ONE on shore.

bool WptModeHandler::handleChat(const ChatMessage& msg,
                                 CommanderContext& ctx)
{
  // Extract the argument after "wpt_mode".
  std::string rest;
  size_t space = msg.cmd.find(' ');
  if(space != std::string::npos) {
    rest = msg.cmd.substr(space + 1);
    size_t f = rest.find_first_not_of(" \t");
    if(f != std::string::npos) rest = rest.substr(f);
    size_t e = rest.find_last_not_of(" \t");
    if(e != std::string::npos) rest = rest.substr(0, e + 1);
  }

  // ----------------------------------------------------------
  // Query form: "wpt_mode" (no arg)
  // ----------------------------------------------------------
  if(rest.empty()) {
    std::string reply;
    if(ctx.fleet_mode) {
      // Shore mode -- we don't track per-vehicle current
      // mode from a single shore variable, so just describe
      // what the operator can do.
      reply = "wpt_mode " + msg.target_label +
              ": set via '[vehicle] wpt_mode fast|precise|hold'";
    } else {
      reply = "wpt_mode: currently " + m_current_mode +
              " (fast / precise / hold)";
    }
    ctx.dm(reply, msg.reply_to);
    m_count_query++;
    m_last_action = "query -> " + msg.reply_to;
    return true;
  }

  // ----------------------------------------------------------
  // Set form: validate the argument
  // ----------------------------------------------------------
  std::string mode = toLower(rest);
  if(!isValidMode(mode)) {
    ctx.dm("Usage: wpt_mode fast|precise|hold. Got: '" + rest + "'",
           msg.reply_to);
    m_reject_count++;
    m_last_action = "reject '" + rest + "' from " + msg.reply_to;
    return false;
  }

  // ----------------------------------------------------------
  // Publish + DM
  // ----------------------------------------------------------
  ctx.publish("WPT_MODE" + msg.sfx, mode);
  m_current_mode = mode;   // local mirror

  std::string description;
  if(mode == "fast")    description = "transit at full speed, release on first capture";
  if(mode == "precise") description = "decelerate on approach, station-keep until settled";
  if(mode == "hold")    description = "decelerate, station-keep at pin until 'resume'";

  ctx.dm("wpt_mode = " + mode + " for " + msg.target_label +
         " (" + description + ").",
         msg.reply_to);

  if(mode == "fast")    m_count_fast++;
  if(mode == "precise") m_count_precise++;
  if(mode == "hold")    m_count_hold++;
  m_last_action = mode + " for " + msg.target_label;
  if(!msg.callsign.empty())
    m_last_action += " (from " + msg.callsign + ")";

  ctx.dlog("WptModeHandler: WPT_MODE" + msg.sfx + "=" + mode);
  return true;
}


// ============================================================
// appcast()
// ============================================================

void WptModeHandler::appcast(std::string& report) const
{
  report += "  Current:    " + m_current_mode + "\n";
  report += "  Default:    " + m_default_mode + "\n";
  report += "  Set fast:   " + std::to_string(m_count_fast)    + "\n";
  report += "  Set precise:" + std::to_string(m_count_precise) + "\n";
  report += "  Set hold:   " + std::to_string(m_count_hold)    + "\n";
  report += "  Queries:    " + std::to_string(m_count_query)   + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count)  + "\n";
  if(!m_last_action.empty())
    report += "  Last:       " + m_last_action + "\n";
}

} // namespace common
