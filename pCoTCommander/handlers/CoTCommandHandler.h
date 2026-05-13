/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommandHandler.h                             */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Abstract base class for every pCoTCommander handler.    */
/*                                                          */
/*  This file is the contract -- when you write a new       */
/*  handler, the methods you override are the ones declared */
/*  here, and the dispatch flow described below is the      */
/*  flow that will reach your code.                         */
/*                                                          */
/*  =======================================================  */
/*  ARCHITECTURE                                            */
/*  =======================================================  */
/*  CoTCommander owns a registry of CoTCommandHandler       */
/*  instances:                                              */
/*                                                          */
/*    std::vector<std::unique_ptr<CoTCommandHandler>> m_handlers;  */
/*                                                          */
/*  Handlers are constructed at OnStartUp() by              */
/*  CommandHandlerFactory based on the .moos config         */
/*  (command_set = shore | vehicle | custom). The same      */
/*  handler class can appear in both bundles -- the         */
/*  shore-only / vehicle-only distinction is implemented    */
/*  by which factory bundle each handler is listed in,      */
/*  NOT by handlers checking fleet_mode internally.         */
/*                                                          */
/*  =======================================================  */
/*  DISPATCH FLOW                                           */
/*  =======================================================  */
/*                                                          */
/*  CoT side (per inbound event on COT_INBOUND):            */
/*    1. Dispatcher parses XML into a ParsedCoT.            */
/*    2. Dispatcher applies cross-cutting filters:          */
/*         own-echo (uid starts with "surveyor-")           */
/*         operator-UID filter (unless handler overrides    */
/*         bypassOperatorFilter()).                         */
/*    3. Dispatcher iterates m_handlers and calls           */
/*       claimsCoT(evt) on each. FIRST claim wins -- the    */
/*       dispatcher calls handleCoT() on that handler and   */
/*       stops. If no handler claims, the event is logged  */
/*       as unhandled and dropped.                          */
/*                                                          */
/*  Chat side (per inbound message on ATAK_CHAT_IN):        */
/*    1. Dispatcher parses callsign/chatroom/message.       */
/*    2. Chatroom is filtered against command_chatroom.     */
/*    3. Message is normalized (lowercase, trim).           */
/*    4. (Fleet mode) Vehicle-prefix resolution: if the     */
/*       first word is not in the keyword index, treat it   */
/*       as a vehicle name and re-resolve.                  */
/*    5. Dispatcher looks up first_word in m_chat_index     */
/*       (a std::map<keyword, CoTCommandHandler*> built     */
/*       once at startup from each handler's                */
/*       chatKeywords()).                                   */
/*    6. Matched handler's handleChat() is called with a    */
/*       fully resolved ChatMessage.                        */
/*    7. No match -> HelpHandler responds with an "unknown  */
/*       command" DM listing valid keywords.                */
/*                                                          */
/*  =======================================================  */
/*  KEYWORD INDEXING                                        */
/*  =======================================================  */
/*  At startup the dispatcher iterates each handler's       */
/*  chatKeywords() and inserts entries into m_chat_index.   */
/*  Duplicate keys (two handlers claim the same keyword)    */
/*  cause a FATAL startup error with a clear message -- we  */
/*  catch this here rather than letting silent mis-dispatch */
/*  ship to a deployed vehicle.                             */
/*                                                          */
/*  The keyword set is ALSO consulted by the vehicle-       */
/*  prefix resolver. If "blue_one" is not a keyword,        */
/*  but "atak" is, then "blue_one atak" parses as           */
/*  vehicle=blue_one, cmd=atak. This makes the keyword      */
/*  index a single source of truth for what counts as a    */
/*  command word.                                           */
/*                                                          */
/*  =======================================================  */
/*  ONE HANDLER, ONE RESPONSIBILITY                         */
/*  =======================================================  */
/*  Handlers stay small (~60-150 lines each), matching the  */
/*  thin-wrapper philosophy used throughout moos-ivp-tak.   */
/*                                                          */
/*  Group keywords on ONE handler when they share parsing   */
/*  and state:                                              */
/*    BehaviorToggleHandler covers {avoid, untag, retry,    */
/*    opreg} because all four follow the                    */
/*    "<keyword> on|off" shape and target ATAK_<NAME> vars. */
/*                                                          */
/*  Do NOT combine unrelated commands just to reduce file   */
/*  count -- the value of the refactor is that each handler */
/*  is independently readable.                              */
/*                                                          */
/*  =======================================================  */
/*  ADDING A NEW COMMAND                                    */
/*  =======================================================  */
/*    1. Create handlers/MyHandler.{h,cpp} inheriting       */
/*       this class.                                        */
/*    2. Override only the methods you need (CoT-only       */
/*       handlers ignore chat hooks and vice versa).        */
/*    3. Add a registration line in CommandHandlerFactory   */
/*       under the bundle(s) where it belongs.              */
/*    4. Add any new .moos config keys to your handler's    */
/*       configure() override.                              */
/*    5. Add a helpLine() so it shows up in "help" output.  */
/*                                                          */
/*  No changes required in CoTCommander itself.             */
/*                                                          */
/*  =======================================================  */
/*  LIFECYCLE                                               */
/*  =======================================================  */
/*    1. CommandHandlerFactory constructs the handler.      */
/*       Handler sets its defaults in its constructor.      */
/*    2. CoTCommander::OnStartUp() calls configure(key,val) */
/*       once per .moos ProcessConfig line; each handler    */
/*       consumes the keys it recognizes and ignores the    */
/*       rest. Unknown keys are NOT errors here -- multiple */
/*       handlers see the same lines.                       */
/*    3. CoTCommander calls registerSubs(); each handler    */
/*       appends MOOS variable names it wants to receive    */
/*       in OnNewMail. The dispatcher unions these with     */
/*       its common subscriptions (DEPLOY, ATAK_MODE etc.)  */
/*       and registers with the MOOSDB.                     */
/*    4. CoTCommander builds m_chat_index by calling        */
/*       chatKeywords() on each handler.                    */
/*    5. Runtime: OnNewMail dispatches to claimsCoT/        */
/*       handleCoT, m_chat_index lookup -> handleChat, or   */
/*       handler-specific subscriptions -> onMail.          */
/*    6. AppCast: dispatcher emits a "==== <name()> ===="   */
/*       section header and calls each handler's            */
/*       appcast() to append its status.                    */
/*                                                          */
/*  =======================================================  */
/*  CROSS-CUTTING POLICY                                    */
/*  =======================================================  */
/*  The operator-UID filter (m_operator_uid_filter on the   */
/*  dispatcher) is applied to all inbound CoT BEFORE        */
/*  handler dispatch.                                       */
/*                                                          */
/*  Handlers that legitimately accept events from non-      */
/*  operator sources (e.g. FlagPursuitHandler receiving     */
/*  flag-position broadcasts from pCoTGraphics, whose uid   */
/*  is "aquaticus-flag-red" not an ATAK device UID)         */
/*  override bypassOperatorFilter() to return true.         */
/*  Default is SECURE -- handlers must explicitly opt out.  */
/*                                                          */
/*  The chatroom filter (command_chatroom) is applied to    */
/*  all inbound chat and is NOT overridable -- there is no  */
/*  current use case for a handler that accepts chat from   */
/*  arbitrary chatrooms.                                    */
/************************************************************/

