/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: StopHandler.h                                   */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "stop" chat command -- pauses the           */
/*  Aquaticus game.                                         */
/*                                                          */
/*  Direct mirror of PlayHandler: shore-only, fleet-wide,   */
/*  rejects vehicle-prefix forms. Publishes "pause"         */
/*  instead of "play".                                      */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    AQUATICUS_GAME_ALL     = "pause"                      */
/*    ATAK_CHAT_OUT (via dm) = "Game stopped."              */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    stop                                                  */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_STOP_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_STOP_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class StopHandler : public CoTCommandHandler
{
public:
  StopHandler();
  ~StopHandler() override = default;

  std::string name() const override { return "Stop"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_stop;
};

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_STOP_HANDLER_HEADER
