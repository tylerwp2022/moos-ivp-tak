/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: HelpHandler.h                                   */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "help" chat command. Auto-generates the     */
/*  help text by iterating every active handler in the      */
/*  registry and concatenating their helpLine() returns.    */
/*  The list is automatically scoped to the active bundle   */
/*  -- vehicle-mode operators see vehicle commands, shore   */
/*  operators see shore commands.                           */
/*                                                          */
/*  =======================================================  */
/*  WHY THIS HANDLER MUST LOAD LAST                         */
/*  =======================================================  */
/*  The dispatcher binds ctx.help_lines() in OnStartUp      */
/*  after all handlers are constructed and configured.      */
/*  HelpHandler reads it at chat dispatch time, so order    */
/*  of construction doesn't matter functionally -- but in   */
/*  the migration plan HelpHandler is the LAST handler to   */
/*  be ported because porting it earlier means its output   */
/*  would be incomplete (missing handlers that haven't      */
/*  been migrated yet).                                     */
/*                                                          */
/*  =======================================================  */
/*  OUTPUT FORMAT                                           */
/*  =======================================================  */
/*  Help is sent as a single multi-line ATAK_CHAT_OUT       */
/*  message. ATAK renders &#10; as a newline in GeoChat;    */
/*  the help text uses literal newlines and the dispatcher  */
/*  -- via ctx.dm -- formats them appropriately.            */
/*                                                          */
/*  Format:                                                 */
/*    "Available commands:"                                 */
/*    "<helpLine 1>"                                        */
/*    "<helpLine 2>"                                        */
/*    "..."                                                 */
/*    "<vehicle> <command>  -- per-vehicle (fleet mode)"    */
/*                                                          */
/*  The last line is appended only in fleet mode -- it      */
/*  explains the vehicle-prefix syntax that the dispatcher  */
/*  handles transparently.                                  */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ATAK_CHAT_OUT (via dm) = help text                    */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    help                                                  */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_HELP_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_HELP_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class HelpHandler : public CoTCommandHandler
{
public:
  HelpHandler();
  ~HelpHandler() override = default;

  std::string name() const override { return "Help"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void appcast(std::string& report) const override;

private:
  unsigned int m_query_count;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_HELP_HANDLER_HEADER
