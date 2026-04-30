/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGraphics.h                                   */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTGraphics — VIEW_* to CoT graphics renderer.         */
/*                                                          */
/*  Subscribes to MOOS VIEW_* variables published by        */
/*  pHelmIvP, uFldFlagManager, and uFldTagManager on the   */
/*  shore MOOSDB, converts them to CoT XML, and publishes   */
/*  to COT_OUTBOUND for pCoTBridge to forward to TAK.       */
/*                                                          */
/*  Supported conversions:                                  */
/*    VIEW_POINT       → spot marker     (b-m-p-s-m)        */
/*    VIEW_SEGLIST     → open polyline   (u-d-f)            */
/*    VIEW_POLYGON     → closed polygon  (u-d-f); filled    */
/*                       only if fill_color explicit in src */
/*    VIEW_CIRCLE      → circle          (u-d-c-c)          */
/*    UTM_ZONE_ONE/TWO → filled polygon  (team zone bounds) */
/*    VIEW_MARKER      → colored flag marker (b-m-p-s-m)   */
/*    FLAG_SUMMARY     → colored flag markers (b-m-p-s-m)  */
/*    VIEW_TEXTBOX     → text label on map (b-m-p-s-m/LABEL)*/
/*    active=false     → delete event    (t-x-d-d)          */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: VIEW_POINT, VIEW_SEGLIST,                 */
/*                VIEW_POLYGON, VIEW_CIRCLE,                */
/*                VIEW_MARKER, FLAG_SUMMARY,                */
/*                UTM_ZONE_ONE, UTM_ZONE_TWO,               */
/*                VIEW_TEXTBOX,                             */
/*                NODE_REPORT, NODE_REPORT_LOCAL            */
/*    Publishes:  COT_OUTBOUND (raw CoT XML strings)        */
/************************************************************/

#ifndef COT_GRAPHICS_HEADER
#define COT_GRAPHICS_HEADER

#include <string>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "CoTGeodesy.h"

// ============================================================
// ViewPoint — one VIEW_POINT entry (waypoints, trackpts)
// ============================================================
struct ViewPoint {
  std::string label;
  double      lat       = 0.0;
  double      lon       = 0.0;
  double      last_sent = 0.0;
  bool        valid     = false;
};

// ============================================================
// ViewSegList — one VIEW_SEGLIST entry (open polyline)
// ============================================================
struct ViewSegList {
  std::string label;
  std::vector<std::pair<double,double>> vertices; // (lat, lon)
  double last_sent = 0.0;
  bool   valid     = false;
};

// ============================================================
// ViewPolygon — one VIEW_POLYGON or UTM_ZONE_* entry
//
// Same pts={} vertex format as VIEW_SEGLIST but:
//   - carries fill_color_argb and edge_color_argb
//   - rendered as a closed filled polygon in ATAK
//   - vertex 0 is repeated at the end to close the shape
//   - uses how="h-e" (confirmed from live ATAK polygon capture)
//
// Sources:
//   VIEW_POLYGON  — uFldFlagManager: flag grab zone circles
//                   (24-vertex circle approximation, grey fill)
//   UTM_ZONE_ONE  — uFldTagManager:  red team boundary (pink fill)
//   UTM_ZONE_TWO  — uFldTagManager:  blue team boundary (light blue)
// ============================================================
struct ViewPolygon {
  std::string label;
  std::vector<std::pair<double,double>> vertices; // (lat, lon)
  int    fill_color_argb = -2130706433; // default: semi-transparent white
  int    edge_color_argb = -1;          // default: opaque white
  bool   filled          = false;       // true = include <fillColor> in CoT.
                                        // false = closed outline, no fill.
                                        // Polygon is always closed (vertex[0]
                                        // repeated) regardless of this flag.
                                        // Set true only when fill_color is
                                        // explicitly present in the raw string,
                                        // or when hardcoded (UTM_ZONE_*).
  double last_sent       = 0.0;
  bool   valid           = false;
};

