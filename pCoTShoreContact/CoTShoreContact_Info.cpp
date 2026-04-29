/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTShoreContact_Info.cpp                        */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTShoreContact_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  Publishes a manually-configured position as a MIL-STD 2525C  ");
  blk("  SA contact to COT_OUTBOUND for pCoTBridge to forward to TAK. ");
  blk("                                                                ");
  blk("  Primary use: marking the shoreside control station on the     ");
  blk("  ATAK map. The shore computer has no GPS so its position is    ");
  blk("  set manually in the ProcessConfig block.                      ");
  blk("                                                                ");
  blk("  Secondary use: any fixed infrastructure point — safety boat,  ");
  blk("  reference buoy, observer station, etc. Add one ANTLER         ");
  blk("  instance per contact.                                         ");
  blk("                                                                ");
  blk("  CoT type: a-f-G-E (always friendly)                           ");
  blk("    Affiliation is fixed as friendly — required for the contact ");
  blk("    to appear in the ATAK contacts list, not just on the map.   ");
  blk("    Icon color is controlled by the team_color config param.    ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTShoreContact file.moos [OPTIONS]                    ");
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
  blu("pCoTShoreContact Example MOOS Configuration                    ");
  blu("================================================================");
  blk("                                                                ");
  blk("ProcessConfig = pCoTShoreContact                               ");
  blk("{                                                               ");
  blk("  AppTick   = 1                                                 ");
  blk("  CommsTick = 1                                                 ");
  blk("                                                                ");
  blk("  // Required — no default                                      ");
  blu("  lat         = 41.34928       // WGS84 decimal degrees        ");
  blu("  lon         = -74.063645     // WGS84 decimal degrees        ");
  blk("                                                                ");
  blk("  // Recommended                                                ");
  blu("  callsign    = AQUATICUS-SHORE                                 ");
  blk("                                                                ");
  blk("  // Optional                                                   ");
  blu("  hae         = 0.0            // height above ellipsoid (m)   ");
  blu("  uid         = shore-001      // auto-generated if omitted     ");
  blu("  team_color  = Green          // ATAK icon color: Green, Cyan, ");
  blu("                               //  Blue, Red, Yellow, White,    ");
  blu("                               //  Magenta, Orange              ");
  blk("                               // Contact is always friendly    ");
  blk("                               // (required for contacts list)  ");
  blk("                                                                ");
  blu("  send_interval = 30.0         // seconds between resends       ");
  blu("  stale_seconds = 604800       // 7 days — contact won't expire ");
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
  blu("pCoTShoreContact INTERFACE                                      ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  none — purely config-driven                                   ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_OUTBOUND = <event ...>...</event>  (raw CoT XML to TAK)  ");
  blk("    type: a-f-G-E (always friendly)                             ");
  blk("    icon: COT_MAPPING_2525C/a-f/a-f-G                          ");
  blk("    color: set by team_color param (<__group name=\"...\"/>)    ");
  blk("    how:  h-g-i-g-o                                             ");
  blk("    ce/le: 9999999  (manually entered, no GPS error bound)      ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTShoreContact", "gpl");
  exit(0);
}
