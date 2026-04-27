/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTBridge.cpp                                   */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "CoTBridge.h"
#include "MBUtils.h"

using namespace std;


// ============================================================
// Constructor
// ============================================================

CoTBridge::CoTBridge()
{
  m_sock_fd              = -1;
  m_tak_host             = "192.168.0.38";
  m_tak_port             = 8088;
  m_last_connect_attempt = 0.0;
  m_reconnect_interval   = 5.0;
  m_cot_delimiter        = "\n";
  m_recv_buf             = "";

  m_multi_mode  = false;
  m_own_vehicle = "";

  m_moving_send_interval     = 1.0;
  m_stationary_send_interval = 3.0;
  m_speed_threshold          = 0.5;
  m_cot_stale_offset         = 10.0;

  m_debug = false;

  m_use_tls      = false;
  m_ssl_ctx      = nullptr;
  m_ssl          = nullptr;
  m_tls_cert_file = "";
  m_tls_key_file  = "";
  m_tls_ca_file   = "";
  m_tls_key_pass  = "";

  m_pos_cot_sent      = 0;
  m_outbound_forwarded = 0;
  m_inbound_received  = 0;
  m_send_failures     = 0;
  m_reconnect_count   = 0;
}

CoTBridge::~CoTBridge()
{
  disconnectFromTAKServer();
  teardownTLS();
}


// ============================================================
// debugLog()
// ============================================================

void CoTBridge::debugLog(const std::string& msg)
{
  if(!m_debug) return;
  m_debug_msgs.push_back(msg);
  if((int)m_debug_msgs.size() > DEBUG_BUF_SIZE)
    m_debug_msgs.pop_front();
}


// ============================================================
// OnConnectToServer()
// ============================================================

bool CoTBridge::OnConnectToServer()
{
  registerVariables();
  return true;
}


// ============================================================
// OnStartUp()
//
// Supports two config modes:
//
//   Single-vehicle (real hardware or one sim vehicle):
//     own_vehicle = alpha
//
//   Multi-vehicle (shoreside sim):
//     own_vehicles     = alpha,bravo,charlie
//     hostile_vehicles = red1,red2,red3
//
//   If both own_vehicle and own_vehicles are set, own_vehicles
//   takes precedence and multi-vehicle mode is used.
// ============================================================

