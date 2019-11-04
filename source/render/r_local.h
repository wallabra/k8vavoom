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
#ifndef VAVOOM_R_LOCAL_HEADER
#define VAVOOM_R_LOCAL_HEADER

#include "cl_local.h"
#include "r_shared.h"
#include "fmd2defs.h"


// ////////////////////////////////////////////////////////////////////////// //
//#define MAX_SPRITE_MODELS  (10*1024)

// was 0.1
#define FUZZY_ALPHA  (0.7f)


// ////////////////////////////////////////////////////////////////////////// //
// dynamic light types
enum DLType {
  DLTYPE_Point,
  DLTYPE_MuzzleFlash,
  DLTYPE_Pulse,
  DLTYPE_Flicker,
  DLTYPE_FlickerRandom,
  DLTYPE_Sector,
  DLTYPE_Subtractive, // partially supported
  DLTYPE_SectorSubtractive, // not supported
  DLTYPE_Spot,
};


// ////////////////////////////////////////////////////////////////////////// //
// Sprites are patches with a special naming convention so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t is range checked at run time.
// A sprite is a patch_t that is assumed to represent a three dimensional object and may have multiple
// rotations pre drawn.
// Horizontal flipping is used to save space, thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used for all views: NNNNF0
struct spriteframe_t {
  // if false use 0 for any position
  // NOTE: as eight entries are available, we might as well insert the same name eight times
  //bool rotate;
  short rotate;
  // lump to use for view angles 0-7
  int lump[16];
  // flip bit (1 = flip) to use for view angles 0-7
  bool flip[16];
};


// a sprite definition:
// a number of animation frames
struct spritedef_t {
  int numframes;
  spriteframe_t *spriteframes;
};


struct segpart_t {
  segpart_t *next;
  texinfo_t texinfo;
  surface_t *surfs;
  float frontTopDist;
  float frontBotDist;
  float backTopDist;
  float backBotDist;
  float TextureOffset;
  float RowOffset;
  //vint32 regidx; // region index (negative means "backsector region")
  sec_region_t *basereg;
};


struct sec_surface_t {
  TSecPlaneRef esecplane;
  texinfo_t texinfo;
  float edist;
  float XScale;
  float YScale;
  float Angle;
  surface_t *surfs;

  inline float PointDist (const TVec &p) const { return esecplane.DotPointDist(p); }
};


struct skysurface_t : surface_t {
  TVec __verts[3]; // so we have 4 of 'em here
};


