/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander.h                                  */
/*    DATE: April 2026                                      */
/*    REV:  May 13, 2026 -- handler-class refactor          */
/*                                                          */
/*  pCoTCommander -- inbound CoT command dispatcher.        */
/*                                                          */
/*  This is the slim dispatcher core. Per-command logic     */
/*  lives in handlers/<Name>Handler.{h,cpp}, each of which  */
/*  is a small class inheriting CoTCommandHandler. The      */
/*  dispatcher owns no command-specific state; it only      */
/*  routes events to handlers and mirrors common runtime    */
/*  state into a CommanderContext that handlers can read.   */
/*                                                          */
/*  See handlers/CoTCommandHandler.h for the handler        */
/*  contract, dispatch flow details, and the lifecycle      */
/*  hooks available to handlers.                            */
/*                                                          */
/*  =======================================================  */
/*  DEPLOYMENT MODES                                        */
/*  =======================================================  */
/*  fleet_mode = true   -- runs on the shoreside MOOSDB,    */
/*                          posts *_ALL variables.          */
/*  fleet_mode = false  -- runs on a vehicle MOOSDB, posts  */
/*                          bare variable names.            */
/*                                                          */
/*  The mode is exposed through CommanderContext::          */
/*  fleet_mode -- handlers consult it when their behavior   */
/*  differs across modes. Mode-exclusive commands (e.g.     */
/*  play/stop which are shore-only) are realized by         */
/*  excluding the handler from the wrong bundle in          */
/*  CommandHandlerFactory, not by per-handler fleet_mode    */
/*  checks.                                                 */
/*                                                          */
/*  =======================================================  */
/*  DISPATCH FLOW                                           */
/*  =======================================================  */
/*  OnNewMail():                                            */
/*    COT_INBOUND      -> parseCoT into ParsedCoT           */
/*                        -> apply cross-cutting filters    */
/*                           (own-echo, operator UID)       */
/*                        -> iterate m_handlers calling     */
/*                           claimsCoT(); first claim wins  */
/*                        -> handleCoT() on matched handler */
/*                                                          */
/*    ATAK_CHAT_IN     -> parse callsign/chatroom/message   */
/*                        -> chatroom filter                */
/*                        -> normalize cmd, resolve vehicle */
/*                           prefix                         */
/*                        -> m_chat_index lookup            */
/*                        -> handleChat() on matched        */
/*                           handler                        */
/*                                                          */
/*    NODE_REPORT(*)   -> initialize geodesy on first one,  */
/*                        update m_ctx.geodesy_ready flag.  */
/*                                                          */
/*    DEPLOY, ATAK_MODE, TAGGED, ATAK_RETRY                 */
/*                     -> mirror into m_ctx.                */
/*                                                          */
/*    (any other key registered by a handler)               */
/*                     -> fan out to handler::onMail().     */
/*                                                          */
/*  =======================================================  */
/*  STARTUP                                                 */
/*  =======================================================  */
/*  OnStartUp():                                            */
/*    1. Read .moos ProcessConfig block; cache lines for    */
/*       handler distribution.                              */
/*    2. CommandHandlerFactory::build(command_set, mission, */
/*       fleet_mode, custom_handlers) returns the handler   */
/*       bundle, which is moved into m_handlers.            */
/*    3. For each handler, replay configure(key,value)      */
/*       over every cached line. Handlers consume what they */
/*       recognize.                                         */
/*    4. Build m_chat_index from each handler's             */
/*       chatKeywords(). Fail fast with a clear error on    */
/*       duplicate keys.                                    */
/*    5. Aggregate registerSubs() across handlers; union    */
/*       with the dispatcher's common subscriptions; call   */
/*       Register() once.                                   */
/*    6. Bind m_ctx callbacks (publish/dm/dlog lambdas      */
/*       capturing this) and pointers.                      */
/*                                                          */
/*  =======================================================  */
/*  MOOS INTERFACE (unchanged from pre-refactor)            */
/*  =======================================================  */
/*    Subscribes (common, dispatcher-managed):              */
/*      COT_INBOUND, ATAK_CHAT_IN, NODE_REPORT,             */
/*      NODE_REPORT_LOCAL, DEPLOY, ATAK_MODE, TAGGED,       */
/*      ATAK_RETRY, ATAK_WPT_REACHED                        */
/*    Subscribes (per-handler):                             */
/*      whatever each handler returns from registerSubs()   */
/*                                                          */
/*    Publishes: Everything the active handlers publish --  */
/*      see each handler's header for its outputs. Common   */
/*      to all configurations: ATAK_CHAT_OUT (operator      */
/*      DMs). The dispatcher itself publishes nothing.      */
/************************************************************/

