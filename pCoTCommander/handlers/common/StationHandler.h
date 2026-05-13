/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StationHandler.h                                */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "station" / "hold" chat command.            */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Orders the addressed vehicle(s) to hold their current   */
/*  position via BHV_StationKeep. As a SIDE EFFECT exits    */
/*  ATAK mode so waypt_atak yields to the station-keep      */
/*  behavior.                                               */
/*                                                          */
/*  Unlike "return", does NOT toggle DEPLOY or              */
/*  MOOS_MANUAL_OVERRIDE -- station-keep is meant to be     */
/*  applicable in both deployed and override states, and    */
/*  the operator manages those independently.               */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    STATION_KEEP+sfx       = true                         */
/*    ATAK_MODE+sfx          = false  (side effect)         */
/*    ATAK_WAYPT_ACTIVE+sfx  = false  (side effect)         */
/*    ATAK_CHAT_OUT (via dm) = "<subject> holding            */
/*                              position."                  */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    station                                               */
/*    hold     (alias)                                      */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_STATION_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_STATION_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class StationHandler : public CoTCommandHandler
{
public:
  StationHandler();
  ~StationHandler() override = default;

  std::string name() const override { return "Station"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_station;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_STATION_HANDLER_HEADER
