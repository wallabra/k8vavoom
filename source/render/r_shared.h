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
//**
//**  Defines shared by refresh and drawer
//**
//**************************************************************************
#ifndef VAVOOM_RENDER_SHARED_H
#define VAVOOM_RENDER_SHARED_H

#include "drawer.h"


// ////////////////////////////////////////////////////////////////////////// //
// lightmap texture dimensions
#define BLOCK_WIDTH      (128)
#define BLOCK_HEIGHT     (128)
// maximum lightmap textures
#define NUM_BLOCK_SURFS  (64)


// ////////////////////////////////////////////////////////////////////////// //
// color maps
enum {
  CM_Default,
  CM_Inverse,
  CM_Gold,
  CM_Red,
  CM_Green,

  CM_Max,
};

// simulate light fading using dark fog
enum {
  FADE_LIGHT = 0xff010101u,
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t {
  TVec saxis;
  float soffs;
  TVec taxis;
  float toffs;
  VTexture *Tex;
  vint32 noDecals;
  // 1.1f for solid surfaces
  // alpha for masked surfaces
  // not always right, tho (1.0f vs 1.1f); need to be checked and properly fixed
  float Alpha;
  vint32 Additive;
  vuint8 ColorMap;

  inline bool isEmptyTexture () const { return (!Tex || Tex->Type == TEXTYPE_Null); }

  // call this to check if we need to change OpenGL texture
  inline bool needChange (const texinfo_t &other, const vuint32 upframe) const {
    if (&other == this) return false;
    return
      Tex != other.Tex ||
      ColorMap != other.ColorMap ||
      FASI(saxis) != FASI(other.saxis) ||
      FASI(soffs) != FASI(other.soffs) ||
      FASI(taxis) != FASI(other.taxis) ||
      FASI(toffs) != FASI(other.toffs) ||
      (Tex && Tex->lastUpdateFrame != upframe);
  }

  // call this to cache info for `needChange()`
  inline void updateLastUsed (const texinfo_t &other) {
    if (&other == this) return;
    Tex = other.Tex;
    ColorMap = other.ColorMap;
    saxis = other.saxis;
    soffs = other.soffs;
    taxis = other.taxis;
    toffs = other.toffs;
    // other fields doesn't matter
  }

  inline void resetLastUsed () {
    Tex = nullptr; // it is enough, but to be sure...
    ColorMap = 255; // impossible colormap
  }

  inline void initLastUsed () {
    saxis = taxis = TVec(-99999, -99999, -99999);
    soffs = toffs = -99999;
    Tex = nullptr;
    noDecals = false;
    Alpha = -666;
    Additive = 666;
    ColorMap = 255;
  }
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

  enum {
    TF_TOP     = 1u<<0,
    TF_BOTTOM  = 1u<<1,
    TF_MIDDLE  = 1u<<2,
    TF_FLOOR   = 1u<<3,
    TF_CEILING = 1u<<4,
    TF_TOPHACK = 1u<<5,
  };

  surface_t *next;
  texinfo_t *texinfo; // points to owning `sec_surface_t`
  TPlane plane; // was pointer
  sec_plane_t *HorizonPlane;
  vuint32 Light; // light level and color
  vuint32 Fade;
  float glowFloorHeight;
  float glowCeilingHeight;
  vuint32 glowFloorColor;
  vuint32 glowCeilingColor;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  vuint32 typeFlags; // TF_xxx
  // not exposed to VC
  int lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  vuint32 queueframe; // this is used to prevent double queuing
  vuint32 dlightframe;
  vuint32 dlightbits;
  vuint32 drawflags; // DF_XXX
  int count;
  /*short*/int texturemins[2];
  /*short*/int extents[2];
  surfcache_t *CacheSurf;
  int plvisible; // cached visibility flag, set in main BSP collector (VRenderLevelShared::SurfCheckAndQueue)
  //vuint32 fixvertbmp; // for world surfaces, this is bitmap of "fix" additional surfaces (bit 1 means "added fix")
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
  inline float CalcDistance (const TVec &p) const { return plane.CalcDistance(p); }

  inline void GetPlane (TPlane *p) const { *p = plane; }
};


// ////////////////////////////////////////////////////////////////////////// //
template<typename TBase> class V2DCache {
public:
  struct Item : public TBase {
    // position in light surface
    //int s, t; // must present in `TBase` (for now)
    // size
    int width, height;
    // line list in block
    Item *bprev;
    Item *bnext;
    // cache list in line
    Item *lprev;
    Item *lnext;
    // next free block in `blockbuf`
    Item *freeChain;
    // light surface index
    vuint32 atlasid;
    Item **owner;
    vuint32 lastframe;
  };

  struct AtlasInfo {
    int width;
    int height;

    inline AtlasInfo () noexcept : width(0), height(0) {}
    inline bool isValid () const noexcept { return (width > 0 && height > 0 && width <= 4096 && height <= 4096); }
  };

protected:

  // block pool
  struct VBlockPool {
  public:
    // number of `Item` items in one pool entry
    enum { NUM_CACHE_BLOCKS_IN_POOL_ENTRY = 4096u };

  public:
    struct PoolEntry {
      Item page[NUM_CACHE_BLOCKS_IN_POOL_ENTRY];
      PoolEntry *prev;
    };

  public:
    PoolEntry *tail;
    unsigned tailused;
    unsigned entries; // full

  public:
    VBlockPool () noexcept : tail(nullptr), tailused(0), entries(0) {}
    VBlockPool (const VBlockPool &) = delete;
    VBlockPool &operator = (const VBlockPool &) = delete;
    ~VBlockPool () noexcept { clear(); }

    inline unsigned itemCount () noexcept { return entries*NUM_CACHE_BLOCKS_IN_POOL_ENTRY+tailused; }

    void clear () noexcept {
      while (tail) {
        PoolEntry *c = tail;
        tail = c->prev;
        Z_Free(c);
      }
      tailused = 0;
      entries = 0;
    }

    Item *alloc () noexcept {
      if (tail && tailused < NUM_CACHE_BLOCKS_IN_POOL_ENTRY) return &tail->page[tailused++];
      if (tail) ++entries; // full entries counter
      // allocate new pool entry
      PoolEntry *c = (PoolEntry *)Z_Calloc(sizeof(PoolEntry));
      c->prev = tail;
      tail = c;
      tailused = 1;
      return &tail->page[0];
    }

    void resetFrames () noexcept {
      for (PoolEntry *c = tail; c; c = c->prev) {
        Item *s = &c->page[0];
        for (unsigned count = NUM_CACHE_BLOCKS_IN_POOL_ENTRY; count--; ++s) s->lastframe = 0;
      }
    }
  };

  struct Atlas {
    int width;
    int height;
    vuint32 id;
    Item *blocks;

    inline bool isValid () const noexcept { return (width > 0 && height > 0 && width <= 4096 && height <= 4096); }
  };

protected:
  TArray<Atlas> atlases;
  Item *freeblocks;
  VBlockPool blockpool;
  vuint32 lastOldFreeFrame;

public:
  vuint32 cacheframecount;

protected:
  Item *performBlockVSplit (int width, int height, Item *block, bool forceAlloc=false) {
    vassert(block->height >= height);
    vassert(!block->lnext);
    const vuint32 aid = block->atlasid;

    if (block->height > height) {
      #ifdef VV_DEBUG_LMAP_ALLOCATOR
      GLog.Logf(NAME_Debug, "  vsplit: aid=%d; w=%d; h=%d", aid, block->width, block->height);
      #endif
      Item *other = getFreeBlock(forceAlloc);
      if (!other) return nullptr;
      other->s = 0;
      other->t = block->t+height;
      other->width = block->width;
      other->height = block->height-height;
      other->lnext = nullptr;
      other->lprev = nullptr;
      other->bnext = block->bnext;
      if (other->bnext) other->bnext->bprev = other;
      block->bnext = other;
      other->bprev = block;
      block->height = height;
      other->owner = nullptr;
      other->atlasid = aid;
      forceAlloc = true; // we need second block unconditionally
    }

    {
      Item *other = getFreeBlock(forceAlloc);
      if (!other) return nullptr;
      other->s = block->s+width;
      other->t = block->t;
      other->width = block->width-width;
      other->height = block->height;
      other->lnext = nullptr;
      block->lnext = other;
      other->lprev = block;
      block->width = width;
      other->owner = nullptr;
      other->atlasid = aid;
    }

    return block;
  }

  Item *performBlockHSplit (int width, Item *block, bool forceAlloc=false) {
    if (block->width < width) return nullptr; // just in case
    if (block->width == width) return block; // nothing to do
    vassert(block->width > width);
    #ifdef VV_DEBUG_LMAP_ALLOCATOR
    GLog.Logf(NAME_Debug, "  hsplit: aid=%u; w=%d; h=%d", block->atlasid, block->width, block->height);
    #endif
    Item *other = getFreeBlock(forceAlloc);
    if (!other) return nullptr;
    other->s = block->s+width;
    other->t = block->t;
    other->width = block->width-width;
    other->height = block->height;
    other->lnext = block->lnext;
    if (other->lnext) other->lnext->lprev = other;
    block->lnext = other;
    other->lprev = block;
    block->width = width;
    other->owner = nullptr;
    other->atlasid = block->atlasid;
    return block;
  }

  Item *getFreeBlock (bool forceAlloc) {
    Item *res = freeblocks;
    if (res) {
      freeblocks = res->freeChain;
    } else {
      // no free blocks
      if (!forceAlloc && blockpool.itemCount() >= 32768) {
        // too many blocks
        flushOldCaches();
        res = freeblocks;
        if (res) freeblocks = res->freeChain; else res = blockpool.alloc(); // force-allocate anyway
      } else {
        // allocate new block
        res = blockpool.alloc();
      }
    }
    if (res) memset(res, 0, sizeof(Item));
    return res;
  }

public:
  void debugDump () noexcept {
    GLog.Logf("=== V2DCache: %d atlases ===", atlases.length());
    for (auto &&it : atlases) {
      GLog.Logf(" -- atlas #%u size=(%d,%d)--", it.id, it.width, it.height);
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        vassert(blines->atlasid == it.id);
        GLog.Logf("  line: ofs=(%d,%d); size=(%d,%d)", blines->s, blines->t, blines->width, blines->height);
        for (Item *block = blines; block; block = block->lnext) {
          GLog.Logf("   block: ofs=(%d,%d); size=(%d,%d)", block->s, block->t, block->width, block->height);
        }
      }
    }
  }

public:
  V2DCache () noexcept : atlases(), freeblocks(nullptr), blockpool(), lastOldFreeFrame(0), cacheframecount(0) {}
  ~V2DCache () noexcept { /*clear();*/ }