bool CoTBridge::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  // Pass 1: debug flag first
  for(auto& line : sParams) {
    string l = tolower(line);
    string param = biteStringX(l, '=');
    if(param == "debug") setBooleanOnString(m_debug, l);
  }

  // Pass 2: all params
  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  for(auto& orig : sParams) {
    string line  = tolower(orig);
    string param = biteStringX(line, '=');
    string value = line;
    bool handled = true;

    if(param == "debug") {
      setBooleanOnString(m_debug, value);
    }
    else if(param == "tak_host") {
      string v = orig; biteStringX(v, '=');
      m_tak_host = stripBlankEnds(v);
      debugLog("Config: tak_host = " + m_tak_host);
    }
    else if(param == "tak_port") {
      m_tak_port = atoi(value.c_str());
      debugLog("Config: tak_port = " + intToString(m_tak_port));
    }
    else if(param == "own_vehicle") {
      // Single-vehicle mode
      string v = orig; biteStringX(v, '=');
      m_own_vehicle = stripBlankEnds(v);
      debugLog("Config: own_vehicle = " + m_own_vehicle);
    }
    else if(param == "own_vehicles") {
      // Multi-vehicle mode — comma-separated friendly vehicle names
      string v = orig; biteStringX(v, '=');
      vector<string> names = parseString(v, ',');
      string log_str;
      for(auto& n : names) {
        string trimmed = stripBlankEnds(n);
        m_own_set.insert(trimmed);
        log_str += trimmed + " ";
      }
      m_multi_mode = true;
      debugLog("Config: own_vehicles = { " + log_str + "}");
    }
    else if(param == "hostile_vehicles") {
      // Multi-vehicle mode — comma-separated hostile vehicle names
      string v = orig; biteStringX(v, '=');
      vector<string> names = parseString(v, ',');
      string log_str;
      for(auto& n : names) {
        string trimmed = stripBlankEnds(n);
        m_hostile_set.insert(trimmed);
        log_str += trimmed + " ";
      }
      m_multi_mode = true;
      debugLog("Config: hostile_vehicles = { " + log_str + "}");
    }
    else if(param == "moving_send_interval") {
      m_moving_send_interval = atof(value.c_str());
      debugLog("Config: moving_send_interval = " +
               doubleToStringX(m_moving_send_interval) + "s");
    }
    else if(param == "stationary_send_interval") {
      m_stationary_send_interval = atof(value.c_str());
      debugLog("Config: stationary_send_interval = " +
               doubleToStringX(m_stationary_send_interval) + "s");
    }
    else if(param == "speed_threshold") {
      m_speed_threshold = atof(value.c_str());
      debugLog("Config: speed_threshold = " +
               doubleToStringX(m_speed_threshold) + "m/s");
    }
    else if(param == "cot_stale_offset") {
      m_cot_stale_offset = atof(value.c_str());
      debugLog("Config: cot_stale_offset = " +
               doubleToStringX(m_cot_stale_offset) + "s");
    }
    else if(param == "reconnect_interval") {
      m_reconnect_interval = atof(value.c_str());
      debugLog("Config: reconnect_interval = " +
               doubleToStringX(m_reconnect_interval) + "s");
    }
    else if(param == "cot_delimiter") {
      m_cot_delimiter = (value == "null") ? string(1, '\0') : "\n";
      debugLog("Config: cot_delimiter = " +
               string(value == "null" ? "null" : "newline"));
    }
    else if(param == "tls_cert_file") {
      string v = orig; biteStringX(v, '=');
      m_tls_cert_file = stripBlankEnds(v);
      debugLog("Config: tls_cert_file = " + m_tls_cert_file);
    }
    else if(param == "tls_key_file") {
      string v = orig; biteStringX(v, '=');
      m_tls_key_file = stripBlankEnds(v);
      debugLog("Config: tls_key_file = " + m_tls_key_file);
    }
    else if(param == "tls_ca_file") {
      string v = orig; biteStringX(v, '=');
      m_tls_ca_file = stripBlankEnds(v);
      debugLog("Config: tls_ca_file = " + m_tls_ca_file);
    }
    else if(param == "tls_key_pass") {
      // Passphrase for encrypted private key (BEGIN ENCRYPTED PRIVATE KEY).
      // Stored in memory only — never logged to AppCast or debug output.
      string v = orig; biteStringX(v, '=');
      m_tls_key_pass = stripBlankEnds(v);
      debugLog("Config: tls_key_pass = [set, not shown]");
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Validate config
  if(m_multi_mode) {
    if(m_own_set.empty() && m_hostile_set.empty())
      reportConfigWarning("pCoTBridge: multi-vehicle mode but no vehicles listed");
    // If own_vehicle was also set, add it to own_set for consistency
    if(!m_own_vehicle.empty())
      m_own_set.insert(m_own_vehicle);
    debugLog("OnStartUp: MULTI-VEHICLE mode — own=" +
             intToString((int)m_own_set.size()) +
             " hostile=" + intToString((int)m_hostile_set.size()));
  }
  else {
    if(m_own_vehicle.empty())
      reportConfigWarning("pCoTBridge: own_vehicle not set — "
                          "position CoT will have empty callsign");
    debugLog("OnStartUp: SINGLE-VEHICLE mode — vehicle=" + m_own_vehicle);
  }

  // Enable TLS automatically when port 8089 is configured
  m_use_tls = (m_tak_port == 8089);
  if(m_use_tls) {
    debugLog("OnStartUp: TLS mode enabled (port 8089)");
    if(!initTLSContext()) {
      // Don't crash the whole app — fall back to plain TCP on 8088.
      // The error is logged via reportRunWarning — visible in AppCast.
      reportRunWarning("pCoTBridge: TLS init failed — "
                       "falling back to plain TCP on port 8088. "
                       "Check cert paths and file formats.");
      m_use_tls  = false;
      m_tak_port = 8088;
    }
  }

  connectToTAKServer();
  registerVariables();
  return true;
}


// ============================================================
// registerVariables()
// ============================================================

void CoTBridge::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NODE_REPORT",       0);
  Register("NODE_REPORT_LOCAL", 0);
  Register("COT_OUTBOUND",      0);
}


