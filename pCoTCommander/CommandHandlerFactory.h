/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CommandHandlerFactory.h                         */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Factory for assembling pCoTCommander handler bundles.   */
/*                                                          */
/*  Called once by CoTCommander::OnStartUp() to construct   */
/*  the set of handlers that will service this run. Bundle  */
/*  selection is driven by two axes in the .moos config:    */
/*                                                          */
/*    command_set = shore     // role: where this app runs  */
/*    command_set = vehicle                                 */
/*    command_set = custom                                  */
/*    mission     = aquaticus // which game / op the boats  */
/*    mission     = hvt       //   are running              */
/*    enable_handler = waypoint                             */
/*    enable_handler = atak_mode                            */
/*    ...                                                   */
/*                                                          */
/*  If command_set is unset, fleet_mode determines the      */
/*  default: shoreside dispatchers get "shore", vehicle     */
/*  dispatchers get "vehicle".                              */
/*                                                          */
/*  If mission is unset, defaults to "aquaticus" (the only  */
/*  mission supported at the time of writing -- HVT and     */
/*  others will register their own).                        */
/*                                                          */
/*  =======================================================  */
/*  TWO AXES                                                */
/*  =======================================================  */
/*  Role and mission are orthogonal:                        */
/*                                                          */
/*                  +-----------+-----------+               */
/*                  |   shore   |  vehicle  |               */
/*    +-------------+-----------+-----------+               */
/*    | aquaticus   |  bundle   |  bundle   |               */
/*    +-------------+-----------+-----------+               */
/*    | hvt         |  bundle   |  bundle   |               */
/*    +-------------+-----------+-----------+               */
/*                                                          */
/*  Each cell is a distinct bundle composition. The         */
/*  factory's job is to assemble the right cell from the    */
/*  handler classes registered in handlers/<scope>/.        */
/*                                                          */
/*  =======================================================  */
/*  HANDLER ORGANIZATION                                    */
/*  =======================================================  */
/*  Handler classes live in three subfolders:               */
/*                                                          */
/*    handlers/common/    -- mission-agnostic handlers      */
/*                           (Deploy, Return, Status, etc.) */
/*    handlers/aquaticus/ -- Aquaticus-specific handlers    */
/*                           (Attack, Defend, Play, Stop,   */
/*                            Waypoint, FlagPursuit)        */
/*    handlers/hvt/       -- HVT-specific handlers (future) */
/*                                                          */
/*  Each subfolder is its own C++ namespace (common::,      */
/*  aquaticus::, hvt::) so that, e.g., aquaticus::          */
/*  AttackHandler and hvt::AttackHandler are distinct       */
/*  classes claiming the same chat keyword "attack" but     */
/*  publishing different MOOS variables.                    */
/*                                                          */
/*  COMMON DOES NOT MEAN AUTOMATICALLY INCLUDED. The        */
/*  "common" folder is a code-organization concept: code    */
/*  that has no mission dependencies. Whether a common      */
/*  handler appears in a given mission's bundle is decided  */
/*  by that mission's bundle definition in the .cpp.        */
/*  (Example: UntagHandler is mission-agnostic CODE -- it   */
/*  just publishes ATAK_AUTO_UNTAG -- but the CONCEPT of    */
/*  tagging is Aquaticus-specific, so it goes in the        */
/*  Aquaticus bundle and not in the HVT bundle.)            */
/*                                                          */
/*  =======================================================  */
/*  BUNDLES                                                 */
/*  =======================================================  */
/*  Bundles are defined as fixed lists of handler names in  */
/*  CommandHandlerFactory.cpp -- NOT in this header. To see */
/*  the current (role, mission) compositions, open the .cpp.*/
/*                                                          */
/*  Why hard-coded instead of self-registration?            */
/*    - One file to read to know what's active in each      */
/*      mode. No static-init-order surprises.               */
/*    - Bundle membership is a deliberate decision, not a   */
/*      side effect of linking in a translation unit.       */
/*    - Adding a handler to a bundle is a code change       */
/*      visible in diff review, not a hidden link-time      */
/*      behavior.                                           */
/*                                                          */
/*  =======================================================  */
/*  ADDING A NEW HANDLER                                    */
/*  =======================================================  */
/*  See handlers/CoTCommandHandler.h for the full           */
/*  workflow. The CommandHandlerFactory.cpp side is:        */
/*    1. Include the new handler's header.                  */
/*    2. Add an entry in buildOne(). Common handlers map a  */
/*       name directly to one class; mission-specific       */
/*       handlers dispatch on the mission argument:         */
/*         if(name == "deploy") return                      */
/*           std::make_unique<common::DeployHandler>();     */
/*         if(name == "attack") {                           */
/*           if(mission == "aquaticus") return              */
/*             std::make_unique<aquaticus::AttackHandler>();*/
/*           if(mission == "hvt") return                    */
/*             std::make_unique<hvt::AttackHandler>();      */
/*         }                                                */
/*    3. Add the handler name to whichever bundle list(s)   */
/*       it should appear in. There is one bundle list per  */
/*       (role, mission) cell -- e.g.                       */
/*       buildAquaticusVehicleBundle().                     */
/*  No changes required in this header.                     */
/*                                                          */
/*  =======================================================  */
/*  ERROR HANDLING                                          */
/*  =======================================================  */
/*  build() returns a BuildResult struct. On success,       */
/*  ok=true and handlers is moved-from into the caller.     */
/*  On failure, ok=false and error contains a human-        */
/*  readable explanation suitable for MOOSTrace().          */
/*                                                          */
/*  Failure modes:                                          */
/*    - Unknown command_set name                            */
/*    - Unknown mission name                                */
/*    - command_set=custom with empty custom_handlers list  */
/*    - command_set=custom referencing an unknown handler   */
/*      name (the rest of the bundle is NOT assembled --    */
/*      we fail loud rather than silently dropping one)     */
/*    - A bundle references a handler that has no           */
/*      registration for the requested mission (e.g.        */
/*      "attack" requested with mission=foo)                */
/*                                                          */
/*  An empty handlers vector with ok=true is a valid        */
/*  success -- means the bundle exists but is currently     */
/*  empty (e.g. during commit #1 of the refactor, before    */
/*  any handlers have been migrated).                       */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMAND_HANDLER_FACTORY_HEADER
#define MOOS_IVP_TAK_COMMAND_HANDLER_FACTORY_HEADER

