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
#ifndef VAVOOM_GL_LOCAL_HEADER
#define VAVOOM_GL_LOCAL_HEADER

#include "gamedefs.h"
#include "../r_shared.h"

#ifdef GL4ES_NO_CONSTRUCTOR
#  define GL_GLEXT_PROTOTYPES
#endif

#ifdef _WIN32
# include <windows.h>
#endif
#ifdef USE_GLAD
# include "glad.h"
#else
# include <GL/gl.h>
#endif
#ifdef _WIN32
/*
# ifndef GL_GLEXT_PROTOTYPES
#  define GL_GLEXT_PROTOTYPES
# endif
*/
# include <GL/glext.h>
#endif

// OpenGL extensions
#define VV_GLDECLS
#include "gl_imports.h"
#undef VV_GLDECLS


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarF gl_alpha_threshold;
extern VCvarB gl_sort_textures;
extern VCvarI r_ambient_min;
extern VCvarB r_allow_ambient;
extern VCvarB r_decals_enabled;
extern VCvarB r_decals_wall_masked;
extern VCvarB r_decals_wall_alpha;
extern VCvarB gl_decal_debug_nostencil;
extern VCvarB gl_decal_debug_noalpha;
extern VCvarB gl_decal_dump_max;
extern VCvarB gl_decal_reset_max;
extern VCvarB gl_uusort_textures;
extern VCvarB gl_dbg_adv_render_surface_textures;
extern VCvarB gl_dbg_adv_render_surface_fog;
extern VCvarB gl_dbg_render_stack_portal_bounds;
extern VCvarB gl_use_stencil_quad_clear;
extern VCvarI gl_dbg_use_zpass;
extern VCvarB gl_dbg_wireframe;
extern VCvarF gl_maxdist;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;
extern VCvarB r_brightmaps_additive;
extern VCvarB r_brightmaps_filter;
extern VCvarB r_glow_flat;
extern VCvarB gl_lmap_allow_partial_updates;

extern VCvarB gl_regular_prefill_depth;
extern VCvarB gl_regular_disable_overbright;

extern VCvarI gl_release_ram_textures_mode;


// ////////////////////////////////////////////////////////////////////////// //
static inline const char *VGetGLErrorStr (const GLenum glerr) {
  switch (glerr) {
    case GL_NO_ERROR: return "no error";
    case GL_INVALID_ENUM: return "invalid enum";
    case GL_INVALID_VALUE: return "invalid value";
    case GL_INVALID_OPERATION: return "invalid operation";
    case GL_STACK_OVERFLOW: return "stack overflow";
    case GL_STACK_UNDERFLOW: return "stack underflow";
    case GL_OUT_OF_MEMORY: return "out of memory";
    default: break;
  }
  static char errstr[32];
  snprintf(errstr, sizeof(errstr), "0x%04x", (unsigned)glerr);
  return errstr;
}

#define GLDRW_RESET_ERROR()  (void)glGetError()

#define GLDRW_CHECK_ERROR(actionmsg_)  do { \
  GLenum glerr_ = glGetError(); \
  if (glerr_ != 0) Sys_Error("OpenGL: cannot %s, error: %s", actionmsg_, VGetGLErrorStr(glerr_)); \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
class VOpenGLDrawer : public VDrawer {
public:
  class VGLShader {
  public:
    VGLShader *next;
    VOpenGLDrawer *owner;
    const char *progname;
    const char *incdir;
    const char *vssrcfile;
    const char *fssrcfile;
    // compiled vertex program
    GLhandleARB prog;
    TArray<VStr> defines;

    typedef float glsl_float2[2];
    typedef float glsl_float3[3];
    typedef float glsl_float4[4];
    typedef float glsl_float9[9];

  public:
    VGLShader() : next(nullptr), owner(nullptr), progname(nullptr), vssrcfile(nullptr), fssrcfile(nullptr), prog(-1) {}

    void MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *aincdir, const char *avssrcfile, const char *afssrcfile);

    virtual void Compile ();
    virtual void Unload ();
    virtual void Setup (VOpenGLDrawer *aowner) = 0;
    virtual void LoadUniforms () = 0;
    virtual void UnloadUniforms () = 0;
    virtual void UploadChangedUniforms (bool forced=false) = 0;
    //virtual void UploadChangedAttrs () = 0;

    void Activate ();
    void Deactivate ();

