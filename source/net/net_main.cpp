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
//**  Copyright (C) 2018-2.020 Ketmar Dark
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
#include "gamedefs.h"
#include "net_local.h"


static const char *cli_Port = nullptr;
//static int cli_Listen = 0;

/*static*/ bool cliRegister_netmain_args =
  VParsedArgs::RegisterStringOption("-port", "explicitly set your host port (default is 26000)", &cli_Port)
  /*&& VParsedArgs::RegisterFlagSet("-listen", nullptr, &cli_Listen)*/;


static VCvarS net_ui_last_join_address("net_ui_last_join_address", "127.0.0.1", "Last server address for manual connection (used in UI).", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
class VNetwork : public VNetworkLocal {
private:
  VNetPollProcedure SlistSendProcedure;
  VNetPollProcedure SlistPollProcedure;
  VNetPollProcedure MasterListSendProcedure;
  VNetPollProcedure MasterListPollProcedure;

  bool SlistInProgress;
  bool SlistSilent;
  bool SlistLocal;
  bool SlistSorted;
  bool SlistMaster;
  double SlistStartTime;
  int SlistLastShown;

  slist_t slist;

  VNetPollProcedure *PollProcedureList;

public:
  // public API
  VNetwork ();
  virtual ~VNetwork () override;

  virtual void Init () override;
  virtual void Shutdown () override;
  virtual VSocketPublic *Connect (const char *) override;
  virtual VSocketPublic *CheckNewConnections () override;
  virtual void Poll () override;
  virtual void StartSearch (bool) override;
  virtual slist_t *GetSlist () override;
  virtual void UpdateMaster () override;
  virtual void QuitMaster () override;

  // API only for network drivers!
  virtual void SchedulePollProcedure (VNetPollProcedure *, double) override;

  void MasterList ();
  virtual void Slist () override;

private:
  static void Slist_Send (void *);
  static void Slist_Poll (void *);
  void Slist_Send ();
  void Slist_Poll ();
  static void MasterList_Send (void *);
  static void MasterList_Poll (void *);
  void MasterList_Send ();
  void MasterList_Poll ();
  void PrintSlistHeader ();
  void PrintSlist ();
  void PrintSlistTrailer ();
};


// ////////////////////////////////////////////////////////////////////////// //
VNetworkPublic *GNet = nullptr;

VCvarS VNetworkLocal::HostName("hostname", "UNNAMED", "Name of this host.", CVAR_PreInit);

VNetDriver *VNetworkLocal::Drivers[MAX_NET_DRIVERS];
int VNetworkLocal::NumDrivers = 0;

VNetLanDriver *VNetworkLocal::LanDrivers[MAX_NET_DRIVERS];
int VNetworkLocal::NumLanDrivers = 0;


//==========================================================================
//
//  VNetworkPublic::Create
//
//==========================================================================
VNetworkPublic *VNetworkPublic::Create () {
  return new VNetwork();
}


//==========================================================================
//
//  VNetworkPublic::VNetworkPublic
//
//==========================================================================
VNetworkPublic::VNetworkPublic ()
  : UnreliableMessagesSent(0)
  , UnreliableMessagesReceived(0)
  , packetsSent(0)
  , packetsReSent(0)
  , packetsReceived(0)
  , receivedDuplicateCount(0)
  , shortPacketCount(0)
  , droppedDatagrams(0)
  , bytesSent(0)
  , bytesReceived(0)
  , bytesRejected(0)
  , minimalSentPacket(0)
  , maximalSentPacket(0)
  , CheckUserAbortCB(nullptr)
  , CheckUserAbortUData(nullptr)
  , CurrNetTime(-1)
{
}


//==========================================================================
//
//  VNetworkPublic::UpdateNetTime
//
//==========================================================================
void VNetworkPublic::UpdateNetTime () noexcept {
  CurrNetTime = Sys_Time();
}


//==========================================================================
//
//  VNetworkPublic::GetNetTime
//
//==========================================================================
double VNetworkPublic::GetNetTime () noexcept {
  if (CurrNetTime < 0) UpdateNetTime();
  return CurrNetTime;
}


//==========================================================================
//
//  VNetworkPublic::UpdateSentStats
//
//==========================================================================
void VNetworkPublic::UpdateSentStats (vuint32 length) noexcept {
  if (length) {
    bytesSent += length;
    if (!minimalSentPacket || minimalSentPacket > length) minimalSentPacket = length;
    if (maximalSentPacket < length) maximalSentPacket = length;
  }
}


//==========================================================================
//
//  VNetworkPublic::UpdateReceivedStats
//
//==========================================================================
void VNetworkPublic::UpdateReceivedStats (vuint32 length) noexcept {
  bytesReceived += length;
}


//==========================================================================
//
//  VNetworkPublic::UpdateRejectedStats
//
//==========================================================================
void VNetworkPublic::UpdateRejectedStats (vuint32 length) noexcept {
  bytesRejected += length;
}


//==========================================================================
//
//  VNetworkLocal::VNetworkLocal
//
//==========================================================================
VNetworkLocal::VNetworkLocal ()
  : VNetworkPublic()
  , ActiveSockets(nullptr)
  , HostCacheCount(0)
  , HostPort(0)
  , DefaultHostPort(26000)
  , IpAvailable(false)
  , Listening(false)
{
  MyIpAddress[0] = 0;
  ReturnReason[0] = 0;
  memset((void *)HostCache, 0, sizeof(HostCache));
}


//==========================================================================
//
//  VNetwork::VNetwork
//
//==========================================================================
VNetwork::VNetwork()
  : SlistSendProcedure(Slist_Send, this)
  , SlistPollProcedure(Slist_Poll, this)
  , MasterListSendProcedure(MasterList_Send, this)
  , MasterListPollProcedure(MasterList_Poll, this)
  , SlistInProgress(false)
  , SlistSilent(false)
  , SlistLocal(true)
  , SlistSorted(true)
  , SlistMaster(false)
  , SlistStartTime(0.0)
  , SlistLastShown(0)
  , PollProcedureList(nullptr)
{
}


//==========================================================================
//
//  VNetwork::~VNetwork
//
//==========================================================================
VNetwork::~VNetwork () {
  Shutdown();
}


//==========================================================================
//
//  VNetwork::Init
//
//==========================================================================
void VNetwork::Init () {
  const char *p = cli_Port;
  if (p && p[0]) {
    DefaultHostPort = VStr::atoi(p);
    if (DefaultHostPort < 1 || DefaultHostPort > 65535) DefaultHostPort = 26000;
  }
  HostPort = DefaultHostPort;

#ifdef CLIENT
  //if (cli_Listen || cls.state == ca_dedicated) Listening = true;
#else
  Listening = true;
#endif

  // initialise all the drivers
  for (int i = 0; i < NumDrivers; ++i) {
    Drivers[i]->Net = this;
    if (Drivers[i]->Init() != -1) {
      Drivers[i]->initialised = true;
      if (Listening) Drivers[i]->Listen(true);
    }
  }

  if (developer && *MyIpAddress) GCon->Logf(NAME_DevNet, "TCP/IP address %s", MyIpAddress);
}


//==========================================================================
//
//  VNetwork::Shutdown
//
//==========================================================================
void VNetwork::Shutdown () {
  while (ActiveSockets) {
    delete ActiveSockets;
    ActiveSockets = nullptr;
  }

  // shutdown the drivers
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised) {
      Drivers[i]->Shutdown();
      Drivers[i]->initialised = false;
    }
  }
}


