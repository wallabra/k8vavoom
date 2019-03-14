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
#ifndef DRAWER_HEADER
#define DRAWER_HEADER

#define BLOCK_WIDTH         (128)
#define BLOCK_HEIGHT        (128)
#if 1
# define NUM_BLOCK_SURFS     (64)
# define NUM_CACHE_BLOCKS    (32*1024)
#else
# define NUM_BLOCK_SURFS     (32)
# define NUM_CACHE_BLOCKS    (8*1024)
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct surface_t;
struct surfcache_t;
struct mmdl_t;
struct VMeshModel;
class VPortal;


// ////////////////////////////////////////////////////////////////////////// //
struct particle_t {
  // drawing info
  TVec org; // position
  vuint32 colour; // ARGB colour
  float Size;
  // handled by refresh
  particle_t *next; // next in the list
  TVec vel; // velocity
  TVec accel; // acceleration
  float die; // cl.time when particle will be removed
  int type;
  float ramp;
  float gravity;
  float dur; // for pt_fading
};


struct surfcache_t {
  // position in light surface
  int s, t;
  // size
  int width, height;
  // line list in block
  surfcache_t *bprev, *bnext;
  // cache list in line
  surfcache_t *lprev, *lnext;
  surfcache_t *chain; // list of drawable surfaces
  surfcache_t *addchain; // list of specular surfaces
  int blocknum; // light surface index
  surfcache_t **owner;
  vuint32 Light; // checked for strobe flash
  int dlight;
  surface_t *surf;
  vuint32 lastframe;
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelDrawer : public VRenderLevelPublic {
protected:
  bool mIsAdvancedRenderer;

public:
  bool NeedsInfiniteFarClip;

  // lightmaps
  rgba_t light_block[NUM_BLOCK_SURFS][BLOCK_WIDTH*BLOCK_HEIGHT];
  bool block_changed[NUM_BLOCK_SURFS];
  surfcache_t *light_chain[NUM_BLOCK_SURFS];

  // specular lightmaps
  rgba_t add_block[NUM_BLOCK_SURFS][BLOCK_WIDTH*BLOCK_HEIGHT];
  bool add_changed[NUM_BLOCK_SURFS];
  surfcache_t *add_chain[NUM_BLOCK_SURFS];

  /*
  surface_t *SimpleSurfsHead;
  surface_t *SimpleSurfsTail;
  surface_t *SkyPortalsHead;
  surface_t *SkyPortalsTail;
  surface_t *HorizonPortalsHead;
  surface_t *HorizonPortalsTail;
  */

  // render lists; various queue functions will put surfaces there
  // those arrays are never cleared, only reset
  // each surface is marked with `currQueueFrame`
  // note that there is no overflow protection, so don't leave
  // the game running one level for weeks ;-)
  TArray<surface_t *> DrawSurfList;
  TArray<surface_t *> DrawSkyList;
  TArray<surface_t *> DrawHorizonList;

  int PortalDepth;
  vuint32 currDLightFrame;
  vuint32 currQueueFrame;

public:
  virtual void BuildLightMap (surface_t *) = 0;

  // defined only after `PushDlights()`
  // public, because it is used in advrender to determine rough
  // lightness of masked surfaces
  // `radius` is used for visibility raycasts
  // `surfplane` is used to light masked surfaces
  virtual vuint32 LightPoint (const TVec &p, float raduis, const TPlane *surfplane=nullptr) = 0;

  inline bool IsAdvancedRenderer () const { return mIsAdvancedRenderer; }
};


// ////////////////////////////////////////////////////////////////////////// //
class VDrawer {
public:
  enum {
    VCB_InitVideo,
    VCB_DeinitVideo,
    VCB_InitResolution,
    VCB_FinishUpdate,
  };

protected:
  bool mInitialized;
  bool isShittyGPU;

  static TArray<void (*) (int phase)> cbInitDeinit;

  static void callICB (int phase);

public:
  static void RegisterICB (void (*cb) (int phase));

public:
  VRenderLevelDrawer *RendLev;