// ============================================================
// shouldTrack()
//
// Single-vehicle mode: only track m_own_vehicle.
// Multi-vehicle mode: track any vehicle in own or hostile set.
// ============================================================

bool CoTBridge::shouldTrack(const std::string& name) const
{
  if(m_multi_mode)
    return (m_own_set.count(name) > 0 || m_hostile_set.count(name) > 0);
  else
    return (name == m_own_vehicle);
}


// ============================================================
// isFriendly()
// ============================================================

bool CoTBridge::isFriendly(const std::string& name) const
{
  if(m_multi_mode)
    return (m_own_set.count(name) > 0);
  else
    return true; // single-vehicle mode — always own vehicle = friendly
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTBridge::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    string key  = msg.m_sKey;
    string sval = msg.m_sVal;

    if(key == "NODE_REPORT" || key == "NODE_REPORT_LOCAL") {
      parseNodeReport(sval);
    }
    else if(key == "COT_OUTBOUND") {
      debugLog("OnNewMail: COT_OUTBOUND (" +
               intToString((int)sval.size()) + " bytes)");
      if(isConnected()) {
        if(sendCoT(sval))
          m_outbound_forwarded++;
      }
      else
        debugLog("OnNewMail: COT_OUTBOUND dropped — not connected");
    }
  }

  return true;
}


// ============================================================
// Iterate()
// ============================================================

bool CoTBridge::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if(!isConnected())
    connectToTAKServer();

  // Send position CoT for each tracked vehicle that is due
  if(isConnected()) {
    for(auto& kv : m_vehicles) {
      VehicleState& vs = kv.second;
      if(!vs.valid) continue;

      bool moving   = (vs.speed > m_speed_threshold);
      double interval = moving ? m_moving_send_interval
                               : m_stationary_send_interval;
      if((m_curr_time - vs.last_sent) < interval) continue;

      string cot = buildPositionCoT(vs);
      if(sendCoT(cot)) {
        vs.last_sent = m_curr_time;
        m_pos_cot_sent++;
        debugLog("Iterate: pos CoT sent for " + vs.name +
                 " [" + string(moving ? "moving" : "static") + "]");
      }
    }
  }

  if(isConnected())
    readInboundCoT();

  AppCastingMOOSApp::PostReport();
  return true;
}


// ============================================================
// parseNodeReport()
//
// Parses a NODE_REPORT and updates m_vehicles if the vehicle
// should be tracked. Creates a new entry on first receipt.
// Returns true if any state was updated.
// ============================================================

