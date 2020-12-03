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
#ifndef VAVOOM_R_LOCAL_HEADER
#define VAVOOM_R_LOCAL_HEADER

#include "../client/cl_local.h"
#include "r_shared.h"
#include "fmd2defs.h"

// this doesn't help much (not even 1 FPS usually), and it is still glitchy
//#define VV_CHECK_1S_CAST_SHADOW


// ////////////////////////////////////////////////////////////////////////// //
//#define MAX_SPRITE_MODELS  (10*1024)

// was 0.1
// moved to "r_fuzzalpha" cvar
//#define FUZZY_ALPHA  (0.7f)

#define BSPIDX_IS_LEAF(bidx_)         ((bidx_)&(NF_SUBSECTOR))
#define BSPIDX_IS_NON_LEAF(bidx_)     (!((bidx_)&(NF_SUBSECTOR)))
#define BSPIDX_LEAF_SUBSECTOR(bidx_)  ((bidx_)&(~(NF_SUBSECTOR)))


extern VCvarF gl_maxdist;
extern VCvarF r_lights_radius;
extern VCvarB r_models_strict;

extern VCvarB prof_r_world_prepare;
extern VCvarB prof_r_bsp_collect;
extern VCvarB prof_r_bsp_world_render;
extern VCvarB prof_r_bsp_mobj_render;

extern VCvarB r_shadowmaps;


static VVA_OKUNUSED inline bool IsAnyProfRActive () noexcept {
  return
    prof_r_world_prepare.asBool() ||
    prof_r_bsp_collect.asBool() ||
    prof_r_bsp_world_render.asBool() ||
    prof_r_bsp_mobj_render.asBool();
}


// ////////////////////////////////////////////////////////////////////////// //
// dynamic light types
enum DLType {
  DLTYPE_Point,
  DLTYPE_MuzzleFlash,
  DLTYPE_Pulse,
  DLTYPE_Flicker,
  DLTYPE_FlickerRandom,
  DLTYPE_Sector,
  //DLTYPE_Subtractive, // partially supported
  //DLTYPE_SectorSubtractive, // not supported
  DLTYPE_TypeMask = 0x1fu,
  // flags (so point light can actually be spotlight; sigh)
  DLTYPE_Subtractive = 0x20u,
  DLTYPE_Additive = 0x40u,
  DLTYPE_Spot = 0x80u,
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
  // fake flats
  float frontFakeFloorDist;
  float frontFakeCeilDist;
  float backFakeFloorDist;
  float backFakeCeilDist;
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
  SurfVertex __verts[3]; // so we have 4 of 'em here
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