// ============================================================
// ViewCircle — one VIEW_CIRCLE entry
//
// Rendered as a CoT circle (u-d-c-c) using the ellipse element
// with equal major/minor axes (radius in meters) and angle=360.
//
// COLOR ENCODING — two systems in one CoT:
//   KML Style block: ABGR hex strings (KML convention, not ARGB)
//   <fillColor value="...">: signed ARGB integer (ATAK native)
// Both must be set consistently — see buildViewCircleCoT().
//
// Default: transparent fill (outline-only circle).
// Source: VIEW_CIRCLE — pHelmIvP loiter regions, tag zones
// ============================================================
struct ViewCircle {
  std::string label;
  double lat              = 0.0;
  double lon              = 0.0;
  double radius           = 0.0;      // meters
  int    edge_color_argb  = -1;       // 0xFFFFFFFF opaque white
  int    fill_color_argb  = 16777215; // 0x00FFFFFF transparent
  double stroke_weight    = 2.4;
  double last_sent        = 0.0;
  bool   valid            = false;
};

// ============================================================
// ViewMarkerGraphic — one VIEW_MARKER or FLAG_SUMMARY entry
//
// Rendered as a colored b-m-p-s-m spot marker in ATAK.
// The ARGB integer is embedded in BOTH the <color> element
// AND the iconsetpath to correctly tint the icon:
//   iconsetpath="COT_MAPPING_SPOTMAP/b-m-p-s-m/<argb>"
//   e.g. "-65536" for red, "-16776961" for blue
//
// Sources:
//   FLAG_SUMMARY — uFldFlagManager: all flags, '#'-delimited
//   VIEW_MARKER  — uFldFlagManager: single flag state update
//
// Format: x=X,y=Y,width=W,range=R,primary_color=red,label=red
// ============================================================
struct ViewMarkerGraphic {
  std::string label;
  double lat        = 0.0;
  double lon        = 0.0;
  int    color_argb = -1;
  double last_sent  = 0.0;
  bool   valid      = false;
};

// ============================================================
// ViewTextBox — one VIEW_TEXTBOX entry (score display)
//
// Rendered as a b-m-p-s-m/LABEL spot marker whose callsign
// IS the display text. This places a text label at a fixed
// map position — used to show the live score.
//
// In Aquaticus, post_score=$(XE) in uFldFlagManager places
// the score label at the east midfield position (XE).
//
// Format: x=X,y=Y,msg="RED:0 BLUE:0",fsize=20,mcolor=yellow
//
// Source: VIEW_TEXTBOX — uFldFlagManager on score change
// ============================================================
struct ViewTextBox {
  std::string label   = "score";  // fixed key in m_view_textboxes map
  std::string msg;                // the text to display ("RED:0 BLUE:0")
  double lat        = 0.0;
  double lon        = 0.0;
  int    color_argb = -256;       // default yellow: 0xFFFFFF00
  double last_sent  = 0.0;
  bool   valid      = false;
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
  // Return false if active=false (sends delete CoT + cleans up)
  // or if required fields are missing / geodesy not ready.
  // --------------------------------------------------------
  bool parseViewPoint(const std::string& raw,         ViewPoint&         vp_out);
  bool parseViewSegList(const std::string& raw,       ViewSegList&       vsl_out);
  bool parseViewPolygon(const std::string& raw,       ViewPolygon&       vp_out,
                        const std::string& map_key);
  bool parseViewMarkerGraphic(const std::string& raw, ViewMarkerGraphic& vm_out);
  bool parseFlagSummary(const std::string& raw);
  bool parseViewTextBox(const std::string& raw,       ViewTextBox&       vtb_out);
  bool parseViewCircle(const std::string& raw,         ViewCircle&        vc_out);

