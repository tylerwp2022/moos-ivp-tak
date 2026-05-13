/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTUtils.h                                      */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Free-function utilities for CoT XML manipulation.       */
/*                                                          */
/*  Currently exposes one function: extractAttr() -- a      */
/*  lightweight attribute scanner that avoids a libxml2     */
/*  dependency. Sufficient for the simple, well-formed CoT  */
/*  XML produced by ATAK and our own pCoT* apps. If we      */
/*  ever need full XPath, namespace handling, or strict     */
/*  XML validation, this file is the swap-in boundary --    */
/*  callers depend only on the function signatures, not on  */
/*  the parsing strategy.                                   */
/*                                                          */
/*  USED BY                                                 */
/*  -------------------------------------------------------- */
/*    - CoTCommander dispatcher: populating ParsedCoT       */
/*      fields (uid, type, lat, lon) at parse time.         */
/*    - Handlers: extracting type-specific attributes from  */
/*      ParsedCoT::raw_xml after the dispatcher has handed  */
/*      off the event. Example: FlagPursuitHandler checks   */
/*      <_aquaticus_graphics sa_broadcast="true"/> via      */
/*        cot::extractAttr(evt.raw_xml, "sa_broadcast")     */
/*                                                          */
/*  THREADING                                               */
/*  -------------------------------------------------------- */
/*  Pure function -- safe to call from any thread. (MOOS    */
/*  apps are single-threaded by default, so this only       */
/*  matters if a handler spawns its own threads.)           */
/************************************************************/

#ifndef MOOS_IVP_TAK_COT_UTILS_HEADER
#define MOOS_IVP_TAK_COT_UTILS_HEADER

#include <string>

namespace cot
{
  // Extract a named attribute value from a CoT XML string.
  //
  // Handles both single- and double-quoted attribute values
  // ('foo' and "foo"). Returns the empty string if the
  // attribute is not found. The FIRST occurrence wins --
  // nested elements with the same attribute name yield the
  // outermost match, which is the expected behavior for
  // top-level CoT attributes like uid/type/lat/lon.
  //
  // Examples:
  //   extractAttr("<event uid=\"foo\" type=\"bar\">", "uid")
  //     -> "foo"
  //   extractAttr("<event type='b-m-p-w-GOTO'>", "type")
  //     -> "b-m-p-w-GOTO"
  //   extractAttr("<event type='x'>", "uid")
  //     -> ""          (not found)
  //
  // Implementation note: scans for the literal substring
  // `name="` (and `name='`); does NOT respect XML escaping
  // or CDATA. CoT in our pipeline doesn't use those, but
  // bear it in mind if a future use case feeds in
  // adversarial XML.
  std::string extractAttr(const std::string& xml,
                           const std::string& attr);
}

#endif // MOOS_IVP_TAK_COT_UTILS_HEADER
