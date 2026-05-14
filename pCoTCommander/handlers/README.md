# Adding a New Command to pCoTCommander

This is the field guide for adding new chat commands or CoT event
handlers to pCoTCommander. The handler-class architecture is designed
to make this a localized change: most of the work happens in one new
`.cpp/.h` pair, plus a few register-it lines. **You should not need to
touch `CoTCommander.cpp` to add a command.**

If you find yourself wanting to modify the dispatcher itself, stop and
re-read this doc — there's almost certainly a way to do what you want
inside a handler.

**Last updated:** May 13, 2026

---

## Quick reference — what kind of handler do I need?

```
START
  │
  ├── "Operator sends a ground-station chat command" ───────────►  Chat handler
  │        │
  │        ├── "On/off toggle" (avoid, untag, retry, opreg) ────►  Use ChatToggleHandlerTemplate
  │        ├── "Single action" (deploy, attack, play) ──────────►  Use ChatCommandHandlerTemplate
  │        └── "Read-only query" (status) ──────────────────────►  Use ChatCommandHandlerTemplate
  │                                                                   (no publications, just a DM)
  │
  ├── "ATAK sends a CoT event" (Go-To, marker, etc.) ───────────►  CoT handler
  │        └── Use CoTEventHandlerTemplate
  │
  └── "Both" (chat command AND CoT event) ──────────────────────►  Implement BOTH halves of
                                                                     CoTCommandHandler — they're
                                                                     independent virtuals.
```

## Where does my handler live?

The folder you pick decides which bundles it can ship in:

- `handlers/common/` — **mission-agnostic.** Deploy, return, station, status, etc. Anything that does the same thing under any mission. These handlers can ship in EVERY bundle. Variable names should be generic (`MY_TOGGLE`, not `ATAK_AQUATICUS_*`).

- `handlers/aquaticus/` — **Aquaticus CTF-specific.** Anything that references game state (attack/defend roles, flag pursuit, play/stop game) goes here. These ship only in Aquaticus bundles.

- **Future:** If you start a second mission (HVT, escort, etc.), make `handlers/<mission>/` and add a new bundle to `CommandHandlerFactory`.

**Rule of thumb:** Could this handler make sense for a completely different mission? → `common/`. Does it only make sense in a CTF context? → `aquaticus/`.

---

## Step-by-step: adding a new command

Suppose you want to add `recall` — a chat command that pulls a single vehicle back to a configurable recovery point.

### 1. Pick a template