  inline VSkyPortal (VRenderLevelShared *ARLev, VSky *ASky) : VPortal(ARLev), Sky(ASky) {}
  virtual bool NeedsDepthBuffer () const override;
  virtual bool IsSky () const override;
  virtual bool MatchSky (VSky *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyBoxPortal : public VPortal {
public:
  VEntity *Viewport;

  inline VSkyBoxPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) {}
  virtual bool IsSky () const override;
  virtual bool IsSkyBox () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSectorStackPortal : public VPortal {
public:
  VEntity *Viewport;

  inline VSectorStackPortal (VRenderLevelShared *ARLev, VEntity *AViewport) : VPortal(ARLev), Viewport(AViewport) {}
  virtual bool IsStack () const override;
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMirrorPortal : public VPortal {
public:
  TPlane *Plane;

  inline VMirrorPortal (VRenderLevelShared *ARLev, TPlane *APlane) : VPortal(ARLev), Plane(APlane) {}
  virtual bool IsMirror () const override;
  virtual bool MatchMirror (TPlane *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
enum ERenderPass {
  // for regular renderer
  RPASS_Normal,
  // for advanced renderer
  RPASS_Ambient, // render to ambient light texture
  RPASS_ShadowVolumes, // render shadow volumes
  RPASS_Light, // render lit model (sadly, uniformly lit yet)
  RPASS_Textures, // render model textures on top of ambient lighting
  RPASS_Fog, // render model darkening/fog
  RPASS_NonShadow, // render "simple" models that doesn't require complex lighting (additive, for example)
  RPASS_ShadowMaps, // render shadow maps
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShared : public VRenderLevelDrawer {
protected:
  friend class VPortal;
  friend class VSkyPortal;
  friend class VSkyBoxPortal;
  friend class VSectorStackPortal;
  friend class VMirrorPortal;
  friend struct AutoSavedView;

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

  struct world_surf_t {
    surface_t *Surf;
    vuint8 Type;
  };

  struct light_t {
    TVec origin;
    float radius;
    vuint32 color;
    //VEntity *dynowner; // for dynamic
    vuint32 ownerUId; // for static
    int leafnum;
    bool active; // for filtering
    TVec coneDirection;
    float coneAngle;
    // all subsectors touched by this static light
    // this is used to trigger static lightmap updates
    TArray<subsector_t *> touchedSubs;
    unsigned invalidateFrame; // to avoid double-processing lights; using `currDLightFrame`
    unsigned dlightframe; // set to `currDLightFrame` if BSP renderer touched any subsector that is connected with this light
    //TArray<polyobj_t *> touchedPolys;
  };

protected:
  struct DLightInfo {
    int needTrace; // <0: no; >1: yes; 0: invisible
    int leafnum; // -1: unknown yet

    inline bool isVisible () const noexcept { return (needTrace != 0); }
    inline bool isNeedTrace () const noexcept { return (needTrace > 0); }
  };

protected:
  VLevel *Level;
  VEntity *ViewEnt;
  VPortal *CurrPortal;

  //unsigned FrustumIndexes[5][6];
  int MirrorLevel;
  int PortalLevel;
  int VisSize;
  int SecVisSize;
  vuint8 *BspVis;
  vuint8 *BspVisSector; // for whole sectors

  subsector_t *r_viewleaf;
  subsector_t *r_oldviewleaf;
  float old_fov;
  int prev_aspect_ratio;
  bool prev_vertical_fov_flag;

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

  struct SubStaticLigtInfo {
    TMapNC<int, bool> touchedStatic;
    vuint32 invalidateFrame; // to avoid double-processing lights; using `currDLightFrame`
  };
  TArray<SubStaticLigtInfo> SubStaticLights; // length == Level->NumSubsectors

  // dynamic lights
  dlight_t DLights[MAX_DLIGHTS];
  DLightInfo dlinfo[MAX_DLIGHTS];
  TMapNC<vuint32, vuint32> dlowners; // key: object id; value: index in `DLights`

  // server uid -> object
  TMapNC<vuint32, VEntity *> suid2ent;

  // moved here so that model rendering methods can be merged
  TVec CurrLightPos;
  float CurrLightRadius;
  bool CurrLightInFrustum;
  bool CurrLightInFront;
  bool CurrLightCalcUnstuck; // set to `true` to calculate "unstuck" distance in `CalcLightVis()`
  TVec CurrLightUnstuckPos; // set in `CalcLightVis()` if `CurrLightCalcUnstuck` is `true`
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
  float bspVisLastCheckRadius;

  // `CalcLightVis()` working variables
  unsigned LightFrameNum;
  VViewClipper LightClip; // internal working clipper
  unsigned *LightVis; // this will be allocated if necessary; set to `LightFrameNum` for subsectors touched in `CalcLightVis()`
  unsigned *LightBspVis; // this will be allocated if necessary; set to `LightFrameNum` for subsectors touched, and marked in `BspVis`
  bool HasLightIntersection; // set by `CalcLightVisCheckNode()`: is any touched subsector also marked in `BspVis`?
  bool LitVisSubHit; // hit any subsector?
  // set `LitCalcBBox` to true to calculate bbox of all hit surfaces, and all following flags
  bool LitCalcBBox; // set this to `false` disables `LitSurfaces`/`LitSurfaceHit` calculation, and all other arrays
  TVec LitBBox[2]; // bounding box for all hit surfaces
  bool LitSurfaceHit; // hit any surface in visible subsector?
  // nope, not used for now
  //bool HasBackLit; // if there's no backlit surfaces, use zpass

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

  // automap
  TArray<sec_surface_t *> amSurfList;
  TArray<TVec> amTmpVerts;
  bool amDoFloors;
  VTexture *amSkyTex;
  AMCheckSubsectorCB amCheckSubsector;
  float amX, amY, amX2, amY2;

  inline bool AM_isBBox3DVisible (const float bbox3d[6]) const noexcept {
    return
      amX2 >= bbox3d[0+0] && amY2 >= bbox3d[0+1] &&
      amX <= bbox3d[3+0] && amY <= bbox3d[3+1];
  }

private:
  VDirtyArea unusedDirty; // required to return reference to it

private:
  inline segpart_t *SurfCreatorGetPSPart () {
    if (pspartsLeft == 0) Sys_Error("internal level surface creation bug");
    --pspartsLeft;
    return pspart++;
  }

protected:
  void NewBSPFloodVisibilityFrame () noexcept;

  bool CheckBSPFloodVisibilitySub (const TVec &org, const float radius, const subsector_t *currsub, const seg_t *firsttravel) noexcept;
  bool CheckBSPFloodVisibility (const TVec &org, float radius, const subsector_t *sub=nullptr) noexcept;

  // this destroys `CurrListPos` and `CurrLightRadius`
  bool CheckBSPVisibilityBoxSub (int bspnum, const float *bbox) noexcept;
  bool CheckBSPVisibilityBox (const TVec &org, float radius, const subsector_t *sub=nullptr) noexcept;

  void ResetVisFrameCount () noexcept;
  inline vuint32 IncVisFrameCount () noexcept {
    if ((++currVisFrame) == 0x7fffffff) ResetVisFrameCount();
    return currVisFrame;
  }

  void ResetDLightFrameCount () noexcept;
  inline int IncDLightFrameCount () noexcept {
    if ((++currDLightFrame) == 0xffffffff) ResetDLightFrameCount();
    return currDLightFrame;
  }

  void ResetUpdateWorldFrame () noexcept;
  inline void IncUpdateWorldFrame () noexcept {
    if ((++updateWorldFrame) == 0xffffffff) ResetUpdateWorldFrame();
  }

  inline void IncQueueFrameCount () noexcept {
    //if ((++currQueueFrame) == 0xffffffff) ResetQueueFrameCount();
    ++currQueueFrame;
    if (currQueueFrame == 0xffffffff) {
      // there is nothing we can do with this yet
      // resetting this is too expensive, and doesn't worth the efforts
      // i am not expecting for someone to play one level for that long in one seat
      Sys_Error("*************** WARNING!!! QUEUE FRAME COUNT OVERFLOW!!! ***************");
    }
  }

  inline void IncLightFrameNum () noexcept {
    if (++LightFrameNum == 0) {
      LightFrameNum = 1;
      memset(LightVis, 0, sizeof(LightVis[0])*Level->NumSubsectors);
      memset(LightBspVis, 0, sizeof(LightBspVis[0])*Level->NumSubsectors);
    }
  }

  // WARNING! NO CHECKS!
  VVA_CHECKRESULT inline bool IsSubsectorLitVis (int sub) const noexcept { return (LightVis[(unsigned)sub] == LightFrameNum); }
  VVA_CHECKRESULT inline bool IsSubsectorLitBspVis (int sub) const noexcept { return (LightBspVis[(unsigned)sub] == LightFrameNum); }

  // clears render queues
  virtual void ClearQueues ();

protected:
  // entity must not be `nullptr`, and must have `SubSector` set
  // also, `viewfrustum` should be valid here
  // this is usually called once for each entity, but try to keep it reasonably fast anyway
  bool IsThingVisible (VEntity *ent) const noexcept;

  /*
  static inline bool IsThingRenderable (VEntity *ent) noexcept {
    return
      ent && ent->State && !ent->IsGoingToDie() &&
      !(ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) &&
      ent->SubSector;
  }
  */

public:
  virtual bool IsNodeRendered (const node_t *node) const noexcept override;
  virtual bool IsSubsectorRendered (const subsector_t *sub) const noexcept override;

  void CalcEntityStaticLightingFromOwned (VEntity *lowner, const TVec &p, float radius, float height, float &l, float &lr, float &lg, float &lb);
  void CalcEntityDynamicLightingFromOwned (VEntity *lowner, const TVec &p, float radius, float height, float &l, float &lr, float &lg, float &lb);

  // defined only after `PushDlights()`
  // `radius` is used for visibility raycasts
  vuint32 LightPoint (VEntity *lowner, TVec p, float radius, float height, const subsector_t *psub=nullptr);
  // `radius` is used for... nothing yet
  vuint32 LightPointAmbient (VEntity *lowner, TVec p, float radius, float height, const subsector_t *psub=nullptr);

  virtual void UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced=false) override;

  virtual void PrecacheLevel () override;
  virtual void UncacheLevel () override;

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

  virtual int GetNumberOfStaticLights () override;

public:
  static vuint32 CountSurfacesInChain (const surface_t *s) noexcept;
  static vuint32 CountSegSurfacesInChain (const segpart_t *sp) noexcept;
  vuint32 CountAllSurfaces () const noexcept;

  static inline VName GetClassNameForModel (VEntity *mobj) noexcept {
    return
      mobj && mobj->State ?
        (r_models_strict ? mobj->GetClass()->Name : mobj->State->Outer->Name) :
        NAME_None;
  }

  // fuckery to avoid having friends, because i am asocial
  inline void CallTransformFrustum () { TransformFrustum(); }

protected:
  VRenderLevelShared (VLevel *ALevel);
  ~VRenderLevelShared ();

  void UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane=nullptr);
  void UpdateTextureOffsetsEx (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const side_tex_params_t *tparam2); // for 3d floors
  void UpdateDrawSeg (subsector_t *r_surf_sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void UpdateSubRegion (subsector_t *r_surf_sub, subregion_t *region);

  void UpdateFakeSectors (subsector_t *viewleaf=nullptr);
  void InitialWorldUpdate ();

  void UpdateBBoxWithSurface (TVec bbox[2], surface_t *surfs, const texinfo_t *texinfo,
                              VEntity *SkyBox, bool CheckSkyBoxAlways);
  void UpdateBBoxWithLine (TVec bbox[2], VEntity *SkyBox, const drawseg_t *dseg);

  // the following should not be called directly
  void CalcLightVisCheckNode (int bspnum, const float *bbox, const float *lightbbox);

  // main entry point for lightvis calculation
  // note that this should be called with filled `BspVis`
  // if we will use this for dynamic lights, they will have one-frame latency
  // sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
  // returns `false` if the light is invisible
  // this also sets the list of touched subsectors for dynamic light
  // (this is used in `LightPoint()` and such
  bool CalcLightVis (const TVec &org, const float radius, int dlnum=-1);

  // does some sanity checks, possibly moves origin a little
  // returns `false` if this light can be dropped
  // `sec` should be valid, null sector means "no checks"
  static bool CheckValidLightPosRough (TVec &lorg, const sector_t *sec);

  // yes, non-virtual
  // dlinfo::leafnum must be set (usually this is done in `PushDlights()`)
  //void MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum);

  virtual void RenderScene (const refdef_t *, const VViewClipper *) = 0;

  // this calculates final dlight visibility, list of affected subsectors, and so on
  void PushDlights ();

  // returns attenuation multiplier (0 means "out of cone")
  static float CheckLightPointCone (VEntity *lowner, const TVec &p, const float radius, const float height, const TVec &coneOrigin, const TVec &coneDir, const float coneAngle);

  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) = 0;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) = 0;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) = 0;
  virtual void QueueWorldSurface (surface_t *) = 0;
  virtual void FreeSurfCache (surfcache_t *&block);
  // this is called after surface queues built, so lightmap renderer can calculate new lightmaps
  // it is called right before starting world drawing
  virtual void ProcessCachedSurfaces ();

  void PrepareWorldRender (const refdef_t *, const VViewClipper *);
  // this should be called after `RenderCollectSurfaces()`
  void BuildVisibleObjectsList ();

  // general
  static float CalcEffectiveFOV (float fov, const refdef_t &refdef);
  void SetupRefdefWithFOV (refdef_t *refdef, float fov);

  void ExecuteSetViewSize ();
  void TransformFrustum ();
  void SetupFrame ();
  void SetupCameraFrame (VEntity *, VTexture *, int, refdef_t *);
  void MarkLeaves ();
  bool UpdateCameraTexture (VEntity *Camera, int TexNum, int FOV); // returns `true` if camera texture was updated
  vuint32 GetFade (sec_region_t *reg, bool isFloorCeiling=false);
  int CollectSpriteTextures (TArray<bool> &texturepresent); // this is actually private, but meh... returns number of new textures
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
  //void QueueTranslucentSurf (surface_t *surf);

  // WARNING: `SurfPrepareForRender()` should be already called!
  void QueueSimpleSurf (surface_t *surf);

  void QueueSkyPortal (surface_t *surf);
  void QueueHorizonPortal (surface_t *surf);

  // main dispatcher (it calls other queue methods)
  enum SFCType {
    SFCT_World,
    SFCT_Sky,
    SFCT_Horizon,
  };
  void CommonQueueSurface (surface_t *surf, SFCType type);

  void DrawSurfaces (subsector_t *sub, sec_region_t *secregion, seg_t *seg, surface_t *InSurfs,
                     texinfo_t *texinfo, VEntity *SkyBox, int LightSourceSector, int SideLight,
                     bool AbsSideLight, bool CheckSkyBoxAlways);

  void GetFlatSetToRender (subsector_t *sub, subregion_t *region, sec_surface_t *surfs[4]);
  void ChooseFlatSurfaces (sec_surface_t *&f0, sec_surface_t *&f1, sec_surface_t *flat0, sec_surface_t *flat1);

  // used in `RenderSubRegion()` and other similar methods
  bool NeedToRenderNextSubFirst (const subregion_t *region) noexcept;

  void RenderHorizon (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg);
  void RenderMirror (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg);
  void RenderLine (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg);
  void RenderSecFlatSurfaces (subsector_t *sub, sec_region_t *secregion, sec_surface_t *flat0, sec_surface_t *flat1, VEntity *SkyBox);
  void RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox);
  void AddPolyObjToClipper (VViewClipper &clip, subsector_t *sub);
  void RenderPolyObj (subsector_t *sub);
  void RenderSubRegion (subsector_t *sub, subregion_t *region);
  void RenderSubsector (int num, bool onlyClip);
  void RenderBSPNode (int bspnum, const float *bbox, unsigned AClipflags, bool onlyClip=false);
  void RenderBspWorld (const refdef_t *, const VViewClipper *);
  void RenderPortals ();

  void CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec wv[4], vuint32 typeFlags, bool doOffset=false);

