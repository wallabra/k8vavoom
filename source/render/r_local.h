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
#ifndef VAVOOM_R_LOCAL_HEADER
#define VAVOOM_R_LOCAL_HEADER

#include "cl_local.h"
#include "r_shared.h"


#define MAX_SPRITE_MODELS 10*1024

// was 0.1
#define FUZZY_ALPHA  (0.7f)


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
};


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
  short lump[16];
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
};


struct sec_surface_t {
  sec_plane_t *secplane;
  texinfo_t texinfo;
  float dist;
  float XScale;
  float YScale;
  float Angle;
  surface_t *surfs;
};


struct fakefloor_t {
  sec_plane_t floorplane;
  sec_plane_t ceilplane;
  sec_params_t params;
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


struct decal_t {
  enum {
    SlideFloor = 0x0001U, // curz: offset with floorz, slide with it
    SlideCeil = 0x0002U, // curz: offset with ceilingz, slide with it
    FlipX = 0x0010U,
    FlipY = 0x0020U,
    Fullbright = 0x0100U,
    Fuzzy = 0x0200U,
    SideDefOne = 0x0800U,
  };
  decal_t *next; // in this seg
  seg_t *seg;
  sector_t *bsec; // backsector for SlideXXX
  VName dectype;
  //VName picname;
  VTextureID texture;
  vuint32 flags;
  float orgz; // original z position
  float curz; // z position (offset with floor/ceiling TexZ if not midtex, see `flags`)
  float xdist; // in pixels
  float linelen; // so we don't have to recalculate it in renderer
  float ofsX, ofsY; // for animators
  float origScaleX, origScaleY; // for animators
  float scaleX, scaleY; // actual
  float origAlpha; // for animators
  float alpha; // decal alpha
  float addAlpha; // alpha for additive translucency (not supported yet)
  VDecalAnim *animator; // decal animator (can be nullptr)
  decal_t *prevanimated; // so we can skip static decals
  decal_t *nextanimated; // so we can skip static decals
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
  virtual bool MatchSkyBox (VEntity *) const override;
  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMirrorPortal : public VPortal {
public:
  TPlane *Plane;

  VMirrorPortal (VRenderLevelShared *ARLev, TPlane *APlane) : VPortal(ARLev), Plane(APlane) { stackedSector = false; }
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
    vuint32 colour;
    int leafnum;
    bool active; // for filtering
  };

protected:
  struct DLightInfo {
    int needTrace; // <0: no; >1: yes; 0: don't know
  };

protected:
  VLevel *Level;

  VEntity *ViewEnt;

  unsigned FrustumIndexes[5][6];
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
  int ColourMap;

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

  // light variables
  TArray<light_t> Lights;
  dlight_t DLights[MAX_DLIGHTS];
  DLightInfo dlinfo[MAX_DLIGHTS];
  TMapNC<vuint64, vuint32> dlowners; // key: pointer; value: index

  // only regular renderer needs this
  vuint32 cacheframecount;

  // moved here so that model rendering methods can be merged
  TVec CurrLightPos;
  float CurrLightRadius;
  int CurrLightsNumber;
  int CurrShadowsNumber;

  // used in `AllocDlight()` to save one call to `PointInSubsector()`
  // reset in `RenderPlayerView()`
  TVec lastDLightView;
  subsector_t *lastDLightViewSub;

  bool showCreateWorldSurfProgress;

  bool updateWorldCheckVisFrame; // `true` for regular, `false` for advanced

protected:
  VRenderLevelShared (VLevel *ALevel);
  ~VRenderLevelShared ();

  void UpdateSubsector (int num, float *bbox);
  void UpdateBSPNode (int bspnum, float *bbox);
  void UpdateWorld (const refdef_t *rd, const VViewClipper *Range);

