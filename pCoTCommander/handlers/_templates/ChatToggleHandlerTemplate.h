/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  ChatToggleHandlerTemplate.h                    */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- copy this to handlers/{common,aquaticus}/   */
/*  and rename before using. Not registered in the factory; */
/*  not built. See handlers/README.md for full walkthrough. */
/*                                                          */
/*  Use this template when:                                 */
/*    * Your command takes "on" / "off" arguments.          */
/*    * It flips a single MOOS variable (true/false).       */
/*    * No CoT events involved.                             */
/*                                                          */
/*  Reference handlers using this pattern:                  */
/*    common::AvoidHandler                                  */
/*    common::OpregHandler                                  */
/*    aquaticus::UntagHandler                               */
/*    aquaticus::RetryHandler                               */
/*                                                          */
/*  RENAMING CHECKLIST                                      */
/*  ===========================================             */
/*  - Class name:       ChatToggleHandlerTemplate -> Foo    */
/*  - Include guard:    TEMPLATE_CHAT_TOGGLE -> FOO         */
/*  - Namespace:        templ -> common or aquaticus        */
/*  - File header NAME / FILE / DATE                        */
/*  - Keyword in chatKeywords()                             */
/*  - MOOS variable name in handleChat()                    */
/*  - Help line text                                        */
/************************************************************/

#ifndef MOOS_IVP_TAK_TEMPLATE_CHAT_TOGGLE_HANDLER_HEADER
#define MOOS_IVP_TAK_TEMPLATE_CHAT_TOGGLE_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace templ {

class ChatToggleHandlerTemplate : public CoTCommandHandler
{
public:
  ChatToggleHandlerTemplate();
  ~ChatToggleHandlerTemplate() override = default;

  // Identity. Shown in AppCast headers and event logs.
  std::string name() const override { return "Template"; }

  // The keyword(s) this handler responds to. Lowercase.
  // Multiple keywords are allowed (e.g. {"return", "rtb"}).
  std::vector<std::string> chatKeywords() const override;

  // The actual command logic.
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;

  // One line shown by the auto-generated 'help' command.
  // Keep <= 60 chars. Format: "<keyword> [args]  -- desc"
  // Return "" to omit from help.
  std::string              helpLine() const override;

  // AppCast section body. Just append diagnostic lines.
  void appcast(std::string& report) const override;

private:
  // Current toggle state. Default value chosen so behavior
  // is on by default at startup (matches Aquaticus norms).
  bool m_state;

  // Diagnostic counters for AppCast. Help operators see
  // whether the command is being received at all.
  unsigned int m_on_count;
  unsigned int m_off_count;
  unsigned int m_reject_count;
  std::string  m_last_command;
};

} // namespace templ

#endif // MOOS_IVP_TAK_TEMPLATE_CHAT_TOGGLE_HANDLER_HEADER
