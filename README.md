# moos-ivp-tak

MOOS-IvP applications for bridging autonomous surface vehicles to the
[TAK (Team Awareness Kit)](https://tak.gov) ecosystem via the
[Cursor-on-Target (CoT)](https://www.mitre.org/sites/default/files/pdf/09_4937.pdf)
messaging protocol.

Developed at the West Point Robotics Research Center (USMA) for the
Project Aquaticus / Maritime Capture-the-Flag (MCTF) testbed.

---

## Apps

| App | Description |
|-----|-------------|
| `lib_cot_geodesy` | Shared library — local XY ↔ WGS84 lat/lon conversion |
| `pCoTBridge` | CoT transport — maintains TCP/TLS connection to TAK server, forwards `COT_OUTBOUND` to TAK, publishes `COT_INBOUND` from TAK |
| `pCoTCommander` | Inbound command dispatcher — translates ATAK "Go To" waypoints and GeoChat role commands into MOOS-IvP behavior updates |
| `pCoTGraphics` | VIEW_* renderer — converts `VIEW_POINT`, `VIEW_SEGLIST`, `VIEW_POLYGON`, `VIEW_MARKER`, `FLAG_SUMMARY`, `UTM_ZONE_*`, and `VIEW_TEXTBOX` to CoT graphics visible on ATAK map |
| `pCoTChat` | GeoChat bridge — sends/receives ATAK chat messages via MOOS variables; announces CTF game events (scores, tags, flag grabs) to operator |

### Architecture

```
                    TAK Server
                        │
                   pCoTBridge          ← TCP/TLS transport (plain or mTLS)
                  (COT_INBOUND /
                   COT_OUTBOUND)
                        │
          ┌─────────────┼──────────────┬─────────────┐
          │             │              │             │
   pCoTGraphics   pCoTCommander   pCoTChat      pCoT* (future)
   VIEW_* → CoT   CoT → MOOS      MOOS ↔ chat
```

All apps communicate through two MOOS variables:
- `COT_OUTBOUND` — any app → pCoTBridge → TAK server
- `COT_INBOUND` — TAK server → pCoTBridge → any app

### Deployment

All pCoT apps run on the **shoreside** MOOS community. The shoreside
MOOSDB already receives `NODE_REPORT` for all vehicles via
`uFldNodeBroker`, making it the natural hub for TAK integration.

```
Shoreside MOOSDB
  ├── NODE_REPORT_* (all vehicles, bridged from uFldNodeBroker)
  ├── UTM_ZONE_ONE/TWO  (uFldTagManager)
  ├── FLAG_SUMMARY      (uFldFlagManager)
  ├── VIEW_POLYGON      (uFldFlagManager)
  ├── VIEW_MARKER       (uFldFlagManager)
  ├── VIEW_TEXTBOX      (uFldFlagManager — score at XE position)
  ├── RED_SCORES / BLUE_SCORES (uFldFlagManager)
  └── COT_OUTBOUND / COT_INBOUND (pCoTBridge ↔ TAK)
```

Commands from ATAK reach individual vehicles via `uFldShoreBroker`
qbridge routing. Add to `uFldShoreBroker` in `meta_shoreside.moos`:

```
qbridge = ATAK_ACTIVE      // L4: gates waypt_atak on target vehicle
qbridge = ATAK_WPT_UPDATE  // L4: waypoint target for waypt_atak
```

`pCoTCommander` posts per-vehicle variables (e.g.,
`ATAK_ACTIVE_BLUE_ONE=true`) which `uFldShoreBroker` routes as
`ATAK_ACTIVE=true` to `blue_one`'s MOOSDB.

---

## Dependencies

### Required
- **MOOS-IvP** — autonomy framework  
  https://oceanai.mit.edu/moos-ivp  
  Tested with MOOS-IvP July 2024 release.

- **OpenSSL** — TLS support for pCoTBridge port 8089  
  ```bash
  sudo apt install libssl-dev   # Ubuntu/Debian
  ```

### Optional
- **MOOSGeodesy** — UTM coordinate projection (included in MOOS-IvP)  
  Falls back to flat-earth approximation if unavailable.

---

## Installation

### 1. Clone the repository

```bash
cd ~/moos-ivp/ivp/src
git clone https://github.com/yourusername/moos-ivp-tak.git
```

Or copy the app directories directly into `~/moos-ivp/ivp/src/`:

```
~/moos-ivp/ivp/src/
├── lib_cot_geodesy/
├── pCoTBridge/
├── pCoTCommander/
├── pCoTGraphics/
└── pCoTChat/
```

### 2. Register apps with MOOS-IvP build system

Add to `~/moos-ivp/ivp/src/CMakeLists.txt`:

```cmake
# In IVP_NON_GUI_LIBS:
lib_cot_geodesy

# In ROBOT_APPS:
pCoTBridge
pCoTCommander
pCoTGraphics
pCoTChat
```

### 3. Build

```bash
cd ~/moos-ivp
./build.sh
```

Binaries are installed to `~/moos-ivp/bin/`.

---

## Configuration

### pCoTBridge

Runs on shoreside in multi-vehicle mode. Reads `NODE_REPORT` for all
vehicles from the shoreside MOOSDB and publishes each as an SA contact
to ATAK. Blue team appears as friendly contacts, red as hostile.

```
ProcessConfig = pCoTBridge
{
  AppTick   = 4
  CommsTick = 4

  tak_host    = 192.168.0.38
  tak_port    = 8088            // 8088 = plain TCP, 8089 = mTLS

  // Multi-vehicle shoreside mode (thesis CTF experiment)
  own_vehicles      = blue_one,blue_two,blue_three
  hostile_vehicles  = red_one,red_two,red_three

  // Single-vehicle mode (on the robot)
  // own_vehicle = alpha

  // mTLS — required when tak_port = 8089
  // tls_cert_file = /path/to/certs/shoreside.pem
  // tls_key_file  = /path/to/certs/shoreside.key
  // tls_ca_file   = /path/to/certs/shoreside-trusted.pem
  // tls_key_pass  = atakatak

  moving_send_interval     = 1.0
  stationary_send_interval = 3.0
  cot_delimiter            = newline

  // Coordinate origin — must match plug_origin_warp.moos
  lat_origin = 41.34928     // Lake Popolopen (West Point, NY)
  lon_origin = -74.063645
}
```

### pCoTCommander

Processes inbound CoT from ATAK and routes commands to specific
vehicles via the `uFldShoreBroker` qbridge mechanism.

**L2 Tactical Control** — operator sends GeoChat role command:
```
"blue_one=attack"   →  ACTION_BLUE_ONE = ATTACK_MED
"blue_two=defend"   →  ACTION_BLUE_TWO = DEFEND_MED
"status"            →  pCoTCommander sends score + role summary to ATAK
```

**L4 Teleoperation** — operator sends ATAK "Go To" for a vehicle:
```
b-m-p-w-GOTO CoT  →  ATAK_ACTIVE_BLUE_ONE = true
                      ATAK_WPT_UPDATE_BLUE_ONE = point=x,y
```

`uFldShoreBroker` routes these (suffix stripped) to the correct
vehicle MOOSDB where `waypt_atak` fires.

```
ProcessConfig = pCoTCommander
{
  AppTick   = 4
  CommsTick = 4

  managed_vehicles = blue_one,blue_two,blue_three

  capture_radius = 15.0       // must match waypt_atak in meta_surveyor.bhv
  ack_chatroom   = All Chat Rooms

  lat_origin = 41.34928
  lon_origin = -74.063645
}
```

Requires `waypt_atak` and `atak_station_keep` behaviors in
`meta_surveyor.bhv`:

```
initialize ATAK_ACTIVE = false

Behavior = BHV_Waypoint
{
  name         = waypt_atak
  pwt          = 150          // beats BHV_RLAgent (50) when ATAK_ACTIVE=true
  perpetual    = true
  condition    = (MODE == CONTROLLED)
  condition    = (ATAK_ACTIVE == true)
  updates      = ATAK_WPT_UPDATE
  endflag      = ATAK_WPT_REACHED = true
  speed        = 3.0
  capture_radius = 15.0
  slip_radius    = 30.0
  point        = 0,0          // always overridden by ATAK_WPT_UPDATE
}

Behavior = BHV_StationKeep
{
  name      = atak_station_keep
  pwt       = 100             // holds position when ATAK_ACTIVE=false
  condition = (MODE == CONTROLLED)
  condition = (ATAK_ACTIVE == false)
  center_activate = true
}
```

Autonomy levels:
- **L0 Full Autonomy** — no ATAK involvement; vehicle runs `BHV_RLAgent`
- **L2 Tactical Control** — GeoChat sets `ACTION` (attack/defend); existing behaviors execute
- **L4 Teleoperation** — GoTo CoT sets `ACTION=CONTROL` + `ATAK_ACTIVE=true`; `waypt_atak` fires

Also requires `bridge = src=ATAK_WPT_REACHED` in each vehicle's
`uFldNodeBroker` block so waypoint completion signals reach the shore.

### pCoTGraphics

Converts MOOS VIEW_* variables to CoT graphics displayed on the ATAK
map. Runs on shoreside where all game state variables are available.

**What appears in ATAK:**

| MOOS Variable | Source | ATAK Display |
|---|---|---|
| `UTM_ZONE_ONE` | uFldTagManager | Red zone boundary (pink filled polygon) |
| `UTM_ZONE_TWO` | uFldTagManager | Blue zone boundary (light-blue filled polygon) |
| `VIEW_POLYGON` | uFldFlagManager | Flag grab zone circles (grey filled circles) |
| `FLAG_SUMMARY` | uFldFlagManager | Red + blue flag markers (colored spot icons) |
| `VIEW_MARKER` | uFldFlagManager | Flag state update on grab/return |
| `VIEW_TEXTBOX` | uFldFlagManager | Score label "RED:0 BLUE:0" at midfield |
| `VIEW_POINT` | pHelmIvP | Waypoint markers |
| `VIEW_SEGLIST` | pHelmIvP | Planned paths |

All CoT is machine-generated (`how="m-g"`). Coordinates are computed
from MOOS XY using `lib_cot_geodesy` (`ce="0" le="0"` — no sensor error).

```
ProcessConfig = pCoTGraphics
{
  AppTick   = 4
  CommsTick = 4

  publish_view_points    = true   // VIEW_POINT → spot marker
  publish_view_seglists  = true   // VIEW_SEGLIST → open polyline
  publish_view_polygons  = true   // VIEW_POLYGON + UTM_ZONE_* → filled polygon
  publish_flag_markers   = true   // FLAG_SUMMARY + VIEW_MARKER → flag markers
  publish_score_label    = true   // VIEW_TEXTBOX → text label at XE position

  immediate_view_points    = true
  stationary_send_interval = 3.0  // seconds between throttled resends

  debug = false
}
```

### pCoTChat

GeoChat bridge for CTF game event announcements and L2 role commands.

```
ProcessConfig = pCoTChat
{
  AppTick   = 4
  CommsTick = 4

  default_chatroom = All Chat Rooms
}
```

Outbound (shore MOOSDB → ATAK):
```cpp
// Game event announcement
Notify("ATAK_CHAT_OUT", "message=blue_one has been tagged|chatroom=All Chat Rooms");

// Waypoint ACK to operator
Notify("ATAK_CHAT_OUT", "message=blue_one: waypoint reached|chatroom=All Chat Rooms");

// Score update
Notify("ATAK_CHAT_OUT", "message=Score — RED:1 BLUE:0|chatroom=All Chat Rooms");
```

Inbound (ATAK GeoChat → shore MOOSDB via `ATAK_CHAT_IN`):
```
// L2 role assignment commands parsed by pCoTCommander:
"blue_one=attack"    →  ACTION_BLUE_ONE = ATTACK_MED
"blue_one=defend"    →  ACTION_BLUE_TWO = DEFEND_MED
"status"             →  score + role summary sent back to operator
```

`ATAK_CHAT_OUT` format: `message=<text>|chatroom=<room>`
(`|` separator — message content may contain commas)

`ATAK_CHAT_IN` format: `callsign=<sender>,chatroom=<room>,message=<text>`

---

## TLS Certificates

Place TAK server certificates in `pCoTBridge/certs/`:
```
pCoTBridge/certs/
├── shoreside.pem          ← client certificate
├── shoreside.key          ← client private key (may be passphrase-protected)
└── shoreside-trusted.pem  ← CA certificate (BEGIN TRUSTED CERTIFICATE format)
```

Default TAK server connection:
- Plain TCP: `192.168.0.38:8088` (no certs required)
- mTLS: `192.168.0.38:8089` (certs required, key passphrase: `atakatak`)

See `pCoTBridge/certs/README.md` for details.

> ⚠️ Never commit private keys to version control. The `.gitignore` in this
> repo excludes `*.key` and `*.pem` files in the `certs/` directory.

---

## MOOS Variable Reference

| Variable | Direction | Publisher | Subscriber(s) |
|----------|-----------|-----------|---------------|
| `NODE_REPORT` | pub | pNodeReporter (via uFldNodeBroker) | pCoTBridge, pCoTCommander, pCoTGraphics |
| `COT_OUTBOUND` | pub | pCoTGraphics, pCoTChat | pCoTBridge |
| `COT_INBOUND` | pub | pCoTBridge | pCoTCommander, pCoTChat |
| `ATAK_ACTIVE_<VEHICLE>` | pub | pCoTCommander | uFldShoreBroker → vehicle MOOSDB |
| `ATAK_WPT_UPDATE_<VEHICLE>` | pub | pCoTCommander | uFldShoreBroker → vehicle MOOSDB |
| `ATAK_WPT_REACHED` | pub | pHelmIvP (endflag) | pCoTCommander (via uFldNodeBroker) |
| `ATAK_CHAT_OUT` | pub | any app | pCoTChat |
| `ATAK_CHAT_IN` | pub | pCoTChat | pCoTCommander, any app |
| `ACTION_<VEHICLE>` | pub | pCoTCommander | uFldShoreBroker → vehicle MOOSDB |
| `VIEW_POINT` | pub | pHelmIvP | pCoTGraphics |
| `VIEW_SEGLIST` | pub | pHelmIvP | pCoTGraphics |
| `VIEW_POLYGON` | pub | uFldFlagManager | pCoTGraphics |
| `VIEW_MARKER` | pub | uFldFlagManager | pCoTGraphics |
| `VIEW_TEXTBOX` | pub | uFldFlagManager | pCoTGraphics |
| `FLAG_SUMMARY` | pub | uFldFlagManager | pCoTGraphics, pCoTChat |
| `UTM_ZONE_ONE` | pub | uFldTagManager | pCoTGraphics |
| `UTM_ZONE_TWO` | pub | uFldTagManager | pCoTGraphics |
| `RED_SCORES` | pub | uFldFlagManager | pCoTChat |
| `BLUE_SCORES` | pub | uFldFlagManager | pCoTChat |

---

## Tested With

- MOOS-IvP July 2024
- SeaRobotics SR-Surveyor M1.8
- ATAK 5.x (Android)
- FreeTAKServer / TAK Server (Java)
- OpenSSL 3.0.x

---

## Author

Tyler Errico  
West Point Robotics Research Center  
United States Military Academy
