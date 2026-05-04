/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander.h                                  */
/*    DATE: April 2026 (updated May 2026)                   */
/*                                                          */
/*  pCoTCommander -- inbound CoT command dispatcher.        */
/*                                                          */
/*  Subscribes to COT_INBOUND (raw CoT XML published by     */
/*  pCoTBridge) and ATAK_CHAT_IN (published by pCoTChat)    */
/*  and translates operator commands from ATAK into MOOS    */
/*  variable publications for pHelmIvP behaviors.           */
/*                                                          */
/*  TWO DEPLOYMENT MODES                                    */
/*  -------------------------------------------------------- */
/*  fleet_mode = true  (runs on SHORESIDE MOOSDB):          */
/*    command_chatroom = AQUATICUS-SHORE                     */
/*    Chat commands post *_ALL variables, which             */
/*    uFldShoreBroker routes to all vehicle communities.    */
/*    Any command can be prefixed with a vehicle name to    */
/*    target that vehicle instead of the whole fleet:       */
/*      "deploy"          -> DEPLOY_ALL=true                */
/*      "blue_one deploy" -> DEPLOY_BLUE_ONE=true           */
/*      "attack"          -> ACTION_ALL=ATTACK_MED          */
/*      "blue_one atak"   -> ATAK_MODE_BLUE_ONE=true        */
/*    Vehicle names must use underscores (blue_one, not     */
/*    blue one). The prefix works for all commands except   */
/*    play/stop (fleet-wide only).                          */
/*                                                          */
/*  fleet_mode = false (runs on VEHICLE MOOSDB):            */
/*    command_chatroom = <vehicle callsign, e.g. blue_one>  */
/*    Chat commands post bare variable names directly       */
/*    on that vehicle's MOOSDB.                             */
/*    PREREQUISITE: pCoTBridge and pCoTChat must be running */
/*    on the vehicle (already present in meta_surveyor.moos)*/
/*                                                          */
/*  Architecture:                                           */
/*    TAK Server                                            */
/*      | (TCP)                                             */
/*    pCoTBridge -> COT_INBOUND                             */
/*    pCoTChat   -> ATAK_CHAT_IN                            */
/*                      |                                   */
/*               pCoTCommander                              */
/*                      |                                   */
/*    CoT commands  -> ATAK_MODE, ATAK_WAYPT_ACTIVE,        */
/*                     ATAK_WPT_UPDATE                      */
/*    Chat commands -> DEPLOY[_ALL], RETURN[_ALL],          */
/*                     STATION_KEEP[_ALL],                  */
/*                     MOOS_MANUAL_OVERRIDE[_ALL],          */
/*                     ATAK_MODE[_ALL],                     */
/*                     ATAK_WAYPT_ACTIVE[_ALL],             */
/*                     ATAK_AVOID_COLLISIONS[_ALL],         */
/*                     ATAK_AUTO_UNTAG[_ALL],               */
/*                     ATAK_RETRY[_ALL],                    */
/*                     ATAK_OPREG_RECOVER[_ALL],            */
/*                     AQUATICUS_GAME_ALL (fleet only),     */
/*                     ACTION[_<VEHICLE>]                   */
/*                                                          */
/*  ATAK MODE                                               */
/*  -------------------------------------------------------- */
/*  When ATAK_MODE=true, game behaviors (attack/defend/     */
/*  loiter) yield because they condition on ATAK_MODE!=true.*/
/*  The waypt_atak behavior (pwt=100) activates when both   */
/*  ATAK_MODE=true and ATAK_WAYPT_ACTIVE=true.             */
/*  BHV_OpRegionRecover (pwt=300) and BHV_AvdColregsV22    */
/*  (pwt=300) are independently toggleable via chat.        */
/*  ATAK mode is cleared by: resume, return, station, pause.*/
/*                                                          */
/*  Handled CoT types:                                      */
/*    b-m-p-w-GOTO -> waypoint command                      */
/*      Rejects if not deployed or geodesy not ready,       */
/*      with a DM explanation to the operator in both cases.*/
/*      On success publishes:                               */
/*        ATAK_MODE         = true                          */
/*        ATAK_WAYPT_ACTIVE = true                          */
/*        ATAK_WPT_UPDATE   = points=x,y # capture_radius=r*/
/*      Requires in meta_surveyor.bhv:                      */
/*        name      = waypt_atak                            */
/*        condition = ATAK_MODE = true                      */
/*        condition = ATAK_WAYPT_ACTIVE = true              */
/*        condition = (TAGGED != true) or                   */
/*                    (ATAK_AUTO_UNTAG = false)             */
/*        updates   = ATAK_WPT_UPDATE                       */
/*        endflag   = ATAK_WPT_REACHED = true               */
/*                                                          */
/*  Supported chat commands (via ATAK GeoChat):             */
/*    -- Mode control --                                     */
/*    atak            -> ATAK_MODE[_ALL]=true               */
/*                       Suppresses game behaviors.         */
/*    resume          -> ATAK_MODE[_ALL]=false              */
/*                       ATAK_WAYPT_ACTIVE[_ALL]=false      */
/*                       Restores game behaviors.           */
/*                                                          */
/*    -- Fleet / deployment --                              */
/*    deploy          -> DEPLOY[_ALL]=true + overrides      */
/*    return | rtb    -> RETURN[_ALL]=true, exits ATAK mode */
/*    station | hold  -> STATION_KEEP[_ALL]=true,           */
/*                       exits ATAK mode                    */
/*    pause           -> MOOS_MANUAL_OVERRIDE[_ALL]=true,   */
/*                       exits ATAK mode                    */
/*    play            -> AQUATICUS_GAME_ALL=play            */
/*                       (fleet mode only)                  */
/*    stop            -> AQUATICUS_GAME_ALL=pause           */
/*                       (fleet mode only)                  */
/*                                                          */
/*    -- ATAK behavior toggles (default all on) --          */
/*    avoid on|off    -> ATAK_AVOID_COLLISIONS[_ALL]=t|f    */
/*                       Gates BHV_AvdColregsV22 and        */
/*                       BHV_AvoidCollision in ATAK mode.   */
/*    untag on|off    -> ATAK_AUTO_UNTAG[_ALL]=t|f          */
/*                       When off, vehicle ignores tags and  */
/*                       stays on ATAK waypoint.            */
/*    retry on|off    -> ATAK_RETRY[_ALL]=t|f               */
/*                       When off, waypoint is cleared after */
/*                       tag recovery; operator must resend. */
/*    opreg on|off    -> ATAK_OPREG_RECOVER[_ALL]=t|f       */
/*                       Gates BHV_OpRegionRecover in ATAK  */
/*                       mode. Warning DM sent on opreg off.*/
/*                                                          */
/*    -- Role assignment --                                  */
/*    attack          -> ACTION[_ALL]=ATTACK_MED            */
/*    attack easy     -> ACTION[_ALL]=ATTACK_E              */
/*    defend          -> ACTION[_ALL]=DEFEND_MED            */
/*    defend easy     -> ACTION[_ALL]=DEFEND_E              */
/*    (warns if vehicle is in ATAK mode -- role takes effect */
/*    only after resume)                                     */
/*                                                          */
/*    -- Info --                                             */
/*    status          -> DM: deployment + ATAK state        */
/*    help            -> DM: mode-appropriate command list  */
/*                       (vehicle mode omits fleet-only cmds*/
/*    vehicle <cmd>   -> per-vehicle targeting (fleet mode) */
/*                       e.g. "blue_one atak", "red_two avoid off" */
/*                                                          */
/*  All commands DM a confirmation or error back to sender. */
/*  Operator always knows the outcome of every command.     */
/*                                                          */
/*  Adding new command types:                               */
/*    CoT:  add a handler method + case in dispatchInboundCoT*/
/*    Chat: add a branch in handleChatCommand()             */
/*    No changes needed in pCoTBridge or pCoTChat           */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: COT_INBOUND      (from pCoTBridge)        */
/*                ATAK_CHAT_IN     (from pCoTChat)          */
/*                NODE_REPORT      (geodesy NAV anchor)     */
/*                NODE_REPORT_LOCAL                         */
/*                ATAK_WPT_REACHED (waypt_atak endflag)     */
/*                DEPLOY           (deployment state)       */
/*                ATAK_MODE        (vehicle mode only)      */
/*                TAGGED           (vehicle mode only)      */
/*    Publishes:  ATAK_MODE[_ALL]            bool           */
/*                ATAK_WAYPT_ACTIVE[_ALL]    bool           */
/*                ATAK_WPT_UPDATE            BHV update str */
/*                ATAK_AVOID_COLLISIONS[_ALL] bool          */
/*                ATAK_AUTO_UNTAG[_ALL]      bool           */
/*                ATAK_RETRY[_ALL]           bool           */
/*                ATAK_OPREG_RECOVER[_ALL]   bool           */
/*                DEPLOY[_ALL]               bool           */
/*                RETURN[_ALL]               bool           */
/*                STATION_KEEP[_ALL]         bool           */
/*                MOOS_MANUAL_OVERRIDE[_ALL] bool           */
/*                AQUATICUS_GAME_ALL         play|pause     */
/*                ACTION[_<VEHICLE>]         role string    */
/*                ATAK_CHAT_OUT              DM to operator */
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
  // CoT XML. CoT is simple enough that a full XML parser is
  // not needed -- attribute scanning avoids a libxml2 dep.
  // --------------------------------------------------------

  // Extract a named attribute value from a CoT XML string.
  // Handles both single and double quoted attribute values.
  // Example:
  //   extractAttr("<event uid=\"foo\" type=\"bar\">", "uid") -> "foo"
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

  // b-m-p-w-GOTO -- "Go To" waypoint from ATAK.
  // Converts lat/lon to local XY via CoTGeodesy and publishes
  // ATAK_MODE=true + ATAK_WAYPT_ACTIVE=true + ATAK_WPT_UPDATE.
  // DMs rejection reason if not deployed or geodesy not ready.
  void handleWaypointCoT(const std::string& uid,
                          double lat, double lon,
                          const std::string& xml);

  // ATAK_CHAT_IN -- GeoChat command from operator.
  // Parses "callsign=X,chatroom=Y,message=Z", filters on
  // command_chatroom, dispatches to the appropriate handler.
  // Every command DMs a confirmation or error back to sender.
  void handleChatCommand(const std::string& moos_val);

