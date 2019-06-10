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
//**
//**  Defines shared by refresh and drawer
//**
//**************************************************************************
#ifndef _R_SHARED_H
#define _R_SHARED_H

#define VAVOOM_BIGGER_RGB_TABLE
//#define VAVOOM_HUGE_RGB_TABLE

//#include "fmd2defs.h"
#include "drawer.h"


// color maps
enum {
  CM_Default,
  CM_Inverse,
  CM_Gold,
  CM_Red,
  CM_Green,

  CM_Max
};

// simulate light fading using dark fog
enum {
  FADE_LIGHT = 0xff010101
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t {
  TVec saxis;
  float soffs;
  TVec taxis;
  float toffs;
  VTexture *Tex;
  /*bool*/vint32 noDecals;
  // 1.1 for solid surfaces
  // alpha for masked surfaces
  float Alpha;
  /*bool*/vint32 Additive;
  vuint8 ColorMap;
};


struct surface_t {
  enum {
    MAXWVERTS = 8+8, // maximum number of vertices in wsurf (world/wall surface)
  };

  enum {
    DF_MASKED       = 1u<<0, // this surface has "masked" texture
    DF_WSURF        = 1u<<1, // is this world/wall surface? such surfs are guaranteed to have space for `MAXWVERTS`
    DF_FIX_CRACKS   = 1u<<2, // this surface must be subdivised to fix "cracks"
    DF_CALC_LMAP    = 1u<<3, // calculate static lightmap
    //DF_FLIP_PLANE   = 1u<<4, // flip plane
    DF_NO_FACE_CULL = 1u<<5, // ignore face culling
  };

  surface_t *next;
  texinfo_t *texinfo; // points to owning `sec_surface_t`
  TPlane plane; // was pointer
  sec_plane_t *HorizonPlane;
  vuint32 Light; // light level and color
  vuint32 Fade;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  // not exposed to VC
  int lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  vuint32 queueframe; // this is used to prevent double queuing
  vuint32 dlightframe;
  vuint32 dlightbits;
  vuint32 drawflags; // DF_XXX
  int count;
  short texturemins[2];
  short extents[2];
  surfcache_t *CacheSurf;
  vuint32 fixvertbmp; // for world surfaces, this is bitmap of "fix" additional surfaces (bit 1 means "added fix")
  TVec verts[1]; // dynamic array

  // to use in renderer
  inline bool IsVisible (const TVec &point) const {
    return (!(drawflags&DF_NO_FACE_CULL) ? !plane.PointOnSide(point) : (plane.PointOnSide2(point) != 2));
  }

  inline int PointOnSide (const TVec &point) const {
    return plane.PointOnSide(point);
  }

  inline bool SphereTouches (const TVec &center, float radius) const {
    return plane.SphereTouches(center, radius);
  }

  inline float GetNormalZ () const { return plane.normal.z; }
  inline const TVec &GetNormal () const { return plane.normal; }
  inline float GetDist () const { return plane.dist; }

  inline void GetPlane (TPlane *p) const { *p = plane; }
};


// ////////////////////////////////////////////////////////////////////////// //
// camera texture
class VCameraTexture : public VTexture {
public:
  bool bNeedsUpdate;
  bool bUpdated;

  VCameraTexture (VName, int, int);
  virtual ~VCameraTexture () override;
  virtual bool CheckModified () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
  void CopyImage ();
  virtual VTexture *GetHighResolutionTexture () override;
};


class VRenderLevelShared;


// ////////////////////////////////////////////////////////////////////////// //
// base class for portals
class VPortal {
public:
  VRenderLevelShared *RLev;
  TArray<surface_t *> Surfs;
  int Level;
  bool stackedSector;