//==========================================================================
//
//  VNetwork::Poll
//
//==========================================================================
void VNetwork::Poll () {
  UpdateNetTime();
  const double ctt = GetNetTime();
  for (VNetPollProcedure *pp = PollProcedureList; pp; pp = pp->next) {
    if (pp->nextTime > ctt) break;
    PollProcedureList = pp->next;
    pp->procedure(pp->arg);
  }
}


//==========================================================================
//
//  VNetwork::SchedulePollProcedure
//
//==========================================================================
void VNetwork::SchedulePollProcedure (VNetPollProcedure *proc, double timeOffset) {
  VNetPollProcedure *pp, *prev;

  proc->nextTime = Sys_Time()+timeOffset;
  for (pp = PollProcedureList, prev = nullptr; pp; pp = pp->next) {
    if (pp->nextTime >= proc->nextTime) break;
    prev = pp;
  }

  if (prev == nullptr) {
    proc->next = PollProcedureList;
    PollProcedureList = proc;
  } else {
    proc->next = pp;
    prev->next = proc;
  }
}


//==========================================================================
//
//  VNetwork::Slist
//
//==========================================================================
void VNetwork::Slist () {
  if (SlistInProgress) return;

  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Looking for k8vavoom servers...");
    PrintSlistHeader();
  }

  SlistMaster = false;
  SlistInProgress = true;
  SlistStartTime = Sys_Time();

  SchedulePollProcedure(&SlistSendProcedure, 0.0);
  SchedulePollProcedure(&SlistPollProcedure, 0.1);

  HostCacheCount = 0;
}