private:
  // --------------------------------------------------------
  // Geodesy -- LatLon -> local XY for waypoint conversion
  // --------------------------------------------------------
  CoTGeodesy  m_geodesy;
  bool        m_geodesy_initialized;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------

  // MOOS variable name for the BHV_Waypoint update string.
  // Must match 'updates = ATAK_WPT_UPDATE' in waypt_atak .bhv
  std::string m_waypoint_update_var;  // default: ATAK_WPT_UPDATE

  // How close (meters) the robot must get to capture the waypoint.
  // Sent in the WPT_UPDATE string as "capture_radius=r".
  double m_capture_radius;            // default: 15.0 m

  // Optional: only accept CoT commands from this ATAK device UID.
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
  // Fleet mode:   shore callsign (e.g. "AQUATICUS-SHORE")
  // Vehicle mode: vehicle callsign (e.g. "blue_one" = $(VNAME))
  std::string m_command_chatroom;

  // Fleet mode:   post *_ALL vars -- uFldShoreBroker routes them.
  // Vehicle mode: post bare variable names directly on own MOOSDB.
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
  std::string  m_last_command;  // last command for AppCast

  // Last waypoint sender -- for acknowledgment DMs
  std::string m_last_sender_callsign;
  double      m_last_wpt_lat;
  double      m_last_wpt_lon;

  // --------------------------------------------------------
  // Runtime state
  // --------------------------------------------------------

  // Deployment state -- tracked from DEPLOY MOOS variable.
  // Waypoints are rejected with a DM if not deployed.
  bool        m_deployed;

  // Vehicle-mode state tracking (fleet_mode=false only).
  // Registered from MOOSDB so these mirror actual behavior state.
  //
  // m_atak_mode:  mirrors ATAK_MODE; used to warn when attack/
  //   defend sent while game behaviors are suppressed.
  //
  // m_tagged:     mirrors TAGGED; used to detect the untagged
  //   transition (true->false) for retry-off logic: when the
  //   robot returns home after being tagged, ATAK_WAYPT_ACTIVE
  //   is cleared if m_atak_retry=false so the robot holds
  //   position rather than immediately retrying the objective.
  //
  // m_atak_retry: local copy of ATAK_RETRY; true = resume
  //   waypoint automatically after being untagged (default),
  //   false = clear waypoint and wait for operator to resend.
  bool        m_atak_mode;
  bool        m_tagged;
  bool        m_atak_retry;

  // Guards against repeated "Waypoint reached" messages while
  // the robot idles at the capture point.
  // Set true on first reached notification; reset on new waypoint.
  bool        m_wpt_reached_sent;
};

#endif // COT_COMMANDER_HEADER