    static inline bool notEqual_float (const float &v1, const float &v2) { return (FASI(v1) != FASI(v2)); }
    static inline bool notEqual_bool (const bool v1, const bool v2) { return (v1 != v2); }
    static inline bool notEqual_vec2 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*2) != 0); }
    static inline bool notEqual_vec3 (const TVec &v1, const TVec &v2) { return (memcmp(&v1.x, &v2.x, sizeof(float)*3) != 0); }
    static inline bool notEqual_vec4 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*4) != 0); }
    static inline bool notEqual_mat3 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*9) != 0); }
    static inline bool notEqual_mat4 (const VMatrix4 &v1, const VMatrix4 &v2) { return (memcmp(&v1.m[0][0], &v2.m[0][0], sizeof(float)*16) != 0); }
    static inline bool notEqual_sampler2D (const vuint32 v1, const vuint32 v2) { return (v1 != v2); }

    static inline void copyValue_float (float &dest, const float &src) { dest = src; }
    static inline void copyValue_bool (bool &dest, const bool &src) { dest = src; }
    static inline void copyValue_vec3 (TVec &dest, const TVec &src) { dest = src; }
    static inline void copyValue_mat4 (VMatrix4 &dest, const VMatrix4 &src) { memcpy(&dest.m[0][0], &src.m[0][0], sizeof(float)*16); }
    static inline void copyValue_vec4 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*4); }
    static inline void copyValue_vec2 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*2); }
    static inline void copyValue_mat3 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*9); }
    static inline void copyValue_sampler2D (vuint32 &dest, const vuint32 &src) { dest = src; }
  };

  friend class VGLShader;

  class FBO {
    //friend class VOpenGLDrawer;
  private:
    class VOpenGLDrawer *mOwner;
    GLuint mFBO;
    GLuint mColorTid;
    GLuint mDepthStencilRBO; // use renderbuffer for depth/stencil (we don't need to read this info yet); 0 if none
    int mWidth;
    int mHeight;
    bool mLinearFilter;

  private:
    void createInternal (VOpenGLDrawer *aowner, int awidth, int aheight, bool createDepthStencil, bool mirroredRepeat);

  public:
    VV_DISABLE_COPY(FBO)
    FBO ();
    ~FBO ();

    inline bool isValid () const noexcept { return (mOwner != nullptr); }
    inline int getWidth () const noexcept { return mWidth; }
    inline int getHeight () const noexcept { return mHeight; }

    inline bool getLinearFilter () const noexcept { return mLinearFilter; }
    // has effect after texture recreation
    inline void setLinearFilter (bool v) noexcept { mLinearFilter = v; }

    inline GLuint getColorTid () const noexcept { return mColorTid; }
    inline GLuint getFBOid () const noexcept { return mFBO; }
    inline GLuint getDSRBTid () const noexcept { return mDepthStencilRBO; }

    void createTextureOnly (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat=false);
    void createDepthStencil (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat=false);
    void destroy ();

    void activate ();
    void deactivate ();

    // this blits only color info
    // restore active FBO manually after calling this
    // it also can reset shader program
    // if `dest` is nullptr, blit to screen buffer (not yet)
    void blitTo (FBO *dest, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filter);

    void blitToScreen ();
  };

  friend class FBO;

  VGLShader *currentActiveShader;
  FBO *currentActiveFBO;

  void DeactivateShader ();
  void ReactivateCurrentFBO ();

#include "gl_shaddef.hi"

private:
  bool usingZPass; // if we are rendering shadow volumes, should we do "z-pass"?
  TVec coneDir; // current spotlight direction
  float coneAngle; // current spotlight cone angle
  bool spotLight; // is current light a spotlight?
  GLint savedDepthMask; // used in various begin/end methods
  // for `DrawTexturedPoly()` API
  VTexture *texturedPolyLastTex;
  float texturedPolyLastAlpha;
  TVec texturedPolyLastLight;
  // list of surfaces with masked textures, for z-prefill
  TArray<surface_t *> zfillMasked;

  bool canIntoBloomFX;
  bool lastOverbrightEnable;

protected:
  VGLShader *shaderHead;

  void registerShader (VGLShader *shader);
  void CompileShaders ();
  void DestroyShaders ();

protected:
  enum { MaxDepthMaskStack = 16 };
  GLint depthMaskStack[MaxDepthMaskStack];
  unsigned depthMaskSP;

  virtual void PushDepthMask () override;
  virtual void PopDepthMask () override;

public:
  // scissor array indicies
  enum {
    SCS_MINX,
    SCS_MINY,
    SCS_MAXX,
    SCS_MAXY,
  };