  void SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                   TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);

  // surf methods
  void SetupSky ();
  void FlushSurfCaches (surface_t *InSurfs);
  // `ssurf` can be `nullptr`, and it will be allocated, otherwise changed
  // this is used both to create initial surfaces, and to update changed surfaces
  sec_surface_t *CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane, subregion_t *sreg, bool fake=false);
  // subsector is not changed, but we need it non-const
  //enum { USS_Normal, USS_Force, USS_IgnoreCMap, USS_ForceIgnoreCMap };
  void UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, subregion_t *sreg, bool ignoreColorMap=false, bool fake=false);
  surface_t *NewWSurf ();
  void FreeWSurfs (surface_t *&);
  surface_t *CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags);
  int CountSegParts (const seg_t *);
  void CreateSegParts (subsector_t *r_surf_sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg, bool isMainRegion);
  void CreateWorldSurfaces ();
  bool CopyPlaneIfValid (sec_plane_t *, const sec_plane_t *, const sec_plane_t *);
  void UpdateFakeFlats (sector_t *);
  void UpdateDeepWater (sector_t *);
  void UpdateFloodBug (sector_t *sec);
  // free all surfaces except the first one, clear first, set number of vertices to vcount
  surface_t *ReallocSurface (surface_t *surfs, int vcount);
  void FreeSurfaces (surface_t *);
  void FreeSegParts (segpart_t *);

  // models
  bool DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
    float ScaleX, float ScaleY, VModel *Mdl,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
    VTextureTranslation *Trans, int Version,
    const RenderStyleInfo &ri,
    bool IsViewModel, float Inter, bool Interpolate,
    ERenderPass Pass);

  bool DrawAliasModel (VEntity *mobj, VName clsName, const TVec &Org, const TAVec &Angles,
    float ScaleX, float ScaleY,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState *State, VState *NextState,
    VTextureTranslation *Trans, int Version,
    const RenderStyleInfo &ri,
    bool IsViewModel, float Inter, bool Interpolate,
    ERenderPass Pass);

  bool DrawEntityModel (VEntity *Ent, const RenderStyleInfo &ri, float Inter, ERenderPass Pass);

  bool CheckAliasModelFrame (VEntity *Ent, float Inter);
  bool HasEntityAliasModel (VEntity *Ent) const;
  static bool IsAliasModelAllowedFor (VEntity *Ent);
  bool IsShadowAllowedFor (VEntity *Ent);

  // things
  void QueueTranslucentSurface (surface_t *surf, const RenderStyleInfo &ri);

  void QueueSpritePoly (const TVec *sv, int lump, const RenderStyleInfo &ri, int translation,
                        const TVec &normal, float pdist, const TVec &saxis, const TVec &taxis,
                        const TVec &texorg, int priority, const TVec &sprOrigin, vuint32 objid);

  void QueueSprite (VEntity *thing, RenderStyleInfo &ri, bool onlyShadow=false); // this can modify `ri`!
  void QueueTranslucentAliasModel (VEntity *mobj, const RenderStyleInfo &ri, float TimeFrac);
  bool RenderAliasModel (VEntity *mobj, const RenderStyleInfo &ri, ERenderPass Pass);
  void RenderThing (VEntity *, ERenderPass);
  void RenderMobjs (ERenderPass);
  void DrawTranslucentPolys ();
  void RenderPSprite (VViewState *VSt, const VAliasModelFrameInfo &mfi, float PSP_DIST, const RenderStyleInfo &ri);
  bool RenderViewModel (VViewState *VSt, const RenderStyleInfo &ri);
  void DrawPlayerSprites ();
  void DrawCrosshair ();

  // used in things rendering to calculate lighting in `ri`
  void SetupRIThingLighting (VEntity *ent, RenderStyleInfo &ri, bool asAmbient, bool allowBM);

  // used in sprite lighting checks
  bool RadiusCastRay (bool textureCheck, sector_t *sector, const TVec &org, sector_t *destsector, const TVec &dest, float radius);

