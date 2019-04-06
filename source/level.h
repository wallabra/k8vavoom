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

#define MAXPLAYERS  (8)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger, but we do not have any moving sectors nearby
#define MAXRADIUS  (32.0f)

// mapblocks are used to check movement against lines and things
#define MapBlock(x)  ((int)floor(x)>>7)

class VDecalDef;


//==========================================================================
//
//  Structures for level network replication
//
//==========================================================================
struct rep_line_t {
  float alpha;
};

struct rep_side_t {
  float TopTextureOffset;
  float BotTextureOffset;
  float MidTextureOffset;
  float TopRowOffset;
  float BotRowOffset;
  float MidRowOffset;
  int TopTexture;
  int BottomTexture;
  int MidTexture;
  vuint32 Flags;
  int Light;
};


struct rep_sector_t {
  int floor_pic;
  float floor_dist;
  float floor_xoffs;
  float floor_yoffs;
  float floor_XScale;
  float floor_YScale;
  float floor_Angle;
  float floor_BaseAngle;
  float floor_BaseYOffs;
  VEntity *floor_SkyBox;
  float floor_MirrorAlpha;
  int ceil_pic;
  float ceil_dist;
  float ceil_xoffs;
  float ceil_yoffs;
  float ceil_XScale;
  float ceil_YScale;
  float ceil_Angle;
  float ceil_BaseAngle;
  float ceil_BaseYOffs;
  VEntity *ceil_SkyBox;
  float ceil_MirrorAlpha;
  int lightlevel;
  int Fade;
  int Sky;
};


struct rep_polyobj_t {
  TVec startSpot;
  float angle;
};


struct rep_light_t {
  TVec Origin;
  float Radius;
  vuint32 Colour;
  VEntity *Owner;
};


struct VSndSeqInfo {
  VName Name;
  vint32 OriginId;
  TVec Origin;
  vint32 ModeNum;
  TArray<VName> Choices;
};


struct VCameraTextureInfo {
  VEntity *Camera;
  int TexNum;
  int FOV;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLevel;
class VLevelInfo;

class VLevelScriptThinker : public VSerialisable {
public:
  bool destroyed; // script `Destroy()` method should set this (and check to avoid double destroying)
  VLevel *XLevel; // level object
  VLevelInfo *Level; // level info object

public:
  VLevelScriptThinker () : destroyed(false), XLevel(nullptr), Level(nullptr) {}
  virtual ~VLevelScriptThinker ();

  inline void DestroyThinker () { Destroy(); }

  // it is guaranteed that `Destroy()` will be called by master before deleting the object
  virtual void Destroy () = 0;
  virtual void ClearReferences () = 0;
  virtual void Tick (float DeltaTime) = 0;

  virtual VName GetName () = 0;
  virtual int GetNumber () = 0;

  virtual VStr DebugDumpToString () = 0;
};

extern VLevelScriptThinker *AcsCreateEmptyThinker ();
//FIXME
extern void AcsSuspendScript (VAcsLevel *acslevel, int number, int map);
extern void AcsTerminateScript (VAcsLevel *acslevel, int number, int map);


//==========================================================================
//
//                  LEVEL
//
//==========================================================================
enum {
  MAX_LEVEL_TRANSLATIONS      = 0xffff,
  MAX_BODY_QUEUE_TRANSLATIONS = 0xff,
};


class VLevel : public VGameObject {
  DECLARE_CLASS(VLevel, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VLevel)

  friend class VUdmfParser;

  struct SectorLink {
    vint32 index;
    vint32 mts; // bit 30: set if ceiling; low byte: movetype
    vint32 next; // index, or -1
  };

  VName MapName;
  VStr MapHash;
  VStr MapHashMD5;
  vuint32 LSSHash; // xxHash32 of linedefs, sidedefs, sectors (in this order)
  vuint32 SegHash; // xxHash32 of segs