public:
  // VDrawer interface
  VOpenGLDrawer ();
  virtual ~VOpenGLDrawer () override;
  virtual void InitResolution () override;
  virtual void DeinitResolution () override;
  virtual void StartUpdate () override;
  virtual void ClearScreen (unsigned clearFlags=CLEAR_COLOR) override;
  virtual void Setup2D () override;
  virtual void *ReadScreen (int *, bool *) override;
  virtual void ReadBackScreen (int, int, rgba_t *) override;
  void ReadFBOPixels (FBO *srcfbo, int Width, int Height, rgba_t *Dest);

  void FinishUpdate ();

  // rendering stuff
  // shoud frustum use far clipping plane?
  virtual bool UseFrustumFarClip () override;
  // setup projection matrix and viewport, clear screen, setup various OpenGL context properties
  // called from `VRenderLevelShared::SetupFrame()`
  virtual void SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) override;
  // setup model matrix according to `viewangles` and `vieworg`
  virtual void SetupViewOrg () override;
  // setup 2D ortho rendering mode
  virtual void EndView (bool ignoreColorTint=false) override;

  virtual void DisableClipPlanes () override;

  // texture stuff
  virtual void PrecacheTexture (VTexture *) override;

  // polygon drawing
  virtual void WorldDrawing () override;
  virtual void DrawWorldAmbientPass () override;

  virtual void BeginShadowVolumesPass () override;
  virtual void BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const TVec &aconeDir, const float aconeAngle) override;
  virtual void EndLightShadowVolumes () override;
  virtual void RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) override;
  void RenderSurfaceShadowVolumeZPassIntr (const surface_t *surf, const TVec &LightPos, float Radius);

  virtual void BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) override;
  virtual void DrawSurfaceLight (surface_t *surf) override;

  virtual void DrawWorldTexturesPass () override;
  virtual void DrawWorldFogPass () override;
  virtual void EndFogPass () override;

  virtual void StartSkyPolygons () override;
  virtual void EndSkyPolygons () override;
  virtual void DrawSkyPolygon (surface_t *, bool, VTexture *, float, VTexture *, float, int) override;
  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) override;

  virtual void BeginTranslucentPolygonDecals () override;
  virtual void DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) override;

  virtual void DrawSpritePolygon (float time, const TVec *cv, VTexture *Tex,
                                  const RenderStyleInfo &ri,
                                  VTextureTranslation *Translation, int CMap,
                                  const TVec &sprnormal, float sprpdist,
                                  const TVec &saxis, const TVec &taxis, const TVec &texorg) override;

  virtual void DrawAliasModel (const TVec &origin, const TAVec &angles, const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe, VTexture *Skin, VTextureTranslation *Trans,
                               int CMap, const RenderStyleInfo &ri, bool is_view_model,
                               float Inter, bool Interpolate, bool ForceDepthUse, bool AllowTransparency,
                               bool onlyDepth) override;

  virtual void BeginModelsAmbientPass () override;
  virtual void EndModelsAmbientPass () override;
  virtual void DrawAliasModelAmbient(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, vuint32, float, float, bool,
    bool, bool) override;
  virtual void BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const TVec &aconeDir, const float aconeAngle) override;
  virtual void EndModelsLightPass () override;
  virtual void DrawAliasModelLight(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, float, float, bool, bool) override;
  virtual void BeginModelsShadowsPass (TVec &LightPos, float LightRadius) override;
  virtual void EndModelsShadowsPass () override;
  virtual void DrawAliasModelShadow(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, float, bool, const TVec &, float) override;
  virtual void BeginModelsTexturesPass () override;
  virtual void EndModelsTexturesPass () override;
  void DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                               const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe,
                               VTexture *Skin, VTextureTranslation *Trans, int CMap,
                               const RenderStyleInfo &ri, float Inter,
                               bool Interpolate, bool ForceDepth, bool AllowTransparency) override;
  virtual void BeginModelsFogPass () override;
  virtual void EndModelsFogPass () override;
  virtual void DrawAliasModelFog(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, vuint32, float, float, bool, bool) override;
  virtual bool StartPortal(VPortal *, bool) override;
  virtual void EndPortal(VPortal *, bool) override;

  // particles
  virtual void StartParticles () override;
  virtual void DrawParticle (particle_t *) override;
  virtual void EndParticles () override;

  // drawing
  virtual void DrawPic (float x1, float y1, float x2, float y2,
                        float s1, float t1, float s2, float t2,
                        VTexture *Tex, VTextureTranslation *Trans, float Alpha) override;
  virtual void DrawPicShadow (float x1, float y1, float x2, float y2,
                              float s1, float t1, float s2, float t2,
                              VTexture *Tex, float shade) override;
  virtual void FillRectWithFlat (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
                                       float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void ShadeRect (float x1, float y1, float x2, float y2, float darkening) override;
  virtual void DrawLine (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawConsoleBackground (int h) override;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) override;

  virtual void BeginTexturedPolys () override;
  virtual void EndTexturedPolys () override;
  virtual void DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha, int vcount, const TVec *verts, const SurfVertex *origverts=nullptr) override;

  // automap
  virtual void StartAutomap (bool asOverlay) override;
  virtual void DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) override;
  virtual void EndAutomap () override;

  // advanced drawing.
  virtual bool SupportsShadowVolumeRendering () override;

  virtual void GetProjectionMatrix (VMatrix4 &mat) override;
  virtual void GetModelMatrix (VMatrix4 &mat) override;
  virtual void SetProjectionMatrix (const VMatrix4 &mat) override;
  virtual void SetModelMatrix (const VMatrix4 &mat) override;

  // call this before doing light scissor calculations (can be called once per scene)
  // sets `vpmats`
  // scissor setup will use those matrices (but won't modify them)
  // called in `SetupViewOrg()`, which setups model transformation matrix
  //virtual void LoadVPMatrices () override;
  // no need to do this:
  //   projection matrix and viewport set in `SetupView()`
  //   model matrix set in `SetupViewOrg()`

  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox=nullptr) override;
  virtual void ResetScissor () override;

  static inline float getAlphaThreshold () { return clampval(gl_alpha_threshold.asFloat(), 0.0f, 1.0f); }

  //virtual void GetRealWindowSize (int *rw, int *rh) override;

  virtual void DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) override;

