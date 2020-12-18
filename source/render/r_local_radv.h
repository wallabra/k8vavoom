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
#ifndef VAVOOM_R_LOCAL_HEADER_RADV
#define VAVOOM_R_LOCAL_HEADER_RADV


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShadowVolume : public VRenderLevelShared {
private:
  vuint32 CurrLightColor;
  // built in `BuildMobjsInCurrLight()`
  // used in rendering of shadow and light things (only)
  TArray<VEntity *> mobjsInCurrLightModels;
  TArray<VEntity *> mobjsInCurrLightSprites;
  int LightsRendered;
  int DynLightsRendered;
  // set this to true before calling `RenderLightShadows()` to indicate dynamic light
  bool DynamicLights;

  // keep 'em here, so we don't have to traverse BSP several times
  TArray<surface_t *> shadowSurfacesSolid;
  TArray<surface_t *> shadowSurfacesMasked;
  TArray<surface_t *> lightSurfacesSolid;
  TArray<surface_t *> lightSurfacesMasked;

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

  void RenderSceneLights (const refdef_t *RD, const VViewClipper *Range);
  void RenderSceneStaticLights (const refdef_t *RD, const VViewClipper *Range);
  void RenderSceneDynamicLights (const refdef_t *RD, const VViewClipper *Range);

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
  void CollectAdvLightBSPNode (int bspnum, const float *bbox);

  // collector entr point
  // `CurrLightPos` and `CurrLightRadius` should be set
  void CollectLightShadowSurfaces ();

  void RenderShadowSurfaceList ();
  void RenderLightSurfaceList ();

  void DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);
  void DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);

  // WARNING! may modify `Pos`
  void RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD, const VViewClipper *Range,
                           TVec &Pos, float Radius, float LightMin, vuint32 Color,
                           TVec coneDir=TVec(0.0f, 0.0f, 0.0f), float coneAngle=0.0f, bool forceRender=false);

  // things
  void BuildMobjsInCurrLight (bool doShadows, bool collectSprites);

  void RenderMobjsAmbient ();
  void RenderMobjsTextures ();
  void RenderMobjsLight (VEntity *owner);
  void RenderMobjsShadow (VEntity *owner, vuint32 dlflags);
  void RenderMobjsShadowMap (VEntity *owner, const unsigned int facenum, vuint32 dlflags);
  void RenderMobjsFog ();

  void RenderMobjSpriteShadowMap (VEntity *owner, const unsigned int facenum, int spShad, vuint32 dlflags);
  // doesn't do any checks, just renders it
  void RenderMobjShadowMapSprite (VEntity *ent, const unsigned int facenum, const bool allowRotating);


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
  virtual bool IsShadowMapRenderer () const noexcept override;
};


#endif