  VDrawer () : RendLev(nullptr) { mInitialized = false; }
  virtual ~VDrawer () {}

  virtual void Init () = 0;
  // fsmode: 0: windowed; 1: scaled FS; 2: real FS
  virtual bool SetResolution (int AWidth, int AHeight, int fsmode) = 0;
  virtual void InitResolution () = 0;
  inline bool IsInited () const { return mInitialized; }

  virtual void StartUpdate (bool allowClear=true) = 0;
  virtual void Setup2D () = 0;
  virtual void Update () = 0;
  //virtual void BeginDirectUpdate() = 0;
  //virtual void EndDirectUpdate() = 0;
  virtual void Shutdown () = 0;
  virtual void *ReadScreen (int *bpp, bool *bot2top) = 0;
  virtual void ReadBackScreen (int Width, int Height, rgba_t *Dest) = 0;
  virtual void WarpMouseToWindowCenter () = 0;
  virtual void GetMousePosition (int *mx, int *my) = 0;

  // rendring stuff
  virtual void SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) = 0;
  virtual void SetupViewOrg () = 0;
  virtual void WorldDrawing () = 0;
  virtual void EndView () = 0;

  // texture stuff
  virtual void PrecacheTexture (VTexture *) = 0;
  virtual void FlushOneTexture (VTexture *) = 0; // unload one texture
  virtual void FlushTextures () = 0; // unload all textures

  // masked/translucent polygon decals
  virtual void FinishMaskedDecals () = 0;

  // polygon drawing
  virtual void DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                               float offs1, VTexture *Texture2, float offs2, int CMap) = 0;
  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) = 0;
  virtual void DrawSpritePolygon (const TVec *cv, VTexture *Tex, float Alpha,
                                  bool Additive, VTextureTranslation *Translation, int CMap,
                                  vuint32 light, vuint32 Fade, const TVec &normal, float pdist, const TVec &saxis,
                                  const TVec &taxis, const TVec &texorg, int hangup) = 0;
  virtual void DrawAliasModel (const TVec &origin, const TAVec &angles, const TVec &Offset,
                               const TVec &Scale, VMeshModel *Mdl, int frame, int nextframe,
                               VTexture *Skin, VTextureTranslation *Trans, int CMap, vuint32 light,
                               vuint32 Fade, float Alpha, bool Additive, bool is_view_model, float Inter,
                               bool Interpolate, bool ForceDepthUse, bool AllowTransparency,
                               bool onlyDepth) = 0;
  virtual bool StartPortal (VPortal *Portal, bool UseStencil) = 0;
  virtual void EndPortal (VPortal *Portal, bool UseStencil) = 0;

  //  particles
  virtual void StartParticles () = 0;
  virtual void DrawParticle (particle_t *) = 0;
  virtual void EndParticles () = 0;

  // drawing
  virtual void DrawPic (float x1, float y1, float x2, float y2,
                        float s1, float t1, float s2, float t2,
                        VTexture *Tex, VTextureTranslation *Trans, float Alpha) = 0;
  virtual void DrawPicShadow (float x1, float y1, float x2, float y2,
                              float s1, float t1, float s2, float t2,
                              VTexture *Tex, float shade) = 0;
  virtual void FillRectWithFlat (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2, VTexture *Tex) = 0;
  virtual void FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
                                       float s1, float t1, float s2, float t2, VTexture *Tex) = 0;
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 colour) = 0;
  virtual void ShadeRect (int x, int y, int w, int h, float darkening) = 0;
  virtual void DrawConsoleBackground (int h) = 0;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) = 0;

  // automap
  virtual void StartAutomap () = 0;
  virtual void DrawLine (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) = 0;
  virtual void EndAutomap () = 0;

  // advanced drawing
  virtual bool SupportsAdvancedRendering () = 0;
  virtual void DrawWorldAmbientPass () = 0;
  virtual void BeginShadowVolumesPass () = 0;
  virtual void BeginLightShadowVolumes (bool hasScissor, const int scoords[4]) = 0;
  virtual void EndLightShadowVolumes () = 0;
  virtual void RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius, int LightCanCross) = 0;
  virtual void BeginLightPass (TVec &LightPos, float Radius, vuint32 Colour) = 0;
  virtual void DrawSurfaceLight (surface_t *Surf, TVec &LightPos, float Radius, int LightCanCross) = 0;
  virtual void DrawWorldTexturesPass () = 0;
  virtual void DrawWorldFogPass () = 0;
  virtual void EndFogPass () = 0;
  virtual void DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                      const TVec &Offset, const TVec &Scale,
                                      VMeshModel *Mdl, int frame, int nextframe,
                                      VTexture *Skin, vuint32 light, float Alpha,
                                      float Inter, bool Interpolate,
                                      bool ForceDepth, bool AllowTransparency) = 0;
  virtual void DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                       const TVec &Offset, const TVec &Scale,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, VTextureTranslation *Trans,
                                       int CMap, float Alpha, float Inter,
                                       bool Interpolate, bool ForceDepth, bool AllowTransparency) = 0;
  virtual void BeginModelsLightPass (TVec &LightPos, float Radius, vuint32 Colour) = 0;
  virtual void DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                    const TVec &Offset, const TVec &Scale,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, float Alpha, float Inter,
                                    bool Interpolate, bool AllowTransparency) = 0;
  virtual void BeginModelsShadowsPass (TVec &LightPos, float LightRadius) = 0;
  virtual void DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                     const TVec &Offset, const TVec &Scale,
                                     VMeshModel *Mdl, int frame, int nextframe,
                                     float Inter, bool Interpolate,
                                     const TVec &LightPos, float LightRadius) = 0;
  virtual void DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                  const TVec &Offset, const TVec &Scale,
                                  VMeshModel *Mdl, int frame, int nextframe,
                                  VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                  bool Interpolate, bool AllowTransparency) = 0;
  virtual void GetRealWindowSize (int *rw, int *rh) = 0;

  // copy current FBO to secondary FBO
  virtual void CopyToSecondaryFBO () = 0;

  virtual void GetProjectionMatrix (VMatrix4 &mat) = 0;
  virtual void GetModelMatrix (VMatrix4 &mat) = 0;

  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4]) = 0;
  virtual void ResetScissor () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer types, menu system uses these numbers