protected:
  virtual void RefilterStaticLights ();

  // used to get a floor to sample lightmap
  // WARNING! can return `nullptr`!
  sec_surface_t *GetNearestFloor (const subsector_t *sub, const TVec &p);

  // this is common code for light point calculation
  // pass light values from ambient pass
  void CalculateDynLightSub (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height);

  // calculate subsector's ambient light (light variables must be initialized)
  void CalculateSubAmbient (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius);

  // calculate subsector's light from static light sources (light variables must be initialized)
  void CalculateSubStatic (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height);

  void CalcBSPNodeLMaps (int slindex, light_t &sl, int bspnum, const float *bbox);
  void CalcStaticLightTouchingSubs (int slindex, light_t &sl);

  // called when some surface from the given subsector changed
  // invalidates lightmaps for all touching lights
  virtual void InvalidateStaticLightmapsSubs (subsector_t *sub);

  virtual void InvalidateStaticLightmaps (const TVec &org, float radius, bool relight);

public:
  virtual particle_t *NewParticle (const TVec &porg) override;

  virtual int GetStaticLightCount () const noexcept override;
  virtual LightInfo GetStaticLight (int idx) const noexcept override;

  virtual int GetDynamicLightCount () const noexcept override;
  virtual LightInfo GetDynamicLight (int idx) const noexcept override;

  virtual void RenderPlayerView () override;

  virtual void SegMoved (seg_t *) override;
  virtual void SetupFakeFloors (sector_t *) override;

  virtual void ResetStaticLights () override;
  virtual void AddStaticLightRGB (vuint32 OwnerUId, const TVec &origin, float radius, vuint32 color, TVec coneDirection=TVec(0,0,0), float coneAngle=0.0f) override;
  virtual void MoveStaticLightByOwner (vuint32 OwnerUId, const TVec &origin) override;
  virtual void RemoveStaticLightByOwner (vuint32 OwnerUId) override;

  void RemoveStaticLightByIndex (int slidx);

  virtual void ClearReferences () override;

  virtual dlight_t *AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid=-1) override;
  virtual dlight_t *FindDlightById (int lightid) override;
  virtual void DecayLights (float) override;

  virtual void RegisterAllThinkers () override;
  virtual void ThinkerAdded (VThinker *Owner) override;
  virtual void ThinkerDestroyed (VThinker *Owner) override;

  virtual void ResetLightmaps (bool recalcNow) override;

  virtual bool isNeedLightmapCache () const noexcept override;
  virtual void saveLightmaps (VStream *strm) override;
  virtual bool loadLightmaps (VStream *strm) override;