private:
  vuint8 decalStcVal;
  bool decalUsedStencil;
  bool stencilBufferDirty;
  bool blendEnabled;
  bool offsetEnabled;
  float offsetFactor, offsetUnits;

  // last used shadow volume scissor
  // if new scissor is inside this one, and stencil buffer is not changed,
  // do not clear stencil buffer
  // reset in `BeginShadowVolumesPass()`
  // modified in `BeginLightShadowVolumes()`
  GLint lastSVScissor[4];
  // set in `BeginShadowVolumesPass()`
  GLint lastSVVport[4];

  // modified in `SetupLightScissor()`/`ResetScissor()`
  GLint currentSVScissor[4];

  // reset in `BeginLightShadowVolumes()`
  // set in `RenderSurfaceShadowVolume()`
  bool wasRenderedShadowSurface;


  enum DecalType { DT_SIMPLE, DT_LIGHTMAP, DT_ADVANCED };

  // this is required for decals
  inline void NoteStencilBufferDirty () { stencilBufferDirty = true; }
  inline bool IsStencilBufferDirty () const { return stencilBufferDirty; }
  inline void ClearStencilBuffer () { if (stencilBufferDirty) glClear(GL_STENCIL_BUFFER_BIT); stencilBufferDirty = false; decalUsedStencil = false; }

  inline void GLEnableBlend () { if (!blendEnabled) { blendEnabled = true; glEnable(GL_BLEND); } }
  inline void GLDisableBlend () { if (blendEnabled) { blendEnabled = false; glDisable(GL_BLEND); } }
  inline void GLSetBlendEnabled (const bool v) { if (blendEnabled != v) { blendEnabled = v; if (v) glEnable(GL_BLEND); else glDisable(GL_BLEND); } }

  virtual void GLEnableOffset () override;
  virtual void GLDisableOffset () override;
  // this also enables it if it was disabled
  virtual void GLPolygonOffset (const float afactor, const float aunits) override;

  virtual void ForceClearStencilBuffer () override;
  virtual void ForceMarkStencilBufferDirty () override;

  virtual void EnableBlend () override;
  virtual void DisableBlend () override;
  virtual void SetBlendEnabled (const bool v) override;

  void RenderPrepareShaderDecals (surface_t *surf);
  bool RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap);

  // regular renderer building parts
  // returns `true` if we need to re-setup texture
  bool RenderSimpleSurface (bool textureChanged, surface_t *surf);
  bool RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache);

  void RestoreDepthFunc ();

  // see "r_shared.h" for `struct GlowParams`

#define VV_GLDRAWER_ACTIVATE_GLOW(shad_,gp_)  do { \
  shad_.SetGlowColorFloor(((gp_.glowCF>>16)&0xff)/255.0f, ((gp_.glowCF>>8)&0xff)/255.0f, (gp_.glowCF&0xff)/255.0f, ((gp_.glowCF>>24)&0xff)/255.0f); \
  shad_.SetGlowColorCeiling(((gp_.glowCC>>16)&0xff)/255.0f, ((gp_.glowCC>>8)&0xff)/255.0f, (gp_.glowCC&0xff)/255.0f, ((gp_.glowCC>>24)&0xff)/255.0f); \
  shad_.SetFloorZ(gp_.floorZ); \
  shad_.SetCeilingZ(gp_.ceilingZ); \
  shad_.SetFloorGlowHeight(gp_.floorGlowHeight); \
  shad_.SetCeilingGlowHeight(gp_.ceilingGlowHeight); \
} while (0)

