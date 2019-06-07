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
#ifndef VAVOOM_NETWORK_HEADER
#define VAVOOM_NETWORK_HEADER

class VNetContext;

// network message class
#include "net_message.h"

// packet header IDs.
// since control and data communications are on different ports, there should
// never be a case when these are mixed. but I will keep it for now just in case.
enum {
  NETPACKET_DATA = 0x40,
  NETPACKET_CTL  = 0x80,
};

//FIXME!
//TODO!
// separate messages from packets, so we can fragment and reassemble one message with several packets
// without this, it is impossible to not hit packet limit
enum {
  MAX_MSGLEN              = /*1024*/1280-40/*+1024*/, // max length of a message
  MAX_PACKET_HEADER_BITS  = /*40*/42, //k8: 41 for 32-bit sequence number, 1 is reserved
  MAX_PACKET_TRAILER_BITS = /*1*/7, //k8: or 7? was 1
  MAX_MESSAGE_HEADER_BITS = /*63*/118, //k8: isack(1), chanindex(41), reliable(1), open(1), close(1), seq(41), chantype(11), len(21)
  OUT_MESSAGE_SIZE        = MAX_MSGLEN*8-MAX_PACKET_HEADER_BITS-MAX_MESSAGE_HEADER_BITS-MAX_PACKET_TRAILER_BITS,
};

enum { MAX_CHANNELS = 1024 };

enum EChannelType {
  CHANNEL_Control = 1,
  CHANNEL_Level,
  CHANNEL_Player,
  CHANNEL_Thinker,
  CHANNEL_ObjectMap,

  CHANNEL_MAX = 8,
};

enum EChannelIndex {
  CHANIDX_General,
  CHANIDX_Player,
  CHANIDX_Level,
  CHANIDX_ThinkersStart
};


// ////////////////////////////////////////////////////////////////////////// //
// public interface of a network socket
class VSocketPublic : public VInterface {
public:
  VStr Address;

  double ConnectTime;
  double LastMessageTime;

  virtual bool IsLocalConnection () = 0;
  virtual int GetMessage (TArray<vuint8> &) = 0;
  virtual int SendMessage (const vuint8 *, vuint32) = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// an entry into a hosts cache table
struct hostcache_t {
  VStr Name;
  VStr Map;
  VStr CName;
  VStr WadFiles[20];
  vint32 Users;
  vint32 MaxUsers;
};


// ////////////////////////////////////////////////////////////////////////// //
// structure returned to progs
struct slist_t {
  enum { SF_InProgress = 0x01 };
  vuint32 Flags;
  hostcache_t *Cache;
  vint32 Count;
  VStr ReturnReason;
};


// ////////////////////////////////////////////////////////////////////////// //
// public networking driver interface
class VNetworkPublic : public VInterface {
public:
  // public API
  double NetTime;

  // statistic counters
  int MessagesSent;
  int MessagesReceived;
  int UnreliableMessagesSent;
  int UnreliableMessagesReceived;
  int packetsSent;
  int packetsReSent;
  int packetsReceived;
  int receivedDuplicateCount;
  int shortPacketCount;
  int droppedDatagrams;

  static VCvarF MessageTimeOut;

  VNetworkPublic ();

  virtual void Init () = 0;
  virtual void Shutdown () = 0;
  virtual VSocketPublic *Connect (const char *) = 0;
  virtual VSocketPublic *CheckNewConnections () = 0;
  virtual void Poll () = 0;
  virtual void StartSearch (bool) = 0;
  virtual slist_t *GetSlist () = 0;
  virtual double SetNetTime () = 0;
  virtual void UpdateMaster () = 0;
  virtual void QuitMaster () = 0;

  static VNetworkPublic *Create ();
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for network channels that are responsible for sending and receiving of the data.
class VChannel {
public:
  VNetConnection *Connection;
  vint32 Index;
  vuint8 Type;
  vuint8 OpenedLocally;
  vuint8 OpenAcked;
  vuint8 Closing;
  VMessageIn *InMsg;
  VMessageOut *OutMsg;

