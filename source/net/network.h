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

extern VCvarF net_timeout;
extern VCvarF net_keepalive;


class VNetContext;

// packet header IDs
// used in handshake only
enum {
  NETPACKET_CTL = 0x80,
};

// sent on handshake, and with `CMD_NewLevel`
// I don't think that communication protocol will change, but just in a case
// k8: and it did
enum {
  NET_PROTOCOL_VERSION = 5,
};

enum {
  MAX_CHANNELS = 1024,
  MAX_RELIABLE_BUFFER = 128,
};

enum EChannelType {
  CHANNEL_Control = 1,
  CHANNEL_Level,
  CHANNEL_Player,
  CHANNEL_Thinker,
  CHANNEL_ObjectMap,

  CHANNEL_MAX,
};

//FIXME!
//TODO!
// separate messages from packets, so we can fragment and reassemble one message with several packets
enum {
  MAX_DGRAM_SIZE = 1400, // max length of a datagram; fuck off, ipshit6

  MAX_PACKET_HEADER_BITS = 32, // packet id

  // including message header
  MAX_MSG_SIZE_BITS = MAX_DGRAM_SIZE*8-MAX_PACKET_HEADER_BITS,
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

  virtual bool IsLocalConnection () const noexcept = 0;
  // dest should be at least `MAX_DGRAM_SIZE+4` (just in case)
  // returns number of bytes received, 0 for "no message", -1 for error
  // if the message is too big for a buffer, return -1
  virtual int GetMessage (void *dest, size_t destSize) = 0;
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
  TArray<VStr> WadFiles;
  vint32 Users;
  vint32 MaxUsers;
  enum {
    Flag_GoodProtocol = 1u<<0,
    Flag_GoodWadList  = 1u<<1,
  };
  vuint32 Flags;
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
  // statistic counters
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
  VNetworkPublic ();

  inline bool CheckForUserAbort () { return (CheckUserAbortCB ? CheckUserAbortCB(CheckUserAbortUData) : false); }

  virtual void Init () = 0;
  virtual void Shutdown () = 0;
  virtual VSocketPublic *Connect (const char *InHost) = 0;
  virtual VSocketPublic *CheckNewConnections () = 0;
  virtual void Poll () = 0;
  virtual void StartSearch (bool) = 0;
  virtual slist_t *GetSlist () = 0;
  virtual void UpdateMaster () = 0;
  virtual void QuitMaster () = 0;

  static double GetNetTime () noexcept { return Sys_Time(); }

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
  bool OpenedLocally; // `true` if opened locally, `false` if opened by the remote
  bool OpenAcked; // is open acked? (obviously)
  bool Closing; // channel sent "close me" message, and is waiting for ack
  int NumInList; // number of packets in InList
  int NumOutList; // number of packets in OutList
  VMessageIn *InList; // incoming data with queued dependencies
  VMessageOut *OutList; // outgoing reliable unacked data
  bool bSentAnyMessages; // if this is `false`, force `bOpen` flag on the sending message (and set this to `true`)

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

public:
  VChannel (VNetConnection *AConnection, EChannelType AType, vint32 AIndex, bool AOpenedLocally);
  virtual ~VChannel ();

  inline const char *GetTypeName () const noexcept { return GetChanTypeName(Type); }

  virtual VStr GetName () const noexcept;
  VStr GetDebugName () const noexcept;

  inline bool IsThinker () const noexcept { return (Type == CHANNEL_Thinker); }

  inline int GetSendQueueSize () const noexcept { return NumOutList; }
  inline int GetRecvQueueSize () const noexcept { return NumInList; }

  // is this channel opened locally?
  inline bool IsLocalChannel () const noexcept { return OpenedLocally; }

  // this is called to set `Closing` flag, and can perform some cleanup
  virtual void SetClosing ();

  // this is called from `ReceivedAcks()`
  // the channel will be automatically closed and destroyed, so don't do it here
  virtual void ReceivedCloseAck ();

  // this sends reliable "close" message, if the channel in not on closing state yet
  virtual void Close ();

  // called by `ReceivedMessage()` to parse new message
  // this call is sequenced (i.e. the sequence is right)
  virtual void ParseMessage (VMessageIn &Msg) = 0;

  // this is called to notify the channel that some messages are lost
  // normal logic simply retransmit the messages with the given packet id
  virtual void PacketLost (vuint32 PacketId);

  // call this periodically to perform various processing
  virtual void Tick ();

  // WARNING! this method can call `delete this`!
  // this is called from connection when this channel got some acks
  void ReceivedAcks ();

  // returns `true` if this channel killed itself
  // this is called to process messages in sequence (from `ReceivedMessage()`)
  bool ProcessInMessage (VMessageIn &Msg);

  // this is called when new message for this channel is received
  void ReceivedMessage (VMessageIn &Msg);

  // sets `PacketId` field in the message, you can use it
  void SendMessage (VMessageOut *Msg);

  bool CanSendData () const noexcept;
  bool CanSendClose () const noexcept;