#ifndef MOOS_IVP_TAK_COT_COMMAND_HANDLER_HEADER
#define MOOS_IVP_TAK_COT_COMMAND_HANDLER_HEADER

#include <string>
#include <vector>

#include "ParsedCoT.h"
#include "ChatMessage.h"
#include "CommanderContext.h"

class CoTCommandHandler
{
public:
  virtual ~CoTCommandHandler() = default;

  // ========================================================
  // Identification
  // ========================================================

  // Short human-readable handler name, used in AppCast
  // section headers and debug log lines. Examples:
  // "Waypoint", "FlagPursuit", "AtakMode", "BehaviorToggle".
  // Must be unique across the registry (the dispatcher
  // does not currently enforce this, but duplicates make
  // AppCast confusing).
  virtual std::string name() const = 0;

  // ========================================================
  // CoT dispatch
  // ========================================================

  // Return true if this handler claims the given CoT event.
  // Typically checks event type (e.g.
  //   evt.type == "b-m-p-w-GOTO"
  // ) and any handler-specific predicate (e.g. uid match
  // for flag pursuit).
  //
  // MUST be fast and side-effect-free -- called for every
  // inbound CoT, in registry order, until a handler claims
  // the event. Do not DM the operator or modify state here.
  // Save those for handleCoT().
  //
  // Default: claims nothing. Override when handling CoT.
  virtual bool claimsCoT(const ParsedCoT& /*evt*/) const
  { return false; }

  // Process a claimed CoT event. Return true if the event
  // was actually handled (counts toward the dispatcher's
  // m_cot_handled counter), false if the handler rejected
  // it after closer inspection (still considered "handled"
  // for dispatch purposes -- the event won't be re-offered
  // to subsequent handlers).
  //
  // Handlers communicate rejection reasons to the operator
  // via ctx.dm(); the bool return value is for accounting
  // only.
  virtual bool handleCoT(const ParsedCoT& /*evt*/,
                          CommanderContext& /*ctx*/)
  { return false; }

  // Cross-cutting policy override. Return true to bypass
  // the operator-UID filter for THIS handler's CoT events.
  // Default (false) = handler is gated by the filter.
  //
  // Use sparingly. Legitimate cases:
  //   - FlagPursuitHandler accepting flag-position
  //     broadcasts from pCoTGraphics.
  //
  // Affects CoT only. Chat dispatch has no UID filter.
  virtual bool bypassOperatorFilter() const
  { return false; }