  // flags
  enum {
    LF_ForServer = 0x01, // true if this level belongs to the server
    LF_Extended  = 0x02, // true if level was in Hexen format
    LF_GLNodesV5 = 0x04, // true if using version 5 GL nodes
    LF_TextMap   = 0x08, // UDMF format map
    // used in map fixer
    LF_ForceRebuildNodes                = 0x0010,
    LF_ForceAllowSeveralPObjInSubsector = 0x0020,
    LF_ForceNoTexturePrecache           = 0x0040,
    LF_ForceNoPrecalcStaticLights       = 0x0080,
    LF_ForceNoDeepwaterFix              = 0x0100,
    LF_ForceNoFloorFloodfillFix         = 0x0200,
    LF_ForceNoCeilingFloodfillFix       = 0x0400,
  };
  vuint32 LevelFlags;

  // MAP related Lookup tables
  // store VERTEXES, LINEDEFS, SIDEDEFS, etc.

  vertex_t *Vertexes;
  vint32 NumVertexes;

  sector_t *Sectors;
  vint32 NumSectors;

  side_t *Sides;
  vint32 NumSides;

  line_t *Lines;
  vint32 NumLines;

  seg_t *Segs;
  vint32 NumSegs;

  subsector_t *Subsectors;
  vint32 NumSubsectors;

  node_t *Nodes;
  vint32 NumNodes;

  vuint8 *VisData;
  vuint8 *NoVis;

  // !!! Used only during level loading
  mthing_t *Things;
  vint32 NumThings;

  // BLOCKMAP
  // created from axis aligned bounding box of the map, a rectangular array of blocks of size ...
  // used to speed up collision detection by spatial subdivision in 2D
  vint32 BlockMapLumpSize;
  vint32 *BlockMapLump; // offsets in blockmap are from here
  vint32 *BlockMap;   // int for larger maps
  vint32 BlockMapWidth;  // Blockmap size.
  vint32 BlockMapHeight; // size in mapblocks
  float BlockMapOrgX; // origin of block map
  float BlockMapOrgY;
  VEntity **BlockLinks;   // for thing chains
  polyblock_t **PolyBlockMap;

  // REJECT
  // for fast sight rejection
  // speeds up enemy AI by skipping detailed LineOf Sight calculation
  // without special effect, this could be used as a PVS lookup as well
  vuint8 *RejectMatrix;
  vint32 RejectMatrixSize;

  // strife conversations
  FRogueConSpeech *GenericSpeeches;
  vint32 NumGenericSpeeches;

  FRogueConSpeech *LevelSpeeches;
  vint32 NumLevelSpeeches;

  // list of all poly-objects on the level
  polyobj_t *PolyObjs;
  vint32 NumPolyObjs;

  // anchor points of poly-objects
  PolyAnchorPoint_t *PolyAnchorPoints;
  vint32 NumPolyAnchorPoints;

  // sound environments for sector zones
  vint32 *Zones;
  vint32 NumZones;

  VThinker *ThinkerHead;
  VThinker *ThinkerTail;

  VLevelInfo *LevelInfo;
  VWorldInfo *WorldInfo;

  VAcsLevel *Acs;

  VRenderLevelPublic *RenderData;
  VNetContext *NetContext;

  rep_line_t *BaseLines;
  rep_side_t *BaseSides;
  rep_sector_t *BaseSectors;
  rep_polyobj_t *BasePolyObjs;

  rep_light_t *StaticLights;
  vint32 NumStaticLights;

  TArray<VSndSeqInfo> ActiveSequences;
  TArray<VCameraTextureInfo> CameraTextures;

  float Time; // game time, in seconds
  vint32 TicTime; // game time, in tics (35 per second)

  msecnode_t *SectorList;
  // phares 3/21/98
  // Maintain a freelist of msecnode_t's to reduce memory allocs and frees
  msecnode_t *HeadSecNode;

  // Translations controlled by ACS scripts
  TArray<VTextureTranslation *> Translations;
  TArray<VTextureTranslation *> BodyQueueTrans;

  VState *CallingState;
  VStateCall *StateCall;

  vint32 NextSoundOriginID;

  decal_t *decanimlist;
  vint32 decanimuid;

  TArray<vint32> sectorlinkStart;
  TArray<SectorLink> sectorlinks;

  TArray<VLevelScriptThinker *> scriptThinkers;

