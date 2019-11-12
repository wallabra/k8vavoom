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
struct particle_t;
struct dlight_t;


// ////////////////////////////////////////////////////////////////////////// //
// there is little need to use bigger translation tables
// usually, 5 bits of color info is enough, so 32x32x32
// color cube is ok for our purposes. but meh...

//#define VAVOOM_RGB_TABLE_7_BITS
#define VAVOOM_RGB_TABLE_6_BITS
//#define VAVOOM_RGB_TABLE_5_BITS


// ////////////////////////////////////////////////////////////////////////// //
struct refdef_t {
  int x, y;
  int width, height;
  float fovx, fovy;
  bool drawworld;
  bool DrawCamera;
};


// ////////////////////////////////////////////////////////////////////////// //
struct fakefloor_t {
  sec_plane_t floorplane;
  sec_plane_t ceilplane;
  sec_params_t params;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnim;

struct decal_t {
  enum {
    SlideFloor = 0x0001U, // curz: offset with floorz, slide with it
    SlideCeil = 0x0002U, // curz: offset with ceilingz, slide with it
    FlipX = 0x0010U,
    FlipY = 0x0020U,
    Fullbright = 0x0100U,
    Fuzzy = 0x0200U,
    SideDefOne = 0x0800U,
    NoMidTex = 0x1000U, // don't render on middle texture
    NoTopTex = 0x2000U, // don't render on top texture
    NoBotTex = 0x4000U, // don't render on bottom texture
  };
  decal_t *prev; // in this seg
  decal_t *next; // in this seg
  seg_t *seg;
  sector_t *slidesec; // backsector for SlideXXX
  VName dectype;
  //VName picname;
  VTextureID texture;
  int translation;
  vuint32 flags;
  // z and x positions has no image offset added
  float orgz; // original z position
  float curz; // z position (offset with floor/ceiling TexZ if not midtex, see `flags`)
  float xdist; // offset from linedef start, in map units
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
class VRenderLevelPublic : public VInterface {
public: //k8: i am too lazy to write accessor methods
  bool staticLightsFiltered;

  // base planes to create fov-based frustum
  TClipBase clip_base;
  refdef_t refdef;

public:
  struct LightInfo {
    TVec origin;
    float radius;
    vuint32 color;
    bool active;
  };

public:
  VRenderLevelPublic () : staticLightsFiltered(false) {}

  virtual void PreRender () = 0;
  virtual void SegMoved (seg_t *) = 0;
  virtual void SetupFakeFloors (sector_t *) = 0;
  virtual void RenderPlayerView () = 0;

  virtual void AddStaticLightRGB (VEntity *Owner, const TVec&, float, vuint32) = 0;
  virtual void MoveStaticLightByOwner (VEntity *Owner, const TVec &origin) = 0;
  virtual void ClearReferences () = 0;

  virtual dlight_t *AllocDlight (VThinker*, const TVec &lorg, float radius, int lightid=-1) = 0;
  virtual dlight_t *FindDlightById (int lightid) = 0;
  virtual void DecayLights (float) = 0;
  virtual void RemoveOwnedLight (VThinker *Owner) = 0;

  virtual particle_t *NewParticle (const TVec &porg) = 0;

  virtual int GetStaticLightCount () const = 0;
  virtual LightInfo GetStaticLight (int idx) const = 0;

  virtual int GetDynamicLightCount () const = 0;
  virtual LightInfo GetDynamicLight (int idx) const = 0;

  virtual void NukeLightmapCache () = 0;
  virtual void ResetLightmaps (bool recalcNow) = 0;

  virtual void FullWorldUpdate (bool forceClientOrigin) = 0;

