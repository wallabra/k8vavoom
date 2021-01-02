//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"
#include "net_local.h"

#ifdef WIN32
# include <windows.h>
# include <errno.h>
# define socklen_t  int
#else
# if !defined(__SWITCH__) && !defined(ANDROID)
#  include <ifaddrs.h>
# endif
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <errno.h>
# include <unistd.h>
# include <netdb.h>
# include <sys/ioctl.h>
# define closesocket close
#endif


static int cli_NoUDP = 0;
static const char *cli_IP = nullptr;

/*static*/ bool cliRegister_netudp_args =
  VParsedArgs::RegisterFlagSet("-noudp", "disable UDP networking", &cli_NoUDP) &&
  VParsedArgs::RegisterAlias("-no-udp", "-noudp") &&
  VParsedArgs::RegisterStringOption("-ip", "explicitly set your IP address", &cli_IP);


static VCvarB net_dbg_dump_udp_inbuffer("net_dbg_dump_udp_inbuffer", false, "Dump UDP input buffer size?");
static VCvarB net_dbg_dump_udp_outbuffer("net_dbg_dump_udp_outbuffer", false, "Dump UDP output buffer size?");

// ////////////////////////////////////////////////////////////////////////// //
class VUdpDriver : public VNetLanDriver {
public:
#ifdef WIN32
  int winsock_initialised;
  WSADATA winsockdata;

  static double blocktime;
  bool mGetLocAddrCalled;
#endif

  enum { INET_HOST_NAME_SIZE = 256 };

  VUdpDriver ();
  virtual int Init () override;
  virtual void Shutdown () override;
  virtual void Listen (bool state) override;
  virtual int OpenListenSocket (int) override;
  virtual int ConnectSocketTo (sockaddr_t *addr) override; // required for UDP
  virtual bool CloseSocket (int) override; // returns `false` on error
  virtual int CheckNewConnections (bool rconOnly) override;
  virtual int Read (int, vuint8 *, int, sockaddr_t *) override;
  virtual int Write (int, const vuint8 *, int, sockaddr_t *) override;
  virtual int Broadcast (int, const vuint8 *, int) override;
  virtual bool CanBroadcast () override;
  virtual const char *AddrToString (sockaddr_t *) override;
  virtual const char *AddrToStringNoPort (sockaddr_t *) override;
  virtual int StringToAddr (const char *, sockaddr_t *) override;
  virtual int GetSocketAddr (int, sockaddr_t *) override;
  virtual const char *GetNameFromAddr (sockaddr_t *) override;
  virtual int GetAddrFromName (const char *, sockaddr_t *, int) override;
  // returns:
  //   -1 if completely not equal
  //    0 if completely equal
  //    1 if only ips are equal
  virtual int AddrCompare (const sockaddr_t *, const sockaddr_t *) override;
  // used to not reject connections from localhost
  virtual bool IsLocalAddress (const sockaddr_t *addr) override;
  virtual int GetSocketPort (const sockaddr_t *) override;
  virtual int SetSocketPort (sockaddr_t *, int) override;
  virtual bool FindExternalAddress (sockaddr_t *addr) override;

#ifdef WIN32
  static BOOL PASCAL FAR BlockingHook ();
  void GetLocalAddress ();
#endif

  int PartialIPAddress (const char *, sockaddr_t *, int);

private:
  static bool SetNonBlocking (int fd) noexcept;
};


#ifdef WIN32
double VUdpDriver::blocktime;
#endif
static VUdpDriver Impl;

#define IPAddrBufMax  (32)
static thread_local char ipaddrbuf[64][IPAddrBufMax];
static thread_local unsigned ipaddrbufcurr = 0;


//==========================================================================
//
//  VUdpDriver::VUdpDriver
//
//==========================================================================
VUdpDriver::VUdpDriver ()
  : VNetLanDriver(0, "UDP")
