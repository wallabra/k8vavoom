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


// ////////////////////////////////////////////////////////////////////////// //
struct VNetUtils {
  static void TVMsecs (timeval *dest, int msecs) noexcept;

  // start with 0, continuous
  static vuint32 CRC32C (vuint32 crc32, const void *buf, size_t length) noexcept;

  // // ChaCha20 // //
  /*
  struct ChaCha20Ctx {
    vuint32 input[16];
  };
  */
  typedef chacha20_ctx ChaCha20Ctx;

  enum {
    ChaCha20KeySize = 16,
    ChaCha20NonceSize = 8,
  };

  /* Key size in bits: either 256 (32 bytes), or 128 (16 bytes) */
  /* Nonce size in bits: 64 (8 bytes) */
  /* returns 0 on success */
  static int ChaCha20SetupEx (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata, vuint32 keybits) noexcept;

  /* chacha setup for 256-bit keys */
  static inline int ChaCha20Setup32 (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata) noexcept {
    return ChaCha20SetupEx(ctx, keydata, noncedata, 256);
  }

  /* chacha setup for 128-bit keys */
  static inline int ChaCha20Setup16 (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata) noexcept {
    return ChaCha20SetupEx(ctx, keydata, noncedata, 128);
  }

  /* chacha setup for 128-bit keys and 32-bit nonce */
  static inline int ChaCha20Setup (ChaCha20Ctx *ctx, const vuint8 keydata[16], const vuint32 nonce) noexcept {
    vuint8 noncebuf[8];
    memset(noncebuf, 0, sizeof(noncebuf));
    noncebuf[0] = nonce&0xffu;
    noncebuf[1] = (nonce>>8)&0xffu;
    noncebuf[2] = (nonce>>16)&0xffu;
    noncebuf[3] = (nonce>>24)&0xffu;
    noncebuf[5] = 0x02;
    noncebuf[6] = 0x9a;
    return ChaCha20SetupEx(ctx, keydata, noncebuf, 128);
  }

  /* encrypts or decrypts a full message */
  /* cypher is symmetric, so `ciphertextdata` and `plaintextdata` can point to the same address */
  static void ChaCha20XCrypt (ChaCha20Ctx *ctx, void *ciphertextdata, const void *plaintextdata, vuint32 msglen) noexcept;

  // generate ChaCha20 encryption key
  static void GenerateKey (vuint8 key[VNetUtils::ChaCha20KeySize]) noexcept;

  // WARNING! cannot do it in-place
  // needs 24 extra bytes (key, nonce, crc)
  // returns new length or -1 on error
  static int EncryptInfoPacket (void *destbuf, const void *srcbuf, int srclen, const vuint8 key[VNetUtils::ChaCha20KeySize]) noexcept;
  // it can decrypt in-place
  // returns new length or -1 on error
  // also sets key
  static int DecryptInfoPacket (vuint8 key[VNetUtils::ChaCha20KeySize], void *destbuf, const void *srcbuf, int srclen) noexcept;
};


class VNetContext;

// sent on handshake
enum {
  NET_PROTOCOL_VERSION_HI = 7,
  NET_PROTOCOL_VERSION_LO = 10,
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
  vuint8 AuthKey[VNetUtils::ChaCha20KeySize];

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
  vint32 DeathMatch; // dm mode
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

  double CurrNetTime; // cached time

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

  // call this to update current network time
  // used to avoid calls to `Sys_Time()` everywhere
  // should be called in connection ticker, in context ticker, and in `GetMessages()`
  void UpdateNetTime () noexcept;
  double GetNetTime () noexcept;

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
  int InListCount; // number of packets in InList
  int InListBits; // rough estimation, used to limit transfers
  int OutListCount; // number of packets in OutList
  int OutListBits; // rough estimation, used to limit transfers
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

  // this unconditionally adds "close" message to the queue, and marks the channel for closing
  // WARNING! DOES NO CHECKS!
  void SendCloseMessageForced ();

public:
  VChannel (VNetConnection *AConnection, EChannelType AType, vint32 AIndex, bool AOpenedLocally);
  virtual ~VChannel ();