This is a single-action chat command — copy `_templates/ChatCommandHandlerTemplate.{h,cpp}` to `common/RecallHandler.{h,cpp}` (it's mission-agnostic — any mission might want to recall a vehicle).

### 2. Find/replace placeholder names

In both files, replace:
- `ChatCommandHandlerTemplate` → `RecallHandler`
- `TEMPLATE_CHAT_COMMAND` (in include guard) → `RECALL`
- The `template` namespace → `common` (or `aquaticus` if you put it there)
- The example keyword "template" → "recall"
- The example help line and DM text → real text

### 3. Implement `handleChat()`

```cpp
bool RecallHandler::handleChat(const ChatMessage& msg,
                                CommanderContext& ctx)
{
  // Validate: don't allow recall if not deployed
  if(!ctx.deployed) {
    ctx.dm("Cannot recall — vehicle is not deployed.", msg.reply_to);
    m_reject_count++;
    return false;
  }

  // Publish what the .bhv / pHelmIvP needs to see
  ctx.publish("RECALL" + msg.sfx,            "true");
  ctx.publish("ATAK_WAYPT_ACTIVE" + msg.sfx, "false");  // clear ATAK waypoint
  ctx.publish("ATAK_MODE" + msg.sfx,         "false");

  // Confirm to operator
  ctx.dm(msg.target_label + " recalling to recovery point.", msg.reply_to);

  m_count++;
  m_last_recall = msg.target_label;
  if(!msg.callsign.empty()) m_last_recall += " (from " + msg.callsign + ")";

  ctx.dlog("RecallHandler: RECALL" + msg.sfx + "=true");
  return true;
}
```

### 4. Register in the factory

Open `CommandHandlerFactory.cpp` and:

```cpp
// Near the top:
#include "handlers/common/RecallHandler.h"

// In buildOne(), in the common section:
if(name == "recall")
  return std::unique_ptr<CoTCommandHandler>(new common::RecallHandler());
```

Then add `"recall"` to the bundles you want it in:

```cpp
// In buildAquaticusShoreBundle() and/or buildAquaticusVehicleBundle()
static const std::vector<std::string> kNames = {
  "deploy", "return", "station", "recall",   // <-- added
  ...
};
```

### 5. Wire into the build

Open `CMakeLists.txt`. In the `SRC` list, under the `common/` section:

```cmake
handlers/common/RecallHandler.cpp     # <-- added
```

### 6. **Update the .bhv file** (if behaviors are involved)

This is the part most likely to bite you. The `.bhv` and the handler are coupled: handlers PUBLISH MOOS variables, behaviors CONSUME them via `condition` lines.

For our recall example, suppose pHelmIvP has a `BHV_Waypoint` named `waypt_recall` that drives the vehicle to a fixed recovery point. You need to:

**a.** Add an `initialize` directive at the top of the `.bhv` so the variable exists before the first tick (otherwise `condition = RECALL = true` is undefined and the behavior won't fire):

```
initialize RECALL = false
```

**b.** Add the `condition` to the recovery behavior:

```
Behavior = BHV_Waypoint
{
  name = waypt_recall
  condition = RECALL = true
  ...
}
```

**c.** If recall is supposed to override other behaviors, add a NEGATIVE condition to them:

```
Behavior = BHV_AttackFlag
{
  ...
  condition = RECALL != true   // <-- new line
}
```

See `pCoTCommander.moos.example`'s embedded BHV_Waypoint block for the canonical waypt_atak example — that's a worked instance of this same pattern for the existing waypoint handler.

### 7. Update the plug file (if config is needed)

If your handler reads config keys via `configure()`, add the keys to:

- `plug_pCoTCommander_shore.moos` (if it ships in the shore bundle)
- `plug_pCoTCommander_vehicle.moos` (if it ships in the vehicle bundle)
- `pCoTCommander.moos.example` (always — this is the reference)

Handlers ignore unknown keys silently, so older configs still work. New keys should have sensible defaults so blank configs also work.

### 8. Update `CoTCommander_Info.cpp` (optional but nice)

If your handler adds new MOOS publications or subscriptions, add them to the SUBSCRIPTIONS / PUBLICATIONS lists in `showInterfaceAndExit()`. Future-Tyler running `pCoTCommander --interface` will thank you.

`help` is auto-generated from the registry, so you don't need to touch it.

### 9. Build, run, test

```sh
cd ~/moos-ivp-tak && ./build.sh
```

In a sim, send the chat command from ATAK or `uPokeDB ATAK_CHAT_IN`:

```
uPokeDB targ_blue_one.moos ATAK_CHAT_IN="callsign=Tyler,chatroom=blue_one,message=recall"
```

Check appcast (`uMS pCoTCommander`) — your handler should appear with its own `==== Recall ====` section.

### 10. Journal it

Add an entry to `thesis_decision_journal.md` noting:
- The new handler and its purpose
- Which bundles it ships in
- Any new MOOS variables or `.bhv` requirements
- Any contract changes (rare — most handlers just compose)

---

## Pattern catalog

### Toggle (`MyToggleHandler`)
Use when the command takes "on" / "off" and flips a single MOOS variable.

**Reference handlers:** `common::AvoidHandler`, `common::OpregHandler`, `aquaticus::UntagHandler`, `aquaticus::RetryHandler`

**Template:** `_templates/ChatToggleHandlerTemplate.{h,cpp}`

**Typical state:** current on/off, on/off counters, reject counter, last command record.

### Single action (`MyCommandHandler`)
Use when the command fires once and posts a fixed set of MOOS variables. May validate against context state (deployed, atak_mode, etc.) and reject with a DM.

**Reference handlers:** `common::DeployHandler`, `common::ReturnHandler`, `common::StationHandler`, `common::AtakHandler`, `aquaticus::AttackHandler`, `aquaticus::PlayHandler`

**Template:** `_templates/ChatCommandHandlerTemplate.{h,cpp}`

### CoT event handler
Use when the trigger is an inbound CoT XML payload from ATAK (or another CoT producer like pCoTGraphics).

**Reference handlers:** `common::WaypointHandler` (operator Go-To), `aquaticus::FlagPursuitHandler` (referee flag broadcast)

**Template:** `_templates/CoTEventHandlerTemplate.{h,cpp}`

**Key methods:** `claimsCoT()` (lightweight predicate — type match, optional uid match, position present?) and `handleCoT()` (the actual work).

**Note on the operator-UID filter:** the dispatcher applies a substring match against the configured `operator_uid_filter` before calling `claimsCoT()`. If your handler legitimately receives CoT from non-operator sources (e.g. the Aquaticus referee, pCoTGraphics), override `bypassOperatorFilter()` to return true.

---

## Common questions

### My handler needs to read DEPLOY/ATAK_MODE/TAGGED. Do I subscribe?

**No.** Those are dispatcher-mirrored. Read them from `ctx`:

```cpp
if(!ctx.deployed) {
  ctx.dm("Not deployed.", msg.reply_to);
  return false;
}
```

`ctx.atak_mode`, `ctx.tagged`, `ctx.atak_retry` are also there. The dispatcher updates them automatically.

### My handler needs to subscribe to a unique variable.

Override `registerSubs()` and `onMail()`:

```cpp
void MyHandler::registerSubs(std::vector<std::string>& subs)
{
  subs.push_back("MY_FANCY_VARIABLE");
}

void MyHandler::onMail(const std::string& key,
                        const std::string& value,
                        CommanderContext& ctx)
{
  if(key != "MY_FANCY_VARIABLE") return;
  // do whatever
}
```

The default `onMail` is a no-op so handlers that don't override pay nothing.

### My handler needs to read configuration from .moos.

Override `configure()`:

```cpp
void MyHandler::configure(const std::string& key,
                          const std::string& value)
{
  std::string k = tolower(key);
  if(k == "my_radius") {
    double r = atof(value.c_str());
    if(r > 0.0) m_radius = r;
  }
}
```

`configure()` is called once per `.moos` ProcessConfig line. Unknown keys are silently ignored — your handler doesn't see (or care about) other handlers' keys.

### My handler needs to know who the operator is (for a DM).

The chat handler gets `msg.callsign` and `msg.reply_to` directly. For CoT handlers, the operator callsign is extracted from `<link parent_callsign="...">` if present, and the dispatcher stashes it as `ctx.last_operator_callsign` (currently only WaypointHandler writes this, but you can read it from anywhere).

For onMail reactions (no operator context), use `ctx.last_operator_callsign` if set, else `ctx.command_chatroom` as a fallback. See FlagPursuitHandler for an example.

### Can two handlers claim the same CoT?

No — first claim wins. Handlers are iterated in registry order. Order matters if two could match the same event. Put more specific handlers earlier in the bundle list in `CommandHandlerFactory.cpp`.

### Can two handlers register the same chat keyword?

No — this is a fatal startup error. `buildChatIndex()` aborts launch with a clear error message. Either:
- Rename one of the keywords
- Add an alias (a handler can declare multiple keywords in `chatKeywords()`)

### My handler is huge / complex. Should I break it up?

Probably yes. If a handler has many states or major branches, decompose into:
- One handler-per-command (preferred: matches the existing pattern)
- A helper class the handler owns

Don't subclass `CoTCommandHandler` to share state — handlers are unique instances owned by the registry.

---

## Anti-patterns (don't do these)

❌ **Don't do MOOS work in the handler constructor.** Constructors run before MOOS is connected. Initialize state from defaults; let `configure()` and `onMail()` do the rest.

❌ **Don't `Notify()` from inside a handler.** Use `ctx.publish()`. The lambda routes back to the dispatcher's `Notify`; this keeps handlers testable in isolation.

❌ **Don't read MOOS variables directly.** Subscribe via `registerSubs()` and react in `onMail()`. The MOOS API isn't visible to handlers by design.

❌ **Don't subscribe to DEPLOY, ATAK_MODE, TAGGED, or ATAK_RETRY in `registerSubs()`.** The dispatcher mirrors these into `ctx` for you. Subscribing creates a duplicate and you'll miss your own state changes.

❌ **Don't put MOOS headers (`MOOS/libMOOS/...`) in handler `.h` files.** Keep them in `.cpp` only. Handlers should be testable without a MOOS install.

❌ **Don't write to `ctx.deployed` / `ctx.atak_mode` / etc.** Those are dispatcher-owned. The only mutable ctx field handlers may write is `ctx.last_operator_callsign`.

❌ **Don't bypass the registry.** If you find yourself adding a special case to `CoTCommander.cpp` to handle a specific keyword or CoT type, stop. Make a handler for it instead.

---

## Checklist for adding a new command

- [ ] Decide bundle: `common/` or `aquaticus/`?
- [ ] Decide pattern: toggle, single-action, CoT, or composite?
- [ ] Copy the right template, rename class + include guard + namespace
- [ ] Implement `handleChat()` / `handleCoT()` / `helpLine()` / `appcast()`
- [ ] Optional: `configure()`, `registerSubs()`, `onMail()`, `bypassOperatorFilter()`
- [ ] Add to `CommandHandlerFactory.cpp`:
   - `#include` line
   - Entry in `buildOne()`
   - Entry in the relevant bundle list(s)
- [ ] Add `.cpp` to `CMakeLists.txt` SRC
- [ ] Build clean
- [ ] **`.bhv` changes** (if any behavior is involved):
   - `initialize MY_VAR = <default>` near the top
   - `condition = MY_VAR = true` / `!= true` on relevant `Behavior` blocks
   - New `Behavior` blocks if needed
- [ ] Plug file additions (if config keys are introduced)
- [ ] `CoTCommander_Info.cpp` SUBSCRIPTIONS / PUBLICATIONS (optional)
- [ ] Test in sim with `uPokeDB` or ATAK
- [ ] Verify the new handler section appears in `uMS pCoTCommander`
- [ ] Journal entry in `thesis_decision_journal.md`

---

## Template files

Three templates in `handlers/_templates/`:

| Template | Use when |
|---|---|
| `ChatToggleHandlerTemplate.{h,cpp}` | Command takes "on" / "off" and flips a MOOS variable. |
| `ChatCommandHandlerTemplate.{h,cpp}` | Command fires once with a fixed publication set. |
| `CoTEventHandlerTemplate.{h,cpp}` | Trigger is an inbound CoT XML event. |

They compile as-is (they implement a fake `template` handler) but aren't registered in the factory, so they don't ship in any bundle. Copy them to `common/` or `aquaticus/`, rename, then register.

The `_templates/` directory is excluded from `CMakeLists.txt` so the templates themselves never get built into the binary.
