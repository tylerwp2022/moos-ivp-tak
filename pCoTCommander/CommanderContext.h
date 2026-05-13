/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CommanderContext.h                              */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Dependency-injection bundle for pCoTCommander handlers. */
/*                                                          */
/*  WHY A CONTEXT OBJECT INSTEAD OF A CoTCommander*         */
/*  -------------------------------------------------------- */
/*  Handlers shouldn't see the whole dispatcher class.      */
/*  Keeping the surface small:                              */
/*    - Makes handlers easier to reason about. The contract */
/*      is one struct, not a class with 30 methods and      */
/*      private state.                                      */
/*    - Lets us unit-test handlers with a stub context      */
/*      (no MOOS runtime needed).                           */
/*    - Prevents handlers from reaching into dispatcher     */
/*      internals or other handlers' state.                 */
/*                                                          */
/*  The context carries exactly the services a handler may  */
/*  need: publish MOOS variables, DM the operator, log,     */
/*  query geodesy, query mirrored runtime state.            */
/*                                                          */
/*  Service callbacks (std::function) are bound in          */
/*  CoTCommander::OnStartUp() to lambdas that capture the   */
/*  dispatcher. The dispatcher owns the context and passes  */
/*  it by reference to every handler invocation.            */
/*                                                          */
/*  THREADING                                               */
/*  -------------------------------------------------------- */
/*  MOOS apps are single-threaded by default. Handlers may  */
/*  safely call publish(), dm(), and dlog() without         */
/*  locking, and may read mirrored state fields directly.   */
/*  Mirrored state is written ONLY by the dispatcher's      */
/*  OnNewMail(); handlers must NEVER modify these fields.   */
/*                                                          */
/*  MIRRORED STATE SEMANTICS                                */
/*  -------------------------------------------------------- */
/*  These fields hold the dispatcher's latest view of       */
/*  MOOSDB values. They are mirrored so handlers don't each */
/*  need to subscribe to the same common variables.         */
/*                                                          */
/*    deployed     -- DEPLOY      both modes                */
/*    atak_mode    -- ATAK_MODE   vehicle mode only         */
/*    tagged       -- TAGGED      vehicle mode only         */
/*    atak_retry   -- ATAK_RETRY  vehicle mode only         */
/*                                                          */
/*  In fleet mode the dispatcher does not subscribe to      */
/*  per-vehicle state (would need 3+ flavors and isn't      */
/*  meaningful in a fleet command context), so atak_mode /  */
/*  tagged / atak_retry hold their defaults. Handlers that  */
/*  depend on them should gate their behavior on            */
/*  fleet_mode == false.                                    */
/*                                                          */
/*  EXTENSION                                               */
/*  -------------------------------------------------------- */
/*  Adding a new shared service:                            */
/*    1. Add the field here (std::function or POD).         */
/*    2. Bind it in CoTCommander::OnStartUp().              */
/*    3. Document its threading and lifetime semantics in   */
/*       the comment block above its declaration.           */
/*  Don't add narrow-use services -- if only one handler    */
/*  needs it, put it on that handler instead.               */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMANDER_CONTEXT_HEADER
#define MOOS_IVP_TAK_COMMANDER_CONTEXT_HEADER

#include <string>
#include <functional>

// Forward declaration. Users that actually call geodesy
// methods (e.g. WaypointHandler) include CoTGeodesy.h
// themselves. Keeping it forward-declared here avoids
// dragging the geodesy header into every handler that
// only needs to publish or DM.
class CoTGeodesy;

struct CommanderContext
{
  // ========================================================
  // Publication callbacks
  // ========================================================

  // Publish a MOOS variable on the local MOOSDB.
  // Wraps CoTCommander::Notify(). Handlers compose the full
  // variable name including any fleet-mode suffix from
  // ChatMessage::sfx:
  //   ctx.publish("DEPLOY" + msg.sfx, "true");
  std::function<void(const std::string& key,
                      const std::string& value)>     publish;

  // Send an ATAK DM (GeoChat reply) to the operator.
  // Formats ATAK_CHAT_OUT internally as
  //   "message=<msg>|chatroom=<reply_to>"
  // (note: '|' delimiter, because message text can contain
  // commas -- see pCoTChat). Handlers just pass the human-
  // readable message and the reply_to chatroom from
  // ChatMessage::reply_to.
  std::function<void(const std::string& msg,
                      const std::string& reply_to)>   dm;