#define VV_GLDRAWER_DEACTIVATE_GLOW(shad_)  do { \
  shad_.SetGlowColorFloor(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetGlowColorCeiling(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetFloorGlowHeight(128); \
  shad_.SetCeilingGlowHeight(128); \
} while (0)

  inline void CalcGlow (GlowParams &gp, const surface_t *surf) const {
    gp.clear();
    if (!surf->seg || !surf->subsector) return;
    bool checkFloorFlat, checkCeilingFlat;
    const sector_t *sec = surf->subsector->sector;
    // check for glowing sector floor
    if (surf->glowFloorHeight > 0 && surf->glowFloorColor) {
      gp.floorGlowHeight = surf->glowFloorHeight;
      gp.glowCF = surf->glowFloorColor;
      gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
      checkFloorFlat = false;
    } else {
      checkFloorFlat = true;
    }
    // check for glowing sector ceiling
    if (surf->glowCeilingHeight > 0 && surf->glowCeilingColor) {
      gp.ceilingGlowHeight = surf->glowCeilingHeight;
      gp.glowCC = surf->glowCeilingColor;
      gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
      checkCeilingFlat = false;
    } else {
      checkCeilingFlat = true;
    }
    if ((checkFloorFlat || checkCeilingFlat) && r_glow_flat) {
      // check for glowing textures
      //FIXME: check actual view height here
      if (sec /*&& !sec->heightsec*/) {
        if (checkFloorFlat && sec->floor.pic) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.floorGlowHeight = 128;
            gp.glowCF = gtex->glowing;
            gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
            if (!gtex->IsGlowFullbright()) {
              // fix light level
              const unsigned slins = (r_allow_ambient ? (surf->Light>>24)&0xff : clampToByte(r_ambient_min));
              gp.glowCF = (gp.glowCF&0x00ffffffu)|(slins<<24);
            }
          }
        }
        if (checkCeilingFlat && sec->ceiling.pic) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.ceilingGlowHeight = 128;
            gp.glowCC = gtex->glowing;
            gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
            if (!gtex->IsGlowFullbright()) {
              // fix light level
              const unsigned slins = (r_allow_ambient ? (surf->Light>>24)&0xff : clampToByte(r_ambient_min));
              gp.glowCC = (gp.glowCF&0x00ffffffu)|(slins<<24);
            }
          }
        }
      }
    }
  }

public:
  GLint glGetUniLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);
  GLint glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);

private:
  vuint8 *readBackTempBuf;
  int readBackTempBufSize;

public:
  struct SurfListItem {
    surface_t *surf;
    surfcache_t *cache;
  };

private:
  TArray<SurfListItem> surfList;

  inline void surfListClear () { surfList.reset(); }

  inline void surfListAppend (surface_t *surf, surfcache_t *cache=nullptr) {
    SurfListItem &si = surfList.alloc();
    si.surf = surf;
    si.cache = cache;
  }

private:
  static inline float getSurfLightLevel (const surface_t *surf) {
    if (r_glow_flat && surf && !surf->seg && surf->subsector) {
      const sector_t *sec = surf->subsector->sector;
      //FIXME: check actual view height here
      if (sec && !sec->heightsec) {
        if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 1.0f;
        }
        if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 1.0f;
        }
      }
    }
    if (!surf) return 0;
    int slins = (r_allow_ambient ? (surf->Light>>24)&0xff : clampToByte(r_ambient_min));
    if (slins < r_ambient_min) slins = clampToByte(r_ambient_min);
    return float(slins)/255.0f;
  }

  static inline void glVertex (const TVec &v) { glVertex3f(v.x, v.y, v.z); }
  static inline void glVertex4 (const TVec &v, const float w) { glVertex4f(v.x, v.y, v.z, w); }

  struct CameraFBOInfo {
  public:
    FBO fbo;
    int texnum; // camera texture number for this FBO
    int camwidth, camheight; // camera texture dimensions for this FBO
    int index; // internal index of this FBO

  public:
    VV_DISABLE_COPY(CameraFBOInfo)
    CameraFBOInfo ();
    ~CameraFBOInfo ();
  };

protected:
  // for sprite VBOs
  /*
  struct __attribute__((packed)) SpriteVertex {
    float x, y, z, s, t;
  };
  */

