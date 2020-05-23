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
//    bytes[32] key
//    vuint23   nonce
//    other data is encrypted with ChaCha20
//    bytes[4]  crc32c
//    bytes     "K8VAVOOM"
//    vuint8    CCREQ_CONNECT
//    vuint8    net_protocol_version_hi  NET_PROTOCOL_VERSION_HI
//    vuint8    net_protocol_version_lo  NET_PROTOCOL_VERSION_LO
//    bytes[32] passwordSHA256
//    vuint32   modlisthash
//    vuint16   modlistcount
//
// CCREQ_SERVER_INFO
//    bytes[32] key
//    vuint23   nonce
//    other data is encrypted with ChaCha20
//    bytes[4]  crc32c
//    bytes     "K8VAVOOM"
//    vuint8    CCREQ_SERVER_INFO
//    vuint8    net_protocol_version_hi  NET_PROTOCOL_VERSION_HI
//    vuint8    net_protocol_version_lo  NET_PROTOCOL_VERSION_LO
//
//
// CCREP_ACCEPT
//    bytes[32] key
//    vuint23   nonce
//    other data is encrypted with ChaCha20
//    bytes[4]  crc32c
//    bytes     "K8VAVOOM"
//    vuint8    CCREP_ACCEPT
//    vuint8    net_protocol_version_hi  NET_PROTOCOL_VERSION_HI
//    vuint8    net_protocol_version_lo  NET_PROTOCOL_VERSION_LO
//    vuint16   port
//
// CCREP_REJECT
//    bytes[32] key
//    vuint23   nonce
//    other data is encrypted with ChaCha20
//    bytes[4]  crc32c
//    bytes     "K8VAVOOM"
//    vuint8    CCREP_REJECT
//    vuint8    extflags (bit 0: has wads?)
//    string    reason (127 bytes max, with terminating 0)
//    if bit 0 of `extflags` is set, modlist follows
//      modlist is asciz strings, terminated with empty string
//
// CCREP_SERVER_INFO
//    bytes[32] key
//    vuint23   nonce
//    other data is encrypted with ChaCha20
//    bytes[4]  crc32c
//    bytes     "K8VAVOOM"
//    vuint8    CCREP_SERVER_INFO
//    vuint8    net_protocol_version_hi  NET_PROTOCOL_VERSION_HI
//    vuint8    net_protocol_version_lo  NET_PROTOCOL_VERSION_LO
//    vuint8    extflags (bit 0: has wads?; bit 1: server is password-protected)
//    vuint8    current_players
//    vuint8    max_players
//    vuint8    deathmatchMode
//    vuint32   modlisthash
//    string    host_name (max 127 bytes, with terminating 0)
//    string    level_name (max 63 bytes, with terminating 0)
//    if bit 0 of `extflags` is set, modlist follows
//      asciiz strings with loaded archive names, terminated with empty string
//
//**************************************************************************
#include "../gamedefs.h"
#include "net_local.h"


static int cli_NoLAN = 0;

/*static*/ bool cliRegister_datagram_args =
  VParsedArgs::RegisterFlagSet("-nolan", "disable networking", &cli_NoLAN) &&
  VParsedArgs::RegisterAlias("-no-lan", "-nolan");


static VCvarB net_dbg_dump_rejected_connections("net_dbg_dump_rejected_connections", true, "Dump rejected connections?");

static VCvarS net_rcon_secret_key("net_rcon_secret_key", "", "Secret key for rcon commands");
static VCvarS net_server_key("net_server_key", "", "Server key for password-protected servers");


// ////////////////////////////////////////////////////////////////////////// //
class VDatagramSocket : public VSocket {
public:
  VNetLanDriver *LanDriver;
  int LanSocket;
  sockaddr_t Addr;
  bool Invalid;

public:
  VDatagramSocket (VNetDriver *Drv) : VSocket(Drv), LanDriver(nullptr), LanSocket(-1), Invalid(false) {}
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

  enum {
    MAX_INFO_DGRAM_SIZE = MAX_DGRAM_SIZE-VNetUtils::ChaCha20HeaderSize,
  };

  // client request
  enum {
    CCREQ_CONNECT     = 1,
    CCREQ_SERVER_INFO = 2,
  };

  enum {
    RCON_PROTO_VERSION = 1u, // 16-bit value

    // client sends this to execute console command on the server
    CCREQ_RCON_COMMAND = 25,
    // server sends this to ack client command
    CCREP_RCON_COMMAND = 26,
  };

  /*
    rcon protocol:

    client connects, and sends CCREQ_RCON_COMMAND. key is used to reject duplicate commands.
    then client must wait for CCREP_RCON_COMMAND. if it got no answer, it should send its
    request again.

    if the server accepts the command, it must remember its key, and don't repeat it, just
    answer with the CCREP_RCON_COMMAND.

    client sends CCREQ_RCON_COMMAND:
      bytes[32] key
      vuint23   nonce
      other data is encrypted with ChaCha20
      bytes[4]  crc32c
      // signed from here
      bytes     "K8VAVOOM"
      vuint8    CCREQ_RCON_COMMAND
      vuint16   RCON_PROTO_VERSION
      bytes[32] secretSHA256
      bytes     command (trailing zero or packet end ends it)

    server sends CCREP_RCON_COMMAND:
      bytes[32] key
      vuint23   nonce
      other data is encrypted with ChaCha20
      bytes[4]  crc32c
      // signed from here
      bytes     "K8VAVOOM"
      vuint8    CCREP_RCON_COMMAND
      vuint16   RCON_PROTO_VERSION
      bytes     errmsg     ; message (zero-terminated, may be empty)
   */

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

private:
  VStr LastMasterAddrStr;
  sockaddr_t LastMasterAddr;
  bool LastMasterIsBad;

private:
  bool ResolveMasterAddr (VNetLanDriver *Drv);

public:
  VDatagramDriver ();

