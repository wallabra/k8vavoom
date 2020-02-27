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
#ifndef VAVOOM_NETWORK_HEADER
#define VAVOOM_NETWORK_HEADER

//#define VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS


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

// sent with `CMD_NewLevel`
enum {
  NETWORK_PROTO_VERSION = 2,
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
  CHANIDX_ObjectMap,
  CHANIDX_ThinkersStart,
  //
  CHANIDX_KnownBmpMask = 0x1111u,
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
  VMessageOut *OutMsg; // sent reliable messages; we want ACK for them
  // if `Closing` flag is set, we should remove the channel when `ReceivedAck()` returns `true`
  // until then, the channel should be alive, because it may contain some reliable messages to resend
  // also, it is safe to remove the channel if `OutMsg` is `nullptr` (we have no messages to ack)

public:
  VChannel (VNetConnection *, EChannelType, vint32, vuint8);
  virtual ~VChannel ();

  void ClearAllQueues ();
  void ClearOutQueue ();

  // is it safe to remove this channel?
  inline bool IsDead () const noexcept { return (Closing && OutMsg == nullptr); }

  inline bool IsThinker () const noexcept { return (Type == CHANNEL_Thinker); }

  // call this instead of `delete this`
  // connection ticker will take care of closed channels
  // WARNING! this will not call `Close()`, but will set `Closing` flag directly
  virtual void Suicide ();

  // VChannel interface
  void ReceivedRawMessage (VMessageIn &);
  virtual void ParsePacket (VMessageIn &) = 0;
  void SendMessage (VMessageOut *);
  virtual bool ReceivedAck (); // returns `true` if closing ack received (the caller should delete it)
  virtual void Close ();
  virtual void Tick ();
  // called when `msg->udata` is not zero
  // DO NOT delete message, DO NOT store pointer to it!
  virtual void SpecialAck (VMessageOut *msg);

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
  virtual void Suicide () override;
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

