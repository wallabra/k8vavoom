//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <time.h>
#ifdef _WIN32
# include <windows.h>
typedef int socklen_t;
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
//#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>


#include "netchan.h"

#ifdef WIN32
static bool vnetchanSockInited = false;

class TWinSockHelper {
public:
  ~TWinSockHelper () { if (vnetchanSockInited) WSACleanup(); vnetchanSockInited = false; }
};

static TWinSockHelper vnetchanHelper__;
#endif


//==========================================================================
//
//  VNetChan::InitialiseSockets
//
//==========================================================================
bool VNetChan::InitialiseSockets () {
  #ifdef WIN32
  if (!vnetchanSockInited) {
    WSADATA winsockdata;
    //MAKEWORD(2, 2)
    int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);
    if (r) return false;
    vnetchanSockInited = true;
  }
  #endif
  return true;
}



//==========================================================================
//
//  GetSysTime
//
//  return valud should not be zero
//
//==========================================================================
static __attribute__((unused)) double GetSysTime () noexcept {
  #if defined(WIN32)
  uint32_t res = (uint32_t)GetTickCount();
  return (double)res/1000.0+1.0;
  #elif defined(__linux__)
  static bool initialized = false;
  static time_t secbase = 0;
  struct timespec ts;
  if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) abort();
  if (!initialized) {
    initialized = true;
    secbase = ts.tv_sec;
  }
  return (ts.tv_sec-secbase)+ts.tv_nsec/1000000000.0+1.0;
  #else
  static bool initialized = false;
  struct timeval tp;
  struct timezone tzp;
  static int secbase = 0;
  gettimeofday(&tp, &tzp);
  if (!initialized) {
    initialized = true;
    secbase = tp.tv_sec;
  }
  return (tp.tv_sec-secbase)+tp.tv_usec/1000000.0+1.0;
  #endif
}


//**************************************************************************
//
// VNetChanSocket
//
//**************************************************************************

//==========================================================================
//
//  VNetChanSocket::create
//
//==========================================================================
bool VNetChanSocket::create () noexcept {
  close();

  // create UDP socket
  sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == -1) return false;

  // make socket non-blocking
  int trueval = 1;
  #ifdef _WIN32
  if (ioctlsocket(sockfd, FIONBIO, (u_long*)&trueval) == -1)
  #else
  if (ioctl(sockfd, FIONBIO, (char*)&trueval) == -1)
  #endif
  {
    #ifdef _WIN32
    ::closesocket(sockfd);
    #else
    ::close(sockfd);
    #endif
    sockfd = -1;
    return false;
  }

  // bind socket to the port
  /*
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(MASTER_SERVER_PORT);
  if (bind(sockfd, (sockaddr *)&address, sizeof(address)) == -1) {
    closesocket(sockfd);
    Logf("Unable to bind socket to a port");
    return -1;
  }
  */

  return true;
}


//==========================================================================
//
//  VNetChanSocket::close
//
//==========================================================================
void VNetChanSocket::close () noexcept {
  if (sockfd >= 0) {
    #ifdef _WIN32
    ::closesocket(sockfd);
    #else
    ::close(sockfd);
    #endif
  }
  sockfd = -1;
}


//==========================================================================
//
//  VNetChanSocket::send
//
//==========================================================================
bool VNetChanSocket::send (const sockaddr *addr, const void *buf, int len) noexcept {
  if (sockfd < 0 || !addr) return false;
  if (len < 0) return false;
  if (len == 0) return true;
  if (!buf) return false;
  return (::sendto(sockfd, buf, len, 0, addr, sizeof(*addr)) == len);
}


//==========================================================================
//
//  VNetChanSocket::hasData
//
//==========================================================================
bool VNetChanSocket::hasData () noexcept {
  if (sockfd < 0) return false;
  uint8_t buf[MAX_DGRAM_SIZE];
  return (recvfrom(sockfd, buf, MAX_DGRAM_SIZE, MSG_PEEK, nullptr, nullptr) > 0);
}


//==========================================================================
//
//  VNetChanSocket::recv
//
//  <0: error
//  0: no data
//  >0: data length
//  `addr` is sender address
//
//==========================================================================
int VNetChanSocket::recv (sockaddr *addr, void *buf, int maxlen) noexcept {
  if (maxlen < 0) return -1;
  if (maxlen == 0) return 0;
  if (!buf) return -1;
  sockaddr tmpaddr;
  if (!addr) addr = &tmpaddr;
  socklen_t addrlen = sizeof(sockaddr);
  int len = (int)recvfrom(sockfd, buf, maxlen, 0, addr, &addrlen);
  if (len < 0) len = -1;
  return len;
}