struct sky_t {
  int texture1;
  int texture2;
  float columnOffset1;
  float columnOffset2;
  float scrollDelta1;
  float scrollDelta2;
  skysurface_t surf;
  TPlane plane;
  texinfo_t texinfo;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSky {
public:
  enum {
    VDIVS = 8,
    HDIVS = 16
  };

  sky_t sky[HDIVS*VDIVS];
  int NumSkySurfs;
  int SideTex;
  bool bIsSkyBox;
  bool SideFlip;

  void InitOldSky (int, int, float, float, bool, bool, bool);
  void InitSkyBox (VName, VName);
  void Init (int, int, float, float, bool, bool, bool, bool);
  void Draw (int);
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyPortal : public VPortal {
public:
  VSky *Sky;

  VSkyPortal (VRenderLevelShared *ARLev, VSky *ASky) : VPortal(ARLev), Sky(ASky) { stackedSector = false; }
  virtual bool NeedsDepthBuffer () const override;
  virtual bool IsSky () const override;
  virtual bool MatchSky (VSky *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyBoxPortal : public VPortal {
public:
  VEntity *Viewport;

  VSkyBoxPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) { stackedSector = false; }
  virtual bool IsSky () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSectorStackPortal : public VPortal {
public:
  VEntity *Viewport;

  VSectorStackPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) { stackedSector = true; }
  virtual bool IsStack () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMirrorPortal : public VPortal {
public:
  TPlane *Plane;

  VMirrorPortal (VRenderLevelShared *ARLev, TPlane *APlane) : VPortal(ARLev), Plane(APlane) { stackedSector = false; }
  virtual bool IsMirror () const override;
  virtual bool MatchMirror (TPlane *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
enum ERenderPass {
  // for regular renderer
  RPASS_Normal,
  // for advanced renderer
  RPASS_Ambient,
  RPASS_ShadowVolumes,
  RPASS_Light,
  RPASS_Textures,
  RPASS_Fog,
  RPASS_NonShadow,
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShared : public VRenderLevelDrawer {
protected:
  friend class VPortal;
  friend class VSkyPortal;
  friend class VSkyBoxPortal;
  friend class VSectorStackPortal;
  friend class VMirrorPortal;

  struct BSPVisInfo {
    //enum { UNKNOWN = -1, INVISIBLE = 0, VISIBLE = 1, };
    vuint32 framecount; // data validity checking
    //float radius;
    //vuint8 vis;
  };

public:
  enum {
    MAX_PARTICLES = 2048, // default max # of particles at one time
    ABSOLUTE_MIN_PARTICLES = 512, // no fewer than this no matter what's on the command line
    MAX_DLIGHTS = 32, // should fit into vuint32, 'cause subsector is using that as bitmask for active lights
  };

  struct trans_sprite_t {
    TVec Verts[4]; // only for sprites
    union {
      surface_t *surf; // for masked polys and sprites
      VEntity *Ent; // only for alias models
    };
    int prio; // for things
    int lump; // basically, has any sense only for sprites, has no sense for alias models
    TVec normal; // not set for alias models
    union {
      float pdist; // masked polys and sprites
      float TimeFrac; // alias models
    };
    TVec saxis; // masked polys and sprites
    TVec taxis; // masked polys and sprites
    TVec texorg; // masked polys and sprites
    float Alpha;
    bool Additive;
    int translation; // masked polys and sprites
    int type; // 0: masked polygon (wall); 1: sprite; 2: alias model
    float dist; // for soriting
    vuint32 objid; // for entities
    int hangup; // 0: normal; -1: no z-buffer write, slightly offset (used for flat-aligned sprites); 666: fake sprite shadow
    vuint32 light;
    vuint32 Fade;
  };

  struct world_surf_t {
    surface_t *Surf;
    vuint8 Type;
  };

  struct light_t {
    TVec origin;
    float radius;
    vuint32 color;
    VEntity *owner;
    int leafnum;
    bool active; // for filtering
  };

protected:
  struct DLightInfo {
    int needTrace; // <0: no; >1: yes; 0: don't know
    int leafnum; // -1: unknown yet
  };

protected:
  VLevel *Level;

  VEntity *ViewEnt;

  //unsigned FrustumIndexes[5][6];
  int MirrorLevel;
  int PortalLevel;
  int VisSize;
  vuint8 *BspVis;
  vuint8 *BspVisThing; // for things, why not

  subsector_t *r_viewleaf;
  subsector_t *r_oldviewleaf;
  float old_fov;
  int prev_aspect_ratio;

  // bumped light from gun blasts
  int ExtraLight;
  int FixedLight;
  int ColorMap;

  int NumParticles;
  particle_t *Particles;
  particle_t *ActiveParticles;
  particle_t *FreeParticles;

  // sky variables
  int CurrentSky1Texture;
  int CurrentSky2Texture;
  bool CurrentDoubleSky;
  bool CurrentLightning;
  VSky BaseSky;

  // world render variables
  VViewClipper ViewClip;
  TArray<world_surf_t> WorldSurfs;
  TArray<VPortal *> Portals;
  TArray<VSky *> SideSkies;

  // (partially) transparent sprites list
  // cleared in `DrawTranslucentPolys()`
  trans_sprite_t *trans_sprites;
  int traspUsed;
  int traspSize;
  int traspFirst; // for portals, `DrawTranslucentPolys()` will start from this

  sec_plane_t sky_plane;
  float skyheight;
  surface_t *free_wsurfs;
  void *AllocatedWSurfBlocks;
  subregion_t *AllocatedSubRegions;
  drawseg_t *AllocatedDrawSegs;
  segpart_t *AllocatedSegParts;

  // static lights
  TArray<light_t> Lights;
  TMapNC<vuint32, vint32> StOwners; // key: object id; value: index in `Lights`
  // dynamic lights
  dlight_t DLights[MAX_DLIGHTS];
  DLightInfo dlinfo[MAX_DLIGHTS];
  TMapNC<vuint32, vuint32> dlowners; // key: object id; value: index in `DLights`

  // moved here so that model rendering methods can be merged
  TVec CurrLightPos;
  float CurrLightRadius;
  bool CurrLightInFrustum; // moved to drawer.h
  vuint32 CurrLightBit; // tag (bitor) subsectors with this in lightvis builder
  int CurrLightsNumber;
  int CurrShadowsNumber;
  int AllLightsNumber;
  int AllShadowsNumber;

  // used in `AllocDlight()` to save one call to `PointInSubsector()`
  // reset in `RenderPlayerView()`
  TVec lastDLightView;
  subsector_t *lastDLightViewSub;

  bool inWorldCreation; // are we creating world surfaces now?

  // mark all updated subsectors with this; increment on each new frame
  vuint32 updateWorldFrame;

  // those three arrays are filled in `BuildVisibleObjectsList()`
  TArray<VEntity *> visibleObjects;
  TArray<VEntity *> visibleAliasModels;
  TArray<VEntity *> allShadowModelObjects; // used in advrender
  bool useInCurrLightAsLight;

  BSPVisInfo *bspVisRadius;
  vuint32 bspVisRadiusFrame;

  VViewClipper LightClip;
  vuint8 *LightVis;
  vuint8 *LightBspVis;
  bool HasLightIntersection; // set by `BuildLightVis()`
  TArray<int> LightSubs; // all affected subsectors
  TArray<int> LightVisSubs; // visible affected subsectors
  TVec LitBBox[2];
  int LitSurfaces;
  bool HasBackLit; // if there's no backlit surfaces, use zpass

  bool doShadows; // for current light

  // renderer info; put here to avoid passing it around
  // k8: it was a bunch of globals; i will eventually got rid of this
  bool MirrorClipSegs;

  // used in `CreateWorldSurfaces()` and friends
  segpart_t *pspart;
  int pspartsLeft;

  vuint32 currVisFrame;

  // chasecam
  double prevChaseCamTime = -1.0;
  TVec prevChaseCamPos;

private:
  VDirtyArea unusedDirty; // required to return reference to it

private:
  inline segpart_t *SurfCreatorGetPSPart () {
    if (pspartsLeft == 0) Sys_Error("internal level surface creation bug");
    --pspartsLeft;
    return pspart++;
  }

protected:
  void NewBSPVisibilityFrame ();
  bool CheckBSPVisibilitySub (const TVec &org, const float radius, const subsector_t *currsub, const seg_t *firsttravel);
  bool CheckBSPVisibility (const TVec &org, float radius, const subsector_t *sub=nullptr);

  void ResetVisFrameCount ();
  inline vuint32 IncVisFrameCount () {
    if ((++currVisFrame) == 0x7fffffff) ResetVisFrameCount();
    return currVisFrame;
  }

  void ResetDLightFrameCount ();
  inline int IncDLightFrameCount () {
    if ((++currDLightFrame) == 0xffffffff) ResetDLightFrameCount();
    return currDLightFrame;
  }

  void ResetUpdateWorldFrame ();
  inline void IncUpdateWorldFrame () {
    if ((++updateWorldFrame) == 0xffffffff) ResetUpdateWorldFrame();
  }

  inline void IncQueueFrameCount () {
    //if ((++currQueueFrame) == 0xffffffff) ResetQueueFrameCount();
    ++currQueueFrame;
    if (currQueueFrame == 0xffffffff) {
      // there is nothing we can do with this yet
      // resetting this is too expensive, and doesn't worth the efforts
      // i am not expecting for someone to play one level for that long in one seat
      Sys_Error("*************** WARNING!!! QUEUE FRAME COUNT OVERFLOW!!! ***************");
    }
  }

  // clears render queues
  virtual void ClearQueues ();

protected:
  // entity must not be `nullptr`, and must have `SubSector` set
  inline bool IsThingVisible (VEntity *ent) const {
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
    return !!(BspVisThing[SubIdx>>3]&(1u<<(SubIdx&7)));
    /*
    if (!(BspVisThing[SubIdx>>3]&(1u<<(SubIdx&7)))) return false;
    // if it is not in a visible level part, check render radius
    if (BspVis[SubIdx>>3]&(1u<<(SubIdx&7))) return true;
    return IsCircleTouchBox2D(ent->Origin.x, ent->Origin.y, ent->Radius, ent->SubSector->bbox2d);
    */
  }

public:
  virtual bool IsNodeRendered (const node_t *node) const override;
  virtual bool IsSubsectorRendered (const subsector_t *sub) const override;

  virtual vuint32 LightPoint (const TVec &p, float radius, float height, const TPlane *surfplane=nullptr, const subsector_t *psub=nullptr) override;

  virtual void UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced=false) override;

  virtual void PrecacheLevel () override;

  // lightmap chain iterator (used in renderer)
  // block number+1 or 0
  virtual vuint32 GetLightChainHead () override;
  // block number+1 or 0
  virtual vuint32 GetLightChainNext (vuint32 bnum) override;

  virtual VDirtyArea &GetLightBlockDirtyArea (vuint32 bnum) override;
  virtual VDirtyArea &GetLightAddBlockDirtyArea (vuint32 bnum) override;
  virtual rgba_t *GetLightBlock (vuint32 bnum) override;
  virtual rgba_t *GetLightAddBlock (vuint32 bnum) override;
  virtual surfcache_t *GetLightChainFirst (vuint32 bnum) override;

  virtual void NukeLightmapCache () override;

  virtual void FullWorldUpdate (bool forceClientOrigin) override;

public:
  static vuint32 CountSurfacesInChain (const surface_t *s) noexcept;
  static vuint32 CountSegSurfacesInChain (const segpart_t *sp) noexcept;
  vuint32 CountAllSurfaces () const noexcept;

protected:
  VRenderLevelShared (VLevel *ALevel);
  ~VRenderLevelShared ();

  void UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane=nullptr);
  void UpdateDrawSeg (subsector_t *r_surf_sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void UpdateSubRegion (subsector_t *r_surf_sub, subregion_t *region);

  void UpdateFakeSectors (subsector_t *viewleaf=nullptr);
  void InitialWorldUpdate ();

  void UpdateBBoxWithSurface (TVec bbox[2], const surface_t *surfs, const texinfo_t *texinfo,
                              VEntity *SkyBox, bool CheckSkyBoxAlways);
  void UpdateBBoxWithLine (TVec bbox[2], VEntity *SkyBox, const drawseg_t *dseg);

  // this two is using to check if light can cast any shadow (and if it is visible at all)
  // note that this should be called with filled `BspVis`
  // if we will use this for dynamic lights, they will have one-frame latency
  void CheckLightSubsector (const subsector_t *sub);
  void BuildLightVis (int bspnum, const float *bbox);
  // main entry point for lightvis calculation
  // sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
  // returns `false` if the light is invisible
  bool CalcLightVis (const TVec &org, const float radius, vuint32 currltbit=0);

  // yes, non-virtual
  // dlinfo::leafnum must be set (usually this is done in `PushDlights()`)
  //void MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum);

  virtual void RenderScene (const refdef_t *, const VViewClipper *) = 0;
  virtual void PushDlights ();
  //virtual vuint32 LightPoint (const TVec &p, VEntity *mobj) = 0; // defined only after `PushDlights()`

  // returns attenuation multiplier (0 means "out of cone")
  static float CheckLightPointCone (const TVec &p, const float radius, const float height, const TVec &coneOrigin, const TVec &coneDir, const float coneAngle);

  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) = 0;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) = 0;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) = 0;
  virtual void QueueWorldSurface (surface_t*) = 0;
  virtual void FreeSurfCache (surfcache_t *&block);
  virtual bool CacheSurface (surface_t*);
  // this is called after surface queues built, so lightmap renderer can calculate new lightmaps
  virtual void ProcessCachedSurfaces ();

  void PrepareWorldRender (const refdef_t*, const VViewClipper*);
  // this should be called after `RenderWorld()`
  void BuildVisibleObjectsList ();

  // general
  void ExecuteSetViewSize ();
  void TransformFrustum ();
  void SetupFrame ();
  void SetupCameraFrame (VEntity*, VTexture*, int, refdef_t*);
  void MarkLeaves ();
  void UpdateCameraTexture (VEntity*, int, int);
  vuint32 GetFade (sec_region_t*);
  int CollectSpriteTextures (TArray<bool> &texturepresent); // this is actually private, but meh... returns number of new textures
  void UncacheLevel ();
  VTextureTranslation *GetTranslation (int);
  void BuildPlayerTranslations ();

  // particles
  void InitParticles ();
  void ClearParticles ();
  void UpdateParticles (float);
  void DrawParticles ();

  // sky methods
  void InitSky ();
  void AnimateSky (float);

  // checks if surface is not queued twice, sets various flags
  // returns `false` if the surface should not be queued
  bool SurfPrepareForRender (surface_t *surf);
  // this checks if surface is not queued twice
  void SurfCheckAndQueue (TArray<surface_t *> &queue, surface_t *surf);

  // world BSP rendering
  void QueueSimpleSurf (surface_t *surf);
  void QueueSkyPortal (surface_t *surf);
  void QueueHorizonPortal (surface_t *surf);
  void CommonQueueSurface (surface_t *surf, vuint8 type);

  void DrawSurfaces (subsector_t *sub, sec_region_t *secregion, seg_t *seg, surface_t *InSurfs,
                     texinfo_t *texinfo, VEntity *SkyBox, int LightSourceSector, int SideLight,
                     bool AbsSideLight, bool CheckSkyBoxAlways);

  void GetFlatSetToRender (subsector_t *sub, subregion_t *region, sec_surface_t *surfs[4]);
  void ChooseFlatSurfaces (sec_surface_t *&f0, sec_surface_t *&f1, sec_surface_t *flat0, sec_surface_t *flat1);

  // used in `RenderSubRegion()` and other similar methods
  static bool NeedToRenderNextSubFirst (const subregion_t *region);

  void RenderHorizon (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg);
  void RenderMirror (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg);
  void RenderLine (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg);
  void RenderSecFlatSurfaces (subsector_t *sub, sec_region_t *secregion, sec_surface_t *flat0, sec_surface_t *flat1, VEntity *SkyBox);
  void RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox);
  void AddPolyObjToClipper (VViewClipper &clip, subsector_t *sub);
  void RenderPolyObj (subsector_t *sub);
  void RenderSubRegion (subsector_t *sub, subregion_t *region);
  void RenderMarkAdjSubsectorsThings (int num); // used for "better things rendering"
  void RenderSubsector (int num, bool onlyClip);
  void RenderBSPNode (int bspnum, const float *bbox, unsigned AClipflags, bool onlyClip=false);
  void RenderBspWorld (const refdef_t*, const VViewClipper*);
  void RenderPortals ();

  void CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec wv[4], vuint32 typeFlags, bool doOffset=false);

  void SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                   TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, opening_t *ops);

