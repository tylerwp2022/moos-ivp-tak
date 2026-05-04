/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander_Info.cpp                           */
/*    DATE: April 2026 (updated May 2026)                   */
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
  blk("  TWO DEPLOYMENT MODES:                                        ");
  blk("  fleet_mode=true  (shoreside MOOSDB):                         ");
  blk("    Posts *_ALL variables -- uFldShoreBroker routes to all     ");
  blk("    vehicle communities. Chatroom = AQUATICUS-SHORE.           ");
  blk("    Prefix any command with a vehicle name to target one:      ");
  blk("      'blue_one atak', 'red_two avoid off'                     ");
  blk("    Vehicle names must use underscores (blue_one, not blue one)");
  blk("    play/stop are fleet-wide only (no vehicle prefix).         ");
  blk("                                                                ");
  blk("  fleet_mode=false (vehicle MOOSDB):                           ");
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
  blk("  'return', 'station', 'pause' also exit ATAK mode.           ");
  blk("                                                                ");
  blk("  CHAT COMMANDS (all modes):                                   ");
  blk("    Mode:    atak, resume                                       ");
  blk("    Fleet:   deploy, return, station, pause, play, stop        ");
  blk("    Role:    attack, defend, attack easy, defend easy           ");
  blk("    Toggles: avoid on|off, untag on|off,                       ");
  blk("             retry on|off, opreg on|off                        ");
  blk("    Info:    status, help                                       ");
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
  blk("// --- SHORESIDE (fleet_mode=true) ---                         ");
  blk("ProcessConfig = pCoTCommander                                   ");
  blk("{                                                               ");
  blk("  AppTick   = 4     // COMMS_DRIVEN -- fires on mail only      ");
  blk("  CommsTick = 10                                                ");
  blk("                                                                ");
  blk("  // fleet_mode=true: posts *_ALL vars via uFldShoreBroker.    ");
  blk("  // command_chatroom must match SHORE_CALLSIGN in pCoTChat.   ");
  blk("  // Prefix any command with a vehicle name to target one:     ");
  blk("  //   'blue_one atak'      -> ATAK_MODE_BLUE_ONE=true         ");
  blk("  //   'blue_one avoid off' -> ATAK_AVOID_COLLISIONS_BLUE_ONE=false");
  blk("  //   'red_two retry off'  -> ATAK_RETRY_RED_TWO=false        ");
  blu("  enable_chat_commands = true                                   ");
  blu("  command_chatroom     = AQUATICUS-SHORE                        ");
  blu("  fleet_mode           = true                                   ");
  blk("                                                                ");
  blk("  // Waypoint control: 'Send To <vehicle>' in ATAK sends a    ");
  blk("  // b-m-p-w-GOTO to that vehicle's pCoTBridge. Commander     ");
  blk("  // converts lat/lon->XY and posts ATAK_MODE + ATAK_WPT_UPDATE.");
  blk("  // Requires in meta_surveyor.bhv:                            ");
  blk("  //   initialize ATAK_MODE              = false               ");
  blk("  //   initialize ATAK_WAYPT_ACTIVE      = false               ");
  blk("  //   initialize ATAK_AVOID_COLLISIONS  = true                ");
  blk("  //   initialize ATAK_AUTO_UNTAG        = true                ");
  blk("  //   initialize ATAK_OPREG_RECOVER     = true                ");
  blk("  //   Behavior = BHV_Waypoint                                 ");
  blk("  //   {                                                        ");
  blk("  //     name      = waypt_atak                                 ");
  blk("  //     pwt       = 100  // recover(300) and colregs(300) win ");
  blk("  //     perpetual = true                                       ");
  blk("  //     condition = ATAK_MODE = true                           ");
  blk("  //     condition = ATAK_WAYPT_ACTIVE = true                   ");
  blk("  //     condition = (TAGGED != true) or (ATAK_AUTO_UNTAG = false)");
  blk("  //     updates   = ATAK_WPT_UPDATE                            ");
  blk("  //     endflag   = ATAK_WPT_REACHED = true                    ");
  blk("  //     speed = 3.5  capture_radius = 15.0  slip_radius = 30.0");
  blk("  //   }                                                        ");
  blk("  //   // All game behaviors (attack/defend/loiter) require:   ");
  blk("  //   condition = ATAK_MODE != true                            ");
  blk("  //   // common.bhv BHV_OpRegionRecover requires:             ");
  blk("  //   condition = (ATAK_MODE = false) or (ATAK_OPREG_RECOVER = true)");
  blk("  //   // common.bhv BHV_AvdColregsV22 requires:               ");
  blk("  //   condition = (ATAK_MODE = false) or (ATAK_AVOID_COLLISIONS = true)");
  blu("  enable_waypoint_control = true                                ");
  blu("  waypoint_update_var     = ATAK_WPT_UPDATE                     ");
  blu("  capture_radius          = 15.0   // meters                   ");
  blk("                                                                ");
  blk("  // Optional: restrict CoT commands to one ATAK device UID.  ");
  blk("  // Find UID in ATAK: Settings -> About.                      ");
  blk("  // operator_uid_filter = ANDROID-abc123                      ");
  blk("                                                                ");
  blu("  use_nav_fallback = false                                      ");
  blu("  debug            = false                                      ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// --- VEHICLE (fleet_mode=false) ---                          ");
  blk("// Use plug_pCoTCommander.moos rather than this block directly.");
  blk("// ProcessConfig = pCoTCommander                               ");
  blk("// {                                                            ");
  blk("//   AppTick   = 4                                              ");
  blk("//   CommsTick = 10                                             ");
  blk("//   fleet_mode           = false                               ");
  blk("//   command_chatroom     = $(VNAME)   // vehicle callsign      ");
  blk("//   enable_chat_commands = true                                ");
  blk("//   enable_waypoint_control = true                             ");
  blk("//   waypoint_update_var     = ATAK_WPT_UPDATE                  ");
  blk("//   capture_radius          = 15.0                             ");
  blk("//   use_nav_fallback = false                                   ");
  blk("//   debug            = false                                   ");
  blk("// }                                                            ");
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
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  COT_INBOUND       = <event ...>...</event>  (from pCoTBridge)");
  blk("  ATAK_CHAT_IN      = callsign=X,chatroom=Y,message=Z          ");
  blk("                      (from pCoTChat)                          ");
  blk("  NODE_REPORT       = NAME=alpha,X=...,Y=...,LAT=...,LON=...   ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("  ATAK_WPT_REACHED  = true/false  (waypt_atak endflag)         ");
  blk("  DEPLOY            = true/false  (deployment state)           ");
  blk("  ATAK_MODE         = true/false  (vehicle mode only --        ");
  blk("                      warns if attack/defend sent while        ");
  blk("                      game behaviors are suppressed)           ");
  blk("  TAGGED            = true/false  (vehicle mode only --        ");
  blk("                      detects untagged transition for          ");
  blk("                      retry-off logic)                         ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  -- From b-m-p-w-GOTO waypoint CoT --                        ");
  blk("  ATAK_MODE[_ALL]       = true                                  ");
  blk("    Enters operator control. All game behaviors yield.         ");
  blk("  ATAK_WAYPT_ACTIVE[_ALL] = true                               ");
  blk("    Activates waypt_atak behavior in pHelmIvP.                 ");
  blk("  ATAK_WPT_UPDATE       = points=x,y # capture_radius=r        ");
  blk("    BHV_Waypoint update string. Variable name is configurable. ");
  blk("  Waypoint rejections DM a reason to the sender:               ");
  blk("    DEPLOY=false  -> 'not deployed, send deploy first'         ");
  blk("    No GPS        -> 'waiting for GPS fix'                     ");
  blk("    Bad CoT       -> 'missing coordinates in GoTo'             ");
  blk("                                                                ");
  blk("  -- From GeoChat commands (chatroom=command_chatroom) --      ");
  blk("  -- All commands DM a confirmation back to the sender. --     ");
  blk("                                                                ");
  blk("  ATAK_MODE[_ALL]            = true|false                      ");
  blk("    true:  'atak' command, or any accepted waypoint.           ");
  blk("    false: 'resume', 'return', 'station', 'pause'.             ");
  blk("  ATAK_WAYPT_ACTIVE[_ALL]    = true|false                      ");
  blk("    true:  accepted waypoint.                                   ");
  blk("    false: 'resume', 'return', 'station', 'pause', or          ");
  blk("           untagged transition with retry=off.                 ");
  blk("  DEPLOY[_ALL]               = true|false                      ");
  blk("  MOOS_MANUAL_OVERRIDE[_ALL] = true|false                      ");
  blk("  RETURN[_ALL]               = true|false                      ");
  blk("  STATION_KEEP[_ALL]         = true                            ");
  blk("  AQUATICUS_GAME_ALL         = play|pause  (fleet mode only)   ");
  blk("  ACTION[_<VEHICLE>]         = ATTACK_MED | ATTACK_E |         ");
  blk("                               DEFEND_MED | DEFEND_E           ");
  blk("    Warning DM sent if vehicle is in ATAK mode (role posts     ");
  blk("    but takes effect only after 'resume').                     ");
  blk("  ATAK_AVOID_COLLISIONS[_ALL] = true|false  ('avoid on|off')   ");
  blk("    Gates BHV_AvdColregsV22 and BHV_AvoidCollision in ATAK    ");
  blk("    mode. No effect on autonomous strategy mode.               ");
  blk("  ATAK_AUTO_UNTAG[_ALL]      = true|false  ('untag on|off')    ");
  blk("    on:  vehicle auto-recovers when tagged, resumes waypoint.  ");
  blk("    off: vehicle ignores tags, stays on ATAK waypoint.         ");
  blk("  ATAK_RETRY[_ALL]           = true|false  ('retry on|off')    ");
  blk("    on:  waypt_atak reactivates automatically after untag.     ");
  blk("    off: ATAK_WAYPT_ACTIVE cleared after untag; operator DM'd; ");
  blk("         operator must send a new waypoint to continue.        ");
  blk("  ATAK_OPREG_RECOVER[_ALL]   = true|false  ('opreg on|off')    ");
  blk("    Gates BHV_OpRegionRecover (field boundary) in ATAK mode.  ");
  blk("    WARNING DM sent when turned off.                           ");
  blk("    Always active outside ATAK mode regardless of setting.     ");
  blk("  ATAK_CHAT_OUT              = message=...|chatroom=<sender>   ");
  blk("    Confirmation DM to every command sender (success or error).");
  blk("    'help' sends mode-appropriate command list (vehicle mode   ");
  blk("    omits fleet-only commands like play/stop/vehicle prefix).  ");
  blk("    NOTE: avoid < > & in message content -- they are embedded  ");
  blk("    raw in GeoChat CoT XML and will corrupt the message.       ");
  blk("                                                                ");
  blk("APPCASTING (vehicle mode):                                      ");
  blk("------------------------------------                            ");
  blk("  State line: deployed=t|f  atak_mode=t|f  tagged=t|f  retry=t|f");
  blk("  Counters: cot_received, cot_handled, cot_ignored,            ");
  blk("            waypoint_commands, chat_commands                   ");
  blk("  Last command description shown for quick field diagnostics.  ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTCommander", "gpl");
  exit(0);
}
