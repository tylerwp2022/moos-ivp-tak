/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: WptModeHandler.h                                */
/*    DATE: May 14, 2026                                    */
/*                                                          */
/*  Chat handler for the "wpt_mode <fast|precise|hold>"     */
/*  command. Sets the persistent waypoint mode that         */
/*  WaypointHandler reads when processing CoT b-m-p-w-GOTO  */
/*  events. The mode determines how the boat behaves at the */
/*  approach to and after reaching the operator's pin.      */
/*                                                          */
/*  =======================================================  */
/*  THE THREE MODES                                         */
/*  =======================================================  */
/*    fast    -- transit at full speed, release on first    */
/*               capture. Boat may coast through the pin.   */
/*               Best for: rapid repositioning, intercepts. */
/*                                                          */
/*    precise -- transit at full speed, decelerate to       */
/*               precise_approach_speed when within         */
/*               (capture_radius + approach_buffer) meters, */
/*               station-keep until sustained low speed.    */
/*               Boat parks ON the pin.                     */
/*               Best for: angle setup, recon, pickup.      */
/*                                                          */
/*    hold    -- same approach as precise. After settling,  */
/*               the bhv stays active forever -- the boat   */
/*               actively returns to the pin if wind or     */
/*               current displaces it. Operator must type   */
/*               'resume' to release.                       */
/*               Best for: defending a fixed point, hold-   */
/*               for-instructions, pickup zone marshaling.  */
/*                                                          */
/*  =======================================================  */
/*  COMMAND FORMS (all routed by dispatcher)                */
/*  =======================================================  */
/*    DM'd directly to a vehicle's chatroom:                */
/*      "wpt_mode precise"   -- sets that boat to precise   */
/*      "wpt_mode"           -- queries current mode        */
/*                                                          */
/*    From shore, global (all vehicles):                    */
/*      "wpt_mode precise"   -- sets all boats to precise   */
/*                                                          */
/*    From shore, per-vehicle (using prefix routing):       */
/*      "blue_one wpt_mode precise" -- sets blue_one only   */
/*                                                          */
/*  =======================================================  */
/*  STATE MODEL                                             */
/*  =======================================================  */
/*  Modes are sticky per vehicle. The handler maintains a   */
/*  local cache of the current mode for its own MOOSDB so   */
/*  "wpt_mode" queries can report it without round-tripping */
/*  through MOOS subscriptions. State is also published to  */
/*  WPT_MODE (vehicle) or WPT_MODE_<vname> (shore) so other */
/*  handlers (WaypointHandler, StatusHandler) can observe.  */
/*                                                          */
/*  Per-shot override: the operator can override the sticky */
/*  mode for a single Go-To by adding "#fast", "#precise",  */
/*  or "#hold" to the CoT remarks/comment field. That logic */
/*  lives in WaypointHandler, not here -- it reads the      */
/*  override tag from the CoT and falls back to the sticky  */
/*  WPT_MODE value if absent.                               */
/*                                                          */
/*  =======================================================  */
/*  MOOS SUBSCRIPTIONS                                      */
/*  =======================================================  */
/*    WPT_MODE  -- shore->vehicle qbridged. Vehicle mode    */
/*                 tracks whatever shore most recently set, */
/*                 OR whatever the operator last DM'd       */
/*                 directly. Shore-mode instance also subs  */
/*                 to its local copies for AppCast.         */
/*                                                          */
/*  =======================================================  */
/*  MOOS PUBLICATIONS                                       */
/*  =======================================================  */
/*    WPT_MODE + sfx -- the new mode string. The bare form  */
/*                       is written on the vehicle for its  */
/*                       own WaypointHandler; the suffixed  */
/*                       form is what shore writes (and the */
/*                       broker qbridge forwards down to    */
/*                       become bare on the vehicle).       */
/*    ATAK_CHAT_OUT via dm -- confirmation/query DM         */
/*                                                          */
/*  =======================================================  */
/*  REQUIRED BROKER BRIDGE                                  */
/*  =======================================================  */
/*    meta_shoreside.moos: qbridge = WPT_MODE               */
/*                                                          */
/*  =======================================================  */
/*  CHAT KEYWORDS                                           */
/*  =======================================================  */
/*    wpt_mode                                              */
/************************************************************/

#ifndef MOOS_IVP_TAK_COMMON_WPT_MODE_HANDLER_HEADER
#define MOOS_IVP_TAK_COMMON_WPT_MODE_HANDLER_HEADER

#include "../CoTCommandHandler.h"

namespace common {

class WptModeHandler : public CoTCommandHandler
{
public:
  WptModeHandler();
  ~WptModeHandler() override = default;

  std::string name() const override { return "WptMode"; }

  std::vector<std::string> chatKeywords() const override;
  bool                     handleChat(const ChatMessage& msg,
                                       CommanderContext& ctx) override;
  std::string              helpLine() const override;

  void configure(const std::string& key,
                  const std::string& value) override;
  void registerSubs(std::vector<std::string>& subs) override;
  void onMail(const std::string& key,
               const std::string& value,
               CommanderContext& ctx) override;

  void appcast(std::string& report) const override;

private:
  // ---- Helpers ----
  // Returns true if mode is "fast" / "precise" / "hold".
  // Case-insensitive.
  bool isValidMode(const std::string& mode) const;

  // ---- Configuration ----
  // The mode applied at startup before any chat command or
  // bridged WPT_MODE has arrived. Should match the default
  // in WaypointHandler so behavior is consistent if a
  // waypoint arrives before any mode has been set.
  std::string m_default_mode;

  // ---- Runtime state ----
  // Current effective mode for this MOOSDB. Vehicle reads
  // bridged WPT_MODE; shore tracks the most recent thing it
  // told the vehicle. Used for "wpt_mode" (no arg) queries.
  std::string m_current_mode;

  // ---- Diagnostics ----
  unsigned int m_count_fast;
  unsigned int m_count_precise;
  unsigned int m_count_hold;
  unsigned int m_count_query;
  unsigned int m_reject_count;
  std::string  m_last_action;
};

} // namespace common

#endif // MOOS_IVP_TAK_COMMON_WPT_MODE_HANDLER_HEADER