enum {
  DRAWER_OpenGL,
  DRAWER_MAX
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer description
struct FDrawerDesc {
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VDrawer *(*Creator) ();

  FDrawerDesc (int Type, const char *AName, const char *ADescription,
    const char *ACmdLineArg, VDrawer *(*ACreator)());
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer driver declaration macro
#define IMPLEMENT_DRAWER(TClass, Type, Name, Description, CmdLineArg) \
static VDrawer *Create##TClass() \
{ \
  return new TClass(); \
} \
FDrawerDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);


#ifdef CLIENT
extern VDrawer *Drawer;
#endif


// ////////////////////////////////////////////////////////////////////////// //
// fancyprogress bar

// reset progress bar, setup initial timing and so on
// returns `false` if graphics is not initialized
bool R_PBarReset ();

// update progress bar, return `true` if something was displayed.
// it is safe to call this even if graphics is not initialized.
// without graphics, it will print occasionally console messages.
// you can call this as often as you want, it will take care of
// limiting output to reasonable amounts.
// `cur` must be zero or positive, `max` must be positive
bool R_PBarUpdate (const char *message, int cur, int max, bool forced=false);


// iniit loader messages system
void R_LdrMsgReset ();

// show loader message
void R_LdrMsgShow (const char *msg, int clr=CR_TAN);

extern int R_LdrMsgColorMain;
extern int R_LdrMsgColorSecondary;

static inline __attribute__((unused)) void R_LdrMsgShowMain (const char *msg) { R_LdrMsgShow(msg, R_LdrMsgColorMain); }
static inline __attribute__((unused)) void R_LdrMsgShowSecondary (const char *msg) { R_LdrMsgShow(msg, R_LdrMsgColorSecondary); }


#endif
