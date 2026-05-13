/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: DeployHandler.cpp                               */
/*    DATE: May 13, 2026                                    */
/************************************************************/

#include <string>

#include "DeployHandler.h"

namespace common {

// ============================================================
// Constructor
// ============================================================
//
// Zero diagnostics. m_last_deploy default-empty -- appcast()
// suppresses the "Last:" line until the first successful
// deploy, so AppCast doesn't show a misleading blank entry.

DeployHandler::DeployHandler()
  : m_deploy_count(0),
    m_reject_count(0)
{}


// ============================================================
// chatKeywords() -- claim the "deploy" word
// ============================================================
//
// Single keyword. The dispatcher's m_chat_index maps "deploy"
// to this handler at startup. If another handler also returns
// "deploy" from chatKeywords(), CoTCommander::buildChatIndex
// fails fast with a duplicate-key error.

std::vector<std::string> DeployHandler::chatKeywords() const
{
  return { "deploy" };
}


// ============================================================
// helpLine() -- one-liner for the help command
// ============================================================
//
// Column alignment: keyword padded to 18 chars, then " -- "
// plus a brief description. HelpHandler concatenates these
// across the registry; consistent padding keeps the column
// aligned in the operator's ATAK chat window.

std::string DeployHandler::helpLine() const
{
  return "deploy            -- start/resume vehicle motion";
}


// ============================================================
// handleChat() -- process a deploy command
// ============================================================
//
// Faithful port of the pre-refactor "deploy" branch in
// CoTCommander::handleChatCommand().
//
// SEQUENCE
//   1. Validate input: cmd must be exactly "deploy" with no
//      trailing arguments. Anything else -> usage DM,
//      reject_count++.
//   2. Publish the deployment trio (DEPLOY, MOOS_MANUAL_
//      OVERRIDE, RETURN). sfx is already resolved by the
//      dispatcher -- this handler does not second-guess it.
//   3. DM the operator a confirmation. target_label is
//      precomputed by the dispatcher:
//        sfx ""        -> "vehicle"
//        sfx "_ALL"    -> "all vehicles"
//        sfx "_X"      -> "x" (lowercase)
//   4. Update per-handler diagnostics (count + last summary).
//   5. Emit a debug log line; visible in AppCast when
//      m_debug is on.
//
// RETURN VALUE
//   true  if the command was successfully published
//   false on malformed input (usage DM already sent)
//   The dispatcher uses the return value for its aggregate
//   m_chat_handled counter and for its [CHAT] event log.

bool DeployHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  // Input validation. The dispatcher matched on first_word,
  // so cmd starts with "deploy" -- but extra args are an
  // error (this command takes none).
  if(msg.cmd != "deploy") {
    ctx.dm("Usage: deploy   (no arguments). Got: \"" +
           msg.cmd + "\"", msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Publish the deployment trio. Order matches the
  // pre-refactor implementation; downstream MOOS apps
  // (pHelmIvP, uFldShoreBroker) do not depend on ordering
  // but consistency aids log comparison during the migration.
  ctx.publish("DEPLOY"               + msg.sfx, "true");
  ctx.publish("MOOS_MANUAL_OVERRIDE" + msg.sfx, "false");
  ctx.publish("RETURN"               + msg.sfx, "false");

  // Confirmation DM.
  ctx.dm("Deploying " + msg.target_label + ".", msg.reply_to);

  // Diagnostics.
  m_deploy_count++;
  m_last_deploy = msg.target_label;
  if(!msg.callsign.empty())
    m_last_deploy += " (from " + msg.callsign + ")";

  ctx.dlog("DeployHandler: deployed " + msg.target_label);
  return true;
}


// ============================================================
// appcast() -- handler section in the AppCast report
// ============================================================
//
// The dispatcher emits a "==== Deploy ====" header before
// calling this; the handler writes only the body, indented
// two spaces and column-aligned to match the rest of
// CoTCommander's report style.
//
// The "Last:" line is suppressed until at least one
// successful command -- showing an empty "Last:" is more
// confusing than omitting the line.

void DeployHandler::appcast(std::string& report) const
{
  report += "  Sent:       " + std::to_string(m_deploy_count) + "\n";
  report += "  Rejected:   " + std::to_string(m_reject_count) + "\n";
  if(!m_last_deploy.empty())
    report += "  Last:       " + m_last_deploy + "\n";
}

} // namespace common