  // surf methods
  void SetupSky ();
  void FlushSurfCaches (surface_t *InSurfs);
  // `ssurf` can be `nullptr`, and it will be allocated, otherwise changed
  // this is used both to create initial surfaces, and to update changed surfaces
  sec_surface_t *CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane);
  // subsector is not changed, but we need it non-const
  //enum { USS_Normal, USS_Force, USS_IgnoreCMap, USS_ForceIgnoreCMap };
  void UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, bool ignoreColorMap=false);
  surface_t *NewWSurf ();
  void FreeWSurfs (surface_t *&);
  surface_t *CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags);
  int CountSegParts (const seg_t *);
  void CreateSegParts (subsector_t *r_surf_sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg, bool isMainRegion);
  void CreateWorldSurfaces ();
  bool CopyPlaneIfValid (sec_plane_t*, const sec_plane_t*, const sec_plane_t*);
  void UpdateFakeFlats (sector_t*);
  void UpdateDeepWater (sector_t*);
  void UpdateFloodBug (sector_t *sec);
  // free all surfaces except the first one, clear first, set number of vertices to vcount
  surface_t *ReallocSurface (surface_t *surfs, int vcount);
  void FreeSurfaces (surface_t*);
  void FreeSegParts (segpart_t*);

  // models
  bool DrawAliasModel (VEntity *mobj, const TVec&, const TAVec&, float, float, VModel*,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
    VTextureTranslation*, int, vuint32, vuint32, float, bool,
    bool, float, bool, ERenderPass);
  bool DrawAliasModel (VEntity *mobj, VName clsName, const TVec&, const TAVec&, float, float,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
    VTextureTranslation*, int, vuint32, vuint32, float, bool,
    bool, float, bool, ERenderPass);
  bool DrawEntityModel (VEntity*, vuint32, vuint32, float, bool, float, ERenderPass);
  bool CheckAliasModelFrame (VEntity *Ent, float Inter);
  bool HasAliasModel (VName clsName) const;

  // things
  void QueueTranslucentPoly (surface_t *surf, TVec *sv, int count, int lump,
                             float Alpha, bool Additive, int translation,
                             bool isSprite, vuint32 light, vuint32 Fade, const TVec &normal, float pdist,
                             const TVec &saxis, const TVec &taxis, const TVec &texorg, int priority=0,
                             bool useSprOrigin=false, const TVec &sprOrigin=TVec(), vuint32 objid=0,
                             int hangup=0);
  void QueueSprite (VEntity *thing, vuint32 light, vuint32 Fade, float Alpha, bool Additive, vuint32 seclight, bool onlyShadow=false);
  void QueueTranslucentAliasModel (VEntity *mobj, vuint32 light, vuint32 Fade, float Alpha, bool Additive, float TimeFrac);
  bool RenderAliasModel (VEntity*, vuint32, vuint32, float, bool, ERenderPass);
  void RenderThing (VEntity*, ERenderPass);
  void RenderMobjs (ERenderPass);
  void DrawTranslucentPolys ();
  void RenderPSprite (VViewState*, const VAliasModelFrameInfo, float, vuint32, vuint32, float, bool);
  bool RenderViewModel (VViewState*, vuint32, vuint32, float, bool);
  void DrawPlayerSprites ();
  void DrawCrosshair ();

  // used in light checking
  bool RadiusCastRay (sector_t *sector, const TVec &org, const TVec &dest, float radius, bool advanced);