//==========================================================================
//
//  VNetwork::Slist_Send
//
//==========================================================================
void VNetwork::Slist_Send (void *Arg) {
  ((VNetwork *)Arg)->Slist_Send();
}


//==========================================================================
//
//  VNetwork::Slist_Poll
//
//==========================================================================
void VNetwork::Slist_Poll (void *Arg) {
  ((VNetwork *)Arg)->Slist_Poll();
}


//==========================================================================
//
//  VNetwork::Slist_Send
//
//==========================================================================
void VNetwork::Slist_Send () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!SlistLocal && i == 0) continue;
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->SearchForHosts(true, SlistMaster);
  }

  if ((Sys_Time()-SlistStartTime) < 0.5) SchedulePollProcedure(&SlistSendProcedure, 0.75);
}


//==========================================================================
//
//  VNetwork::Slist_Poll
//
//==========================================================================
void VNetwork::Slist_Poll () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!SlistLocal && i == 0) continue;
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->SearchForHosts(false, SlistMaster);
  }

  if (!SlistSilent) PrintSlist();

  if ((Sys_Time()-SlistStartTime) < 1.5) {
    SchedulePollProcedure(&SlistPollProcedure, 0.1);
    return;
  }

  if (!SlistSilent) PrintSlistTrailer();
  SlistInProgress = false;
  SlistSilent = false;
  SlistLocal = true;
  SlistSorted = false;
}


//==========================================================================
//
//  VNetwork::MasterList
//
//==========================================================================
void VNetwork::MasterList () {
  if (SlistInProgress) return;

  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Looking for k8vavoom servers...");
    PrintSlistHeader();
  }

  SlistMaster = true;
  SlistInProgress = true;
  SlistStartTime = Sys_Time();

  SchedulePollProcedure(&MasterListSendProcedure, 0.0);
  SchedulePollProcedure(&MasterListPollProcedure, 0.1);

  HostCacheCount = 0;
}


//==========================================================================
//
//  VNetwork::MasterList_Send
//
//==========================================================================
void VNetwork::MasterList_Send (void *Arg) {
  ((VNetwork *)Arg)->MasterList_Send();
}


//==========================================================================
//
//  VNetwork::MasterList_Poll
//
//==========================================================================
void VNetwork::MasterList_Poll (void *Arg) {
  ((VNetwork *)Arg)->MasterList_Poll();
}


//==========================================================================
//
//  VNetwork::MasterList_Send
//
//==========================================================================
void VNetwork::MasterList_Send () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->QueryMaster(true);
  }

  if ((Sys_Time()-SlistStartTime) < 0.5) SchedulePollProcedure(&MasterListSendProcedure, 0.75);
}