  void SendRpc (VMethod *, VObject *);
  bool ReadRpc (VMessageIn &Msg, int, VObject *);

public: // this interface can be used to split data streams into separate messages
  // returns `true` if appending `strm` will overflow the message
  bool WillOverflowMsg (const VMessageOut *msg, const VBitStreamWriter &strm) const noexcept;

  // *moves* steam to msg (sending previous msg if necessary)
  void PutStream (VMessageOut *msg, VBitStreamWriter &strm);

  // sends message if it is not empty, and clears it
  void FlushMsg (VMessageOut *msg);
};


// ////////////////////////////////////////////////////////////////////////// //
class VControlChannel : public VChannel {
public:
  VControlChannel (VNetConnection *, vint32, vuint8 = true);

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ParseMessage (VMessageIn &) override;
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
  VNetClientServerInfo csi;

public:
  VLevelChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VLevelChannel () override;

  void SetLevel (VLevel *);
  void Update ();
  void SendNewLevel ();
  void SendStaticLights ();
  void ResetLevel ();

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ParseMessage (VMessageIn &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating thinkers
class VThinkerChannel : public VChannel {
protected:
  VThinker *Thinker;
  vuint8 *OldData; // old field data, for creating deltas
  bool NewObj; // is this a new object?
  vuint8 *FieldCondValues;

public:
  vuint32 LastUpdateFrame; // see `UpdateFrameCounter` in VNetConnection

public:
  VThinkerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally=true);
  virtual ~VThinkerChannel () override;

  inline VThinker *GetThinker () const noexcept { return Thinker; }

  void SetThinker (VThinker *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();
  void RemoveThinkerFromGame ();

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ParseMessage (VMessageIn &) override;
  virtual void SetClosing () override;
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

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void SetClosing () override;
  virtual void ParseMessage (VMessageIn &) override;
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

  void Update ();

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ReceivedCloseAck () override; // sets `ObjMapSent` flag
  virtual void ParseMessage (VMessageIn &) override;
  virtual void Tick () override;
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

  static VCvarI net_speed_limit;

public:
  VNetworkPublic *Driver;
  VNetContext *Context;
  VBasePlayer *Owner;
  ENetConState State;
  bool NeedsUpdate; // should we call `VNetConnection::UpdateLevel()`? this updates mobjs in sight, and sends new mobj state
  bool AutoAck; // `true` for demos: autoack all pacekts
  double LastLevelUpdateTime;
  vuint32 UpdateFrameCounter; // monotonically increasing

  VNetObjectsMap *ObjMap;
  bool ObjMapSent;
  bool LevelInfoSent;
  // when we detach a thinker, there's no need to send any updates for it anymore
  // we cannot have this flag in thinker itself, because new
  // clients should still get detached thinkers once
  TMapNC<VThinker *, bool> DetachedThinkers;

  // timings, etc.
  double LastReceiveTime; // last time a packet was received, for timeout checking
  double LastSendTime; // last time a packet was sent, for keepalives
  double LastTickTime; // last time when `Tick()` was called (used to estimate bandwidth)
  int SaturaDepth; // estimation: how much bytes the connection can queue before saturation? (positive means "saturated")
  bool ForceFlush; // should we force network flush in the next tick?

  // statistics
  double LastStatsUpdateTime; // time of last stat update
  double InRate, OutRate; // rate for last interval
  double InPackets, OutPackets; // packet counts
  double InMessages, OutMessages; // message counts
  double InLoss, OutLoss; // packet loss percent
  double InOrder, OutOrder; // out of order incoming packets
  double AvgLag, PrevLag; // average lag, average lag from the previous update

  // statistics accumulators
  double LagAcc, PrevLagAcc; // average lag
  int InLossAcc, OutLossAcc; // packet loss accumulator
  int InPktAcc, OutPktAcc; // packet accumulator
  int InMsgAcc, OutMsgAcc; // message accumulator
  int InByteAcc, OutByteAcc; // byte accumulator
  int InOrdAcc; // out of order accumulator
  int LagCount; // counter for lag measurement
  double LastFrameStartTime, FrameDeltaTime; // monitors frame time
  double CumulativeTime, AverageFrameTime;
  int StatsFrameCounter;

  // current packet
  VBitStreamWriter Out; // outgoing packet
  double OutLagTime[256]; // for lag measuring
  vuint32 OutLagPacketId[256]; // for lag measuring
  vuint32 InPacketId; // full incoming packet index
  vuint32 OutPacketId; // most recently sent packet
  vuint32 OutAckPacketId; // most recently acked outgoing packet
  vuint32 LastInPacketIdAck; // if not 0xffffffffu, this is last acked received packet

  // channel table
  VChannel *Channels[MAX_CHANNELS];
  vuint32 OutReliable[MAX_CHANNELS];
  vuint32 InReliable[MAX_CHANNELS];
  TArray<vuint32> QueuedAcks;
  TArray<vuint32> AcksToResend;
  TArray<VChannel *> OpenChannels;
  TMapNC<VThinker *, VThinkerChannel *> ThinkerChannels;

private:
  vuint8 *UpdatePvs;
  int UpdatePvsSize;
  const vuint8 *LeafPvs;
  VViewClipper Clipper;
  // this is used in `VNetConnection::UpdateLevel()`
  // temporary buffers, only valid in that method.
  TArray<VThinker *> PendingThinkers;
  TArray<VEntity *> PendingGoreEnts;
  TArray<vint32> AliveGoreChans;

public:
  // current estimated message byte size
  // used to check if we can add given number of bits without flushing
  inline int CalcEstimatedByteSize (int addBits=0) const noexcept {
    const int endsize = (Out.GetNumBits() ? 0 : MAX_PACKET_HEADER_BITS)+Out.GetNumBits()+addBits+1;
    return (endsize+7)>>3;
  }

  inline int GetNetSpeed () const noexcept {
    if (AutoAck) return 100000000;
    if (IsLocalConnection()) return 100000000;
    return max2(2400, net_speed_limit.asInt());
  }

protected:
  // WARNING! this can change channel list!
  void AckEverythingEverywhere ();

public:
  VNetConnection (VSocketPublic *ANetCon, VNetContext *AContext, VBasePlayer *AOwner);
  virtual ~VNetConnection ();

  VChannel *CreateChannel (vuint8 Type, vint32 AIndex, vuint8 OpenedLocally/*=true*/);

  inline VChannel *GetGeneralChannel () noexcept { return Channels[CHANIDX_General]; }
  inline VPlayerChannel *GetPlayerChannel () noexcept { return (VPlayerChannel *)(Channels[CHANIDX_Player]); }
  inline VLevelChannel *GetLevelChannel () noexcept { return (VLevelChannel *)(Channels[CHANIDX_Level]); }

  inline VChannel *GetChannelByIndex (int idx) noexcept { return (idx >= 0 && idx < MAX_CHANNELS ? Channels[idx] : nullptr); }

  inline bool IsOpen () const noexcept { return (State > NETCON_Closed); }
  inline bool IsClosed () const noexcept { return (State <= NETCON_Closed); }

  bool IsClient () noexcept;
  bool IsServer () noexcept;
  bool IsLocalConnection () const noexcept;

  inline VStr GetAddress () const { return (NetCon ? NetCon->Address : VStr()); }

  // call this to process incoming messages
  void GetMessages ();

  virtual int GetRawPacket (void *dest, size_t destSize); // used in demos

  // read and process one incoming message
  // returns `false` if no message was processed
  bool GetMessage ();

  // this is called when incoming message was read; it should decode and process network packet
  void ReceivedPacket (VBitStreamReader &Packet);

  // sets `PacketId` field in the message, you can use it
  virtual void SendMessage (VMessageOut *Msg);

  virtual void SendPacketAck (vuint32 PacketId);

  virtual void Flush ();
  virtual void Tick (/*bool onlyHeartbeat=false*/);
  // if we're doing only heartbeats, we will silently drop all incoming datagrams
  // this is requred so the connection won't be timeouted on map i/o, for example
  //TODO
  virtual void KeepaliveTick ();

  // this marks connection as "saturated"
  virtual void Saturate () noexcept;

  virtual bool CanSendData () const noexcept;

  // resend all acks from `AcksToResend`
  void ResendAcks (bool allowOutOverflow=true);
  // clear `AcksToResend`
  void ForgetResendAcks ();

  // call this to make sure that the output buffer has enough room
  void Prepare (int addBits);

  // this is called to notify all channels that some messages are lost
  // normal logic simply retransmit the messages with the given packet id
  void PacketLost (vuint32 PacketId);

  // send console command
  void SendCommand (VStr Str);

  // fat PVS should be set up
  bool IsRelevant (VThinker *Th);

  // this is called by server code to send updates to clients
  void UpdateLevel ();

  void SendServerInfo ();
  void LoadedNewLevel ();
  void ResetLevel ();

  // for demo playback
  virtual void Intermission (bool active);

  bool SecCheckFatPVS (sector_t *);
  int CheckFatPVS (subsector_t *);

protected:
  void SetupFatPVS ();
  void SetupPvsNode (int, float *);

protected:
  // used in `UpdateLevel()`
  void UpdateThinkers ();

  // returns `true` if connection timeouted
  bool IsTimeoutExceeded ();
  bool IsKeepAliveExceeded ();

  void ShowTimeoutStats ();
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
  virtual int GetRawPacket (void *dest, size_t destSize) override;
  virtual void SendMessage (VMessageOut *Msg) override;

  virtual void Intermission (bool active) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingNetConnection : public VNetConnection {
public:
  bool inIntermission;

public:
  VDemoRecordingNetConnection (VSocketPublic *, VNetContext *, VBasePlayer *);

  // VNetConnection interface
  virtual int GetRawPacket (void *dest, size_t destSize) override;

  virtual void Intermission (bool active) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingSocket : public VSocketPublic {
public:
  virtual bool IsLocalConnection () const noexcept override;
  virtual int GetMessage (void *dest, size_t destSize) override;
  virtual int SendMessage (const vuint8 *, vuint32) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// global access to the low-level networking services
extern VNetworkPublic *GNet;
extern VNetContext *GDemoRecordingContext;


#endif