  inline const char *GetTypeName () const noexcept { return GetChanTypeName(Type); }

  virtual VStr GetName () const noexcept;
  VStr GetDebugName () const noexcept;

  inline bool IsThinker () const noexcept { return (Type == CHANNEL_Thinker); }

  inline int GetSendQueueSize () const noexcept { return OutListCount; }
  inline int GetRecvQueueSize () const noexcept { return InListCount; }

  vuint32 GetLastOutSeq () const noexcept;

  // returns:
  //  -1: oversaturated
  //   0: ok
  //   1: full
  virtual int IsQueueFull () noexcept;

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
  // this handler SHOULD NOT DELETE THE CHANNEL!
  virtual void Tick ();

  // called by `ReceivedAcks()`, strictly in sequence
  virtual void OutMessageAcked (VMessageOut &Msg);

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

  bool CanSendData () noexcept;
  bool CanSendClose () noexcept;

  void SendRpc (VMethod *, VObject *);
  bool ReadRpc (VMessageIn &Msg, int, VObject *);

public: // this interface can be used to split data streams into separate messages
  // returns `true` if appending `addbits` will overflow the message
  bool WillOverflowMsg (const VMessageOut *msg, int addbits) const noexcept;

  // returns `true` if appending `strm` will overflow the message
  bool WillOverflowMsg (const VMessageOut *msg, const VBitStreamWriter &strm) const noexcept;

  // *moves* steam to msg (sending previous msg if necessary)
  // returns `true` if something was flushed
  bool PutStream (VMessageOut *msg, VBitStreamWriter &strm);

  // sends message if it is not empty, and clears it
  // returns `true` if something was flushed
  bool FlushMsg (VMessageOut *msg);
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
  VNetClientServerInfo csi;

  int StaticLightsNext;

  // build by `BuildUpdateSets()`
  TMapNC<vint32, bool> UpdatedLines;
  TMapNC<vint32, bool> UpdatedSides;

protected:
  // connection fat PVS must be built
  void BuildUpdateSets ();

  // updaters returns 1 if the stream should be sent, or -1 if we should abort updating
  // parsers returns `false` on any error

  int UpdateLine (VMessageOut &Msg, VBitStreamWriter &strm, int lidx);
  bool ParseLine (VMessageIn &Msg);

  int UpdateSide (VMessageOut &Msg, VBitStreamWriter &strm, int sidx);
  bool ParseSide (VMessageIn &Msg);
  bool ParseSideTexture (VMessageIn &Msg);
  bool ParseSideTOffset (VMessageIn &Msg);
  bool ParseSideROffset (VMessageIn &Msg);
  bool ParseSideScale (VMessageIn &Msg);

  int UpdateSector (VMessageOut &Msg, VBitStreamWriter &strm, int sidx);
  bool ParseSector (VMessageIn &Msg);
  bool ParseSectorTexture (VMessageIn &Msg);
  bool ParseSectorLight (VMessageIn &Msg);

  int UpdatePolyObj (VMessageOut &Msg, VBitStreamWriter &strm, int oidx);
  bool ParsePolyObj (VMessageIn &Msg);

  int UpdateCameraTexture (VMessageOut &Msg, VBitStreamWriter &strm, int idx);
  bool ParseCameraTexture (VMessageIn &Msg);

  int UpdateTranslation (VMessageOut &Msg, VBitStreamWriter &strm, int idx);
  bool ParseTranslation (VMessageIn &Msg);

  int UpdateBodyQueueTran (VMessageOut &Msg, VBitStreamWriter &strm, int idx);
  bool ParseBodyQueueTran (VMessageIn &Msg);

  int UpdateStaticLight (VMessageOut &Msg, VBitStreamWriter &strm, int idx, bool forced);
  bool ParseStaticLight (VMessageIn &Msg);

protected:
  enum {
    PhaseServerInfo,
    PhaseWaitingMapLoaded,
    PhaseStaticLights,
    PhasePrerender,
    PhaseDone,
  };

