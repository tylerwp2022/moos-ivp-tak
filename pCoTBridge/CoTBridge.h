/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: CoTBridge.h                                     */
/*    DATE: April 2026                                      */
/*                                                          */
/*  pCoTBridge — pure CoT transport layer.                  */
/*                                                          */
/*  Maintains a TCP/TLS connection to a TAK server and      */
/*  moves bytes in both directions:                         */
/*                                                          */
/*    COT_OUTBOUND (MOOSDB) → TAK server socket             */
/*    TAK server socket → COT_INBOUND (MOOSDB)             */
/*                                                          */
/*  pCoTBridge has no knowledge of vehicles, callsigns,     */
/*  or CoT content. It does not build or parse CoT XML.     */
/*  Content generation is handled by sibling apps:          */
/*                                                          */
/*    pCoTContact       → vehicle SA position CoT           */
/*    pCoTShoreContact  → static position CoT               */
/*    pCoTGraphics      → VIEW_* map overlay CoT            */
/*    pCoTChat          → GeoChat CoT                       */
/*    pCoTCommander     → inbound command dispatch          */
/*                                                          */
/*  Supports plain TCP (port 8088) and mTLS (port 8089).    */
/*                                                          */
/*  MOOS Interface:                                         */
/*    Subscribes: COT_OUTBOUND (raw CoT XML strings)        */
/*    Publishes:  COT_INBOUND  (raw CoT XML strings)        */
/************************************************************/

#ifndef COT_BRIDGE_HEADER
#define COT_BRIDGE_HEADER

#include <string>
#include <deque>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

#include <openssl/ssl.h>
#include <openssl/err.h>


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
  // TCP/TLS connection management
  // --------------------------------------------------------
  bool connectToTAKServer();
  void disconnectFromTAKServer();
  bool isConnected() const { return m_sock_fd >= 0; }
  bool sendRawCoT(const std::string& xml);
  void readInboundCoT();

  // --------------------------------------------------------
  // TLS
  // --------------------------------------------------------
  bool initTLSContext();
  void teardownTLS();

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
  // TLS (port 8089 only)
  // --------------------------------------------------------
  bool        m_use_tls;
  SSL_CTX*    m_ssl_ctx;
  SSL*        m_ssl;
  std::string m_tls_cert_file;
  std::string m_tls_key_file;
  std::string m_tls_ca_file;
  std::string m_tls_key_pass;

  // --------------------------------------------------------
  // Debug
  // --------------------------------------------------------
  bool m_debug;
  static const int DEBUG_BUF_SIZE = 8;
  std::deque<std::string> m_debug_msgs;

  // --------------------------------------------------------
  // Diagnostics
  // --------------------------------------------------------
  unsigned int m_outbound_forwarded;
  unsigned int m_inbound_received;
  unsigned int m_send_failures;
  unsigned int m_reconnect_count;
};

#endif // COT_BRIDGE_HEADER