  VChannel (VNetConnection *, EChannelType, vint32, vuint8);
  virtual ~VChannel ();

  // VChannel interface
  void ReceivedRawMessage (VMessageIn &);
  virtual void ParsePacket (VMessageIn &) = 0;
  void SendMessage (VMessageOut *);
  virtual void ReceivedAck ();
  virtual void Close ();
  virtual void Tick ();
  void SendRpc (VMethod *, VObject *);
  bool ReadRpc (VMessageIn &Msg, int, VObject *);

  int CountInMessages () const;
  int CountOutMessages () const;
};


// ////////////////////////////////////////////////////////////////////////// //
class VControlChannel : public VChannel {
public:
  VControlChannel (VNetConnection *, vint32, vuint8 = true);

  // VChannel interface
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating level data
class VLevelChannel : public VChannel {
public:
  struct VBodyQueueTrInfo {
    vuint8 TranslStart;
    vuint8 TranslEnd;
    vint32 Color;
  };

  VLevel *Level;
  rep_line_t *Lines;
  rep_side_t *Sides;
  rep_sector_t *Sectors;
  rep_polyobj_t *PolyObjs;
  TArray<VCameraTextureInfo> CameraTextures;
  TArray<TArray<VTextureTranslation::VTransCmd> > Translations;
  TArray<VBodyQueueTrInfo> BodyQueueTrans;

  VLevelChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VLevelChannel () override;
  void SetLevel (VLevel *);
  void Update ();
  void SendNewLevel ();
  void SendStaticLights ();
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating thinkers
class VThinkerChannel : public VChannel {
public:
  VThinker *Thinker;
  VClass *ThinkerClass;
  vuint8 *OldData;
  bool NewObj;
  bool UpdatedThisFrame;
  vuint8 *FieldCondValues;

  VThinkerChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VThinkerChannel () override;
  void SetThinker (VThinker *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();
  virtual void ParsePacket (VMessageIn &) override;
  virtual void Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating player data
class VPlayerChannel : public VChannel {
public:
  VBasePlayer *Plr;
  vuint8 *OldData;
  bool NewObj;
  vuint8 *FieldCondValues;

  VPlayerChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VPlayerChannel () override;
  void SetPlayer (VBasePlayer *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for sending object map at startup
class VObjectMapChannel : public VChannel {
private:
  vint32 CurrName;
  vint32 CurrClass;
  vint32 LastNameCount;
  vint32 LastClassCount;

protected:
  // `Msg.bOpen` must be valid
  void WriteCounters (VMessageOut &Msg);
  // `Msg.bOpen` must be valid
  void ReadCounters (VMessageIn &Msg);

public:
  VObjectMapChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VObjectMapChannel () override;
  virtual void Tick () override;
  void Update ();
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
enum ENetConState {
  NETCON_Closed,
  NETCON_Open,
};


// ////////////////////////////////////////////////////////////////////////// //
// network connection class, responsible for sending and receiving network
// packets and managing of channels.
class VNetConnection {
protected:
  VSocketPublic *NetCon;

public:
  VNetworkPublic *Driver;
  VNetContext *Context;
  VBasePlayer *Owner;
  ENetConState State;
  double LastSendTime;
  bool NeedsUpdate;
  bool AutoAck;
  VBitStreamWriter Out;
  VChannel *Channels[MAX_CHANNELS];
  TArray<VChannel *> OpenChannels;
  vuint32 InSequence[MAX_CHANNELS];
  vuint32 OutSequence[MAX_CHANNELS];
  TMap<VThinker *, VThinkerChannel *> ThinkerChannels;
  vuint32 AckSequence;
  vuint32 UnreliableSendSequence;
  vuint32 UnreliableReceiveSequence;
  VNetObjectsMap *ObjMap;
  bool ObjMapSent;
  bool LevelInfoSent;

private:
  vuint8 *UpdatePvs;
  int UpdatePvsSize;
  const vuint8 *LeafPvs;
  VViewClipper Clipper;

public:
  VNetConnection (VSocketPublic *, VNetContext *, VBasePlayer *);
  virtual ~VNetConnection ();

  // VNetConnection interface
  void GetMessages ();
  virtual int GetRawPacket (TArray<vuint8> &);
  void ReceivedPacket (VBitStreamReader &);
  VChannel *CreateChannel (vuint8, vint32, vuint8 = true);
  virtual void SendRawMessage (VMessageOut &);
  virtual void SendAck (vuint32);
  void PrepareOut (int);
  void Flush ();
  bool IsLocalConnection ();
  inline VStr GetAddress () const { return (NetCon ? NetCon->Address : VStr()); }
  void Tick ();
  void SendCommand (VStr Str);
  void SetUpFatPVS ();
  int CheckFatPVS (subsector_t *);
  bool SecCheckFatPVS (sector_t *);
  bool IsRelevant (VThinker *Th);
  void UpdateLevel ();
  void SendServerInfo ();

private:
  void SetUpPvsNode (int, float *);
};


// ////////////////////////////////////////////////////////////////////////// //
// class that provides access to client or server specific data
class VNetContext {
public:
  VField *RoleField;
  VField *RemoteRoleField;
  VNetConnection *ServerConnection;
  TArray<VNetConnection *> ClientConnections;

  VNetContext ();
  virtual ~VNetContext ();

  // VNetContext interface
  virtual VLevel *GetLevel() = 0;
  void ThinkerDestroyed (VThinker *);
  void Tick ();
};


// ////////////////////////////////////////////////////////////////////////// //
// a client side network context
class VClientNetContext : public VNetContext {
public:
  // VNetContext interface
  virtual VLevel *GetLevel () override;
};


// ////////////////////////////////////////////////////////////////////////// //
// server side network context
class VServerNetContext : public VNetContext {
public:
  // VNetContext interface
  virtual VLevel *GetLevel () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VNetObjectsMap {
private:
  TArray<VName> NameLookup;
  TArray<int> NameMap;

  TArray<VClass *> ClassLookup;
  TMap<VClass *, vuint32> ClassMap;

public:
  VNetConnection *Connection;

public:
  VNetObjectsMap ();
  VNetObjectsMap (VNetConnection *);

  void SetUpClassLookup ();
  bool CanSerialiseObject (VObject *);
  bool SerialiseName (VStream &, VName &);
  bool SerialiseObject (VStream &, VObject *&);
  bool SerialiseClass (VStream &, VClass *&);
  bool SerialiseState (VStream &, VState *&);

  friend class VObjectMapChannel;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoPlaybackNetConnection : public VNetConnection {
public:
  float NextPacketTime;
  bool bTimeDemo;
  VStream *Strm;
  int td_lastframe; // to meter out one message a frame
  int td_startframe;  // host_framecount at start
  double td_starttime; // realtime at second frame of timedemo

  VDemoPlaybackNetConnection (VNetContext *, VBasePlayer *, VStream *, bool);
  virtual ~VDemoPlaybackNetConnection () override;

  // VNetConnection interface
  virtual int GetRawPacket (TArray<vuint8> &) override;
  virtual void SendRawMessage (VMessageOut &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingNetConnection : public VNetConnection {
public:
  VDemoRecordingNetConnection (VSocketPublic *, VNetContext *, VBasePlayer *);

  // VNetConnection interface
  virtual int GetRawPacket (TArray<vuint8> &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingSocket : public VSocketPublic {
public:
  virtual bool IsLocalConnection () override;
  virtual int GetMessage (TArray<vuint8> &) override;
  virtual int SendMessage (const vuint8 *, vuint32) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// global access to the low-level networking services
extern VNetworkPublic *GNet;
extern VNetContext *GDemoRecordingContext;


#endif
