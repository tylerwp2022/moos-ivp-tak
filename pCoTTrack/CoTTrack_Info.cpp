/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTTrack_Info.cpp                               */
/*    DATE: August 2026                                     */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTTrack_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  Inbound TAK situational awareness for the pCoT* family.       ");
  blk("  The mirror of pCoTContact: reads ground-unit position         ");
  blk("  reports (PLI) off COT_INBOUND and republishes them as         ");
  blk("  NODE_REPORT, so an ATAK user appears as a contact in          ");
  blk("  pMarineViewer and to pContactMgrV20 / contact behaviors.      ");
  blk("                                                                ");
  blk("  Only ground-unit CoT is claimed (a-{f,h,n,u}-G-U by default). ");
  blk("  GOTO waypoints, chat and graphics on COT_INBOUND are left     ");
  blk("  to pCoTCommander and pCoTChat.                                ");
  blk("                                                                ");
  blk("  Self-echo from the TAK server is filtered twice: by CoT type  ");
  blk("  (pCoTContact emits surface vessels, not ground units) and by  ");
  blk("  uid prefix (ignore_uid_prefix, default \"surveyor-\").         ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTTrack file.moos [OPTIONS]                            ");
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
  blu("pCoTTrack Example MOOS Configuration                            ");
  blu("================================================================");
  blk("                                                                ");
  blk("ProcessConfig = pCoTTrack                                       ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blk("  // ---- how the ATAK user appears in MOOS ----               ");
  blk("  // Node name is node_prefix + sanitized ATAK callsign,       ");
  blk("  // e.g. callsign \"Delta 1\" -> atak_delta_1                   ");
  blu("  node_prefix = atak_                                           ");
  blk("  // pMarineViewer body: swimmer, kayak, ship, track, buoy...  ");
  blu("  node_type   = swimmer                                         ");
  blu("  node_color  = yellow                                          ");
  blu("  node_length = 2.0                                             ");
  blk("                                                                ");
  blk("  // ---- team assignment ----                                 ");
  blk("  // GROUP= is the operator's game team: uFldTagManager only   ");
  blk("  // accepts nodes whose GROUP matches one of its two teams,   ");
  blk("  // and it is what uFldNodeComms filters on when              ");
  blk("  // apply_groups=true. By default GROUP comes from the team   ");
  blk("  // color the operator picked in ATAK, mapped red/maroon/     ");
  blk("  // magenta -> red and blue/dark blue/cyan/teal -> blue.      ");
  blu("  group_from_team = true                                        ");
  blk("  // team_map   = red:red, maroon:red, cyan:blue, dark blue:blue");
  blk("  // Fallback team for unmapped colors (or all, if            ");
  blk("  // group_from_team=false). Unset = no GROUP field.          ");
  blk("  // node_group = blue                                        ");
  blk("                                                                ");
  blk("  // ---- what to ingest ----                                  ");
  blk("  // Prefix match. a-f-G-U also claims a-f-G-U-C-I etc.       ");
  blu("  track_cot_types = a-f-G-U,a-h-G-U,a-n-G-U,a-u-G-U            ");
  blk("                                                                ");
  blk("  // ---- loopback suppression ----                            ");
  blk("  // Drop our own contacts echoed back by the TAK server.     ");
  blu("  ignore_uid_prefix = surveyor-                                 ");
  blk("  // ignore_callsign = shoreside,safety_boat                   ");
  blk("                                                                ");
  blk("  // ---- callsign whitelist ----                              ");
  blk("  // When set, ONLY these callsigns are tracked; every other   ");
  blk("  // PLI is dropped. Case-insensitive. Unset (default) admits  ");
  blk("  // all callsigns.                                            ");
  blk("  // callsign_whitelist = delta 1, tyler-atak                  ");
  blk("                                                                ");
  blk("  // ---- outputs ----                                         ");
  blu("  publish_node_report = true                                    ");
  blu("  publish_view_marker = false                                   ");
  blu("  marker_type  = triangle                                       ");
  blu("  marker_color = yellow                                         ");
  blu("  marker_width = 6.0                                            ");
  blk("                                                                ");
  blk("  // ---- motion source ----                                   ");
  blk("  // true = ignore <track speed course> in the CoT and always  ");
  blk("  // derive speed/heading from successive position fixes.      ");
  blk("  // false (default) = use <track> when the client sends it.   ");
  blu("  derive_motion_only = false                                    ");
  blk("                                                                ");
  blk("  // ---- rates ----                                           ");
  blk("  // 0 = post on every inbound CoT, plus each AppTick as a     ");
  blk("  // keepalive so downstream consumers never see it stale.     ");
  blu("  post_interval = 0.0                                           ");
  blk("  // Drop the track entirely after this long with no CoT.      ");
  blu("  stale_timeout = 30.0                                          ");
  blu("  max_tracks    = 20                                            ");
  blk("                                                                ");
  blu("  debug = false                                                 ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("Requires LatOrigin / LongOrigin in the mission file — the same ");
  blk("origin as plug_origin_warp.moos — to place inbound lat/lon on  ");
  blk("the local grid.                                                 ");
  blk("                                                                ");
  exit(0);
}

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTTrack INTERFACE                                             ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  COT_INBOUND  = <event type=\"a-f-G-U-C\" uid=...>...</event>  ");
  blk("                 (published by pCoTBridge)                      ");
  blk("  NODE_REPORT       = geodesy NAV anchor only                   ");
  blk("  NODE_REPORT_LOCAL = geodesy NAV anchor only                   ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  NODE_REPORT = NAME=atak_delta_1,TYPE=swimmer,TIME=...,        ");
  blk("                X=..,Y=..,LAT=..,LON=..,SPD=..,HDG=..,          ");
  blk("                DEP=0,LENGTH=..,MODE=TAK,VSOURCE=pCoTTrack,     ");
  blk("                GROUP=<team>  (from the ATAK team color via     ");
  blk("                team_map; see --example)                        ");
  blk("  VIEW_MARKER = x=..,y=..,label=atak_delta_1,type=triangle,...  ");
  blk("                (only when publish_view_marker=true)            ");
  blk("                                                                ");
  blk("NOTES:                                                          ");
  blk("------------------------------------                            ");
  blk("  NODE_REPORT TIME uses the local MOOS clock, not the CoT event ");
  blk("  time. Downstream staleness checks compare against MOOSTime,   ");
  blk("  and the sending Android device is a different clock reference.");
  blk("                                                                ");
  blk("  Speed and heading come from <track speed course> when the     ");
  blk("  client sends it, and are derived from successive fixes when   ");
  blk("  it does not.                                                  ");
  blk("                                                                ");
  blk("  A PLI reporting lat=0 lon=0 (no GPS fix) is discarded rather  ");
  blk("  than placed on the map.                                       ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTTrack", "gpl");
  exit(0);
}
