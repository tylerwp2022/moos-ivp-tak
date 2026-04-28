/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTShoreContact.cpp                             */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include "CoTShoreContact.h"
#include "MBUtils.h"

using namespace std;

// ============================================================
// Constructor
// ============================================================

CoTShoreContact::CoTShoreContact()
{
  m_lat          = 0.0;
  m_lon          = 0.0;
  m_hae          = 0.0;
  m_callsign     = "SHORE";
  m_uid          = "";           // auto-generated in OnStartUp if empty
  m_affiliation  = "f";          // default: friendly

  m_send_interval = 30.0;        // resend every 30s — shore is static so
                                  // no need for the 1-3s rate used by moving
                                  // vehicle contacts
  m_last_sent     = -9999.0;     // force immediate send on first Iterate()
  m_cot_stale_sec = 7 * 86400.0; // 7 days — shore doesn't move; long stale
                                  // prevents ATAK from dropping the contact
                                  // during a multi-day exercise

  m_config_valid = false;
  m_sent_once    = false;
  m_debug        = false;
  m_cot_sent     = 0;
}


// ============================================================
// debugLog()
// ============================================================

void CoTShoreContact::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTShoreContact::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
//
// ProcessConfig = pCoTShoreContact
// {
//   AppTick   = 1
//   CommsTick = 1
//
//   // Required
//   lat         = 41.34928       // WGS84 decimal degrees
//   lon         = -74.063645     // WGS84 decimal degrees
//   callsign    = AQUATICUS-SHORE
//
//   // Optional
//   hae         = 0.0            // height above ellipsoid (meters)
//   uid         = shore-001      // CoT UID (auto from callsign if omitted)
//   affiliation = f              // f=friendly, h=hostile, n=neutral, u=unknown
//
//   send_interval = 30.0         // seconds between resends (default: 30)
//   stale_seconds = 604800       // ATAK stale timeout (default: 7 days)
//
//   debug = false
// }
// ============================================================

bool CoTShoreContact::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug flag first
  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

  bool got_lat = false, got_lon = false;

  // Pass 2: all params
  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  for(auto& orig : sParams) {
    string line  = orig;          // preserve case for callsign/uid
    string param = tolower(biteStringX(line, '='));
    string value = stripBlankEnds(line);
    bool handled = true;

    if(param == "debug") {
      setBooleanOnString(m_debug, value);
    }
    else if(param == "lat") {
      m_lat = atof(value.c_str());
      got_lat = true;
      debugLog("Config: lat = " + doubleToStringX(m_lat, 7));
    }
    else if(param == "lon") {
      m_lon = atof(value.c_str());
      got_lon = true;
      debugLog("Config: lon = " + doubleToStringX(m_lon, 7));
    }
    else if(param == "hae") {
      m_hae = atof(value.c_str());
      debugLog("Config: hae = " + doubleToStringX(m_hae, 2));
    }
    else if(param == "callsign") {
      m_callsign = value;
      debugLog("Config: callsign = " + m_callsign);
    }
    else if(param == "uid") {
      m_uid = value;
      debugLog("Config: uid = " + m_uid);
    }
    else if(param == "affiliation") {
      string a = tolower(value);
      if(validAffiliation(a)) {
        m_affiliation = a;
        debugLog("Config: affiliation = " + m_affiliation);
      }
      else {
        reportConfigWarning("pCoTShoreContact: affiliation must be f/h/n/u, "
                            "got '" + value + "' — defaulting to 'f'");
      }
    }
    else if(param == "send_interval") {
      m_send_interval = atof(value.c_str());
      if(m_send_interval < 1.0) m_send_interval = 1.0; // floor at 1s
      debugLog("Config: send_interval = " +
               doubleToStringX(m_send_interval, 1) + "s");
    }
    else if(param == "stale_seconds") {
      m_cot_stale_sec = atof(value.c_str());
      debugLog("Config: stale_seconds = " +
               doubleToStringX(m_cot_stale_sec, 0) + "s");
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Auto-generate UID from callsign if not explicitly set.
  // Replace spaces with hyphens so the UID is URL-safe and
  // unambiguous in CoT XML attribute context.
  if(m_uid.empty()) {
    m_uid = "aquaticus-shore-" + m_callsign;
    for(char& c : m_uid)
      if(c == ' ') c = '-';
    debugLog("OnStartUp: auto-generated uid = " + m_uid);
  }

  // Validate that required params were supplied
  if(!got_lat || !got_lon) {
    reportConfigWarning("pCoTShoreContact: 'lat' and 'lon' are required. "
                        "No CoT will be published until they are set.");
    m_config_valid = false;
  }
  else {
    m_config_valid = true;
    debugLog("OnStartUp: config valid — contact ready to publish");
  }

  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
//
// pCoTShoreContact publishes only — it subscribes to nothing.
// The contact is entirely config-driven; no MOOS mail is needed
// to build or update it.
// ============================================================

void CoTShoreContact::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  // No subscriptions — purely output-driven
}


// ============================================================
// OnNewMail()
//
// No mail expected — included for AppCastingMOOSApp compliance.
// ============================================================

bool CoTShoreContact::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);
  return true;
}


