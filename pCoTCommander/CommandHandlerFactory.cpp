/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CommandHandlerFactory.cpp                       */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Factory implementation. Adding a new handler is a       */
/*  three-line diff in this file:                           */
/*    1. #include "handlers/<scope>/MyHandler.h"            */
/*    2. Add an entry to buildOne()                         */
/*    3. Add the handler name to the relevant bundle list   */
/************************************************************/

#include "CommandHandlerFactory.h"

// ---- common/ handlers --------------------------------------
#include "handlers/common/DeployHandler.h"
#include "handlers/common/ReturnHandler.h"
#include "handlers/common/StationHandler.h"
#include "handlers/common/PauseHandler.h"
#include "handlers/common/AtakHandler.h"
#include "handlers/common/ResumeHandler.h"
#include "handlers/common/AvoidHandler.h"
#include "handlers/common/OpregHandler.h"
#include "handlers/common/StatusHandler.h"
#include "handlers/common/WaypointHandler.h"
#include "handlers/common/HelpHandler.h"

// ---- aquaticus/ handlers -----------------------------------
#include "handlers/aquaticus/AttackHandler.h"
#include "handlers/aquaticus/DefendHandler.h"
#include "handlers/aquaticus/PlayHandler.h"
#include "handlers/aquaticus/StopHandler.h"
#include "handlers/aquaticus/UntagHandler.h"
#include "handlers/aquaticus/RetryHandler.h"
#include "handlers/aquaticus/FlagPursuitHandler.h"


// ============================================================
// buildOne() -- name + mission -> concrete handler
// ============================================================
//
// Single source of truth for "what handler does name X
// resolve to under mission Y?". Common handlers map a name
// directly to one class; mission-overloaded handlers
// dispatch on the mission arg.
//
// Returns nullptr if (name, mission) is not registered.
// Callers report the failure with bundle context.

std::unique_ptr<CoTCommandHandler>
CommandHandlerFactory::buildOne(const std::string& name,
                                 const std::string& mission)
{
  // ---- Common (mission-agnostic) handlers ----
  if(name == "deploy")    return std::unique_ptr<CoTCommandHandler>(new common::DeployHandler());
  if(name == "return")    return std::unique_ptr<CoTCommandHandler>(new common::ReturnHandler());
  if(name == "station")   return std::unique_ptr<CoTCommandHandler>(new common::StationHandler());
  if(name == "pause")     return std::unique_ptr<CoTCommandHandler>(new common::PauseHandler());
  if(name == "atak")      return std::unique_ptr<CoTCommandHandler>(new common::AtakHandler());
  if(name == "resume")    return std::unique_ptr<CoTCommandHandler>(new common::ResumeHandler());
  if(name == "avoid")     return std::unique_ptr<CoTCommandHandler>(new common::AvoidHandler());
  if(name == "opreg")     return std::unique_ptr<CoTCommandHandler>(new common::OpregHandler());
  if(name == "status")    return std::unique_ptr<CoTCommandHandler>(new common::StatusHandler());
  if(name == "waypoint")  return std::unique_ptr<CoTCommandHandler>(new common::WaypointHandler());
  if(name == "help")      return std::unique_ptr<CoTCommandHandler>(new common::HelpHandler());

  // ---- Mission-specific handlers ----
  if(name == "attack") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::AttackHandler());
    return nullptr;
  }
  if(name == "defend") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::DefendHandler());
    return nullptr;
  }
  if(name == "play") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::PlayHandler());
    return nullptr;
  }
  if(name == "stop") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::StopHandler());
    return nullptr;
  }
  if(name == "untag") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::UntagHandler());
    return nullptr;
  }
  if(name == "retry") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::RetryHandler());
    return nullptr;
  }
  if(name == "flag_pursuit") {
    if(mission == "aquaticus")
      return std::unique_ptr<CoTCommandHandler>(new aquaticus::FlagPursuitHandler());
    return nullptr;
  }

  return nullptr;
}


