# pCoTCommander Refactor — File Inventory

**As of:** May 13, 2026
**Status:** Complete. All 18 handlers ported, build infrastructure in place. Ready to upload and build.

---

## Directory layout

```
pCoTCommander/
├── INVENTORY.md                    (this file)
├── CMakeLists.txt                  (updated for handler subfolders)
├── ParsedCoT.h                     POD: parsed CoT event
├── ChatMessage.h                   POD: parsed/resolved chat command
├── CoTUtils.h                      cot::extractAttr() declaration
├── CoTUtils.cpp                    cot::extractAttr() impl (lifted from old code)
├── CommanderContext.h              DI bundle for handlers
├── CommandHandlerFactory.h         Factory contract + bundle definitions
├── CommandHandlerFactory.cpp       Registers all 18 handlers, builds bundles
├── CoTCommander.h                  Slim dispatcher declaration
├── CoTCommander.cpp                Slim dispatcher implementation
└── handlers/
    ├── CoTCommandHandler.h         Abstract base — handler contract
    ├── common/                     Mission-agnostic handlers (11)
    │   ├── DeployHandler.{h,cpp}     deploy
    │   ├── ReturnHandler.{h,cpp}     return | rtb
    │   ├── StationHandler.{h,cpp}    station | hold
    │   ├── PauseHandler.{h,cpp}      pause
    │   ├── AtakHandler.{h,cpp}       atak
    │   ├── ResumeHandler.{h,cpp}     resume
    │   ├── AvoidHandler.{h,cpp}      avoid on|off
    │   ├── OpregHandler.{h,cpp}      opreg on|off
    │   ├── StatusHandler.{h,cpp}     status
    │   ├── WaypointHandler.{h,cpp}   CoT b-m-p-w-GOTO + ATAK_WPT_REACHED
    │   └── HelpHandler.{h,cpp}       help (auto-generated from registry)
    └── aquaticus/                  Aquaticus CTF-specific handlers (7)
        ├── AttackHandler.{h,cpp}     attack [easy|med]
        ├── DefendHandler.{h,cpp}     defend [easy|med]
        ├── PlayHandler.{h,cpp}       play (shore-only)
        ├── StopHandler.{h,cpp}       stop (shore-only)
        ├── UntagHandler.{h,cpp}      untag on|off
        ├── RetryHandler.{h,cpp}      retry on|off
        └── FlagPursuitHandler.{h,cpp}  CoT b-m-p-s-m flag uid match
```

## What's here (47 source files + INVENTORY)

| Tier | Count |
|---|---|
| Top-level headers (.h) | 6 |
| Top-level sources (.cpp) | 3 |
| Handler base | 1 |
| `common/` handlers (h + cpp) | 22 |
| `aquaticus/` handlers (h + cpp) | 14 |
| Build script (CMakeLists.txt) | 1 |
| **Total** | **47 + INVENTORY** |

## What ISN'T in this bundle (preserved unchanged in your existing checkout)

- `main.cpp` — no change needed; same entrypoint
- `CoTCommander_Info.h` — header signatures unchanged
- `pCoTCommander_moos.example` (the OLD original) — superseded by the new `pCoTCommander.moos.example` and the two plug files in this bundle. Keep the new ones; delete the old.

## How to integrate

1. **Back up your current `moos-ivp-tak/src/pCoTCommander/`** somewhere.
2. **Replace its contents** with this bundle's `pCoTCommander/` contents. The four files that change are:
   - `CoTCommander.h` (slim dispatcher, ~317 lines vs. old ~210)
   - `CoTCommander.cpp` (slim dispatcher, ~632 lines vs. old ~1353)
   - `CMakeLists.txt` (adds handler .cpps and include path)
   - Plus all-new files: `CoTUtils.{h,cpp}`, `CommandHandlerFactory.{h,cpp}`, `CommanderContext.h`, `ParsedCoT.h`, `ChatMessage.h`, and the `handlers/` tree.
3. Leave `main.cpp`, `CoTCommander_Info.{h,cpp}`, and your `.moos` plug files untouched.
4. `cmake --build` as normal. The handler subfolder includes resolve via the `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR})` line added in CMakeLists.txt.

## Handler progress: 18 of 18 ported ✅

### `common/` (11)
- Deploy, Return, Station, Pause
- Atak, Resume
- Avoid, Opreg
- Status, Help
- Waypoint (CoT)

### `aquaticus/` (7)
- Attack, Defend
- Play (shore-only), Stop (shore-only)
- Untag, Retry
- FlagPursuit (CoT)

## Bundle composition (assembled by `CommandHandlerFactory`)

**Aquaticus shore** (16 handlers, chat-only):
deploy, return, station, pause, atak, resume, avoid, opreg, attack, defend, play, stop, untag, retry, status, help

**Aquaticus vehicle** (16 handlers, 14 chat + 2 CoT):
deploy, return, station, pause, atak, resume, avoid, opreg, attack, defend, untag, retry, status, waypoint *(CoT)*, flag_pursuit *(CoT)*, help

Mission selection: runtime via `mission = aquaticus` in `.moos` (default).
Role selection: runtime via `command_set = shore | vehicle | custom` (default: derive from `fleet_mode`).
Custom mode: `command_set = custom` plus one `enable_handler = <name>` per desired handler.

## Contract changes from the legacy app (vs. old monolith)

1. **`onMail()` in the handler base class accepts `CommanderContext& ctx`.** Old signature was `(key, value)`. WaypointHandler and FlagPursuitHandler use the ctx to DM the operator and publish reset flags in response to subscribed mail. All other handlers default to no-op (cost: one virtual dispatch per mail item for non-overriding handlers; negligible).

2. **`CommanderContext` has three new fields beyond the base state mirrors:**
   - `command_chatroom` (string, set at startup) — FlagPursuit reads this as DM fallback
   - `last_operator_callsign` (string, mutable scratchpad) — WaypointHandler writes on accept, FlagPursuit and WaypointHandler::onMail read for DM destination
   - `help_lines` (std::function callback) — HelpHandler calls this to assemble help text

3. **New `.moos` config keys** (all with defaults; backward compatible):
   - `command_set = shore | vehicle | custom` (default: derived from `fleet_mode`)
   - `mission = aquaticus` (default; only Aquaticus is currently registered)
   - `enable_handler = <name>` (repeatable; only honored when `command_set = custom`)

## Reference — design docs in this bundle

- **`handlers/CoTCommandHandler.h`** — handler contract, dispatch flow, lifecycle, "adding a new command" workflow
- **`CommandHandlerFactory.h`** — (role, mission) axis, folder organization, bundle composition rules
- **`CommanderContext.h`** — DI bundle, threading rules, mirrored-state semantics
- **`CoTCommander.h`** — dispatcher flow, startup sequence, MOOS interface

The decision journal entry for this work is in `thesis_decision_journal.md` under "May 13, 2026". Next session items: update the journal with the onMail signature change and CommanderContext additions; eventually update `CoTCommander_Info.cpp` and `pCoTCommander_moos.example` to reflect the new config knobs.
