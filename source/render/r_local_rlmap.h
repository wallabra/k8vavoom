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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#ifndef VAVOOM_R_LOCAL_HEADER_RLMAP
#define VAVOOM_R_LOCAL_HEADER_RLMAP


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
  void SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf);
  void AddDynamicLights (surface_t *surf);

  // lightmap cache manager
  // WARNING: `SurfPrepareForRender()` should be already called!
  bool QueueLMapSurface (surface_t *surf); // returns `true` if surface was queued

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


#endif