  virtual int Init () override;
  virtual void Listen (bool) override;
  virtual void SearchForHosts (bool, bool) override;
  virtual VSocket *Connect (const char *) override;
  virtual VSocket *CheckNewConnections (bool rconOnly) override;
  virtual void UpdateMaster () override;
  virtual void QuitMaster () override;
  virtual bool QueryMaster (bool) override;
  virtual void EndQueryMaster () override;
  virtual void Shutdown () override;

  void SearchForHosts (VNetLanDriver *, bool, bool);
  VSocket *Connect (VNetLanDriver *, const char *);
  VSocket *CheckNewConnections (VNetLanDriver *Drv, bool rconOnly);
  void SendConnectionReject (VNetLanDriver *Drv, const vuint8 key[VNetUtils::ChaCha20KeySize], VStr reason, int acceptsock, sockaddr_t clientaddr, bool sendModList=false);
  void UpdateMaster (VNetLanDriver *);
  void QuitMaster (VNetLanDriver *);
  bool QueryMaster (VNetLanDriver *, bool);

  static void WriteGameSignature (VBitStreamWriter &strm);
  static bool CheckGameSignature (VBitStreamReader &strm);

  static void WritePakList (VBitStreamWriter &strm);
  static bool ReadPakList (TArray<VStr> &list, VBitStreamReader &strm); // won't clear list

