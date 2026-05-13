/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ParsedCoT.h                                     */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  POD struct representing a parsed CoT event.             */
/*                                                          */
/*  Produced by CoTCommander when an event arrives on       */
/*  COT_INBOUND and passed by const-ref to every handler    */
/*  whose claimsCoT() returns true. Carries the most        */
/*  common attributes (uid, type, lat, lon) plus the raw    */
/*  XML so handlers can extract additional attributes via   */
/*  cot::extractAttr() when needed.                         */
/*                                                          */
/*  WHY A POD INSTEAD OF A CLASS                            */
/*  -------------------------------------------------------- */
/*  Handlers should be cheap to write. A struct with plain  */
/*  fields makes it obvious what's available without        */
/*  methods to look up. raw_xml carries everything the      */
/*  dispatcher's parser couldn't anticipate -- handlers     */
/*  that need exotic attributes call cot::extractAttr() on  */
/*  it directly (see CoTUtils.h).                           */
/*                                                          */
/*  HAS_POSITION FLAG                                       */
/*  -------------------------------------------------------- */
/*  A CoT event MAY omit lat/lon (some non-spatial types    */
/*  do). Storing 0.0 with a separate has_position flag is   */
/*  safer than treating 0.0 as a sentinel -- lat=0 lon=0    */
/*  is a valid (if silly) position somewhere off the coast  */
/*  of Africa. Handlers that need a position MUST check     */
/*  has_position before using lat/lon, or they will quietly */
/*  command vehicles to drive there.                        */
/************************************************************/

#ifndef MOOS_IVP_TAK_PARSED_COT_HEADER
#define MOOS_IVP_TAK_PARSED_COT_HEADER

#include <string>

struct ParsedCoT
{
  // CoT event uid attribute -- unique sender/event identifier.
  // Used by the operator-UID filter (in the dispatcher) and
  // for own-echo suppression: events whose uid begins with
  // "surveyor-" are this vehicle's own SA broadcast looped
  // back through the TAK server, and are dropped before any
  // handler sees them.
  std::string uid;

  // CoT event type attribute -- e.g. "b-m-p-w-GOTO" (waypoint
  // Go-To from ATAK), "b-m-p-s-m" (spot marker, used for the
  // Aquaticus flag), "a-f-S-C-U-N" (friendly surface vessel
  // SA contact). Dispatcher routes on this string by asking
  // each handler's claimsCoT().
  std::string type;

  // Position in WGS84 decimal degrees. Valid only if
  // has_position == true.
  double      lat{0.0};
  double      lon{0.0};
  bool        has_position{false};

  // The original CoT XML. Handlers needing attributes beyond
  // the parsed fields call cot::extractAttr(raw_xml, "name")
  // on this string -- e.g. FlagPursuitHandler checking for
  // <_aquaticus_graphics sa_broadcast="true"/> to suppress
  // its own map-display broadcasts.
  //
  // Stored by value (not reference). CoT messages are small
  // (typ. <2 KB) and copy-by-value frees handlers from
  // worrying about the dispatcher's parse buffer lifetime.
  std::string raw_xml;
};

#endif // MOOS_IVP_TAK_PARSED_COT_HEADER