//==========================================================================
//
//  VNetwork::MasterList_Poll
//
//==========================================================================
void VNetwork::MasterList_Poll () {
  // check for reply from master server
  bool GotList = false;
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    if (Drivers[i]->QueryMaster(false)) GotList = true;
  }

  // if no reply, try again
  if (!GotList && (Sys_Time()-SlistStartTime) < 1.5) {
    SchedulePollProcedure(&MasterListPollProcedure, 0.1);
    return;
  }

  // close socket for communicating with master server
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised) Drivers[i]->EndQueryMaster();
  }

  // if we got list, server info command has been sent to all servers, so start listening for their replies
  if (GotList) {
    SlistStartTime = Sys_Time();
    SchedulePollProcedure(&SlistPollProcedure, 0.1);
    return;
  }

  // could not connect to the master server
  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Could not connect to the master server.");
  }

  SlistInProgress = false;
  SlistSilent = false;
  SlistLocal = true;
  SlistSorted = false;
}


//==========================================================================
//
//  VNetwork::PrintSlistHeader
//
//==========================================================================
void VNetwork::PrintSlistHeader () {
  GCon->Log(NAME_DevNet, "Server          Map             Users");
  GCon->Log(NAME_DevNet, "--------------- --------------- -----");
  SlistLastShown = 0;
}


//==========================================================================
//
//  VNetwork::PrintSlist
//
//==========================================================================
void VNetwork::PrintSlist () {
  int n;
  for (n = SlistLastShown; n < HostCacheCount; ++n) {
    if (HostCache[n].MaxUsers) {
      GCon->Logf(NAME_DevNet, "%-15s %-15s %2d/%2d", *HostCache[n].Name, *HostCache[n].Map, HostCache[n].Users, HostCache[n].MaxUsers);
    } else {
      GCon->Logf(NAME_DevNet, "%-15s %-15s", *HostCache[n].Name, *HostCache[n].Map);
    }
  }
  SlistLastShown = n;
}


//==========================================================================
//
//  VNetwork::PrintSlistTrailer
//
//==========================================================================
void VNetwork::PrintSlistTrailer () {
  if (HostCacheCount) {
    GCon->Log(NAME_DevNet, "== end list ==");
  } else {
    GCon->Log(NAME_DevNet, "No k8vavoom servers found.");
  }
  GCon->Log(NAME_DevNet, "");
}


//==========================================================================
//
//  VNetwork::StartSearch
//
//==========================================================================
void VNetwork::StartSearch (bool Master) {
  SlistSilent = true;
  SlistLocal = false;
  GCon->Logf(NAME_DevNet, "VNetwork::StartSearch: Master=%d", (int)Master);
  if (Master) {
    MasterList();
  } else {
    Slist();
  }
}


//==========================================================================
//
//  VNetwork::GetSlist
//
//==========================================================================
slist_t *VNetwork::GetSlist () {
  if (!SlistSorted) {
    if (HostCacheCount > 1) {
      vuint8 temp[sizeof(hostcache_t)];
      for (int i = 0; i < HostCacheCount; ++i) {
        for (int j = i+1; j < HostCacheCount; ++j) {
          if (HostCache[j].Name.Cmp(HostCache[i].Name) < 0) {
            memcpy(&temp, (void *)(&HostCache[j]), sizeof(hostcache_t));
            memcpy((void *)(&HostCache[j]), (void *)(&HostCache[i]), sizeof(hostcache_t));
            memcpy((void *)(&HostCache[i]), &temp, sizeof(hostcache_t));
          }
        }
      }
    }
    SlistSorted = true;
    memset(ReturnReason, 0, sizeof(ReturnReason));
  }

  if (SlistInProgress) {
    slist.Flags |= slist_t::SF_InProgress;
  } else {
    slist.Flags &= ~slist_t::SF_InProgress;
  }
  slist.Count = HostCacheCount;
  slist.Cache = HostCache;
  slist.ReturnReason = ReturnReason;
  return &slist;
}


