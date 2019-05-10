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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#ifndef VAVOOM_NET_LOCAL_HEADER
#define VAVOOM_NET_LOCAL_HEADER

#include "network.h"


// ////////////////////////////////////////////////////////////////////////// //
#define NET_NAMELEN  (64)

#define MAX_NET_DRIVERS  (8)

#define HOSTCACHESIZE  (8)

#define NET_DATAGRAMSIZE  (MAX_MSGLEN)


// ////////////////////////////////////////////////////////////////////////// //
class VNetDriver;
class VNetLanDriver;


// ////////////////////////////////////////////////////////////////////////// //
struct sockaddr_t {
  vint16 sa_family;
  vint8 sa_data[14];
};


// ////////////////////////////////////////////////////////////////////////// //
class VSocket : public VSocketPublic {
public:
  VSocket *Next;
  VNetDriver *Driver;

  VSocket (VNetDriver *);
  virtual ~VSocket () override;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VNetPollProcedure {
  VNetPollProcedure *next;
  double nextTime;
  void (*procedure) (void *);
  void *arg;

  VNetPollProcedure () : next(nullptr), nextTime(0.0), procedure(nullptr), arg(nullptr) {}
  VNetPollProcedure (void (*aProcedure) (void *), void *aArg) : next(nullptr), nextTime(0.0), procedure(aProcedure), arg(aArg) {}
};


// ////////////////////////////////////////////////////////////////////////// //
class VNetworkLocal : public VNetworkPublic {
public:
  VSocket *ActiveSockets;

  int HostCacheCount;
  hostcache_t HostCache[HOSTCACHESIZE];

  int HostPort;
  int DefaultHostPort;

  char MyIpAddress[NET_NAMELEN];

  bool IpAvailable;

  char ReturnReason[32];

  bool Listening;

  static VNetDriver *Drivers[MAX_NET_DRIVERS];
  static int NumDrivers;

  static VNetLanDriver *LanDrivers[MAX_NET_DRIVERS];
  static int NumLanDrivers;

  static VCvarS HostName;

public:
  VNetworkLocal ();
  virtual void SchedulePollProcedure (VNetPollProcedure *, double) = 0;

  virtual void Slist () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
class VNetDriver : public VInterface {
public:
  const char *name;
  bool initialised;
  VNetworkLocal *Net;

  VNetDriver (int, const char *);
  virtual int Init () = 0;
  virtual void Listen (bool) = 0;
  virtual void SearchForHosts (bool, bool ForMaster) = 0;
  virtual VSocket *Connect (const char *host) = 0;
  virtual VSocket *CheckNewConnections () = 0;
  virtual void UpdateMaster () = 0;
  virtual void QuitMaster () = 0;
  virtual bool QueryMaster (bool) = 0;
  virtual void EndQueryMaster () = 0;
  virtual void Shutdown () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
class VNetLanDriver : public VInterface {
public:
  const char *name;
  bool initialised;
  int controlSock;
  int MasterQuerySocket;
  VNetworkLocal *Net;

  int net_acceptsocket; // socket for fielding new connections
  int net_controlsocket;
  int net_broadcastsocket;
  sockaddr_t broadcastaddr;

  vuint32 myAddr;

  VNetLanDriver (int, const char *);
  virtual int Init () = 0;
  virtual void Shutdown () = 0;
  virtual void Listen (bool state) = 0;
  virtual int OpenSocket (int port) = 0;
  virtual int CloseSocket (int socket) = 0;
  virtual int Connect (int, sockaddr_t *) = 0;
  virtual int CheckNewConnections () = 0;
  virtual int Read (int socket, vuint8 *buf, int len, sockaddr_t *addr) = 0;
  virtual int Write (int socket, const vuint8 *buf, int len, sockaddr_t *addr) = 0;
  virtual bool CanBroadcast () = 0;
  virtual int Broadcast (int socket, const vuint8 *buf, int len) = 0;
  virtual char *AddrToString (sockaddr_t *addr) = 0;
  virtual int StringToAddr (const char *string, sockaddr_t *addr) = 0;
  virtual int GetSocketAddr (int socket, sockaddr_t *addr) = 0;
  virtual VStr GetNameFromAddr (sockaddr_t *addr) = 0;
  virtual int GetAddrFromName (const char *name, sockaddr_t *addr,int DefaultPort) = 0;
  virtual int AddrCompare (const sockaddr_t *addr1, const sockaddr_t *addr2) = 0;
  virtual int GetSocketPort (const sockaddr_t *addr) = 0;
  virtual int SetSocketPort (sockaddr_t *addr, int port) = 0;
};


#endif