  virtual bool isNeedLightmapCache () const noexcept = 0;
  virtual void saveLightmaps (VStream *strm) = 0;
  virtual bool loadLightmaps (VStream *strm) = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// r_data
void R_InitData ();
void R_ShutdownData ();
void R_InstallSprite (const char *, int);
void R_InstallSpriteComplete ();
bool R_AreSpritesPresent (int);
int R_ParseDecorateTranslation (VScriptParser *, int);
int R_GetBloodTranslation (int);
void R_ParseEffectDefs ();
VLightEffectDef *R_FindLightEffect (VStr Name);

// r_main
void R_Init (); // Called by startup code.
void R_Start (VLevel *);
void R_SetViewSize (int blocks);
void R_RenderPlayerView ();
VTextureTranslation *R_GetCachedTranslation (int, VLevel *);

// r_things
// ignoreVScr: draw on framebuffer, ignore virutal screen
void R_DrawSpritePatch (float x, float y, int sprite, int frame=0, int rot=0,
                        int TranslStart=0, int TranslEnd=0, int Color=0, float scale=1.0f,
                        bool ignoreVScr=false);
void R_InitSprites ();

//  2D graphics
void R_DrawPic (int x, int y, int handle, float Alpha=1.0f);
void R_DrawPicScaled (int x, int y, int handle, float scale, float Alpha=1.0f);
void R_DrawPicFloat (float x, float y, int handle, float Alpha=1.0f);
// wdt and hgt are in [0..1] range
void R_DrawPicPart (int x, int y, float pwdt, float phgt, int handle, float Alpha=1.0f);
void R_DrawPicFloatPart (float x, float y, float pwdt, float phgt, int handle, float Alpha=1.0f);
void R_DrawPicPartEx (int x, int y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha=1.0f);
void R_DrawPicFloatPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha=1.0f);

float R_GetAspectRatio ();

bool R_ModelNoSelfShadow (VName clsName);

#ifdef SERVER
// r_sky
void R_InitSkyBoxes ();
#endif

// WARNING! this can call VM code!
bool R_IsSkyFlatPlane (sec_plane_t *SPlane);

VName R_HasNamedSkybox (VStr aname);


// ////////////////////////////////////////////////////////////////////////// //
// camera texture
class VCameraTexture : public VTexture {
public:
  bool bUsedInFrame;
  double NextUpdateTime; // systime
  bool bUpdated;

  VCameraTexture (VName, int, int);
  virtual ~VCameraTexture () override;
  virtual bool CheckModified () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
  void CopyImage ();
  bool NeedUpdate ();
  virtual VTexture *GetHighResolutionTexture () override;
};


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<int> AllModelTextures;
extern int validcount; // defined in "sv_main.cpp"
extern int validcountSZCache; // defined in "sv_main.cpp"

extern rgba_t r_palette[256];
extern vuint8 r_black_color;
extern vuint8 r_white_color;

#if defined(VAVOOM_RGB_TABLE_7_BITS)
# define VAVOOM_COLOR_COMPONENT_MAX  (128)
# define VAVOOM_COLOR_COMPONENT_BITS (7)
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# define VAVOOM_COLOR_COMPONENT_MAX  (64)
# define VAVOOM_COLOR_COMPONENT_BITS (6)
#else
# define VAVOOM_COLOR_COMPONENT_MAX  (32)
# define VAVOOM_COLOR_COMPONENT_BITS (5)
#endif
extern vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

extern int usegamma;
//extern const vuint8 gammatable[5][256];
extern const vuint8 *getGammaTable (int idx);


//==========================================================================
//
//  R_LookupRGB
//
//==========================================================================
#if defined(VAVOOM_RGB_TABLE_7_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<13)&0x1fc000)|(((vuint32)clampToByte(g)<<6)&0x3f80)|((clampToByte(b)>>1)&0x7fU)];
}
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# if defined(VAVOOM_RGB_TABLE_7_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<10)&0x3f000U)|(((vuint32)clampToByte(g)<<4)&0xfc0U)|((clampToByte(b)>>2)&0x3fU)];
}
#elif defined(VAVOOM_RGB_TABLE_5_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_7_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<7)&0x7c00U)|(((vuint32)clampToByte(g)<<2)&0x3e0U)|((clampToByte(b)>>3)&0x1fU)];
}
#else
#  error "choose RGB table size"
#endif
