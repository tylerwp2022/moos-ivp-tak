/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTChat.h                                       */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTChat — ATAK GeoChat ↔ MOOS bridge.                 */
/*                                                          */
/*  INBOUND (TAK → MOOS):                                  */
/*    Subscribes to COT_INBOUND, filters for b-t-f type,   */
/*    extracts sender callsign, chatroom, and message text  */
/*    from <__chat> and <remarks> elements, publishes:      */
/*      ATAK_CHAT_IN = callsign=X,chatroom=Y,message=Z     */
/*                                                          */
/*  OUTBOUND (MOOS → TAK):                                  */
/*    Subscribes to ATAK_CHAT_OUT. Format:                  */
/*      ATAK_CHAT_OUT = message=hello,chatroom=All Chat Rooms */
/*    Builds appropriate GeoChat CoT based on chatroom:     */
/*      "All Chat Rooms" → all-rooms broadcast              */
/*      "Groups"/"UserGroups" → all groups broadcast        */
/*      "Teams"/"TeamGroups" → all teams broadcast          */
/*      Known team color (Cyan,Red,etc) → team broadcast    */
/*      Known role (HQ,etc) → role broadcast                */
/*      Known group name → group message with <hierarchy>   */
/*      Anything else → direct message to that callsign     */
/*                                                          */
/*  CONTACT TRACKING:                                       */
/*    Builds a callsign→UID table from SA contact CoT       */
/*    (a-f-*, a-h-*) seen in COT_INBOUND. Used to resolve  */
/*    UIDs for group message <chatgrp> and <hierarchy>.     */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: COT_INBOUND    (from pCoTBridge)          */
/*                ATAK_CHAT_OUT  (outbound message request) */
/*                NODE_REPORT    (position for outbound CoT)*/
/*                NODE_REPORT_LOCAL                         */
/*    Publishes:  COT_OUTBOUND   (GeoChat CoT to TAK)      */
/*                ATAK_CHAT_IN   (parsed inbound message)   */
/************************************************************/

#ifndef COT_CHAT_HEADER
#define COT_CHAT_HEADER

#include <string>
#include <map>
#include <vector>
#include <set>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

// ============================================================
// ContactInfo — one tracked ATAK contact
// ============================================================
struct ContactInfo {
  std::string uid;
  std::string callsign;
  double      last_seen = 0.0; // MOOS time of last SA CoT
};

// ============================================================
// ChatDestType — resolved destination type for outbound messages
// ============================================================
enum ChatDestType {
  CHAT_ALL_ROOMS,    // "All Chat Rooms"
  CHAT_ALL_GROUPS,   // "Groups" / "UserGroups"
  CHAT_ALL_TEAMS,    // "Teams" / "TeamGroups"
  CHAT_TEAM_COLOR,   // "Cyan", "Red", "Blue", etc.
  CHAT_ROLE,         // "HQ", etc.
  CHAT_GROUP,        // named group with known members
  CHAT_DIRECT        // direct message to a callsign
};


class CoTChat : public AppCastingMOOSApp
{
public:
  CoTChat();
  virtual ~CoTChat() {}

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // --------------------------------------------------------
  // XML utilities — shared with pCoTCommander approach
  // --------------------------------------------------------
  std::string extractAttr(const std::string& xml,
                           const std::string& attr);
  std::string extractTagContent(const std::string& xml,
                                 const std::string& tag);

  // --------------------------------------------------------
  // Contact tracking
  // Parses SA CoT (a-f-*, a-h-*) to build callsign→UID table
  // --------------------------------------------------------
  void updateContactTable(const std::string& xml);

  // --------------------------------------------------------
  // Inbound chat parsing
  // --------------------------------------------------------
  void handleInboundChat(const std::string& xml);

  // --------------------------------------------------------
  // Outbound chat building
  // Parses ATAK_CHAT_OUT = "message=X,chatroom=Y"
  // and builds the appropriate GeoChat CoT
  // --------------------------------------------------------
  void handleOutboundChat(const std::string& moos_val);

  // Resolve destination type from chatroom string
  ChatDestType resolveDestType(const std::string& chatroom) const;

  // CoT builders — one per destination type
  std::string buildAllRoomsCoT(const std::string& message);
  std::string buildAllGroupsCoT(const std::string& message);
  std::string buildAllTeamsCoT(const std::string& message);
  std::string buildTeamColorCoT(const std::string& chatroom,
                                 const std::string& message);
  std::string buildRoleCoT(const std::string& chatroom,
                            const std::string& message);
  std::string buildGroupCoT(const std::string& group_name,
                             const std::string& message);
  std::string buildDirectCoT(const std::string& callsign,
                              const std::string& message);

  // Shared CoT assembly — builds the full <event> wrapper
  // around a pre-built <detail> block
  std::string assembleCoT(const std::string& uid,
                           const std::string& detail,
                           const std::string& chatroom);

  std::string formatCoTTime(double moos_time, double offset = 0.0);
  std::string generateMsgId();

private:
  // --------------------------------------------------------
  // Own vehicle identity
  // --------------------------------------------------------
  std::string m_own_callsign;  // config: own_callsign = alpha
  std::string m_own_uid;       // config: own_uid = surveyor-alpha
                                // default: "surveyor-" + own_callsign

  // Own vehicle position (from NODE_REPORT) — included in
  // outbound CoT so ATAK shows where the message came from
  double m_nav_lat;
  double m_nav_lon;
  bool   m_nav_valid;

  // --------------------------------------------------------
  // Contact tracking table
  // key: callsign → ContactInfo (uid, callsign, last_seen)
  // Built from SA contact CoT seen in COT_INBOUND.
  // Also keyed by uid for reverse lookup.
  // --------------------------------------------------------
  std::map<std::string, ContactInfo> m_contacts_by_callsign;
  std::map<std::string, ContactInfo> m_contacts_by_uid;

  // --------------------------------------------------------
  // Known destination keyword sets
  // Used by resolveDestType() to classify the chatroom string
  // --------------------------------------------------------
  std::set<std::string> m_team_colors; // Cyan, Red, Blue, etc.
  std::set<std::string> m_roles;       // HQ, etc. (configurable)

  // --------------------------------------------------------
  // Group registry
  // Tracks named groups and their members.
  // Built from incoming group chat CoT (chatgrp/hierarchy).
  // key: group name → vector of member callsigns
  // --------------------------------------------------------
  std::map<std::string, std::vector<std::string>> m_groups;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------
  bool        m_echo_filter;    // filter own outbound echoes
  bool        m_debug;

  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_chat_in_received;
  unsigned int m_chat_in_published;
  unsigned int m_chat_out_sent;
  unsigned int m_contacts_tracked;
};

#endif // COT_CHAT_HEADER