  // ========================================================
  // Chat dispatch
  // ========================================================

  // Return the chat keywords this handler claims as the
  // first word of a command. The dispatcher indexes these
  // into a map at startup. Conflicting claims (same keyword
  // from two handlers) fail startup with a clear error.
  //
  // Group related keywords on ONE handler when they share
  // parsing -- e.g. BehaviorToggleHandler returns
  //   {"avoid", "untag", "retry", "opreg"}
  // because all four follow the "<keyword> on|off" shape.
  //
  // Empty vector (default) = handler doesn't process chat.
  virtual std::vector<std::string> chatKeywords() const
  { return {}; }

  // Process a chat message whose first_word matched one of
  // this handler's keywords. The dispatcher has already:
  //   - normalized the message (lowercase, trim)
  //   - resolved the vehicle prefix (sfx, target_vehicle,
  //     target_label all populated)
  //   - looked up first_word in m_chat_index
  // ...so the handler just acts on the resolved ChatMessage
  // and DMs a confirmation or error via ctx.dm().
  //
  // Return value is currently informational; reserved for
  // a future per-handler success/failure metric.
  virtual bool handleChat(const ChatMessage& /*msg*/,
                           CommanderContext& /*ctx*/)
  { return false; }

  // One-line help text shown when an operator sends "help".
  // Empty string (default) = handler omitted from help
  // (e.g. internal-only handlers, or CoT-only handlers
  // that have nothing to advertise in chat).
  //
  // The HelpHandler iterates the registry and concatenates
  // non-empty helpLine() returns. This means each handler
  // is the single source of truth for its own help text --
  // it can never go out of sync with what the handler
  // actually accepts.
  //
  // Convention: start with the keyword(s), pad to a column,
  // then "--" and a short description. Examples:
  //   "deploy            -- start/resume vehicle motion"
  //   "avoid on|off      -- toggle collision avoidance"
  //
  // For fleet-only or vehicle-only handlers, the line
  // should describe the command as it appears in the
  // relevant mode. (The dispatcher selects the help bundle
  // by which factory built it -- a vehicle-mode help     */
  // request only sees handlers in the vehicle bundle.)
  virtual std::string helpLine() const
  { return ""; }

  // ========================================================
  // Lifecycle hooks
  // ========================================================

  // Called once per .moos ProcessConfig line during the
  // dispatcher's OnStartUp(). Handlers extract keys they
  // recognize and ignore the rest. The order of calls
  // matches the order of lines in the .moos file.
  //
  // Each handler is independently responsible for setting
  // sensible defaults in its constructor; configure() is
  // purely for overriding defaults. Handlers must not
  // throw -- log and ignore unparseable values instead.
  //
  // Example:
  //   WaypointHandler::configure("capture_radius", "12.5")
  //     -> sets m_capture_radius = 12.5.
  virtual void configure(const std::string& /*key*/,
                          const std::string& /*value*/)
  {}

  // Called once after configure() to collect MOOS variables
  // this handler wants to receive in OnNewMail. The
  // dispatcher aggregates subs across all handlers and
  // calls Register() for the union, with its own common
  // subscriptions added.
  //
  // Handlers receive matched mail through onMail() -- they
  // do not have direct access to the MOOSMSG_LIST.
  //
  // Most handlers don't need this. The dispatcher already
  // subscribes to and mirrors the common state vars
  // (DEPLOY, ATAK_MODE, TAGGED, ATAK_RETRY); read those
  // from CommanderContext instead. Use registerSubs() only
  // when a handler needs a variable outside the common set
  // (e.g. FlagPursuitHandler subscribing to per-team
  // HAS_FLAG_* variables enumerated in its own config).
  virtual void registerSubs(std::vector<std::string>& /*subs*/)
  {}

  // Called for each mail item whose key was registered by
  // this handler via registerSubs(). Common state mirroring
  // (DEPLOY etc.) is already done by the dispatcher and
  // exposed through CommanderContext -- handlers receive
  // ONLY their handler-specific subscriptions here.
  //
  // The context is provided so handlers can react to mail
  // by publishing, DMing the operator, or reading mirrored
  // state. Example: WaypointHandler watches
  // ATAK_WPT_REACHED and DMs "Waypoint reached." to the
  // operator who sent the original waypoint.
  //
  // If multiple handlers register the same variable, each
  // one's onMail() is called.
  virtual void onMail(const std::string& /*key*/,
                       const std::string& /*value*/,
                       CommanderContext& /*ctx*/)
  {}

  // Append handler-specific status to the AppCast report.
  // The dispatcher emits a "==== <name()> ====" section
  // header before calling this, so the handler writes only
  // the body. Keep it brief -- 2-6 lines is typical.
  // Show counters, last action, and any noteworthy state
  // that helps a human diagnose what the handler is doing.
  virtual void appcast(std::string& /*report*/) const
  {}
};

#endif // MOOS_IVP_TAK_COT_COMMAND_HANDLER_HEADER
