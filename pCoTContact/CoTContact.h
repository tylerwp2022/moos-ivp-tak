/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTContact.h                                    */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTContact — vehicle SA position CoT publisher.        */
/*                                                          */
/*  Subscribes to NODE_REPORT and NODE_REPORT_LOCAL,        */
/*  tracks vehicle positions, and publishes MIL-STD 2525C   */
/*  SA contact CoT to COT_OUTBOUND on a throttled schedule. */
/*  pCoTBridge picks up COT_OUTBOUND and forwards to TAK.   */
/*                                                          */
/*  Supports two deployment modes:                          */
/*                                                          */
/*  Single-vehicle (real hardware / one sim vehicle):       */
/*    own_vehicle = blue_one                                 */
/*    Tracks only the named vehicle as a friendly contact.  */
/*    Runs on the robot's own computer in hardware mode.    */
/*                                                          */
/*  Multi-vehicle (shoreside sim):                          */
/*    own_vehicles     = blue_one,blue_two,blue_three        */
/*    hostile_vehicles = red_one,red_two,red_three           */
/*    Tracks all listed vehicles. Friendly = own_vehicles,  */
/*    hostile = hostile_vehicles. Unlisted names ignored.   */
/*    Runs on the shoreside computer in simulation mode.    */
/*                                                          */
/*  CoT types:                                              */
/*    Friendly: a-f-S-C-U-N  (friendly surface vessel)     */
/*    Hostile:  a-h-S-C-U-N  (hostile  surface vessel)     */
/*                                                          */
/*  Send rate is throttled separately for moving vs.        */
/*  stationary vehicles to reduce TAK server load:          */
/*    moving_send_interval     = 1.0s   (default)           */
/*    stationary_send_interval = 3.0s   (default)           */
/*    speed_threshold          = 0.5 m/s                    */
/*                                                          */
/*  Stealth integration (optional, multi-vehicle mode):     */
/*    stealth_integration = true                            */
/*    Subscribes to HVT_REVEAL_STATE from uFldNodeCommsHVT  */
/*    and suppresses CoT for vehicles currently hidden, so  */
/*    they do not appear in TAK until revealed. Hostiles    */
/*    not (yet) listed in HVT_REVEAL_STATE are treated as   */
/*    hidden — fail-safe: no leak while the hidden roster   */
/*    builds up during startup. Assumes the hidden group    */
/*    covers the hostile vehicles.                          */
/*    Default is false — always report all locations.       */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: NODE_REPORT, NODE_REPORT_LOCAL,           */
/*                HVT_REVEAL_STATE                          */
/*    Publishes:  COT_OUTBOUND (raw CoT XML strings)        */
/************************************************************/

#ifndef COT_CONTACT_HEADER
#define COT_CONTACT_HEADER

#include <string>
#include <map>
#include <set>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

// ============================================================
// VehicleState — one tracked vehicle's current position/state.
// Keyed by vehicle name in CoTContact::m_vehicles.
// ============================================================
struct VehicleState {
  std::string name;
  double      lat       = 0.0;
  double      lon       = 0.0;
  double      heading   = 0.0;
  double      speed     = 0.0;
  double      timestamp = 0.0;  // MOOS time from NODE_REPORT TIME= field
  double      last_sent = 0.0;  // MOOS time of last COT_OUTBOUND post
  bool        valid     = false; // true after first NODE_REPORT received
  bool        friendly  = true;  // false = hostile CoT type (a-h-S-C-U-N)
};


class CoTContact : public AppCastingMOOSApp
{
public:
  CoTContact();
  virtual ~CoTContact() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // NODE_REPORT → VehicleState update
  bool parseNodeReport(const std::string& report);

  // VehicleState → SA contact CoT XML
  std::string buildPositionCoT(const VehicleState& vs);

  // CoT timestamp formatter
  std::string formatCoTTime(double moos_time, double offset_sec = 0.0);

  // Vehicle tracking helpers
  bool shouldTrack(const std::string& name) const;
  bool isFriendly(const std::string& name) const;

  // Stealth integration (HVT_REVEAL_STATE from uFldNodeCommsHVT)
  void handleRevealState(const std::string& spec);
  bool isHidden(const std::string& name) const;

private:
  // --------------------------------------------------------
  // Vehicle configuration
  // --------------------------------------------------------
  bool        m_multi_mode;          // true = multi-vehicle (sim) mode
  std::string m_own_vehicle;         // single-vehicle mode name
  std::set<std::string> m_own_set;      // multi: friendly vehicle names
  std::set<std::string> m_hostile_set;  // multi: hostile vehicle names

  // All tracked vehicles — keyed by name
  std::map<std::string, VehicleState> m_vehicles;

  // --------------------------------------------------------
  // Send throttle
  // --------------------------------------------------------
  double m_moving_send_interval;      // seconds between sends when moving
  double m_stationary_send_interval;  // seconds between sends when stationary
  double m_speed_threshold;           // m/s — above = moving, below = stationary
  double m_cot_stale_offset;          // seconds until ATAK marks contact stale
  bool   m_immediate;                 // true = send on every NODE_REPORT;
                                      // moving/stationary throttle disabled.

  // --------------------------------------------------------
  // Affiliation and team colors
  //
  //   affiliation=f, team_color=Cyan → friendly cyan,  IN contacts list
  //   affiliation=h, team_color=Red  → hostile red,    IN contacts list
  //   affiliation=h (no team_color)  → hostile diamond, MAP ONLY
  //
  // In multi-vehicle mode, affiliation is derived from
  // m_own_set / m_hostile_set. team_color applies to friendly
  // vehicles, hostile_team_color to hostile vehicles. An empty
  // color means no __group element — the contact renders from
  // its CoT type (hostile diamond for a-h) and is map-only.
  // --------------------------------------------------------
  std::string m_affiliation;         // "f" | "h" | "n" | "u" (default: "f")
  std::string m_team_color;          // ATAK color for friendlies, or empty
  std::string m_hostile_team_color;  // ATAK color for hostiles,  or empty

  // --------------------------------------------------------
  // Stealth integration (uFldNodeCommsHVT)
  //
  // stealth_integration=true: vehicles marked "hidden" in the
  // latest HVT_REVEAL_STATE are tracked internally but produce
  // no CoT — they vanish from TAK (contact goes stale after
  // cot_stale_offset). Hostiles not (yet) listed in the reveal
  // state are treated as hidden so no position leaks while the
  // hidden roster builds up during startup.
  //
  // stealth_integration=false (default): always report all
  // tracked vehicles regardless of reveal state.
  // --------------------------------------------------------
  bool m_stealth_integration;
  bool m_reveal_state_received;         // first HVT_REVEAL_STATE seen
  std::map<std::string, bool> m_hidden_map;  // vname -> currently hidden

  // --------------------------------------------------------
  // Debug
  // --------------------------------------------------------
  bool m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_pos_cot_sent;
  unsigned int m_pos_cot_suppressed;  // sends skipped while hidden
};

#endif // COT_CONTACT_HEADER
