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

extern VCvarB net_fixed_name_set;
extern VCvarB net_debug_fixed_name_set;


class VNetContext;

// packet header IDs.
// since control and data communications are on different ports, there should
// never be a case when these are mixed. but I will keep it for now just in case.
enum {
  NETPACKET_DATA   = 0x40,
  NETPACKET_DATA_C = 0x44, // combined reliable and unreliable packet data
  NETPACKET_CTL    = 0x80,
};

// sent on handshake, and with `CMD_NewLevel`
// I don't think that communication protocol will change, but just in a case
// k8: and it did
enum {
  NET_PROTOCOL_VERSION = 4,
};

//FIXME!
//TODO!
// separate messages from packets, so we can fragment and reassemble one message with several packets
enum {
  MAX_DGRAM_SIZE    = 1400, // max length of a datagram; fuck off, ipshit6
  MSG_HEADER_SIZE   = 2*4, // seq, ackseq
  MAX_MSG_SIZE      = MAX_DGRAM_SIZE-MSG_HEADER_SIZE-1, // max length of a message (excluding header, and NETPACKET_DATA)

  MAX_MSG_SIZE_BITS = MAX_MSG_SIZE*8,
};

enum {
  MAX_CHANNELS = 1024,
};

enum EChannelType {
  CHANNEL_Control = 1,
  CHANNEL_Level,
  CHANNEL_Player,
  CHANNEL_Thinker,
  CHANNEL_ObjectMap,

  CHANNEL_MAX,
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


// used in level channel
struct VNetClientServerInfo {
  VStr mapname;
  VStr maphash;
  vuint32 modhash;
  VStr sinfo;
  int maxclients;
  int deathmatch;
};


// network message classes
//#include "net_message.h"
class VMessageIn;
class VMessageOut;


// ////////////////////////////////////////////////////////////////////////// //
// public interface of a network socket
class VSocketPublic : public VInterface {
public:
  VStr Address;

  double ConnectTime = 0;
  double LastMessageTime = 0;

  vuint64 bytesSent = 0;
  vuint64 bytesReceived = 0;
  vuint64 bytesRejected = 0;
  unsigned minimalSentPacket = 0;
  unsigned maximalSentPacket = 0;

  virtual bool IsLocalConnection () = 0;
  virtual int GetMessage (TArray<vuint8> &) = 0;
  virtual int SendMessage (const vuint8 *, vuint32) = 0;

  virtual void UpdateSentStats (vuint32 length) noexcept;
  virtual void UpdateReceivedStats (vuint32 length) noexcept;
  virtual void UpdateRejectedStats (vuint32 length) noexcept;

  virtual void DumpStats ();

  static VStr u64str (vuint64 v, bool comatose=true) noexcept;
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
  vuint64 bytesSent;
  vuint64 bytesReceived;
  vuint64 bytesRejected;
  unsigned minimalSentPacket;
  unsigned maximalSentPacket;

  // if set, `Connect()` will call this to check if user aborted the process
  // return `true` if aborted
  bool (*CheckUserAbortCB) (void *udata);
  void *CheckUserAbortUData; // user-managed

public:
  static VCvarF MessageTimeOut;

public:
  VNetworkPublic ();

  inline bool CheckForUserAbort () { return (CheckUserAbortCB ? CheckUserAbortCB(CheckUserAbortUData) : false); }

  virtual void Init () = 0;
  virtual void Shutdown () = 0;
  virtual VSocketPublic *Connect (const char *InHost) = 0;
  virtual VSocketPublic *CheckNewConnections () = 0;
  virtual void Poll () = 0;
  virtual void StartSearch (bool) = 0;
  virtual slist_t *GetSlist () = 0;
  virtual double SetNetTime () = 0;
  virtual void UpdateMaster () = 0;
  virtual void QuitMaster () = 0;

  void UpdateSentStats (vuint32 length) noexcept;
  void UpdateReceivedStats (vuint32 length) noexcept;
  void UpdateRejectedStats (vuint32 length) noexcept;

public:
  static VNetworkPublic *Create ();
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for network channels that are responsible for sending and receiving of the data.
class VChannel {
public:
  VNetConnection *Connection;
  vint32 Index;
  vuint8 Type;
  bool OpenedLocally;
  bool OpenAcked;
  // `true` if we're waiting ACK for close packet
  bool Closing;
  // `true` if we got ACK for close packet
  bool CloseAcked;

  struct RPCOut {
    bool reliable;
    VBitStreamWriter *strm;
    RPCOut *next;
    VStr debugName;
  };