bool CoTBridge::parseNodeReport(const std::string& report)
{
  bool got_name = false, got_lat = false, got_lon = false;
  bool got_hdg  = false, got_spd = false, got_time = false;

  string name;
  double lat = 0, lon = 0, hdg = 0, spd = 0, t = 0;

  vector<string> tokens = parseString(report, ',');
  for(auto& tok : tokens) {
    string t_copy = tok;
    string key = toupper(biteStringX(t_copy, '='));
    string val = t_copy;

    if     (key == "NAME") { name = val;               got_name = true; }
    else if(key == "LAT")  { lat  = atof(val.c_str()); got_lat  = true; }
    else if(key == "LON")  { lon  = atof(val.c_str()); got_lon  = true; }
    else if(key == "HDG")  { hdg  = atof(val.c_str()); got_hdg  = true; }
    else if(key == "SPD")  { spd  = atof(val.c_str()); got_spd  = true; }
    else if(key == "TIME") { t    = atof(val.c_str()); got_time = true; }
  }

  if(!got_name || !got_lat || !got_lon || !got_hdg || !got_spd || !got_time)
    return false;

  // In single-vehicle mode: auto-learn vehicle name from first report
  // if own_vehicle wasn't configured
  if(!m_multi_mode && m_own_vehicle.empty())
    m_own_vehicle = name;

  // Skip vehicles we're not tracking
  if(!shouldTrack(name)) {
    debugLog("parseNodeReport: ignoring " + name + " (not tracked)");
    return false;
  }

  // Create entry if new
  bool is_new = (m_vehicles.find(name) == m_vehicles.end());
  if(is_new) {
    VehicleState vs;
    vs.name     = name;
    vs.friendly = isFriendly(name);
    m_vehicles[name] = vs;
    debugLog("parseNodeReport: new vehicle " + name +
             " [" + string(vs.friendly ? "friendly" : "hostile") + "]");
  }

  VehicleState& vs = m_vehicles[name];
  vs.lat       = lat;
  vs.lon       = lon;
  vs.heading   = hdg;
  vs.speed     = spd;
  vs.timestamp = t;
  vs.valid     = true;

  debugLog("parseNodeReport: updated " + name +
           " lat=" + doubleToStringX(lat, 6) +
           " lon=" + doubleToStringX(lon, 6));
  return true;
}


// ============================================================
// buildPositionCoT()
// ============================================================

string CoTBridge::buildPositionCoT(const VehicleState& vs)
{
  string cot_type = vs.friendly ? "a-f-S-C-U-N" : "a-h-S-C-U-N";

  string t_time  = formatCoTTime(vs.timestamp, 0.0);
  string t_start = formatCoTTime(vs.timestamp, 0.0);
  string t_stale = formatCoTTime(vs.timestamp, m_cot_stale_offset);

  string uid = "surveyor-" + vs.name;

  string detail =
    "<detail>"
      "<contact callsign=\"" + vs.name + "\""
        " endpoint=\"*:-1:stcp\"/>"
      "<__group name=\"Cyan\" role=\"Team Member\"/>"
      "<uid Droid=\""        + vs.name + "\"/>"
      "<takv"
        " device=\"SeaRobotics-Surveyor\""
        " platform=\"pCoTBridge\""
        " os=\"0\""
        " version=\"1.0.0\"/>"
      "<precisionlocation geopointsrc=\"GPS\" altsrc=\"GPS\"/>"
      "<track"
        " speed=\""  + doubleToStringX(vs.speed,   2) + "\""
        " course=\"" + doubleToStringX(vs.heading,  1) + "\"/>"
    "</detail>";

  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<event"
      " version=\"2.0\""
      " uid=\""    + uid      + "\""
      " type=\""   + cot_type + "\""
      " how=\"m-g\""
      " time=\""   + t_time   + "\""
      " start=\""  + t_start  + "\""
      " stale=\""  + t_stale  + "\">\n"
    "  <point"
      " lat=\"" + doubleToStringX(vs.lat, 7) + "\""
      " lon=\"" + doubleToStringX(vs.lon, 7) + "\""
      " hae=\"0.0\""
      " ce=\"9999999.0\""
      " le=\"9999999.0\"/>\n"
    "  " + detail + "\n"
    "</event>";
}


// ============================================================
// formatCoTTime()
// ============================================================

