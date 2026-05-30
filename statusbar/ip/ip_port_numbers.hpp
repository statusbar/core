#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <cstdint>

namespace statusbar::ip {

///
/// Well-known port numbers for UDP and TCP protocols.
/// These are standardized by IANA (Internet Assigned Numbers Authority).
///
/// Port number ranges:
/// - 0-1023: Well-known (system) ports
/// - 1024-49151: Registered (user) ports
/// - 49152-65535: Dynamic/private ports
///
namespace port {

//
// System Services (0-1023)
//

/// File Transfer Protocol (data transfer)
constexpr uint16_t FTP_DATA = 20;

/// File Transfer Protocol (control)
constexpr uint16_t FTP_CONTROL = 21;

/// Secure Shell
constexpr uint16_t SSH = 22;

/// Telnet
constexpr uint16_t TELNET = 23;

/// Simple Mail Transfer Protocol
constexpr uint16_t SMTP = 25;

/// Domain Name System
constexpr uint16_t DNS = 53;

/// DHCP Server (Bootstrap Protocol Server)
constexpr uint16_t DHCP_SERVER = 67;

/// DHCP Client (Bootstrap Protocol Client)
constexpr uint16_t DHCP_CLIENT = 68;

/// Trivial File Transfer Protocol
constexpr uint16_t TFTP = 69;

/// Hypertext Transfer Protocol
constexpr uint16_t HTTP = 80;

/// Post Office Protocol v3
constexpr uint16_t POP3 = 110;

/// Network Time Protocol
constexpr uint16_t NTP = 123;

/// NetBIOS Name Service
constexpr uint16_t NETBIOS_NS = 137;

/// NetBIOS Datagram Service
constexpr uint16_t NETBIOS_DGM = 138;

/// NetBIOS Session Service
constexpr uint16_t NETBIOS_SSN = 139;

/// Internet Message Access Protocol
constexpr uint16_t IMAP = 143;

/// Simple Network Management Protocol
constexpr uint16_t SNMP = 161;

/// SNMP Trap
constexpr uint16_t SNMP_TRAP = 162;

/// Border Gateway Protocol
constexpr uint16_t BGP = 179;

/// Lightweight Directory Access Protocol
constexpr uint16_t LDAP = 389;

/// HTTPS (HTTP over TLS/SSL)
constexpr uint16_t HTTPS = 443;

/// Microsoft SMB (Server Message Block)
constexpr uint16_t SMB = 445;

/// Syslog
constexpr uint16_t SYSLOG = 514;

/// SMTP over TLS/SSL
constexpr uint16_t SMTPS = 465;

/// LDAP over TLS/SSL
constexpr uint16_t LDAPS = 636;

/// IMAP over TLS/SSL
constexpr uint16_t IMAPS = 993;

/// POP3 over TLS/SSL
constexpr uint16_t POP3S = 995;

//
// Registered Ports (1024-49151)
//

/// Microsoft SQL Server
constexpr uint16_t MSSQL = 1433;

/// Oracle Database
constexpr uint16_t ORACLE = 1521;

/// MySQL Database
constexpr uint16_t MYSQL = 3306;

/// Remote Desktop Protocol
constexpr uint16_t RDP = 3389;

/// PostgreSQL Database
constexpr uint16_t POSTGRESQL = 5432;

//
// Audio/Video Streaming
//

/// Real-time Transport Protocol (RTP)
constexpr uint16_t RTP = 5004;

/// RTP Control Protocol (RTCP)
constexpr uint16_t RTCP = 5005;

/// Multicast DNS (mDNS/Bonjour)
constexpr uint16_t MDNS = 5353;

/// IEEE 1722 AVTP Audio/Video Transport Protocol
constexpr uint16_t AVTP = 17220;

/// IEEE 1722.1 ATDECC
constexpr uint16_t ATDECC = 17221;

//
// Common Application Ports
//

/// Redis Database
constexpr uint16_t REDIS = 6379;

/// HTTP Alternate (commonly used for proxies)
constexpr uint16_t HTTP_ALT = 8080;

/// HTTPS Alternate
constexpr uint16_t HTTPS_ALT = 8443;

/// Elasticsearch HTTP
constexpr uint16_t ELASTICSEARCH = 9200;

/// MongoDB
constexpr uint16_t MONGODB = 27017;

}  // namespace port

}  // namespace statusbar::ip
