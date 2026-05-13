/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: PlayHandler.h                                   */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "play" chat command -- starts the           */
/*  Aquaticus game.                                         */
/*                                                          */
/*  SHORE-ONLY. The CommandHandlerFactory only includes     */
/*  this in the Aquaticus shore bundle. If it somehow ends  */
/*  up in a vehicle bundle, the handler still works (it     */
/*  just publishes the variable on the wrong DB), but       */
/*  shouldn't happen via the supported config paths.        */
/*                                                          */
/*  =======================================================  */
/*  WHAT IT DOES                                            */
/*  =======================================================  */
/*  Publishes AQUATICUS_GAME_ALL = play -- a fleet-wide     */
/*  game-start signal that uFldShoreBroker routes to the    */
/*  Aquaticus game engine.                                  */
/*                                                          */
/*  =======================================================  */
/*  INPUT VALIDATION                                        */
/*  =======================================================  */
/*  Game control is fleet-wide only -- a vehicle prefix     */
/*  ("blue_one play") is rejected with a usage DM. There    */
/*  is no per-vehicle game state to manipulate.             */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    AQUATICUS_GAME_ALL     = "play"                       */
/*    ATAK_CHAT_OUT (via dm) = "Game started."              */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    play                                                  */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_PLAY_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_PLAY_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class PlayHandler : public CoTCommandHandler
{
public:
  PlayHandler();
  ~PlayHandler() override = default;

  std::string name() const override { return "Play"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_play;
};

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_PLAY_HANDLER_HEADER
