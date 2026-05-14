/************************************************************/
/*    NAME:  <your name>                                    */
/*    ORGN:  West Point Robotics Research Center, USMA      */
/*    FILE:  CoTEventHandlerTemplate.h                      */
/*    DATE:  <date>                                         */
/*                                                          */
/*  TEMPLATE -- copy this to handlers/{common,aquaticus}/   */
/*  and rename before using. Not registered in the factory; */
/*  not built. See handlers/README.md for full walkthrough. */
/*                                                          */
/*  Use this template when:                                 */
/*    * The trigger is an inbound CoT XML payload from ATAK */
/*      (or another CoT source like pCoTGraphics).           */
/*    * You want to react to a specific CoT type and/or uid.*/
/*    * Lat/lon conversion to local XY may be required.     */
/*    * You may need additional MOOS subscriptions.         */
/*                                                          */
/*  Reference handlers using this pattern:                  */
/*    common::WaypointHandler                                */
/*       - Operator Go-To CoT (b-m-p-w-GOTO)                */
/*       - Subscribes to ATAK_WPT_REACHED                   */
/*       - Reads config: capture_radius, etc.               */
/*    aquaticus::FlagPursuitHandler                          */
/*       - Referee flag-position CoT (b-m-p-s-m + uid)      */
/*       - Subscribes to HAS_FLAG_<TEAM>_* for termination  */
/*       - Bypasses the operator-UID filter                  */
/*                                                          */
/*  RENAMING CHECKLIST                                      */
/*  ===========================================             */
/*  - Class name:       CoTEventHandlerTemplate -> Foo      */
/*  - Include guard:    TEMPLATE_COT_EVENT -> FOO           */
/*  - Namespace:        templ -> common or aquaticus        */
/*  - File header NAME / FILE / DATE                        */
/*  - CoT type in claimsCoT()                                */
/*  - Optional uid match in claimsCoT()                      */
/*  - Lat/lon -> XY conversion in handleCoT() (if needed)   */
/*  - MOOS publications in handleCoT()                       */
/*  - Config keys in configure() (if any)                    */
/*  - Subscriptions in registerSubs() (if any)               */
/*  - onMail() handling (if subscribing)                     */
/*  - bypassOperatorFilter() (if event is not from operator) */
/************************************************************/

#ifndef MOOS_IVP_TAK_TEMPLATE_COT_EVENT_HANDLER_HEADER
#define MOOS_IVP_TAK_TEMPLATE_COT_EVENT_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace templ {

class CoTEventHandlerTemplate : public CoTCommandHandler
{
public:
  CoTEventHandlerTemplate();
  ~CoTEventHandlerTemplate() override = default;

  std::string name() const override { return "TemplateCoT"; }

  // ---- CoT side -- the two key virtuals ----

  // Predicate: does this handler want the event? Should
  // be FAST (called once per inbound CoT before any handler
  // gets to do work). Check type, optional uid, position
  // presence -- not state. State checks belong in
  // handleCoT.
  bool claimsCoT(const ParsedCoT& evt) const override;

  // Do the work. Return true on success (counts as
  // "handled" for dispatcher diagnostics), false on
  // any rejection. First-claim-wins: subsequent handlers
  // will NOT see this event.
  bool handleCoT(const ParsedCoT& evt,
                  CommanderContext& ctx) override;

  // Override if this handler legitimately receives CoT
  // from non-operator sources (e.g. pCoTGraphics, referee).
  // Default returns false, which means the dispatcher's
  // operator_uid_filter substring check applies. See
  // aquaticus::FlagPursuitHandler for an example.
  //
  // bool bypassOperatorFilter() const override { return true; }

  // ---- Optional lifecycle hooks ----

  // Called once per .moos ProcessConfig line. Ignore keys
  // you don't recognize -- other handlers will consume them.
  void configure(const std::string& key,
                  const std::string& value) override;

  // Append MOOS variable names you want to subscribe to.
  // Called once at startup. The dispatcher dedups across
  // handlers, so safe to list a var another handler also
  // wants.
  void registerSubs(std::vector<std::string>& subs) override;

  // Called for each piece of mail on a subscribed var.
  // Handler self-filters by checking key.
  void onMail(const std::string& key,
               const std::string& value,
               CommanderContext& ctx) override;

  // ---- AppCast ----
  void appcast(std::string& report) const override;

private:
  // ---- Configuration ----
  // (Read from .moos in configure())
  std::string m_my_uid_match;   // optional uid substring
  double      m_my_param;       // example numeric param

  // ---- Runtime state ----
  // (Updated in handleCoT / onMail)

  // ---- Diagnostics ----
  unsigned int m_count;
  unsigned int m_reject_not_deployed;
  unsigned int m_reject_no_geodesy;
  std::string  m_last_event;
};

} // namespace templ

#endif // MOOS_IVP_TAK_TEMPLATE_COT_EVENT_HANDLER_HEADER