public: // automap
  virtual void RenderTexturedAutomap (
    float m_x, float m_y, float m_x2, float m_y2,
    bool doFloors, // floors or ceiling?
    float alpha,
    AMCheckSubsectorCB CheckSubsector,
    AMIsHiddenSubsectorCB IsHiddenSubsector,
    AMMapXYtoFBXYCB MapXYtoFBXY
  ) override;

private: // automap
  static sec_surface_t *AM_getFlatSurface (subregion_t *reg, bool doFloors);
  void amFlatsCheckSubsector (int num);
  void amFlatsCheckNode (int bspnum);

  void amFlatsCollectSurfaces ();

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
class VRenderLevelLightmap;

class VLMapCache : public V2DCache<surfcache_t> {
public:
  VRenderLevelLightmap *renderer;

public:
  VLMapCache () noexcept : V2DCache<surfcache_t>(), renderer(nullptr) {}

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
  // list of all surfaces with lightmaps; used only in `ProcessCachedSurfaces()`
  // surfaces from this list will be put either to lightmap chains, or to normal lists
  TArray<surface_t *> LMSurfList;
  bool nukeLightmapsOnNextFrame;

  bool invalidateRelight;

  // temporary flag for lightmap cache loader
  // set if some surface wasn't found
  unsigned lmcacheUnknownSurfaceCount;