//==========================================================================
//
//  VNetwork::Connect
//
//==========================================================================
VSocketPublic *VNetwork::Connect (const char *InHost) {
  VStr host = InHost;
  VSocket *ret;
  int numdrivers = NumDrivers;
  int n;

  if (host.IsNotEmpty()) {
    if (host == "local") {
      numdrivers = 1;
      goto JustDoIt;
    }

    if (HostCacheCount) {
      for (n = 0; n < HostCacheCount; ++n) {
        if (HostCache[n].Name.ICmp(host) == 0) {
          host = HostCache[n].CName;
          break;
        }
      }
      if (n < HostCacheCount) goto JustDoIt;
    }
  }

  SlistSilent = host.IsNotEmpty();
  Slist();

  while (SlistInProgress && !CheckForUserAbort()) {
    Poll();
  }

  if (host.IsEmpty()) {
    if (HostCacheCount != 1) return nullptr;
    host = HostCache[0].CName;
    GCon->Log(NAME_DevNet, "Connecting to...");
    GCon->Logf(NAME_DevNet, "%s @ %s", *HostCache[0].Name, *host);
  }

  if (HostCacheCount) {
    for (n = 0; n < HostCacheCount; ++n) {
      if (HostCache[n].Name.ICmp(host) == 0) {
        host = HostCache[n].CName;
        break;
      }
    }
  }

JustDoIt:
  for (int i = 0; i < numdrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    ret = Drivers[i]->Connect(*host);
    if (ret) return ret;
  }

  if (host.IsNotEmpty()) {
    PrintSlistHeader();
    PrintSlist();
    PrintSlistTrailer();
  }

  return nullptr;
}


//==========================================================================
//
//  VNetwork::CheckNewConnections
//
//==========================================================================
VSocketPublic *VNetwork::CheckNewConnections () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    if (i && Listening == false) continue;
    VSocket *ret = Drivers[i]->CheckNewConnections();
    if (ret) return ret;
  }
  return nullptr;
}


//==========================================================================
//
//  VNetwork::UpdateMaster
//
//==========================================================================
void VNetwork::UpdateMaster () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!Drivers[i]->initialised) continue;
    Drivers[i]->UpdateMaster();
  }
}


//==========================================================================
//
//  VNetwork::QuitMaster
//
//==========================================================================
void VNetwork::QuitMaster () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!Drivers[i]->initialised) continue;
    Drivers[i]->QuitMaster();
  }
}


//==========================================================================
//
//  VSocketPublic::u64str
//
//==========================================================================
VStr VSocketPublic::u64str (vuint64 v, bool comatose) noexcept {
  if (!v) return VStr("0");
  char buf[128];
  unsigned dpos = (unsigned)sizeof(buf);
  int digitsPut = (comatose ? 0 : 666);
  buf[--dpos] = 0;
  do {
    if (digitsPut == 3) {
      if (!dpos) abort();
      buf[--dpos] = ',';
      digitsPut = 0;
    }
    if (!dpos) abort();
    buf[--dpos] = '0'+(int)(v%10);
    ++digitsPut;
  } while (v /= 10);
  return VStr(buf+dpos);
}


//==========================================================================
//
//  VSocketPublic::DumpStats
//
//==========================================================================
void VSocketPublic::DumpStats () {
  if (bytesSent|bytesReceived|bytesRejected) {
    if (bytesRejected) {
      GCon->Logf(NAME_DevNet, "Closing stats for %s: %s bytes sent, %s bytes received (%s rejected)", *Address, *u64str(bytesSent), *u64str(bytesReceived), *u64str(bytesRejected));
    } else {
      GCon->Logf(NAME_DevNet, "Closing stats for %s: %s bytes sent, %s bytes received", *Address, *u64str(bytesSent), *u64str(bytesReceived));
    }
  }
}


//==========================================================================
//
//  VSocketPublic::UpdateSentStats
//
//==========================================================================
void VSocketPublic::UpdateSentStats (vuint32 length) noexcept {
  if (length) {
    bytesSent += length;
    if (!minimalSentPacket || minimalSentPacket > length) minimalSentPacket = length;
    if (maximalSentPacket < length) maximalSentPacket = length;
  }
}


//==========================================================================
//
//  VSocketPublic::UpdateReceivedStats
//
//==========================================================================
void VSocketPublic::UpdateReceivedStats (vuint32 length) noexcept {
  bytesReceived += length;
}


