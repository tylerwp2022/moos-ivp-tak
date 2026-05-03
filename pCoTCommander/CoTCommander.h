/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander.h                                  */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTCommander — inbound CoT command dispatcher.         */
/*                                                          */
/*  Subscribes to COT_INBOUND (raw CoT XML published by     */
/*  pCoTBridge) and ATAK_CHAT_IN (published by pCoTChat)    */
/*  and translates operator commands from ATAK into MOOS    */
/*  variable publications for pHelmIvP behaviors.           */
/*                                                          */
/*  TWO DEPLOYMENT MODES                                    */
/*  ─────────────────────────────────────────────────────── */
/*  fleet_mode = true  (runs on SHORESIDE MOOSDB):          */
/*    command_chatroom = AQUATICUS-SHORE                     */
/*    Chat commands post *_ALL variables, which             */
/*    uFldShoreBroker routes to all vehicle communities.    */
/*    Any command can be prefixed with a vehicle name to    */
/*    target that vehicle instead of the whole fleet:       */
/*      "deploy"          → DEPLOY_ALL=true                 */
/*      "blue_one deploy" → DEPLOY_BLUE_ONE=true            */
/*      "attack"          → ACTION_ALL=ATTACK_MED           */
/*      "blue_one attack" → ACTION_BLUE_ONE=ATTACK_MED      */
/*    Vehicle names must use underscores (blue_one, not     */
/*    blue one). The prefix works for all commands except   */
/*    play/stop (fleet-wide only).                          */
/*                                                          */
/*  fleet_mode = false (runs on VEHICLE MOOSDB):            */
/*    command_chatroom = <vehicle callsign, e.g. blue_one>  */
/*    Chat commands post bare variable names directly       */
/*    on that vehicle's MOOSDB.                             */
/*    Role commands use bare "attack|defend" (no prefix).   */
/*    PREREQUISITE: ATAK_CHAT_IN and COT_INBOUND must be    */
/*    shared from shore to the vehicle via pShare.          */
/*                                                          */
/*  Architecture:                                           */
/*    TAK Server                                            */
/*      ↓ (TCP)                                             */
/*    pCoTBridge → COT_INBOUND                              */
/*    pCoTChat   → ATAK_CHAT_IN                             */
/*                      ↓                                   */
/*               pCoTCommander                              */
/*                      ↓                                   */
/*    CoT commands  → ATAK_MODE + ATAK_WAYPT_ACTIVE +       */
/*                    ATAK_WPT_UPDATE                        */
/*    Chat commands → DEPLOY[_ALL], RETURN[_ALL],           */
/*                    STATION_KEEP[_ALL],                   */
/*                    MOOS_MANUAL_OVERRIDE[_ALL],           */
/*                    AQUATICUS_GAME_ALL (fleet only),       */
/*                    ACTION[_<VEHICLE>]                    */
/*                                                          */
/*  Handled CoT types:                                      */
/*    b-m-p-w-GOTO → waypoint command                       */
/*      Publishes: ATAK_MODE         = true                 */
/*                 ATAK_WAYPT_ACTIVE = true                 */
/*                 ATAK_WPT_UPDATE = points=x,y #           */
/*                                   capture_radius=r       */
/*      Requires waypt_atak behavior in .bhv with:          */
/*        condition = ATAK_MODE = true                      */
/*        condition = ATAK_WAYPT_ACTIVE = true              */
/*        updates   = ATAK_WPT_UPDATE                       */
/*                                                          */
/*  Supported chat commands (via ATAK GeoChat):             */
/*    deploy          → DEPLOY[_ALL]=true + overrides       */
/*    return | rtb    → RETURN[_ALL]=true                   */
/*                      ATAK_MODE[_ALL]=false               */
/*                      ATAK_WAYPT_ACTIVE[_ALL]=false       */
/*    station | hold  → STATION_KEEP[_ALL]=true             */
/*                      ATAK_MODE[_ALL]=false               */
/*                      ATAK_WAYPT_ACTIVE[_ALL]=false       */
/*    pause           → MOOS_MANUAL_OVERRIDE[_ALL]=true     */
/*                      ATAK_MODE[_ALL]=false               */
/*                      ATAK_WAYPT_ACTIVE[_ALL]=false       */
/*    atak            → ATAK_MODE[_ALL]=true                */
/*                      Enters operator control; game       */
/*                      behaviors yield until "resume".     */
/*    resume          → ATAK_MODE[_ALL]=false               */
/*                      ATAK_WAYPT_ACTIVE[_ALL]=false       */
/*                      Returns to autonomous strategy.     */
/*    play            → AQUATICUS_GAME_ALL=play (fleet only)*/
/*    stop            → AQUATICUS_GAME_ALL=pause (fleet only*/
/*    status          → DM reply with deployment state      */
/*    attack|defend   → ACTION[_ALL]=ATTACK/DEFEND_MED      */
/*    attack|defend easy → ACTION[_ALL]=ATTACK/DEFEND_E     */
/*    help            → DM reply listing all commands       */
/*    vehicle <cmd>   → per-vehicle variant of any command  */
/*                      (fleet mode; use underscores)       */
/*                                                          */
/*  Adding new command types:                               */
/*    CoT: add a handler method + case in dispatchInboundCoT*/
/*    Chat: add a branch in handleChatCommand()             */
/*    No changes needed in pCoTBridge                       */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: COT_INBOUND    (from pCoTBridge)          */
/*                ATAK_CHAT_IN   (from pCoTChat)            */
/*                NODE_REPORT    (for NAV anchor — geodesy) */
/*                NODE_REPORT_LOCAL                         */
/*                ATAK_WPT_REACHED (waypt endflag)          */
/*                DEPLOY          (deployment state)        */
/*                ATAK_MODE       (vehicle mode only —      */
/*                  warns operator when attack/defend sent  */
/*                  while game behaviors are suppressed)    */
/*    Publishes:  ATAK_MODE     (bool string)               */
/*                ATAK_WAYPT_ACTIVE (bool string)            */
/*                ATAK_WPT_UPDATE (BHV_Waypoint update str) */
/*                DEPLOY[_ALL], RETURN[_ALL],               */
/*                STATION_KEEP[_ALL],                       */
/*                MOOS_MANUAL_OVERRIDE[_ALL],               */
/*                AQUATICUS_GAME_ALL, ACTION[_<VEHICLE>]    */
/************************************************************/