  vuint32 csTouchCount;
  // for ChangeSector; Z_Malloc'ed, NumSectors elements
  // bit 31 is return value from `ChangeSectorInternal()`
  // if other bits are equal to csTouchCount
  vuint32 *csTouched;

  // sectors with fake floors/ceilings, so world updater can skip iterating over all of them
  TArray<vint32> FakeFCSectors;
  TArray<vint32> TaggedSectors;

private:
  bool ChangeSectorInternal (sector_t *sector, int crunch);

  void xxHashLinedef (XXH32_state_t *xctx, const line_t &line) const {
    if (!xctx) return;
    /* ignore vertices and flags
    vuint32 v0idx = (vuint32)(ptrdiff_t)(line.v1-Vertexes);
    XXH32_update(xctx, &v0idx, sizeof(v0idx));
    vuint32 v1idx = (vuint32)(ptrdiff_t)(line.v2-Vertexes);
    XXH32_update(xctx, &v1idx, sizeof(v1idx));
    XXH32_update(xctx, &line.flags, sizeof(line.flags));
    XXH32_update(xctx, &line.SpacFlags, sizeof(line.SpacFlags));
    */
    XXH32_update(xctx, &line.sidenum[0], sizeof(line.sidenum[0]));
    XXH32_update(xctx, &line.sidenum[1], sizeof(line.sidenum[1]));
    /* ignore bbox and slope
    for (int f = 0; f < 4; ++f) XXH32_update(xctx, &line.bbox[f], sizeof(line.bbox[f]));
    XXH32_update(xctx, &line.slopetype, sizeof(line.slopetype));
    */
    vuint32 fsnum = (line.frontsector ? (vuint32)(ptrdiff_t)(line.frontsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &fsnum, sizeof(fsnum));
    vuint32 bsnum = (line.backsector ? (vuint32)(ptrdiff_t)(line.backsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &bsnum, sizeof(bsnum));
    //hash something else?
  }

  void xxHashSidedef (XXH32_state_t *xctx, const side_t &side) const {
    if (!xctx) return;
    /* ignore textures
    XXH32_update(xctx, &side.TopTextureOffset, sizeof(side.TopTextureOffset));
    XXH32_update(xctx, &side.BotTextureOffset, sizeof(side.BotTextureOffset));
    XXH32_update(xctx, &side.MidTextureOffset, sizeof(side.MidTextureOffset));

    XXH32_update(xctx, &side.TopRowOffset, sizeof(side.TopRowOffset));
    XXH32_update(xctx, &side.BotRowOffset, sizeof(side.BotRowOffset));
    XXH32_update(xctx, &side.MidRowOffset, sizeof(side.MidRowOffset));

    XXH32_update(xctx, &side.TopTexture.id, sizeof(side.TopTexture.id));
    XXH32_update(xctx, &side.BottomTexture.id, sizeof(side.BottomTexture.id));
    XXH32_update(xctx, &side.MidTexture.id, sizeof(side.MidTexture.id));
    */
    vuint32 secnum = (side.Sector ? (vuint32)(ptrdiff_t)(side.Sector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &secnum, sizeof(secnum));
    XXH32_update(xctx, &side.LineNum, sizeof(side.LineNum));
    //!XXH32_update(xctx, &side.Flags, sizeof(side.Flags));
    //hash something else?
  }

  void xxHashPlane (XXH32_state_t *xctx, const TPlane &pl) const {
    if (!xctx) return;
    XXH32_update(xctx, &pl.normal.x, sizeof(pl.normal.x));
    XXH32_update(xctx, &pl.normal.y, sizeof(pl.normal.y));
    XXH32_update(xctx, &pl.normal.z, sizeof(pl.normal.z));
    XXH32_update(xctx, &pl.dist, sizeof(pl.dist));
  }

  void xxHashSecPlane (XXH32_state_t *xctx, const sec_plane_t &spl) const {
    if (!xctx) return;
    xxHashPlane(xctx, spl);
    XXH32_update(xctx, &spl.minz, sizeof(spl.minz));
    XXH32_update(xctx, &spl.maxz, sizeof(spl.maxz));
    //!XXH32_update(xctx, &spl.TexZ, sizeof(spl.TexZ));
    //!XXH32_update(xctx, &spl.pic.id, sizeof(spl.pic.id));
    //XXH32_update(xctx, &spl.xoffs, sizeof(spl.xoffs));
    //XXH32_update(xctx, &spl.yoffs, sizeof(spl.yoffs));
    //XXH32_update(xctx, &spl.XScale, sizeof(spl.XScale));
    //XXH32_update(xctx, &spl.YScale, sizeof(spl.YScale));
    //!XXH32_update(xctx, &spl.Angle, sizeof(spl.Angle));
    //!XXH32_update(xctx, &spl.BaseAngle, sizeof(spl.BaseAngle));
    //!XXH32_update(xctx, &spl.BaseYOffs, sizeof(spl.BaseYOffs));
    //!XXH32_update(xctx, &spl.flags, sizeof(spl.flags));
    //XXH32_update(xctx, &spl.Alpha, sizeof(spl.Alpha));
    //XXH32_update(xctx, &spl.MirrorAlpha, sizeof(spl.MirrorAlpha));
    //XXH32_update(xctx, &spl.LightSourceSector, sizeof(spl.LightSourceSector));
  }

  void xxHashSectordef (XXH32_state_t *xctx, const sector_t &sec) const {
    if (!xctx) return;
    /* ignore planes
    xxHashSecPlane(xctx, sec.floor);
    xxHashSecPlane(xctx, sec.ceiling);
    vuint32 drnum = (sec.deepref ? (vuint32)(ptrdiff_t)(sec.deepref-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &drnum, sizeof(drnum));
    XXH32_update(xctx, &sec.skyheight, sizeof(sec.skyheight));
    */
    //hash sector line indicies?
    XXH32_update(xctx, &sec.linecount, sizeof(sec.linecount));
    /* ignore sector flags
    XXH32_update(xctx, &sec.SectorFlags, sizeof(sec.SectorFlags));
    */
    //hash something else?
  }

  void xxHashSegdef (XXH32_state_t *xctx, const seg_t &seg) const {
    if (!xctx) return;
    xxHashPlane(xctx, seg);
    vuint32 v0idx = (vuint32)(ptrdiff_t)(seg.v1-Vertexes);
    XXH32_update(xctx, &v0idx, sizeof(v0idx));
    vuint32 v1idx = (vuint32)(ptrdiff_t)(seg.v2-Vertexes);
    XXH32_update(xctx, &v1idx, sizeof(v1idx));
    //!XXH32_update(xctx, &seg.offset, sizeof(seg.offset));
    //!XXH32_update(xctx, &seg.length, sizeof(seg.length));
    vuint32 sdnum = (seg.sidedef ? (vuint32)(ptrdiff_t)(seg.sidedef-Sides) : 0xffffffffU);
    XXH32_update(xctx, &sdnum, sizeof(sdnum));
    vuint32 ldnum = (seg.linedef ? (vuint32)(ptrdiff_t)(seg.linedef-Lines) : 0xffffffffU);
    XXH32_update(xctx, &ldnum, sizeof(ldnum));
    vuint32 fsnum = (seg.frontsector ? (vuint32)(ptrdiff_t)(seg.frontsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &fsnum, sizeof(fsnum));
    vuint32 bsnum = (seg.backsector ? (vuint32)(ptrdiff_t)(seg.backsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &bsnum, sizeof(bsnum));
    vuint32 pnum = (seg.partner ? (vuint32)(ptrdiff_t)(seg.partner-Segs) : 0xffffffffU);
    XXH32_update(xctx, &pnum, sizeof(pnum));
    XXH32_update(xctx, &seg.side, sizeof(seg.side));
    //hash something else?
  }

public:
  // if `ImmediateRun` is true, init some script variables, but don't register thinker
  void AddScriptThinker (VLevelScriptThinker *sth, bool ImmediateRun);
  void RemoveScriptThinker (VLevelScriptThinker *sth); // won't call `Destroy()`, won't call `delete`
  void RunScriptThinkers (float DeltaTime);

  void SuspendNamedScriptThinkers (const VStr &name, int map);
  void TerminateNamedScriptThinkers (const VStr &name, int map);

public:
  void IncrementValidCount ();

  // this saves everything except thinkers, so i can load it for further experiments
  void DebugSaveLevel (VStream &strm);

  // this is slow!
  float CalcSkyHeight () const;

  // some sectors (like doors) has floor and ceiling on the same level, so
  // we have to look at neighbour sector to get height.
  // note that if neighbour sector is closed door too, we can safely use
  // our zero height, as camera cannot see through top/bottom textures.
  void CalcSectorBoundingHeight (const sector_t *sector, float *minz, float *maxz) const;

  void UpdateSubsectorBBox (int num, float *bbox, const float skyheight);
  void RecalcWorldNodeBBox (int bspnum, float *bbox, const float skyheight);
  void RecalcWorldBBoxes ();

  void GetSubsectorBBox (const subsector_t *sub, float bbox[6]) const;

public:
  virtual void SerialiseOther (VStream &Strm) override;
  virtual void ClearReferences () override;
  virtual void Destroy () override;

  // map loader
  void LoadMap (VName MapName);

  subsector_t *PointInSubsector (const TVec &point) const;

  inline bool HasPVS () const { return !!VisData; }

  //const vuint8 *LeafPVS (const subsector_t *ss) const;
  inline const vuint8 *LeafPVS (const subsector_t *ss) const {
    if (VisData) {
      const int sub = (int)(ptrdiff_t)(ss-Subsectors);
      return VisData+(((NumSubsectors+7)>>3)*sub);
    }
    return NoVis;
  }

  inline const vuint8 IsLeafVisible (const subsector_t *from, const subsector_t *dest) const {
    if (VisData && from != dest && from && dest) {
      const int sub = (int)(ptrdiff_t)(from-Subsectors);
      const vuint8 *vd = VisData+(((NumSubsectors+7)>>3)*sub);
      const int ss2 = (int)(ptrdiff_t)(dest-Subsectors);
      return vd[ss2>>3]&(1<<(ss2&7));
    }
    return 0xff;
  }

  void AddStaticLightRGB (VEntity *Ent, const TVec &Origin, float Radius, vuint32 Colour);
  void MoveStaticLightByOwner (VEntity *Ent, const TVec &Origin);

  VThinker *SpawnThinker (VClass *AClass, const TVec &AOrigin=TVec(0, 0, 0),
                          const TAVec &AAngles=TAVec(0, 0, 0), mthing_t *mthing=nullptr,
                          bool AllowReplace=true);
  void AddThinker (VThinker *Th);
  void RemoveThinker (VThinker *Th);
  void DestroyAllThinkers ();
  void TickWorld (float DeltaTime);

  // poly-objects
  void SpawnPolyobj (float x, float y, int tag, bool crush, bool hurt);
  void AddPolyAnchorPoint (float x, float y, int tag);
  void InitPolyobjs ();
  polyobj_t *GetPolyobj (int polyNum); // actually, tag
  int GetPolyobjMirror (int poly); // tag again
  bool MovePolyobj (int num, float x, float y, bool forced=false); // tag (GetPolyobj)
  bool RotatePolyobj (int num, float angle); // tag (GetPolyobj)

  bool ChangeSector (sector_t *sector, int crunch);

  bool TraceLine (linetrace_t &, const TVec &, const TVec &, int);

  // doesn't check pvs or reject
  bool CastCanSee (const TVec &org, const TVec &dest, float radius, sector_t *DestSector=nullptr);

  void SetCameraToTexture (VEntity *, VName, int);

  msecnode_t *AddSecnode (sector_t *, VEntity *, msecnode_t *);
  msecnode_t *DelSecnode (msecnode_t *);
  void DelSectorList ();

  int FindSectorFromTag (int tag, int start=-1);
  line_t *FindLine (int, int *);
  void SectorSetLink (int controltag, int tag, int surface, int movetype);

  inline bool IsForServer () const { return !!(LevelFlags&LF_ForServer); }
  inline bool IsForClient () const { return !(LevelFlags&LF_ForServer); }

  void BuildPVS ();

  void SaveCachedData (VStream *strm);
  bool LoadCachedData (VStream *strm);
  void ClearAllLevelData (); // call this if cache is corrupted

  void FixKnownMapErrors ();

private:
  // map loaders
  void LoadVertexes (int, int, int&);
  void LoadSectors (int);
  void LoadSideDefs (int);
  void LoadLineDefs1 (int, int, const mapInfo_t&);
  void LoadLineDefs2 (int, int, const mapInfo_t&);
  void LoadGLSegs (int, int);
  void LoadSubsectors (int);
  void LoadNodes (int);
  void LoadPVS (int);
  bool LoadCompressedGLNodes (int Lump, char hdr[4]);
  void LoadBlockMap (int);
  void LoadReject (int);
  void LoadThings1 (int);
  void LoadThings2 (int);
  void LoadLoadACS (int lacsLump, int XMapLump); // load libraries from 'loadacs'
  void LoadACScripts (int BehLump, int XMapLump);
  void LoadTextMap (int, const mapInfo_t &);
  // call this after loading things
  void SetupThingsFromMapinfo ();

  // call this after level is loaded or nodes are built
  void PostLoadSegs ();
  void PostLoadSubsectors ();

  void FixDeepWaters ();
  void FixSelfRefDeepWater (); // called from `FixDeepWaters`

  vuint32 IsFloodBugSector (sector_t *sec);
  sector_t *FindGoodFloodSector (sector_t *sec, bool wantFloor);

  void BuildDecalsVVList ();
  void BuildDecalsVVListOld ();

  // map loading helpers
  int FindGLNodes (VName) const;
  int TexNumForName (const char *name, int Type, bool CMap=false, bool fromUDMF=false) const;
  int TexNumForName2 (const char *name, int Type, bool fromUDMF) const;
  int TexNumOrColour (const char *, int, bool &, vuint32 &) const;
  void CreateSides ();
  void FinaliseLines ();
  void CreateRepBase ();
  void CreateBlockMap ();
  void BuildNodesAJ ();
  void BuildNodesZD ();
  void BuildNodes ();
  bool CreatePortals (void *pvsinfo);
  void SimpleFlood (/*portal_t*/void *srcportalp, int leafnum, void *pvsinfo);
  bool LeafFlow (int leafnum, void *pvsinfo);
  void BasePortalVis (void *pvsinfo);
  void HashSectors ();
  void HashLines ();
  void BuildSectorLists ();

  // post-loading routines
  void GroupLines () const;
  void LinkNode (int, node_t *) const;
  void ClearBox (float *) const;
  void AddToBox (float *, float, float) const;
  void FloodZones ();
  void FloodZone (sector_t *, int);

  // loader of the Strife conversations
  void LoadRogueConScript (VName, int, FRogueConSpeech *&, int &) const;

  // internal poly-object methods
  void IterFindPolySegs (const TVec &, seg_t **, int &, const TVec &);
  void TranslatePolyobjToStartSpot (float, float, int);
  void UpdatePolySegs (polyobj_t *);
  void InitPolyBlockMap ();
  void LinkPolyobj (polyobj_t *);
  void UnLinkPolyobj (polyobj_t *);
  bool PolyCheckMobjBlocking (seg_t *, polyobj_t *);

  // internal TraceLine methods
  bool CheckPlane (linetrace_t &, const sec_plane_t *) const;
  bool CheckPlanes (linetrace_t &, sector_t *) const;
  bool CheckLine (linetrace_t &, seg_t *) const;
  bool CrossSubsector (linetrace_t &, int) const;
  bool CrossBSPNode (linetrace_t &, int) const;

  int SetBodyQueueTrans (int, int);

  void AddDecal (TVec org, const VName &dectype, int side, line_t *li, int level);
  void AddDecalById (TVec org, int id, int side, line_t *li, int level);
  // called by `AddDecal()`
  void AddOneDecal (int level, TVec org, VDecalDef *dec, sector_t *sec, line_t *li);
  void PutDecalAtLine (int tex, float orgz, float segdist, VDecalDef *dec, sector_t *sec, line_t *li, int prevdir, vuint32 flips);

  void AddAnimatedDecal (decal_t *dc);
  void RemoveAnimatedDecal (decal_t *dc); // this will also kill animator

  void PostProcessForDecals ();

  void processSoundSector (int validcount, TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin);
  void doRecursiveSound (int validcount, TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin);

  void eventBeforeWorldTick (float deltaTime) {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("BeforeWorldTick"));
    P_PASS_SELF;
    P_PASS_FLOAT(deltaTime);
    EV_RET_VOID_IDX(mtindex);
  }

  void eventAfterWorldTick (float deltaTime) {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("AfterWorldTick"));
    P_PASS_SELF;
    P_PASS_FLOAT(deltaTime);
    EV_RET_VOID_IDX(mtindex);
  }

  void eventEntitySpawned (VEntity *e) {
    if (e) {
      static int mtindex = -666;
      if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("OnEntitySpawned"));
      P_PASS_SELF;
      P_PASS_REF(e);
      EV_RET_VOID_IDX(mtindex);
    }
  }

  void eventEntityDying (VEntity *e) {
    if (e) {
      static int mtindex = -666;
      if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("OnEntityDying"));
      P_PASS_SELF;
      P_PASS_REF(e);
      EV_RET_VOID_IDX(mtindex);
    }
  }


  void eventKnownMapBugFixer () {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("KnownMapBugFixer"));
    P_PASS_SELF;
    EV_RET_VOID_IDX(mtindex);
  }

public:
  void eventAfterUnarchiveThinkers () {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("AfterUnarchiveThinkers"));
    P_PASS_SELF;
    EV_RET_VOID_IDX(mtindex);
  }

private:
  DECLARE_FUNCTION(GetLineIndex)