#ifndef MOOS_IVP_TAK_COT_COMMANDER_HEADER
#define MOOS_IVP_TAK_COT_COMMANDER_HEADER

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

#include "CoTGeodesy.h"
#include "CommanderContext.h"
#include "ParsedCoT.h"
#include "ChatMessage.h"
#include "handlers/CoTCommandHandler.h"

class CoTCommander : public AppCastingMOOSApp
{
public:
  CoTCommander();
  virtual ~CoTCommander() {}

  // ========================================================
  // MOOS lifecycle
  // ========================================================
  bool OnNewMail(MOOSMSG_LIST& NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  // ========================================================
  // Startup helpers
  // ========================================================

  // Parse the .moos ProcessConfig block:
  //   - Capture dispatcher-level config (fleet_mode,
  //     command_chatroom, operator_uid_filter, command_set,
  //     debug, enable_chat_commands).
  //   - Fan every line out to each handler's configure().
  // The dispatcher-level config and the configure() fan-
  // out are both performed inline in OnStartUp().

  // Wire the m_ctx callbacks (publish, dm, dlog, help_lines)
  // and copy mode/chatroom/geodesy pointer into m_ctx. Called
  // once during OnStartUp() after handlers are built.
  void bindContext();

  // Build the chat keyword index. Iterates m_handlers,
  // pulls each handler's chatKeywords(), and inserts
  // (keyword -> handler*) entries. Returns false and emits
  // a clear error message on duplicate keyword claims --
  // we fail at startup rather than letting silent mis-
  // dispatch ship.
  bool buildChatIndex();

  // Aggregate handler::registerSubs() across the registry,
  // union with the dispatcher's common subscription set,
  // and call MOOS Register() for the result.
  void registerVariables();

  // ========================================================
  // Mail dispatch
  // ========================================================

  // Parse a COT_INBOUND XML payload into a ParsedCoT,
  // apply cross-cutting filters (own-echo suppression,
  // operator-UID filter respecting bypass overrides),
  // then offer the event to handlers via
  // claimsCoT()/handleCoT(). Updates m_cot_received /
  // m_cot_handled / m_cot_ignored counters.
  void dispatchInboundCoT(const std::string& xml);

  // Parse an ATAK_CHAT_IN payload, verify chatroom against
  // m_command_chatroom, normalize and trim, resolve the
  // vehicle prefix (fleet mode only), look up the resulting
  // first_word in m_chat_index, and call handleChat() on
  // the matched handler. Unknown keywords get a DM listing
  // valid commands.
  void dispatchChatCommand(const std::string& moos_val);

  // Initialize geodesy LatOrigin/LonOrigin from the first
  // NODE_REPORT seen. Subsequent reports are ignored -- the
  // origin is fixed for the run. Updates m_ctx.geodesy and
  // m_ctx.geodesy_ready on success.
  void updateGeodesy(const std::string& node_report);

  // ========================================================
  // Chat helpers
  // ========================================================
  //
  // Vehicle-prefix resolution ("blue_one attack" -> attack
  // targeted at blue_one) is performed inline inside
  // dispatchChatCommand(). It uses m_chat_index to decide
  // whether the first word is a known keyword (-> use as
  // command) or unknown (-> treat as a vehicle name in
  // fleet mode, fall through to unknown-command DM in
  // vehicle mode).

  // ========================================================
  // Misc
  // ========================================================

  // Push a line into the debug circular buffer. Visible in
  // AppCast when m_debug == true. Bound into m_ctx.dlog
  // for handler use.
  void debugLog(const std::string& msg);

private:
  // ========================================================
  // Handler registry
  // ========================================================

  // Owned handlers. Order is the order in which
  // CommandHandlerFactory added them; CoT dispatch
  // iterates in this order (first claimsCoT() true wins).
  std::vector<std::unique_ptr<CoTCommandHandler>> m_handlers;

  // Keyword -> handler* lookup index for chat dispatch.
  // Built once at OnStartUp from each handler's
  // chatKeywords(). Pointers are NON-OWNING -- m_handlers
  // owns the lifetime, this map is just a lookup view.
  std::map<std::string, CoTCommandHandler*> m_chat_index;

  // ========================================================
  // Shared services exposed to handlers via DI
  // ========================================================

  // The dispatcher-owned LatLon<->XY converter. m_ctx.geodesy
  // points to this once initialized.
  CoTGeodesy m_geodesy;
  bool       m_geodesy_initialized;

  // DI bundle passed to handlers by reference. Lambdas
  // capturing 'this' are bound to m_ctx.publish/dm/dlog
  // in OnStartUp. Mirrored state (deployed, atak_mode,
  // tagged, atak_retry) is updated by the dispatcher's
  // OnNewMail and read by handlers.
  CommanderContext m_ctx;

  // ========================================================
  // Dispatcher-level config (.moos ProcessConfig)
  // ========================================================

  // Which bundle of handlers to instantiate. Recognized
  // values: "shore", "vehicle", "custom". If unset,
  // defaults to "shore" when fleet_mode = true, else
  // "vehicle". Custom mode lets the operator enumerate
  // handlers explicitly via enable_handler = <name> lines
  // (handled by CommandHandlerFactory).
  std::string m_command_set;

  // Which mission's handler set to use. Recognized values:
  // "aquaticus" (default), "hvt" (future). Affects which
  // mission-specific handlers appear in the bundle, and
  // which concrete class is built for mission-overloaded
  // names like "attack" (e.g. aquaticus::AttackHandler vs.
  // hvt::AttackHandler).
  std::string m_mission;

  // True on shoreside MOOSDB, false on a vehicle MOOSDB.
  // Mirrored into m_ctx.fleet_mode for handlers.
  bool m_fleet_mode;

  // Only process chat messages whose chatroom field equals
  // this string. Fleet mode: shore callsign (e.g.
  // "AQUATICUS-SHORE"). Vehicle mode: vehicle callsign
  // ($(VNAME)).
  std::string m_command_chatroom;

  // Optional substring filter on CoT event uid. Empty
  // string disables the filter. Applied as a cross-cutting
  // check in dispatchInboundCoT() before any handler sees
  // the event, EXCEPT for handlers that override
  // bypassOperatorFilter() to return true.
  std::string m_operator_uid_filter;

  // Master switch for chat command processing. When false,
  // ATAK_CHAT_IN is still received but immediately dropped.
  // Equivalent to building a bundle with no chat handlers,
  // but cheaper to toggle in .moos for debugging.
  bool m_enable_chat_commands;

  // Master debug switch. When true, debugLog() entries are
  // exposed in AppCast and handlers' dlog() calls become
  // visible. When false, dlog() still appends to the
  // circular buffer (cheap) but the buffer is not rendered.
  bool m_debug;

  // ========================================================
  // Debug circular buffer
  // ========================================================

  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // ========================================================
  // Aggregate diagnostics
  // ========================================================

  unsigned int m_cot_received;
  unsigned int m_cot_handled;   // a handler claimed and processed
  unsigned int m_cot_ignored;   // no handler claimed, or filter dropped

  unsigned int m_chat_received;
  unsigned int m_chat_handled;  // keyword matched a handler
  unsigned int m_chat_unknown;  // no handler keyword matched

  // Last successful dispatch summary for AppCast.
  // Format: "<handler name>: <short description>"
  // Updated after each successful CoT or chat dispatch.
  std::string m_last_dispatch;
};

#endif // MOOS_IVP_TAK_COT_COMMANDER_HEADER
