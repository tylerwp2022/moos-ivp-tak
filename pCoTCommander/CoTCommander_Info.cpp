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
  blk("  Subscribes to COT_INBOUND (raw CoT XML from pCoTBridge)      ");
  blk("  and translates operator commands from ATAK into MOOS          ");
  blk("  variable publications for pHelmIvP behaviors.                 ");
  blk("                                                                ");
  blk("  To add a new command type: add a handler method and one       ");
  blk("  else-if in dispatchInboundCoT(). No other changes needed.    ");
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
  blk("  AppTick   = 10                                                ");
  blk("  CommsTick = 10                                                ");
  blk("                                                                ");
  blk("  // Waypoint control                                           ");
  blk("  // b-m-p-w-GOTO from ATAK activates the waypt_atak behavior  ");
  blk("  // Requires in .bhv:                                          ");
  blk("  //   condition = ATAK_ACTIVE = true                           ");
  blk("  //   updates   = ATAK_WPT_UPDATE                              ");
  blu("  enable_waypoint_control = true                                ");
  blu("  waypoint_update_var     = ATAK_WPT_UPDATE                     ");
  blu("  capture_radius          = 15.0   // meters                   ");
  blk("                                                                ");
  blk("  // Optional: restrict to one ATAK device UID                 ");
  blk("  // Find UID in ATAK: Settings → About                        ");
  blk("  // operator_uid_filter = ANDROID-abc123                      ");
  blk("                                                                ");
  blu("  // Geodesy — same options as pCoTBridge                       ");
  blu("  // use_nav_fallback = false                                   ");
  blk("                                                                ");
  blu("  debug = false                                                 ");
  blk("}                                                               ");
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
  blk("  NODE_REPORT       = NAME=alpha,X=...,Y=...,LAT=...,LON=...   ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  ATAK_ACTIVE     = true                                        ");
  blk("    Posted when a b-m-p-w-GOTO waypoint is received.           ");
  blk("    Activates the waypt_atak behavior in pHelmIvP.             ");
  blk("                                                                ");
  blk("  ATAK_WPT_UPDATE = points=x,y # capture_radius=r              ");
  blk("    BHV_Waypoint update string. 'updates' var name is           ");
  blk("    configurable via waypoint_update_var in .moos.             ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTCommander", "gpl");
  exit(0);
}
