/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTGraphics_Info.cpp                            */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTGraphics_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  VIEW_* to CoT graphics renderer for the pCoT* app family.    ");
  blk("  Subscribes to VIEW_POINT and VIEW_SEGLIST published by        ");
  blk("  pMarineViewer and pHelmIvP, converts them to CoT XML, and    ");
  blk("  publishes to COT_OUTBOUND for pCoTBridge to forward to TAK.  ");
  blk("                                                                ");
  blk("  VIEW_POINT   → CoT spot marker (b-m-p-s-m)                   ");
  blk("  VIEW_SEGLIST → CoT polyline   (u-d-f)                        ");
  blk("  active=false → CoT delete     (t-x-d-d)                      ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTGraphics file.moos [OPTIONS]                        ");
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
  blu("pCoTGraphics Example MOOS Configuration                        ");
  blu("================================================================");
  blk("                                                                ");
  blk("ProcessConfig = pCoTGraphics                                    ");
  blk("{                                                               ");
  blk("  AppTick   = 10                                                ");
  blk("  CommsTick = 10                                                ");
  blk("                                                                ");
  blu("  publish_view_points    = true                                 ");
  blu("  publish_view_seglists  = true                                 ");
  blk("                                                                ");
  blk("  // VIEW_POLYGON + UTM_ZONE_ONE/TWO → filled polygons         ");
  blk("  // (uFldFlagManager grab zones, uFldTagManager team zones)    ");
  blu("  publish_view_polygons  = true                                 ");
  blk("                                                                ");
  blk("  // FLAG_SUMMARY + VIEW_MARKER → colored flag spot markers    ");
  blu("  publish_flag_markers   = true                                 ");
  blk("                                                                ");
  blk("  // VIEW_TEXTBOX → text label (score display)                 ");
  blu("  publish_score_label    = true                                 ");
  blk("                                                                ");
  blk("  // Send VIEW_POINTs immediately on every update (no throttle) ");
  blk("  // Required for trackpt to stay in sync with pMarineViewer   ");
  blu("  immediate_view_points  = true                                 ");
  blk("                                                                ");
  blk("  // Seconds between re-sends for throttled graphics           ");
  blk("  // Applies to VIEW_SEGLISTs and VIEW_POINTs when not immediate");
  blu("  stationary_send_interval = 3.0                               ");
  blk("                                                                ");
  blk("  // Geodesy                                                    ");
  blu("  use_nav_fallback = false                                      ");
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
  blu("pCoTGraphics INTERFACE                                          ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  VIEW_POINT        = x=...,y=...,label=...,active=true/false  ");
  blk("  VIEW_SEGLIST      = pts={x,y:x,y:...},label=...,active=...   ");
  blk("  NODE_REPORT       = NAME=alpha,X=...,Y=...,LAT=...,LON=...   ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_OUTBOUND = <event ...>...</event>  (raw CoT XML to TAK)  ");
  blk("    b-m-p-s-m  for VIEW_POINT (spot marker)                    ");
  blk("    u-d-f      for VIEW_SEGLIST (polyline)                     ");
  blk("    t-x-d-d    for active=false (delete)                       ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTGraphics", "gpl");
  exit(0);
}