string CoTBridge::formatCoTTime(double moos_time, double offset)
{
  time_t t = (time_t)(moos_time + offset);
  struct tm* utc = gmtime(&t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
  return string(buf);
}


// ============================================================
// connectToTAKServer()
// ============================================================

bool CoTBridge::connectToTAKServer()
{
  if((m_curr_time - m_last_connect_attempt) < m_reconnect_interval)
    return false;
  m_last_connect_attempt = m_curr_time;

  debugLog("connectToTAKServer: attempt → " +
           m_tak_host + ":" + intToString(m_tak_port));

  if(m_sock_fd >= 0) disconnectFromTAKServer();

  struct hostent* server = gethostbyname(m_tak_host.c_str());
  if(!server) {
    reportRunWarning("pCoTBridge: cannot resolve " + m_tak_host);
    return false;
  }

  m_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(m_sock_fd < 0) {
    reportRunWarning("pCoTBridge: socket() failed");
    m_sock_fd = -1;
    return false;
  }

  struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
  setsockopt(m_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setsockopt(m_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons((uint16_t)m_tak_port);
  memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

  if(connect(m_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    reportRunWarning("pCoTBridge: connect() failed to " +
                     m_tak_host + ":" + intToString(m_tak_port) +
                     " — " + string(strerror(errno)));
    close(m_sock_fd);
    m_sock_fd = -1;
    return false;
  }

  // --------------------------------------------------------
  // TLS handshake (port 8089 only)
  // Wraps the connected TCP socket in an SSL session.
  // The SSL context was initialised in OnStartUp with our
  // client cert/key and the CA trust chain.
  // --------------------------------------------------------
  if(m_use_tls) {
    if(!m_ssl_ctx) {
      reportRunWarning("pCoTBridge: TLS context not initialized");
      close(m_sock_fd);
      m_sock_fd = -1;
      return false;
    }

    m_ssl = SSL_new(m_ssl_ctx);
    if(!m_ssl) {
      reportRunWarning("pCoTBridge: SSL_new() failed");
      close(m_sock_fd);
      m_sock_fd = -1;
      return false;
    }

    SSL_set_fd(m_ssl, m_sock_fd);

    int ret = SSL_connect(m_ssl);
    if(ret != 1) {
      unsigned long err = ERR_get_error();
      char err_buf[256];
      ERR_error_string_n(err, err_buf, sizeof(err_buf));
      reportRunWarning(string("pCoTBridge: SSL_connect() failed — ") + err_buf);
      debugLog("connectToTAKServer: TLS handshake FAILED — " + string(err_buf));
      SSL_free(m_ssl);
      m_ssl = nullptr;
      close(m_sock_fd);
      m_sock_fd = -1;
      return false;
    }

    debugLog("connectToTAKServer: TLS handshake OK — cipher=" +
             string(SSL_get_cipher(m_ssl)));
  }

  // Set non-blocking for reads after TLS handshake
  // (handshake must run in blocking mode)
  int flags = fcntl(m_sock_fd, F_GETFL, 0);
  fcntl(m_sock_fd, F_SETFL, flags | O_NONBLOCK);

  m_reconnect_count++;
  reportEvent("pCoTBridge: connected to " +
              m_tak_host + ":" + intToString(m_tak_port) +
              (m_use_tls ? " [TLS]" : " [plain]"));
  debugLog("connectToTAKServer: connected fd=" + intToString(m_sock_fd));
  return true;
}


// ============================================================
// disconnectFromTAKServer()
// ============================================================

void CoTBridge::disconnectFromTAKServer()
{
  // Tear down SSL session before closing socket
  if(m_ssl) {
    SSL_shutdown(m_ssl);
    SSL_free(m_ssl);
    m_ssl = nullptr;
  }
  if(m_sock_fd >= 0) {
    close(m_sock_fd);
    m_sock_fd = -1;
  }
}


// ============================================================
// sendCoT()
// ============================================================

bool CoTBridge::sendCoT(const std::string& xml)
{
  if(m_sock_fd < 0) return false;

  // Temporarily set blocking for the send
  int flags = fcntl(m_sock_fd, F_GETFL, 0);
  fcntl(m_sock_fd, F_SETFL, flags & ~O_NONBLOCK);

  string msg = xml + m_cot_delimiter;
  ssize_t n;

  if(m_use_tls && m_ssl) {
    // SSL_write handles framing internally — no MSG_NOSIGNAL needed
    n = SSL_write(m_ssl, msg.c_str(), (int)msg.size());
  }
  else {
    n = send(m_sock_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
  }

  fcntl(m_sock_fd, F_SETFL, flags); // restore non-blocking

  if(n < 0 || (size_t)n < msg.size()) {
    m_send_failures++;
    if(m_use_tls && m_ssl) {
      unsigned long err = ERR_get_error();
      char err_buf[256];
      ERR_error_string_n(err, err_buf, sizeof(err_buf));
      reportRunWarning(string("pCoTBridge: SSL_write() failed — ") + err_buf);
    }
    else {
      reportRunWarning("pCoTBridge: send() failed — " +
                       string(strerror(errno)));
    }
    disconnectFromTAKServer();
    return false;
  }

  debugLog("sendCoT: " + intToString((int)n) + " bytes");
  return true;
}


// ============================================================
// readInboundCoT()
//
// Non-blocking reads on m_sock_fd. Buffers bytes, extracts
// complete </event> messages, publishes to COT_INBOUND.
// Filters own echoes (uid="surveyor-*").
// ============================================================

void CoTBridge::readInboundCoT()
{
  char buf[COT_BUF_SIZE];
  while(true) {
    ssize_t n;
    if(m_use_tls && m_ssl) {
      n = SSL_read(m_ssl, buf, sizeof(buf) - 1);
      // SSL_read returns <= 0 on no-data or error (non-blocking)
      if(n <= 0) break;
    }
    else {
      n = recv(m_sock_fd, buf, sizeof(buf) - 1, 0);
      if(n <= 0) break;
    }
    buf[n] = '\0';
    m_recv_buf.append(buf, n);
  }

  const string END_TAG = "</event>";
  size_t pos;
  while((pos = m_recv_buf.find(END_TAG)) != string::npos) {
    string event = m_recv_buf.substr(0, pos + END_TAG.size());
    m_recv_buf.erase(0, pos + END_TAG.size());

    size_t first = m_recv_buf.find_first_not_of(" \t\r\n");
    if(first != string::npos)
      m_recv_buf = m_recv_buf.substr(first);

    // Skip own position CoT echoed back by the TAK server
    if(event.find("uid=\"surveyor-") != string::npos) {
      debugLog("readInboundCoT: skipping own echo");
      continue;
    }

    m_inbound_received++;
    debugLog("readInboundCoT: COT_INBOUND (" +
             intToString((int)event.size()) + " bytes)");
    Notify("COT_INBOUND", event);
  }

  if(m_recv_buf.size() > (size_t)(COT_BUF_SIZE * 4)) {
    reportRunWarning("pCoTBridge: recv buffer overflow — clearing");
    m_recv_buf.clear();
  }
}



// ============================================================
// initTLSContext()
//
// Called once in OnStartUp() when tak_port = 8089.
// Creates the OpenSSL SSL_CTX with:
//   - Client certificate (shoreside.pem)
//   - Client private key  (shoreside.key)
//   - CA trust chain      (shoreside-trusted.pem)
//
// The context is reused across reconnections — only one context
// per app lifetime. Each connection creates a new SSL* session
// in connectToTAKServer() via SSL_new(m_ssl_ctx).
// ============================================================

bool CoTBridge::initTLSContext()
{
  // Initialise OpenSSL (safe to call multiple times — idempotent in 1.1+)
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  // TLS_client_method: negotiate the highest mutually supported version
  m_ssl_ctx = SSL_CTX_new(TLS_client_method());
  if(!m_ssl_ctx) {
    reportRunWarning("pCoTBridge: SSL_CTX_new() failed");
    return false;
  }

  // --------------------------------------------------------
  // Load client certificate — proves our identity to the
  // TAK server (mTLS: both sides authenticate).
  // shoreside.pem: BEGIN CERTIFICATE
  // --------------------------------------------------------
  if(!m_tls_cert_file.empty()) {
    if(SSL_CTX_use_certificate_file(m_ssl_ctx,
                                    m_tls_cert_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1) {
      reportRunWarning("pCoTBridge: failed to load cert: " + m_tls_cert_file);
      return false;
    }
    debugLog("initTLSContext: client cert loaded: " + m_tls_cert_file);
  }

  // --------------------------------------------------------
  // Load client private key — used to sign the TLS handshake.
  //
  // If the key is passphrase-protected (BEGIN ENCRYPTED PRIVATE KEY),
  // we set a password callback so OpenSSL can decrypt it automatically.
  //
  // The callback receives m_tls_key_pass via the userdata pointer and
  // copies it into the buffer OpenSSL provides. If tls_key_pass is empty,
  // the callback returns 0 (no passphrase) which works for plaintext keys.
  // --------------------------------------------------------
  if(!m_tls_key_file.empty()) {
    // Set passphrase callback — called by OpenSSL when decrypting the key
    SSL_CTX_set_default_passwd_cb(m_ssl_ctx,
      [](char* buf, int size, int /*rwflag*/, void* userdata) -> int {
        const char* pass = static_cast<const char*>(userdata);
        if(!pass || pass[0] == '\0') return 0;
        int len = (int)strlen(pass);
        if(len > size) len = size;
        memcpy(buf, pass, len);
        return len;
      });
    // Pass the passphrase string as userdata — stays valid for the
    // duration of the SSL_CTX_use_PrivateKey_file call below
    SSL_CTX_set_default_passwd_cb_userdata(
      m_ssl_ctx,
      m_tls_key_pass.empty() ? nullptr
                              : (void*)m_tls_key_pass.c_str());

    if(SSL_CTX_use_PrivateKey_file(m_ssl_ctx,
                                   m_tls_key_file.c_str(),
                                   SSL_FILETYPE_PEM) != 1) {
      unsigned long err = ERR_get_error();
      char err_buf[256];
      ERR_error_string_n(err, err_buf, sizeof(err_buf));
      reportRunWarning("pCoTBridge: failed to load key: " + m_tls_key_file +
                       " — " + string(err_buf) +
                       (m_tls_key_pass.empty()
                        ? " (hint: key may be encrypted — set tls_key_pass)"
                        : " (check passphrase)"));
      return false;
    }
    // Verify cert and key are a matching pair
    if(SSL_CTX_check_private_key(m_ssl_ctx) != 1) {
      reportRunWarning("pCoTBridge: cert/key mismatch — "
                       "check tls_cert_file and tls_key_file");
      return false;
    }
    debugLog("initTLSContext: private key loaded and verified");
  }

  // --------------------------------------------------------
  // Load CA certificate — used to verify the TAK server's cert.
  //
  // shoreside-trusted.pem uses "BEGIN TRUSTED CERTIFICATE" format
  // (OpenSSL X509_TRUST extension) rather than plain "BEGIN CERTIFICATE".
  //
  // SSL_CTX_load_verify_locations() may reject TRUSTED CERTIFICATE
  // on some builds. We use X509_STORE directly as a robust fallback
  // that handles both formats.
  // --------------------------------------------------------
  if(!m_tls_ca_file.empty()) {
    // Try standard load first
    int ca_ok = SSL_CTX_load_verify_locations(m_ssl_ctx,
                                              m_tls_ca_file.c_str(),
                                              nullptr);
    if(ca_ok != 1) {
      // Fallback: load via X509_STORE which handles TRUSTED CERTIFICATE
      ERR_clear_error();
      X509_STORE* store = SSL_CTX_get_cert_store(m_ssl_ctx);
      X509_LOOKUP* lookup = X509_STORE_add_lookup(store,
                                                  X509_LOOKUP_file());
      // X509_FILETYPE_DEFAULT handles both BEGIN CERTIFICATE and
      // BEGIN TRUSTED CERTIFICATE formats
      ca_ok = X509_LOOKUP_load_file(lookup,
                                    m_tls_ca_file.c_str(),
                                    X509_FILETYPE_DEFAULT);
      if(ca_ok != 1) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        reportRunWarning("pCoTBridge: failed to load CA cert: " +
                         m_tls_ca_file + " — " + string(err_buf));
        return false;
      }
      debugLog("initTLSContext: CA cert loaded via X509_STORE fallback");
    }
    else {
      debugLog("initTLSContext: CA cert loaded: " + m_tls_ca_file);
    }
  }

  // --------------------------------------------------------
  // Require server certificate verification.
  // Without this, TLS is encrypted but not authenticated —
  // we wouldn't know if we're talking to the real TAK server.
  // --------------------------------------------------------
  SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_verify_depth(m_ssl_ctx, 4);

  reportEvent("pCoTBridge: TLS context initialized (mTLS)");
  debugLog("initTLSContext: TLS context ready");
  return true;
}


// ============================================================
// teardownTLS()
//
// Frees the SSL context. Called from destructor.
// SSL session (m_ssl) is freed in disconnectFromTAKServer().
// ============================================================

void CoTBridge::teardownTLS()
{
  if(m_ssl) {
    SSL_shutdown(m_ssl);
    SSL_free(m_ssl);
    m_ssl = nullptr;
  }
  if(m_ssl_ctx) {
    SSL_CTX_free(m_ssl_ctx);
    m_ssl_ctx = nullptr;
  }
}


// ============================================================
// buildReport()
// ============================================================

bool CoTBridge::buildReport()
{
  string conn = isConnected()
    ? "CONNECTED " + m_tak_host + ":" + intToString(m_tak_port) +
      (m_use_tls ? " [TLS]" : " [plain]")
    : "DISCONNECTED (retry in " +
      doubleToStringX(m_reconnect_interval -
                      (m_curr_time - m_last_connect_attempt), 1) + "s)";

  m_msgs << "TAK: " << conn
         << "  reconnects=" << m_reconnect_count
         << "  debug=" << boolToString(m_debug) << endl;

  string mode = m_multi_mode
    ? "MULTI-VEHICLE (own=" + intToString((int)m_own_set.size()) +
      " hostile=" + intToString((int)m_hostile_set.size()) + ")"
    : "SINGLE-VEHICLE (" + m_own_vehicle + ")";
  m_msgs << "Mode: " << mode << endl;
  m_msgs << endl;

  m_msgs << "CoT: pos_sent=" << m_pos_cot_sent
         << "  forwarded=" << m_outbound_forwarded
         << "  inbound=" << m_inbound_received
         << "  failures=" << m_send_failures << endl;
  m_msgs << endl;

  m_msgs << "Tracked vehicles (" << m_vehicles.size() << "):" << endl;
  for(auto& kv : m_vehicles) {
    const VehicleState& vs = kv.second;
    double age = m_curr_time - vs.timestamp;
    bool stale = (age > 5.0);
    m_msgs << "  " << vs.name
           << (vs.friendly ? " [own]" : " [opp]")
           << (vs.speed > m_speed_threshold ? " MOV" : " STA")
           << (stale ? " STALE(" + doubleToStringX(age, 1) + "s)" : "")
           << "  lat=" << doubleToStringX(vs.lat, 6)
           << " lon=" << doubleToStringX(vs.lon, 6)
           << " sent=" << doubleToStringX(m_curr_time - vs.last_sent, 1)
           << "s ago" << endl;
  }

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
