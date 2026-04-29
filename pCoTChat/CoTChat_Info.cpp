/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTChat_Info.cpp                                */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTChat_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  ATAK GeoChat <-> MOOS bridge for the pCoT* app family.       ");
  blk("  Handles inbound chat from TAK → ATAK_CHAT_IN,                ");
  blk("  and outbound chat from ATAK_CHAT_OUT → COT_OUTBOUND.         ");
  blk("  Builds a callsign→UID contact table from SA CoT for          ");
  blk("  accurate direct message and group message addressing.         ");
  blk("                                                                ");
  blk("  Supported outbound destinations:                              ");
  blk("    All Chat Rooms, Groups, Teams, team colors (Cyan etc),      ");
  blk("    roles (HQ etc), named groups, direct messages.              ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTChat file.moos [OPTIONS]                            ");
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
  blu("pCoTChat Example MOOS Configuration                            ");
  blu("================================================================");
  blk("                                                                ");
  blk("ProcessConfig = pCoTChat                                        ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4    // event-driven: COMMS_DRIVEN_ITERATE_AND_MAIL");
  blk("                                                                ");
  blk("  own_callsign = alpha                                          ");
  blk("  // own_uid = surveyor-alpha  // default: surveyor-{callsign} ");
  blk("                                                                ");
  blk("  echo_filter = true   // suppress own messages echoed by TAK  ");
  blk("                                                                ");
  blu("  debug = false                                                 ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// Outbound usage (post to MOOSDB):                            ");
  blk("// Fields separated by '|' — NOT ',' — because message content ");
  blk("// may contain commas (e.g. coordinates, lists).               ");
  blk("//   ATAK_CHAT_OUT = message=hello team|chatroom=All Chat Rooms ");
  blk("//   ATAK_CHAT_OUT = message=hello team|chatroom=Cyan           ");
  blk("//   ATAK_CHAT_OUT = message=hello|chatroom=Tyler   // DM       ");
  blk("                                                                ");
  exit(0);
}

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTChat INTERFACE                                              ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  COT_INBOUND       = <event ...>...</event>  (from pCoTBridge)");
  blk("  ATAK_CHAT_OUT     = message=X|chatroom=Y  ('|' separator,    ");
  blk("                      not ',' — messages may contain commas)   ");
  blk("  NODE_REPORT       = NAME=alpha,LAT=...,LON=...               ");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  ATAK_CHAT_IN  = callsign=X,chatroom=Y,message=Z              ");
  blk("  COT_OUTBOUND  = <event type=b-t-f ...>...</event>            ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTChat", "gpl");
  exit(0);
}
