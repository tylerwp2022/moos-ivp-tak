/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTShoreContact.h                               */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTShoreContact — static SA contact publisher.         */
/*                                                          */
/*  Publishes a manually-configured position as a MIL-STD  */
/*  2525C SA contact to COT_OUTBOUND, which pCoTBridge      */
/*  forwards to the TAK server. The contact appears on the  */
/*  ATAK map as a persistent icon at a fixed location.      */
/*                                                          */
/*  Primary use case: marking the shoreside control station */
/*  on the ATAK map. The shore computer has no GPS so its   */
/*  position is set manually in the ProcessConfig block.    */
/*                                                          */
/*  Secondary use cases (add more ANTLER instances):        */
/*    - Safety boat reference position                      */
/*    - Field boundary reference markers                    */
/*    - Observer stations                                   */
/*    - Any fixed infrastructure point                      */
/*                                                          */
/*  CoT type structure: a-{affil}-G-E                       */
/*    a       = atom (point entity)                         */
/*    {affil} = f (friendly) | h (hostile) |                */
/*              n (neutral)  | u (unknown)                  */
/*    G       = ground domain                               */
/*    E       = equipment (non-combatant installation)      */
/*                                                          */
/*  The position uses ce=9999999 le=9999999 because it is   */
/*  manually entered — there is no GPS error bound.         */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: nothing (purely config-driven)            */
/*    Publishes:  COT_OUTBOUND (raw CoT XML string)         */
/************************************************************/

#ifndef COT_SHORE_CONTACT_HEADER
#define COT_SHORE_CONTACT_HEADER

#include <string>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class CoTShoreContact : public AppCastingMOOSApp
{
public:
  CoTShoreContact();
  virtual ~CoTShoreContact() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // Build the SA contact CoT XML from current config
  std::string buildContactCoT();

  // Format a MOOS time as a CoT timestamp string
  std::string formatCoTTime(double moos_time, double offset_sec = 0.0);

  // Validate that a given affiliation character is legal
  bool validAffiliation(const std::string& affil);

private:
  // --------------------------------------------------------
  // Contact configuration (set in ProcessConfig, immutable
  // at runtime — restart to change position or callsign)
  // --------------------------------------------------------
  double      m_lat;           // WGS84 latitude  (decimal degrees)
  double      m_lon;           // WGS84 longitude (decimal degrees)
  double      m_hae;           // height above ellipsoid (meters, default 0)
  std::string m_callsign;      // contact callsign displayed in ATAK
  std::string m_uid;           // CoT UID (auto-generated from callsign if empty)
  std::string m_affiliation;   // "f", "h", "n", or "u"

  // --------------------------------------------------------
  // Send rate
  // --------------------------------------------------------
  double m_send_interval;  // seconds between CoT resends
  double m_last_sent;      // MOOS time of last COT_OUTBOUND post
  double m_cot_stale_sec;  // seconds until ATAK marks contact stale

  // --------------------------------------------------------
  // State
  // --------------------------------------------------------
  bool m_config_valid;    // false if required params are missing
  bool m_sent_once;       // true after first successful send

  // --------------------------------------------------------
  // Debug
  // --------------------------------------------------------
  bool m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_cot_sent;
};

#endif // COT_SHORE_CONTACT_HEADER
