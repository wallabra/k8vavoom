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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
//**
//**  Datagram driver, handles all LAN drivers
//**
//**************************************************************************
//
// This is the network info/connection protocol. It is used to find k8vavoom
// servers, get info about them, and connect to them. Once connected, the
// k8vavoom game protocol (documented elsewhere) is used.
//
//
// CCREQ_CONNECT
//    bytes     "K8VAVOOM"
//    vuint8    net_protocol_version  NET_PROTOCOL_VERSION
//    vuint32   modlisthash
//    vuint16   modlistcount
//
// CCREQ_SERVER_INFO
//    bytes     "K8VAVOOM"
//    vuint8    net_protocol_version  NET_PROTOCOL_VERSION
//
//
// CCREP_ACCEPT
//    long    port
//
// CCREP_REJECT
//    string    reason
//    bool      extflags (always false)
//    bool      haswads
//      if haswads is true, mod list follows
//      modlist is asciz strings, terminated with empty string
//
// CCREP_SERVER_INFO
//    string    host_name
//    string    level_name
//    vuint8    current_players
//    vuint8    max_players
//    vuint8    protocol_version    NET_PROTOCOL_VERSION
//    vuint32   modlisthash
//    asciiz strings with loaded archive names, terminated with empty string
//
//**************************************************************************
#include "gamedefs.h"
#include "net_local.h"


static int cli_NoLAN = 0;

/*static*/ bool cliRegister_datagram_args =
  VParsedArgs::RegisterFlagSet("-nolan", "disable networking", &cli_NoLAN) &&
  VParsedArgs::RegisterAlias("-no-lan", "-nolan");


static VCvarB net_dbg_dump_rejected_connections("net_dbg_dump_rejected_connections", false, "Dump rejected connections?");


// ////////////////////////////////////////////////////////////////////////// //
class VDatagramSocket : public VSocket {
public:
  VNetLanDriver *LanDriver;
  int LanSocket;
  sockaddr_t Addr;
  bool Invalid;

public:
  VDatagramSocket (VNetDriver *Drv) : VSocket(Drv), LanDriver(nullptr), LanSocket(0), Invalid(false) {}
  virtual ~VDatagramSocket() override;

  virtual int GetMessage (void *dest, size_t destSize) override;
  virtual int SendMessage (const vuint8 *Data, vuint32 Length) override;
  virtual bool IsLocalConnection () const noexcept override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDatagramDriver : public VNetDriver {
public:
  enum { MASTER_SERVER_PORT = 26002 };
  enum { MASTER_PROTO_VERSION = 1 };

  // client request
  enum {
    CCREQ_CONNECT     = 1,
    CCREQ_SERVER_INFO = 2,
  };

  // server reply
  enum {
    CCREP_ACCEPT      = 11,
    CCREP_REJECT      = 12,
    CCREP_SERVER_INFO = 13,
  };

  // master server request
  enum {
    MCREQ_JOIN = 1,
    MCREQ_QUIT = 2,
    MCREQ_LIST = 3,
  };

  // master server reply
  enum {
    MCREP_LIST = 1,
  };

  struct {
    vuint8 data[MAX_DGRAM_SIZE];
  } packetBuffer;

public:
  VDatagramDriver ();

  virtual int Init () override;
  virtual void Listen (bool) override;
  virtual void SearchForHosts (bool, bool) override;
  virtual VSocket *Connect (const char *) override;
  virtual VSocket *CheckNewConnections () override;
  virtual void UpdateMaster () override;
  virtual void QuitMaster () override;
  virtual bool QueryMaster (bool) override;
  virtual void EndQueryMaster () override;
  virtual void Shutdown () override;

  void SearchForHosts (VNetLanDriver *, bool, bool);
  VSocket *Connect (VNetLanDriver *, const char *);
  VSocket *CheckNewConnections (VNetLanDriver *Drv);
  void SendConnectionReject (VNetLanDriver *Drv, VStr reason, int acceptsock, sockaddr_t clientaddr, bool sendModList=false);
  void UpdateMaster (VNetLanDriver *);
  void QuitMaster (VNetLanDriver *);
  bool QueryMaster (VNetLanDriver *, bool);

  static void WriteGameSignature (VBitStreamWriter &strm);
  static bool CheckGameSignature (VBitStreamReader &strm);

  static void WritePakList (VBitStreamWriter &strm);
  static bool ReadPakList (TArray<VStr> &list, VBitStreamReader &strm); // won't clear list
};


// ////////////////////////////////////////////////////////////////////////// //
extern int num_connected;
extern TArray<VStr> fsysWadFileNames; // this is from corelib

static VCvarB UseMaster("master_allowed", false, "Is communication with master server allowed?", CVAR_Archive);
static VCvarS MasterSrv("master_address", "ketmar.no-ip.org", "Master server domain name.", CVAR_Archive);

static VDatagramDriver Impl;


//==========================================================================
//
//  VDatagramDriver::VDatagramDriver
//
//==========================================================================
VDatagramDriver::VDatagramDriver () : VNetDriver(1, "Datagram") {
  memset(&packetBuffer, 0, sizeof(packetBuffer));
}


//==========================================================================
//
//  VDatagramDriver::WriteGameSignature
//
//==========================================================================
void VDatagramDriver::WriteGameSignature (VBitStreamWriter &strm) {
  const char *sign = "K8VAVOOM";
  strm.Serialise((void *)sign, 8);
}


//==========================================================================
//
//  VDatagramDriver::CheckGameSignature
//
//==========================================================================
bool VDatagramDriver::CheckGameSignature (VBitStreamReader &strm) {
  char sign[8];
  if (strm.IsError()) return false;
  memset(sign, 0, sizeof(sign));
  strm.Serialise(sign, 8);
  if (strm.IsError()) return false;
  return (memcmp(sign, "K8VAVOOM", 8) == 0);
}


//==========================================================================
//
//  VDatagramDriver::WritePakList
//
//==========================================================================
void VDatagramDriver::WritePakList (VBitStreamWriter &strm) {
  // write names
  auto list = FL_GetWadPk3List();
  for (int f = 0; f < list.length(); ++f) {
    VStr pak = list[f].extractFileName();
    if (pak.length() == 0) {
      // this is base pak
      pak = list[f];
    } else {
      pak = pak.stripExtension();
    }
    if (pak.length() == 0) continue;
    //GCon->Logf(NAME_Debug, "%d: <%s>", f, *pak);
    //if (pak.length() > 255) pak = pak.right(255); // just in case
    if (strm.GetNumBits()+pak.length()*8+8 > MAX_DGRAM_SIZE*8-8) break;
    for (int sidx = 0; sidx < pak.length(); ++sidx) {
      vuint8 ch = (vuint8)(pak[sidx]);
      strm << ch;
    }
    vuint8 zero = 0;
    strm << zero;
  }
  // write trailing zero
  {
    vuint8 zero = 0;
    strm << zero;
  }
}


//==========================================================================
//
//  VDatagramDriver::ReadPakList
//
//  won't clear list
//
//==========================================================================
bool VDatagramDriver::ReadPakList (TArray<VStr> &list, VBitStreamReader &strm) {
  char buf[MAX_DGRAM_SIZE+2];
  while (!strm.AtEnd()) {
    int bufpos = 0;
    for (;;) {
      vuint8 ch = 0;
      strm << ch;
      if (strm.IsError()) return false;
      if (bufpos >= (int)sizeof(buf)-2) return false;
      buf[bufpos++] = ch;
      if (!ch) break;
    }
    if (buf[0] == 0) return true; // done
    list.append(VStr(buf));
  }
  return false;
}


//==========================================================================
//
//  VDatagramDriver::Init
//
//==========================================================================
int VDatagramDriver::Init () {
  if (cli_NoLAN > 0) return -1;

  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    VNetworkLocal::LanDrivers[i]->Net = Net;
    int csock = VNetworkLocal::LanDrivers[i]->Init();
    if (csock == -1) continue;
    VNetworkLocal::LanDrivers[i]->initialised = true;
    VNetworkLocal::LanDrivers[i]->controlSock = csock;
  }