// ============================================================
// Aquaticus shore bundle
// ============================================================
//
// 16 chat-only handlers. No CoT handlers because the TAK
// server routes operationally meaningful CoT (Go-To,
// flag-pursuit) directly to the targeted vehicle's
// pCoTBridge -- the shore dispatcher never sees them.
//
// HelpHandler is listed LAST so its registry view at
// startup is complete when it builds the keyword index.
// (Construction order doesn't affect runtime help output,
// which iterates the registry at chat dispatch time, but
// it's the right discipline anyway.)

std::vector<std::unique_ptr<CoTCommandHandler>>
CommandHandlerFactory::buildAquaticusShoreBundle()
{
  static const std::vector<std::string> kNames = {
    "deploy", "return", "station", "pause",
    "atak",   "resume",
    "avoid",  "opreg",
    "attack", "defend",
    "play",   "stop",        // shore-only game state
    "untag",  "retry",
    "status",
    "help"                   // last
  };

  std::vector<std::unique_ptr<CoTCommandHandler>> out;
  out.reserve(kNames.size());
  for(const std::string& n : kNames) {
    auto h = buildOne(n, "aquaticus");
    if(h) out.push_back(std::move(h));
  }
  return out;
}


// ============================================================
// Aquaticus vehicle bundle
// ============================================================
//
// 16 handlers: same 14 chat handlers as shore (minus
// play/stop which are shore-only), plus WaypointHandler
// (common, CoT) and FlagPursuitHandler (aquaticus, CoT).

std::vector<std::unique_ptr<CoTCommandHandler>>
CommandHandlerFactory::buildAquaticusVehicleBundle()
{
  static const std::vector<std::string> kNames = {
    "deploy", "return", "station", "pause",
    "atak",   "resume",
    "avoid",  "opreg",
    "attack", "defend",
    "untag",  "retry",
    "status",
    "waypoint",     // common, CoT
    "flag_pursuit", // aquaticus, CoT
    "help"          // last
  };

  std::vector<std::unique_ptr<CoTCommandHandler>> out;
  out.reserve(kNames.size());
  for(const std::string& n : kNames) {
    auto h = buildOne(n, "aquaticus");
    if(h) out.push_back(std::move(h));
  }
  return out;
}


// ============================================================
// build() -- public entry point
// ============================================================
//
// Resolves defaults, dispatches to the appropriate bundle
// builder, and packages the result for the dispatcher.
//
// Defaulting rules:
//   - command_set unset: shore if fleet_mode, else vehicle
//   - mission     unset: aquaticus

CommandHandlerFactory::BuildResult
CommandHandlerFactory::build(const std::string& command_set,
                              const std::string& mission,
                              bool fleet_mode,
                              const std::vector<std::string>& custom_handlers)
{
  BuildResult result;

  // Resolve defaults
  std::string cs = command_set;
  if(cs.empty()) cs = fleet_mode ? "shore" : "vehicle";

  std::string m = mission;
  if(m.empty()) m = "aquaticus";

  // Dispatch by command_set
  if(cs == "shore") {
    if(m == "aquaticus") {
      result.handlers = buildAquaticusShoreBundle();
      result.ok = true;
      return result;
    }
    // Future: hvt etc.
    result.ok = false;
    result.error = "Unknown mission '" + m + "' for shore command_set";
    return result;
  }

  if(cs == "vehicle") {
    if(m == "aquaticus") {
      result.handlers = buildAquaticusVehicleBundle();
      result.ok = true;
      return result;
    }
    result.ok = false;
    result.error = "Unknown mission '" + m + "' for vehicle command_set";
    return result;
  }

  if(cs == "custom") {
    if(custom_handlers.empty()) {
      result.ok = false;
      result.error = "command_set=custom but no enable_handler entries provided";
      return result;
    }
    result.handlers.reserve(custom_handlers.size());
    for(const std::string& n : custom_handlers) {
      auto h = buildOne(n, m);
      if(!h) {
        result.handlers.clear();
        result.ok = false;
        result.error = "Unknown handler '" + n + "' for mission '" + m + "'";
        return result;
      }
      result.handlers.push_back(std::move(h));
    }
    result.ok = true;
    return result;
  }

  result.ok = false;
  result.error = "Unknown command_set '" + cs +
                 "' (expected shore | vehicle | custom)";
  return result;
}