  // Append a debug log line. Routed to the dispatcher's
  // circular buffer and shown in AppCast under debug mode.
  // Free to call frequently -- the buffer caps growth at
  // CoTCommander::DEBUG_BUF_SIZE.
  std::function<void(const std::string& msg)>        dlog;

  // Returns the helpLine() of every active handler in
  // registry order. Empty strings are filtered out by the
  // caller (HelpHandler). Bound in OnStartUp; reading it
  // is safe at any time after handler registration.
  //
  // Only HelpHandler should call this. Having it on the
  // context (rather than passing the registry directly)
  // keeps the contract narrow: handlers see a function,
  // not a vector of pointers they could mishandle.
  std::function<std::vector<std::string>()>          help_lines;

  // ========================================================
  // Geodesy
  // ========================================================

  // LatLon <-> local XY converter. Initialized when the
  // first NODE_REPORT arrives carrying a LatOrigin /
  // LonOrigin pair, then held fixed for the run.
  //
  // Handlers needing position conversion (e.g. Waypoint and
  // FlagPursuit) MUST check geodesy_ready before calling
  // geodesy methods. The dispatcher does NOT short-circuit
  // dispatch on !geodesy_ready -- some handlers don't need
  // geodesy at all, and gating in the dispatcher would
  // block them needlessly.
  //
  // Lifetime: the underlying CoTGeodesy lives on the
  // dispatcher and outlives all handlers. Pointer is
  // stable for the run.
  CoTGeodesy* geodesy{nullptr};
  bool        geodesy_ready{false};

  // ========================================================
  // Mode flag
  // ========================================================

  // True when running on the shoreside MOOSDB (handler
  // publications go to *_ALL variables, routed to vehicles
  // by uFldShoreBroker). False when running on a vehicle
  // MOOSDB (handler publications go to bare variable names
  // on that vehicle's DB).
  //
  // Set once in CoTCommander::OnStartUp() from the .moos
  // ProcessConfig and never mutated thereafter. Handlers
  // can cache it locally if they want.
  bool fleet_mode{false};

  // The configured command_chatroom string. Fleet mode:
  // shore callsign ("AQUATICUS-SHORE"). Vehicle mode:
  // vehicle callsign ($(VNAME)). Set once in OnStartUp.
  // Used by handlers that need a fallback DM destination
  // when there's no operator callsign to reply to (e.g.
  // FlagPursuitHandler when no human triggered the pursuit).
  std::string command_chatroom;

  // ========================================================
  // Mirrored runtime state (handler-readable, dispatcher-writes)
  // ========================================================

  // DEPLOY -- vehicle deployment state. Both modes.
  // Handlers (e.g. WaypointHandler) gate destructive
  // commands on this -- a waypoint when !deployed is
  // rejected with an operator DM rather than silently
  // queued.
  bool deployed{false};

  // ATAK_MODE -- vehicle mode only. True when the
  // operator's ATAK supervision is active and game
  // behaviors are suppressed. Handlers reading this
  // should also check fleet_mode == false (fleet
  // dispatcher does not subscribe to per-vehicle mode).
  bool atak_mode{false};

  // TAGGED -- vehicle mode only. True when the vehicle has
  // been tagged in the Aquaticus game and must return to
  // base. Used by AtakModeHandler's retry-off logic.
  bool tagged{false};

  // ATAK_RETRY -- vehicle mode only. True (default) =
  // automatically reactivate the ATAK waypoint after a
  // tag-recovery cycle; false = clear the waypoint and
  // wait for the operator to resend.
  bool atak_retry{true};

  // ========================================================
  // Shared scratchpad (handler-writable)
  // ========================================================

  // The callsign of the most recent operator who sent a
  // waypoint CoT. Set by WaypointHandler when it parses
  // parent_callsign out of the incoming CoT. Read by
  // FlagPursuitHandler to decide where to DM the pursuit
  // announcement (so it goes to the operator who's been
  // driving the vehicle, not to "All Chat Rooms").
  //
  // This is the only field handlers are allowed to write
  // to. Document any future shared-scratchpad fields here
  // explicitly -- the default is read-only.
  std::string last_operator_callsign;
};

#endif // MOOS_IVP_TAK_COMMANDER_CONTEXT_HEADER
