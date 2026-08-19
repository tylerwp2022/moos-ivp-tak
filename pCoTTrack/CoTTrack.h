/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTTrack.h                                      */
/*    DATE: August 2026                                     */
/*                                                          */
/*  pCoTTrack — inbound TAK SA → MOOS node reports.         */
/*                                                          */
/*  The mirror image of pCoTContact. Where pCoTContact      */
/*  turns NODE_REPORT into outbound CoT so MOOS vehicles    */
/*  appear in ATAK, pCoTTrack turns inbound CoT position    */
/*  reports into NODE_REPORT so ATAK users appear in        */
/*  MOOS-IvP — visible in pMarineViewer and consumable by   */
/*  pContactMgrV20 and contact-driven behaviors.            */
/*                                                          */
/*  PIPELINE                                                */
/*  -------------------------------------------------------- */
/*    TAK server → pCoTBridge → COT_INBOUND → pCoTTrack      */
/*      → NODE_REPORT (+ optional VIEW_MARKER) → shore DB   */
/*      → uFldNodeComms → NODE_REPORT_<VNAME> → vehicles    */
/*                                                          */
/*  WHAT IS INGESTED                                        */
/*  -------------------------------------------------------- */
/*  Only ground-unit position location information (PLI) —  */
/*  the self-position broadcast an ATAK/WinTAK client emits  */
/*  for its own operator. Default match set:                */
/*                                                          */
/*    a-f-G-U   friendly ground unit  (a-f-G-U-C etc.)      */
/*    a-h-G-U   hostile  ground unit                        */
/*    a-n-G-U   neutral  ground unit                        */
/*    a-u-G-U   unknown  ground unit                        */
/*                                                          */
/*  Matching is by PREFIX, so a-f-G-U also claims the more  */
/*  specific a-f-G-U-C-I (infantry) and a-f-G-U-C-V         */
/*  (vehicle) subtypes ATAK emits. Override with            */
/*  track_cot_types. Everything else on COT_INBOUND —       */
/*  GOTO waypoints, chat, graphics — is ignored here and    */
/*  left to pCoTCommander and pCoTChat.                     */
/*                                                          */
/*  A callsign_whitelist further narrows ingest: when set,  */
/*  only the listed callsigns (case-insensitive) are        */
/*  tracked. Empty (the default) admits all callsigns.      */
/*                                                          */
/*  LOOPBACK SUPPRESSION                                    */
/*  -------------------------------------------------------- */
/*  A TAK server echoes back everything this MOOS community  */
/*  publishes. Without a filter, pCoTContact's own vehicle   */
/*  contacts would return through COT_INBOUND and be         */
/*  republished as duplicate NODE_REPORTs, doubling every    */
/*  vehicle on the map and feeding phantom contacts to the   */
/*  behaviors. Two independent guards prevent this:          */
/*                                                          */
/*    1. Type filter — pCoTContact emits a-f-S-C-U-N        */
/*       (surface vessel) and pCoTShoreContact emits        */
/*       a-f-G-E (ground equipment). Neither is a           */
/*       ground UNIT, so neither matches track_cot_types.   */
/*                                                          */
/*    2. UID prefix filter — ignore_uid_prefix drops any    */
/*       event whose uid begins with a listed string.       */
/*       Defaults to "surveyor-", the prefix pCoTContact    */
/*       stamps on every contact it sends.                  */
/*                                                          */
/*  Guard 2 is redundant today but survives someone later   */
/*  changing a CoT type upstream, which is exactly when a   */
/*  feedback loop would otherwise appear.                    */
/*                                                          */
/*  NODE NAMING                                             */
/*  -------------------------------------------------------- */
/*  Tracks are keyed on the CoT uid, which is stable for the */
/*  life of an ATAK install. The MOOS node name is derived   */
/*  from the <contact callsign="..."> instead, because that  */
/*  is what an operator recognizes — but callsigns are free  */
/*  text ("Delta 1", "TYLER-ATAK") and NODE_REPORT is a      */
/*  comma/equals delimited format, so the callsign is        */
/*  sanitized to [a-z0-9_] and given a configurable prefix   */
/*  (default "atak_") that keeps TAK-sourced nodes visibly   */
/*  distinct from vehicles and immune to name collisions.    */
/*                                                          */
/*  TEAM MAPPING                                            */
/*  -------------------------------------------------------- */
/*  ATAK operators are players, not spectators: boats must   */
/*  be able to target and tag them. uFldTagManager rejects   */
/*  any NODE_REPORT whose GROUP is not one of its two team   */
/*  names, so each track's GROUP is derived from the team    */
/*  the operator picked in ATAK (<__group name="Red">),      */
/*  mapped through team_map (reds → red, blues → blue by     */
/*  default). Colors with no mapping fall back to            */
/*  node_group. Set group_from_team=false to disable and     */
/*  use node_group for everyone.                             */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: COT_INBOUND, NODE_REPORT, NODE_REPORT_LOCAL*/
/*    Publishes:  NODE_REPORT, VIEW_MARKER (optional)       */
/************************************************************/

