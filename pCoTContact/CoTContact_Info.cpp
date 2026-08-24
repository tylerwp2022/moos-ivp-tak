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
  blk("  Single-vehicle mode: one robot, one contact. Affiliation and  ");
  blk("    team color set via config (f/h, Cyan/Red etc).              ");
  blk("    own_role=hostile makes a hidden-group boat honor stealth:   ");
  blk("    no CoT until HVT_REVEAL_STATE (bridged from the shoreside)  ");
  blk("    marks it revealed.                                          ");
  blk("  Multi-vehicle mode:  full CTF picture from shoreside MOOSDB.  ");
  blk("    Friendly = own_vehicles, hostile = hostile_vehicles.        ");
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
  blk("  // ATAK icon color for friendly contacts.                    ");
  blk("  // Hostile contacts never get __group regardless.            ");
  blu("  team_color = Cyan                                             ");
  blk("                                                                ");
  blk("  // Callsign overrides — TAK displays these instead of the    ");
  blk("  // vehicle name. One line per vehicle; case preserved.       ");
  blu("  // callsign = blue_one:BOAT-1                                 ");
  blu("  // callsign = blue_two:BOAT-2                                 ");
  blk("                                                                ");
  blu("  moving_send_interval     = 1.0   // seconds                  ");
  blu("  stationary_send_interval = 3.0   // seconds                  ");
  blu("  speed_threshold          = 0.5   // m/s                      ");
  blu("  cot_stale_offset         = 10.0  // seconds                  ");
  blk("                                                                ");
  blk("  // immediate=true disables moving/stationary throttle        ");
  blk("  // and sends CoT on every NODE_REPORT.                       ");
  blu("  immediate = false                                             ");
  blk("                                                                ");
  blk("  // stealth_integration=true suppresses CoT for vehicles      ");
  blk("  // marked hidden in HVT_REVEAL_STATE (uFldNodeCommsHVT).     ");
  blk("  // false (default) = always report all locations.            ");
  blu("  stealth_integration = false                                   ");
  blk("                                                                ");
  blk("  // hide_tagged=true: friendly vehicles in TAGGED_VEHICLES    ");
  blk("  // (uFldTagManager) stop reporting and vanish from TAK.      ");
  blu("  hide_tagged = false                                           ");
  blk("                                                                ");
  blk("  // contact_alerts=true: on HVT_REVEAL_EVENT, raise an ATAK   ");
  blk("  // In Contact alert on every friendly vehicle; alerts follow ");
  blk("  // the boats and are cancelled after contact_alert_duration. ");
  blu("  contact_alerts         = false                                ");
  blu("  contact_alert_duration = 3   // seconds                      ");
  blk("  // Cancels are re-sent for this long so a dropped packet or  ");
  blk("  // bridge disconnect can't strand an alert on TAK screens.   ");
  blu("  contact_alert_cancel_repeat = 5   // seconds                 ");
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
  blk("//   callsign    = BOAT-1     // TAK callsign override          ");
  blk("//                                                              ");
  blk("//   // f=friendly h=hostile; team_color sets ATAK icon color  ");
  blk("//   // affil=f+team_color=Cyan → cyan icon, IN contacts list  ");
  blk("//   // affil=h (no team_color) → hostile diamond, MAP ONLY    ");
  blk("//   affiliation = f                                            ");
  blk("//   team_color  = Cyan                                         ");
  blk("//                                                              ");
  blk("//   immediate = true   // send on every NODE_REPORT_LOCAL     ");
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
  blk("  HVT_REVEAL_STATE  = mode=3,red_one=hidden,red_two=revealed   ");
  blk("    (only when stealth_integration=true; from uFldNodeCommsHVT)");
  blk("  HVT_REVEAL_EVENT  = vname=red_one,observer=blue_one,why=...  ");
  blk("    (only when contact_alerts=true; from uFldNodeCommsHVT)     ");
  blk("  TAGGED_VEHICLES   = blue_two,red_one                         ");
  blk("    (only when hide_tagged=true; from uFldTagManager)          ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_OUTBOUND = <event type=\"a-{affil}-S-C-U-N\">...</event> ");
  blk("    affil driven by affiliation= param (single-vehicle) or     ");
  blk("    own_vehicles/hostile_vehicles membership (multi-vehicle).  ");
  blk("                                                                ");
  blk("KEY CONFIG PARAMS:                                              ");
  blk("------------------------------------                            ");
  blk("  callsign = <cs> | <vname>:<cs>   TAK callsign override.     ");
  blk("    Bare form: single-vehicle mode, applies to own vehicle.   ");
  blk("    <vname>:<cs> form: per-vehicle, repeatable, either mode.  ");
  blk("    Overrides the displayed callsign only — the CoT uid stays ");
  blk("    surveyor-<vname>, so no duplicate contact appears in TAK. ");
  blk("  affiliation = f|h|n|u   CoT type affiliation (default: f)   ");
  blk("  team_color  = Cyan|Red|...  ATAK icon color via <__group>   ");
  blk("    Omit team_color for map-only (no contacts list entry).     ");
  blk("  immediate = true|false   true = send on every NODE_REPORT;  ");
  blk("    moving/stationary throttle disabled.                       ");
  blk("  stealth_integration = true|false  (default: false)          ");
  blk("    true = withhold CoT for vehicles currently hidden per     ");
  blk("    HVT_REVEAL_STATE; they reappear in TAK when revealed.     ");
  blk("    Hostiles not yet listed in the reveal state are withheld ");
  blk("    (fail-safe). false = always report everyone.             ");
  blk("  hide_tagged = true|false  (default: false)                 ");
  blk("    true = friendly vehicles listed in TAGGED_VEHICLES stop  ");
  blk("    reporting and disappear from TAK until untagged.         ");
  blk("  contact_alerts = true|false  (default: false)              ");
  blk("    true = each HVT_REVEAL_EVENT raises an ATAK In Contact   ");
  blk("    emergency alert (b-a-o-opn) on every friendly vehicle.   ");
  blk("    Alerts follow the boats' positions and are explicitly    ");
  blk("    cancelled (b-a-o-can) after contact_alert_duration secs. ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTContact", "gpl");
  exit(0);
}