  virtual void RenderScene (const refdef_t *, const VViewClipper *) = 0;
  virtual void PushDlights () = 0;
  virtual vuint32 LightPoint (const TVec &p, VEntity *mobj) = 0;
  virtual void InitSurfs (surface_t*, texinfo_t*, TPlane*, subsector_t*) = 0;
  virtual surface_t *SubdivideFace (surface_t*, const TVec&, const TVec*) = 0;
  virtual surface_t *SubdivideSeg (surface_t*, const TVec&, const TVec*) = 0;
  virtual void QueueWorldSurface (seg_t*, surface_t*) = 0;
  virtual void FreeSurfCache (surfcache_t*);
  virtual bool CacheSurface (surface_t*);

  // general
  void ExecuteSetViewSize ();
  // create frustum planes for current FOV (set in `SetupFrame()` or `SetupCameraFrame()`)
  // [0] is left, [1] is right, [2] is top, [3] is bottom, [4] is back (if `createbackplane` is true)
  void TransformFrustumTo (TClipPlane *frustum, const TVec &org, const TAVec &angles, bool createbackplane);
  void TransformFrustum ();
  void SetupFrame ();
  void SetupCameraFrame (VEntity*, VTexture*, int, refdef_t*);
  void MarkLeaves ();
  void UpdateCameraTexture (VEntity*, int, int);
  vuint32 GetFade (sec_region_t*);
  void PrecacheLevel ();
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

  // world BSP rendering
  void SetUpFrustumIndexes ();
  void QueueSimpleSurf (seg_t*, surface_t*);
  void QueueSkyPortal (surface_t*);
  void QueueHorizonPortal (surface_t*);
  void DrawSurfaces (seg_t*, surface_t*, texinfo_t*, VEntity*, int, int, bool, bool);
  void RenderHorizon (drawseg_t*);
  void RenderMirror (drawseg_t*);
  void RenderLine (drawseg_t*);
  void RenderSecSurface (sec_surface_t*, VEntity*);
  void RenderSubRegion (subregion_t*);
  void RenderSubsector (int);
  void RenderBSPNode (int bspnum, const float *bbox, unsigned AClipflags);
  void RenderBspWorld (const refdef_t*, const VViewClipper*);
  void RenderPortals ();

  // surf methods
  void SetupSky ();
  void FlushSurfCaches (surface_t*);
  sec_surface_t *CreateSecSurface (subsector_t*, sec_plane_t*);
  void UpdateSecSurface (sec_surface_t*, sec_plane_t*, subsector_t*);
  surface_t *NewWSurf ();
  void FreeWSurfs (surface_t*);
  surface_t *CreateWSurfs (TVec*, texinfo_t*, seg_t*, subsector_t*);
  int CountSegParts (seg_t*);
  void CreateSegParts (drawseg_t*, seg_t*);
  void UpdateRowOffset (segpart_t*, float);
  void UpdateTextureOffset (segpart_t*, float);
  void UpdateDrawSeg (drawseg_t*, bool);
  void CreateWorldSurfaces ();
  void UpdateSubRegion (subregion_t*, bool);
  bool CopyPlaneIfValid (sec_plane_t*, const sec_plane_t*, const sec_plane_t*);
  void UpdateFakeFlats (sector_t*);
  void UpdateDeepWater (sector_t*);
  void UpdateFloodBug (sector_t *sec);
  void FreeSurfaces (surface_t*);
  void FreeSegParts (segpart_t*);

  // models
  bool DrawAliasModel (const TVec&, const TAVec&, float, float, VModel*,
    //int, int,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
    VTextureTranslation*, int, vuint32, vuint32, float, bool,
    bool, float, bool, ERenderPass);
  bool DrawAliasModel (VName clsName, const TVec&, const TAVec&, float, float,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState*, VState*,
    VTextureTranslation*, int, vuint32, vuint32, float, bool,
    bool, float, bool, ERenderPass);
  bool DrawEntityModel (VEntity*, vuint32, vuint32, float, bool, float, ERenderPass);
  bool CheckAliasModelFrame (VEntity *Ent, float Inter);

