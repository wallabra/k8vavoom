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
#ifndef DRAWER_HEADER
#define DRAWER_HEADER


// ////////////////////////////////////////////////////////////////////////// //
struct surface_t;
struct surfcache_t; // see "render/r_shared.h"
struct mmdl_t;
struct VMeshModel;
class VPortal;


// ////////////////////////////////////////////////////////////////////////// //
struct particle_t {
  // drawing info
  TVec org; // position
  vuint32 color; // ARGB color
  float Size;
  // handled by refresh
  particle_t *next; // next in the list
  TVec vel; // velocity
  TVec accel; // acceleration
  float die; // cl.time when particle will be removed
  vint32 type;
  float ramp;
  float gravity;
  float dur; // for pt_fading
};


// ////////////////////////////////////////////////////////////////////////// //
//TODO: add profiler, check if several dirty rects are better
struct VDirtyArea {
public:
  int x0, y0;
  int x1, y1; // exclusive

public:
  inline VDirtyArea () noexcept : x0(0), y0(0), x1(0), y1(0) {}
  inline void clear () noexcept { x0 = y0 = x1 = y1 = 0; }
  inline bool isValid () const noexcept { return (x1-x0 > 0 && y1-y0 > 0); }
  inline int getWidth () const noexcept { return x1-x0; }
  inline int getHeight () const noexcept { return y1-y0; }
  inline void addDirty (int ax0, int ay0, int awdt, int ahgt) noexcept {
    if (awdt < 1 || ahgt < 1) return;
    if (isValid()) {
      x0 = min2(x0, ax0);
      y0 = min2(y0, ay0);
      x1 = max2(x1, ax0+awdt);
      y1 = max2(y1, ay0+ahgt);
    } else {
      x0 = ax0;
      y0 = ay0;
      x1 = ax0+awdt;
      y1 = ay0+ahgt;
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct trans_sprite_t;

class VRenderLevelDrawer : public VRenderLevelPublic {
protected:
  bool mIsShadowVolumeRenderer;

public:
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

    // no need to setup this
    inline trans_sprite_t () noexcept {}
    inline trans_sprite_t (EArrayNew) noexcept {}
    inline trans_sprite_t (ENoInit) noexcept {}
    inline ~trans_sprite_t () noexcept {}
  };

  struct DrawLists {
    // render lists; various queue functions will put surfaces there
    // those arrays are never cleared, only reset
    // each surface is marked with `currQueueFrame`
    // note that there is no overflow protection, so don't leave
    // the game running one level for weeks ;-)
    TArray<surface_t *> DrawSurfListSolid; // solid surfaces
    TArray<surface_t *> DrawSurfListMasked; // masked surfaces
    TArray<surface_t *> DrawSurfListAlpha; // alpha-blended surfaces
    TArray<surface_t *> DrawSurfListAdditive; // additive surfaces
    TArray<surface_t *> DrawSkyList;
    TArray<surface_t *> DrawHorizonList;
    TArray<trans_sprite_t> DrawSpriteList;

    inline void resetAll () {
      DrawSurfListSolid.reset();
      DrawSurfListMasked.reset();
      DrawSurfListAlpha.reset();
      DrawSurfListAdditive.reset();
      DrawSkyList.reset();
      DrawHorizonList.reset();
      DrawSpriteList.reset();
    }
  };

  TArray<DrawLists> DrawListStack;

  int PortalDepth;
  vuint32 currDLightFrame;
  vuint32 currQueueFrame;

public:
  inline DrawLists &GetCurrentDLS () { return DrawListStack[DrawListStack.length()-1]; }

  // should be called before rendering a frame
  // (i.e. in initial render, `RenderPlayerView()`)
  // creates 0th element of the stack
  void ResetDrawStack ();

  void PushDrawLists ();
  void PopDrawLists ();

  struct DrawListStackMark {
  public:
    VRenderLevelDrawer *owner;
    int level;
  public:
    inline DrawListStackMark (VRenderLevelDrawer *aowner) noexcept : owner(aowner), level(aowner->DrawListStack.length()) {
      vassert(owner);
      vassert(level > 0);
    }
    DrawListStackMark (const DrawListStackMark &) = delete;
    DrawListStackMark &operator = (const DrawListStackMark &) = delete;
    inline ~DrawListStackMark () {
      vassert(owner);
      vassert(level > 0);
      vassert(owner->DrawListStack.length() >= level);
      while (owner->DrawListStack.length() > level) owner->PopDrawLists();
      owner = nullptr;
      level = 0;
    }
  };

public:
  // lightmap chain iterator (used in renderer)
  // block number+1 or 0
  virtual vuint32 GetLightChainHead () = 0;
  // block number+1 or 0
  virtual vuint32 GetLightChainNext (vuint32 bnum) = 0;

  virtual VDirtyArea &GetLightBlockDirtyArea (vuint32 bnum) = 0;
  virtual VDirtyArea &GetLightAddBlockDirtyArea (vuint32 bnum) = 0;
  virtual rgba_t *GetLightBlock (vuint32 bnum) = 0;
  virtual rgba_t *GetLightAddBlock (vuint32 bnum) = 0;
  virtual surfcache_t *GetLightChainFirst (vuint32 bnum) = 0;

  virtual void BuildLightMap (surface_t *) = 0;

  // defined only after `PushDlights()`
  // public, because it is used in advrender to determine rough
  // lightness of masked surfaces
  // `radius` is used for visibility raycasts
  // `surfplane` is used to light masked surfaces
  virtual vuint32 LightPoint (const TVec &p, float raduis, float height, const TPlane *surfplane=nullptr, const subsector_t *psub=nullptr) = 0;

  virtual void UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced=false) = 0;

  inline bool IsShadowVolumeRenderer () const noexcept { return mIsShadowVolumeRenderer; }

  virtual bool IsNodeRendered (const node_t *node) const noexcept = 0;
  virtual bool IsSubsectorRendered (const subsector_t *sub) const noexcept = 0;

  virtual void PrecacheLevel () = 0;

public:
  static inline bool IsAdditiveStyle (int style) {
    switch (style) {
      case STYLE_AddStencil:
      case STYLE_Add:
      case STYLE_Subtract:
      case STYLE_AddShaded:
        return true;
      default: break;
    }
    return false;
  }

  static inline int CoerceRenderStyle (int style) {
    switch (style) {
      case STYLE_None:
      case STYLE_Normal:
      case STYLE_Fuzzy:
      case STYLE_SoulTrans:
      case STYLE_OptFuzzy:
      case STYLE_Stencil:
      case STYLE_AddStencil:
      case STYLE_Translucent:
      case STYLE_Add:
      case STYLE_Dark:
        return style;
      case STYLE_Shaded: return STYLE_Stencil; // not implemented
      case STYLE_TranslucentStencil: return STYLE_Translucent; // not implemented
      case STYLE_Shadow: return /*STYLE_Translucent*/STYLE_Fuzzy; // not implemented
      case STYLE_Subtract: return STYLE_Add; // not implemented
      case STYLE_AddShaded: return STYLE_Add; // not implemented
      default: break;
    }
    return STYLE_Normal;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t;

struct AliasModelTrans {
  TVec Shift; // unscaled, done before scaling and offseting
  TVec Offset;
  TVec Scale;

  inline AliasModelTrans () noexcept
    : Shift(0.0f, 0.0f, 0.0f)
    , Offset(0.0f, 0.0f, 0.0f)
    , Scale(1.0f, 1.0f, 1.0f)
  {}
};


class VDrawer {
public:
  enum {
    VCB_InitVideo,
    VCB_DeinitVideo,
    VCB_InitResolution,
    VCB_DeinitResolution, //FIXME: not implemented yet
    VCB_FinishUpdate,
  };

  enum {
    CLEAR_COLOR = 1u<<0,
    CLEAR_DEPTH = 1u<<1,
    CLEAR_STENCIL = 1u<<2,
    CLEAR_ALL = (CLEAR_COLOR|CLEAR_DEPTH|CLEAR_STENCIL),
  };

protected:
  bool mInitialized;
  bool isShittyGPU;
  bool shittyGPUCheckDone;
  bool useReverseZ;
  bool HaveDepthClamp;

  vuint32 updateFrame; // counter

  // moved here, so we can fix them with FBO change
  int ScrWdt, ScrHgt;

  static TArray<void (*) (int phase)> cbInitDeinit;

  static void callICB (int phase);

public:
  VViewPortMats vpmats;
  // the following is here because alot of code can access it
  // current rendering position and angles
  TVec vieworg;
  TAVec viewangles;
  // calculated from `viewangles`, normalised
  TVec viewforward;
  TVec viewright;
  TVec viewup;
  // calculated from `vieworg` and `viewangles`
  TFrustum view_frustum;
  bool MirrorFlip;
  bool MirrorClip;
  static float LightFadeMult;

public:
  static void RegisterICB (void (*cb) (int phase));

public:
  VRenderLevelDrawer *RendLev;

public:
  VDrawer () noexcept;
  virtual ~VDrawer ();

  inline bool CanUseRevZ () const noexcept { return useReverseZ; }
  inline bool IsShittyGPU () const noexcept { return isShittyGPU; }
  inline bool IsInited () const noexcept { return mInitialized; }
  inline bool CanUseDepthClamp () const noexcept { return HaveDepthClamp; }

  inline int getWidth () const noexcept { return ScrWdt; }
  inline int getHeight () const noexcept { return ScrHgt; }

  inline void SetUpdateFrame (vuint32 n) noexcept { updateFrame = n; }
  inline vuint32 GetUpdateFrame () const noexcept { return updateFrame; }
  inline void IncUpdateFrame () noexcept { if (++updateFrame == 0) updateFrame = 1; }
  void ResetTextureUpdateFrames () noexcept;

  virtual void Init () = 0;
  // fsmode: 0: windowed; 1: scaled FS; 2: real FS
  virtual bool SetResolution (int AWidth, int AHeight, int fsmode) = 0;
  virtual void InitResolution () = 0;
  virtual void DeinitResolution () = 0;

  virtual void EnableBlend () = 0;
  virtual void DisableBlend () = 0;
  virtual void SetBlendEnabled (const bool v) = 0;

  virtual void Posteffect_Bloom (int ax, int ay, int awidth, int aheight) = 0;

  virtual void StartUpdate () = 0;
  virtual void ClearScreen (unsigned clearFlags=CLEAR_COLOR) = 0;
  virtual void Setup2D () = 0;
  virtual void Update () = 0;
  virtual void Shutdown () = 0;
  virtual void *ReadScreen (int *bpp, bool *bot2top) = 0;
  virtual void ReadBackScreen (int Width, int Height, rgba_t *Dest) = 0;
  virtual void WarpMouseToWindowCenter () = 0;
  virtual void GetMousePosition (int *mx, int *my) = 0;

  // rendring stuff
  virtual bool UseFrustumFarClip () = 0;
  virtual void SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) = 0;
  virtual void SetupViewOrg () = 0;
  virtual void WorldDrawing () = 0;
  virtual void EndView (bool ignoreColorTint=false) = 0;

  virtual void SetMainFBO (bool forced=false) = 0;

  virtual void ClearCameraFBOs () = 0;
  virtual int GetCameraFBO (int texnum, int width, int height) = 0; // returns index or -1; (re)creates FBO if necessary
  virtual int FindCameraFBO (int texnum) = 0; // returns index or -1
  virtual void SetCameraFBO (int cfboindex) = 0;
  virtual GLuint GetCameraFBOTextureId (int cfboindex) = 0; // returns 0 if cfboindex is invalid

  // this copies main FBO to wipe FBO, so we can run wipe shader
  virtual void PrepareWipe () = 0;
  // render wipe from wipe to main FBO
  // should be called after `StartUpdate()`
  // and (possibly) rendering something to the main FBO
  // time is in seconds, from zero to...
  // returns `false` if wipe is complete
  // -1 means "show saved wipe screen"
  virtual bool RenderWipe (float time) = 0;

  // renderer require this
  virtual void GLEnableOffset () = 0;
  virtual void GLDisableOffset () = 0;
  // this also enables it if it was disabled
  virtual void GLPolygonOffset (const float afactor, const float aunits) = 0;

  // texture stuff
  virtual void PrecacheTexture (VTexture *) = 0;
  virtual void FlushOneTexture (VTexture *tex, bool forced=false) = 0; // unload one texture
  virtual void FlushTextures (bool forced=false) = 0; // unload all textures

  // polygon drawing
  virtual void StartSkyPolygons () = 0;
  virtual void EndSkyPolygons () = 0;
  virtual void DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                               float offs1, VTexture *Texture2, float offs2, int CMap) = 0;

  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) = 0;