protected:
  virtual void RefilterStaticLights ();

  // used to get a floor to sample lightmap
  // WARNING! can return `nullptr`!
  sec_surface_t *GetNearestFloor (const subsector_t *sub, const TVec &p);

  // this is common code for light point calculation
  // pass light values from ambient pass
  void CalculateDynLightSub (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, const TPlane *surfplane);

  // calculate subsector's ambient light (light variables must be initialized)
  void CalculateSubAmbient (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, const TPlane *surfplane);

  // calculate subsector's light from static light sources (light variables must be initialized)
  void CalculateSubStatic (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, const TPlane *surfplane);

  virtual void InvalidateStaticLightmaps (const TVec &org, float radius, bool relight);

public:
  virtual particle_t *NewParticle (const TVec &porg) override;

  virtual int GetStaticLightCount () const override;
  virtual LightInfo GetStaticLight (int idx) const override;

  virtual int GetDynamicLightCount () const override;
  virtual LightInfo GetDynamicLight (int idx) const override;

  virtual void RenderPlayerView () override;

  virtual void SegMoved (seg_t *) override;
  virtual void SetupFakeFloors (sector_t *) override;

  virtual void AddStaticLightRGB (VEntity *Owner, const TVec &origin, float radius, vuint32 color) override;
  virtual void MoveStaticLightByOwner (VEntity *Owner, const TVec &origin) override;
  virtual void ClearReferences () override;

  virtual dlight_t *AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid=-1) override;
  virtual dlight_t *FindDlightById (int lightid) override;
  virtual void DecayLights (float) override;
  virtual void RemoveOwnedLight (VThinker *Owner) override;

  virtual void ResetLightmaps (bool recalcNow) override;

  virtual bool isNeedLightmapCache () const noexcept override;
  virtual void saveLightmaps (VStream *strm) override;
  virtual bool loadLightmaps (VStream *strm) override;