protected:
  vuint8 *tmpImgBuf0;
  vuint8 *tmpImgBuf1;
  int tmpImgBufSize;

  // set by `SetTexture()` and company
  // non-scaled
  float tex_iw, tex_ih;
  int tex_w, tex_h;
  // scaled
  float tex_iw_sc, tex_ih_sc;
  int tex_w_sc, tex_h_sc;
  // scale
  float tex_scale_x, tex_scale_y;

  int lastgamma;
  int CurrentFade;

  bool hasNPOT;
  bool hasBoundsTest; // GL_EXT_depth_bounds_test

  FBO mainFBO;
  FBO ambLightFBO; // we'll copy ambient light texture here, so we can use it in decal renderer to light decals
  FBO wipeFBO; // we'll copy main FBO here to render wipe transitions
  // bloom
  FBO bloomscratchFBO, bloomscratch2FBO, bloomeffectFBO, bloomcoloraveragingFBO;
  // view (texture) camera updates will use this to render camera views
  // as reading from rendered texture is very slow, we will flip-flop FBOs,
  // using `current` to render new camera views, and `current^1` to get previous ones
  TArray<CameraFBOInfo *> cameraFBOList;
  // current "main" fbo: <0: `mainFBO`, otherwise camera FBO
  int currMainFBO;

  GLint maxTexSize;

  GLuint lmap_id[NUM_BLOCK_SURFS];
  GLuint addmap_id[NUM_BLOCK_SURFS];
  bool atlasesGenerated;
  vuint8 atlases_updated[NUM_BLOCK_SURFS];

  GLenum ClampToEdge;
  GLfloat max_anisotropy; // 1.0: off
  bool anisotropyExists;

  bool usingFPZBuffer;

  //bool HaveDepthClamp; // moved to drawer
  bool HaveStencilWrap;

  TArray<GLhandleARB> CreatedShaderObjects;
  TArray<VMeshModel *> UploadedModels;

  template<typename T> class VBO {
  protected:
    class VOpenGLDrawer *mOwner;
    GLuint vboId;
    int maxElems;
    bool isStream;

  public:
    TArray<T> data;

  public:
    inline VBO () noexcept : mOwner(nullptr), vboId(0), maxElems(0), isStream(false), data() {}
    inline VBO (VOpenGLDrawer *aOwner, bool aStream) noexcept : mOwner(aOwner), vboId(0), maxElems(0), isStream(aStream), data() {}

    static size_t getTypeSize () noexcept { return sizeof(T); }

    inline void setOwner (VOpenGLDrawer *aOwner) noexcept {
      if (mOwner != aOwner) {
        destroy();
        mOwner = aOwner;
      }
    }

    VBO (VOpenGLDrawer *aOwner, int aMaxElems, bool aStream=false) noexcept
      : mOwner(aOwner)
      , vboId(0)
      , maxElems(aMaxElems)
      , isStream(aStream)
      , data()
    {
      vassert(aMaxElems >= 0);
      if (aMaxElems == 0) return;
      data.setLength(aMaxElems);
      memset((void *)data.ptr(), 0, sizeof(T));
      GLDRW_RESET_ERROR();
      mOwner->p_glGenBuffersARB(1, &vboId);
      if (vboId == 0) Sys_Error("VBO: ctor (cannot create)");
      GLDRW_CHECK_ERROR("VBO: ctor (creation)");
      // reserve room for vertices
      mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
      const int len = (int)sizeof(T)*aMaxElems;
      mOwner->p_glBufferDataARB(GL_ARRAY_BUFFER, len, data.ptr(), (isStream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW));
      GLDRW_CHECK_ERROR("VBO: VBO ctor (allocating)");
      mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    inline ~VBO () noexcept { destroy(); }

    inline void destroy () noexcept {
      data.clear();
      if (vboId) {
        if (mOwner) mOwner->p_glDeleteBuffersARB(1, &vboId);
        vboId = 0;
      }
      maxElems = 0;
    }

    inline bool isValid () const noexcept { return (mOwner && vboId != 0); }
    inline GLuint getId () const noexcept { return vboId; }

    inline int capacity () const noexcept { return maxElems; }

    // this activates VBO
    void ensure (int count, int extraReserve=0) noexcept {
      if (!mOwner) Sys_Error("VBO: trying to ensure uninitialised VBO");
      const int oldlen = maxElems;
      if (count > oldlen || !vboId) {
        count += (extraReserve > 0 ? extraReserve : 0);
        maxElems = count;
        data.setLength(count);
        if (vboId) { mOwner->p_glDeleteBuffersARB(1, &vboId); vboId = 0; }
        // cleare newly allocated array part
        memset((void *)(data.ptr()+oldlen), 0, sizeof(T)*(unsigned)(count-oldlen));
        GLDRW_RESET_ERROR();
        mOwner->p_glGenBuffersARB(1, &vboId);
        if (vboId == 0) Sys_Error("VBO: ensure (cannot create)");
        GLDRW_CHECK_ERROR("VBO: ensure (creation)");
        // reserve room for vertices
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
        const int len = (int)sizeof(T)*count;
        mOwner->p_glBufferDataARB(GL_ARRAY_BUFFER, len, data.ptr(), (isStream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW));
        GLDRW_CHECK_ERROR("VBO: VBO ensure (allocating)");
      } else {
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
      }
      //p_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    inline void deactivate () const noexcept {
      if (mOwner) mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    inline void activate () const noexcept {
      if (!mOwner) Sys_Error("cannot activate uninitialised VBO");
      if (!vboId) Sys_Error("cannot activate empty VBO");
      mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
    }

    // this activates VBO
    inline void uploadData (int count, const T *buf=nullptr) noexcept {
      if (count <= 0) return;
      if (!mOwner) Sys_Error("VBO: trying to upload data to uninitialised VBO");
      ensure(count);
      if (!buf) buf = data.ptr();
      // upload data
      const int len = (int)sizeof(T)*count;
      mOwner->p_glBufferSubDataARB(GL_ARRAY_BUFFER, 0, len, buf);
    }

    // VBO should be activated!
    // this enables attribute
    inline void setupAttrib (GLuint attrIdx, int elemCount, ptrdiff_t byteOfs=0) noexcept {
      mOwner->p_glEnableVertexAttribArrayARB(attrIdx);
      mOwner->p_glVertexAttribPointerARB(attrIdx, elemCount, GL_FLOAT, GL_FALSE, sizeof(T), (void *)byteOfs);
    }

    inline void setupAttribNoEnable (GLuint attrIdx, int elemCount, ptrdiff_t byteOfs=0) noexcept {
      mOwner->p_glVertexAttribPointerARB(attrIdx, elemCount, GL_FLOAT, GL_FALSE, sizeof(T), (void *)byteOfs);
    }

    // VBO should be activated!
    inline void enableAttrib (GLuint attrIdx) const noexcept { mOwner->p_glEnableVertexAttribArrayARB(attrIdx); }
    inline void disableAttrib (GLuint attrIdx) const noexcept { mOwner->p_glDisableVertexAttribArrayARB(attrIdx); }
  };

  // VBO for sprite rendering
  VBO<TVec> vboSprite;

  // VBO for masked surface rendering
  VBO<SurfVertex> vboMaskedSurf;

  // for VBOs
  struct __attribute__((packed)) SkyVBOVertex {
    float x, y, z;
    float s, t;
  };
  static_assert(sizeof(SkyVBOVertex) == sizeof(float)*5, "invalid SkyVBOVertex size");

  // VBO for sky rendering (created lazily, because we don't know the proper size initially)
  VBO<SkyVBOVertex> vboSky;

  // console variables
  static VCvarI texture_filter;
  static VCvarI sprite_filter;
  static VCvarI model_filter;
  static VCvarI gl_texture_filter_anisotropic;
  static VCvarB clear;
  static VCvarB ext_anisotropy;
  static VCvarI multisampling_sample;
  static VCvarB gl_smooth_particles;
  static VCvarB gl_dump_vendor;
  static VCvarB gl_dump_extensions;

  // extensions
  bool CheckExtension (const char *ext);
  virtual void *GetExtFuncPtr (const char *name) = 0;

  void SetFade (vuint32 NewFade);

  // returns 0 if generation is disabled, and atlas is not created
  void GenerateLightmapAtlasTextures ();
  void DeleteLightmapAtlases ();

  virtual void FlushOneTexture (VTexture *tex, bool forced=false) override; // unload one texture
  virtual void FlushTextures (bool forced=false) override; // unload all textures
  void DeleteTextures ();
  void FlushTexture (VTexture *);
  void DeleteTexture (VTexture *);

  // if `ShadeColor` is not zero, ignore translation, and use "shaded" mode
  // high byte of `ShadeColor` means nothing
  void SetTexture (VTexture *Tex, int CMap, vuint32 ShadeColor=0);
  void SetDecalTexture (VTexture *Tex, VTextureTranslation *Translation, int CMap, vuint32 ShadeColor=0);
  void SetBrightmapTexture (VTexture *Tex);
  void SetSpriteLump (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool asPicture, vuint32 ShadeColor=0);
  void SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor=0);
  void SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor=0);

  void GenerateTexture (VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool asPicture, bool needUpdate, vuint32 ShadeColor);
  void UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal, int SourceLump);
  void UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal, int SourceLump);
  void UploadTexture (int width, int height, const rgba_t *data, bool doFringeRemove, int SourceLump);

  void DoHorizonPolygon (surface_t *surf);
  void DrawPortalArea (VPortal *Portal);

  GLhandleARB LoadShader (const char *progname, const char *incdir, GLenum Type, VStr FileName, const TArray<VStr> &defines=TArray<VStr>());
  GLhandleARB CreateProgram (const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader);

  void UploadModel (VMeshModel *Mdl);
  void UnloadModels ();

  void SetupTextureFiltering (int level); // level is taken from the appropriate cvar

  void SetupBlending (const RenderStyleInfo &ri);
  void RestoreBlending (const RenderStyleInfo &ri);

