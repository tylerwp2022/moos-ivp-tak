/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StatusHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*    REV:  May 14, 2026 -- expanded to report task & state */
/*    REV:  May 14, 2026 -- shore-relay reply path + fleet  */
/*    REV:  May 14, 2026 -- per-vehicle _STATE cache; no    */
/*                            more request/reply ping-pong  */
/*                                                          */
/*  Handles the "status" / "status all" chat commands.      */
/*                                                          */
/*  =======================================================  */
/*  DESIGN                                                  */
/*  =======================================================  */
/*  Shore maintains a per-vehicle state cache populated by   */
/*  two parallel sources, both of which write to the SAME    */
/*  suffixed MOOS variables (e.g. ATAK_MODE_STATE_BLUE_ONE): */
/*                                                          */
/*    Source A: Shore-mode handlers, on chat-driven         */
/*              commands -- write ATAK_*_STATE + msg.sfx    */
/*              locally on shore.                           */
/*                                                          */
/*    Source B: Vehicle-mode handlers (Waypoint,            */
/*              FlagPursuit) write bare ATAK_*_STATE on the */
/*              vehicle MOOSDB; uFldNodeBroker bridges them */
/*              to shore as ATAK_*_STATE_<VNAME_UPPER>.     */
/*                                                          */
/*  Last writer wins -- this matches operational intent.    */
/*  Most-recent action (shore chat or vehicle-side CoT) is  */
/*  the current state.                                      */
/*                                                          */
/*  Required infrastructure (see plug docs for details):    */
/*    meta_shoreside.moos: qbridge for all ATAK_* command   */
/*      vars (shore->vehicle).                              */
/*    meta_surveyor.moos:  uFldNodeBroker bridges for       */
/*      ATAK_MODE_STATE, ATAK_WAYPT_ACTIVE_STATE,           */
/*      ATAK_FLAG_PURSUIT_STATE (vehicle->shore).           */
/*    plug_pCoTCommander_shore.moos: status_vehicles =      */
/*      $(VNAMES) (drives which suffixed vars to subscribe).*/
/*                                                          */
/*  =======================================================  */
/*  DISPATCH PATHS                                          */
/*  =======================================================  */
/*                                                          */
/*  P1. VEHICLE + chat "status":                            */
/*      Build status from ctx + locally-subscribed bare     */
/*      vars (ACTION, HAS_FLAG, etc.). DM the operator.     */
/*                                                          */
/*  P2. SHORE + chat "status" (no vehicle prefix):          */
/*      DM the game state plus a usage hint.                */
/*                                                          */
/*  P3. SHORE + chat "<vehicle> status":                    */
/*      Look up that vehicle in the cache, build status     */
/*      from cached state, DM the operator. Reply lands in  */
/*      the shore chat thread (since shore is the sender).  */
/*                                                          */
/*  P4. SHORE + chat "status all":                          */
/*      Iterate cached vehicles, build one status per       */
/*      vehicle, DM the operator. All replies land in the   */
/*      shore chat thread.                                  */
/*                                                          */
/*  =======================================================  */
/*  TASK DERIVATION (priority order, first match wins)      */
/*  =======================================================  */
/*    !deployed         -> "Idle (not deployed)"            */
/*    tagged && !untag  -> "Tagged, holding (untag off)"    */
/*    tagged            -> "Tagged, recovering"             */
/*    atak && pursuit   -> "Pursuing flag (ATAK auto)"      */
/*    atak && waypt     -> "Navigating to operator waypt"   */
/*    atak              -> "Awaiting operator command"      */
/*    action=ATTACK_*   -> "Attacking flag"                 */
/*    action=DEFEND_*   -> "Defending zone"                 */
/*    else              -> "Autonomous (no role assigned)"  */
/*                                                          */
/*  =======================================================  */
/*  MOOS SUBSCRIPTIONS                                      */
/*  =======================================================  */
/*  VEHICLE mode (bare names, local to this vehicle):       */
/*    ACTION, ATAK_WAYPT_ACTIVE, ATAK_FLAG_PURSUIT,         */
/*    ATAK_AUTO_UNTAG, HAS_FLAG, AQUATICUS_GAME             */
/*                                                          */
/*  SHORE mode (per-vehicle suffixed names):                */
/*    For each V in status_vehicles, uppercase to U:        */
/*      DEPLOY_<U>                                          */
/*      ATAK_MODE_STATE_<U>                                 */
/*      ATAK_WAYPT_ACTIVE_STATE_<U>                         */
/*      ATAK_FLAG_PURSUIT_STATE_<U>                         */
/*      TAGGED_<U>                                          */
/*      HAS_FLAG_<U>                                        */
/*      ATAK_AUTO_UNTAG_<U>                                 */
/*      ACTION_<U>                                          */
/*    Plus: AQUATICUS_GAME (bare on shore).                 */
/*                                                          */
/*  Mode-aware subscription means each instance subscribes  */
/*  to only what it can read. configure() reads             */
/*  fleet_mode from the dispatcher's config, registerSubs() */
/*  branches on it.                                         */
/*                                                          */
/*  =======================================================  */
/*  CONFIG KEYS                                             */
/*  =======================================================  */
/*    status_vehicles = blue_one:blue_two:red_one:red_two   */
/*       Colon-separated list of vehicles for shore mode.   */
/*       Typically passed via nsplug as $(VNAMES). Ignored  */
/*       in vehicle mode.                                   */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    status            -- individual (vehicle, or shore    */
/*                          with "<vehicle> status" prefix) */
/*    status all        -- fleet rollcall (shore only)      */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER

#include <map>
#include <vector>

#include "../CoTCommandHandler.h"

namespace common {

class StatusHandler : public CoTCommandHandler
{
public:
  StatusHandler();
  ~StatusHandler() override = default;

  std::string name() const override { return "Status"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void configure(const std::string& key,
                  const std::string& value) override;
  void registerSubs(std::vector<std::string>& subs) override;
  void onMail(const std::string& key,
               const std::string& value,
               CommanderContext& ctx) override;

  void appcast(std::string& report) const override;

private:
  // ---- Per-vehicle cache (shore mode) ----
  struct VehicleState {
    std::string vname_label;   // lowercase display form: "blue_one"
    std::string vname_upper;   // suffix form:            "BLUE_ONE"
    bool deployed         = false;
    bool atak_mode        = false;
    bool atak_waypt_active = false;
    bool atak_flag_pursuit = false;
    bool tagged           = false;
    bool has_flag         = false;
    bool atak_auto_untag  = true;
    std::string action;
  };

  // Keyed by uppercase vname so onMail's suffix-strip lookup
  // is O(log n) without case conversion.
  std::map<std::string, VehicleState> m_vehicles;

  // Ordered list of upper-vnames in the configured order,
  // so "status all" iterates predictably.
  std::vector<std::string> m_vehicle_order;

  // ---- Mode-and-config tracking ----
  bool m_fleet_mode_resolved;   // set in configure() when we
                                 // see the dispatcher's
                                 // fleet_mode line; we need
                                 // this before registerSubs
                                 // runs to know which sub set
                                 // to register. If unset (no
                                 // fleet_mode in config),
                                 // default vehicle-mode subs.
  bool m_fleet_mode;

  // ---- Builders ----
  std::string buildVehicleStatusFromCtx(const CommanderContext& ctx) const;
  std::string buildVehicleStatusFromCache(const VehicleState& v) const;
  std::string buildVehicleStatusOneLine(const VehicleState& v) const;
  std::string buildShoreStatus() const;
  std::string deriveTask(bool deployed,
                          bool tagged,
                          bool atak_mode,
                          bool atak_waypt_active,
                          bool atak_flag_pursuit,
                          bool atak_auto_untag,
                          const std::string& action) const;
  std::string deriveMode(bool deployed, bool atak_mode) const;

  // Look up a vehicle by various keys. Helpers tolerate
  // mixed case input ("BLUE_ONE" or "blue_one"); the cache
  // is keyed on uppercase.
  VehicleState* findVehicle(const std::string& key_any_case);
  const VehicleState* findVehicle(const std::string& key_any_case) const;

  // ---- Vehicle-mode local state (used in vehicle mode only) ----
  std::string m_action;
  bool        m_atak_waypt_active;
  bool        m_atak_flag_pursuit;
  bool        m_atak_auto_untag;
  bool        m_has_flag;
  std::string m_aquaticus_game;

  // ---- Diagnostics ----
  unsigned int m_chat_queries;
  unsigned int m_chat_fleet;
  unsigned int m_cache_updates;
  std::string  m_last_action;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_STATUS_HANDLER_HEADER
