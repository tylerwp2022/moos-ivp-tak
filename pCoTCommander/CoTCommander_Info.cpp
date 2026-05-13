/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander_Info.cpp                           */
/*    DATE: April 2026                                      */
/*    REV:  May 13, 2026 -- handler-class refactor          */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTCommander_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  Inbound CoT command dispatcher for the pCoT* app family.     ");
  blk("  Subscribes to COT_INBOUND (from pCoTBridge) and              ");
  blk("  ATAK_CHAT_IN (from pCoTChat), translates operator commands   ");
  blk("  from ATAK into MOOS variable publications for pHelmIvP.      ");
  blk("  Every command DMs a confirmation or error back to sender.    ");
  blk("                                                                ");
  blk("  ARCHITECTURE (handler-class refactor, May 2026):              ");
  blk("  pCoTCommander is a thin dispatcher. Per-command logic lives  ");
  blk("  in 18 handler classes under handlers/{common,aquaticus}/.    ");
  blk("  At startup, CommandHandlerFactory assembles a bundle based   ");
  blk("  on (command_set, mission). The dispatcher then iterates the  ");
  blk("  bundle to dispatch CoT events (claimsCoT first-claim-wins)   ");
  blk("  and chat commands (keyword index lookup).                    ");
  blk("                                                                ");
  blk("  TWO DEPLOYMENT MODES:                                        ");
  blk("  fleet_mode=true  (shoreside MOOSDB):                         ");
  blk("    Loads Aquaticus SHORE bundle: 16 chat-only handlers.       ");
  blk("    Posts *_ALL variables -- uFldShoreBroker routes to all     ");
  blk("    vehicle communities. Chatroom = AQUATICUS-SHORE.           ");
  blk("    Prefix any command with a vehicle name to target one:      ");
  blk("      'blue_one atak', 'red_two avoid off'                     ");
  blk("    Vehicle names must use underscores (blue_one, not blue one)");
  blk("    play/stop are fleet-wide only (no vehicle prefix).         ");
  blk("                                                                ");
  blk("  fleet_mode=false (vehicle MOOSDB):                           ");
  blk("    Loads Aquaticus VEHICLE bundle: 14 chat + 2 CoT handlers.  ");
  blk("    Posts bare variable names directly on the vehicle's MOOSDB.");
  blk("    Chatroom = $(VNAME). Operator DMs the vehicle callsign.    ");
  blk("    Requires pCoTBridge and pCoTChat on the vehicle ANTLER.    ");
  blk("                                                                ");
  blk("  ATAK MODE:                                                    ");
  blk("  'atak' or any incoming waypoint -> ATAK_MODE=true            ");
  blk("    All game behaviors yield (condition on ATAK_MODE!=true).   ");
  blk("    Vehicle holds position until a waypoint or 'resume'.       ");
  blk("  'resume' -> ATAK_MODE=false + ATAK_WAYPT_ACTIVE=false        ");
  blk("    Game behaviors resume on next helm tick.                   ");
  blk("  'return', 'station', 'pause' also exit ATAK mode.            ");
  blk("                                                                ");
  blk("  CHAT COMMANDS (depend on loaded bundle):                     ");
  blk("    Mode:    atak, resume                                       ");
  blk("    Fleet:   deploy, return, station, pause, play, stop        ");
  blk("    Role:    attack, defend, attack easy, defend easy           ");
  blk("    Toggles: avoid on|off, untag on|off,                       ");
  blk("             retry on|off, opreg on|off                        ");
  blk("    Info:    status, help                                       ");
  blk("    (help is auto-generated from registry -- no drift.)        ");
  blk("                                                                ");
  blk("  FLAG PURSUIT (vehicle mode only):                            ");
  blk("  FlagPursuitHandler claims b-m-p-s-m CoT with uid=flag_uid,   ");
  blk("  enters ATAK mode, and navigates to the flag. Subscribes to   ");
  blk("  every variable in team_flag_vars; pursuit ends when ANY      ");
  blk("  goes true (teammate has the flag). Config:                   ");
  blk("    flag_pursuit_enabled, flag_uid, flag_my_team,              ");
  blk("    flag_capture_radius, team_flag_vars.                       ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTCommander file.moos [OPTIONS]                       ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("Options:                                                        ");
  mag("  --alias","=<ProcessName>                                      ");
  mag("  --example, -e                                                 ");
  mag("  --help, -h                                                    ");
  mag("  --interface, -i                                               ");
  mag("  --version, -v                                                 ");
  blk("                                                                ");
  exit(0);
}

void showExampleConfigAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTCommander Example MOOS Configuration                       ");
  blu("================================================================");
  blk("                                                                ");
  blk("  Production deployments should use the plug files, not this   ");
  blk("  block verbatim. See:                                          ");
  blk("    plug_pCoTCommander_shore.moos                              ");
  blk("    plug_pCoTCommander_vehicle.moos                            ");
  blk("    pCoTCommander.moos.example  (full reference)               ");
  blk("                                                                ");
  blk("// --- SHORESIDE (fleet_mode=true) ---                         ");
  blk("ProcessConfig = pCoTCommander                                   ");
  blk("{                                                               ");
  blk("  AppTick   = 4     // COMMS_DRIVEN -- fires on mail only      ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blk("  // ---- Mode ----                                            ");
  blu("  fleet_mode = true                                             ");
  blk("  // command_set optional; factory derives 'shore' from        ");
  blk("  // fleet_mode=true. Set explicitly for belt-and-suspenders:  ");
  blk("  //   command_set = shore | vehicle | custom                  ");
  blu("  mission    = aquaticus                                        ");
  blk("                                                                ");
  blk("  // ---- Chat routing ----                                    ");
  blu("  command_chatroom     = AQUATICUS-SHORE                        ");
  blu("  enable_chat_commands = true                                   ");
  blk("  // operator_uid_filter = ANDROID  (optional substring)       ");
  blk("                                                                ");
  blu("  use_nav_fallback = false                                      ");
  blu("  debug            = false                                      ");
  blk("                                                                ");
  blk("  // Shore bundle has neither WaypointHandler nor              ");
  blk("  // FlagPursuitHandler, so capture_radius, flag_*, etc. are   ");
  blk("  // unused. Setting them does no harm; handlers ignore        ");
  blk("  // unknown keys silently.                                     ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// --- VEHICLE (fleet_mode=false) ---                          ");
  blk("// Use plug_pCoTCommander_vehicle.moos rather than this block. ");
  blk("// ProcessConfig = pCoTCommander                               ");
  blk("// {                                                            ");
  blk("//   AppTick   = 4                                              ");
  blk("//   CommsTick = 4                                              ");
  blk("//                                                              ");
  blk("//   fleet_mode       = false                                   ");
  blk("//   mission          = aquaticus                               ");
  blk("//   command_chatroom = $(VNAME)   // vehicle ATAK callsign     ");
  blk("//   enable_chat_commands = true                                ");
  blk("//   debug = false                                              ");
  blk("//                                                              ");
  blk("//   // WaypointHandler config                                  ");
  blk("//   capture_radius          = 15.0                             ");
  blk("//   waypoint_update_var     = ATAK_WPT_UPDATE                  ");
  blk("//   enable_waypoint_control = true                             ");
  blk("//                                                              ");
  blk("//   // FlagPursuitHandler config                               ");
  blk("//   flag_pursuit_enabled = true                                ");
  blk("//   flag_uid             = aquaticus-flag-red                  ");
  blk("//   flag_my_team         = blue                                ");
  blk("//   flag_capture_radius  = 5.0                                 ");
  blk("//   team_flag_vars       = HAS_FLAG_BLUE_ONE,HAS_FLAG_BLUE_TWO,HAS_FLAG_BLUE_THREE");
  blk("// }                                                            ");
  blk("                                                                ");
  blk("// --- CUSTOM bundle (advanced) ---                            ");
  blk("// command_set = custom                                         ");
  blk("// enable_handler = deploy                                      ");
  blk("// enable_handler = return                                      ");
  blk("// enable_handler = atak                                        ");
  blk("// enable_handler = resume                                      ");
  blk("// enable_handler = status                                      ");
  blk("// enable_handler = help    // last                             ");
  blk("                                                                ");
  blk("BHV_Waypoint requirements (vehicle mode) -- in meta_surveyor.bhv:");
  blk("  initialize ATAK_MODE              = false                    ");
  blk("  initialize ATAK_WAYPT_ACTIVE      = false                    ");
  blk("  initialize ATAK_AVOID_COLLISIONS  = true                     ");
  blk("  initialize ATAK_AUTO_UNTAG        = true                     ");
  blk("  initialize ATAK_OPREG_RECOVER     = true                     ");
  blk("  Behavior = BHV_Waypoint                                       ");
  blk("  {                                                             ");
  blk("    name      = waypt_atak                                      ");
  blk("    pwt       = 100  // recover(300) and colregs(300) win      ");
  blk("    perpetual = true                                            ");
  blk("    condition = ATAK_MODE = true                                ");
  blk("    condition = ATAK_WAYPT_ACTIVE = true                        ");
  blk("    condition = (TAGGED != true) or (ATAK_AUTO_UNTAG = false)  ");
  blk("    updates   = ATAK_WPT_UPDATE                                 ");
  blk("    endflag   = ATAK_WPT_REACHED = true                         ");
  blk("    speed = 3.5  capture_radius = 15.0  slip_radius = 30.0      ");
  blk("  }                                                             ");
  blk("  // All game behaviors (attack/defend/loiter) require:        ");
  blk("  condition = ATAK_MODE != true                                 ");
  blk("  // BHV_OpRegionRecover requires:                              ");
  blk("  condition = (ATAK_MODE = false) or (ATAK_OPREG_RECOVER = true)");
  blk("  // BHV_AvdColregsV22 requires:                                ");
  blk("  condition = (ATAK_MODE = false) or (ATAK_AVOID_COLLISIONS = true)");
  blk("                                                                ");
  exit(0);
}

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTCommander INTERFACE                                         ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS (dispatcher-managed):                             ");
  blk("------------------------------------                            ");
  blk("  COT_INBOUND       = <event ...>...</event>  (from pCoTBridge)");
  blk("  ATAK_CHAT_IN      = callsign=X,chatroom=Y,message=Z          ");
  blk("                      (from pCoTChat)                          ");
  blk("  NODE_REPORT       = NAME=alpha,X=...,Y=...,LAT=...,LON=...   ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("  DEPLOY            = true/false  (deployment state mirror)    ");
  blk("                                                                ");
  blk("  Vehicle mode only:                                            ");
  blk("  ATAK_MODE         = true/false  (operator-control mirror)    ");
  blk("  TAGGED            = true/false  (game-tag state mirror)      ");
  blk("  ATAK_RETRY        = true/false  (retry-toggle mirror)        ");
  blk("                                                                ");
  blk("SUBSCRIPTIONS (handler-specific):                               ");
  blk("------------------------------------                            ");
  blk("  ATAK_WPT_REACHED  = true/false  (waypt_atak endflag)         ");
  blk("    From WaypointHandler. Triggers ack DM and reset to false.  ");
  blk("                                                                ");
  blk("  HAS_FLAG_*        = true/false                                ");
  blk("    From FlagPursuitHandler. One subscription per entry in     ");
  blk("    team_flag_vars (e.g. HAS_FLAG_BLUE_ONE). ANY true ends     ");
  blk("    pursuit silently. Vehicle mode only.                       ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  [_sfx] notation: '_ALL' in fleet mode, '_<VEHICLE>' when     ");
  blk("  vehicle-prefixed, '' (bare) in vehicle mode.                 ");
  blk("                                                                ");
  blk("  -- From WaypointHandler (b-m-p-w-GOTO CoT) --                ");
  blk("  ATAK_MODE             = true   (enter operator control)     ");
  blk("  ATAK_WAYPT_ACTIVE     = true   (activate waypt_atak)        ");
  blk("  ATAK_WPT_UPDATE       = points=x,y # capture_radius=r        ");
  blk("    Variable name configurable via waypoint_update_var.        ");
  blk("  ATAK_WPT_REACHED      = false  (reset after ack DM)          ");
  blk("                                                                ");
  blk("  -- From FlagPursuitHandler (b-m-p-s-m flag CoT) --           ");
  blk("  ATAK_MODE             = true   (enter operator control)     ");
  blk("  ATAK_WAYPT_ACTIVE     = true                                  ");
  blk("  ATAK_FLAG_PURSUIT     = true|false                            ");
  blk("    True while pursuing; false when teammate captures or        ");
  blk("    pursuit otherwise terminates.                              ");
  blk("  ATAK_WPT_UPDATE       = points=x,y # capture_radius=r        ");
  blk("                                                                ");
  blk("  -- From chat handlers (GeoChat commands) --                  ");
  blk("  -- All commands DM a confirmation back to sender. --         ");
  blk("                                                                ");
  blk("  DEPLOY[_sfx]               = true|false                      ");
  blk("    DeployHandler, ReturnHandler, PauseHandler                  ");
  blk("  MOOS_MANUAL_OVERRIDE[_sfx] = true|false                      ");
  blk("    DeployHandler, ReturnHandler, PauseHandler                  ");
  blk("  RETURN[_sfx]               = true|false                      ");
  blk("    DeployHandler, ReturnHandler                                ");
  blk("  STATION_KEEP[_sfx]         = true                            ");
  blk("    StationHandler                                              ");
  blk("  ATAK_MODE[_sfx]            = true|false                      ");
  blk("    true:  AtakHandler, WaypointHandler, FlagPursuitHandler    ");
  blk("    false: ResumeHandler, ReturnHandler, StationHandler,       ");
  blk("           PauseHandler, FlagPursuitHandler (on pursuit end)   ");
  blk("  ATAK_WAYPT_ACTIVE[_sfx]    = true|false                      ");
  blk("    Same handlers as ATAK_MODE above.                           ");
  blk("  AQUATICUS_GAME_ALL         = play|pause  (fleet mode only)   ");
  blk("    PlayHandler / StopHandler                                  ");
  blk("  ACTION[_sfx]               = ATTACK_MED | ATTACK_E |         ");
  blk("                               DEFEND_MED | DEFEND_E           ");
  blk("    AttackHandler / DefendHandler                              ");
  blk("    Warning DM sent if vehicle is in ATAK mode (role posts     ");
  blk("    but takes effect only after 'resume').                     ");
  blk("  ATAK_AVOID_COLLISIONS[_sfx] = true|false  ('avoid on|off')   ");
  blk("    AvoidHandler. Gates BHV_AvdColregsV22 in ATAK mode.        ");
  blk("  ATAK_AUTO_UNTAG[_sfx]      = true|false  ('untag on|off')    ");
  blk("    UntagHandler.                                              ");
  blk("    on:  vehicle auto-recovers when tagged, resumes waypoint.  ");
  blk("    off: vehicle ignores tags, stays on ATAK waypoint.         ");
  blk("  ATAK_RETRY[_sfx]           = true|false  ('retry on|off')    ");
  blk("    RetryHandler.                                              ");
  blk("    on:  waypt_atak reactivates automatically after untag.     ");
  blk("    off: ATAK_WAYPT_ACTIVE cleared after untag; operator DM'd. ");
  blk("  ATAK_OPREG_RECOVER[_sfx]   = true|false  ('opreg on|off')    ");
  blk("    OpregHandler. Gates BHV_OpRegionRecover in ATAK mode.      ");
  blk("    WARNING DM sent when turned off. Always active outside     ");
  blk("    ATAK mode regardless of setting.                            ");
  blk("                                                                ");
  blk("  ATAK_CHAT_OUT              = message=...|chatroom=<sender>   ");
  blk("    Confirmation DM from every handler (success or error).     ");
  blk("    'help' auto-generates from registry -- shows only the      ");
  blk("    commands present in the loaded bundle.                     ");
  blk("    NOTE: avoid < > & in message content -- they are embedded  ");
  blk("    raw in GeoChat CoT XML and will corrupt the message.       ");
  blk("                                                                ");
  blk("APPCASTING:                                                     ");
  blk("------------------------------------                            ");
  blk("  Dispatcher section:                                           ");
  blk("    Geodesy state, mode, chatroom, optional UID filter         ");
  blk("    State mirror: deployed [+ atak_mode/tagged/retry]          ");
  blk("    Counters:  cot_received/handled/ignored,                   ");
  blk("               chat_received/handled/unknown                   ");
  blk("    Last dispatched handler name + payload summary             ");
  blk("                                                                ");
  blk("  Per-handler sections:                                         ");
  blk("    Each handler contributes its own block (==== NAME ====).   ");
  blk("    Counters and last-action are handler-specific.             ");
  blk("    Set 'debug = true' to also dump the circular debug buffer. ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTCommander", "gpl");
  exit(0);
}
