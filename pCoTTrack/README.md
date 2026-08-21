# pCoTTrack

Inbound TAK situational awareness for the pCoT* family. pCoTTrack is the
mirror image of pCoTContact: where pCoTContact turns `NODE_REPORT` into
outbound CoT so MOOS vehicles appear in ATAK, pCoTTrack turns inbound CoT
position reports (PLI) into `NODE_REPORT` so ATAK users appear in MOOS-IvP —
visible in pMarineViewer and consumable by pContactMgrV20 and contact-driven
behaviors (and taggable by uFldTagManager).

## Pipeline

```
TAK server → pCoTBridge → COT_INBOUND → pCoTTrack
  → NODE_REPORT (+ optional VIEW_MARKER) → shore DB
  → uFldNodeComms → NODE_REPORT_<VNAME> → vehicles
```

## What is ingested

Only ground-unit position location information (PLI) — the self-position
broadcast an ATAK/WinTAK client emits for its own operator. The default
match set covers all four affiliations:

| CoT type  | Meaning              |
|-----------|----------------------|
| `a-f-G-U` | friendly ground unit |
| `a-h-G-U` | hostile ground unit  |
| `a-n-G-U` | neutral ground unit  |
| `a-u-G-U` | unknown ground unit  |

Matching is by **prefix**, so `a-f-G-U` also claims the more specific
`a-f-G-U-C-I` (infantry) and `a-f-G-U-C-V` (vehicle) subtypes ATAK emits.
Override with `track_cot_types` (the first explicit entry replaces the
default set rather than appending to it). Everything else on `COT_INBOUND`
— GOTO waypoints, chat, graphics — is ignored here and left to
pCoTCommander and pCoTChat.

A PLI reporting lat=0, lon=0 (an ATAK client with no GPS fix) is discarded
rather than planted in the Gulf of Guinea.

## Loopback suppression

A TAK server echoes back everything this MOOS community publishes. Without
a filter, pCoTContact's own vehicle contacts would return through
`COT_INBOUND` and be republished as duplicate `NODE_REPORT`s. Two
independent guards prevent this:

1. **Type filter** — pCoTContact emits `a-f-S-C-U-N` (surface vessel) and
   pCoTShoreContact emits `a-f-G-E` (ground equipment). Neither is a ground
   *unit*, so neither matches `track_cot_types`.
2. **UID prefix filter** — `ignore_uid_prefix` drops any event whose uid
   begins with a listed string. Defaults to `surveyor-`, the prefix
   pCoTContact stamps on every contact it sends.

Guard 2 is redundant today but survives someone later changing a CoT type
upstream — which is exactly when a feedback loop would otherwise appear.

Ingest can be narrowed further with `ignore_callsign` (exact-match
blocklist) and `callsign_whitelist` (when set, *only* the listed callsigns
are tracked, case-insensitively).

## Node naming

Tracks are keyed on the CoT `uid`, which is stable for the life of an ATAK
install. The MOOS node name is derived from `<contact callsign="...">`
instead, because that is what an operator recognizes — but callsigns are
free text (`Delta 1 (TL)`), so they are sanitized to `[a-z0-9_-]` and given
a configurable prefix (default `atak_`): callsign `Delta 1 (TL)` becomes
node `atak_delta_1_tl`. The prefix keeps TAK-sourced nodes visibly distinct
from vehicles and immune to name collisions.

`vname_map` is the exception: `vname_map = bark:blue_four` posts that
operator's track under the exact vehicle name (no prefix), letting a real
ATAK operator stand in for a standard mission vehicle (e.g. the HVT) with
no mission-side renaming.

If an operator renames themselves in ATAK, the track survives (same uid)
but the node name changes; the rename is surfaced as an app event.

## Team mapping

ATAK operators are players, not spectators: boats must be able to target
and tag them. uFldTagManager rejects any `NODE_REPORT` whose `GROUP` is not
one of its two team names, so each track's `GROUP` is derived from the team
color the operator picked in ATAK (`<__group name="Red">`), folded through
`team_map`. By default red/maroon/magenta → `red` and
blue/dark blue/cyan/teal → `blue`. Unmapped colors fall back to
`node_group` (never the raw color). Set `group_from_team = false` to give
everyone `node_group` instead. The lookup happens per-post, so an operator
who switches teams mid-game moves on their next report.

## Speed and heading

By default, speed and heading come from the CoT `<track speed course>`
element when the client supplies it, and are **derived from consecutive
GPS fixes** when it does not. Derivation refuses fix pairs closer than
0.5 s apart (noise amplifies into absurd speeds) and treats movement under
1 m as standing still (speed 0, previous heading kept).

Two knobs control the source:

- `heading_source = cot | gps` — where **HDG** comes from when the CoT
  does carry `<track course>`:
  - `cot` (default): use the course the ATAK client reports.
  - `gps`: always derive heading from consecutive fixes; the client's
    course is ignored. Speed still comes from the CoT when present.
  Useful when a client's reported course is stale or noisy at walking
  speed — heading derived from actual position deltas is then the more
  trustworthy source.