  int Phase;
  double MapLoadingStartTime;

protected:
  void SendServerInfo ();
  void WaitForMapLoaded ();
  void SendStaticLights ();
  void SendPreRender ();

public:
  VLevelChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VLevelChannel () override;

  void SetLevel (VLevel *);
  // returns `false` if there's nothing more to send
  bool SendLevelData ();
  void Update ();
  void ResetLevel ();

  // used by the client
  void SendMapLoaded ();

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
  // this is used by the thinker updater
  // if 1/3 of the channel is still full after this time, we'll block any updates until it is completely drained
  double DrainTime;
  bool bNeedToDrain;

public:
  // set by the client when it gets `Origin` update
  bool GotOrigin;
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
  // limit thinkers by the number of outgoing packets instead
  virtual int IsQueueFull () noexcept override;
};


// ////////////////////////////////////////////////////////////////////////// //
// a channel for updating player data
class VPlayerChannel : public VChannel {
public:
  VBasePlayer *Plr;
  vuint8 *OldData;
  bool NewObj;
  vuint8 *FieldCondValues;
  // some class references may be not sent at the start; repeat 'em while we can
  TMapNC<VField *, bool> FieldsToResend;
  // this is used in server, to limit client update rate
  double NextUpdateTime;
  vuint32 LastMOSUid;
  VField *GameTimeField;
  // set at the client side after we got `MO.Origin`
  // reset on player reset, or when MO disappears
  bool GotMOOrigin;

public:
  VPlayerChannel (VNetConnection *, vint32, vuint8 = true);
  virtual ~VPlayerChannel () override;

  void SetPlayer (VBasePlayer *);
  void EvalCondValues (VObject *, VClass *, vuint8 *);
  void Update ();

  // call this when new level is loaded
  void ResetLevel ();

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
  // names are compressed, because we usually have alot (around 400kb)
  vint32 unpDataSize;
  vint32 cprBufferSize;
  vint32 cprBufferPos;
  vuint8 *cprBuffer;
  // set when we complete sending initial data (known names and classes)
  // after we set this one: `CurrName` is next name to receive, or last acked name
  // `NextNameToSend` is for server only
  bool InitialDataDone;
  vint32 NextNameToSend;
  // for client
  vuint8 serverReplicationHash[SHA512_DIGEST_SIZE];

protected:
  void UpdateSendPBar ();
  void UpdateRecvPBar (bool forced);

  void ClearCprBuffer ();
  void CompressNames ();
  void DecompressNames ();

  // this fills
  void BuildNetFieldsHash (vuint8 hash[SHA512_DIGEST_SIZE]);

  // this sends new names
  void LiveUpdate ();

  void LiveParse (VMessageIn &Msg);

public:
  VObjectMapChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally);
  virtual ~VObjectMapChannel () override;

  void Update ();

  // VChannel interface
  virtual VStr GetName () const noexcept override;
  virtual void ReceivedCloseAck () override; // sets `ObjMapSent` flag
  virtual void ParseMessage (VMessageIn &) override;
  virtual void Tick () override;
  // slightly higher limits
  virtual int IsQueueFull () noexcept override;
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
  struct ThinkerSortInfo {
    VEntity *MO;
    TVec ViewOrg;
    TAVec ViewAngles;
    // everything that is behind this is behind our back
    TPlane ViewPlane;

    inline ThinkerSortInfo () noexcept {}
    ThinkerSortInfo (VBasePlayer *Owner) noexcept;
  };

public:
  enum {
    LNFO_UNSENT,
    LNFO_SENT_INPROGRESS, // we're still have something to send
    LNFO_SENT_COMPLETE, // we're sending static lights
  };