#ifndef COT_COMMANDER_HEADER
#define COT_COMMANDER_HEADER

#include <string>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "CoTGeodesy.h"

class CoTCommander : public AppCastingMOOSApp
{
public:
  CoTCommander();
  virtual ~CoTCommander() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // --------------------------------------------------------
  // CoT parsing utilities
  //
  // extractAttr() is a lightweight attribute extractor for
  // CoT XML. CoT is simple enough that a full XML parser
  // is not needed — attribute scanning is sufficient and
  // avoids a libxml2 dependency.
  // --------------------------------------------------------

  // Extract a named attribute value from a CoT XML string.
  // Handles both single and double quoted attribute values.
  // Example:
  //   extractAttr("<event uid=\"foo\" type=\"bar\">", "uid") → "foo"
  std::string extractAttr(const std::string& xml,
                           const std::string& attr);

  // Dispatch a received CoT event to the appropriate handler.
  // Returns true if the event was handled (known type).
  bool dispatchInboundCoT(const std::string& xml);

  // --------------------------------------------------------
  // Command handlers
  //
  // Each handler corresponds to one CoT type or family.
  // Add new handlers here as new command types are needed.
  // --------------------------------------------------------

  // b-m-p-w-GOTO — "Go To" waypoint from ATAK.
  // Converts lat/lon to local XY via CoTGeodesy and publishes
  // ATAK_MODE=true + ATAK_WAYPT_ACTIVE=true + ATAK_WPT_UPDATE
  // to pHelmIvP.
  void handleWaypointCoT(const std::string& uid,
                          double lat, double lon,
                          const std::string& xml);

  // ATAK_CHAT_IN — GeoChat command from operator.
  // Parses "callsign=X,chatroom=Y,message=Z", filters on
  // command_chatroom, dispatches to fleet or vehicle commands.
  void handleChatCommand(const std::string& moos_val);

private:
  // --------------------------------------------------------
  // Geodesy — LatLon → local XY for waypoint conversion
  // --------------------------------------------------------
  CoTGeodesy  m_geodesy;
  bool        m_geodesy_initialized;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------

  // MOOS variable published when ATAK sends a waypoint.
  // Must match 'updates = ATAK_WPT_UPDATE' in waypt_atak .bhv
  std::string m_waypoint_update_var;  // default: ATAK_WPT_UPDATE

  // How close (meters) robot must get before waypoint is captured.
  // Sent in the WPT_UPDATE string as "capture_radius=r".
  double m_capture_radius;            // default: 15.0m

  // Optional: only accept commands from this ATAK device UID.
  // Leave empty to accept from any connected ATAK client.
  std::string m_operator_uid_filter;

  // Whether to enable each command type
  bool m_enable_waypoint_control;

  // --------------------------------------------------------
  // Chat command config
  // --------------------------------------------------------

  // Enable/disable the ATAK_CHAT_IN command interface.
  bool        m_enable_chat_commands;

  // Only process chat messages directed to this chatroom.
  // Fleet mode:   set to shore callsign (e.g. "AQUATICUS-SHORE")
  // Vehicle mode: set to the vehicle callsign (e.g. "blue_one")
  std::string m_command_chatroom;

  // Fleet mode: post *_ALL variables — uFldShoreBroker routes
  //             them to all vehicle communities.
  // Vehicle mode: post bare variable names directly on this
  //               vehicle's MOOSDB.
  bool        m_fleet_mode;

  bool m_debug;

  // --------------------------------------------------------
  // Debug message circular buffer
  // --------------------------------------------------------
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_cot_received;
  unsigned int m_cot_handled;
  unsigned int m_cot_ignored;
  unsigned int m_waypoint_commands;
  unsigned int m_chat_commands;
  std::string  m_last_command;        // description of last command for AppCast

  // Last waypoint sender — used for acknowledgment DM back to operator
  std::string m_last_sender_callsign;
  double      m_last_wpt_lat;
  double      m_last_wpt_lon;

  // Deployment state — tracked from DEPLOY MOOS variable.
  // Waypoints are rejected if the robot is not deployed.
  bool        m_deployed;

  // Tracks the ATAK_MODE variable published on this vehicle's MOOSDB.
  // Only subscribed and tracked in vehicle mode (fleet_mode=false) —
  // the shore MOOSDB never holds a bare ATAK_MODE variable.
  // Used to warn the operator when attack/defend commands are sent
  // while the vehicle is in ATAK mode (game behaviors are suppressed,
  // so the ACTION post would have no visible effect until resume).
  bool        m_atak_mode;

  // Guards against repeated "Waypoint reached" messages.
  // Set true after the first reached notification for a given
  // waypoint. Reset to false when a new waypoint arrives.
  bool        m_wpt_reached_sent;
};

#endif // COT_COMMANDER_HEADER
