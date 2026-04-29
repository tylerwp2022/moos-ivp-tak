/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTCommander_Info.cpp                           */
/*    DATE: April 2026                                      */
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
  blk("  Subscribes to COT_INBOUND and ATAK_CHAT_IN and translates    ");
  blk("  operator commands from ATAK into MOOS variable publications. ");
  blk("                                                                ");
  blk("  TWO DEPLOYMENT MODES:                                        ");
  blk("  fleet_mode=true  (shoreside): posts *_ALL variables,         ");
  blk("    routed by uFldShoreBroker. Chatroom=AQUATICUS-SHORE.       ");
  blk("  fleet_mode=false (per-vehicle): posts bare variable names    ");
  blk("    directly on that vehicle's MOOSDB. Chatroom=<callsign>.    ");
  blk("                                                                ");
  blk("  Chat commands: deploy, return, station, pause, play, stop,   ");
  blk("    status, help, attack, defend (fleet and vehicle modes).    ");
  blk("  Fleet mode: prefix any command with a vehicle name to target ");
  blk("    one vehicle: 'blue_one deploy', 'red_two attack'.          ");
  blk("  Vehicle names require underscores (blue_one, not blue one).  ");
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
  blk("ProcessConfig = pCoTCommander                                   ");
  blk("{                                                               ");
  blk("  AppTick   = 4     // COMMS_DRIVEN — fires on mail only       ");
  blk("  CommsTick = 10                                                ");
  blk("                                                                ");
  blk("  // ---- Chat command interface ----                          ");
  blk("  // Operator DMs the command_chatroom callsign in ATAK.       ");
  blk("  // fleet_mode=true  (shore): posts *_ALL vars via uFldShoreBroker");
  blk("  // fleet_mode=false (vehicle): posts directly on own MOOSDB  ");
  blu("  enable_chat_commands = true                                   ");
  blu("  command_chatroom     = AQUATICUS-SHORE  // must match SHORE_CALLSIGN");
  blu("  fleet_mode           = true                                   ");
  blk("                                                                ");
  blk("  // ---- Waypoint control ----                                ");
  blk("  // b-m-p-w-GOTO from ATAK activates the waypt_atak behavior  ");
  blk("  // Requires in .bhv:                                          ");
  blk("  //   condition = ATAK_ACTIVE = true                           ");
  blk("  //   updates   = ATAK_WPT_UPDATE                              ");
  blu("  enable_waypoint_control = true                                ");
  blu("  waypoint_update_var     = ATAK_WPT_UPDATE                     ");
  blu("  capture_radius          = 15.0   // meters                   ");
  blk("                                                                ");
  blk("  // Optional: restrict CoT commands to one ATAK device UID   ");
  blk("  // Find UID in ATAK: Settings → About                        ");
  blk("  // operator_uid_filter = ANDROID-abc123                      ");
  blk("                                                                ");
  blu("  use_nav_fallback = false                                      ");
  blu("  debug = false                                                 ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// --- Vehicle-mode example (per-surveyor) ---                 ");
  blk("// ProcessConfig = pCoTCommander                               ");
  blk("// {                                                            ");
  blk("//   AppTick   = 4                                              ");
  blk("//   CommsTick = 10                                             ");
  blk("//   enable_chat_commands = true                                ");
  blk("//   command_chatroom     = blue_one  // this vehicle's callsign");
  blk("//   fleet_mode           = false                               ");
  blk("//   enable_waypoint_control = true                             ");
  blk("//   capture_radius = 15.0                                      ");
  blk("//   use_nav_fallback = false                                   ");
  blk("//   debug = false                                               ");
  blk("// }                                                            ");
  blk("// PREREQUISITE: share ATAK_CHAT_IN and COT_INBOUND from shore ");
  blk("// to each vehicle via pShare (plug_pShare.moos on the vehicle)");
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
  blk("                      (from pCoTChat — GeoChat commands)       ");
  blk("  NODE_REPORT       = NAME=alpha,X=...,Y=...,LAT=...,LON=...   ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("  ATAK_WPT_REACHED  = true/false  (waypt_atak endflag)         ");
  blk("  DEPLOY            = true/false  (tracks deployment state)    ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  -- From b-m-p-w-GOTO waypoint CoT --                        ");
  blk("  ATAK_ACTIVE     = true                                        ");
  blk("    Activates waypt_atak behavior in pHelmIvP.                 ");
  blk("  ATAK_WPT_UPDATE = points=x,y # capture_radius=r              ");
  blk("    BHV_Waypoint update string. Var name is configurable.      ");
  blk("                                                                ");
  blk("  -- From GeoChat commands (chatroom=command_chatroom) --      ");
  blk("  DEPLOY[_ALL]               = true|false                      ");
  blk("  MOOS_MANUAL_OVERRIDE[_ALL] = true|false                      ");
  blk("  RETURN[_ALL]               = true|false                      ");
  blk("  STATION_KEEP[_ALL]         = true                            ");
  blk("  AQUATICUS_GAME_ALL         = play|pause  (fleet mode only)   ");
  blk("  ACTION[_<VEHICLE>]         = ATTACK_MED|ATTACK_E|            ");
  blk("                               DEFEND_MED|DEFEND_E             ");
  blk("  ATAK_CHAT_OUT              = message=...|chatroom=<sender>   ");
  blk("    Confirmation DM sent back to command sender.               ");
  blk("    'help' sends the full command list as a DM.               ");
  blk("    NOTE: avoid < > & in any ATAK_CHAT_OUT message content    ");
  blk("    — they are embedded raw in CoT XML and will corrupt it.   ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTCommander", "gpl");
  exit(0);
}