public:
  VNetworkPublic *Driver;
  VNetContext *Context;
  VBasePlayer *Owner;
  ENetConState State;
  vuint8 AuthKey[VNetUtils::ChaCha20KeySize];
  bool NeedsUpdate; // should we call `VNetConnection::UpdateLevel()`? this updates mobjs in sight, and sends new mobj state
  bool AutoAck; // `true` for demos: autoack all pacekts
  double LastLevelUpdateTime;
  double LastThinkersUpdateTime;
  vuint32 UpdateFrameCounter; // monotonically increasing
  vuint32 UpdateFingerUId; // see `UpdateThinkers()` for the explanation

  VNetObjectsMap *ObjMap;
  bool ObjMapSent;
  int LevelInfoSent; // see LNFO_*
  // when we detach a thinker, there's no need to send any updates for it anymore
  // we cannot have this flag in thinker itself, because new
  // clients should still get detached thinkers once
  TMapNC<VThinker *, bool> DetachedThinkers;
  TMapNC<VThinker *, bool> SimulatedThinkers;

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
  double LagAcc; // average lag
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
  vuint32 OutLastWrittenAck; // starts with 0

  // channel table
  VChannel *Channels[MAX_CHANNELS];
  vuint32 OutReliable[MAX_CHANNELS];
  vuint32 InReliable[MAX_CHANNELS];
  TArray<vuint32> QueuedAcks;
  TArray<vuint32> AcksToResend;
  TArray<VChannel *> OpenChannels;
  TMapNC<VThinker *, VThinkerChannel *> ThinkerChannels;

  VField *OriginField;
  VField *DataGameTimeField;

private:
  // subsectors
  //vuint8 *UpdatePvs;
  //int UpdatePvsSize;

public:
  // hash tables, to speed up various updates
  // built with fat PVS
  TMapNC<vint32, bool> UpdatedSubsectors;
  TMapNC<vint32, bool> UpdatedSectors;

private:
  // `LeafPvs` was meant to point to glVIS info, but it is currently disabled
  const vuint8 *LeafPvs;
  VViewClipper Clipper;
  // this is used in `VNetConnection::UpdateLevel()`
  // temporary buffers, only valid in that method.
  TArray<VThinker *> PendingThinkers;
  TArray<VEntity *> PendingGoreEnts;
  TArray<vint32> AliveGoreChans;
  TArray<VThinker *> AliveThinkerChans;

  void CollectAndSortAliveThinkerChans (ThinkerSortInfo *snfo);