  V2DCache (const V2DCache &) = delete;
  V2DCache &operator = (const V2DCache &) = delete;

  inline int getAtlasCount () const noexcept { return atlases.length(); }

  // this resets all allocated blocks (but doesn't deallocate atlases)
  void reset () noexcept {
    clearCacheInfo();
    // clear blocks
    blockpool.clear();
    freeblocks = nullptr;
    //initLightChain();
    // release all lightmap atlases
    for (int idx = atlases.length()-1; idx >= 0; --idx) releaseAtlas(atlases[idx].id);
    atlases.reset();
    /*
    // setup lightmap atlases (no allocations, all atlases are free)
    for (auto &&it : atlases) {
      vassert(it.isValid());
      Item *block = blockpool.alloc();
      memset((void *)block, 0, sizeof(Item));
      block->width = it.width;
      block->height = it.height;
      block->atlasid = it.id;
      it.blocks = block;
    }
    */
  }

  // zero all `lastframe` fields
  void zeroFrames () noexcept {
    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          block->lastframe = 0;
        }
      }
    }
    lastOldFreeFrame = 0;
  }

  virtual void releaseAtlas (vuint32 id) noexcept = 0;

  virtual void resetBlock (Item *block) noexcept = 0;

  // called when we need to clear all cache pointers
  // calls `resetBlock()`
  void resetAllBlocks () noexcept {
    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          resetBlock(block);
        }
      }
    }
  }

  // `FreeSurfCache()` calls this with `true`
  Item *freeBlock (Item *block, bool checkLines) noexcept {
    resetBlock(block);

    if (block->owner) {
      *block->owner = nullptr;
      block->owner = nullptr;
    }

    if (block->lnext && !block->lnext->owner) {
      Item *other = block->lnext;
      block->width += other->width;
      block->lnext = other->lnext;
      if (block->lnext) block->lnext->lprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->lprev && !block->lprev->owner) {
      Item *other = block;
      block = block->lprev;
      block->width += other->width;
      block->lnext = other->lnext;
      if (block->lnext) block->lnext->lprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->lprev || block->lnext || !checkLines) return block;

    if (block->bnext && !block->bnext->lnext) {
      Item *other = block->bnext;
      block->height += other->height;
      block->bnext = other->bnext;
      if (block->bnext) block->bnext->bprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->bprev && !block->bprev->lnext) {
      Item *other = block;
      block = block->bprev;
      block->height += other->height;
      block->bnext = other->bnext;
      if (block->bnext) block->bnext->bprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    return block;
  }

  void flushOldCaches () noexcept {
    if (lastOldFreeFrame == cacheframecount) return; // already done
    for (auto &&it : atlases) {
      vassert(it.isValid());
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          if (block->owner && cacheframecount != block->lastframe) block = freeBlock(block, false);
        }
        if (!blines->owner && !blines->lprev && !blines->lnext) blines = freeBlock(blines, true);
      }
      //FIXME: release free atlases here
    }
    if (!freeblocks) {
      //Sys_Error("No more free blocks");
      GLog.Logf(NAME_Warning, "Surface cache overflow, and no old surfaces found");
    } else {
      GLog.Logf(NAME_Debug, "Surface cache overflow, old caches flushed");
    }
  }

  Item *allocBlock (int width, int height) noexcept {
    vassert(width > 0);
    vassert(height > 0);
    #ifdef VV_DEBUG_LMAP_ALLOCATOR
    GLog.Logf(NAME_Debug, "V2DCache::allocBlock: w=%d; h=%d", width, height);
    #endif

    Item *splitBlock = nullptr;

    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        if (blines->height < height) continue;
        if (blines->height == height) {
          for (Item *block = blines; block; block = block->lnext) {
            if (block->owner) continue;
            if (block->width < width) continue;
            block = performBlockHSplit(width, block);
            vassert(block->atlasid == it.id);
            return block;
          }
        }
        // possible vertical split?
        if (!splitBlock && !blines->lnext && blines->height >= height) {
          splitBlock = blines;
          vassert(splitBlock->atlasid == it.id);
        }
      }
    }

    if (splitBlock) {
      Item *other = performBlockVSplit(width, height, splitBlock);
      if (other) return other;
    }

    // try to get a new atlas
    AtlasInfo nfo = allocAtlas((vuint32)atlases.length(), width, height);
    if (!nfo.isValid()) return nullptr;

    // setup new atlas
    Atlas &atp = atlases.alloc();
    atp.width = nfo.width;
    atp.height = nfo.height;
    atp.id = (vuint32)atlases.length()-1;
    atp.blocks = blockpool.alloc();
    memset((void *)atp.blocks, 0, sizeof(Item));
    atp.blocks->width = atp.width;
    atp.blocks->height = atp.height;
    atp.blocks->atlasid = atp.id;

    // first split ever
    {
      Item *blines = atp.blocks;
      if (blines->height < height || blines->width < width) return nullptr; // something is wrong with this atlas
      Item *other;
      if (blines->height == height) {
        other = performBlockHSplit(width, blines, true);
      } else {
        other = performBlockVSplit(width, height, blines, true);
      }
      if (other) return other;
    }

    GLog.Logf(NAME_Error, "Surface cache overflow!");
    return nullptr;
  }

  // this is called in `reset()` to perform necessary cleanups
  // it is called before any other actions are done
  virtual void clearCacheInfo () noexcept = 0;

  // this method will be called when allocator runs out of atlases
  // return invalid atlas info if no more free atlases available
  // `aid` is the number of this atlas (starting from zero)
  // it can be used to manage atlas resources
  virtual AtlasInfo allocAtlas (vuint32 aid, int minwidth, int minheight) noexcept = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