private: // bloom
  int bloomWidth = 0, bloomHeight = 0, bloomMipmapCount = 0;

  bool bloomColAvgValid = false;
  GLuint bloomFullSizeDownsampleFBOid = 0;
  GLuint bloomFullSizeDownsampleRBOid = 0;
  GLuint bloomColAvgGetterPBOid = 0;
  int bloomScrWdt, bloomScrHgt;
  unsigned bloomCurrentFBO = 0;

  inline void bloomResetFBOs () noexcept { bloomCurrentFBO = 0; }
  inline void bloomSwapFBOs () noexcept { bloomCurrentFBO ^= 1; }
  inline FBO *bloomGetActiveFBO () noexcept { return (bloomCurrentFBO ? &bloomscratch2FBO : &bloomscratchFBO); }
  inline FBO *bloomGetInactiveFBO () noexcept { return (bloomCurrentFBO ? &bloomscratchFBO : &bloomscratch2FBO); }

  void BloomDeinit ();
  void BloomAllocRBO (int width, int height, GLuint *RBO, GLuint *FBO);
  void BloomInitEffectTexture ();
  void BloomInitTextures ();
  void BloomDownsampleView (int ax, int ay, int awidth, int aheight);
  void BloomDarken ();
  void BloomDoGaussian ();
  void BloomDrawEffect (int ax, int ay, int awidth, int aheight);

public:
  virtual void Posteffect_Bloom (int ax, int ay, int awidth, int aheight) override;