  RPCOut *rpcOutHead;
  RPCOut *rpcOutTail;

public:
  inline static const char *GetChanTypeName (vuint8 type) noexcept {
    switch (type) {
      case 0: return "<broken0>";
      case CHANNEL_Control: return "Control";
      case CHANNEL_Level: return "Level";
      case CHANNEL_Player: return "Player";
      case CHANNEL_Thinker: return "Thinker";
      case CHANNEL_ObjectMap: return "ObjectMap";
      default: return "<internalerror>";
    }
  }

protected:
  bool WillFlushMsg (VMessageOut &msg, VBitStreamWriter &strm);
  // moves steam to msg (sending previous msg if necessary)
  void PutStream (VMessageOut &msg, VBitStreamWriter &strm);
  // sends message if it is not empty, and clears it
  void FlushMsg (VMessageOut &msg);

public:
  VChannel (VNetConnection *, EChannelType, vint32, vuint8);
  virtual ~VChannel ();

  inline const char *GetTypeName () const noexcept { return GetChanTypeName(Type); }

  virtual VStr GetName () const noexcept;
  VStr GetDebugName () const noexcept;

  // is it safe to remove this channel?
  //inline bool IsDead () const noexcept { return (Closing && OutMsg == nullptr); }
  inline bool IsDead () const noexcept { return (Closing && CloseAcked); }

  inline bool IsThinker () const noexcept { return (Type == CHANNEL_Thinker); }

  // is this channel opened locally?
  inline bool IsLocalChannel () const noexcept { return OpenedLocally; }

  // is this channel opened from the other end?
  inline bool IsRemoteChannel () const noexcept { return !OpenedLocally; }

  VBitStreamWriter &AllocRPCOut (bool reliable, VStr dbgname=VStr::EmptyString);

  void SendRPCOuts (VMessageOut &msg);
  void DropUnreliableRPCOuts ();
  void DropAllRPCOuts ();

  // fill buffer with unreliable RPC data while we can, drop all unfit RPC data
  void PutUnreliableRPCOut (VBitStreamWriter &wrs, const vuint32 maxWrsSizeBits);

  // call this instead of `delete this`
  // connection ticker will take care of closed channels
  // WARNING! this will not call `Close()`, but will set `Closing` flag directly
  virtual void Suicide ();

  // VChannel interface
  // this is called when new message for this channel is received
  void ReceivedRawMessage (VMessageIn &Msg);

  void SendMessage (VMessageOut &Msg);

  // called by `ReceivedRawMessage()` to parse new message
  // this call is sequenced (i.e. the sequence is right)
  virtual void ParsePacket (VMessageIn &Msg) = 0;

  // some channels may want to set some flags here; WARNING! don't close/suicide here!
  // this is called by `ReceivedRawMessage()` *after* `ParsePacket()` is called
  // won't be called for already closing channels
  virtual void ReceivedClosingAck ();

  // this sends reliable "close" message, if the channel in not on closing state yet
  // you can pass already created message to use its header instead of creating a separate "close" msg
  // passed message will be put into send queue (i.e. there's no need to separately send it)
  virtual void Close (VMessageOut *msg=nullptr);

  // call this periodically to perform various processing
  virtual void Tick ();

  void SendRpc (VMethod *, VObject *);
  bool ReadRpc (VMessageIn &Msg, int, VObject *);
};


// ////////////////////////////////////////////////////////////////////////// //
class VControlChannel : public VChannel {
public:
  VControlChannel (VNetConnection *, vint32, vuint8 = true);

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating level data
class VLevelChannel : public VChannel {
  // server states
  enum {
    // we just sent level info to the client
    SState_NewLevelSent,
    // we got ack for the last server info packet
    // this means that the client is loading a map, so we should use bigger timeout
    SState_ServerInfoAcked,
    // we got `CMD_ClientMapLoaded` packet (i.e. client is ready, use normal timeout)
  };

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
  VNetClientServerInfo csi;