// ============================================================
// Iterate()
//
// Resends the static contact CoT every send_interval seconds.
// The first send fires immediately (m_last_sent initialized to
// -9999 so the elapsed time check is always true on first call).
//
// The shore station never moves, so a long send_interval (30s)
// is appropriate — this is much less frequent than the 1-3s
// used for moving vehicle contacts.
// ============================================================

bool CoTShoreContact::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if(!m_config_valid) {
    AppCastingMOOSApp::PostReport();
    return true;
  }

  double elapsed = m_curr_time - m_last_sent;
  if(elapsed >= m_send_interval) {
    string cot = buildContactCoT();
    Notify("COT_OUTBOUND", cot);
    m_last_sent = m_curr_time;
    m_cot_sent++;
    m_sent_once = true;
    debugLog("Iterate: published contact CoT (" +
             intToString(m_cot_sent) + " total sends)");
  }

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// buildContactCoT()
//
// Builds the MIL-STD 2525C SA contact CoT for the shore station.
//
// CoT type: a-{affil}-G-E
//   a       = atom (single point entity)
//   {affil} = f | h | n | u  (friendly/hostile/neutral/unknown)
//   G       = ground domain
//   E       = equipment (non-combatant installation)
//
// ce/le = 9999999 — standard value for manually-entered positions
// where there is no GPS error bound. Distinct from pCoTGraphics
// (ce=0 le=0) because those positions are computed from geodesy,
// not entered by a human.
//
// iconsetpath = COT_MAPPING_2525C/a-{affil}/a-{affil}-G
//   Confirmed from live ATAK capture of a ground station contact.
//   The /a-{affil}-G suffix selects the general ground icon for
//   the given affiliation color.
//
// how = "h-g-i-g-o" — confirmed from live ATAK capture.
//   This is used even though the position is manually entered
//   because ATAK uses this how value for ground contacts generally.
// ============================================================

