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
/*  MOOS Interface:                                         */
/*    Subscribes: NODE_REPORT, NODE_REPORT_LOCAL            */
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
                                      // false = use throttle intervals only.

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
};

#endif // COT_CONTACT_HEADER