public:
  virtual void SetMainFBO (bool forced=false) override;

  virtual void ClearCameraFBOs () override;
  virtual int GetCameraFBO (int texnum, int width, int height) override; // returns index or -1; (re)creates FBO if necessary
  virtual int FindCameraFBO (int texnum) override; // returns index or -1
  virtual void SetCameraFBO (int cfboindex) override;
  virtual GLuint GetCameraFBOTextureId (int cfboindex) override; // returns 0 if cfboindex is invalid

  // this copies main FBO to wipe FBO, so we can run wipe shader
  virtual void PrepareWipe () override;
  // render wipe from wipe to main FBO
  // should be called after `StartUpdate()`
  // and (possibly) rendering something to the main FBO
  // time is in seconds, from zero to...
  // returns `false` if wipe is complete
  // -1 means "show saved wipe screen"
  virtual bool RenderWipe (float time) override;

  void DestroyCameraFBOList ();

  void ActivateMainFBO ();
  FBO *GetMainFBO ();

public:
  // calculate sky texture U/V (S/T)
  // texture must be selected
  inline float CalcSkyTexCoordS (const TVec vert, const texinfo_t *tex, const float offs) const noexcept {
    return (DotProduct(vert, tex->saxis*tex_scale_x)+(tex->soffs-offs)*tex_scale_x)*tex_iw;
  }

  inline void CalcSkyTexCoordS2 (float *outs1, float *outs2, const TVec vert, const texinfo_t *tex, const float offs1, const float offs2) const noexcept {
    const float dp = DotProduct(vert, tex->saxis*tex_scale_x);
    *outs1 = (dp+(tex->soffs-offs1)*tex_scale_x)*tex_iw;
    *outs2 = (dp+(tex->soffs-offs2)*tex_scale_x)*tex_iw;
  }

  inline float CalcSkyTexCoordT (const TVec vert, const texinfo_t *tex) const noexcept {
    return (DotProduct(vert, tex->taxis*tex_scale_y)+tex->toffs*tex_scale_y)*tex_ih;
  }

public:
  #define VV_GLIMPORTS
  #define VGLAPIPTR(x,optional)  x##_t p_##x
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS

  inline void SelectTexture (int level) { p_glActiveTextureARB(GLenum(GL_TEXTURE0_ARB+level)); }

  static inline void SetColor (vuint32 c) {
    glColor4ub((vuint8)((c>>16)&255), (vuint8)((c>>8)&255), (vuint8)(c&255), (vuint8)((c>>24)&255));
  }

  static const char *glTypeName (GLenum type) {
    switch (type) {
      case /*GL_BYTE*/ 0x1400: return "byte";
      case /*GL_UNSIGNED_BYTE*/ 0x1401: return "ubyte";
      case /*GL_SHORT*/ 0x1402: return "short";
      case /*GL_UNSIGNED_SHORT*/ 0x1403: return "ushort";
      case /*GL_INT*/ 0x1404: return "int";
      case /*GL_UNSIGNED_INT*/ 0x1405: return "uint";
      case /*GL_FLOAT*/ 0x1406: return "float";
      case /*GL_2_BYTES*/ 0x1407: return "byte2";
      case /*GL_3_BYTES*/ 0x1408: return "byte3";
      case /*GL_4_BYTES*/ 0x1409: return "byte4";
      case /*GL_DOUBLE*/ 0x140A: return "double";
      case /*GL_FLOAT_VEC2*/ 0x8B50: return "vec2";
      case /*GL_FLOAT_VEC3*/ 0x8B51: return "vec3";
      case /*GL_FLOAT_VEC4*/ 0x8B52: return "vec4";
      case /*GL_INT_VEC2*/ 0x8B53: return "ivec2";
      case /*GL_INT_VEC3*/ 0x8B54: return "ivec3";
      case /*GL_INT_VEC4*/ 0x8B55: return "ivec4";
      case /*GL_BOOL*/ 0x8B56: return "bool";
      case /*GL_BOOL_VEC2*/ 0x8B57: return "bvec2";
      case /*GL_BOOL_VEC3*/ 0x8B58: return "bvec3";
      case /*GL_BOOL_VEC4*/ 0x8B59: return "bvec4";
      case /*GL_FLOAT_MAT2*/ 0x8B5A: return "mat2";
      case /*GL_FLOAT_MAT3*/ 0x8B5B: return "mat3";
      case /*GL_FLOAT_MAT4*/ 0x8B5C: return "mat4";
      case /*GL_SAMPLER_1D*/ 0x8B5D: return "sampler1D";
      case /*GL_SAMPLER_2D*/ 0x8B5E: return "sampler2D";
      case /*GL_SAMPLER_3D*/ 0x8B5F: return "sampler3D";
      case /*GL_SAMPLER_CUBE*/ 0x8B60: return "samplerCube";
      case /*GL_SAMPLER_1D_SHADOW*/ 0x8B61: return "sampler1D_shadow";
      case /*GL_SAMPLER_2D_SHADOW*/ 0x8B62: return "sampler2D_shadow";
    }
    return "<unknown>";
  }
};


#endif