string CoTShoreContact::buildContactCoT()
{
  string t_now   = formatCoTTime(m_curr_time, 0.0);
  string t_stale = formatCoTTime(m_curr_time, m_cot_stale_sec);

  string affil = m_affiliation;  // "f", "h", "n", or "u"

  // CoT type and icon path both encode the affiliation
  string cot_type  = "a-" + affil + "-G-E";
  string icon_path = "COT_MAPPING_2525C/a-" + affil + "/a-" + affil + "-G";

  // SA contact detail block — matches real ATAK contact format.
  // Key elements that place this in the contacts list (not just on map):
  //   endpoint="*:-1:stcp" — marks as a reachable SA endpoint
  //   <uid Droid="..."/>   — callsign binding for the contacts list
  //   <__group>            — team/role assignment (shows in contact card)
  //   <takv>               — platform identification
  //   <track>              — speed/course (0 for static shore station)
  //
  // Intentionally omitted (these cause map-marker behavior, not contact):
  //   <archive/>           — makes ATAK treat it as a persistent drawing
  //   <usericon>           — custom icon path used for drawings/markers
  string detail =
    "<detail>"
      "<contact callsign=\"" + m_callsign + "\" endpoint=\"*:-1:stcp\"/>"
      "<uid Droid=\"" + m_callsign + "\"/>"
      "<__group name=\"Cyan\" role=\"HQ\"/>"
      "<takv"
        " device=\"Shoreside\""
        " platform=\"pCoTShoreContact\""
        " os=\"Linux\""
        " version=\"1.0.0\"/>"
      "<status battery=\"100\"/>"
      "<track course=\"0.0\" speed=\"0.0\"/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""    + m_uid     + "\""
      " type=\""   + cot_type  + "\""
      " how=\"h-g-i-g-o\""
      " time=\""   + t_now     + "\""
      " start=\""  + t_now     + "\""
      " stale=\""  + t_stale   + "\""
      " access=\"Undefined\">"
    "<point"
      " lat=\""  + doubleToStringX(m_lat, 7) + "\""
      " lon=\""  + doubleToStringX(m_lon, 7) + "\""
      " hae=\""  + doubleToStringX(m_hae, 2) + "\""
      " ce=\"9999999\" le=\"9999999\"/>"
    + detail +
    "</event>";
}


// ============================================================
// validAffiliation()
// ============================================================

bool CoTShoreContact::validAffiliation(const std::string& affil)
{
  return (affil == "f" || affil == "h" ||
          affil == "n" || affil == "u");
}


// ============================================================
// formatCoTTime()
// ============================================================

string CoTShoreContact::formatCoTTime(double moos_time, double offset)
{
  time_t t = (time_t)(moos_time + offset);
  struct tm* utc = gmtime(&t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
  return string(buf);
}


// ============================================================
// buildReport()
// ============================================================

bool CoTShoreContact::buildReport()
{
  if(!m_config_valid) {
    m_msgs << "STATUS: MISCONFIGURED — lat and lon are required" << endl;
    m_msgs << endl;
    m_msgs << "Add to ProcessConfig:" << endl;
    m_msgs << "  lat       = <decimal degrees>" << endl;
    m_msgs << "  lon       = <decimal degrees>" << endl;
    m_msgs << "  callsign  = <name shown in ATAK>" << endl;
    return true;
  }

  string affil_label;
  if     (m_affiliation == "f") affil_label = "friendly";
  else if(m_affiliation == "h") affil_label = "hostile";
  else if(m_affiliation == "n") affil_label = "neutral";
  else                          affil_label = "unknown";

  m_msgs << "Contact:     " << m_callsign
         << " [" << affil_label << "]" << endl;
  m_msgs << "UID:         " << m_uid << endl;
  m_msgs << "Position:    " << doubleToStringX(m_lat, 7)
         << ", "            << doubleToStringX(m_lon, 7)
         << "  hae="        << doubleToStringX(m_hae, 1) << "m" << endl;
  m_msgs << "CoT type:    a-" << m_affiliation << "-G-E" << endl;
  m_msgs << endl;
  m_msgs << "Send interval: " << doubleToStringX(m_send_interval, 0)
         << "s   Stale: "
         << doubleToStringX(m_cot_stale_sec / 86400.0, 1) << " days" << endl;
  m_msgs << "Sends total:   " << m_cot_sent << endl;
  m_msgs << "Last sent:     ";
  if(m_sent_once)
    m_msgs << doubleToStringX(m_curr_time - m_last_sent, 1) << "s ago" << endl;
  else
    m_msgs << "never" << endl;

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
