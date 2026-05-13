/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: ChatMessage.h                                   */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  POD struct representing a parsed and resolved chat      */
/*  command from ATAK GeoChat.                              */
/*                                                          */
/*  LIFECYCLE                                               */
/*  -------------------------------------------------------- */
/*    1. ATAK_CHAT_IN arrives on the MOOSDB as              */
/*         "callsign=X,chatroom=Y,message=Z"                */
/*    2. Dispatcher splits on the (sentinel) delimiters     */
/*       ",chatroom=" and ",message=" so commas inside the  */
/*       message text don't break parsing.                  */
/*    3. Chatroom is verified against the configured        */
/*       command_chatroom -- non-matching rooms are dropped */
/*       before a ChatMessage is built.                     */
/*    4. Message text is lowercased and trimmed.            */
/*    5. (Fleet mode only) The first word is checked        */
/*       against the keyword index. If it's not a known     */
/*       keyword, it's treated as a vehicle name prefix:    */
/*         "blue_one atak"  -> target_vehicle = blue_one    */
/*                             sfx            = _BLUE_ONE   */
/*                             cmd            = atak        */
/*                             first_word     = atak        */
/*    6. The dispatcher looks up first_word in m_chat_index */
/*       and calls the matched handler's handleChat().      */
/*                                                          */
/*  Handlers receive the resolved form -- they never need   */
/*  to re-parse vehicle prefixes or switch on fleet vs.     */
/*  vehicle suffix conventions. The canonical pattern is:   */
/*                                                          */
/*      ctx.publish("DEPLOY" + msg.sfx, "true");            */
/*                                                          */
/*  ...which produces:                                      */
/*    vehicle mode    -> DEPLOY=true                        */
/*    fleet, no targ. -> DEPLOY_ALL=true                    */
/*    fleet, targeted -> DEPLOY_BLUE_ONE=true               */
/*                                                          */
/*  TARGET LABEL CONVENTION                                 */
/*  -------------------------------------------------------- */
/*  target_label is precomputed for confirmation DMs:       */
/*    sfx ""        -> target_label "vehicle"               */
/*    sfx "_ALL"    -> target_label "all vehicles"          */
/*    sfx "_X"      -> target_label "x" (lowercased)        */
/*  So handlers can write:                                  */
/*    ctx.dm("Returning " + msg.target_label + " to base.", */
/*           msg.reply_to);                                 */
/*  ...without re-deriving the label from sfx.              */
/************************************************************/

#ifndef MOOS_IVP_TAK_CHAT_MESSAGE_HEADER
#define MOOS_IVP_TAK_CHAT_MESSAGE_HEADER

#include <string>

struct ChatMessage
{
  // --------------------------------------------------------
  // Raw parsed fields
  // --------------------------------------------------------

  // ATAK device or operator callsign (the sender). May be
  // empty if the chat client omitted it -- in that case
  // reply_to falls back to "All Chat Rooms".
  std::string callsign;

  // Chatroom the message was sent in. Already verified to
  // match the configured command_chatroom by the time the
  // ChatMessage reaches a handler -- handlers can ignore it.
  // (Kept on the struct for AppCast / debug visibility.)
  std::string chatroom;

  // Where confirmation and error DMs should be sent. Equal
  // to callsign if non-empty, otherwise "All Chat Rooms".
  // Handlers pass this as the second arg to ctx.dm().
  std::string reply_to;

  // --------------------------------------------------------
  // Resolved command
  // --------------------------------------------------------

  // The command portion of the message, lowercased and
  // trimmed, with any vehicle prefix already stripped.
  // Example: input "Blue_One Attack Easy" yields
  //   cmd = "attack easy"
  //   target_vehicle = "blue_one"
  std::string cmd;

  // First word of cmd -- the keyword used for handler
  // lookup in CoTCommander::m_chat_index. Example:
  //   cmd = "avoid off"
  //   first_word = "avoid"
  std::string first_word;

  // MOOS variable suffix for publication. Handlers append
  // this to base variable names to produce the correct
  // destination:
  //   vehicle mode    -> sfx = ""             DEPLOY
  //   fleet, no targ. -> sfx = "_ALL"         DEPLOY_ALL
  //   fleet, targeted -> sfx = "_BLUE_ONE"    DEPLOY_BLUE_ONE
  std::string sfx;

  // Human-readable label for the target, used in
  // confirmation DMs: "vehicle", "all vehicles", or
  // "blue_one" etc.
  std::string target_label;

  // Resolved target vehicle name in lowercase (e.g.
  // "blue_one"), or empty for fleet-wide or vehicle-mode
  // messages. Provided for handlers that need to look up
  // per-vehicle state, not for variable-name composition
  // (use sfx for that).
  std::string target_vehicle;
};

#endif // MOOS_IVP_TAK_CHAT_MESSAGE_HEADER
