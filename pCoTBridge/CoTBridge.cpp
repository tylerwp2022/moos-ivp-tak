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

  m_use_tls      = false;
  m_ssl_ctx      = nullptr;
  m_ssl          = nullptr;
  m_tls_cert_file = "";
  m_tls_key_file  = "";
  m_tls_ca_file   = "";
  m_tls_key_pass  = "";

  m_debug = false;

  m_outbound_forwarded = 0;
  m_inbound_received   = 0;
  m_send_failures      = 0;
  m_reconnect_count    = 0;
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
      // Passphrase for encrypted private key — never logged
      string v = orig; biteStringX(v, '=');
      m_tls_key_pass = stripBlankEnds(v);
      debugLog("Config: tls_key_pass = [set, not shown]");
    }
    else
      handled = false;

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // Enable TLS automatically when port 8089 is configured
  m_use_tls = (m_tak_port == 8089);
  if(m_use_tls) {
    debugLog("OnStartUp: TLS mode enabled (port 8089)");
    if(!initTLSContext()) {
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
//
// pCoTBridge subscribes only to COT_OUTBOUND — it forwards
// whatever CoT XML it receives to the TAK server without
// parsing or inspecting the content. Vehicle position CoT
// is produced by pCoTContact and arrives here via COT_OUTBOUND.
// ============================================================

void CoTBridge::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("COT_OUTBOUND", 0);
}


// ============================================================
// OnNewMail()
// ============================================================

bool CoTBridge::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  for(auto& msg : NewMail) {
    if(msg.m_sKey == "COT_OUTBOUND") {
      debugLog("OnNewMail: COT_OUTBOUND (" +
               intToString((int)msg.m_sVal.size()) + " bytes)");
      if(isConnected()) {
        if(sendRawCoT(msg.m_sVal))
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

  if(isConnected())
    readInboundCoT();

  AppCastingMOOSApp::PostReport();
  return true;
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

  // TLS handshake (port 8089 only)
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
      SSL_free(m_ssl);
      m_ssl = nullptr;
      close(m_sock_fd);
      m_sock_fd = -1;
      return false;
    }

    debugLog("connectToTAKServer: TLS handshake OK — cipher=" +
             string(SSL_get_cipher(m_ssl)));
  }

  // Set non-blocking for reads after TLS handshake completes
  int flags = fcntl(m_sock_fd, F_GETFL, 0);
  fcntl(m_sock_fd, F_SETFL, flags | O_NONBLOCK);

  m_reconnect_count++;
  reportEvent("pCoTBridge: connected to " +
              m_tak_host + ":" + intToString(m_tak_port) +
              (m_use_tls ? " [TLS]" : " [plain]"));
  return true;
}


// ============================================================
// disconnectFromTAKServer()
// ============================================================

void CoTBridge::disconnectFromTAKServer()
{
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
// sendRawCoT()
// ============================================================

bool CoTBridge::sendRawCoT(const std::string& xml)
{
  if(m_sock_fd < 0) return false;

  int flags = fcntl(m_sock_fd, F_GETFL, 0);
  fcntl(m_sock_fd, F_SETFL, flags & ~O_NONBLOCK); // blocking for send

  string msg = xml + m_cot_delimiter;
  ssize_t n;

  if(m_use_tls && m_ssl)
    n = SSL_write(m_ssl, msg.c_str(), (int)msg.size());
  else
    n = send(m_sock_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);

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

  debugLog("sendRawCoT: " + intToString((int)n) + " bytes");
  return true;
}


// ============================================================
// readInboundCoT()
//
// Non-blocking reads from the TAK socket. Buffers bytes until
// a complete </event> is found, then publishes to COT_INBOUND.
//
// Echo filtering is intentionally removed — pCoTBridge no
// longer knows vehicle UIDs. Downstream apps (pCoTCommander,
// pCoTChat) ignore CoT types they don't act on, so extra
// inbound traffic is harmless.
// ============================================================

void CoTBridge::readInboundCoT()
{
  char buf[COT_BUF_SIZE];
  while(true) {
    ssize_t n;
    if(m_use_tls && m_ssl) {
      n = SSL_read(m_ssl, buf, sizeof(buf) - 1);
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
// ============================================================

bool CoTBridge::initTLSContext()
{
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  m_ssl_ctx = SSL_CTX_new(TLS_client_method());
  if(!m_ssl_ctx) {
    reportRunWarning("pCoTBridge: SSL_CTX_new() failed");
    return false;
  }

  if(!m_tls_cert_file.empty()) {
    if(SSL_CTX_use_certificate_file(m_ssl_ctx,
                                    m_tls_cert_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1) {
      reportRunWarning("pCoTBridge: failed to load cert: " + m_tls_cert_file);
      return false;
    }
    debugLog("initTLSContext: client cert loaded");
  }

  if(!m_tls_key_file.empty()) {
    SSL_CTX_set_default_passwd_cb(m_ssl_ctx,
      [](char* buf, int size, int /*rwflag*/, void* userdata) -> int {
        const char* pass = static_cast<const char*>(userdata);
        if(!pass || pass[0] == '\0') return 0;
        int len = (int)strlen(pass);
        if(len > size) len = size;
        memcpy(buf, pass, len);
        return len;
      });
    SSL_CTX_set_default_passwd_cb_userdata(
      m_ssl_ctx,
      m_tls_key_pass.empty() ? nullptr : (void*)m_tls_key_pass.c_str());

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
    if(SSL_CTX_check_private_key(m_ssl_ctx) != 1) {
      reportRunWarning("pCoTBridge: cert/key mismatch");
      return false;
    }
    debugLog("initTLSContext: private key loaded and verified");
  }

  if(!m_tls_ca_file.empty()) {
    int ca_ok = SSL_CTX_load_verify_locations(m_ssl_ctx,
                                              m_tls_ca_file.c_str(),
                                              nullptr);
    if(ca_ok != 1) {
      ERR_clear_error();
      X509_STORE* store = SSL_CTX_get_cert_store(m_ssl_ctx);
      X509_LOOKUP* lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
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
      debugLog("initTLSContext: CA cert loaded");
    }
  }

  SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_verify_depth(m_ssl_ctx, 4);

  reportEvent("pCoTBridge: TLS context initialized (mTLS)");
  return true;
}


// ============================================================
// teardownTLS()
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
  m_msgs << endl;
  m_msgs << "forwarded=" << m_outbound_forwarded
         << "  inbound="  << m_inbound_received
         << "  failures=" << m_send_failures << endl;

  if(m_debug && !m_debug_msgs.empty()) {
    m_msgs << endl << "-- debug --" << endl;
    for(const auto& dm : m_debug_msgs)
      m_msgs << "  " << dm << endl;
  }

  return true;
}