  VPortal (VRenderLevelShared *ARLev);
  virtual ~VPortal ();
  virtual bool NeedsDepthBuffer () const;
  virtual bool IsSky () const;
  virtual bool IsMirror () const;
  virtual bool IsStack () const;
  virtual bool MatchSky (class VSky *) const;
  virtual bool MatchSkyBox (VEntity *) const;
  virtual bool MatchMirror (TPlane *) const;
  void Draw (bool);
  virtual void DrawContents () = 0;

protected:
  void SetUpRanges (const refdef_t &refdef, VViewClipper &Range, bool Revert, bool SetFrustum);
};


// ////////////////////////////////////////////////////////////////////////// //
struct VMeshFrame {
  VStr Name;
  TVec Scale;
  TVec Origin;
  TVec *Verts;
  TVec *Normals;
  TPlane *Planes;
  //TArray<vuint8> ValidTris;
  // those are used for rebuilt frames (for multiframe models `TriCount` is constant)
  vuint32 TriCount;
  // cached offsets on OpenGL buffer
  vuint32 VertsOffset;
  vuint32 NormalsOffset;
};

#pragma pack(push,1)
struct VMeshSTVert {
  float S;
  float T;
};

struct VMeshTri {
  vuint16 VertIndex[3];
};

struct VMeshEdge {
  vuint16 Vert1;
  vuint16 Vert2;
  vint16 Tri1;
  vint16 Tri2;
};
#pragma pack(pop)

struct mmdl_t;

struct VMeshModel {
  VStr Name;
  int MeshIndex;
  TArray<VName> Skins;
  TArray<VMeshFrame> Frames;
  TArray<TVec> AllVerts;
  TArray<TVec> AllNormals;
  TArray<TPlane> AllPlanes;
  TArray<VMeshSTVert> STVerts;
  TArray<VMeshTri> Tris; // vetex indicies
  TArray<VMeshEdge> Edges; // for `Tris`
  bool loaded;
  bool Uploaded;
  bool HadErrors;
  vuint32 VertsBuffer;
  vuint32 IndexBuffer;
};


// ////////////////////////////////////////////////////////////////////////// //
void R_DrawViewBorder ();
void R_ParseMapDefSkyBoxesScript (VScriptParser *sc);
VName R_HasNamedSkybox (const VStr &aname);


// ////////////////////////////////////////////////////////////////////////// //
// POV related
extern TVec vieworg;
extern TVec viewforward;
extern TVec viewright;
extern TVec viewup;
extern TAVec viewangles;

//extern VCvarI r_fog;
extern VCvarF r_fog_r;
extern VCvarF r_fog_g;
extern VCvarF r_fog_b;
extern VCvarF r_fog_start;
extern VCvarF r_fog_end;
extern VCvarF r_fog_density;

extern VCvarB r_vsync;
extern VCvarB r_vsync_adaptive;

extern VCvarB r_fade_light;
extern VCvarF r_fade_factor;

extern VCvarF r_sky_bright_factor;

extern TFrustum view_frustum;

extern bool MirrorFlip;
extern bool MirrorClip;

extern rgba_t r_palette[256];
extern vuint8 r_black_color;
extern vuint8 r_white_color;

#if defined(VAVOOM_HUGE_RGB_TABLE)
# define VAVOOM_COLOR_COMPONENT_MAX  (128)
#elif defined(VAVOOM_BIGGER_RGB_TABLE)
# define VAVOOM_COLOR_COMPONENT_MAX  (64)
#else
# define VAVOOM_COLOR_COMPONENT_MAX  (32)
#endif
extern vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

extern int usegamma;
//extern const vuint8 gammatable[5][256];
extern const vuint8 *getGammaTable (int idx);

extern float PixelAspect;

extern VTextureTranslation ColorMaps[CM_Max];


//==========================================================================
//
//  R_LookupRGB
//
//==========================================================================
#if defined(VAVOOM_HUGE_RGB_TABLE)
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<13)&0x1fc000)|(((vuint32)clampToByte(g)<<6)&0x3f80)|((clampToByte(b)>>1)&0x7fU)];
}
#elif defined(VAVOOM_BIGGER_RGB_TABLE)
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<10)&0x3f000U)|(((vuint32)clampToByte(g)<<4)&0xfc0U)|((clampToByte(b)>>2)&0x3fU)];
}
#else
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<7)&0x7c00U)|(((vuint32)clampToByte(g)<<2)&0x3e0U)|((clampToByte(b)>>3)&0x1fU)];
}
#endif


#endif