#ifdef WIN32
  , winsock_initialised(0)
  , mGetLocAddrCalled(false)
#endif
{
}


//==========================================================================
//
//  VUdpDriver::Init
//
//==========================================================================
int VUdpDriver::Init () {
  char buff[INET_HOST_NAME_SIZE];

  if (cli_NoUDP > 0) return -1;

  #ifdef WIN32
  if (winsock_initialised == 0) {
    int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);
    if (r) {
      GCon->Log(NAME_Init, "Winsock initialisation failed.");
      return -1;
    }
  }
  ++winsock_initialised;
  #endif

  // determine my name & address
  auto ghres = gethostname(buff, INET_HOST_NAME_SIZE);
  #ifdef WIN32
  if (ghres == SOCKET_ERROR) {
    GCon->Log(NAME_DevNet, "Winsock TCP/IP Initialisation failed.");
    if (--winsock_initialised == 0) WSACleanup();
    return -1;
  }
  #else
  if (ghres == -1) {
    GCon->Log(NAME_DevNet, "Cannot get host name, defaulting to 'localhost'.");
    strcpy(buff, "localhost");
  }
  #endif
  GCon->Logf(NAME_Init, "Host name: %s", buff);

  const char *pp = cli_IP;
  if (!pp) pp = "any";
  if (pp && pp[0]) {
    if (VStr::strEquCI(pp, "any") || VStr::strEquCI(pp, "all")) {
      myAddr = INADDR_ANY;
      VStr::Cpy(Net->MyIpAddress, "INADDR_ANY");
    } else {
      #ifdef _WIN32
      myAddr = inet_addr(pp);
      if (myAddr == INADDR_NONE) Sys_Error("'%s' is not a valid IP address", pp);
      VStr::Cpy(Net->MyIpAddress, pp);
      #else
      struct in_addr addr;
      if (inet_aton(pp, &addr) == 0) Sys_Error("'%s' is not a valid IP address", pp);
      VStr::Cpy(Net->MyIpAddress, inet_ntoa(addr));
      myAddr = addr.s_addr;
      #endif
    }
  } else {
    #ifdef WIN32
    myAddr = INADDR_ANY;
    VStr::Cpy(Net->MyIpAddress, "INADDR_ANY");
    #elif defined(__SWITCH__)
    myAddr = gethostid();
    // if wireless is currently down and/or the nifm service is not up,
    // gethostid() will return 127.0.0.1 in network order
    // thanks nintendo (?)
    if (myAddr == 0x7f000001) myAddr = ntohl(myAddr);
    #else
    hostent *local = gethostbyname(buff);
    if (!local) {
      // do not crash, it is unfair!
      GCon->Logf(NAME_Warning, "UDP_Init: Couldn't get local host by name '%s', check your /etc/hosts file.", buff);
      myAddr = inet_addr("127.0.0.1");
      VStr::Cpy(Net->MyIpAddress, "127.0.0.1");
      if (myAddr == INADDR_NONE) Sys_Error("'127.0.0.1' is not a valid IP address (why?!)");
    } else {
      myAddr = *(int *)local->h_addr_list[0];
      Net->MyIpAddress[0] = 0;
    }
    #endif
  }

  #ifndef WIN32
  {
    sockaddr_t exaddr;
    if (FindExternalAddress(&exaddr)) {
      //VStr::Cpy(Net->MyIpAddress, AddrToStringNoPort(&exaddr));
      //GCon->Logf(NAME_Init, "UDP external address guessed as %s", Net->MyIpAddress);
      GCon->Logf(NAME_Init, "UDP external address guessed as %s", AddrToStringNoPort(&exaddr));
      //if (myAddr == INADDR_ANY) myAddr = ((sockaddr_in *)&exaddr)->sin_addr.s_addr;
    }
  }
  #endif

  // if the k8vavoom hostname isn't set, set it to the machine name
  if (VStr::Cmp(Net->HostName, "UNNAMED") == 0) {
    #ifdef WIN32
    char *p;
    // see if it's a text IP address (well, close enough)
    for (p = buff; *p; ++p) if ((*p < '0' || *p > '9') && *p != '.') break;
    // if it is a real name, strip off the domain; we only want the host
    if (*p) {
      int i;
      for (i = 0; i < 15; ++i) if (buff[i] == '.') break;
      buff[i] = 0;
    }
    #else
    buff[15] = 0;
    #endif
    Net->HostName = buff;
  }

  if ((net_controlsocket = /*OpenListenSocket(0)*/ConnectSocketTo(nullptr)) == -1) {
    #ifdef WIN32
    GCon->Log(NAME_Init, "WINS_Init: Unable to open control socket");
    if (--winsock_initialised == 0) WSACleanup();
    #else
    //Sys_Error("UDP_Init: Unable to open control socket");
    GCon->Log(NAME_Warning, "UDP_Init: Unable to open control socket");
    #endif
    return -1;
  }

  ((sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
  ((sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
  ((sockaddr_in *)&broadcastaddr)->sin_port = htons((vuint16)Net->HostPort);

  if (Net->MyIpAddress[0] == 0) {
    VStr::Cpy(Net->MyIpAddress, "127.0.0.1");
  }
  /*
  #ifndef WIN32
  if (Net->MyIpAddress[0] == 0) {
    sockaddr_t addr;
    GetSocketAddr(net_controlsocket, &addr);
    VStr::Cpy(Net->MyIpAddress, AddrToString(&addr));
    char *colon = strrchr(Net->MyIpAddress, ':');
    if (colon) *colon = 0;
    //GCon->Logf(NAME_Init, "My IP address: %s", Net->MyIpAddress);
  }
  {
    sockaddr_t addr;
    GetSocketAddr(net_controlsocket, &addr);
    GCon->Logf(NAME_Init, "UDP control socket address is %s", AddrToString(&addr));
  }
  #endif
  */

  GCon->Logf(NAME_Init, "UDP Initialised on %s", Net->MyIpAddress);
  Net->IpAvailable = true;

  return net_controlsocket;
}


//==========================================================================
//
//  VUdpDriver::Shutdown
//
//==========================================================================
void VUdpDriver::Shutdown () {
  Listen(false);
  CloseSocket(net_controlsocket);
  #ifdef WIN32
  if (--winsock_initialised == 0) WSACleanup();
  #endif
}


#ifdef WIN32
//==========================================================================
//
//  VUdpDriver::BlockingHook
//
//==========================================================================
BOOL PASCAL FAR VUdpDriver::BlockingHook () {
  MSG msg;
  BOOL ret;

  if ((Sys_Time()-blocktime) > 2.0) {
    WSACancelBlockingCall();
    return FALSE;
  }

  // get the next message, if any
  ret = (BOOL)PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);

  // if we got one, process it
  if (ret) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // TRUE if we got a message
  return ret;
}


//==========================================================================
//
//  VUdpDriver::GetLocalAddress
//
//==========================================================================
void VUdpDriver::GetLocalAddress () {
  hostent *local;
  char buff[INET_HOST_NAME_SIZE];
  vuint32 addr;

  if (myAddr != INADDR_ANY) return;

  if (mGetLocAddrCalled) return;
  mGetLocAddrCalled = true;

  if (gethostname(buff, INET_HOST_NAME_SIZE) == SOCKET_ERROR) {
    myAddr = inet_addr("127.0.0.1");
    VStr::Cpy(Net->MyIpAddress, "127.0.0.1");
    if (myAddr == INADDR_NONE) Sys_Error("'127.0.0.1' is not a valid IP address (why?!)");
    return;
  }

  blocktime = Sys_Time();
  WSASetBlockingHook(FARPROC(BlockingHook));
  local = gethostbyname(buff);
  WSAUnhookBlockingHook();
  if (local == nullptr) return;

  myAddr = *(int *)local->h_addr_list[0];

  addr = ntohl(myAddr);
  snprintf(Net->MyIpAddress, sizeof(Net->MyIpAddress), "%d.%d.%d.%d", (addr>>24)&0xff, (addr>>16)&0xff, (addr>>8)&0xff, addr&0xff);
}
#endif


//==========================================================================
//
//  VUdpDriver::Listen
//
//==========================================================================
void VUdpDriver::Listen (bool state) {
  if (state) {
    // enable listening
    if (net_acceptsocket == -1) {
      net_acceptsocket = OpenListenSocket(Net->HostPort);
      if (net_acceptsocket == -1) Sys_Error("UDP_Listen: Unable to open accept socket");
      GCon->Logf(NAME_DevNet, "created listening socket");
    }
  } else {
    // disable listening
    if (net_acceptsocket != -1) {
      CloseSocket(net_acceptsocket);
      net_acceptsocket = -1;
    }
  }
}


//==========================================================================
//
//  VUdpDriver::SetNonBlocking
//
//  switch to non-blocking mode
//
//==========================================================================
bool VUdpDriver::SetNonBlocking (int fd) noexcept {
  if (fd < 0) return -1;
  #ifdef WIN32
  DWORD trueval = 1;
  if (ioctlsocket(fd, FIONBIO, &trueval) == -1)
  #else
  u_long trueval = 1;
  if (ioctl(fd, FIONBIO, (char *)&trueval) == -1)
  #endif
  {
    return false;
  }
  return true;
}


//==========================================================================
//
//  VUdpDriver::OpenListenSocket
//
//==========================================================================
int VUdpDriver::OpenListenSocket (int port) {
  int newsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (newsocket == -1) return -1;

  if (!SetNonBlocking(newsocket)) {
    closesocket(newsocket);
    return -1;
  }

  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = myAddr;
  address.sin_port = htons((vuint16)port);
  if (bind(newsocket, (sockaddr *)&address, sizeof(address)) == 0) return newsocket;

  const int err = errno;
  closesocket(newsocket);

  Sys_Error("Unable to bind to %s:%u (err=%d)", AddrToStringNoPort((sockaddr_t *)&address), port, err);
  return -1;
}


//==========================================================================
//
//  VUdpDriver::ConnectSocketTo
//
//  required for UDP
//
//==========================================================================
int VUdpDriver::ConnectSocketTo (sockaddr_t *addr) {
  int newsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (newsocket == -1) return -1;

  if (!SetNonBlocking(newsocket)) {
    closesocket(newsocket);
    return -1;
  }

  return newsocket;
  /* k8: for some reason, after `connect` we cannot use this socket in `recvfrom()` anymore
         (it always return "no data yet). wtf?!
  GCon->Logf(NAME_DevNet, "binding new socket connection to %s", AddrToString(addr));
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = (((const sockaddr_in *)addr)->sin_addr.s_addr);
  address.sin_port = ((sockaddr_in *)addr)->sin_port;

  if (connect(newsocket, (sockaddr *)&address, sizeof(address)) == 0) {
    sockaddr_t skad;
    socklen_t skadlen = sizeof(sockaddr_t);
    memset(&skad, 0, sizeof(sockaddr_t));
    getsockname(newsocket, (sockaddr *)&skad, &skadlen);
    GCon->Logf(NAME_DevNet, "(bound to local address %s)", AddrToString(&skad));
    return newsocket;
  }

  const int err = errno;
  closesocket(newsocket);

  GCon->Logf(NAME_Error, "Unable to bind to %s (err=%d)", AddrToString(addr), err);
  return -1;
  */
}


//==========================================================================
//
//  VUdpDriver::CloseSocket
//
//==========================================================================
bool VUdpDriver::CloseSocket (int socket) {
  if (socket < 0) return true;
  if (socket == net_broadcastsocket) net_broadcastsocket = -1;
  return (closesocket(socket) == 0);
}


//==========================================================================
//
//  VUdpDriver::CheckNewConnections
//
//==========================================================================
int VUdpDriver::CheckNewConnections (bool rconOnly) {
  char buf[4096];
  if (net_acceptsocket == -1) return -1;
  if (recvfrom(net_acceptsocket, buf, sizeof(buf), MSG_PEEK, nullptr, nullptr) >= 0) {
    return net_acceptsocket;
  }
  return -1;
}


//==========================================================================
//
//  VUdpDriver::Read
//
//  returns:
//    length
//    -1: error
//    -2: no message
//
//==========================================================================
int VUdpDriver::Read (int socket, vuint8 *buf, int len, sockaddr_t *addr) {
  #ifndef WIN32
  if (net_dbg_dump_udp_inbuffer) {
    int value = 0;
    if (ioctl(socket, FIONREAD, &value) == 0) GCon->Logf(NAME_DevNet, "VUdpDriver::Read: FIONREAD=%d", value);
  }
  #endif
  socklen_t addrlen = sizeof(sockaddr_t);
  memset((void *)addr, 0, addrlen);
  int ret = recvfrom(socket, (char *)buf, len, 0, (sockaddr *)addr, &addrlen);
  if (ret >= 0) return ret;
  #ifdef WIN32
  int e = WSAGetLastError();
  if (e == WSAEWOULDBLOCK || e == EAGAIN) return -2;
  #else
  if (errno == EWOULDBLOCK || errno == EAGAIN) return -2;
  #endif
  return -1;
}


//==========================================================================
//
//  VUdpDriver::Write
//
//  returns:
//    length
//    -1: error
//    -2: outgoung queue is full
//
//==========================================================================
int VUdpDriver::Write (int socket, const vuint8 *buf, int len, sockaddr_t *addr) {
  #if !defined(WIN32) && !defined(__SWITCH__) && !defined(__CYGWIN__)
  if (net_dbg_dump_udp_outbuffer) {
    int value = 0;
    if (ioctl(socket, TIOCOUTQ, &value) == 0) GCon->Logf(NAME_DevNet, "VUdpDriver::Write:000: TIOCOUTQ=%d", value);
  }
  #endif
  int ret = sendto(socket, (const char *)buf, len, 0, (sockaddr *)addr, sizeof(sockaddr));
  #if !defined(WIN32) && !defined(__SWITCH__) && !defined(__CYGWIN__)
  if (net_dbg_dump_udp_outbuffer) {
    int value = 0;
    if (ioctl(socket, TIOCOUTQ, &value) == 0) GCon->Logf(NAME_DevNet, "VUdpDriver::Write:001: TIOCOUTQ=%d (res=%d)", value, ret);
  }
  #endif
  if (ret >= 0) return ret;
  #ifdef WIN32
  int e = WSAGetLastError();
  if (e == WSAEWOULDBLOCK || e == EAGAIN) return -2;
  #else
  if (errno == EWOULDBLOCK || errno == EAGAIN) return -2;
  #endif
  return -1;
}


//==========================================================================
//
//  VUdpDriver::CanBroadcast
//
//==========================================================================
bool VUdpDriver::CanBroadcast () {
  #ifdef WIN32
  GetLocalAddress();
  #endif
  #if 0
  vuint32 addr = ntohl(myAddr);
  return (myAddr != INADDR_ANY && ((addr>>24)&0xff) != 0x7f); // ignore localhost
  #else
  vuint32 addr = ntohl(myAddr);
  return (myAddr == INADDR_ANY || ((addr>>24)&0xff) != 0x7f); // ignore localhost
  #endif
}


//==========================================================================
//
//  VUdpDriver::Broadcast
//
//==========================================================================
int VUdpDriver::Broadcast (int socket, const vuint8 *buf, int len) {
  if (socket != net_broadcastsocket) {
    if (net_broadcastsocket >= 0) Sys_Error("Attempted to use multiple broadcasts sockets");
    if (!CanBroadcast()) {
      GCon->Log(NAME_DevNet, "Unable to make socket broadcast capable (1)");
      return -1;
    }
    // make this socket broadcast capable
    int i = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0) {
      GCon->Log(NAME_DevNet, "Unable to make socket broadcast capable");
      return -1;
    }
    net_broadcastsocket = socket;
  }
  return Write(socket, buf, len, &broadcastaddr);
}


//==========================================================================
//
//  VUdpDriver::AddrToString
//
//==========================================================================
const char *VUdpDriver::AddrToString (sockaddr_t *addr) {
  //char buffer[32];
  const unsigned bidx = ipaddrbufcurr++;
  ipaddrbufcurr &= (IPAddrBufMax-1);
  char *buffer = ipaddrbuf[bidx];
  int haddr = ntohl(((sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(ipaddrbuf[0]), "%d.%d.%d.%d:%d", (haddr>>24)&0xff,
    (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
}


//==========================================================================
//
//  VUdpDriver::AddrToStringNoPort
//
//==========================================================================
const char *VUdpDriver::AddrToStringNoPort (sockaddr_t *addr) {
  //char buffer[32];
  const unsigned bidx = ipaddrbufcurr++;
  ipaddrbufcurr &= (IPAddrBufMax-1);
  char *buffer = ipaddrbuf[bidx];
  int haddr = ntohl(((sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(ipaddrbuf[0]), "%d.%d.%d.%d", (haddr>>24)&0xff,
    (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff);
  return buffer;
}


//==========================================================================
//
//  VUdpDriver::StringToAddr
//
//==========================================================================
int VUdpDriver::StringToAddr (const char *string, sockaddr_t *addr) {
  int ha1, ha2, ha3, ha4, hp;
  int ipaddr;

  sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
  ipaddr = (ha1<<24)|(ha2<<16)|(ha3<<8)|ha4;

  addr->sa_family = AF_INET;
  ((sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
  ((sockaddr_in *)addr)->sin_port = htons((vuint16)hp);
  return 0;
}


//==========================================================================
//
//  VUdpDriver::GetSocketAddr
//
//==========================================================================
int VUdpDriver::GetSocketAddr (int socket, sockaddr_t *addr) {
  socklen_t addrlen = sizeof(sockaddr_t);
  vuint32 a;

  memset(addr, 0, sizeof(sockaddr_t));
  getsockname(socket, (sockaddr *)addr, &addrlen);
  if (myAddr != INADDR_ANY) {
    a = ((sockaddr_in *)addr)->sin_addr.s_addr;
    if (a == 0 || a == inet_addr("127.0.0.1")) ((sockaddr_in *)addr)->sin_addr.s_addr = myAddr;
  }

  return 0;
}


//==========================================================================
//
//  VUdpDriver::GetNameFromAddr
//
//==========================================================================
const char *VUdpDriver::GetNameFromAddr (sockaddr_t *addr) {
  hostent *hostentry = gethostbyaddr((char *)&((sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
  if (hostentry) return (char *)hostentry->h_name;
  return AddrToString(addr);
}


//==========================================================================
//
//  VUdpDriver::PartialIPAddress
//
//  This lets you type only as much of the net address as required, using
//  the local network components to fill in the rest
//
//==========================================================================
int VUdpDriver::PartialIPAddress (const char *in, sockaddr_t *hostaddr, int DefaultPort) {
  char buff[256];
  char *b;
  int addr;
  int num;
  int mask;
  int run;
  int port;

  buff[0] = '.';
  b = buff;
  VStr::Cpy(buff+1, in);
  if (buff[1] == '.') ++b;

  addr = 0;
  mask = -1;
  while (*b == '.') {
    ++b;
    num = 0;
    run = 0;
    while (!(*b < '0' || *b > '9')) {
      num = num*10+(*b++)-'0';
      if (++run > 3) return -1;
    }
    if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0) return -1;
    if (num < 0 || num > 255) return -1;
    mask <<= 8;
    addr = (addr<<8)+num;
  }

  if (*b++ == ':') port = VStr::atoi(b); else port = DefaultPort;

  hostaddr->sa_family = AF_INET;
  ((sockaddr_in *)hostaddr)->sin_port = htons((short)port);
  ((sockaddr_in *)hostaddr)->sin_addr.s_addr = (myAddr&htonl(mask))|htonl(addr);

  return 0;
}


//==========================================================================
//
//  VUdpDriver::GetAddrFromName
//
//  TODO: IPv6
//
//==========================================================================
int VUdpDriver::GetAddrFromName (const char *name, sockaddr_t *addr, int DefaultPort) {
  hostent *hostentry;

  if (name[0] >= '0' && name[0] <= '9') return PartialIPAddress(name, addr, DefaultPort);

  hostentry = gethostbyname(name);
  if (!hostentry) return -1;

  addr->sa_family = AF_INET;
  ((sockaddr_in *)addr)->sin_port = htons(DefaultPort);
  ((sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

  return 0;
}


//==========================================================================
//
//  VUdpDriver::AddrCompare
//
//  returns:
//    -1 if completely not equal
//     0 if completely equal
//     1 if only ips are equal
//
//==========================================================================
int VUdpDriver::AddrCompare (const sockaddr_t *addr1, const sockaddr_t *addr2) {
  if (addr1->sa_family != addr2->sa_family) return -1;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return -1;
  if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return 1;
  return 0;
}


//==========================================================================
//
//  VUdpDriver::IsLocalAddress
//
//  used to not reject connections from localhost
//
//==========================================================================
bool VUdpDriver::IsLocalAddress (const sockaddr_t *addr) {
  int haddr = ntohl(((sockaddr_in *)addr)->sin_addr.s_addr);
  return
    (((haddr>>24)&0xff) == 127) ||
    (((haddr>>24)&0xff) == 10) || //10.0.0.0/8
    ((((haddr>>24)&0xff) == 172) && (((haddr>>16)&0xf0) == 16)) || //172.16.0.0/12
    ((((haddr>>24)&0xff) == 192) && (((haddr>>16)&0xff) == 168)); //192.168.0.0/16
}


//==========================================================================
//
//  VUdpDriver::GetSocketPort
//
//==========================================================================
int VUdpDriver::GetSocketPort (const sockaddr_t *addr) {
  return ntohs(((const sockaddr_in *)addr)->sin_port);
}


//==========================================================================
//
//  VUdpDriver::SetSocketPort
//
//==========================================================================
int VUdpDriver::SetSocketPort (sockaddr_t *addr, int port) {
  ((sockaddr_in *)addr)->sin_port = htons(port);
  return 0;
}


//==========================================================================
//
//  VUdpDriver::FindExternalAddress
//
//==========================================================================
bool VUdpDriver::FindExternalAddress (sockaddr_t *addr) {
  #if defined(WIN32) || defined(__SWITCH__) || defined(ANDROID)
  return false;
  #else
  ifaddrs *ifAddrStruct = nullptr;
  ifaddrs *ifa = nullptr;

  bool found = false;

  getifaddrs(&ifAddrStruct);
  for (ifa = ifAddrStruct; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) continue;
    if (ifa->ifa_addr->sa_family != AF_INET) continue;
    if (IsLocalAddress((struct sockaddr_t *)ifa->ifa_addr)) continue;
    if (found) return false; // two addresses
    found = true;
    if (addr) *addr = *(struct sockaddr_t *)ifa->ifa_addr;
  }
  if (ifAddrStruct) freeifaddrs(ifAddrStruct);
  return found;
  #endif
}
