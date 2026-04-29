/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTContact_Info.cpp                             */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTContact_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  Vehicle SA contact CoT publisher for the pCoT* family.       ");
  blk("  Tracks NODE_REPORT positions and publishes MIL-STD 2525C     ");
  blk("  surface vessel SA contacts to COT_OUTBOUND. pCoTBridge       ");
  blk("  forwards them to the TAK server.                              ");
  blk("                                                                ");
  blk("  Single-vehicle mode: one robot, one friendly contact.        ");
  blk("  Multi-vehicle mode:  full 6-vehicle CTF picture (own +       ");
  blk("  hostile) from the shoreside MOOSDB.                          ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTContact file.moos [OPTIONS]                         ");
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
  blu("pCoTContact Example MOOS Configuration                         ");
  blu("================================================================");
  blk("                                                                ");
  blk("// ---- Multi-vehicle mode (shoreside sim) ----                 ");
  blk("ProcessConfig = pCoTContact                                     ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blu("  own_vehicles      = blue_one,blue_two,blue_three              ");
  blu("  hostile_vehicles  = red_one,red_two,red_three                 ");
  blk("                                                                ");
  blu("  moving_send_interval     = 1.0   // seconds                  ");
  blu("  stationary_send_interval = 3.0   // seconds                  ");
  blu("  speed_threshold          = 0.5   // m/s                      ");
  blu("  cot_stale_offset         = 10.0  // seconds                  ");
  blk("                                                                ");
  blu("  debug = false                                                 ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// ---- Single-vehicle mode (hardware, on the robot) ----       ");
  blk("// ProcessConfig = pCoTContact                                  ");
  blk("// {                                                            ");
  blk("//   AppTick   = 4                                              ");
  blk("//   CommsTick = 4                                              ");
  blk("//   own_vehicle = blue_one   // auto-learned if omitted        ");
  blk("//                                                              ");
  blk("//   // affiliation: f=friendly, h=hostile, n=neutral, u=unknown");
  blk("//   // In wp_2025, set via nsplug #ifdef VTEAM in plug file:   ");
  blk("//   //   blue team: affiliation=f, team_color=Cyan             ");
  blk("//   //   red  team: affiliation=h  (team_color unused)         ");
  blk("//   affiliation = f                                            ");
  blk("//   team_color  = Cyan   // ATAK icon color for friendly only  ");
  blk("//                                                              ");
  blk("//   moving_send_interval     = 1.0                             ");
  blk("//   stationary_send_interval = 3.0                             ");
  blk("// }                                                            ");
  blk("                                                                ");
  exit(0);
}

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTContact INTERFACE                                           ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  NODE_REPORT       = NAME=...,LAT=...,LON=...,HDG=...,SPD=...");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_OUTBOUND = <event type=\"a-f-S-C-U-N\">...</event>       ");
  blk("                 (or a-h-S-C-U-N for hostile vehicles)         ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTContact", "gpl");
  exit(0);
}