public: // k8: so i don't have to fuck with friends
  struct PPNode {
    vuint8 *mem;
    int size;
    int used;
    PPNode *next;
  };

  struct PPMark {
    PPNode *curr;
    int currused;

    PPMark () : curr(nullptr), currused(-666) {}
    inline bool isValid () const { return (currused != -666); }
  };


  static PPNode *pphead;
  static PPNode *ppcurr;
  static int ppMinNodeSize;

  static void CreatePortalPool ();
  static void KillPortalPool ();

public:
  static void ResetPortalPool (); // called on frame start
  static void SetMinPoolNodeSize (int minsz);

  static void MarkPortalPool (PPMark *mark);
  static void RestorePortalPool (PPMark *mark);
  static vuint8 *AllocPortalPool (int size);
};


// ////////////////////////////////////////////////////////////////////////// //
class VLMapCache : public V2DCache<surfcache_t> {
public:
  VLMapCache () noexcept : V2DCache<surfcache_t>() {}

  virtual void releaseAtlas (vuint32 id) noexcept override;
  virtual void resetBlock (Item *block) noexcept override;
  virtual void clearCacheInfo () noexcept override;
  virtual AtlasInfo allocAtlas (vuint32 aid, int minwidth, int minheight) noexcept override;
};


// ////////////////////////////////////////////////////////////////////////// //
// lightmapped renderer
class VRenderLevelLightmap : public VRenderLevelShared {
private:
  // light chain bookkeeping
  struct LCEntry {
    vuint32 lastframe; // using `cacheframecount`
    vuint32 next; // next used entry in this frame+1, or 0
  };

private:
  int c_subdivides;
  int c_seg_div;

