/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGraphics.h                                   */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTGraphics — VIEW_* to CoT graphics renderer.         */
/*                                                          */
/*  Subscribes to MOOS VIEW_* variables published by        */
/*  pMarineViewer and pHelmIvP, converts them to CoT XML,  */
/*  and publishes to COT_OUTBOUND for pCoTBridge to forward */
/*  to the TAK server.                                      */
/*                                                          */
/*  Supported VIEW_* types:                                 */
/*    VIEW_POINT   → CoT spot marker (b-m-p-s-m)           */
/*    VIEW_SEGLIST → CoT polyline   (u-d-f)                 */
/*                                                          */
/*  active=false handling:                                  */
/*    Both types support active=false deactivation.         */
/*    When received, a CoT delete (t-x-d-d) is sent to     */
/*    remove the icon from the ATAK map immediately.        */
/*                                                          */
/*  Send rate:                                              */
/*    VIEW_POINTs: configurable — immediate (every update)  */
/*                 or throttled at stationary_interval      */
/*    VIEW_SEGLISTs: throttled at stationary_interval       */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: VIEW_POINT, VIEW_SEGLIST                  */
/*                NODE_REPORT, NODE_REPORT_LOCAL (geodesy)  */
/*    Publishes:  COT_OUTBOUND (raw CoT XML strings)        */
/************************************************************/

#ifndef COT_GRAPHICS_HEADER
#define COT_GRAPHICS_HEADER

#include <string>
#include <map>
#include <vector>
#include <utility>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "CoTGeodesy.h"

// ============================================================
// ViewPoint — one VIEW_POINT entry
// ============================================================
struct ViewPoint {
  std::string label;
  double      lat       = 0.0;
  double      lon       = 0.0;
  double      last_sent = 0.0;
  bool        valid     = false;
};

// ============================================================
// ViewSegList — one VIEW_SEGLIST entry
// ============================================================
struct ViewSegList {
  std::string label;
  std::vector<std::pair<double,double>> vertices; // (lat, lon)
  double last_sent = 0.0;
  bool   valid     = false;
};


class CoTGraphics : public AppCastingMOOSApp
{
public:
  CoTGraphics();
  virtual ~CoTGraphics() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // --------------------------------------------------------
  // VIEW_* parsers
  // Return false if active=false (handles delete) or parse fails.
  // --------------------------------------------------------
  bool parseViewPoint(const std::string& raw,   ViewPoint&   vp_out);
  bool parseViewSegList(const std::string& raw, ViewSegList& vsl_out);

  // --------------------------------------------------------
  // CoT builders
  // --------------------------------------------------------
  std::string buildViewPointCoT(const ViewPoint& vp);
  std::string buildViewSegListCoT(const ViewSegList& vsl);
  std::string buildDeleteCoT(const std::string& target_uid,
                              double lat, double lon);
  std::string formatCoTTime(double moos_time,
                             double stale_offset_sec = 0.0);

  // Sanitize a label for use in a CoT UID
  // (spaces and apostrophes → underscores)
  std::string sanitizeLabel(const std::string& label);

private:
  // --------------------------------------------------------
  // Geodesy
  // --------------------------------------------------------
  CoTGeodesy  m_geodesy;
  bool        m_geodesy_initialized;

  // --------------------------------------------------------
  // Tracked graphics state
  // Keyed by label — updated in-place on each VIEW_* message
  // --------------------------------------------------------
  std::map<std::string, ViewPoint>   m_view_points;
  std::map<std::string, ViewSegList> m_view_seglists;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------
  bool   m_publish_view_points;      // enable VIEW_POINT → CoT
  bool   m_publish_view_seglists;    // enable VIEW_SEGLIST → CoT
  bool   m_immediate_view_points;    // true: send VIEW_POINT on every update
                                     // false: throttle at stationary_interval
  double m_stationary_send_interval; // seconds between throttled sends
  double m_cot_stale_offset;         // seconds — spot markers stale time

  bool   m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_vp_cot_sent;
  unsigned int m_vsl_cot_sent;
  unsigned int m_delete_cot_sent;
};

#endif // COT_GRAPHICS_HEADER