  //int srvState; // see SState_*

public:
  VLevelChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VLevelChannel () override;
  void SetLevel (VLevel *);
  void Update ();
  void SendNewLevel ();
  void SendStaticLights ();
  void ResetLevel ();
  virtual VStr GetName () const noexcept override;
  virtual void ParsePacket (VMessageIn &) override;
  // used on the client to initiate map loading
  //virtual void SpecialAck (VMessageOut *msg) override;
  // call this from client when the map is ready
  //void SentClientMapLoaded ();
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
  virtual VStr GetName () const noexcept override;
  virtual void Suicide () override;
  virtual void ParsePacket (VMessageIn &) override;
  virtual void ReceivedClosingAck () override;
  virtual void Close (VMessageOut *msg=nullptr) override;

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
  virtual VStr GetName () const noexcept override;
  virtual void ParsePacket (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for sending object map at startup
class VObjectMapChannel : public VChannel {
private:
  vint32 CurrName;
  vint32 CurrClass;
  bool needOpenMessage; // valid only for local channel (sender)

protected:
  void UpdateSendPBar ();

public:
  VObjectMapChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally);
  virtual ~VObjectMapChannel () override;
  virtual void Tick () override;
  void Update ();
  void ReceivedClosingAck () override; // sets `ObjMapSent` flag
  virtual VStr GetName () const noexcept override;
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
  // non-thinker channels
  VChannel *KnownChannels[CHANIDX_ThinkersStart];
  vuint32 ChanFreeBitmap[(MAX_CHANNELS+31)/32];
  // use random channel ids for thinkers
  int ChanFreeIds[MAX_CHANNELS];
  unsigned ChanFreeIdsUsed;
  // thinker channels (also used to iterate for ticking)
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
  double NextUpdateTimeThinkers; // this is used by the server to limit updates
  double NextUpdateTimeLevel; // this is used by the server to limit updates
  bool NeedsUpdate; // should we call `VNetConnection::UpdateLevel()`? this updates mobjs in sight, and sends new mobj state
  bool AutoAck; // `true` for demos
  VBitStreamWriter Out;
  TMapNC<VThinker *, VThinkerChannel *> ThinkerChannels;

  // queued message
  struct QMessage {
    vuint8 data[MAX_MSG_SIZE];
    vuint32 dataSize;
    QMessage *next;
  };
  // this is current message to send/ack
  QMessage *sendQueueHead;
  // this is where we'll add our messages
  QMessage *sendQueueTail;
  int sendQueueSize;

  // for client, doesn't matter
  // for server, set when we got something from client
  bool AllowMessageSend;

  //vuint8 sendData[MAX_DGRAM_SIZE];
  //vuint32 sendDataSize;

  // current reliable message is kept here (and it is popped from the send queue)
  vuint8 relSendData[MAX_DGRAM_SIZE];
  vuint32 relSendDataSize;

  //vuint8 recvData[MAX_DGRAM_SIZE];
  //vuint32 recvDataSize;

  // sequencing
  vuint32 incoming_sequence;
  vuint32 incoming_acknowledged;
  vuint32 incoming_reliable_acknowledged; // one bit
  vuint32 incoming_reliable_sequence; // one bit; maintained local

  vuint32 outgoing_sequence;
  vuint32 reliable_sequence; // one bit
  vuint32 last_reliable_sequence;

  VNetObjectsMap *ObjMap;
  // various flags
  bool ObjMapSent;
  bool LevelInfoSent;
  // when we detach thinker, there's no need to send it anymore
  // we cannot have this flag in thinker itself, because new
  // clients should still get detached thinkers
  TMapNC<VThinker *, bool> DetachedThinkers;

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

protected:
  void ProcessSendQueue ();
  bool ProcessRecvQueue (VBitStreamReader &Packet);

public:
  VNetConnection (VSocketPublic *ANetCon, VNetContext *AContext, VBasePlayer *AOwner);
  virtual ~VNetConnection ();

  // VNetConnection interface
  void GetMessages ();
  virtual int GetRawPacket (TArray<vuint8> &);
  void ReceivedPacket (VBitStreamReader &);
  VChannel *CreateChannel (vuint8, vint32, vuint8 = true);
  virtual void SendMessage (VMessageOut &);

  inline int GetSendQueueSize () const noexcept { return sendQueueSize; }

  bool WillOverflow (VMessageOut &strm, int moredata);

  // if `resetUpdated` is true, reset "updated" flag for thinker channels
  void RemoveDeadThinkerChannels (bool resetUpdated=false);

  // call this if you want to send something to client regardless of its activity
  // note that this can (and prolly will) break sequencing!
  inline void ForceAllowSendForServer () noexcept { AllowMessageSend = true; }

  bool IsClient () noexcept;
  bool IsServer () noexcept;

  void Flush (bool asKeepalive=false);
  bool IsLocalConnection ();
  inline VStr GetAddress () const { return (NetCon ? NetCon->Address : VStr()); }
  void Tick ();
  void KeepaliveTick ();
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
  // used in `UpdateLevel()`
  void UpdateThinkers ();

  void PutOutToSendQueue ();

  // returns `true` if connection timeouted
  bool IsTimeoutExceeded ();

  void ShowTimeoutStats ();

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
  void KeepaliveTick ();
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

  TMapNC<VName, int> NewName2Idx;
  TMapNC<int, VName> NewIdx2Name;

  int NewNameFirstIndex;

public:
  VNetConnection *Connection;

public:
  VNetObjectsMap ();
  VNetObjectsMap (VNetConnection *);
  virtual ~VNetObjectsMap ();

  void SetupClassLookup ();
  bool CanSerialiseObject (VObject *);

  // called on name receiving
  void SetNumberOfKnownNames (int newlen);
  // called on reading
  void ReceivedName (int index, VName Name);

  // this is for initial class sending
  // out stream may be dropped, so we need to defer name internalising here
  bool SerialiseNameNoIntern (VStream &, VName &);
  void InternName (VName);

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
  virtual void SendMessage (VMessageOut &) override;

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