  // used in lightmap atlas manager
  //vuint32 cacheframecount;

  // lightmaps
  rgba_t light_block[NUM_BLOCK_SURFS][BLOCK_WIDTH*BLOCK_HEIGHT];
  VDirtyArea block_dirty[NUM_BLOCK_SURFS];
  surfcache_t *light_chain[NUM_BLOCK_SURFS];
  LCEntry light_chain_used[NUM_BLOCK_SURFS];
  vuint32 light_chain_head; // entry+1 (i.e. 0 means "none")

  // specular lightmaps
  rgba_t add_block[NUM_BLOCK_SURFS][BLOCK_WIDTH*BLOCK_HEIGHT];
  VDirtyArea add_block_dirty[NUM_BLOCK_SURFS];

  // surface (lightmap) cache
  VLMapCache lmcache;
  TArray<surface_t *> LMSurfList; // list of all surfaces with lightmaps
  bool nukeLightmapsOnNextFrame;

  bool invalidateRelight;

  // temporary flag for lightmap cache loader
  // set if some surface wasn't found
  unsigned lmcacheUnknownSurfaceCount;

private:
  // returns `false` if cannot allocate lightmap block
  bool BuildSurfaceLightmap (surface_t *surface);

public:
  struct LMapTraceInfo {
    enum { GridSize = 18 };
    TVec smins, smaxs;
    TVec worldtotex[2];
    TVec textoworld[2];
    TVec texorg;
    TVec surfpt[GridSize*GridSize*4]; // *4 for extra filtering
    int numsurfpt;
    bool pointsCalced;
    bool light_hit;
    bool didExtra;
    // for spotlights
    bool spotLight;
    float coneAngle;
    TVec coneDir;