#include <memory>
#include <string>
#include <vector>

#include "handlers/CoTCommandHandler.h"

class CommandHandlerFactory
{
public:
  // ========================================================
  // BuildResult -- return value of build()
  // ========================================================
  //
  // handlers : ownership of the constructed bundle. Move
  //            into CoTCommander::m_handlers on success.
  // ok       : true on success (even with empty handlers);
  //            false on configuration error.
  // error    : human-readable failure description, empty
  //            on success.
  struct BuildResult
  {
    std::vector<std::unique_ptr<CoTCommandHandler>> handlers;
    bool        ok{false};
    std::string error;
  };

  // ========================================================
  // build() -- the only entry point
  // ========================================================
  //
  // Assemble the handler bundle for this run.
  //
  // command_set:
  //   "shore"    -> bundle for shoreside MOOSDB
  //   "vehicle"  -> bundle for vehicle-side MOOSDB
  //   "custom"   -> use custom_handlers list verbatim
  //   ""         -> default: "shore" if fleet_mode, else
  //                 "vehicle"
  //
  // mission:
  //   "aquaticus" (default) -> Aquaticus CTF bundles
  //   "hvt"                 -> HVT bundles (when those
  //                            handlers exist)
  //   ""         -> default to "aquaticus"
  //
  //   Mission affects (a) which mission-specific handlers
  //   appear in the bundle, and (b) which concrete class
  //   is constructed for mission-overloaded names like
  //   "attack" or "defend".
  //
  // fleet_mode:
  //   Only consulted when command_set is empty (for
  //   defaulting). Has no effect on bundle composition once
  //   command_set is resolved -- handlers themselves consult
  //   CommanderContext::fleet_mode when their behavior
  //   differs across modes.
  //
  // custom_handlers:
  //   When command_set=="custom", this is the explicit list
  //   of handler names to include (parsed by the caller
  //   from enable_handler = <name> lines in the .moos
  //   ProcessConfig). The mission argument still applies to
  //   custom bundles -- so e.g. custom_handlers={"attack"}
  //   with mission="hvt" builds hvt::AttackHandler. Ignored
  //   for other command_set values.
  //
  // Caller responsibility:
  //   On success, std::move(result.handlers) into the
  //   dispatcher's registry. On failure, MOOSTrace the
  //   error string and return false from OnStartUp.
  static BuildResult build(const std::string& command_set,
                            const std::string& mission,
                            bool fleet_mode,
                            const std::vector<std::string>& custom_handlers);

private:
  // ========================================================
  // Bundle builders (defined in .cpp)
  // ========================================================
  //
  // One builder per (role, mission) cell. Each returns its
  // bundle's full handler name list. Adding a new mission
  // means adding two new builders (shore + vehicle) and one
  // mission case in build()'s dispatch.
  //
  // buildOne() is the single point where a (name, mission)
  // pair is turned into a handler instance. All paths --
  // role bundles and the custom bundle -- go through it,
  // which guarantees that "attack" produces an identical
  // handler regardless of how it was selected.

  static std::vector<std::unique_ptr<CoTCommandHandler>>
  buildAquaticusShoreBundle();

  static std::vector<std::unique_ptr<CoTCommandHandler>>
  buildAquaticusVehicleBundle();

  // (HVT builders will be added when those handlers exist.)

  // Returns nullptr if (name, mission) is unknown. Callers
  // report the failure with context (which bundle, which
  // line).
  static std::unique_ptr<CoTCommandHandler>
  buildOne(const std::string& handler_name,
           const std::string& mission);
};

#endif // MOOS_IVP_TAK_COMMAND_HANDLER_FACTORY_HEADER