//==========================================================================
//
//  VSocketPublic::UpdateRejectedStats
//
//==========================================================================
void VSocketPublic::UpdateRejectedStats (vuint32 length) noexcept {
  bytesRejected += length;
}


//==========================================================================
//
//  VSocket::VSocket
//
//==========================================================================
VSocket::VSocket (VNetDriver *Drv) : Driver(Drv) {
  // add it to active list
  Next = Driver->Net->ActiveSockets;
  Driver->Net->ActiveSockets = this;

  Driver->Net->UpdateNetTime();
  ConnectTime = Driver->Net->GetNetTime();
  Address = "UNSET ADDRESS";
  LastMessageTime = ConnectTime;
}


//==========================================================================
//
//  VSocket::~VSocket
//
//==========================================================================
VSocket::~VSocket () {
  // remove it from active list
  if (this == Driver->Net->ActiveSockets) {
    Driver->Net->ActiveSockets = Driver->Net->ActiveSockets->Next;
  } else {
    VSocket *s = nullptr;
    for (s = Driver->Net->ActiveSockets; s; s = s->Next) {
      if (s->Next == this) {
        s->Next = this->Next;
        break;
      }
    }
    if (!s) Sys_Error("NET_FreeQSocket: not active");
  }
}


//==========================================================================
//
//  VSocket::UpdateSentStats
//
//==========================================================================
void VSocket::UpdateSentStats (vuint32 length) noexcept {
  VSocketPublic::UpdateSentStats(length);
  if (Driver && Driver->Net) Driver->Net->UpdateSentStats(length);
}


//==========================================================================
//
//  VSocket::UpdateReceivedStats
//
//==========================================================================
void VSocket::UpdateReceivedStats (vuint32 length) noexcept {
  VSocketPublic::UpdateReceivedStats(length);
  if (Driver && Driver->Net) Driver->Net->UpdateReceivedStats(length);
}


//==========================================================================
//
//  VSocket::UpdateRejectedStats
//
//==========================================================================
void VSocket::UpdateRejectedStats (vuint32 length) noexcept {
  VSocketPublic::UpdateRejectedStats(length);
  if (Driver && Driver->Net) Driver->Net->UpdateRejectedStats(length);
}


//==========================================================================
//
//  VNetDriver::VNetDriver
//
//==========================================================================
VNetDriver::VNetDriver (int Level, const char *AName) : name(AName), initialised(false) {
  VNetwork::Drivers[Level] = this;
  if (VNetwork::NumDrivers <= Level) VNetwork::NumDrivers = Level+1;
}


//==========================================================================
//
//  VNetLanDriver::VNetLanDriver
//
//==========================================================================
VNetLanDriver::VNetLanDriver (int Level, const char *AName)
  : name(AName)
  , initialised(false)
  , controlSock(-1)
  , MasterQuerySocket(-1)
  , net_acceptsocket(-1)
  , net_controlsocket(-1)
  , net_broadcastsocket(-1)
  , myAddr(0)
{
  memset(&broadcastaddr, 0, sizeof(broadcastaddr));
  VNetwork::LanDrivers[Level] = this;
  if (VNetwork::NumLanDrivers <= Level) VNetwork::NumLanDrivers = Level+1;
}


//==========================================================================
//
//  VNetUtils::TVMsecs
//
//==========================================================================
void VNetUtils::TVMsecs (timeval *dest, int msecs) noexcept {
  if (!dest) return;
  if (msecs < 0) msecs = 0;
  dest->tv_sec = msecs/1000;
  dest->tv_usec = msecs%1000;
  dest->tv_usec *= 1000;
}


//==========================================================================
//
//  VNetUtils::CRC32C
//
//  start with 0, continuous
//
//==========================================================================
vuint32 VNetUtils::CRC32C (vuint32 crc32, const void *buf, size_t length) noexcept {
  return crc32cBuffer(crc32, buf, length);
}