  // this is used to limit static lightmap recalc time
  vuint32 lastLMapStaticRecalcFrame;
  double lmapStaticRecalcTimeLeft; // <0: no limit

public:
  void releaseAtlas (vuint32 id) noexcept;
  void allocAtlas (vuint32 aid) noexcept;

  // returns `true` if expired
  bool IsStaticLightmapTimeLimitExpired ();

private:
  // returns `false` if cannot allocate lightmap block
  bool BuildSurfaceLightmap (surface_t *surface);

public:
  struct LMapTraceInfo {
  public:
    enum { GridSize = 18 };
    enum { MaxSurfPoints = GridSize*GridSize*4/*16*/ }; // *4 for extra filtering

  public:
    TVec smins, smaxs;
    TVec worldtotex[2];
    TVec textoworld[2];
    TVec texorg;
    TVec surfpt[MaxSurfPoints];
    int numsurfpt;
    bool pointsCalced;
    bool light_hit;
    bool didExtra;
    // for spotlights
    bool spotLight;
    float coneAngle;
    TVec coneDir;

  public:
    VV_DISABLE_COPY(LMapTraceInfo)
    inline LMapTraceInfo () noexcept { memset((void *)this, 0, sizeof(LMapTraceInfo)); }

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

  void InvalidateStaticLightmapsSurfaces (surface_t *surf);
  void InvalidateStaticLightmapsLine (drawseg_t *dseg);
  void InvalidateStaticLightmapsSubsector (subsector_t *sub);