  DECLARE_FUNCTION(PointInSector)
  DECLARE_FUNCTION(PointInSubsector)
  DECLARE_FUNCTION(TraceLine)
  DECLARE_FUNCTION(ChangeSector)
  DECLARE_FUNCTION(AddExtraFloor)
  DECLARE_FUNCTION(SwapPlanes)

  DECLARE_FUNCTION(SetFloorPic)
  DECLARE_FUNCTION(SetCeilPic)
  DECLARE_FUNCTION(SetLineTexture)
  DECLARE_FUNCTION(SetLineAlpha)
  DECLARE_FUNCTION(SetFloorLightSector)
  DECLARE_FUNCTION(SetCeilingLightSector)
  DECLARE_FUNCTION(SetHeightSector)

  DECLARE_FUNCTION(FindSectorFromTag)
  DECLARE_FUNCTION(FindLine)
  DECLARE_FUNCTION(SectorSetLink)

  //  Polyobj functions
  DECLARE_FUNCTION(SpawnPolyobj)
  DECLARE_FUNCTION(AddPolyAnchorPoint)
  DECLARE_FUNCTION(GetPolyobj)
  DECLARE_FUNCTION(GetPolyobjMirror)
  DECLARE_FUNCTION(MovePolyobj)
  DECLARE_FUNCTION(RotatePolyobj)

