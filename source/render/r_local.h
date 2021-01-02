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


// ////////////////////////////////////////////////////////////////////////// //
// sprite orientations
enum {
  SPR_VP_PARALLEL_UPRIGHT, // 0 (default)
  SPR_FACING_UPRIGHT, // 1
  SPR_VP_PARALLEL, // 2: parallel to camera visplane
  SPR_ORIENTED, // 3
  SPR_VP_PARALLEL_ORIENTED, // 4 (xy billboard)
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED, // 5
  SPR_ORIENTED_OFS, // 6 (offset slightly by pitch -- for floor/ceiling splats)
  SPR_FLAT, // 7 (offset slightly by pitch -- for floor/ceiling splats; ignore roll angle)
  SPR_WALL, // 8 (offset slightly by yaw -- for wall splats; ignore pitch and roll angle)
};


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

  inline float PointDistance (const TVec &p) const { return esecplane.PointDistance(p); }
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
extern VCvarF gl_maxdist;
extern VCvarF r_lights_radius;
extern VCvarB r_models_strict;

extern VCvarB prof_r_world_prepare;
extern VCvarB prof_r_bsp_collect;
extern VCvarB prof_r_bsp_world_render;
extern VCvarB prof_r_bsp_mobj_render;
extern VCvarB prof_r_bsp_mobj_collect;

extern VCvarB r_shadowmaps;
extern VCvarB r_shadows;


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


// ////////////////////////////////////////////////////////////////////////// //
static VVA_OKUNUSED inline bool IsAnyProfRActive () noexcept {
  return
    prof_r_world_prepare.asBool() ||
    prof_r_bsp_collect.asBool() ||
    prof_r_bsp_world_render.asBool() ||
    prof_r_bsp_mobj_render.asBool();
}


// ////////////////////////////////////////////////////////////////////////// //
// r_model
void R_InitModels ();
void R_FreeModels ();

void R_LoadAllModelsSkins ();

int R_SetMenuPlayerTrans (int, int, int);

void R_DrawLightBulb (const TVec &Org, const TAVec &Angles, vuint32 rgbLight, ERenderPass Pass, bool isShadowVol, float ScaleX=1.0f, float ScaleY=1.0);

// used to check for view models
bool R_HaveClassModelByName (VName clsName);


// ////////////////////////////////////////////////////////////////////////// //
#include "r_local_sky.h"
#include "r_local_rshared.h"
#include "r_local_rlmap.h"
#include "r_local_radv.h"


#endif