  // --------------------------------------------------------
  // CoT builders
  // --------------------------------------------------------
  std::string buildViewPointCoT(const ViewPoint& vp);
  std::string buildViewSegListCoT(const ViewSegList& vsl);
  std::string buildViewPolygonCoT(const ViewPolygon& vp);
  std::string buildViewCircleCoT(const ViewCircle& vc);
  std::string buildViewMarkerGraphicCoT(const ViewMarkerGraphic& vm);
  std::string buildViewTextBoxCoT(const ViewTextBox& vtb);
  std::string buildDeleteCoT(const std::string& target_uid,
                              double lat, double lon);
  std::string formatCoTTime(double moos_time,
                             double stale_offset_sec = 0.0);

  // --------------------------------------------------------
  // Shared pts={x,y:x,y:...} extraction helper.
  // Used by parseViewSegList and parseViewPolygon — avoids
  // duplicating the vertex parsing and geodesy conversion.
  // --------------------------------------------------------
  bool parsePtsBlock(const std::string& raw,
                     std::vector<std::pair<double,double>>& vertices_out);

  // --------------------------------------------------------
  // Color helper — MOOS color name + fill_transparency
  // → signed ATAK ARGB integer (0xAARRGGBB)
  // transparency: 0.0 = fully opaque, 1.0 = fully transparent
  // --------------------------------------------------------
  int moosColorToArgb(const std::string& color_name,
                      double transparency = 0.0);

  // Sanitize label for use in CoT UID (spaces/apostrophes → _)
  std::string sanitizeLabel(const std::string& label);

  // Returns true if the label should be dropped (shoreside mode
  // vehicle-name filter or legacy label_block_contains patterns).
  bool isLabelBlocked(const std::string& label) const;

private:
  // --------------------------------------------------------
  // Geodesy
  // --------------------------------------------------------
  CoTGeodesy  m_geodesy;
  bool        m_geodesy_initialized;

  // --------------------------------------------------------
  // Tracked graphics state — keyed by label
  // --------------------------------------------------------
  std::map<std::string, ViewPoint>         m_view_points;
  std::map<std::string, ViewSegList>       m_view_seglists;
  std::map<std::string, ViewPolygon>       m_view_polygons;
  std::map<std::string, ViewCircle>        m_view_circles;
  std::map<std::string, ViewMarkerGraphic> m_view_marker_graphics;
  std::map<std::string, ViewTextBox>       m_view_textboxes;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------
  bool   m_publish_view_points;
  bool   m_publish_view_seglists;
  bool   m_publish_view_polygons;    // VIEW_POLYGON + UTM_ZONE_*
  bool   m_publish_view_circles;     // VIEW_CIRCLE
  bool   m_publish_flag_markers;     // FLAG_SUMMARY + VIEW_MARKER
  bool   m_publish_score_label;      // VIEW_TEXTBOX score label

  bool   m_immediate_view_points;    // send VIEW_POINT on every update
  bool   m_immediate_view_seglists;  // send VIEW_SEGLIST on every update;
                                     // set true on vehicle for avoidance segs
  double m_stationary_send_interval; // seconds between throttled sends
  double m_cot_stale_offset;         // seconds graphics persist in ATAK

  // --------------------------------------------------------
  // Shoreside mode — vehicle label filtering
  //
  // shoreside = true: any VIEW_* label containing a vehicle name
  //   from vehicle_names is dropped before CoT is built.
  // vehicle_names: colon-separated list matching $(VNAMES).
  // shoreside = false (default): no filtering.
  // --------------------------------------------------------
  bool                     m_shoreside_mode;
  std::set<std::string>    m_vehicle_names;
  std::vector<std::string> m_label_block_contains; // legacy fallback

  bool   m_debug;
  static const int DEBUG_BUF_SIZE = 12;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_vp_cot_sent;
  unsigned int m_vsl_cot_sent;
  unsigned int m_poly_cot_sent;
  unsigned int m_circle_cot_sent;
  unsigned int m_flag_cot_sent;
  unsigned int m_text_cot_sent;
  unsigned int m_delete_cot_sent;
};

#endif // COT_GRAPHICS_HEADER