//==========================================================================
//
//  VNetUtils::ChaCha20SetupEx
//
//  Key size in bits: either 256 (32 bytes), or 128 (16 bytes)
//  Nonce size in bits: 64 (8 bytes)
//  returns 0 on success
//
//==========================================================================
int VNetUtils::ChaCha20SetupEx (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata, vuint32 keybits) noexcept {
  return chacha20_setup_ex((chacha20_ctx *)ctx, keydata, noncedata, keybits);
}


//==========================================================================
//
//  VNetUtils::ChaCha20XCrypt
//
//  encrypts or decrypts a full message
//  cypher is symmetric, so `ciphertextdata` and `plaintextdata`
//  can point to the same address
//
//==========================================================================
void VNetUtils::ChaCha20XCrypt (ChaCha20Ctx *ctx, void *ciphertextdata, const void *plaintextdata, vuint32 msglen) noexcept {
  return chacha20_xcrypt((chacha20_ctx *)ctx, ciphertextdata, plaintextdata, msglen);
}


//==========================================================================
//
//  VNetUtils::GenerateKey
//
//==========================================================================
void VNetUtils::GenerateKey (vuint8 key[ChaCha20KeySize]) noexcept {
  /*
  vuint32 *dest = (vuint32 *)key;
  for (int f = 0; f < ChaCha20KeySize/4; ++f) *dest++ = GenRandomU32();
  */
  prng_randombytes(key, ChaCha20KeySize);
}


//==========================================================================
//
//  VNetUtils::DerivePublicKey
//
//  derive public key from secret one
//
//==========================================================================
void VNetUtils::DerivePublicKey (uint8_t mypk[ChaCha20KeySize], const uint8_t mysk[ChaCha20KeySize]) {
  curve25519_donna_public(mypk, mysk);
}


//==========================================================================
//
//  VNetUtils::DeriveSharedKey
//
//  derive shared secret from our secret and their public
//
//==========================================================================
void VNetUtils::DeriveSharedKey (uint8_t sharedk[ChaCha20KeySize], const uint8_t mysk[ChaCha20KeySize], const uint8_t theirpk[ChaCha20KeySize]) {
  curve25519_donna_shared(sharedk, mysk, theirpk);
}


//==========================================================================
//
//  VNetUtils::EncryptInfoPacket
//
//  WARNING! cannot do it in-place
//  needs 24 extra bytes (key, nonce, crc)
//  returns new length or -1 on error
//
//==========================================================================
int VNetUtils::EncryptInfoPacket (void *destbuf, const void *srcbuf, int srclen, const vuint8 key[ChaCha20KeySize]) noexcept {
  if (srclen < 0) return -1;
  if (!destbuf) return -1;
  if (srclen > 0 && !srcbuf) return -1;
  //const vuint32 nonce = GenRandomU32();
  vuint32 nonce;
  prng_randombytes(&nonce, sizeof(nonce));
  vuint8 *dest = (vuint8 *)destbuf;
  // copy key
  memcpy(dest, key, ChaCha20KeySize);
  // copy nonce
  dest[ChaCha20KeySize+0] = nonce&0xffU;
  dest[ChaCha20KeySize+1] = (nonce>>8)&0xffU;
  dest[ChaCha20KeySize+2] = (nonce>>16)&0xffU;
  dest[ChaCha20KeySize+3] = (nonce>>24)&0xffU;
  // copy crc32
  const vuint32 crc32 = CRC32C(0, srcbuf, (unsigned)srclen);
  dest[ChaCha20KeySize+4] = crc32&0xffU;
  dest[ChaCha20KeySize+5] = (crc32>>8)&0xffU;
  dest[ChaCha20KeySize+6] = (crc32>>16)&0xffU;
  dest[ChaCha20KeySize+7] = (crc32>>24)&0xffU;
  // copy data
  if (srclen) memcpy(dest+ChaCha20HeaderSize, srcbuf, (unsigned)srclen);
  // encrypt crc32 and data
  ChaCha20Ctx cctx;
  ChaCha20Setup(&cctx, key, nonce);
  ChaCha20XCrypt(&cctx, dest+ChaCha20KeySize+ChaCha20NonceSize, dest+ChaCha20KeySize+ChaCha20NonceSize, (unsigned)(srclen+ChaCha20CheckSumSize));
  return srclen+ChaCha20HeaderSize;
}