  VStr serverInfoBuf;
  int severInfoPacketCount;
  int severInfoCurrPacket;
  ClientServerInfo csi;

public:
  VLevelChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VLevelChannel () override;
  void SetLevel (VLevel *);
  void Update ();
  void SendNewLevel ();
  void SendStaticLights ();
  void ResetLevel ();
  virtual void Suicide () override;
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

public:
  VThinkerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally=true);
  virtual ~VThinkerChannel () override;
  void SetThinker (VThinker *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();
  virtual void Suicide () override;
  virtual void ParsePacket (VMessageIn &) override;
  virtual void Close () override;

  void RemoveThinkerFromGame ();
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating player data
class VPlayerChannel : public VChannel {
public:
  VBasePlayer *Plr;
  vuint8 *OldData;
  bool NewObj;
  vuint8 *FieldCondValues;

public:
  VPlayerChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VPlayerChannel () override;
  void SetPlayer (VBasePlayer *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();
  virtual void Suicide () override;
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
  virtual void Suicide () override;
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

protected:
  // open channels (in random order)
  //TArray<VChannel *> OpenChannels;
  // non-thinker channels
  VChannel *KnownChannels[CHANIDX_ThinkersStart];
  vuint32 ChanFreeBitmap[(MAX_CHANNELS+31)/32];
  // use random channel ids for thinkers
  int ChanFreeIds[MAX_CHANNELS];
  unsigned ChanFreeIdsUsed;
  // thinker channels
  TMapNC<vint32, VChannel *> ChanIdxMap; // index -> channel
  // if this flag is set, we *may* have some dead channels, and should call `RemoveDeadThinkerChannels()`
  bool HasDeadChannels;

protected:
  void ReleaseThinkerChannelId (int idx);
  int GetRandomThinkerChannelId ();

public:
  void RegisterChannel (VChannel *chan);
  void UnregisterChannel (VChannel *chan, bool touchMap=true);

  // can return -1 if there are no free thinker channels
  int AllocThinkerChannelId ();

  inline void MarkChannelsDirty () noexcept { HasDeadChannels = true; }

  inline VChannel *GetGeneralChannel () noexcept { return KnownChannels[CHANIDX_General]; }
  inline VPlayerChannel *GetPlayerChannel () noexcept { return (VPlayerChannel *)(KnownChannels[CHANIDX_Player]); }
  inline VLevelChannel *GetLevelChannel () noexcept { return (VLevelChannel *)(KnownChannels[CHANIDX_Level]); }

  inline VChannel *GetChannelByIndex (int idx) noexcept { auto pp = ChanIdxMap.find(idx); return (pp ? *pp : nullptr); }

public:
  VNetworkPublic *Driver;
  VNetContext *Context;
  VBasePlayer *Owner;
  ENetConState State;
  double LastSendTime;
  bool NeedsUpdate;
  bool AutoAck;
  VBitStreamWriter Out;
  //VChannel *Channels[MAX_CHANNELS];
  //TArray<VChannel *> OpenChannels;
  vuint32 InSequence[MAX_CHANNELS];
  vuint32 OutSequence[MAX_CHANNELS];
  TMapNC<VThinker *, VThinkerChannel *> ThinkerChannels;
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
  // this is used in `VNetConnection::UpdateLevel()` to collect
  // thinkers that wants to be updated, but we have no free channel.
  // after visible entities are updated, we will rescan channels, and
  // append close-up entities if some channels are freed.
  // this is temporary buffer, only valid in that method.
  TArray<VThinker *> PendingThinkers;
  TArray<VEntity *> PendingGoreEnts;
  TArray<vint32> AliveGoreChans;

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

  // if `resetUpdated` is true, reset "updated" flag for thinker channels
  void RemoveDeadThinkerChannels (bool resetUpdated=false);

  void PrepareOut (int);
  void Flush ();
  void FlushOutput (); // call this to send all queued packets at the end of the frame
  bool IsLocalConnection ();
  inline VStr GetAddress () const { return (NetCon ? NetCon->Address : VStr()); }
  void Tick ();
  void SendCommand (VStr Str);
  void SetupFatPVS ();
  int CheckFatPVS (subsector_t *);
  bool SecCheckFatPVS (sector_t *);
  bool IsRelevant (VThinker *Th);
  void UpdateLevel ();
  void SendServerInfo ();
  void LoadedNewLevel ();
  void ResetLevel ();
  // for demo playback
  virtual void Intermission (bool active);

private:
  void SetupPvsNode (int, float *);
};


// ////////////////////////////////////////////////////////////////////////// //
// class that provides access to client or server specific data
class VNetContext {
public:
  VField *RoleField;
  VField *RemoteRoleField;
  VNetConnection *ServerConnection; // non-nullptr for clients (only)
  TArray<VNetConnection *> ClientConnections; // known clients for servers

public:
  VNetContext ();
  virtual ~VNetContext ();

  inline bool IsClient () const noexcept { return (ServerConnection != nullptr); }
  inline bool IsServer () const noexcept { return (ServerConnection == nullptr); }

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
class VNetObjectsMap : public VNetObjectsMapBase {
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
  virtual ~VNetObjectsMap ();

  void SetupClassLookup ();
  bool CanSerialiseObject (VObject *);

  virtual bool SerialiseName (VStream &, VName &) override;
  virtual bool SerialiseObject (VStream &, VObject *&) override;
  virtual bool SerialiseClass (VStream &, VClass *&) override;
  virtual bool SerialiseState (VStream &, VState *&) override;

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
  bool inIntermission;

public:
  VDemoPlaybackNetConnection (VNetContext *, VBasePlayer *, VStream *, bool);
  virtual ~VDemoPlaybackNetConnection () override;

  // VNetConnection interface
  virtual int GetRawPacket (TArray<vuint8> &) override;
  virtual void SendRawMessage (VMessageOut &) override;

  virtual void Intermission (bool active) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingNetConnection : public VNetConnection {
public:
  bool inIntermission;

public:
  VDemoRecordingNetConnection (VSocketPublic *, VNetContext *, VBasePlayer *);

  // VNetConnection interface
  virtual int GetRawPacket (TArray<vuint8> &) override;

  virtual void Intermission (bool active) override;
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