  // called when some surface from the given subsector changed
  // invalidates lightmaps for all touching lights
  virtual void InvalidateStaticLightmapsSubs (subsector_t *sub) override;

  virtual void InvalidateStaticLightmaps (const TVec &org, float radius, bool relight) override;

  // general
  virtual void RenderScene (const refdef_t *, const VViewClipper *) override;

  // surf methods
  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) override;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) override;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) override;

  // light methods
  // cast light ray
  // returns `false` if cannot reach
  //   `dist` will be set to distance (zero means "too far away"); can be `nullptr`
  bool CastStaticRay (float *dist, sector_t *srcsector, const TVec &p1, const TVec &p2, float squaredist);
  static void CalcMinMaxs (LMapTraceInfo &lmi, const surface_t *surf);
  static bool CalcFaceVectors (LMapTraceInfo &lmi, const surface_t *surf);
  void CalcPoints (LMapTraceInfo &lmi, const surface_t *surf, bool lowres); // for dynlights, set `lowres` to `true`
  void SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf, const vuint8 *facevis);
  void AddDynamicLights (surface_t *surf);

  // lightmap cache manager
  // WARNING: `SurfPrepareForRender()` should be already called!
  bool QueueLMapSurface (surface_t *); // returns `true` if surface was queued

  void FlushCaches ();
  virtual void FreeSurfCache (surfcache_t *&block) override;
  virtual void ProcessCachedSurfaces () override;

  // world BSP rendering
  virtual void QueueWorldSurface (surface_t *) override;
  // this does BSP traversing, and collect world surfaces into various lists to drive GPU rendering
  void RenderCollectSurfaces (const refdef_t *rd, const VViewClipper *Range);

  void RelightMap (bool recalcNow, bool onlyMarked);

public:
  // this is fast and rough check
  bool CanFaceBeStaticallyLit (surface_t *surf);

  // this has to be public for now
  // this method calculates static lightmap for a surface
  void LightFace (surface_t *surf);

  // this is called from BSP renderer
  // you can use `surf->NeedRecalcStaticLightmap()` to check success
  void LightFaceTimeCheckedFreeCaches (surface_t *surf);

public:
  VRenderLevelLightmap (VLevel *);

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
  int DynLightsRendered;
  // set this to true before calling `RenderLightShadows()` to indicate dynamic light
  bool DynamicLights;

  // keep 'em here, so we don't have to traverse BSP several times
  TArray<surface_t *> smapSurfaces;

  TArray<surface_t *> shadowSurfaces;
  TArray<surface_t *> lightSurfaces;

  // used to avoid double-checking; sized by NumSectors
  struct FlatSectorShadowInfo {
    enum {
      NoFloor   = 1u<<0,
      NoCeiling = 1u<<1,
    };
    unsigned renderFlag;
    unsigned frametag;
  };

  TArray<FlatSectorShadowInfo> fsecCheck;

  #ifdef VV_CHECK_1S_CAST_SHADOW
  struct Line1SShadowInfo {
    int canShadow;
    unsigned frametag;
  };

  TArray<Line1SShadowInfo> flineCheck;
  #endif

  unsigned fsecCounter;

  inline unsigned fsecCounterGen () noexcept {
    if ((++fsecCounter) == 0 || fsecCheck.length() != Level->NumSectors
        #ifdef VV_CHECK_1S_CAST_SHADOW
        || flineCheck.length() != Level->NumLines
        #endif
       ) {
      if (fsecCheck.length() != Level->NumSectors) fsecCheck.setLength(Level->NumSectors);
      for (auto &&it : fsecCheck) it.frametag = 0;
      #ifdef VV_CHECK_1S_CAST_SHADOW
      if (flineCheck.length() != Level->NumLines) flineCheck.setLength(Level->NumLines);
      for (auto &&it : flineCheck) it.frametag = 0;
      #endif
      fsecCounter = 1;
    }
    return fsecCounter;
  }

  // to avoid checking sectors twice in `CheckShadowingFlats`
  TArray<unsigned> fsecSeenSectors;
  unsigned fsecSeenSectorsCounter;

  inline unsigned fsecSeenSectorsGen () noexcept {
    if ((++fsecSeenSectorsCounter) == 0 || fsecSeenSectors.length() != Level->NumSectors) {
      if (fsecSeenSectors.length() != Level->NumSectors) fsecSeenSectors.setLength(Level->NumSectors);
      for (auto &&it : fsecSeenSectors) it = 0;
      fsecSeenSectorsCounter = 1;
    }
    return fsecSeenSectorsCounter;
  }


  // returns `renderFlag`
  unsigned CheckShadowingFlats (subsector_t *sub);

  #ifdef VV_CHECK_1S_CAST_SHADOW
  bool CheckCan1SCastShadow (line_t *line);
  #endif