//==========================================================================
//
//  VNetUtils::DecryptInfoPacket
//
//  it can decrypt in-place
//  returns new length or -1 on error
//  also sets key
//
//==========================================================================
int VNetUtils::DecryptInfoPacket (vuint8 key[ChaCha20KeySize], void *destbuf, const void *srcbuf, int srclen) noexcept {
  if (srclen < ChaCha20HeaderSize) return -1;
  if (!destbuf) return -1;
  if (!srcbuf) return -1;
  srclen -= ChaCha20KeySize+ChaCha20NonceSize; // key and nonce
  const vuint8 *src = (const vuint8 *)srcbuf;
  vuint8 *dest = (vuint8 *)destbuf;
  // get key
  memcpy(key, srcbuf, ChaCha20KeySize);
  // get nonce
  vuint32 nonce =
    ((vuint32)src[ChaCha20KeySize+0])|
    (((vuint32)src[ChaCha20KeySize+1])<<8)|
    (((vuint32)src[ChaCha20KeySize+2])<<16)|
    (((vuint32)src[ChaCha20KeySize+3])<<24);
  // decrypt packet
  ChaCha20Ctx cctx;
  ChaCha20Setup(&cctx, key, nonce);
  ChaCha20XCrypt(&cctx, dest, src+ChaCha20KeySize+ChaCha20NonceSize, (unsigned)srclen);
  // calculate and check crc32
  srclen -= ChaCha20CheckSumSize;
  vassert(srclen >= 0);
  vuint32 crc32 = CRC32C(0, dest+ChaCha20CheckSumSize, (unsigned)srclen);
  if ((crc32&0xff) != dest[0] ||
      ((crc32>>8)&0xff) != dest[1] ||
      ((crc32>>16)&0xff) != dest[2] ||
      ((crc32>>24)&0xff) != dest[3])
  {
    // oops
    return -1;
  }
  // copy decrypted data
  if (srclen > 0) memcpy(dest, dest+ChaCha20CheckSumSize, (unsigned)srclen);
  return srclen;
}



#if defined(CLIENT) && defined(SERVER) /* I think like this */
//==========================================================================
//
//  COMMAND Listen
//
//==========================================================================
COMMAND(Listen) {
  VNetwork *Net = (VNetwork *)GNet;
  if (Args.length() != 2) {
    GCon->Logf(NAME_DevNet, "\"listen\" is \"%d\"", Net->Listening ? 1 : 0);
    return;
  }
  Net->Listening = (VStr::atoi(*Args[1]) ? true : false);
  for (int i = 0; i < VNetwork::NumDrivers; ++i) {
    if (VNetwork::Drivers[i]->initialised == false) continue;
    VNetwork::Drivers[i]->Listen(Net->Listening);
  }
}
#endif


//==========================================================================
//
//  COMMAND Port
//
//==========================================================================
COMMAND(Port) {
  int n;

  VNetwork *Net = (VNetwork *)GNet;
  if (Args.length() != 2) {
    GCon->Logf(NAME_DevNet, "\"port\" is \"%d\"", Net->HostPort);
    return;
  }

  n = VStr::atoi(*Args[1]);
  if (n < 1 || n > 65534) {
    GCon->Log(NAME_Error, "Bad value, must be between 1 and 65534");
    return;
  }

  Net->DefaultHostPort = n;
  Net->HostPort = n;

  if (Net->Listening) {
    // force a change to the new port
    GCmdBuf << "listen 0\n";
    GCmdBuf << "listen 1\n";
  }
}


//==========================================================================
//
//  COMMAND Slist
//
//==========================================================================
COMMAND(Slist) {
  ((VNetwork *)GNet)->Slist();
}


//==========================================================================
//
//  COMMAND MasterList
//
//==========================================================================
COMMAND(MasterList) {
  ((VNetwork *)GNet)->MasterList();
}