  //  ACS functions
  DECLARE_FUNCTION(StartACS)
  DECLARE_FUNCTION(SuspendACS)
  DECLARE_FUNCTION(TerminateACS)
  DECLARE_FUNCTION(StartTypedACScripts)

  DECLARE_FUNCTION(RunACS)
  DECLARE_FUNCTION(RunACSAlways)
  DECLARE_FUNCTION(RunACSWithResult)

  DECLARE_FUNCTION(RunNamedACS)
  DECLARE_FUNCTION(RunNamedACSAlways)
  DECLARE_FUNCTION(RunNamedACSWithResult)

  DECLARE_FUNCTION(SetBodyQueueTrans)

  DECLARE_FUNCTION(AddDecal)
  DECLARE_FUNCTION(AddDecalById)

  DECLARE_FUNCTION(doRecursiveSound)

  DECLARE_FUNCTION(AllThinkers)
  DECLARE_FUNCTION(AllActivePlayers)

  DECLARE_FUNCTION(LdrTexNumForName)
};

void CalcLine (line_t *line);
void CalcSeg (seg_t *seg);
void SV_LoadLevel (VName MapName);
void CL_LoadLevel (VName MapName);
sec_region_t *AddExtraFloor (line_t *line, sector_t *dst);
void SwapPlanes (sector_t *);
void CalcSecMinMaxs (sector_t *sector);

extern VLevel *GLevel;
extern VLevel *GClLevel;
