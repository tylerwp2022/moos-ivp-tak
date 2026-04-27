/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTBridge_Info.cpp                              */
/*    DATE: April 2026                                      */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "CoTBridge_Info.h"

using namespace std;

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  CoT transport layer for the pCoT* app family.                ");
  blk("  Maintains TCP/TLS connection to a TAK server, publishes own  ");
  blk("  vehicle SA position CoT, forwards COT_OUTBOUND to TAK,       ");
  blk("  and publishes received CoT to COT_INBOUND.                   ");
  blk("                                                                ");
  blk("  Supports plain TCP (port 8088) and mTLS (port 8089).         ");
  blk("  Supports single-vehicle and multi-vehicle (sim) modes.       ");
  blk("  Downstream apps handle content — pCoTBridge is pure          ");
  blk("  transport.                                                    ");
}

void showHelpAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("Usage: pCoTBridge file.moos [OPTIONS]                          ");
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
  blu("pCoTBridge Example MOOS Configuration                          ");
  blu("================================================================");
  blk("                                                                ");
  blk("// ---- Single-vehicle mode (real hardware / one sim vehicle) ----");
  blk("ProcessConfig = pCoTBridge                                      ");
  blk("{                                                               ");
  blk("  AppTick   = 10                                                ");
  blk("  CommsTick = 10                                                ");
  blk("                                                                ");
  blk("  tak_host    = 192.168.0.38                                    ");
  blk("  tak_port    = 8088    // 8088=plain TCP, 8089=TLS             ");
  blk("  own_vehicle = alpha   // must match NAME in NODE_REPORT       ");
  blk("                                                                ");
  blk("  // TLS params — required when tak_port = 8089                ");
  blk("  // tls_cert_file = /path/to/certs/shoreside.pem              ");
  blk("  // tls_key_file  = /path/to/certs/shoreside.key              ");
  blk("  // tls_ca_file   = /path/to/certs/shoreside-trusted.pem      ");
  blk("  // tls_key_pass  = atakatak  // if key is passphrase-protected");
  blk("                                                                ");
  blu("  moving_send_interval     = 1.0   // seconds                  ");
  blu("  stationary_send_interval = 3.0   // seconds                  ");
  blu("  speed_threshold          = 0.5   // m/s                      ");
  blu("  cot_stale_offset         = 10.0  // seconds                  ");
  blu("  reconnect_interval       = 5.0   // seconds                  ");
  blu("  cot_delimiter            = newline                            ");
  blu("  debug                    = false                              ");
  blk("}                                                               ");
  blk("                                                                ");
  blk("// ---- Multi-vehicle mode (shoreside sim) ----                 ");
  blk("// ProcessConfig = pCoTBridge                                   ");
  blk("// {                                                            ");
  blk("//   AppTick   = 10                                             ");
  blk("//   CommsTick = 10                                             ");
  blk("//   tak_host          = 192.168.0.38                           ");
  blk("//   tak_port          = 8088                                   ");
  blk("//   own_vehicles      = alpha,bravo,charlie                    ");
  blk("//   hostile_vehicles  = red1,red2,red3                         ");
  blk("//   ... (same throttle/TLS params as above)                    ");
  blk("// }                                                            ");
  blk("                                                                ");
  exit(0);
}

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("================================================================");
  blu("pCoTBridge INTERFACE                                            ");
  blu("================================================================");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  NODE_REPORT       = NAME=alpha,LAT=...,LON=...,HDG=...,SPD=..");
  blk("  NODE_REPORT_LOCAL = (same format)                             ");
  blk("  COT_OUTBOUND      = <event ...>...</event>  (raw CoT XML)    ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  COT_INBOUND = <event ...>...</event>  (raw CoT XML from TAK) ");
  blk("                                                                ");
  blk("  // Own vehicle SA position CoT is sent directly to the TAK   ");
  blk("  // server — it is not published to MOOSDB.                   ");
  blk("                                                                ");
  exit(0);
}

void showReleaseInfoAndExit()
{
  showReleaseInfo("pCoTBridge", "gpl");
  exit(0);
}