//==========================================================================
//
//  VNetChanSocket::AddrToString
//
//==========================================================================
const char *VNetChanSocket::AddrToString (const sockaddr *addr) noexcept {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d:%d", (haddr>>24)&0xff, (haddr>>16)&0xff,
    (haddr>>8)&0xff, haddr&0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
}


//==========================================================================
//
//  VNetChanSocket::AddrToStringNoPort
//
//==========================================================================
const char *VNetChanSocket::AddrToStringNoPort (const sockaddr *addr) noexcept {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", (haddr>>24)&0xff, (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff);
  return buffer;
}


//==========================================================================
//
//  VNetChanSocket::AddrEqu
//
//==========================================================================
bool VNetChanSocket::AddrEqu (const sockaddr *addr1, const sockaddr *addr2) noexcept {
  if (addr1 == addr2) return (addr1 != nullptr);
  if (addr1->sa_family != addr2->sa_family) return false;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return false;
  if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return false;
  return true;
}


//==========================================================================
//
//  VNetChanSocket::GetAddrFromName
//
//==========================================================================
bool VNetChanSocket::GetAddrFromName (const char *hostname, sockaddr *addr, uint16_t port) noexcept {
  if (!addr) return false;
  memset((void *)addr, 0, sizeof(*addr));

  if (!hostname || !hostname[0]) return false;

  hostent *hostentry = gethostbyname(hostname);
  if (!hostentry) return false;

  addr->sa_family = AF_INET;
  ((sockaddr_in *)addr)->sin_port = htons(port);
  ((sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

  return true;
}



//**************************************************************************
//
// VNetChan
//
//**************************************************************************


//==========================================================================
//
//  VNetChan::VNetChan
//
//==========================================================================
VNetChan::VNetChan ()
  : sock()
  , lastSendSeq(0)
  , lastAckSeq(0)
  , resendCount(0)
  , lastSendTime(0)
  , lastRecvTime(0)
  , bOpenAcked(false)
  , bCloseAcked(false)
  , bCloseSent(false)
  , inQueue(nullptr)
  , outQueue(nullptr)
{
  memset((void *)&claddr, 0, sizeof(claddr));
}


//==========================================================================
//
//  VNetChan::VNetChan
//
//==========================================================================
VNetChan::~VNetChan () {
  close();
}


//==========================================================================
//
//  VNetChan::close
//
//==========================================================================
void VNetChan::close () {
  while (inQueue) {
    Message *msg = inQueue;
    inQueue = msg->next;
    delete msg;
  }

  while (outQueue) {
    Message *msg = outQueue;
    outQueue = msg->next;
    delete msg;
  }

  sock.close();

  lastSendSeq = 0;
  lastAckSeq = 0;
  resendCount = 0;
  lastSendTime = 0;
  lastRecvTime = 0;
  bOpenAcked = false;
  bCloseAcked = false;
  bCloseSent = false;

  memset((void *)&claddr, 0, sizeof(claddr));
}


//==========================================================================
//
//  VNetChan::canSendData
//
//==========================================================================
bool VNetChan::canSendData () const noexcept {
  if (!sock.isOpen()) return false;
  return true;
}


//==========================================================================
//
//  VNetChan::hasIncomingData
//
//==========================================================================
bool VNetChan::hasIncomingData () const noexcept {
  return (inQueue && inQueue->acked);
}


//==========================================================================
//
//  VNetChan::requestClose
//
//==========================================================================
bool VNetChan::requestClose () {
  if (!sock.isOpen()) return false;
  if (bCloseSent) return true;

  Message *msg = new Message;
  msg->len = 0;
  msg->next = nullptr;
  msg->pid = 0;
  msg->seq = ++lastSendSeq;
  msg->acked = false;
  msg->close = true;
  Message **mp = &outQueue;
  while (*mp) mp = &(*mp)->next;
  *mp = msg;
  bCloseSent = true;
  return true;
}


//==========================================================================
//
//  VNetChan::send
//
//==========================================================================
bool VNetChan::send (const void *buf, int len) {
  if (!sock.isOpen()) return false;
  if (len < 0) return false;
  if (len == 0) return true;
  if (len > MAX_DATA_SIZE) return false;
  if (!buf) return false;
  if (bCloseSent) return false;

  Message *msg = new Message;
  msg->len = len;
  memcpy(msg->data, buf, len);
  msg->next = nullptr;
  msg->pid = 0;
  msg->seq = ++lastSendSeq;
  msg->acked = false;
  msg->close = true;
  Message **mp = &outQueue;
  while (*mp) mp = &(*mp)->next;
  *mp = msg;
  bCloseSent = true;
  return true;
}


//==========================================================================
//
//  VNetChan::read
//
//==========================================================================
int VNetChan::read (void *buf, int maxlen) {
  return -1;
}


//==========================================================================
//
//  VNetChan::tick
//
//==========================================================================
void VNetChan::tick () {
}