  // things
  void DrawTranslucentPoly (surface_t *surf, TVec *sv, int count, int lump,
                            float Alpha, bool Additive, int translation,
                            bool isSprite, vuint32 light, vuint32 Fade, const TVec &normal, float pdist,
                            const TVec &saxis, const TVec &taxis, const TVec &texorg, int priority=0,
                            bool useSprOrigin=false, const TVec &sprOrigin=TVec());
  void RenderSprite (VEntity*, vuint32, vuint32, float, bool);
  void RenderTranslucentAliasModel (VEntity*, vuint32, vuint32, float, bool, float);
  bool RenderAliasModel (VEntity*, vuint32, vuint32, float, bool, ERenderPass);
  void RenderThing (VEntity*, ERenderPass);
  void RenderMobjs (ERenderPass);
  void DrawTranslucentPolys ();
  void RenderPSprite (VViewState*, const VAliasModelFrameInfo, float, vuint32, vuint32, float, bool);
  bool RenderViewModel (VViewState*, vuint32, vuint32, float, bool);
  void DrawPlayerSprites ();
  void DrawCrosshair ();

  virtual void GentlyFlushAllCaches () {}

  // used in light checking
  bool RadiusCastRay (const TVec &org, const TVec &dest, float radius, bool advanced);

public:
  virtual particle_t *NewParticle (const TVec &porg) override;

  virtual void RenderPlayerView () override;

  virtual void SegMoved (seg_t *) override;
  virtual void SetupFakeFloors (sector_t *) override;

  virtual void AddStaticLight (const TVec&, float, vuint32) override;
  virtual dlight_t *AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid=-1) override;
  virtual void DecayLights (float) override;
  virtual void RemoveOwnedLight (VThinker *Owner) override;

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