    LMapTraceInfo () { memset((void *)this, 0, sizeof(LMapTraceInfo)); }
    LMapTraceInfo (const LMapTraceInfo &) = delete;
    LMapTraceInfo & operator = (const LMapTraceInfo &) = delete;

    inline TVec calcTexPoint (const float us, const float ut) const { return texorg+textoworld[0]*us+textoworld[1]*ut; }

    inline void setupSpotlight (const TVec &aconeDir, const float aconeAngle) {
      spotLight = false;
      coneAngle = (aconeAngle <= 0.0f || aconeAngle >= 360.0f ? 0.0f : aconeAngle);
      coneDir = aconeDir;
      if (coneAngle && coneDir.isValid() && !coneDir.isZero()) {
        spotLight = true;
        coneDir.normaliseInPlace();
      }
    }
  };

protected:
  void InvalidateSurfacesLMaps (const TVec &org, float radius, surface_t *surf);
  void InvalidateLineLMaps (const TVec &org, float radius, drawseg_t *dseg);

  void InvalidateSubsectorLMaps (const TVec &org, float radius, int num);
  void InvalidateBSPNodeLMaps (const TVec &org, float radius, int bspnum, const float *bbox);

protected:
  void initLightChain ();
  void chainLightmap (surfcache_t *cache);
  void advanceCacheFrame ();

public:
  // lightmap chain iterator (used in renderer)
  // block number+1 or 0
  virtual vuint32 GetLightChainHead () override;
  // block number+1 or 0
  virtual vuint32 GetLightChainNext (vuint32 bnum) override;

  virtual VDirtyArea &GetLightBlockDirtyArea (vuint32 bnum) override;
  virtual VDirtyArea &GetLightAddBlockDirtyArea (vuint32 bnum) override;
  virtual rgba_t *GetLightBlock (vuint32 bnum) override;
  virtual rgba_t *GetLightAddBlock (vuint32 bnum) override;
  virtual surfcache_t *GetLightChainFirst (vuint32 bnum) override;
  virtual void NukeLightmapCache () override;

protected:
  // clears render queues
  virtual void ClearQueues () override;

  virtual void InvalidateStaticLightmaps (const TVec &org, float radius, bool relight) override;

  // general
  virtual void RenderScene (const refdef_t *, const VViewClipper *) override;

  // surf methods
  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) override;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) override;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) override;

  // light methods
  float CastRay (sector_t *ssector, const TVec &p1, const TVec &p2, float squaredist);
  static void CalcMinMaxs (LMapTraceInfo &lmi, const surface_t *surf);
  static bool CalcFaceVectors (LMapTraceInfo &lmi, const surface_t *surf);
  void CalcPoints (LMapTraceInfo &lmi, const surface_t *surf, bool lowres); // for dynlights, set `lowres` to `true`
  void SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf, const vuint8 *facevis);
  void AddDynamicLights (surface_t *surf);
  //virtual void PushDlights () override;

  // lightmap cache manager
  void FlushCaches ();
  virtual void FreeSurfCache (surfcache_t *&block) override;
  virtual bool CacheSurface (surface_t*) override;
  virtual void ProcessCachedSurfaces () override;

  // world BSP rendering
  virtual void QueueWorldSurface (surface_t*) override;
  void RenderWorld (const refdef_t*, const VViewClipper*);

  void RelightMap (bool recalcNow, bool onlyMarked);

public:
  // this has to be public for now
  // this method calculates static lightmap for a surface
  void LightFace (surface_t *surf, subsector_t *leaf);

public:
  VRenderLevelLightmap (VLevel*);

  virtual void PreRender () override;

  virtual void BuildLightMap (surface_t *) override;

  virtual void ResetLightmaps (bool recalcNow) override;

  virtual bool isNeedLightmapCache () const noexcept override;
  virtual void saveLightmaps (VStream *strm) override;
  virtual bool loadLightmaps (VStream *strm) override;

