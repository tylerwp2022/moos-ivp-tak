/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTUtils.cpp                                    */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Implementation of cot::extractAttr(). Logic preserved   */
/*  verbatim from the pre-refactor                          */
/*  CoTCommander::extractAttr() so that handler and         */
/*  dispatcher behavior on real CoT XML is bit-identical    */
/*  before vs. after the refactor.                          */
/************************************************************/

#include <string>

#include "CoTUtils.h"

namespace cot {

std::string extractAttr(const std::string& xml,
                         const std::string& attr)
{
  // Search for the attribute name followed by '=' and a
  // quote (either ' or "). Returns the first match -- nested
  // elements with the same attribute name yield the
  // outermost.
  std::string needle = attr + "=";
  size_t pos = xml.find(needle);
  if(pos == std::string::npos) return "";

  pos += needle.size();
  if(pos >= xml.size()) return "";

  char quote = xml[pos];
  if(quote != '\'' && quote != '"') return "";  // malformed

  size_t start = pos + 1;
  size_t end   = xml.find(quote, start);
  if(end == std::string::npos) return "";

  return xml.substr(start, end - start);
}

} // namespace cot