  static VCvarB times_render_highlevel;
  static VCvarB times_render_lowlevel;
  static VCvarB r_disable_world_update;
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevel : public VRenderLevelShared {
private:
  int c_subdivides;
  int c_seg_div;

  // surface cache
  surfcache_t *freeblocks;
  surfcache_t *cacheblocks[NUM_BLOCK_SURFS];
  surfcache_t blockbuf[NUM_CACHE_BLOCKS];

protected:
  // general
  virtual void RenderScene (const refdef_t *, const VViewClipper *) override;

  // surf methods
  virtual void InitSurfs (surface_t*, texinfo_t*, TPlane*, subsector_t*) override;
  virtual surface_t *SubdivideFace (surface_t*, const TVec&, const TVec*) override;
  virtual surface_t *SubdivideSeg (surface_t*, const TVec&, const TVec*) override;

  // light methods
  static void CalcMinMaxs (surface_t *surf);
  float CastRay (const TVec &p1, const TVec &p2, float squaredist);
  static bool CalcFaceVectors (surface_t *surf);
  void CalcPoints (surface_t *surf);
  void SingleLightFace (light_t *light, surface_t *surf, const vuint8 *facevis);
  void LightFace (surface_t *surf, subsector_t *leaf);
  void MarkLights (dlight_t *light, vuint32 bit, int bspnum);
  void AddDynamicLights (surface_t *surf);
  virtual void PushDlights () override;

  void FlushCaches ();
  void FlushOldCaches ();
  virtual void GentlyFlushAllCaches () override;
  surfcache_t *AllocBlock (int, int);
  surfcache_t *FreeBlock (surfcache_t*, bool);
  virtual void FreeSurfCache (surfcache_t*) override;
  virtual bool CacheSurface (surface_t*) override;

  // world BSP rendering
  virtual void QueueWorldSurface (seg_t*, surface_t*) override;
  void RenderWorld (const refdef_t*, const VViewClipper*);

public:
  VRenderLevel (VLevel*);

  virtual void PreRender () override;

  virtual vuint32 LightPoint (const TVec &p, VEntity *mobj) override;
  virtual void BuildLightMap (surface_t *) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VAdvancedRenderLevel : public VRenderLevelShared {
private:
  VViewClipper LightClip;
  vuint8 *LightVis;
  vuint8 *LightBspVis;
  vuint32 CurrLightColour;
  TArray<VEntity *> mobjAffected; // built in `ResetMobjsLightCount()`

protected:
  void RefilterStaticLights ();

  // general
  virtual void RenderScene (const refdef_t*, const VViewClipper*) override;

  // surf methods
  virtual void InitSurfs (surface_t*, texinfo_t*, TPlane*, subsector_t*) override;
  virtual surface_t *SubdivideFace (surface_t*, const TVec&, const TVec*) override;
  virtual surface_t *SubdivideSeg (surface_t*, const TVec&, const TVec*) override;

  // light methods
  virtual void PushDlights () override;
  vuint32 LightPointAmbient (const TVec &p, VEntity *mobj);

  // world BSP rendering
  virtual void QueueWorldSurface (seg_t*, surface_t*) override;
  void RenderWorld (const refdef_t*, const VViewClipper*);

  void BuildLightVis (int bspnum, float *bbox);
  void DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo, bool CheckSkyBoxAlways, bool LightCanCross);
  void RenderShadowLine (drawseg_t *dseg);
  void RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox);
  void RenderShadowSubRegion (subregion_t *region);
  void RenderShadowSubsector (int num);
  void RenderShadowBSPNode (int bspnum, float *bbox, bool LimitLights);
  void DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                          VEntity *SkyBox, bool CheckSkyBoxAlways, bool LightCanCross);
  void RenderLightLine (drawseg_t *dseg);
  void RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox);
  void RenderLightSubRegion (subregion_t *region);
  void RenderLightSubsector (int num);
  void RenderLightBSPNode (int bspnum, float *bbox, bool LimitLights);
  void RenderLightShadows (const refdef_t *RD, const VViewClipper *Range,
                           TVec &Pos, float Radius, vuint32 Colour, bool LimitLights);

  // things
  void ResetMobjsLightCount (bool first); // if `first` is true, build array of affected entities
  void RenderThingAmbient (VEntity*);
  void RenderMobjsAmbient ();
  void RenderThingTextures (VEntity*);
  void RenderMobjsTextures ();
  bool IsTouchedByLight (VEntity*, bool);
  void RenderThingLight (VEntity*);
  void RenderMobjsLight ();
  void RenderThingShadow (VEntity*);
  void RenderMobjsShadow ();
  void RenderThingFog (VEntity*);
  void RenderMobjsFog ();

public:
  VAdvancedRenderLevel (VLevel *);
  ~VAdvancedRenderLevel ();

  virtual void PreRender () override;

  virtual vuint32 LightPoint (const TVec &p, VEntity *mobj) override;
  virtual void BuildLightMap (surface_t *) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// r_sky
void R_InitSkyBoxes ();

// r_model
void R_InitModels ();
void R_FreeModels ();

int R_SetMenuPlayerTrans (int, int, int);


// ////////////////////////////////////////////////////////////////////////// //
extern spritedef_t sprites[MAX_SPRITE_MODELS];

// r_main
extern int screenblocks;
extern int r_visframecount;

extern vuint8 light_remap[256];
extern VCvarB r_darken;
extern VCvarB r_dynamic;
extern VCvarB r_static_lights;

extern TClipBase clip_base;
extern refdef_t refdef;

extern VCvarI aspect_ratio;
extern VCvarB r_interpolate_frames;

extern VTextureTranslation **TranslationTables;
extern int NumTranslationTables;
extern VTextureTranslation IceTranslation;
extern TArray<VTextureTranslation *> DecorateTranslations;
extern TArray<VTextureTranslation *> BloodTranslations;

extern subsector_t *r_surf_sub;


//==========================================================================
//
//  IsSky
//
//==========================================================================
static inline bool IsSky (sec_plane_t *SPlane) {
  return (SPlane->pic == skyflatnum || (SPlane->SkyBox && SPlane->SkyBox->eventSkyBoxGetAlways()));
}

#endif
