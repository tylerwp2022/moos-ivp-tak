/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTBridge.h                                     */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTBridge — CoT transport layer.                       */
/*                                                          */
/*  Supports two deployment modes:                          */
/*                                                          */
/*  Single-vehicle mode (real hardware / one sim vehicle):  */
/*    own_vehicle = alpha                                    */
/*    One NODE_REPORT tracked. One SA CoT sent per tick.    */
/*    Runs on the robot's own computer.                     */
/*                                                          */
/*  Multi-vehicle mode (shoreside sim):                     */
/*    own_vehicles     = alpha,bravo,charlie                 */
/*    hostile_vehicles = red1,red2,red3                     */
/*    All listed vehicles tracked. Per-vehicle SA CoT sent. */
/*    Friendly/hostile determined by which list the vehicle  */
/*    appears in. Unlisted vehicles are ignored.            */
/*                                                          */
/*  In both modes:                                          */
/*    - Forwards COT_OUTBOUND → TAK server                  */
/*    - Publishes COT_INBOUND from TAK server               */
/************************************************************/

#ifndef COT_BRIDGE_HEADER
#define COT_BRIDGE_HEADER

#include <string>
#include <map>
#include <set>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

// OpenSSL — for TLS connection to TAK server port 8089
#include <openssl/ssl.h>
#include <openssl/err.h>

// ============================================================
// VehicleState — tracks one vehicle's current position/state.
// One entry per vehicle in m_vehicles map (keyed by name).
// ============================================================
struct VehicleState {
  std::string name;
  double      lat       = 0.0;
  double      lon       = 0.0;
  double      heading   = 0.0;
  double      speed     = 0.0;
  double      timestamp = 0.0;
  double      last_sent = 0.0;  // MOOS time of last CoT send
  bool        valid     = false; // true once first NODE_REPORT received
  bool        friendly  = true;  // false = hostile CoT type
};


class CoTBridge : public AppCastingMOOSApp
{
public:
  CoTBridge();
  virtual ~CoTBridge();

  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();

protected:
  void registerVariables();
  void debugLog(const std::string& msg);

  // --------------------------------------------------------
  // TCP connection
  // --------------------------------------------------------
  bool connectToTAKServer();
  void disconnectFromTAKServer();
  bool isConnected() const { return m_sock_fd >= 0; }
  bool sendCoT(const std::string& xml);

  // --------------------------------------------------------
  // CoT builders
  // --------------------------------------------------------
  std::string buildPositionCoT(const VehicleState& vs);
  std::string formatCoTTime(double moos_time,
                             double stale_offset_sec = 0.0);

  // --------------------------------------------------------
  // NODE_REPORT parsing
  // Updates m_vehicles map. Returns true if state changed.
  // --------------------------------------------------------
  bool parseNodeReport(const std::string& report);

  // --------------------------------------------------------
  // Inbound CoT reader — publishes to COT_INBOUND
  // --------------------------------------------------------
  void readInboundCoT();

  // --------------------------------------------------------
  // Vehicle tracking helpers
  // --------------------------------------------------------

  // Returns true if this vehicle should be tracked.
  // In single-vehicle mode: only m_own_vehicle matches.
  // In multi-vehicle mode: any name in m_own_set or m_hostile_set.
  bool shouldTrack(const std::string& name) const;

  // Returns true if this vehicle is friendly (own team).
  bool isFriendly(const std::string& name) const;

private:
  // --------------------------------------------------------
  // Network
  // --------------------------------------------------------
  int         m_sock_fd;
  std::string m_tak_host;
  int         m_tak_port;
  double      m_last_connect_attempt;
  double      m_reconnect_interval;
  std::string m_cot_delimiter;
  std::string m_recv_buf;
  static const int COT_BUF_SIZE = 8192;

  // --------------------------------------------------------
  // Vehicle configuration
  //
  // Single-vehicle mode: m_own_vehicle set, m_multi_mode=false
  //   own_vehicle = alpha
  //
  // Multi-vehicle mode: m_multi_mode=true
  //   own_vehicles     = alpha,bravo,charlie
  //   hostile_vehicles = red1,red2,red3
  // --------------------------------------------------------
  bool        m_multi_mode;        // true = multi-vehicle mode
  std::string m_own_vehicle;       // single-vehicle mode name
  std::set<std::string> m_own_set;      // multi: friendly vehicle names
  std::set<std::string> m_hostile_set;  // multi: hostile vehicle names

  // All tracked vehicles — keyed by name
  // Single-vehicle mode: at most one entry
  // Multi-vehicle mode:  one entry per listed vehicle
  std::map<std::string, VehicleState> m_vehicles;

  // --------------------------------------------------------
  // Send throttle
  // --------------------------------------------------------
  double m_moving_send_interval;
  double m_stationary_send_interval;
  double m_speed_threshold;
  double m_cot_stale_offset;

  // --------------------------------------------------------
  // Config
  // --------------------------------------------------------
  bool        m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // TLS (for port 8089 secure connection)
  //
  // When tak_port = 8089, the TCP socket is wrapped in an
  // OpenSSL SSL* context after connect(). All sends and
  // receives go through SSL_write()/SSL_read() instead of
  // send()/recv(). Port 8088 uses plain TCP as before.
  //
  // Configured via .moos:
  //   tls_cert_file = /path/to/certs/shoreside.pem
  //   tls_key_file  = /path/to/certs/shoreside.key
  //   tls_ca_file   = /path/to/certs/shoreside-trusted.pem
  // --------------------------------------------------------
  bool        m_use_tls;          // true when tak_port = 8089
  SSL_CTX*    m_ssl_ctx;          // OpenSSL context (one per app lifetime)
  SSL*        m_ssl;              // SSL session (one per connection)
  std::string m_tls_cert_file;    // client certificate (PEM)
  std::string m_tls_key_file;     // client private key (PEM)
  std::string m_tls_ca_file;      // CA / trust chain (PEM)
  std::string m_tls_key_pass;     // passphrase for encrypted private key
                                  // set via 'tls_key_pass' in .moos

  bool initTLSContext();          // called once in OnStartUp
  void teardownTLS();             // closes SSL session + context

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_pos_cot_sent;
  unsigned int m_outbound_forwarded;
  unsigned int m_inbound_received;
  unsigned int m_send_failures;
  unsigned int m_reconnect_count;
};

#endif // COT_BRIDGE_HEADER
// NOTE: append this before the final #endif — added separately for clarity