- `derive_motion_only = true` — the blunt version: ignore `<track>`
  entirely and derive *both* speed and heading from fixes. Overrides
  `heading_source`.

The AppCast shows the source per track: `spd=1.20 (cot) hdg=274.0 (derived)`.

## Staleness and re-posting

Downstream consumers (uFldNodeComms, pContactMgrV20) judge freshness by
`NODE_REPORT` arrival, so pCoTTrack re-posts every live track each Iterate
with a current `TIME` — a track stays alive through lulls in the ATAK
client's reporting, right up until `stale_timeout` (default 30 s without
CoT) deliberately drops it. `NODE_REPORT TIME` is always the local MOOS
clock, never the CoT event time: the sending Android device is a different
clock reference and would make contacts read permanently stale or
impossibly fresh.

`post_interval = 0` (default) additionally posts immediately on every
accepted CoT event, so operator movement shows in pMarineViewer without a
tick of latency.

## MOOS interface

**Subscribes:**

- `COT_INBOUND` — raw CoT XML from pCoTBridge.
- `NODE_REPORT`, `NODE_REPORT_LOCAL` — used only as a geodesy NAV anchor
  (a simultaneous X/Y + LAT/LON pair) for the flat-earth fallback when
  MOOSGeodesy is not compiled in. Reports produced by this app are skipped
  to avoid anchoring on our own output.

**Publishes:**

- `NODE_REPORT` —
  `NAME=atak_delta_1,TYPE=swimmer,TIME=...,X=..,Y=..,LAT=..,LON=..,SPD=..,HDG=..,DEP=0,LENGTH=..,MODE=TAK,VSOURCE=pCoTTrack,COLOR=..,GROUP=<team>`
- `VIEW_MARKER` — only when `publish_view_marker = true`; markers are
  retracted (`active=false`) when a track goes stale or is renamed.

Requires `LatOrigin` / `LongOrigin` in the mission file — the same origin
as the rest of the mission — to place inbound lat/lon on the local grid.

## Configuration parameters

| Parameter             | Default                             | Notes |
|-----------------------|-------------------------------------|-------|
| `node_prefix`         | `atak_`                             | prepended to every node name |
| `node_type`           | `swimmer`                           | pMarineViewer body |
| `node_color`          | `yellow`                            | `NODE_REPORT COLOR=` |
| `node_length`         | `2.0`                               | meters |
| `lowercase_names`     | `true`                              | fold node names to lower case |
| `node_group`          | (unset)                             | fallback `GROUP=`; unset omits the field |
| `group_from_team`     | `true`                              | derive `GROUP` from the ATAK team color |
| `team_map`            | reds→red, blues→blue                | `color:team` pairs, comma separated |
| `vname_map`           | (unset)                             | `callsign:vname` — post as an exact vehicle name |
| `track_cot_types`     | `a-f-G-U,a-h-G-U,a-n-G-U,a-u-G-U`   | CoT type prefixes to accept (case sensitive) |
| `ignore_uid_prefix`   | `surveyor-`                         | loopback suppression |
| `ignore_callsign`     | (unset)                             | exact callsigns to drop |
| `callsign_whitelist`  | (unset = allow all)                 | when set, only these callsigns are tracked |
| `max_tracks`          | `20`                                | cap on concurrent tracks |
| `publish_node_report` | `true`                              | |
| `publish_view_marker` | `false`                             | |
| `marker_type`         | `triangle`                          | |
| `marker_color`        | `yellow`                            | |
| `marker_width`        | `6.0`                               | |
| `post_interval`       | `0.0`                               | min seconds between posts per track; 0 = every event |
| `stale_timeout`       | `30.0`                              | seconds without CoT before the track is dropped |
| `derive_motion_only`  | `false`                             | ignore `<track>`; derive speed *and* heading from fixes |
| `heading_source`      | `cot`                               | `cot` = heading from the CoT `<track course>`; `gps` = derive heading from consecutive fixes |
| `debug`               | `false`                             | ring buffer of debug lines in the AppCast |

## Example configuration

```
ProcessConfig = pCoTTrack
{
  AppTick   = 4
  CommsTick = 4

  node_prefix = atak_
  node_type   = swimmer
  node_color  = yellow

  group_from_team = true
  // team_map     = red:red, maroon:red, cyan:blue, dark blue:blue
  // node_group   = blue
  // vname_map    = bark:blue_four

  track_cot_types   = a-f-G-U,a-h-G-U,a-n-G-U,a-u-G-U
  ignore_uid_prefix = surveyor-
  // callsign_whitelist = delta 1, tyler-atak

  publish_node_report = true
  publish_view_marker = false

  // heading from the CoT <track course> (cot) or derived from
  // consecutive GPS fixes (gps)
  heading_source     = cot
  derive_motion_only = false

  post_interval = 0.0
  stale_timeout = 30.0
  max_tracks    = 20
}
```

Run `pCoTTrack --example` and `pCoTTrack --interface` for the same
information from the binary itself.