protected:
  virtual void RefilterStaticLights () override;

  // general
  virtual void RenderScene (const refdef_t *, const VViewClipper *) override;

  // surf methods
  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) override;
  virtual surface_t *SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) override;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) override;

  virtual void ProcessCachedSurfaces () override;

  // world BSP rendering
  virtual void QueueWorldSurface (surface_t *) override;
  // this does BSP traversing, and collect world surfaces into various lists to drive GPU rendering
  void RenderCollectSurfaces (const refdef_t *rd, const VViewClipper *Range);

  void AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, bool asShadow);

  // we can collect surfaces for lighting and shadowing in one pass
  // don't forget to reset `shadowSurfaces` and `lightSurfaces`
  void CollectAdvLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross,
                                unsigned int ssflag);
  void CollectAdvLightLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg, unsigned int ssflag);
  void CollectAdvLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox, unsigned int ssflag);
  void CollectAdvLightPolyObj (subsector_t *sub, unsigned int ssflag);
  void CollectAdvLightSubRegion (subsector_t *sub, subregion_t *region, unsigned int ssflag);
  void CollectAdvLightSubsector (int num);
  void CollectAdvLightBSPNode (int bspnum, const float *bbox, bool LimitLights);

  void RenderShadowSurfaceList ();
  void RenderLightSurfaceList ();

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
  // WARNING! may modify `Pos`
  void RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD, const VViewClipper *Range,
                           TVec &Pos, float Radius, float LightMin, vuint32 Color,
                           bool LimitLights,
                           TVec coneDir=TVec(0.0f, 0.0f, 0.0f), float coneAngle=0.0f, bool forceRender=false);

  // things
  void BuildMobjsInCurrLight (bool doShadows);

  void RenderMobjsAmbient ();
  void RenderMobjsTextures ();
  void RenderMobjsLight (VEntity *owner);
  void RenderMobjsShadow (VEntity *owner, vuint32 dlflags);
  void RenderMobjsShadowMap (VEntity *owner, vuint32 dlflags);
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
extern int screenblocks; // viewport size

extern vuint8 light_remap[256];
extern VCvarB r_darken;
extern VCvarB r_dynamic_lights;
extern VCvarB r_static_lights;

extern VCvarI r_aspect_ratio;
extern VCvarB r_interpolate_frames;
extern VCvarB r_allow_shadows;

extern VCvarB r_vertical_fov;

extern VCvarB r_models;
extern VCvarB r_models_view;
//extern VCvarB r_models_strict;

extern VCvarB r_models_monsters;
extern VCvarB r_models_corpses;
extern VCvarB r_models_missiles;
extern VCvarB r_models_pickups;
extern VCvarB r_models_decorations;
extern VCvarB r_models_other;
extern VCvarB r_models_players;

extern VCvarB r_model_shadows;
extern VCvarB r_camera_player_shadows;
extern VCvarB r_shadows_monsters;
extern VCvarB r_shadows_corpses;
extern VCvarB r_shadows_missiles;
extern VCvarB r_shadows_pickups;
extern VCvarB r_shadows_decorations;
extern VCvarB r_shadows_other;
extern VCvarB r_shadows_players;

extern VCvarB r_separate_translucent_lists;

extern VCvarB dbg_show_lightmap_cache_messages;

extern VCvarB dbg_dlight_vis_check_messages;
extern VCvarF r_dynamic_light_vis_check_radius_tolerance;
extern VCvarB r_vis_check_flood;
extern VCvarF r_fade_mult_regular;
extern VCvarF r_fade_mult_advanced;

extern VCvarB r_dbg_lightbulbs_static;
extern VCvarB r_dbg_lightbulbs_dynamic;
extern VCvarF r_dbg_lightbulbs_zofs_static;
extern VCvarF r_dbg_lightbulbs_zofs_dynamic;

extern VTextureTranslation **TranslationTables;
extern int NumTranslationTables;
extern VTextureTranslation IceTranslation;
extern TArray<VTextureTranslation *> DecorateTranslations;
extern TArray<VTextureTranslation *> BloodTranslations;

extern double dbgCheckVisTime;


void R_DrawLightBulb (const TVec &Org, const TAVec &Angles, vuint32 rgbLight, ERenderPass Pass, bool isShadowVol, float ScaleX=1.0f, float ScaleY=1.0);

// used to check for view models
bool R_HaveClassModelByName (VName clsName);


#endif