  virtual void BeginTranslucentPolygonDecals () = 0;
  virtual void DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) = 0;

  virtual void DrawSpritePolygon (const TVec *cv, VTexture *Tex, float Alpha,
                                  bool Additive, VTextureTranslation *Translation, int CMap,
                                  vuint32 light, vuint32 Fade, const TVec &normal, float pdist, const TVec &saxis,
                                  const TVec &taxis, const TVec &texorg, int hangup) = 0;
  virtual void DrawAliasModel (const TVec &origin, const TAVec &angles, const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe, VTexture *Skin, VTextureTranslation *Trans,
                               int CMap, vuint32 light, vuint32 Fade, float Alpha, bool Additive, bool is_view_model,
                               float Inter, bool Interpolate, bool ForceDepthUse, bool AllowTransparency,
                               bool onlyDepth) = 0;

  virtual bool StartPortal (VPortal *Portal, bool UseStencil) = 0;
  virtual void EndPortal (VPortal *Portal, bool UseStencil) = 0;

  virtual void ForceClearStencilBuffer () = 0;
  virtual void ForceMarkStencilBufferDirty () = 0;

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
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void ShadeRect (float x1, float y1, float x2, float y2, float darkening) = 0;
  virtual void DrawLine (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void DrawConsoleBackground (int h) = 0;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) = 0;

  virtual void BeginTexturedPolys () = 0;
  virtual void EndTexturedPolys () = 0;
  virtual void DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha, int vcount, const TVec *verts, const TVec *origverts=nullptr) = 0;

  // automap
  virtual void StartAutomap (bool asOverlay) = 0;
  virtual void DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) = 0;
  virtual void EndAutomap () = 0;

  // advanced drawing
  virtual bool SupportsShadowVolumeRendering () = 0;
  virtual void DrawWorldAmbientPass () = 0;
  virtual void BeginShadowVolumesPass () = 0;
  virtual void BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const TVec &aconeDir, const float aconeAngle) = 0;
  virtual void EndLightShadowVolumes () = 0;
  virtual void RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) = 0;

  virtual void BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) = 0;
  virtual void DrawSurfaceLight (surface_t *Surf) = 0;

  virtual void DrawWorldTexturesPass () = 0;
  virtual void DrawWorldFogPass () = 0;
  virtual void EndFogPass () = 0;

  virtual void DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                      const AliasModelTrans &Transform,
                                      VMeshModel *Mdl, int frame, int nextframe,
                                      VTexture *Skin, vuint32 light, float Alpha,
                                      float Inter, bool Interpolate,
                                      bool ForceDepth, bool AllowTransparency) = 0;
  virtual void DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                       const AliasModelTrans &Transform,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, VTextureTranslation *Trans,
                                       int CMap, float Alpha, float Inter,
                                       bool Interpolate, bool ForceDepth, bool AllowTransparency) = 0;
  virtual void BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const TVec &aconeDir, const float aconeAngle) = 0;
  virtual void DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                    const AliasModelTrans &Transform,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, float Alpha, float Inter,
                                    bool Interpolate, bool AllowTransparency) = 0;
  virtual void BeginModelsShadowsPass (TVec &LightPos, float LightRadius) = 0;
  virtual void DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                     const AliasModelTrans &Transform,
                                     VMeshModel *Mdl, int frame, int nextframe,
                                     float Inter, bool Interpolate,
                                     const TVec &LightPos, float LightRadius) = 0;
  virtual void DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                  const AliasModelTrans &Transform,
                                  VMeshModel *Mdl, int frame, int nextframe,
                                  VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                  bool Interpolate, bool AllowTransparency) = 0;
  virtual void GetRealWindowSize (int *rw, int *rh) = 0;

  virtual void GetProjectionMatrix (VMatrix4 &mat) = 0;
  virtual void GetModelMatrix (VMatrix4 &mat) = 0;
  virtual void SetProjectionMatrix (const VMatrix4 &mat) = 0;
  virtual void SetModelMatrix (const VMatrix4 &mat) = 0;

  void CalcProjectionMatrix (VMatrix4 &ProjMat, VRenderLevelDrawer *ARLev, const refdef_t *rd);
  void CalcModelMatrix (VMatrix4 &ModelMat, const TVec &origin, const TAVec &angles, bool MirrorFlip=false);

  // call this before doing light scissor calculations (can be called once per scene)
  // sets `vpmats`
  // scissor setup will use those matrices (but won't modify them)
  // also, `SetupViewOrg()` automatically call this method
  //virtual void LoadVPMatrices () = 0;
  // no need to do this:
  //   projection matrix and viewport set in `SetupView()`
  //   model matrix set in `SetupViewOrg()`

  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox=nullptr) = 0;
  virtual void ResetScissor () = 0;

  virtual void DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) = 0;
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

static inline VVA_OKUNUSED void R_LdrMsgShowMain (const char *msg) { R_LdrMsgShow(msg, R_LdrMsgColorMain); }
static inline VVA_OKUNUSED void R_LdrMsgShowSecondary (const char *msg) { R_LdrMsgShow(msg, R_LdrMsgColorSecondary); }


#endif