struct surfcache_t {
  // position in light surface
  int s, t;
  surfcache_t *chain; // list of drawable surfaces for the atlas
  vuint32 Light; // checked for strobe flash
  int dlight;
  surface_t *surf;
};


// ////////////////////////////////////////////////////////////////////////// //
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
// one alias model frame
struct VMeshFrame {
  VStr Name;
  TVec Scale;
  TVec Origin;
  TVec *Verts;
  TVec *Normals;
  TPlane *Planes;
  vuint32 TriCount;
  // cached offsets on OpenGL buffer
  vuint32 VertsOffset;
  vuint32 NormalsOffset;
};

#pragma pack(push,1)
// texture coordinates
struct VMeshSTVert {
  float S;
  float T;
};

// one mesh triangle
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
  struct SkinInfo {
    VName fileName;
    int textureId; // -1: not loaded yet
    int shade; // -1: none
  };

  VStr Name;
  int MeshIndex;
  TArray<SkinInfo> Skins;
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


// ////////////////////////////////////////////////////////////////////////// //
// POV related
// k8: this should be prolly moved to renderer, and recorded in render list
// if i'll do that, i will be able to render stacked sectors, and mirrors
// without portals.
//
// in render lists it is prolly enough to store current view transformation
// matrix, because surface visibility flags are already set by the queue manager.
//
// another thing to consider is queue manager limitation: one surface can be
// queued only once. this is not a hard limitation, though, as queue manager is
// using arrays to keep surface pointers, but it is handy for various render
// checks. we prolly need to increment queue frame counter when view changes.
extern TVec vieworg;
extern TVec viewforward;
extern TVec viewright;
extern TVec viewup;
extern TAVec viewangles;
extern TFrustum view_frustum;

extern bool MirrorFlip;
extern bool MirrorClip;


// ////////////////////////////////////////////////////////////////////////// //
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

extern float PixelAspect;

extern VTextureTranslation ColorMaps[CM_Max];


#endif