private:
  void saveLightmapsInternal (VStream *strm);
  bool loadLightmapsInternal (VStream *strm);
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShadowVolume : public VRenderLevelShared {
private:
  vuint32 CurrLightColor;
  // built in `BuildMobjsInCurrLight()`
  // used in rendering of shadow and light things (only)
  TArray<VEntity *> mobjsInCurrLight;
  int LightsRendered;

protected:
  virtual void RefilterStaticLights () override;

  // general
  virtual void RenderScene (const refdef_t*, const VViewClipper*) override;

  // surf methods
  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) override;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) override;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) override;

  // light methods
  //virtual void PushDlights () override;
  // `radius` is used for... nothing yet
  // `surfplace` is used to light masked surfaces
  vuint32 LightPointAmbient (const TVec &p, float radius, const subsector_t *psub=nullptr);

  // world BSP rendering
  virtual void QueueWorldSurface (surface_t*) override;
  void RenderWorld (const refdef_t*, const VViewClipper*);

  void RenderTranslucentWallsDecals ();

  void AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, bool asShadow);

  void DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);
  void RenderShadowLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg);
  void RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox);
  void RenderShadowPolyObj (subsector_t *sub);
  void RenderShadowSubRegion (subsector_t *sub, subregion_t *region);
  void RenderShadowSubsector (int num);
  void RenderShadowBSPNode (int bspnum, const float *bbox, bool LimitLights);

  void DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                          VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);
  void RenderLightLine (sec_region_t *secregion, drawseg_t *dseg);
  void RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox);
  void RenderLightPolyObj (subsector_t *sub);
  void RenderLightSubRegion (subsector_t *sub, subregion_t *region);
  void RenderLightSubsector (int num);
  void RenderLightBSPNode (int bspnum, const float *bbox, bool LimitLights);
  void RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD, const VViewClipper *Range,
                           TVec &Pos, float Radius, float LightMin, vuint32 Color,
                           bool LimitLights,
                           TVec coneDir=TVec(0.0f, 0.0f, 0.0f), float coneAngle=0.0f);

  // things
  void BuildMobjsInCurrLight (bool doShadows);
  void RenderMobjsAmbient ();
  void RenderMobjsTextures ();
  void RenderMobjsLight ();
  void RenderMobjsShadow (VEntity *owner, vuint32 dlflags);
  void RenderMobjsFog ();

  inline bool IsTouchedByCurrLight (const VEntity *ent) const {
    const float clr = CurrLightRadius;
    //if (clr < 2) return false; // arbitrary number
    const TVec eofs = CurrLightPos-ent->Origin;
    const float edist = ent->Radius+clr;
    if (eofs.Length2DSquared() >= edist*edist) return false;
    // if light is higher than thing height, assume that the thing is not touched
    if (eofs.z >= clr+ent->Height) return false;
    // if light is lower than the thing, assume that the thing is not touched
    if (eofs.z <= -clr) return false;
    return true;
  }

public:
  VRenderLevelShadowVolume (VLevel *);
  ~VRenderLevelShadowVolume ();

  virtual void PreRender () override;

  virtual void BuildLightMap (surface_t *) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// r_model
void R_InitModels ();
void R_FreeModels ();

void R_LoadAllModelsSkins ();

int R_SetMenuPlayerTrans (int, int, int);


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<spritedef_t> sprites; //[MAX_SPRITE_MODELS];

// r_main
extern int screenblocks;

extern vuint8 light_remap[256];
extern VCvarB r_darken;
extern VCvarB r_dynamic_lights;
extern VCvarB r_static_lights;

extern VCvarI aspect_ratio;
extern VCvarB r_interpolate_frames;
extern VCvarB r_allow_shadows;

extern VTextureTranslation **TranslationTables;
extern int NumTranslationTables;
extern VTextureTranslation IceTranslation;
extern TArray<VTextureTranslation *> DecorateTranslations;
extern TArray<VTextureTranslation *> BloodTranslations;

extern vuint32 blocklightsr[VRenderLevelLightmap::LMapTraceInfo::GridSize*VRenderLevelLightmap::LMapTraceInfo::GridSize];
extern vuint32 blocklightsg[VRenderLevelLightmap::LMapTraceInfo::GridSize*VRenderLevelLightmap::LMapTraceInfo::GridSize];
extern vuint32 blocklightsb[VRenderLevelLightmap::LMapTraceInfo::GridSize*VRenderLevelLightmap::LMapTraceInfo::GridSize];


#endif