#ifndef COT_TRACK_HEADER
#define COT_TRACK_HEADER

#include <string>
#include <map>
#include <vector>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "CoTGeodesy.h"

// ============================================================
// TakTrack — one ATAK user currently being tracked.
// Keyed by CoT uid in CoTTrack::m_tracks.
// ============================================================
struct TakTrack {
  // Identity
  std::string uid;         // CoT event uid — the map key, never changes
  std::string callsign;    // <contact callsign=""> — operator-facing
  std::string node_name;   // sanitized MOOS node name (prefix + callsign)
  std::string cot_type;    // e.g. a-f-G-U-C
  std::string team_color;  // <__group name=""> — ATAK team color, may be empty

  // Position
  double lat      = 0.0;
  double lon      = 0.0;
  double hae      = 0.0;   // height above ellipsoid, meters
  double x        = 0.0;   // local grid meters (from lib_cot_geodesy)
  double y        = 0.0;
  bool   xy_valid = false; // false until a geodesy conversion succeeds

  // Motion. ATAK normally supplies <track speed course>, but not every
  // client does, and a stationary client may omit it. When absent the
  // values are derived from successive fixes (see deriveMotion).
  double speed           = 0.0;  // m/s
  double course          = 0.0;  // degrees true
  bool   motion_from_cot = false;// false = derived from position deltas

  // Timing. cot_time is the sender's clock and is kept for diagnostics
  // only — NODE_REPORT TIME uses the local MOOS clock (see postTrack).
  double cot_time  = 0.0;  // event time= as epoch seconds
  double first_rx  = 0.0;  // MOOS time of first accepted event
  double last_rx   = 0.0;  // MOOS time of most recent accepted event
  double last_post = 0.0;  // MOOS time of last NODE_REPORT post

  // Previous fix — used by deriveMotion when <track> is absent
  double prev_x     = 0.0;
  double prev_y     = 0.0;
  double prev_rx    = 0.0;
  bool   prev_valid = false;

  unsigned int updates = 0;  // accepted CoT events for this uid
};


class CoTTrack : public AppCastingMOOSApp
{
public:
  CoTTrack();
  virtual ~CoTTrack() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // COT_INBOUND → TakTrack create/update. Returns true if the event
  // was accepted (matched the type filter and carried a position).
  bool handleInboundCoT(const std::string& xml);

  // NODE_REPORT → geodesy NAV anchor (flat-earth fallback mode only)
  void updateNavAnchor(const std::string& report);

  // TakTrack → NODE_REPORT (+ VIEW_MARKER) on the MOOSDB
  void postTrack(TakTrack& tr);

  // Remove a track's marker from pMarineViewer
  void retractMarker(const TakTrack& tr);

  // Fill speed/course from successive fixes when <track> was absent
  void deriveMotion(TakTrack& tr);

  // ---- filters ----
  bool matchesTrackType(const std::string& type) const;
  bool isIgnoredUid(const std::string& uid)      const;
  bool isIgnoredCallsign(const std::string& cs)  const;
  bool isWhitelistedCallsign(const std::string& cs) const;

