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
| `pCoTCommander` | Inbound command dispatcher — translates ATAK "Go To" waypoints into MOOS-IvP behavior updates |
| `pCoTGraphics` | VIEW_* renderer — converts `VIEW_POINT` and `VIEW_SEGLIST` to CoT graphics visible on ATAK map |
| `pCoTChat` | GeoChat bridge — sends/receives ATAK chat messages via MOOS variables |

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

```
ProcessConfig = pCoTBridge
{
  AppTick   = 10
  CommsTick = 10

  tak_host    = 192.168.0.38
  tak_port    = 8088            // 8088 = plain TCP, 8089 = TLS

  // Single-vehicle mode (on the robot)
  own_vehicle = alpha

  // Multi-vehicle mode (shoreside sim)
  // own_vehicles      = alpha,bravo,charlie
  // hostile_vehicles  = red1,red2,red3

  // TLS — required when tak_port = 8089
  // tls_cert_file = /path/to/certs/shoreside.pem
  // tls_key_file  = /path/to/certs/shoreside.key
  // tls_ca_file   = /path/to/certs/shoreside-trusted.pem
  // tls_key_pass  = atakatak

  moving_send_interval     = 1.0
  stationary_send_interval = 3.0
  cot_delimiter            = newline
}
```

### pCoTCommander

```
ProcessConfig = pCoTCommander
{
  AppTick   = 4
  CommsTick = 10

  enable_waypoint_control = true
  waypoint_update_var     = ATAK_WPT_UPDATE
  capture_radius          = 15.0
}
```

Requires `waypt_atak` behavior in `.bhv`:
```
Behavior = BHV_Waypoint
{
  name      = waypt_atak
  pwt       = 150
  condition = DEPLOY = true
  condition = ATAK_ACTIVE = true
  endflag   = ATAK_WPT_REACHED = true
  perpetual = true
  updates   = ATAK_WPT_UPDATE
  speed     = 3.0
  capture_radius = 15.0
  slip_radius    = 30.0
  points    = 0,0
}
```

### pCoTChat

```
ProcessConfig = pCoTChat
{
  AppTick   = 4
  CommsTick = 10

  own_callsign = alpha
  echo_filter  = true
}
```

Sending a chat message from any MOOS app:
```cpp
// Direct message
Notify("ATAK_CHAT_OUT", "message=hello|chatroom=Tyler");

// Team broadcast
Notify("ATAK_CHAT_OUT", "message=status update|chatroom=Cyan");

// All Chat Rooms
Notify("ATAK_CHAT_OUT", "message=mission started|chatroom=All Chat Rooms");
```

---

## TLS Certificates

Place TAK server certificates in `pCoTBridge/certs/`:
```
pCoTBridge/certs/
├── shoreside.pem          ← client certificate
├── shoreside.key          ← client private key (may be passphrase-protected)
└── shoreside-trusted.pem  ← CA certificate
```

See `pCoTBridge/certs/README.md` for details.

> ⚠️ Never commit private keys to version control. The `.gitignore` in this
> repo excludes `*.key` and `*.pem` files in the `certs/` directory.

---

## MOOS Variable Reference

| Variable | Direction | Publisher | Subscriber(s) |
|----------|-----------|-----------|---------------|
| `NODE_REPORT` | pub | pNodeReporter | pCoTBridge, pCoTCommander, pCoTGraphics, pCoTChat |
| `COT_OUTBOUND` | pub | pCoTGraphics, pCoTChat | pCoTBridge |
| `COT_INBOUND` | pub | pCoTBridge | pCoTCommander, pCoTChat |
| `ATAK_ACTIVE` | pub | pCoTCommander | pHelmIvP |
| `ATAK_WPT_UPDATE` | pub | pCoTCommander | pHelmIvP |
| `ATAK_WPT_REACHED` | pub | pHelmIvP (endflag) | pCoTCommander |
| `ATAK_CHAT_OUT` | pub | any app | pCoTChat |
| `ATAK_CHAT_IN` | pub | pCoTChat | any app |
| `VIEW_POINT` | pub | pHelmIvP | pCoTGraphics |
| `VIEW_SEGLIST` | pub | pHelmIvP | pCoTGraphics |

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