  // returns size or -1
  static int EncryptInfoBitStream (vuint8 *dest, VBitStreamWriter &strm, const vuint8 key[VNetUtils::ChaCha20KeySize]) noexcept;
  static bool DecryptInfoBitStream (vuint8 key[VNetUtils::ChaCha20KeySize], VBitStreamReader &strm, void *srcbuf, int srclen);

public: // rcon
  vuint8 rconLastKey[VNetUtils::ChaCha20KeySize];
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
  memset((void *)&LastMasterAddr, 0, sizeof(LastMasterAddr));
  LastMasterIsBad = true;
  vassert(LastMasterAddrStr.isEmpty());
  memset(rconLastKey, 0, VNetUtils::ChaCha20KeySize);
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
  TArray<VStr> list;
  FL_GetNetWads(list);
  for (int f = 0; f < list.length(); ++f) {
    VStr pak = list[f];
    if (pak.isEmpty()) continue;
    //GCon->Logf(NAME_Debug, "%d: <%s>", f, *pak);
    //if (pak.length() > 255) pak = pak.right(255); // just in case
    if (strm.GetNumBits()+pak.length()*8+8 > MAX_INFO_DGRAM_SIZE*8-8) break;
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
//  VDatagramDriver::EncryptInfoBitStream
//
//==========================================================================
int VDatagramDriver::EncryptInfoBitStream (vuint8 *dest, VBitStreamWriter &strm, const vuint8 key[VNetUtils::ChaCha20KeySize]) noexcept {
  if (!dest) return -1;
  if (strm.IsError() || strm.GetNumBytes() > MAX_INFO_DGRAM_SIZE) return -1;

  // align (just in case)
  while (strm.GetNumBits()&7) strm.WriteBit(false);
  if (strm.IsError() || strm.GetNumBytes() > MAX_INFO_DGRAM_SIZE) return -1;

  // encrypt
  return VNetUtils::EncryptInfoPacket(dest, strm.GetData(), strm.GetNumBytes(), key);
}


//==========================================================================
//
//  VDatagramDriver::DecryptInfoBitStream
//
//==========================================================================
bool VDatagramDriver::DecryptInfoBitStream (vuint8 key[VNetUtils::ChaCha20KeySize], VBitStreamReader &strm, void *srcbuf, int srclen) {
  strm.Clear(true);

  if (srclen < VNetUtils::ChaCha20KeySize+4) {
    strm.SetError();
    return false;
  }

  int dlen = VNetUtils::DecryptInfoPacket(key, srcbuf, srcbuf, srclen);
  if (dlen <= 0) {
    // it must have data
    strm.SetError();
    return false;
  }

  strm.SetupFrom((const vuint8 *)srcbuf, dlen<<3);
  return true;
}


//==========================================================================
//
//  VDatagramDriver::SearchForHosts
//
//==========================================================================
void VDatagramDriver::SearchForHosts (VNetLanDriver *Drv, bool xmit, bool ForMaster) {
  sockaddr_t myaddr;
  sockaddr_t readaddr;
  //vuint8 control;
  vuint8 msgtype;
  int n;
  vuint8 TmpByte;

  Drv->GetSocketAddr(Drv->controlSock, &myaddr);
  if (xmit && Drv->CanBroadcast()) {
    GCon->Log(NAME_DevNet, "sending broadcast query");
    vuint8 edata[MAX_DGRAM_SIZE];

    VBitStreamWriter Reply(MAX_INFO_DGRAM_SIZE<<3);
    WriteGameSignature(Reply);
    TmpByte = CCREQ_SERVER_INFO;
    Reply << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION_HI;
    Reply << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION_LO;
    Reply << TmpByte;

    // encrypt
    vuint8 key[VNetUtils::ChaCha20KeySize];
    VNetUtils::GenerateKey(key);
    int elen = EncryptInfoBitStream(edata, Reply, key);
    if (elen <= 0) return; // just in case
    if (elen > 0) Drv->Broadcast(Drv->controlSock, edata, elen);
  }

  //GCon->Logf(NAME_Debug, "SearchForHosts: trying to read a datagram (me:%s)", Drv->AddrToString(&myaddr));
  int pktleft = 128;
  while (pktleft-- > 0) {
    int len = Drv->Read(Drv->controlSock, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr);
    if (len < 0) break; // no message or error
    if (len < VNetUtils::ChaCha20KeySize+4) continue;

    // don't answer our own query
    if (!ForMaster && Drv->AddrCompare(&readaddr, &myaddr) == 0) continue;

    // is the cache full?
    //if (Net->HostCacheCount == HOSTCACHESIZE) continue;

    // decrypt
    vuint8 key[VNetUtils::ChaCha20KeySize];
    VBitStreamReader msg;

    if (!DecryptInfoBitStream(key, msg, packetBuffer.data, len)) continue;

    GCon->Logf(NAME_DevNet, "SearchForHosts: got datagram from %s (len=%d; dlen=%d)", Drv->AddrToString(&readaddr), len, msg.GetNumBytes());

    if (!CheckGameSignature(msg)) continue;

    msg << msgtype;
    if (msg.IsError() || msgtype != CCREP_SERVER_INFO) continue;

    VStr str;
    VStr addr = Drv->AddrToString(&readaddr);

    GCon->Logf(NAME_DevNet, "SearchForHosts: got valid packet from %s", Drv->AddrToString(&readaddr));

    // search the cache for this server
    for (n = 0; n < Net->HostCacheCount; ++n) {
      if (addr.strEqu(Net->HostCache[n].CName)) break;
    }

    // is it already there?
    if (n < Net->HostCacheCount) continue;

    if (Net->HostCacheCount == HOSTCACHESIZE) {
      GCon->Logf(NAME_DevNet, "too many hosts, ignoring");
      continue;
    }

    // add it
    ++Net->HostCacheCount;
    vassert(n >= 0 && n < Net->HostCacheCount);
    hostcache_t *hinfo = &Net->HostCache[n];
    hinfo->Flags = 0;
    // protocol version
    vuint8 protoHi = 0, protoLo = 0;
    msg << protoHi << protoLo;
    // flags
    vuint8 extflags = 0;
    msg << extflags;
    // current players
    msg << TmpByte;
    hinfo->Users = TmpByte;
    // max players
    msg << TmpByte;
    hinfo->MaxUsers = TmpByte;
    // deathmatch mode
    msg << TmpByte;
    hinfo->DeathMatch = TmpByte;
    // wadlist hash
    vuint32 mhash = 0;
    msg << mhash;
    // server name
    msg << str;
    hinfo->Name = str;
    // map name
    msg << str;
    hinfo->Map = str;

    if (protoHi == NET_PROTOCOL_VERSION_HI && protoLo == NET_PROTOCOL_VERSION_LO) {
      hinfo->Flags |= hostcache_t::Flag_GoodProtocol;
    }

    if (extflags&2) hinfo->Flags |= hostcache_t::Flag_PasswordProtected;

    //GCon->Logf(NAME_DevNet, " WHASH: theirs=0x%08x  mine=0x%08x", mhash, FL_GetNetWadsHash());
    if (mhash == FL_GetNetWadsHash()) hinfo->Flags |= hostcache_t::Flag_GoodWadList;
    hinfo->CName = addr;
    hinfo->WadFiles.clear();
    if (!msg.IsError() && (extflags&1)) ReadPakList(hinfo->WadFiles, msg);

    if (msg.IsError()) {
      // remove it
      hinfo->WadFiles.clear();
      --Net->HostCacheCount;
      continue;
    }
    //GCon->Logf(NAME_DevNet, " wcount: %d %d", hinfo->WadFiles.length(), FL_GetNetWadsCount());
    if ((hinfo->Flags&hostcache_t::Flag_GoodWadList) && hinfo->WadFiles.length() != FL_GetNetWadsCount()) {
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
  //vuint8 control;
  VStr reason;
  vuint8 msgtype;
  vuint16 newport;
  VBitStreamReader *msg = nullptr;
  vuint8 TmpByte;
  vuint8 otherProtoHi, otherProtoLo;

  if (!host || !host[0]) return nullptr;

  SCR_Update();
  R_OSDMsgReset(OSD_Network);

  R_OSDMsgShow(va("getting address for [%s]", host));

  // see if we can resolve the host name
  if (Drv->GetAddrFromName(host, &sendaddr, Net->HostPort) == -1) return nullptr;

  //R_OSDMsgShow("creating socket");
  R_OSDMsgShow(va("connecting to [%s]", Drv->AddrToString(&sendaddr)));

  newsock = Drv->ConnectSocketTo(&sendaddr);
  if (newsock == -1) return nullptr;

  vuint8 origkey[VNetUtils::ChaCha20KeySize];
  vuint8 srvkey[VNetUtils::ChaCha20KeySize]; // this is what we should receive from the server
  bool replyWithServerKey = false;
  VNetUtils::GenerateKey(origkey);
  VNetUtils::DerivePublicKey(srvkey, origkey);

  sock = new VDatagramSocket(this);
  memcpy(sock->AuthKey, srvkey, VNetUtils::ChaCha20KeySize);
  memcpy(sock->ClientKey, origkey, VNetUtils::ChaCha20KeySize);
  sock->LanSocket = newsock;
  sock->LanDriver = Drv;
  sock->Addr = sendaddr;
  sock->Address = Drv->AddrToString(&sock->Addr);

  // send the connection request
  GCon->Logf(NAME_DevNet, "trying %s", Drv->AddrToString(&sendaddr));
  //SCR_Update();
  Net->UpdateNetTime();
  start_time = Net->GetNetTime();

  for (reps = 0; reps < 3; ++reps) {
    if (Net->CheckForUserAbort()) { ret = 0; break; }

    R_OSDMsgShow("sending handshake");

    vuint8 edata[MAX_DGRAM_SIZE];

    VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
    WriteGameSignature(MsgOut);
    // command
    TmpByte = CCREQ_CONNECT;
    MsgOut << TmpByte;
    // protocol version
    TmpByte = NET_PROTOCOL_VERSION_HI;
    MsgOut << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION_LO;
    MsgOut << TmpByte;
    // password hash
    vuint8 cldig[SHA256_DIGEST_SIZE];
    // we'll fix it later
    vassert((MsgOut.GetPos()&7) == 0);
    int digpos = MsgOut.GetPos()>>3;
    memset(cldig, 0, SHA256_DIGEST_SIZE);
    MsgOut.Serialise(cldig, SHA256_DIGEST_SIZE);
    // mod info
    vuint32 modhash = FL_GetNetWadsHash();
    MsgOut << modhash;
    vuint16 modcount = (vuint16)FL_GetNetWadsCount();
    MsgOut << modcount;

    // fix hash
    sha256_ctx shactx;
    sha256_init(&shactx);
    // hash key
    sha256_update(&shactx, origkey, VNetUtils::ChaCha20KeySize);
    // hash whole packet
    sha256_update(&shactx, MsgOut.GetData(), MsgOut.GetNumBytes());
    // hash password
    sha256_update(&shactx, *net_server_key.asStr(), (unsigned)net_server_key.asStr().length());
    sha256_final(&shactx, cldig);
    // update hash
    memcpy(MsgOut.GetData()+digpos, cldig, SHA256_DIGEST_SIZE);

    // encrypt
    int elen = EncryptInfoBitStream(edata, MsgOut, origkey);
    if (elen <= 0) {
      ret = -1; // network error
      break;
    }
    Drv->Write(newsock, edata, elen, &sendaddr);

    bool aborted = false;
    vuint8 key[VNetUtils::ChaCha20KeySize];
    replyWithServerKey = false;
    do {
      ret = Drv->Read(newsock, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr);
      if (ret == -2) ret = 0; // "no message" means "message with zero size"

      // if we got something, validate it
      if (ret > VNetUtils::ChaCha20KeySize+4) {
        // is it from the right place?
        if (sock->LanDriver->AddrCompare(&readaddr, &sendaddr) != 0) {
          ret = 0;
          continue;
        }

        // decrypt
        msg = new VBitStreamReader();
        if (!DecryptInfoBitStream(key, *msg, packetBuffer.data, ret)) {
          delete msg;
          ret = 0;
          continue;
        }

        // check if we got the right key
        // origkey may be used for rejection
        if (memcmp(key, srvkey, VNetUtils::ChaCha20KeySize) != 0 &&
            memcmp(key, origkey, VNetUtils::ChaCha20KeySize) != 0)
        {
          delete msg;
          ret = 0;
          continue;
        }
        replyWithServerKey = (memcmp(key, srvkey, VNetUtils::ChaCha20KeySize) == 0);

        if (!CheckGameSignature(*msg)) {
          ret = 0;
          delete msg;
          msg = nullptr;
          continue;
        }
      } else if (ret > 0) {
        ret = 0;
      }

      if (ret == 0) { aborted = Net->CheckForUserAbort(); if (aborted) break; }
      Net->UpdateNetTime();
    } while (ret == 0 && (Net->GetNetTime()-start_time) < 2.5);

    if (ret || aborted) break;

    GCon->Logf(NAME_DevNet, "still trying %s", Drv->AddrToString(&sendaddr));
    //SCR_Update();
    Net->UpdateNetTime();
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
    vuint8 extflags = 0;
    *msg << extflags;
    *msg << reason;
    GCon->Logf(NAME_Error, "Connection rejected: %s", *reason);
    VStr::NCpy(Net->ReturnReason, *reason, 31);
    if (extflags&1) {
      TArray<VStr> list;
      if (ReadPakList(list, *msg)) {
        if (list.length()) {
          GCon->Log(NAME_Error, "=== SERVER REJECTED CONNETION; WAD LIST: ===");
          for (auto &&pak : list) GCon->Logf(NAME_Error, "  %s", *pak);
        }
      }
    }
    if (!reason.isEmpty()) flWarningMessage = VStr("CONNECTION FAILURE\n\n")+reason;
    goto ErrorReturn;
  }

  if (msgtype != CCREP_ACCEPT) {
    reason = "Bad response";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  // check for good key
  if (!replyWithServerKey) {
    reason = "Bad response";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  *msg << otherProtoHi;
  *msg << otherProtoLo;
  *msg << newport;

  if (msg->IsError()) {
    reason = "Bad response";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  if (otherProtoHi != NET_PROTOCOL_VERSION_HI || otherProtoLo != NET_PROTOCOL_VERSION_LO) {
    reason = "Bad protocol version";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  if (newport == 0) {
    reason = "Bad port";
    GCon->Logf(NAME_Error, "Connection failure: %s", *reason);
    VStr::Cpy(Net->ReturnReason, *reason);
    goto ErrorReturn;
  }

  delete msg;

  // switch the connection to the specified address
  memcpy(&sock->Addr, &readaddr, sizeof(sockaddr_t));
  Drv->SetSocketPort(&sock->Addr, newport);
  sock->Address = Drv->AddrToString(&sock->Addr);

  GCon->Logf(NAME_DevNet, "Connection accepted at %s (redirected to port %u)", *sock->Address, newport);
  Net->UpdateNetTime();
  sock->LastMessageTime = Net->GetNetTime();

  R_OSDMsgShow("receiving initial data");

  //m_return_onerror = false;
  return sock;

ErrorReturn:
  delete sock;
  sock = nullptr;
  Drv->CloseSocket(newsock);
  if (msg) delete msg;
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
void VDatagramDriver::SendConnectionReject (VNetLanDriver *Drv, const vuint8 key[VNetUtils::ChaCha20KeySize], VStr reason, int acceptsock, sockaddr_t clientaddr, bool sendModList) {
  vuint8 TmpByte;
  VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
  reason = reason.left(127);
  WriteGameSignature(MsgOut);

  TmpByte = CCREP_REJECT;
  MsgOut << TmpByte;
  // flags
  TmpByte = (sendModList ? 1u : 0u);
  MsgOut << TmpByte;
  // reason
  MsgOut << reason;
  // modlist
  if (sendModList) WritePakList(MsgOut);

  vuint8 edata[MAX_DGRAM_SIZE];
  int elen = EncryptInfoBitStream(edata, MsgOut, key);
  if (elen <= 0) return;

  Drv->Write(acceptsock, edata, elen, &clientaddr);
}


//==========================================================================
//
//  VDatagramDriver::CheckNewConnections
//
//==========================================================================
VSocket *VDatagramDriver::CheckNewConnections (VNetLanDriver *Drv, bool rconOnly) {
#ifdef SERVER
  sockaddr_t clientaddr;
  sockaddr_t newaddr;
  int acceptsock;
  int newsock;
  int len;
  vuint8 command;
  VDatagramSocket *sock;
  vuint8 TmpByte;
  VStr TmpStr;

  Net->UpdateNetTime();
  acceptsock = Drv->CheckNewConnections(rconOnly);
  if (acceptsock == -1) return nullptr;

  vuint8 clientKey[VNetUtils::ChaCha20KeySize];
  vuint8 edata[MAX_DGRAM_SIZE];

  len = Drv->Read(acceptsock, packetBuffer.data, MAX_DGRAM_SIZE, &clientaddr);
  if (len == -2) len = 0; // "no message" means "zero size"
  if (len < VNetUtils::ChaCha20KeySize+4) {
    if (len > 0 && net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: too short packet (%d) from %s", len, Drv->AddrToString(&clientaddr));
    if (len < 0) GCon->Logf(NAME_DevNet, "CONN: error reading incoming packet from %s", Drv->AddrToString(&clientaddr));
    return nullptr;
  }
  GCon->Logf(NAME_DevNet, "CONN: ACCEPTING something from %s", Drv->AddrToString(&clientaddr));

  VBitStreamReader msg;
  if (!DecryptInfoBitStream(clientKey, msg, packetBuffer.data, len)) {
    GCon->Logf(NAME_DevNet, "CONN: got something wrong from %s", Drv->AddrToString(&clientaddr));
    return nullptr;
  }

  if (!CheckGameSignature(msg)) {
    /*if (net_dbg_dump_rejected_connections)*/ GCon->Logf(NAME_DevNet, "CONN: invalid packet type from %s", Drv->AddrToString(&clientaddr));
    return nullptr;
  }

  msg << command;
  //GCon->Logf(NAME_DevNet, "CONN: command=%u", command);
  if (!rconOnly && command == CCREQ_SERVER_INFO) {
    if (msg.AtEnd()) return nullptr;

    // check request version
    vuint8 reqVerHi = 7, reqVerLo = 7;
    msg << reqVerHi << reqVerLo;
    if (msg.IsError()) return nullptr;

    // version sanity check
    if (reqVerHi < 7 || reqVerHi > 42) {
      // arbtrary
      GCon->Logf(NAME_DevNet, "CONN: rejected info request due to wrong request version (%u.%u)", reqVerHi, reqVerLo);
    }
    //if (reqVerHi == NET_PROTOCOL_VERSION_HI && reqVerLo > NET_PROTOCOL_VERSION_LO) return nullptr;

    GCon->Logf(NAME_DevNet, "CONN: sending server info to %s (request version is %u.%u)", Drv->AddrToString(&clientaddr), reqVerHi, reqVerLo);

    VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
    WriteGameSignature(MsgOut);
    // type
    TmpByte = CCREP_SERVER_INFO;
    MsgOut << TmpByte;
    // protocol version
    TmpByte = NET_PROTOCOL_VERSION_HI;
    MsgOut << TmpByte;
    TmpByte = NET_PROTOCOL_VERSION_LO;
    MsgOut << TmpByte;
    // extflags
    TmpByte = 1u|(net_server_key.asStr().isEmpty() ? 0u : 2u); // has modlist
    MsgOut << TmpByte;
    // current number of players
    TmpByte = svs.num_connected;
    MsgOut << TmpByte;
    // max number of players
    TmpByte = svs.max_clients;
    MsgOut << TmpByte;
    // deathmatch type
    TmpByte = svs.deathmatch;
    MsgOut << TmpByte;
    // modlist hash
    vuint32 mhash = FL_GetNetWadsHash();
    MsgOut << mhash;
    // host name
    TmpStr = VNetworkLocal::HostName.asStr().left(127);
    MsgOut << TmpStr;
    // map name
    TmpStr = VStr(GLevel ? *GLevel->MapName : "intermission").left(63);
    MsgOut << TmpStr;
    // write pak list
    WritePakList(MsgOut);

    int elen = EncryptInfoBitStream(edata, MsgOut, clientKey);
    if (elen <= 0) return nullptr; // just in case
    Drv->Write(acceptsock, edata, elen, &clientaddr);
    return nullptr;
  }

  // rcon
  if (command == CCREQ_RCON_COMMAND) {
    VStr mysecret = net_rcon_secret_key.asStr();
    if (mysecret.isEmpty()) return nullptr; // do noting

    // protocol version
    vuint16 pver = 0;
    msg << pver;
    if (msg.IsError() || pver != RCON_PROTO_VERSION) return nullptr; // do noting

    // read secret
    vassert((msg.GetPos()&7) == 0);
    int digpos = msg.GetPos()>>3;
    vuint8 cldig[SHA256_DIGEST_SIZE];
    msg.Serialise(cldig, SHA256_DIGEST_SIZE);
    if (msg.IsError()) return nullptr; // do noting

    // read command
    bool badCommand = false;
    VStr cmdtext;
    while (!msg.AtEnd()) {
      vuint8 ch = 0;
      msg << ch;
      if (msg.IsError()) return nullptr; // do noting
      if (!ch) break;
      if (ch != 9 && (ch < 32 || ch > 127)) { cmdtext += "?"; badCommand = true; continue; } // invalid char
      cmdtext += (char)ch;
    }

    // check secret
    vuint8 svdig[SHA256_DIGEST_SIZE];
    // clear hash buffer
    memset(msg.GetData()+digpos, 0, SHA256_DIGEST_SIZE);
    sha256_ctx shactx;
    sha256_init(&shactx);
    // hash key
    sha256_update(&shactx, clientKey, VNetUtils::ChaCha20KeySize);
    // hash whole packet
    sha256_update(&shactx, msg.GetData(), msg.GetNumBytes());
    // hash password
    sha256_update(&shactx, *mysecret, (unsigned)mysecret.length());
    sha256_final(&shactx, svdig);
    // compare
    if (memcmp(cldig, svdig, SHA256_DIGEST_SIZE) != 0) return nullptr; // invalid secret

    // check for duplicate command
    if (memcmp(rconLastKey, clientKey, VNetUtils::ChaCha20KeySize) != 0) {
      // new command
      memcpy(rconLastKey, clientKey, VNetUtils::ChaCha20KeySize);
      GCon->Logf(NAME_DevNet, "CONN: got new rcon command from %s: %s", Drv->AddrToString(&clientaddr), *cmdtext.quote(true));
      if (!badCommand) {
        cmdtext += "\n";
        GCmdBuf << cmdtext;
      }
    } else {
      GCon->Logf(NAME_DevNet, "CONN: got duplicate rcon command from %s: %s", Drv->AddrToString(&clientaddr), *cmdtext.quote(true));
    }

    // send reply
    VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
    WriteGameSignature(MsgOut);
    TmpByte = CCREP_RCON_COMMAND;
    MsgOut << TmpByte;
    // protocol version
    pver = RCON_PROTO_VERSION;
    MsgOut << pver;
    // reply text
    VStr reptext;
    if (badCommand) reptext = "bad command text"; else reptext = "OK"; // why not?
    for (int f = 0; f < reptext.length(); ++f) {
      TmpByte = (vuint8)(reptext[f]);
      MsgOut << TmpByte;
    }
    TmpByte = 0;
    MsgOut << TmpByte;

    // encrypt
    int elen = EncryptInfoBitStream(edata, MsgOut, clientKey);
    if (elen > 0) Drv->Write(acceptsock, edata, elen, &clientaddr);
    // done
    return nullptr;
  }

  if (rconOnly) {
    return nullptr;
  }

  // it should be "connect"
  if (command != CCREQ_CONNECT) {
    if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: unknown packet command (%u) from %s", command, Drv->AddrToString(&clientaddr));
    SendConnectionReject(Drv, clientKey, "unknown query", acceptsock, clientaddr);
    return nullptr;
  }

  // read version
  vuint8 remVerHi = 0, remVerLo = 0;
  msg << remVerHi << remVerLo;
  if (msg.IsError()) {
    GCon->Logf(NAME_DevNet, "connection error: no version");
    // send reject packet, why not?
    SendConnectionReject(Drv, clientKey, "invalid handshake", acceptsock, clientaddr);
    return nullptr;
  }

  // read auth hash
  vassert((msg.GetPos()&7) == 0);
  int digpos = msg.GetPos()>>3;
  vuint8 cldig[SHA256_DIGEST_SIZE];
  msg.Serialise(cldig, SHA256_DIGEST_SIZE);
  if (msg.IsError()) {
    GCon->Logf(NAME_DevNet, "connection error: no password hash");
    // send reject packet, why not?
    SendConnectionReject(Drv, clientKey, "invalid handshake", acceptsock, clientaddr);
    return nullptr;
  }

  // read modlist
  vuint32 modhash = 0;
  vuint16 modcount = 0;
  msg << modhash;
  msg << modcount;
  if (msg.IsError()) {
    GCon->Log(NAME_DevNet, "connection error: cannot read modlist");
    // send reject packet
    SendConnectionReject(Drv, clientKey, "invalid handshake", acceptsock, clientaddr);
    return nullptr;
  }

  // fix packet
  memset(msg.GetData()+digpos, 0, SHA256_DIGEST_SIZE);
  // check password
  vuint8 svdig[SHA256_DIGEST_SIZE];
  sha256_ctx shactx;
  sha256_init(&shactx);
  // hash key
  sha256_update(&shactx, clientKey, VNetUtils::ChaCha20KeySize);
  // hash whole packet
  sha256_update(&shactx, msg.GetData(), msg.GetNumBytes());
  // hash password
  sha256_update(&shactx, *net_server_key.asStr(), (unsigned)net_server_key.asStr().length());
  sha256_final(&shactx, svdig);
  // compare
  if (memcmp(cldig, svdig, SHA256_DIGEST_SIZE) != 0) {
    GCon->Logf(NAME_DevNet, "connection error: invalid password");
    // send reject packet, why not?
    SendConnectionReject(Drv, clientKey, "invalid password", acceptsock, clientaddr);
    return nullptr;
  }

  // check version
  if (remVerHi != NET_PROTOCOL_VERSION_HI || remVerLo != NET_PROTOCOL_VERSION_LO) {
    GCon->Logf(NAME_DevNet, "connection error: invalid protocol version, got %u:%u, but expected %u:%u", remVerHi, remVerLo, NET_PROTOCOL_VERSION_HI, NET_PROTOCOL_VERSION_LO);
    // send reject packet, why not?
    SendConnectionReject(Drv, clientKey, "invalid protocol version", acceptsock, clientaddr);
    return nullptr;
  }

  // check modlist
  if (modhash != FL_GetNetWadsHash() || modcount != (vuint16)FL_GetNetWadsCount()) {
    GCon->Log(NAME_DevNet, "connection error: incompatible mod list");
    // send reject packet
    SendConnectionReject(Drv, clientKey, "incompatible loaded mods list", acceptsock, clientaddr, true); // send modlist
    return nullptr;
  }

  // see if this guy is already connected
  for (VSocket *as = Net->ActiveSockets; as; as = as->Next) {
    if (as->Driver != this) continue;
    VDatagramSocket *s = (VDatagramSocket *)as;
    if (s->Invalid) continue;
    // if we already have this client, resend accept packet, and replace client address
    if (memcmp(clientKey, s->ClientKey, VNetUtils::ChaCha20KeySize) == 0) {
      if (Net->GetNetTime()-s->ConnectTime < 2.0) {
        GCon->Logf(NAME_DevNet, "CONN: duplicate connection request from %s (this is ok)", Drv->AddrToString(&clientaddr));
        s->Addr = clientaddr; // update address
        // yes, so send a duplicate reply
        VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
        Drv->GetSocketAddr(s->LanSocket, &newaddr);
        WriteGameSignature(MsgOut);
        TmpByte = CCREP_ACCEPT;
        MsgOut << TmpByte;
        // protocol version
        TmpByte = NET_PROTOCOL_VERSION_HI;
        MsgOut << TmpByte;
        TmpByte = NET_PROTOCOL_VERSION_LO;
        MsgOut << TmpByte;
        // client port
        vint16 TmpPort = Drv->GetSocketPort(&newaddr);
        MsgOut << TmpPort;
        int elen = EncryptInfoBitStream(edata, MsgOut, clientKey);
        if (elen > 0) {
          Drv->Write(acceptsock, edata, elen, &clientaddr);
          return nullptr;
        }
      }
      // drop this
      GCon->Logf(NAME_DevNet, "CONN: DROPPING %s", Drv->AddrToString(&clientaddr));
      // marking existing socket as "invalid" will block all i/o on in (i/o attempt will return error)
      // the corresponding connection will close itself on i/o error
      s->Invalid = true;
      return nullptr;
      /*
      // it is prolly somebody coming back in from a crash/disconnect
      // so close the old socket and let their retry get them back in
      // but don't do that for localhost
      // meh, use strict comparison -- routers and such...
      if (ret != 0 && Drv->IsLocalAddress(&clientaddr) && Drv->IsLocalAddress(&s->Addr)) {
        GCon->Logf(NAME_DevNet, "CONN: LOCALHOST for %s", Drv->AddrToString(&clientaddr));
      } else if (ret == 0) {
        GCon->Logf(NAME_DevNet, "CONN: RETRYWAIT for %s", Drv->AddrToString(&clientaddr));
        s->Invalid = true;
        return nullptr;
      }
      */
    }
  }

  if (svs.num_connected >= svs.max_clients) {
    // no room; try to let him know
    SendConnectionReject(Drv, clientKey, "server is full", acceptsock, clientaddr);
    return nullptr;
  }

  GCon->Logf(NAME_DevNet, "new client from %s (connecting back)", Drv->AddrToString(&clientaddr));

  // update client key
  vuint8 srvKey[VNetUtils::ChaCha20KeySize];
  VNetUtils::DerivePublicKey(srvKey, clientKey);
  memcpy(clientKey, srvKey, sizeof(clientKey));

  // allocate new "listening" network socket
  newsock = Drv->OpenListenSocket(0);
  if (newsock == -1) return nullptr;

  // allocate a VSocket
  // everything is allocated, just fill in the details
  sock = new VDatagramSocket(this);
  memcpy(sock->AuthKey, srvKey, VNetUtils::ChaCha20KeySize);
  memcpy(sock->ClientKey, clientKey, VNetUtils::ChaCha20KeySize);
  sock->LanSocket = newsock;
  sock->LanDriver = Drv;
  sock->Addr = clientaddr;
  sock->Address = Drv->AddrToString(&clientaddr);

  Drv->GetSocketAddr(newsock, &newaddr);

  GCon->Logf(NAME_DevNet, "allocated socket %s for client %s", Drv->AddrToString(&newaddr), *sock->Address);

  // send him back the info about the server connection he has been allocated
  VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
  WriteGameSignature(MsgOut);
  TmpByte = CCREP_ACCEPT;
  MsgOut << TmpByte;
  // protocol version
  TmpByte = NET_PROTOCOL_VERSION_HI;
  MsgOut << TmpByte;
  TmpByte = NET_PROTOCOL_VERSION_LO;
  MsgOut << TmpByte;
  // client port
  vint16 TmpPort = Drv->GetSocketPort(&newaddr);
  MsgOut << TmpPort;
  int elen = EncryptInfoBitStream(edata, MsgOut, clientKey);
  if (elen <= 0) {
    delete sock;
    return nullptr;
  }
  Drv->Write(acceptsock, edata, elen, &clientaddr);

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
VSocket *VDatagramDriver::CheckNewConnections (bool rconOnly) {
  for (int i = 0; i < Net->NumLanDrivers; ++i) {
    if (Net->LanDrivers[i]->initialised) {
      VSocket *ret = CheckNewConnections(Net->LanDrivers[i], rconOnly);
      if (ret != nullptr) return ret;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VDatagramDriver::ResolveMasterAddr
//
//==========================================================================
bool VDatagramDriver::ResolveMasterAddr (VNetLanDriver *Drv) {
  if (LastMasterAddrStr == MasterSrv) return !LastMasterIsBad;
  LastMasterAddrStr = MasterSrv;
  if (Drv->GetAddrFromName(*LastMasterAddrStr, &LastMasterAddr, MASTER_SERVER_PORT) == -1) {
    GCon->Logf(NAME_DevNet, "Could not resolve server name (%s)", *LastMasterAddrStr);
    LastMasterIsBad = true;
    return false;
  }
  GCon->Logf(NAME_DevNet, "resolved master address '%s' to '%s'", *LastMasterAddrStr, Drv->AddrToString(&LastMasterAddr));
  LastMasterIsBad = false;
  return true;
}


//==========================================================================
//
//  VDatagramDriver::UpdateMaster
//
//==========================================================================
void VDatagramDriver::UpdateMaster (VNetLanDriver *Drv) {
  // see if we can resolve the host name
  if (!ResolveMasterAddr(Drv)) return;

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
  TmpByte = NET_PROTOCOL_VERSION_HI;
  MsgOut << TmpByte;
  TmpByte = NET_PROTOCOL_VERSION_LO;
  MsgOut << TmpByte;
  Drv->Write(Drv->net_acceptsocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &LastMasterAddr);
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
  // see if we can resolve the host name
  if (!ResolveMasterAddr(Drv)) return;

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
  Drv->Write(Drv->net_acceptsocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &LastMasterAddr);
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
  vuint8 control;
  vuint8 TmpByte;

  if (Drv->MasterQuerySocket < 0) Drv->MasterQuerySocket = Drv->OpenListenSocket(0);

  Drv->GetSocketAddr(Drv->MasterQuerySocket, &myaddr);
  if (xmit) {
    // see if we can resolve the host name
    if (!ResolveMasterAddr(Drv)) return false;
    // send the query request
    VBitStreamWriter MsgOut(MAX_DGRAM_SIZE<<3);
    WriteGameSignature(MsgOut);
    TmpByte = MASTER_PROTO_VERSION;
    MsgOut << TmpByte;
    TmpByte = MCREQ_LIST;
    MsgOut << TmpByte;
    Drv->Write(Drv->MasterQuerySocket, MsgOut.GetData(), MsgOut.GetNumBytes(), &LastMasterAddr);
    GCon->Logf(NAME_DevNet, "sent query to master at %s", Drv->AddrToString(&LastMasterAddr));
    return false;
  }

  //GCon->Logf(NAME_DevNet, "waiting for master reply");

  bool res = false;
  int pktleft = 256;
  while (pktleft-- > 0) {
    int len = Drv->Read(Drv->MasterQuerySocket, packetBuffer.data, MAX_DGRAM_SIZE, &readaddr);
    if (len < 1) continue; // error, no message, zero message

    GCon->Logf(NAME_DevNet, "got master reply from %s (%d bytes)", Drv->AddrToString(&readaddr), len);
    // is the cache full?
    //if (Net->HostCacheCount == HOSTCACHESIZE) continue;

    //GCon->Logf("processing master reply");
    VBitStreamReader msg(packetBuffer.data, len<<3);
    if (!CheckGameSignature(msg)) continue;

    msg << TmpByte;
    if (msg.IsError() || TmpByte != MASTER_PROTO_VERSION) continue;

    msg << control;
    if (msg.IsError() || control != MCREP_LIST) continue;

    msg << control; // control byte: bit 0 means "first packet", bit 1 means "last packet"
    if (msg.IsError()) continue;
    //GCon->Logf(NAME_DevNet, "  control byte: 0x%02x", (unsigned)control);

    //if ((control&0x01) == 0) continue; // first packed is missing, but nobody cares

    while (!msg.AtEnd()) {
      vuint8 pver0 = 0, pver1 = 0;
      tmpaddr = readaddr;
      msg.Serialise(&pver0, 1);
      msg.Serialise(&pver1, 1);
      msg.Serialise(tmpaddr.sa_data+2, 4);
      msg.Serialise(tmpaddr.sa_data, 2);
      if (!msg.IsError() && pver0 == NET_PROTOCOL_VERSION_HI && pver1 == NET_PROTOCOL_VERSION_LO) {
        GCon->Logf(NAME_DevNet, "  sending server query to %s", Drv->AddrToString(&tmpaddr));
        VBitStreamWriter MsgOut(MAX_INFO_DGRAM_SIZE<<3);
        WriteGameSignature(MsgOut);
        TmpByte = CCREQ_SERVER_INFO;
        MsgOut << TmpByte;
        TmpByte = NET_PROTOCOL_VERSION_HI;
        MsgOut << TmpByte;
        TmpByte = NET_PROTOCOL_VERSION_LO;
        MsgOut << TmpByte;
        // encrypt and send
        vuint8 edata[MAX_DGRAM_SIZE];
        vuint8 key[VNetUtils::ChaCha20KeySize];
        VNetUtils::GenerateKey(key);
        int elen = EncryptInfoBitStream(edata, MsgOut, key);
        if (elen > 0) Drv->Write(Drv->controlSock, edata, elen, &tmpaddr);
      } else if (msg.IsError()) {
        GCon->Logf(NAME_DevNet, "  server: %s, error reading reply, size=%d; pos=%d", Drv->AddrToString(&tmpaddr), msg.GetNumBits(), msg.GetPos());
      } else {
        GCon->Logf(NAME_DevNet, "  server: %s, bad proto version %u:%u", Drv->AddrToString(&tmpaddr), pver0, pver1);
        /*
        int f = 0;
        while (f < len) {
          VStr s = "   ";
          for (int c = 0; f+c < len && c < 16; ++c) {
            if (c == 8) s += " ";
            s += " ";
            const char *hexd = "0123456789abcdef";
            s += hexd[(packetBuffer.data[f+c]>>4)&0x0fu];
            s += hexd[packetBuffer.data[f+c]&0x0fu];
          }
          GCon->Logf(NAME_DevNet, "%s", *s);
          f += 16;
        }
        */
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
  vuint8 data[MAX_DGRAM_SIZE+4];

  for (;;) {
    // read message
    int length = LanDriver->Read(LanSocket, data, NET_DATAGRAMSIZE, &readaddr);
    if (length == 0) continue; // zero-sized message, oops

    if (length == -2) {
      // no more messages
      //if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: no data from %s (expected address is %s)", LanDriver->AddrToString(&readaddr), *LanDriver->AddrToString(&Addr));
      return 0;
    }

    if (length < 0) {
      GCon->Logf(NAME_DevNet, "%s: Read error", LanDriver->AddrToString(&Addr));
      return -1;
    }

    if (LanDriver->AddrCompare(&readaddr, &Addr) != 0) {
      if (net_dbg_dump_rejected_connections) GCon->Logf(NAME_DevNet, "CONN: rejected packet from %s due to wrong address (%s expected)", LanDriver->AddrToString(&readaddr), LanDriver->AddrToString(&Addr));
      UpdateRejectedStats(length);
      continue;
    }

    UpdateReceivedStats(length);

    if ((unsigned)length > destSize) {
      GCon->Logf(NAME_DevNet, "%s: Read error (message too big)", LanDriver->AddrToString(&Addr));
      return -1;
    }

    memcpy(dest, data, length);
    return length;
  }
  abort();
}


//==========================================================================
//
//  VDatagramSocket::SendMessage
//
//  Send a packet over the net connection.
//  returns  1 if the packet was sent properly
//  returns -1 if the connection died
//  returns  0 if the connection is oversaturated (cannot send more data)
//
//==========================================================================
int VDatagramSocket::SendMessage (const vuint8 *Data, vuint32 Length) {
  vensure(Length > 0);
  vensure(Length <= MAX_DGRAM_SIZE);
  if (Invalid) return -1;
  const int res = LanDriver->Write(LanSocket, Data, Length, &Addr);
  if (res > 0) UpdateSentStats(Length);
  if (res == -2) return 0;
  //GCon->Logf(NAME_DevNet, "+++ SK:%d(%s): sent %u bytes of data (res=%d)", LanSocket, LanDriver->AddrToString(&Addr), Length, res);
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
