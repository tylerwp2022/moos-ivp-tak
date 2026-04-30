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
  blk("  Subscribes to VIEW_* variables on the MOOS community,        ");
  blk("  converts them to CoT XML, and publishes to COT_OUTBOUND for  ");
  blk("  pCoTBridge to forward to TAK.                                ");
  blk("                                                                ");
  blk("  VIEW_POINT       → CoT spot marker      (b-m-p-s-m)          ");
  blk("  VIEW_SEGLIST     → CoT open polyline    (u-d-f)              ");
  blk("  VIEW_POLYGON     → CoT closed polygon   (u-d-f, fill optional)");
  blk("  VIEW_CIRCLE      → CoT circle           (u-d-c-c)            ");
  blk("  UTM_ZONE_ONE/TWO → CoT filled polygons  (team zone bounds)   ");
  blk("  FLAG_SUMMARY     → CoT colored markers  (b-m-p-s-m)          ");
  blk("  VIEW_MARKER      → CoT colored marker   (b-m-p-s-m)          ");
  blk("  VIEW_TEXTBOX     → CoT text label       (b-m-p-s-m/LABEL)    ");
  blk("  active=false     → CoT delete event     (t-x-d-d)            ");
  blk("                                                                ");
  blk("  Two deployment modes:                                         ");
  blk("    Vehicle (default): publishes own VIEW_* graphics only.     ");
  blk("    Shoreside: publishes field graphics; vehicle-specific       ");
  blk("      shapes filtered via shoreside=true + vehicle_names.      ");
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
  blk("  // VIEW_POLYGON + UTM_ZONE_ONE/TWO → filled or outline polygons");
  blu("  publish_view_polygons  = true                                 ");
  blk("                                                                ");
  blk("  // VIEW_CIRCLE → circle (u-d-c-c)                            ");
  blu("  publish_view_circles   = true                                 ");
  blk("                                                                ");
  blk("  // FLAG_SUMMARY + VIEW_MARKER → colored flag spot markers    ");
  blu("  publish_flag_markers   = true                                 ");
  blk("                                                                ");
  blk("  // VIEW_TEXTBOX → text label (score display)                 ");
  blu("  publish_score_label    = true                                 ");
  blk("                                                                ");
  blk("  // Send VIEW_POINTs immediately on every update (no throttle) ");
  blu("  immediate_view_points  = true                                 ");
  blk("                                                                ");
  blk("  // Seconds between re-sends for throttled graphics           ");
  blu("  stationary_send_interval = 3.0                               ");
  blk("                                                                ");
  blk("  // Geodesy                                                    ");
  blu("  use_nav_fallback = false                                      ");
  blk("                                                                ");
  blk("  // ---- Shoreside instance: filter vehicle-specific graphics  ");
  blk("  // shoreside=true blocks any VIEW_* label containing a vehicle");
  blk("  // name. vehicle_names = $(VNAMES) from launch_shoreside.sh. ");
  blk("  // shoreside     = true                                       ");
  blk("  // vehicle_names = $(VNAMES)                                  ");
  blk("                                                                ");
  blk("  // Legacy fallback: label_block_contains = loiter,opreg,wpt  ");
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
  blk("SUBSCRIPTIONS (conditional on publish_* config flags):          ");
  blk("------------------------------------                            ");
  blk("  VIEW_POINT        = x=...,y=...,label=...,active=true/false  ");
  blk("  VIEW_SEGLIST      = pts={x,y:x,y:...},label=...,active=...   ");
  blk("  VIEW_POLYGON      = pts={...},label=...,fill_color=...,active=");
  blk("  VIEW_CIRCLE       = x=,y=,radius=,label=,edge_color=,active= ");
  blk("  UTM_ZONE_ONE      = pts={...}  (red team boundary, shoreside) ");
  blk("  UTM_ZONE_TWO      = pts={...}  (blue team boundary, shoreside)");
  blk("  FLAG_SUMMARY      = <flag>#<flag>#...  (shoreside at startup) ");
  blk("  VIEW_MARKER       = x=,y=,primary_color=,label=  (shoreside) ");
  blk("  VIEW_TEXTBOX      = x=,y=,msg=  (score label, shoreside)     ");
  blk("  NODE_REPORT       = NAME=...,X=...,Y=...,LAT=...,LON=...     ");
  blk("  NODE_REPORT_LOCAL = (same format — geodesy anchor fallback)   ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_OUTBOUND = <event ...>...</event>  (raw CoT XML to TAK)  ");
  blk("    b-m-p-s-m  VIEW_POINT, FLAG_SUMMARY/VIEW_MARKER, score     ");
  blk("    u-d-f      VIEW_SEGLIST, VIEW_POLYGON, UTM_ZONE_*          ");
  blk("    u-d-c-c    VIEW_CIRCLE                                      ");
  blk("    t-x-d-d    active=false on any variable (delete event)     ");
  blk("                                                                ");
  blk("KEY CONFIG PARAMS:                                              ");
  blk("------------------------------------                            ");
  blk("  shoreside = true         Block vehicle-specific labels.       ");
  blk("  vehicle_names = $(VNAMES) Colon-separated, from nsplug.      ");
  blk("  label_block_contains = loiter,opreg  (legacy fallback)       ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTGraphics", "gpl");
  exit(0);
}