public:
  // current estimated message byte size
  // used to check if we can add given number of bits without flushing
  inline int CalcEstimatedByteSize (int addBits=0) const noexcept {
    const int endsize = (Out.GetNumBits() ? 0 : MAX_PACKET_HEADER_BITS)+Out.GetNumBits()+addBits+1;
    return (endsize+7)>>3;
  }

  int GetNetSpeed () const noexcept;

  // used in server context; if `false`, server context will call `SendServerInfo()`
  inline bool IsLevelInfoSendingComplete () const noexcept { return (LevelInfoSent == LNFO_SENT_COMPLETE); }

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

  inline VStr GetAddress () const { return (NetCon ? NetCon->Address : VStr()); }

  inline bool IsOpen () const noexcept { return (State > NETCON_Closed); }
  inline bool IsClosed () const noexcept { return (State <= NETCON_Closed); }

  bool IsClient () noexcept;
  bool IsServer () noexcept;
  bool IsLocalConnection () const noexcept;

  // this marks the connection as closed, but doesn't destroy anything
  void Close ();

  // call this to process incoming messages
  // if `asHearbeat` is `true`, update timers, but drop all received messages
  void GetMessages (bool asHearbeat=false);

  virtual int GetRawPacket (void *dest, size_t destSize); // used in demos

  // read and process one incoming message
  // returns `false` if no message was processed
  // if `asHearbeat` is `true`, update timers, but drop all received messages
  bool GetMessage (bool asHearbeat);

  // this is called when incoming message was read; it should decode and process network packet
  void ReceivedPacket (VBitStreamReader &Packet);

  // this is called by channel send/recv methods on fatal queue overflow
  // you *CANNOT* `delete` channel here, as it is *NOT* guaranteed that call to this
  // method is followed by `return`!
  // you CAN call `chan->Close()` here
  virtual void AbortChannel (VChannel *chan);

  // sets `PacketId` field in the message, you can use it
  virtual void SendMessage (VMessageOut *Msg);

  virtual void SendPacketAck (vuint32 PacketId);

  virtual void Flush ();
  virtual void Tick ();
  // if we're doing only heartbeats, we will silently drop all incoming datagrams
  // this is requred so the connection won't be timeouted on map i/o, for example
  virtual void KeepaliveTick ();

  // this marks connection as "saturated"
  virtual void Saturate () noexcept;

  virtual bool CanSendData () const noexcept;

  // returns argument for `Prepare` if putting the ack will overflow the output buffer.
  // i.e. if it returned non-zero, ack is not put.
  // if `forceSend` is `true`, flush the output buffer if necessary (always sends, returns 0).
  int PutOneAck (vuint32 ackId, bool forceSend=false);
  // for convenience
  inline void PutOneAckForced (vuint32 ackId) { const int res = PutOneAck(ackId, true); vassert(res == 0); }
  // resend all acks from `AcksToResend`, and clears `AcksToResend`
  void ResendAcks ();

  // call this to make sure that the output buffer has enough room
  void Prepare (int addBits);

  // this is called to notify all channels that some messages are lost
  // normal logic simply retransmit the messages with the given packet id
  void PacketLost (vuint32 PacketId);

  // send console command
  void SendCommand (VStr Str);

  // fat PVS should be set up
  bool IsRelevant (VThinker *th);

  // inventory is always relevant too
  // doesn't check PVS
  // call after `IsRelevant()` returned `true`, because this does much less checks
  bool IsAlwaysRelevant (VThinker *th);

  // this is called by server code to send updates to clients
  void UpdateLevel ();

  void SendServerInfo ();
  void LoadedNewLevel ();
  void ResetLevel ();

  bool SecCheckFatPVS (const sector_t *Sec);
  bool CheckFatPVS (const subsector_t *Subsector);

  // used by client to lower rendering speed
  // this is hack for my GPU
  bool IsDangerousTimeout ();

protected:
  void SetupFatPVS ();
  void SetupPvsNode (int, float *);
  void PvsMarkExtra (sector_t *sec);
  void PvsAddSector (sector_t *sec);

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
  VField *OwnerField;
  VField *TargetField;
  VField *TracerField;
  VField *MasterField;
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

public:
  VNetConnection *Connection;

public:
  VNetObjectsMap ();
  VNetObjectsMap (VNetConnection *AConnection);
  virtual ~VNetObjectsMap ();

  void SetupClassLookup ();
  bool CanSerialiseObject (VObject *Obj);

  // called on name receiving
  void SetNumberOfKnownNames (int newlen);
  // called on reading
  void ReceivedName (int index, VName Name);

  // this is for initial class sending
  // out stream may be dropped, so we need to defer name internalising here
  bool SerialiseNameNoIntern (VStream &Strm, VName &Name);

  void AckNameWithIndex (int index);

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

public:
  VDemoPlaybackNetConnection (VNetContext *, VBasePlayer *, VStream *, bool);
  virtual ~VDemoPlaybackNetConnection () override;

  // VNetConnection interface
  virtual int GetRawPacket (void *dest, size_t destSize) override;
  virtual void SendMessage (VMessageOut *Msg) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingNetConnection : public VNetConnection {
public:
  VDemoRecordingNetConnection (VSocketPublic *, VNetContext *, VBasePlayer *);

  // VNetConnection interface
  virtual int GetRawPacket (void *dest, size_t destSize) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDemoRecordingSocket : public VSocketPublic {
public:
  VDemoRecordingSocket ();
  virtual bool IsLocalConnection () const noexcept override;
  virtual int GetMessage (void *dest, size_t destSize) override;
  virtual int SendMessage (const vuint8 *, vuint32) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// global access to the low-level networking services
extern VNetworkPublic *GNet;
extern VNetContext *GDemoRecordingContext;


#endif
