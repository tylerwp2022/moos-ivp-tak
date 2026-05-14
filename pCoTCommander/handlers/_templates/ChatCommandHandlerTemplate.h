/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  ChatCommandHandlerTemplate.h                   */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- copy this to handlers/{common,aquaticus}/   */
/*  and rename before using. Not registered in the factory; */
/*  not built. See handlers/README.md for full walkthrough. */
/*                                                          */
/*  Use this template when:                                 */
/*    * Your command takes no argument or fixed-vocabulary  */
/*      arguments (e.g. "attack easy" vs "attack med").     */
/*    * It fires once and posts a fixed set of MOOS vars.   */
/*    * No on/off toggling.                                 */
/*                                                          */
/*  Reference handlers using this pattern:                  */
/*    common::DeployHandler                                 */
/*    common::ReturnHandler                                 */
/*    common::StationHandler                                */
/*    common::AtakHandler                                   */
/*    common::ResumeHandler                                 */
/*    common::PauseHandler                                  */
/*    common::StatusHandler  (read-only variant -- no       */
/*                            publication, just a DM)       */
/*    aquaticus::AttackHandler                              */
/*    aquaticus::DefendHandler                              */
/*    aquaticus::PlayHandler                                */
/*    aquaticus::StopHandler                                */
/*                                                          */
/*  RENAMING CHECKLIST                                      */
/*  ===========================================             */
/*  - Class name:       ChatCommandHandlerTemplate -> Foo   */
/*  - Include guard:    TEMPLATE_CHAT_COMMAND -> FOO        */
/*  - Namespace:        templ -> common or aquaticus        */
/*  - File header NAME / FILE / DATE                        */
/*  - Keyword in chatKeywords()                             */
/*  - MOOS variable name(s) in handleChat()                 */
/*  - Help line text                                        */
/*  - Optional precondition checks (ctx.deployed, etc.)     */
/************************************************************/

#ifndef MOOS_IVP_TAK_TEMPLATE_CHAT_COMMAND_HANDLER_HEADER
#define MOOS_IVP_TAK_TEMPLATE_CHAT_COMMAND_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace templ {

class ChatCommandHandlerTemplate : public CoTCommandHandler
{
public:
  ChatCommandHandlerTemplate();
  ~ChatCommandHandlerTemplate() override = default;

  std::string name() const override { return "Template"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  // Diagnostic counters. Adjust to your command's shape --
  // e.g. attack/defend track count per difficulty level.
  unsigned int m_count;
  unsigned int m_reject_count;
  std::string  m_last_action;
};

} // namespace templ

#endif // MOOS_IVP_TAK_TEMPLATE_CHAT_COMMAND_HANDLER_HEADER
