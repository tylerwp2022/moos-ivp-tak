/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: AttackHandler.h                                 */
/*    DATE: May 13, 2026                                    */
/*                                                          */
/*  Handles the "attack" chat command for the Aquaticus     */
/*  CTF mission.                                            */
/*                                                          */
/*  =======================================================  */
/*  MISSION-SPECIFIC BEHAVIOR                               */
/*  =======================================================  */
/*  In Aquaticus, "attack" assigns the addressed vehicle    */
/*  (or all vehicles) an attacking role by publishing       */
/*  ACTION+sfx = ATTACK_MED. With the optional "easy"       */
/*  modifier, the value becomes ATTACK_E.                   */
/*                                                          */
/*  Other missions claim the same "attack" keyword via      */
/*  their own AttackHandler in handlers/<mission>/ -- e.g.  */
/*  hvt::AttackHandler will publish a different variable    */
/*  with different semantics. The CommandHandlerFactory     */
/*  selects which class to construct based on the mission   */
/*  config line.                                            */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    ACTION+sfx                  = "ATTACK_MED" or         */
/*                                  "ATTACK_E"              */
/*    ATAK_CHAT_OUT (via ctx.dm)  = confirmation or warning */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    attack                  -> ATTACK_MED                 */
/*    attack easy             -> ATTACK_E                   */
/*    attack med              -> ATTACK_MED (explicit)      */
/*                                                          */
/*  =======================================================  */
/*  STATE INTERACTIONS                                      */
/*  =======================================================  */
/*  When vehicle is in ATAK mode (ctx.atak_mode==true and   */
/*  ctx.fleet_mode==false), game role assignments are       */
/*  suppressed by pHelmIvP's behavior conditions. This      */
/*  handler still publishes the ACTION variable -- the role */
/*  will take effect when the operator sends "resume" -- but */
/*  it appends a warning to the confirmation DM so the      */
/*  operator isn't surprised when the vehicle doesn't       */
/*  immediately switch behavior.                            */
/************************************************************/

#ifndef MOOS_IVP_TAK_AQUATICUS_ATTACK_HANDLER_HEADER
#define MOOS_IVP_TAK_AQUATICUS_ATTACK_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace aquaticus {

class AttackHandler : public CoTCommandHandler
{
public:
  AttackHandler();
  ~AttackHandler() override = default;

  // ========================================================
  // Identification
  // ========================================================
  //
  // The handler name is unqualified ("Attack") -- the
  // namespace already disambiguates aquaticus::AttackHandler
  // from hvt::AttackHandler at the C++ level, and AppCast
  // section headers only show one mission's handlers at a
  // time (only one is registered per run).
  std::string name() const override { return "Attack"; }

  // ========================================================
  // Chat dispatch
  // ========================================================
  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  // ========================================================
  // Status
  // ========================================================
  void appcast(std::string& report) const override;

private:
  // Diagnostics
  unsigned int m_count_med;       // attack | attack med
  unsigned int m_count_easy;      // attack easy
  unsigned int m_reject_count;    // unknown modifier

  // Last successful assignment for AppCast visibility.
  // Format: "<target_label> -> ATTACK_MED (from <callsign>)"
  std::string  m_last_assignment;
};

} // namespace aquaticus

#endif // MOOS_IVP_TAK_AQUATICUS_ATTACK_HANDLER_HEADER