  // ---- small XML helpers (CoT is regular enough not to need a parser) ----
  static std::string extractAttr(const std::string& xml,
                                  const std::string& attr);
  static std::string extractElemAttr(const std::string& xml,
                                      const std::string& elem,
                                      const std::string& attr);

  // ISO8601 CoT timestamp → epoch seconds. Returns 0.0 if unparseable.
  static double parseCoTTime(const std::string& iso);

  // Free-text callsign → legal MOOS node name
  std::string sanitizeName(const std::string& raw) const;

  // NODE_REPORT GROUP= for this track: the ATAK team color mapped
  // through team_map when group_from_team is set, node_group otherwise
  std::string groupForTrack(const TakTrack& tr) const;

private:
  // --------------------------------------------------------
  // Node identity / appearance in MOOS
  // --------------------------------------------------------
  std::string m_node_prefix;     // prepended to every node name
  std::string m_node_type;       // NODE_REPORT TYPE= (pMarineViewer body)
  std::string m_node_group;      // NODE_REPORT GROUP= — "" omits the field

  // ATAK team color → game team. uFldTagManager only accepts nodes
  // whose GROUP is one of its two team names, so an ATAK operator who
  // should be tag-able must carry GROUP=red or GROUP=blue. The ATAK
  // client already declares a team (<__group name="Red">); team_map
  // turns that color into the game team, keyed lowercase.
  bool m_group_from_team;                          // use team_map at all
  std::map<std::string, std::string> m_team_map;   // "dark blue" → "blue"

  // vname_map: callsign → exact MOOS vehicle name, posted verbatim
  // (no node_prefix). Lets a real ATAK operator stand in for a
  // standard mission vehicle (e.g. the HVT blue_four) so nothing
  // mission-side has to be renamed. Keys stored lowercase; matched
  // against the lowercased callsign.
  std::map<std::string, std::string> m_vname_map;
  std::string m_node_color;      // NODE_REPORT COLOR=
  double      m_node_length;     // NODE_REPORT LENGTH= (meters)
  bool        m_lowercase_names; // fold node names to lower case

  // --------------------------------------------------------
  // Ingest filters
  // --------------------------------------------------------
  std::vector<std::string> m_track_types;    // CoT type prefixes to accept
  std::vector<std::string> m_ignore_uids;    // uid prefixes to drop
  std::vector<std::string> m_ignore_calls;   // exact callsigns to drop
  std::vector<std::string> m_allow_calls;    // callsign whitelist, stored
                                             // lowercase; empty = allow all
  unsigned int             m_max_tracks;     // cap on concurrent tracks

  // --------------------------------------------------------
  // Output selection
  // --------------------------------------------------------
  bool        m_publish_node_report;
  bool        m_publish_view_marker;
  std::string m_marker_type;
  std::string m_marker_color;
  double      m_marker_width;

  // --------------------------------------------------------
  // Rates
  // --------------------------------------------------------
  double m_post_interval;  // min seconds between NODE_REPORTs per track;
                            // 0 = post on every accepted CoT event
  double m_stale_timeout;  // seconds without CoT before a track is dropped

  // --------------------------------------------------------
  // Motion source
  // --------------------------------------------------------
  bool m_derive_motion_only; // ignore <track> in the CoT; always derive
                             // speed/course from successive fixes

  // --------------------------------------------------------
  // State
  // --------------------------------------------------------
  std::map<std::string, TakTrack> m_tracks;  // keyed by CoT uid
  CoTGeodesy                      m_geodesy;

  // --------------------------------------------------------
  // Debug
  // --------------------------------------------------------
  bool m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_cot_received;    // COT_INBOUND messages seen
  unsigned int m_cot_accepted;    // matched the type filter, had a position
  unsigned int m_cot_loopback;    // dropped by the uid/callsign filters
  unsigned int m_cot_not_listed;  // dropped by the callsign whitelist
  unsigned int m_reports_posted;  // NODE_REPORTs published
  unsigned int m_tracks_dropped;  // tracks expired on stale_timeout
  unsigned int m_geo_failures;    // lat/lon → XY conversions that failed
};

#endif // COT_TRACK_HEADER