  return 0;
}


//==========================================================================
//
//  VDatagramDriver::Listen
//
//==========================================================================
void VDatagramDriver::Listen (bool state) {
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (VNetworkLocal::LanDrivers[i]->initialised) VNetworkLocal::LanDrivers[i]->Listen(state);
  }
}


//==========================================================================
//
//  VDatagramDriver::SearchForHosts
//
//==========================================================================
void VDatagramDriver::SearchForHosts (VNetLanDriver *Drv, bool xmit, bool ForMaster) {
  sockaddr_t myaddr;
  sockaddr_t readaddr;
  int len;
  vuint8 control;
  vuint8 msgtype;
  int n;
  vuint8 TmpByte;

  Drv->GetSocketAddr(Drv->controlSock, &myaddr);
  if (xmit && Drv->CanBroadcast()) {
    VBitStreamWriter Reply(MAX_DGRAM_SIZE<<3);
    TmpByte = NETPACKET_CTL;
    Reply << TmpByte;
    WriteGameSignature(Reply);
    TmpByte = CCREQ_SERVER_INFO;
    Reply << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION;
    Reply << TmpByte;
    Drv->Broadcast(Drv->controlSock, Reply.GetData(), Reply.GetNumBytes());
  }

  //GCon->Logf(NAME_Debug, "SearchForHosts: trying to read a datagram (me:%s)...", *Drv->AddrToString(&myaddr));
  int pktleft = 128;
  while (pktleft-- > 0 && (len = Drv->Read(Drv->controlSock, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr)) > 0) {
    if (len < (int)sizeof(int)) continue;

    // don't answer our own query
    if (!ForMaster && Drv->AddrCompare(&readaddr, &myaddr) >= 0) continue;

    // is the cache full?
    //if (Net->HostCacheCount == HOSTCACHESIZE) continue;

    GCon->Logf(NAME_DevNet, "SearchForHosts: got datagram from %s (len=%d)", *Drv->AddrToString(&readaddr), len);

    VBitStreamReader msg(packetBuffer.data, len<<3);
    msg << control;
    if (msg.IsError() || control != NETPACKET_CTL) continue;
    if (!CheckGameSignature(msg)) continue;

    msg << msgtype;
    if (msg.IsError() || msgtype != CCREP_SERVER_INFO) continue;

    VStr str;
    VStr addr = Drv->AddrToString(&readaddr);

    GCon->Logf(NAME_DevNet, "SearchForHosts: got valid packet from %s", *Drv->AddrToString(&readaddr));

    // search the cache for this server
    for (n = 0; n < Net->HostCacheCount; ++n) {
      if (addr.strEqu(Net->HostCache[n].CName)) break;
    }

    // is it already there?
    if (n < Net->HostCacheCount) continue;

    if (Net->HostCacheCount == HOSTCACHESIZE) {
      GCon->Logf(NAME_DevNet, "too many hosts, ignoring...");
      continue;
    }

    // add it
    ++Net->HostCacheCount;
    vassert(n >= 0 && n < Net->HostCacheCount);
    hostcache_t *hinfo = &Net->HostCache[n];
    hinfo->Flags = 0;
    msg << str;
    hinfo->Name = str;
    msg << str;
    hinfo->Map = str;
    msg << TmpByte;
    hinfo->Users = TmpByte;
    msg << TmpByte;
    hinfo->MaxUsers = TmpByte;
    msg << TmpByte;
    if (TmpByte == NET_PROTOCOL_VERSION) hinfo->Flags |= hostcache_t::Flag_GoodProtocol;
    vuint32 mhash = 0;
    msg << mhash;
    //GCon->Logf(NAME_DevNet, " WHASH: theirs=0x%08x  mine=0x%08x", mhash, SV_GetModListHash());
    if (mhash == SV_GetModListHash()) hinfo->Flags |= hostcache_t::Flag_GoodWadList;
    hinfo->CName = addr;
    hinfo->WadFiles.clear();
    ReadPakList(hinfo->WadFiles, msg);
    if (msg.IsError()) {
      // remove it
      hinfo->WadFiles.clear();
      --Net->HostCacheCount;
      continue;
    }
    //GCon->Logf(NAME_DevNet, " wcount: %d %d", hinfo->WadFiles.length(), FL_GetWadPk3List().length());
    if ((hinfo->Flags&hostcache_t::Flag_GoodWadList) && hinfo->WadFiles.length() != FL_GetWadPk3List().length()) {
      hinfo->Flags &= ~hostcache_t::Flag_GoodWadList;
    }
    //GCon->Logf(NAME_DevNet, " flags: 0x%04x", hinfo->Flags);

    GCon->Logf(NAME_DevNet, "SearchForHosts: got server info from %s: name=%s; map=%s; users=%d; maxusers=%d; flags=0x%02x", *hinfo->CName, *hinfo->Name, *hinfo->Map, hinfo->Users, hinfo->MaxUsers, hinfo->Flags);
    //for (int f = 0; f < hinfo->WadFiles.length(); ++f) GCon->Logf("  %d: <%s>", f, *hinfo->WadFiles[f]);

    // check for a name conflict
    /*
    for (int i = 0; i < Net->HostCacheCount; ++i) {
      if (i == n) continue;
      if (Net->HostCache[n].Name.strEquCI(Net->HostCache[i].Name)) {
        i = Net->HostCache[n].Name.Length();
        if (i < 15 && Net->HostCache[n].Name[i-1] > '8') {
          Net->HostCache[n].Name += '0';
        } else {
          ++(*Net->HostCache[n].Name.GetMutableCharPointer(i-1));
          //Net->HostCache[n].Name[i - 1]++;
        }
        i = -1;
      }
    }
    */
  }
}


//==========================================================================
//
//  VDatagramDriver::SearchForHosts
//
//==========================================================================
void VDatagramDriver::SearchForHosts (bool xmit, bool ForMaster) {
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (Net->HostCacheCount == HOSTCACHESIZE) break;
    if (VNetworkLocal::LanDrivers[i]->initialised) SearchForHosts(VNetworkLocal::LanDrivers[i], xmit, ForMaster);
  }
}


//==========================================================================
//
//  VDatagramDriver::Connect
//
//==========================================================================
VSocket *VDatagramDriver::Connect (VNetLanDriver *Drv, const char *host) {
#ifdef CLIENT
  sockaddr_t sendaddr;
  sockaddr_t readaddr;
  VDatagramSocket *sock;
  int newsock;
  double start_time;
  int reps;
  int ret;
  vuint8 control;
  VStr reason;
  vuint8 msgtype;
  int newport;
  VBitStreamReader *msg = nullptr;
  vuint8 TmpByte;

  if (!host || !host[0]) return nullptr;

  SCR_Update();
  R_OSDMsgReset(OSD_Network);

  R_OSDMsgShow(va("getting address for [%s]", host));

  // see if we can resolve the host name
  if (Drv->GetAddrFromName(host, &sendaddr, Net->HostPort) == -1) return nullptr;

  //R_OSDMsgShow("creating socket");
  R_OSDMsgShow(va("connecting to [%s]", *Drv->AddrToString(&sendaddr)));

  newsock = Drv->ConnectSocketTo(&sendaddr);
  if (newsock == -1) return nullptr;

  sock = new VDatagramSocket(this);
  sock->LanSocket = newsock;
  sock->LanDriver = Drv;
  sock->Addr = sendaddr;
  sock->Address = Drv->AddrToString(&sock->Addr);

  // send the connection request
  GCon->Logf(NAME_DevNet, "trying %s", *Drv->AddrToString(&sendaddr));
  //SCR_Update();
  start_time = Net->GetNetTime();

  for (reps = 0; reps < 3; ++reps) {
    if (Net->CheckForUserAbort()) { ret = 0; break; }

    R_OSDMsgShow("sending handshake");

    VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
    // save space for the header, filled in later
    TmpByte = NETPACKET_CTL;
    MsgOut << TmpByte;
    WriteGameSignature(MsgOut);
    TmpByte = CCREQ_CONNECT;
    MsgOut << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION;
    MsgOut << TmpByte;
    vuint32 modhash = SV_GetModListHash();
    MsgOut << modhash;
    vuint16 modcount = (vuint16)FL_GetWadPk3List().length();
    MsgOut << modcount;
    Drv->Write(newsock, MsgOut.GetData(), MsgOut.GetNumBytes(), &sendaddr);

    bool aborted = false;
    do {
      ret = Drv->Read(newsock, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr);
      // if we got something, validate it
      if (ret > 0) {
        // is it from the right place?
        if (sock->LanDriver->AddrCompare(&readaddr, &sendaddr) != 0) {
          ret = 0;
          continue;
        }

        if (ret < (int)sizeof(vint32)) {
          ret = 0;
          continue;
        }

        msg = new VBitStreamReader(packetBuffer.data, ret<<3);

        *msg << control;
        if (msg->IsError() || control != NETPACKET_CTL) {
          ret = 0;
          delete msg;
          msg = nullptr;
          continue;
        }
        if (!CheckGameSignature(*msg)) {
          ret = 0;
          delete msg;
          msg = nullptr;
          continue;
        }
      }

      if (ret == 0) { aborted = Net->CheckForUserAbort(); if (aborted) break; }
    } while (ret == 0 && (Net->GetNetTime()-start_time) < 2.5);
    if (ret || aborted) break;
    GCon->Logf(NAME_DevNet, "still trying %s", *Drv->AddrToString(&sendaddr));
    //SCR_Update();
    start_time = Net->GetNetTime();
  }

  //SCR_Update();

  if (ret == 0) {
    reason = "No Response";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  if (ret == -1) {
    reason = "Network Error";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  *msg << msgtype;
  if (msgtype == CCREP_REJECT) {
    *msg << reason;
    GCon->Logf(NAME_Error, "Connection rejected: %s", *reason);
    VStr::NCpy(Net->ReturnReason, *reason, 31);
    if (!msg->ReadBit()) {
      if (msg->ReadBit()) {
        TArray<VStr> list;
        if (ReadPakList(list, *msg)) {
          if (list.length()) {
            GCon->Log(NAME_Error, "=== SERVER REJECTED CONNETION; WAD LIST: ===");
            for (auto &&pak : list) GCon->Logf(NAME_Error, "  %s", *pak);
          }
        }
      }
    }
    goto ErrorReturn;
  }

  if (msgtype != CCREP_ACCEPT) {
    reason = "Bad Response";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  *msg << newport;

  // switch the connection to the specified address
  memcpy(&sock->Addr, &readaddr, sizeof(sockaddr_t));
  Drv->SetSocketPort(&sock->Addr, newport);
  sock->Address = Drv->AddrToString(&sock->Addr);

  GCon->Logf(NAME_DevNet, "Connection accepted at %s (redirected to port %u)", *sock->Address, newport);
  sock->LastMessageTime = Net->GetNetTime();

  delete msg;
  msg = nullptr;

  R_OSDMsgShow("receiving initial data");

  //m_return_onerror = false;
  return sock;

ErrorReturn:
  delete sock;
  sock = nullptr;
  Drv->CloseSocket(newsock);
  if (msg) {
    delete msg;
    msg = nullptr;
  }
  SCR_Update();
#endif
  return nullptr;
}


//==========================================================================
//
//  VDatagramDriver::Connect
//
//==========================================================================
VSocket *VDatagramDriver::Connect (const char *host) {
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (VNetworkLocal::LanDrivers[i]->initialised) {
      VSocket *ret = Connect(VNetworkLocal::LanDrivers[i], host);
      if (ret) return ret;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VDatagramDriver::SendConnectionReject
//
//==========================================================================
void VDatagramDriver::SendConnectionReject (VNetLanDriver *Drv, VStr reason, int acceptsock, sockaddr_t clientaddr, bool sendModList) {
  vuint8 TmpByte;
  VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
  TmpByte = NETPACKET_CTL;
  MsgOut << TmpByte;
  WriteGameSignature(MsgOut);
  TmpByte = CCREP_REJECT;
  MsgOut << TmpByte;
  MsgOut << reason;
  // "false" means "simple format"
  MsgOut.WriteBit(false);
  if (!sendModList) {
    // no list
    MsgOut.WriteBit(false);
  } else {
    // list flag
    MsgOut.WriteBit(true);
    // list itself
    WritePakList(MsgOut);
  }
  Drv->Write(acceptsock, MsgOut.GetData(), MsgOut.GetNumBytes(), &clientaddr);
}


//==========================================================================
//
//  VDatagramDriver::CheckNewConnections
//
//==========================================================================
VSocket *VDatagramDriver::CheckNewConnections (VNetLanDriver *Drv) {
#ifdef SERVER
  sockaddr_t clientaddr;
  sockaddr_t newaddr;
  int acceptsock;
  int newsock;
  int len;
  vuint8 control;
  vuint8 command;
  VDatagramSocket *sock;
  int ret;
  vuint8 TmpByte;
  VStr TmpStr;

  acceptsock = Drv->CheckNewConnections();
  if (acceptsock == -1) return nullptr;

  len = Drv->Read(acceptsock, packetBuffer.data, MAX_DGRAM_SIZE, &clientaddr);
  if (len < (int)sizeof(vint32)) {
    if (len >= 0 && net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: too short packet (%d) from %s", len, *Drv->AddrToString(&clientaddr));
    if (len < 0) GCon->Logf(NAME_DevNet, "CONN: error reading incoming packet from %s", *Drv->AddrToString(&clientaddr));
    return nullptr;
  }
  GCon->Logf(NAME_DevNet, "CONN: ACCEPTING something from %s", *Drv->AddrToString(&clientaddr));

  VBitStreamReader msg(packetBuffer.data, len<<3);

  msg << control;
  if (msg.IsError() || control != NETPACKET_CTL) {
    if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: invalid packet type (%u) from %s", control, *Drv->AddrToString(&clientaddr));
    return nullptr;
  }

  if (!CheckGameSignature(msg)) {
    if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: invalid packet type (%u) from %s", control, *Drv->AddrToString(&clientaddr));
    return nullptr;
  }

  msg << command;
  if (command == CCREQ_SERVER_INFO) {
    GCon->Logf(NAME_DevNet, "CONN: sending server info to %s", *Drv->AddrToString(&clientaddr));

    VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
    TmpByte = NETPACKET_CTL;
    MsgOut << TmpByte;
    WriteGameSignature(MsgOut);
    TmpByte = CCREP_SERVER_INFO;
    MsgOut << TmpByte;
    TmpStr = VNetworkLocal::HostName;
    MsgOut << TmpStr;
    TmpStr = (GLevel ? *GLevel->MapName : "intermission");
    MsgOut << TmpStr;
    TmpByte = svs.num_connected;
    MsgOut << TmpByte;
    TmpByte = svs.max_clients;
    MsgOut << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION;
    MsgOut << TmpByte;
    vuint32 mhash = SV_GetModListHash();
    MsgOut << mhash;
    // write pak list
    WritePakList(MsgOut);
    Drv->Write(acceptsock, MsgOut.GetData(), MsgOut.GetNumBytes(), &clientaddr);
    return nullptr;
  }

  if (command != CCREQ_CONNECT) {
    if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: unknown packet command (%u) from %s", command, *Drv->AddrToString(&clientaddr));
    return nullptr;
  }

  // check version
  TmpByte = 0;
  msg << TmpByte;
  if (msg.IsError() || TmpByte != NET_PROTOCOL_VERSION) {
    GCon->Logf(NAME_DevNet, "connection error: invalid protocol version, got %u, but expected %d", TmpByte, NET_PROTOCOL_VERSION);
    // send reject packet, why not?
    SendConnectionReject(Drv, "invalid protocol version", acceptsock, clientaddr);
    return nullptr;
  }

  // check modlist
  vuint32 modhash = 0;
  vuint16 modcount = 0;
  msg << modhash;
  msg << modcount;
  if (msg.IsError() || modhash != SV_GetModListHash() || modcount != (vuint16)FL_GetWadPk3List().length()) {
    GCon->Log(NAME_DevNet, "connection error: incompatible mod list");
    // send reject packet
    SendConnectionReject(Drv, "incompatible loaded mods list", acceptsock, clientaddr, true); // send modlist
    return nullptr;
  }

  // see if this guy is already connected
  for (VSocket *as = Net->ActiveSockets; as; as = as->Next) {
    if (as->Driver != this) continue;
    VDatagramSocket *s = (VDatagramSocket *)as;
    ret = Drv->AddrCompare(&clientaddr, &s->Addr);
    if (ret >= 0) {
      // is this a duplicate connection reqeust?
      if (ret == 0 && Net->GetNetTime()-s->ConnectTime < 2.0) {
        GCon->Logf(NAME_DevNet, "CONN: duplicate connection request from %s (this is ok)", *Drv->AddrToString(&clientaddr));
        // yes, so send a duplicate reply
        VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
        Drv->GetSocketAddr(s->LanSocket, &newaddr);
        TmpByte = NETPACKET_CTL;
        MsgOut << TmpByte;
        WriteGameSignature(MsgOut);
        TmpByte = CCREP_ACCEPT;
        MsgOut << TmpByte;
        vint32 TmpPort = Drv->GetSocketPort(&newaddr);
        MsgOut << TmpPort;
        Drv->Write(acceptsock, MsgOut.GetData(), MsgOut.GetNumBytes(), &clientaddr);
        return nullptr;
      }
      // it is prolly somebody coming back in from a crash/disconnect
      // so close the old socket and let their retry get them back in
      // but don't do that for localhost
      if (ret != 0 && Drv->IsLocalAddress(&clientaddr) && Drv->IsLocalAddress(&s->Addr)) {
        GCon->Logf(NAME_DevNet, "CONN: LOCALHOST for %s", *Drv->AddrToString(&clientaddr));
      } else {
        GCon->Logf(NAME_DevNet, "CONN: RETRYWAIT for %s", *Drv->AddrToString(&clientaddr));
        s->Invalid = true;
        return nullptr;
      }
    }
  }

  if (svs.num_connected >= svs.max_clients) {
    // no room; try to let him know
    SendConnectionReject(Drv, "server is full", acceptsock, clientaddr);
    return nullptr;
  }

  GCon->Logf(NAME_DevNet, "new client from %s (connecting back)", *Drv->AddrToString(&clientaddr));

  // allocate new "listening" network socket
  newsock = Drv->OpenListenSocket(0);
  if (newsock == -1) return nullptr;

  // allocate a VSocket
  // everything is allocated, just fill in the details
  sock = new VDatagramSocket(this);
  sock->LanSocket = newsock;
  sock->LanDriver = Drv;
  sock->Addr = clientaddr;
  sock->Address = Drv->AddrToString(&clientaddr);

  Drv->GetSocketAddr(newsock, &newaddr);

  GCon->Logf(NAME_DevNet, "allocated socket %s for client %s", *Drv->AddrToString(&newaddr), *sock->Address);

  // send him back the info about the server connection he has been allocated
  VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
  TmpByte = NETPACKET_CTL;
  MsgOut << TmpByte;
  WriteGameSignature(MsgOut);
  TmpByte = CCREP_ACCEPT;
  MsgOut << TmpByte;
  vint32 TmpPort = Drv->GetSocketPort(&newaddr);
  MsgOut << TmpPort;
  Drv->Write(acceptsock, MsgOut.GetData(), MsgOut.GetNumBytes(), &clientaddr);

  return sock;
#else
  return nullptr;
#endif
}


//==========================================================================
//
//  VDatagramDriver::CheckNewConnections
//
//==========================================================================
VSocket *VDatagramDriver::CheckNewConnections () {
  for (int i = 0; i < Net->NumLanDrivers; ++i) {
    if (Net->LanDrivers[i]->initialised) {
      VSocket *ret = CheckNewConnections(Net->LanDrivers[i]);
      if (ret != nullptr) return ret;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VDatagramDriver::UpdateMaster
//
//==========================================================================
void VDatagramDriver::UpdateMaster (VNetLanDriver *Drv) {
  sockaddr_t sendaddr;

  // see if we can resolve the host name
  if (Drv->GetAddrFromName(MasterSrv, &sendaddr, MASTER_SERVER_PORT) == -1) {
    GCon->Log(NAME_DevNet, "Could not resolve server name");
    return;
  }

  if (Drv->net_acceptsocket == -1) {
    GCon->Log(NAME_DevNet, "Listen socket not open");
    return;
  }

  // send the connection request
  VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
  WriteGameSignature(MsgOut);
  vuint8 TmpByte = MASTER_PROTO_VERSION;
  MsgOut << TmpByte;
  TmpByte = MCREQ_JOIN;
  MsgOut << TmpByte;
  TmpByte = NET_PROTOCOL_VERSION;
  MsgOut << TmpByte;
  Drv->Write(Drv->net_acceptsocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &sendaddr);
}


//==========================================================================
//
//  VDatagramDriver::UpdateMaster
//
//==========================================================================
void VDatagramDriver::UpdateMaster () {
  if (!UseMaster) return;
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (VNetworkLocal::LanDrivers[i]->initialised) UpdateMaster(VNetworkLocal::LanDrivers[i]);
  }
}


//==========================================================================
//
//  VDatagramDriver::QuitMaster
//
//==========================================================================
void VDatagramDriver::QuitMaster (VNetLanDriver *Drv) {
  sockaddr_t sendaddr;

  // see if we can resolve the host name
  if (Drv->GetAddrFromName(MasterSrv, &sendaddr, MASTER_SERVER_PORT) == -1) {
    GCon->Log(NAME_DevNet, "Could not resolve server name");
    return;
  }

  if (Drv->net_acceptsocket == -1) {
    GCon->Log(NAME_DevNet, "Listen socket not open");
    return;
  }

  // send the quit request
  VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
  WriteGameSignature(MsgOut);
  vuint8 TmpByte = MASTER_PROTO_VERSION;
  MsgOut << TmpByte;
  TmpByte = MCREQ_QUIT;
  MsgOut << TmpByte;
  Drv->Write(Drv->net_acceptsocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &sendaddr);
}


//==========================================================================
//
//  VDatagramDriver::QuitMaster
//
//==========================================================================
void VDatagramDriver::QuitMaster () {
  if (!UseMaster) return;
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (VNetworkLocal::LanDrivers[i]->initialised) QuitMaster(VNetworkLocal::LanDrivers[i]);
  }
}


//==========================================================================
//
//  VDatagramDriver::QueryMaster
//
//==========================================================================
bool VDatagramDriver::QueryMaster (VNetLanDriver *Drv, bool xmit) {
  sockaddr_t myaddr;
  sockaddr_t readaddr;
  sockaddr_t tmpaddr;
  int len;
  vuint8 control;
  vuint8 TmpByte;

  if (Drv->MasterQuerySocket < 0) Drv->MasterQuerySocket = Drv->OpenListenSocket(0);

  Drv->GetSocketAddr(Drv->MasterQuerySocket, &myaddr);
  if (xmit) {
    sockaddr_t sendaddr;
    // see if we can resolve the host name
    if (Drv->GetAddrFromName(MasterSrv, &sendaddr, MASTER_SERVER_PORT) == -1) {
      GCon->Log(NAME_DevNet, "Could not resolve server name");
      return false;
    }
    // send the query request
    VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
    WriteGameSignature(MsgOut);
    TmpByte = MASTER_PROTO_VERSION;
    MsgOut << TmpByte;
    TmpByte = MCREQ_LIST;
    MsgOut << TmpByte;
    Drv->Write(Drv->MasterQuerySocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &sendaddr);
    GCon->Logf(NAME_DevNet, "sent query to master at %s...", *Drv->AddrToString(&sendaddr));
    return false;
  }

  //GCon->Logf(NAME_DevNet, "waiting for master reply...");

  bool res = false;
  int pktleft = 256;
  while (pktleft-- > 0 && (len = Drv->Read(Drv->MasterQuerySocket, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr)) > 0) {
    if (len < 1) continue;

    GCon->Logf(NAME_DevNet, "got master reply from %s", *Drv->AddrToString(&readaddr));
    // is the cache full?
    //if (Net->HostCacheCount == HOSTCACHESIZE) continue;

    //GCon->Logf("processing master reply...");
    VBitStreamReader msg(packetBuffer.data, len<<3);
    if (!CheckGameSignature(msg)) continue;

    msg << TmpByte;
    if (msg.IsError() || TmpByte != MASTER_PROTO_VERSION) continue;

    msg << control;
    if (msg.IsError() || control != MCREP_LIST) continue;

    msg << control; // control byte: bit 0 means "first packet", bit 1 means "last packet"
    //GCon->Logf(NAME_Dev, "  control byte: 0x%02x", (unsigned)control);

    //if ((control&0x01) == 0) continue; // first packed is missing, but nobody cares

    while (!msg.AtEnd()) {
      vuint8 pver;
      tmpaddr = readaddr;
      msg.Serialise(&pver, 1);
      msg.Serialise(tmpaddr.sa_data+2, 4);
      msg.Serialise(tmpaddr.sa_data, 2);
      if (pver == NET_PROTOCOL_VERSION) {
        GCon->Logf(NAME_DevNet, "  sending server query to %s...", *Drv->AddrToString(&tmpaddr));
        VBitStreamWriter Reply(MAX_DGRAM_SIZE<<3);
        TmpByte = NETPACKET_CTL;
        Reply << TmpByte;
        WriteGameSignature(Reply);
        TmpByte = CCREQ_SERVER_INFO;
        Reply << TmpByte;
        TmpByte = NET_PROTOCOL_VERSION;
        Reply << TmpByte;
        Drv->Write(Drv->controlSock, Reply.GetData(), Reply.GetNumBytes(), &tmpaddr);
      } else {
        GCon->Logf(NAME_DevNet, "  server: %s, bad proto version %u", *Drv->AddrToString(&tmpaddr), pver);
      }
    }

    //if (control&0x02) return true; // nobody cares, again
    //return true;
    res = true;
  }

  return res;
}


//==========================================================================
//
//  VDatagramDriver::QueryMaster
//
//==========================================================================
bool VDatagramDriver::QueryMaster (bool xmit) {
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (Net->HostCacheCount == HOSTCACHESIZE) break;
    if (VNetworkLocal::LanDrivers[i]->initialised) {
      if (QueryMaster(VNetworkLocal::LanDrivers[i], xmit)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VDatagramDriver::EndQueryMaster
//
//==========================================================================
void VDatagramDriver::EndQueryMaster () {
  for (int i = 0; i < VNetworkLocal::NumLanDrivers; ++i) {
    if (VNetworkLocal::LanDrivers[i]->initialised && VNetworkLocal::LanDrivers[i]->MasterQuerySocket > 0) {
      VNetworkLocal::LanDrivers[i]->CloseSocket(VNetworkLocal::LanDrivers[i]->MasterQuerySocket);
      VNetworkLocal::LanDrivers[i]->MasterQuerySocket = -1;
    }
  }
}


//==========================================================================
//
//  VDatagramDriver::Shutdown
//
//==========================================================================
void VDatagramDriver::Shutdown () {
  // shutdown the lan drivers
  for (int i = 0; i < Net->NumLanDrivers; ++i) {
    if (Net->LanDrivers[i]->initialised) {
      Net->LanDrivers[i]->Shutdown();
      Net->LanDrivers[i]->initialised = false;
    }
  }
}


//==========================================================================
//
//  VDatagramSocket::~VDatagramSocket
//
//==========================================================================
VDatagramSocket::~VDatagramSocket () {
  LanDriver->CloseSocket(LanSocket);
}


//==========================================================================
//
//  VDatagramSocket::GetMessage
//
//  dest should be at least `MAX_DGRAM_SIZE+4` (just in case)
//  returns number of bytes received, 0 for "no message", -1 for error
//
//==========================================================================
int VDatagramSocket::GetMessage (void *dest, size_t destSize) {
  if (Invalid) return -1;
  if (destSize == 0) return -1;
  if (!dest) return -1;

  sockaddr_t readaddr;
  int ret = 0;
  vuint8 data[MAX_DGRAM_SIZE+4];

  for (;;) {
    // read message
    vuint32 length = LanDriver->Read(LanSocket, data, NET_DATAGRAMSIZE, &readaddr);

    if (length == 0) {
      //if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: no data from %s (expected address is %s)", *LanDriver->AddrToString(&readaddr), *LanDriver->AddrToString(&Addr));
      break;
    }

    if ((int)length < 0) {
      GCon->Logf(NAME_DevNet, "%s: Read error", *LanDriver->AddrToString(&Addr));
      return -1;
    }

    if (LanDriver->AddrCompare(&readaddr, &Addr) != 0) {
      if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: rejected packet from %s due to wrong address (%s expected)", *LanDriver->AddrToString(&readaddr), *LanDriver->AddrToString(&Addr));
      UpdateRejectedStats(length);
      continue;
    }

    UpdateReceivedStats(length);

    if (length > destSize) {
      GCon->Logf(NAME_DevNet, "%s: Read error (message too big)", *LanDriver->AddrToString(&Addr));
      return -1;
    }

    memcpy(dest, data, length);
    ret = (int)length;
    break;
  }

  return ret;
}


//==========================================================================
//
//  VDatagramSocket::SendMessage
//
//  Send a packet over the net connection.
//  returns  1 if the packet was sent properly
//  returns -1 if the connection died
//
//==========================================================================
int VDatagramSocket::SendMessage (const vuint8 *Data, vuint32 Length) {
  vensure(Length > 0);
  vensure(Length <= MAX_DGRAM_SIZE);
  if (Invalid) return -1;
  const int res = (LanDriver->Write(LanSocket, Data, Length, &Addr) == -1 ? -1 : 1);
  if (res > 0) UpdateSentStats(Length);
  //GCon->Logf(NAME_DevNet, "+++ SK:%d(%s): sent %u bytes of data (res=%d)", LanSocket, *LanDriver->AddrToString(&Addr), Length, res);
  return res;
}


//==========================================================================
//
//  VDatagramSocket::IsLocalConnection
//
//==========================================================================
bool VDatagramSocket::IsLocalConnection () const noexcept {
  return false;
}


//==========================================================================
//
//  PrintStats
//
//==========================================================================
static void PrintStats (VSocket *sock) {
  GCon->Logf(NAME_DevNet, "=== SOCKET %s ===", *sock->Address);
  GCon->Logf(NAME_DevNet, "  minimalSentPacket = %u", sock->minimalSentPacket);
  GCon->Logf(NAME_DevNet, "  maximalSentPacket = %u", sock->maximalSentPacket);
  GCon->Logf(NAME_DevNet, "  bytesSent         = %s", *VSocketPublic::u64str(sock->bytesSent));
  GCon->Logf(NAME_DevNet, "  bytesReceived     = %s", *VSocketPublic::u64str(sock->bytesReceived));
  GCon->Logf(NAME_DevNet, "  bytesRejected     = %s", *VSocketPublic::u64str(sock->bytesRejected));
}


//==========================================================================
//
//  COMMAND NetStats
//
//==========================================================================
COMMAND(NetStats) {
  VSocket *s;

  VNetworkLocal *Net = (VNetworkLocal *)GNet;
  if (Args.Num() == 1) {
    GCon->Logf(NAME_DevNet, "unreliable messages sent = %d", Net->UnreliableMessagesSent);
    GCon->Logf(NAME_DevNet, "unreliable messages recv = %d", Net->UnreliableMessagesReceived);
    GCon->Logf(NAME_DevNet, "packetsSent              = %d", Net->packetsSent);
    GCon->Logf(NAME_DevNet, "packetsReSent            = %d", Net->packetsReSent);
    GCon->Logf(NAME_DevNet, "packetsReceived          = %d", Net->packetsReceived);
    GCon->Logf(NAME_DevNet, "receivedDuplicateCount   = %d", Net->receivedDuplicateCount);
    GCon->Logf(NAME_DevNet, "shortPacketCount         = %d", Net->shortPacketCount);
    GCon->Logf(NAME_DevNet, "droppedDatagrams         = %d", Net->droppedDatagrams);
    GCon->Logf(NAME_DevNet, "minimalSentPacket        = %u", Net->minimalSentPacket);
    GCon->Logf(NAME_DevNet, "maximalSentPacket        = %u", Net->maximalSentPacket);
    GCon->Logf(NAME_DevNet, "bytesSent                = %s", *VSocketPublic::u64str(Net->bytesSent));
    GCon->Logf(NAME_DevNet, "bytesReceived            = %s", *VSocketPublic::u64str(Net->bytesReceived));
    GCon->Logf(NAME_DevNet, "bytesRejected            = %s", *VSocketPublic::u64str(Net->bytesRejected));
  } else {
    for (s = Net->ActiveSockets; s; s = s->Next) {
      bool hit = false;
      for (int f = 1; f < Args.length(); ++f) if (s->Address.globMatchCI(Args[f])) { hit = true; break; }
      if (hit) PrintStats(s);
    }
  }
}
