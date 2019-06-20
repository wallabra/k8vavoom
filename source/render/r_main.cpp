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
#include <limits.h>
#include <float.h>

#include "gamedefs.h"
#include "r_local.h"

//#define VAVOOM_DEBUG_PORTAL_POOL

extern int light_reset_surface_cache; // in r_light_reg.cpp
extern VCvarB r_draw_pobj;
extern VCvarB r_advlight_opt_optimise_scissor;
extern VCvarB dbg_clip_dump_added_ranges;

static VCvarB dbg_autoclear_automap("dbg_autoclear_automap", false, "Clear automap before rendering?", 0/*CVAR_Archive*/);
static VCvarB r_lightflood_check_plane_angles("r_lightflood_check_plane_angles", true, "Check seg planes angles in light floodfill?", CVAR_Archive);


void R_FreeSkyboxData ();


int screenblocks = 0;

TVec vieworg(0, 0, 0);
TVec viewforward(0, 0, 0);
TVec viewright(0, 0, 0);
TVec viewup(0, 0, 0);
TAVec viewangles(0, 0, 0);

TFrustum view_frustum;

VCvarB r_chasecam("r_chasecam", false, "Chasecam mode.", CVAR_Archive);
VCvarF r_chase_dist("r_chase_dist", "32", "Chasecam distance.", CVAR_Archive);
VCvarF r_chase_up("r_chase_up", "128", "Chasecam position: up.", CVAR_Archive);
VCvarF r_chase_right("r_chase_right", "0", "Chasecam position: right.", CVAR_Archive);
VCvarI r_chase_front("r_chase_front", "0", "Chasecam position: front.", CVAR_Archive);

//VCvarI r_fog("r_fog", "0", "Fog mode (0:GL_LINEAR; 1:GL_LINEAR; 2:GL_EXP; 3:GL_EXP2; add 4 to get \"nicer\" fog).");
VCvarB r_fog_test("r_fog_test", false, "Is fog testing enabled?");
VCvarF r_fog_r("r_fog_r", "0.5", "Fog color: red component.");
VCvarF r_fog_g("r_fog_g", "0.5", "Fog color: green component.");
VCvarF r_fog_b("r_fog_b", "0.5", "Fog color: blue component.");
VCvarF r_fog_start("r_fog_start", "1", "Fog start distance.");
VCvarF r_fog_end("r_fog_end", "2048", "Fog end distance.");
VCvarF r_fog_density("r_fog_density", "0.5", "Fog density.");

VCvarB r_advrender_translucent_as_light("r_advrender_translucent_as_light", false, "Render translucent walls as light affectors in advrender?", CVAR_Archive);

VCvarI aspect_ratio("r_aspect_ratio", "0", "Aspect ratio correction mode ([0..3]: normal/4:3/16:9/16:10).", CVAR_Archive);
VCvarB r_interpolate_frames("r_interpolate_frames", true, "Use frame interpolation for smoother rendering?", CVAR_Archive);
VCvarB r_vsync("r_vsync", true, "VSync mode.", CVAR_Archive);
VCvarB r_vsync_adaptive("r_vsync_adaptive", true, "Use adaptive VSync mode.", CVAR_Archive);
VCvarB r_fade_light("r_fade_light", "0", "Fade lights?", CVAR_Archive);
VCvarF r_fade_factor("r_fade_factor", "7", "Fade actor lights?", CVAR_Archive);
VCvarF r_sky_bright_factor("r_sky_bright_factor", "1", "Skybright actor factor.", CVAR_Archive);

VCvarF r_lights_radius("r_lights_radius", "3072", "Lights out of this radius (from camera) will be dropped.", CVAR_Archive);
//static VCvarB r_lights_cast_many_rays("r_lights_cast_many_rays", false, "Cast more rays to better check light visibility (usually doesn't make visuals any better)?", CVAR_Archive);
//static VCvarB r_light_opt_separate_vis("r_light_opt_separate_vis", false, "Calculate light and render vis intersection as separate steps?", CVAR_Archive|CVAR_PreInit);

VCvarB r_allow_shadows("r_allow_shadows", true, "Allow stencil shadows, and shadows from dynamic lights?", CVAR_Archive);

static VCvarF r_hud_fullscreen_alpha("r_hud_fullscreen_alpha", "0.44", "Alpha for fullscreen HUD", CVAR_Archive);

extern VCvarB r_light_opt_shadow;

VDrawer *Drawer;

float PixelAspect;

bool MirrorFlip = false;
bool MirrorClip = false;


static FDrawerDesc *DrawerList[DRAWER_MAX];

VCvarI screen_size("screen_size", "11", "Screen size.", CVAR_Archive);
bool set_resolutioon_needed = true;

// angles in the SCREENWIDTH wide window
VCvarF fov("fov", "90", "Field of vision.");

// translation tables
VTextureTranslation *PlayerTranslations[MAXPLAYERS+1];
static TArray<VTextureTranslation *> CachedTranslations;

static VCvarB r_precache_textures("r_precache_textures", true, "Precache level textures?", CVAR_Archive);
static VCvarI r_level_renderer("r_level_renderer", "1", "Level renderer type (0:auto; 1:lightmap; 2:stenciled).", CVAR_Archive);

int r_precache_textures_override = -1;


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for portal data
// ////////////////////////////////////////////////////////////////////////// //
VRenderLevelShared::PPNode *VRenderLevelShared::pphead = nullptr;
VRenderLevelShared::PPNode *VRenderLevelShared::ppcurr = nullptr;
int VRenderLevelShared::ppMinNodeSize = 0;


//==========================================================================
//
//  CalcAspect
//
//==========================================================================
static float CalcAspect (int aspectRatio, int scrwdt, int scrhgt) {
  switch (aspectRatio) {
    default:
    case 0: // original aspect ratio
      //return ((float)scrhgt*320.0f)/((float)scrwdt*200.0f);
      return 1.2f; // original vanilla pixels are 20% taller than wide
    case 1: // 4:3 aspect ratio
      return ((float)scrhgt*4.0f)/((float)scrwdt*3.0f);
    case 2: // 16:9 aspect ratio
      return ((float)scrhgt*16.0f)/((float)scrwdt*9.0f);
    case 3: // 16:9 aspect ratio
      return ((float)scrhgt*16.0f)/((float)scrwdt*10.0f);
  }
}


//==========================================================================
//
//  GetAspectRatio
//
//==========================================================================
float R_GetAspectRatio () {
  return CalcAspect(aspect_ratio, ScreenWidth, ScreenHeight);
}


//==========================================================================
//
//  VRenderLevelShared::SetMinPoolNodeSize
//
//==========================================================================
void VRenderLevelShared::SetMinPoolNodeSize (int minsz) {
  if (minsz < 0) minsz = 0;
  minsz = (minsz|0xfff)+1;
  if (ppMinNodeSize < minsz) {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: new min node size is %d\n", minsz);
#endif
    ppMinNodeSize = minsz;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreatePortalPool
//
//==========================================================================
void VRenderLevelShared::CreatePortalPool () {
  KillPortalPool();
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  fprintf(stderr, "PORTALPOOL: new\n");
#endif
}


//==========================================================================
//
//  VRenderLevelShared::KillPortalPool
//
//==========================================================================
void VRenderLevelShared::KillPortalPool () {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  int count = 0, total = 0;
#endif
  while (pphead) {
    PPNode *n = pphead;
    pphead = n->next;
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    ++count;
    total += n->size;
#endif
    Z_Free(n->mem);
    Z_Free(n);
  }
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  if (count) fprintf(stderr, "PORTALPOOL: freed %d nodes (%d total bytes)\n", count, total);
#endif
  pphead = ppcurr = nullptr;
  ppMinNodeSize = 0;
}


//==========================================================================
//
//  VRenderLevelShared::ResetPortalPool
//
//  called on frame start
//
//==========================================================================
void VRenderLevelShared::ResetPortalPool () {
  ppcurr = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::MarkPortalPool
//
//==========================================================================
void VRenderLevelShared::MarkPortalPool (PPMark *mark) {
  if (!mark) return;
  mark->curr = ppcurr;
  mark->currused = (ppcurr ? ppcurr->used : 0);
}


//==========================================================================
//
//  VRenderLevelShared::RestorePortalPool
//
//==========================================================================
void VRenderLevelShared::RestorePortalPool (PPMark *mark) {
  if (!mark || mark->currused == -666) return;
  ppcurr = mark->curr;
  if (ppcurr) ppcurr->used = mark->currused;
  mark->currused = -666; // just in case
}


//==========================================================================
//
//  VRenderLevelShared::AllocPortalPool
//
//==========================================================================
vuint8 *VRenderLevelShared::AllocPortalPool (int size) {
  if (size < 1) size = 1;
  int reqsz = size;
  if (reqsz%16 != 0) reqsz = (reqsz|0x0f)+1;
  int allocsz = (reqsz|0xfff)+1;
       if (allocsz < ppMinNodeSize) allocsz = ppMinNodeSize;
  else if (allocsz > ppMinNodeSize) ppMinNodeSize = allocsz;
  // was anything allocated?
  if (ppcurr == nullptr && pphead && pphead->size < allocsz) {
    // cannot use first node, free pool (it will be recreated later)
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: freeing allocated nodes (old size is %d, minsize is %d)\n", pphead->size, allocsz);
#endif
    while (pphead) {
      PPNode *n = pphead;
      pphead = n->next;
      Z_Free(n->mem);
      Z_Free(n);
    }
  }
  // no nodes at all?
  if (pphead == nullptr) {
    if (ppcurr != nullptr) Sys_Error("PortalPool: ppcur is not empty");
    // allocate first node
    pphead = (PPNode *)Z_Malloc(sizeof(PPNode));
    pphead->mem = (vuint8 *)Z_Malloc(allocsz);
    pphead->size = allocsz;
    pphead->used = 0;
    pphead->next = nullptr;
  }
  // was anything allocated?
  if (ppcurr == nullptr) {
    // nope, we should have a good first node here
    if (!pphead) Sys_Error("PortalPool: pphead is empty");
    if (pphead->size < reqsz) Sys_Error("PortalPool: pphead is too small");
    ppcurr = pphead;
    ppcurr->used = 0;
  }
  // check for easy case
  if (ppcurr->used+reqsz <= ppcurr->size) {
    vuint8 *res = ppcurr->mem+ppcurr->used;
    ppcurr->used += reqsz;
    return res;
  }
  // check if we have enough room in the next pool node
  if (ppcurr->next && reqsz <= ppcurr->next->size) {
    // yes; move to the next node, and use it
    ppcurr = ppcurr->next;
    ppcurr->used = reqsz;
    return ppcurr->mem;
  }
  // next node is absent or too small: kill "rest" nodes
  if (ppcurr->next) {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: freeing \"rest\" nodes (old size is %d, minsize is %d)\n", ppcurr->size, allocsz);
#endif
    while (ppcurr->next) {
      PPNode *n = ppcurr;
      ppcurr->next = n->next;
      Z_Free(n->mem);
      Z_Free(n);
    }
  }
  // allocate a new node
  PPNode *nnn = (PPNode *)Z_Malloc(sizeof(PPNode));
  nnn->mem = (vuint8 *)Z_Malloc(allocsz);
  nnn->size = allocsz;
  nnn->used = reqsz;
  nnn->next = nullptr;
  ppcurr->next = nnn;
  ppcurr = nnn;
  return ppcurr->mem;
}


//==========================================================================
//
//  FDrawerDesc::FDrawerDesc
//
//==========================================================================
FDrawerDesc::FDrawerDesc (int Type, const char *AName, const char *ADescription, const char *ACmdLineArg, VDrawer *(*ACreator) ())
  : Name(AName)
  , Description(ADescription)
  , CmdLineArg(ACmdLineArg)
  , Creator(ACreator)
{
  DrawerList[Type] = this;
}


//==========================================================================
//
//  R_Init
//
//==========================================================================
void R_Init () {
  R_InitSkyBoxes();
  R_InitModels();
  // create light remapping table
  for (int i = 0; i < 256; ++i) {
    int n = i*i/255;
    /*
         if (n == 0) n = 4;
    //else if (n < 64) n += n/2;
    else if (n < 128) n += n/3;
    */
    if (n < 8) n = 8;
    if (n > 255) n = 255; else if (n < 0) n = 0;
    light_remap[i] = clampToByte(n);
  }
}


//==========================================================================
//
//  R_Start
//
//==========================================================================
void R_Start (VLevel *ALevel) {
  SCR_Update(false); // partial update
  if (r_level_renderer > 1 && !Drawer->SupportsAdvancedRendering()) {
    GCon->Logf(NAME_Warning, "Your GPU doesn't support Advanced Renderer, so I will switch to regular one.");
    r_level_renderer = 1;
  } else if (r_level_renderer <= 0) {
    if (Drawer->SupportsAdvancedRendering()) {
      GCon->Logf(NAME_Warning, "Your GPU supports Advanced Renderer, but it is slow and unfinished, so i won't use it.");
    }
    //r_level_renderer = 1;
  }
  // now create renderer
  if (r_level_renderer <= 1) {
    ALevel->RenderData = new VRenderLevel(ALevel);
  } else {
    ALevel->RenderData = new VAdvancedRenderLevel(ALevel);
  }
}


//==========================================================================
//
//  VRenderLevelShared::VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::VRenderLevelShared (VLevel *ALevel)
  : VRenderLevelDrawer()
  , Level(ALevel)
  , ViewEnt(nullptr)
  , MirrorLevel(0)
  , PortalLevel(0)
  , VisSize(0)
  , BspVis(nullptr)
  , BspVisThing(nullptr)
  , r_viewleaf(nullptr)
  , r_oldviewleaf(nullptr)
  , old_fov(90.0f)
  , prev_aspect_ratio(0)
  , ExtraLight(0)
  , FixedLight(0)
  , Particles(0)
  , ActiveParticles(0)
  , FreeParticles(0)
  , CurrentSky1Texture(-1)
  , CurrentSky2Texture(-1)
  , CurrentDoubleSky(false)
  , CurrentLightning(false)
  , trans_sprites(nullptr)
  , traspUsed(0)
  , traspSize(0)
  , traspFirst(0)
  , free_wsurfs(nullptr)
  , AllocatedWSurfBlocks(nullptr)
  , AllocatedSubRegions(nullptr)
  , AllocatedDrawSegs(nullptr)
  , AllocatedSegParts(nullptr)
  , cacheframecount(0)
  , inWorldCreation(false)
  , updateWorldFrame(0)
  , bspVisRadius(nullptr)
  , bspVisRadiusFrame(0)
  , pspart(nullptr)
  , pspartsLeft(0)
{
  currDLightFrame = 0;
  currQueueFrame = 0;
  currVisFrame = 0;

  memset(light_block, 0, sizeof(light_block));
  memset(block_changed, 0, sizeof(block_changed));
  memset(light_chain, 0, sizeof(light_chain));
  memset(add_block, 0, sizeof(add_block));
  memset(add_changed, 0, sizeof(add_changed));
  memset(add_chain, 0, sizeof(add_chain));
  PortalDepth = 0;
  //VPortal::ResetFrame();

  VisSize = (Level->NumSubsectors+7)>>3;
  BspVis = new vuint8[VisSize];
  BspVisThing = new vuint8[VisSize];

  LightVis = new vuint8[VisSize];
  LightBspVis = new vuint8[VisSize];

  memset(BspVis, 0, VisSize);
  memset(BspVisThing, 0, VisSize);
  memset(LightVis, 0, VisSize);
  memset(LightBspVis, 0, VisSize);

  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  memset(DLights, 0, sizeof(DLights));

  CreatePortalPool();

  InitParticles();
  ClearParticles();

  screenblocks = 0;

  // preload graphics
  if (r_precache_textures_override != 0) {
    if (r_precache_textures || r_precache_textures_override > 0) PrecacheLevel();
  }

  ResetVisFrameCount();
  ResetDLightFrameCount();

  ColorMap = 0;
  skyheight = 0;
  memset((void *)(&dlinfo[0]), 0, sizeof(dlinfo));
  CurrLightRadius = 0;
  CurrLightInFrustum = false;
  CurrLightBit = 0;
  CurrLightsNumber = 0;
  CurrShadowsNumber = 0;
  AllLightsNumber = 0;
  AllShadowsNumber = 0;
  HasLightIntersection = false;
  LitSurfaces = 0;
  HasBackLit = false;
  doShadows = false;
  MirrorClipSegs = false;
}


//==========================================================================
//
//  VRenderLevelShared::~VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::~VRenderLevelShared () {
  delete[] bspVisRadius;
  bspVisRadius = nullptr;

  // free fake floor data
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].fakefloors) {
      delete Level->Sectors[i].fakefloors;
      Level->Sectors[i].fakefloors = nullptr;
    }
  }

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    for (subregion_t *r = Level->Subsectors[i].regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) {
        FreeSurfaces(r->realfloor->surfs);
        delete r->realfloor;
        r->realfloor = nullptr;
      }
      if (r->realceil != nullptr) {
        FreeSurfaces(r->realceil->surfs);
        delete r->realceil;
        r->realceil = nullptr;
      }
      if (r->fakefloor != nullptr) {
        FreeSurfaces(r->fakefloor->surfs);
        delete r->fakefloor;
        r->fakefloor = nullptr;
      }
      if (r->fakeceil != nullptr) {
        FreeSurfaces(r->fakeceil->surfs);
        delete r->fakeceil;
        r->fakeceil = nullptr;
      }
    }
  }

  // free seg parts
  for (int i = 0; i < Level->NumSegs; ++i) {
    for (drawseg_t *ds = Level->Segs[i].drawsegs; ds; ds = ds->next) {
      if (ds->top) FreeSegParts(ds->top);
      if (ds->mid) FreeSegParts(ds->mid);
      if (ds->bot) FreeSegParts(ds->bot);
      if (ds->topsky) FreeSegParts(ds->topsky);
      if (ds->extra) FreeSegParts(ds->extra);
      if (ds->HorizonTop) Z_Free(ds->HorizonTop);
      if (ds->HorizonBot) Z_Free(ds->HorizonBot);
    }
  }

  // free allocated wall surface blocks
  for (void *Block = AllocatedWSurfBlocks; Block; ) {
    void *Next = *(void **)Block;
    Z_Free(Block);
    Block = Next;
  }
  AllocatedWSurfBlocks = nullptr;

  // free big blocks
  delete[] AllocatedSubRegions;
  AllocatedSubRegions = nullptr;
  delete[] AllocatedDrawSegs;
  AllocatedDrawSegs = nullptr;
  delete[] AllocatedSegParts;
  AllocatedSegParts = nullptr;

  delete[] Particles;
  Particles = nullptr;

  delete[] BspVis;
  BspVis = nullptr;
  delete[] BspVisThing;
  BspVisThing = nullptr;

  delete[] LightVis;
  LightVis = nullptr;
  delete[] LightBspVis;
  LightBspVis = nullptr;

  for (int i = 0; i < SideSkies.Num(); ++i) {
    delete SideSkies[i];
    SideSkies[i] = nullptr;
  }

  KillPortalPool();

  // free translucent sprite list
  if (trans_sprites) Z_Free(trans_sprites);
  trans_sprites = nullptr;
  traspUsed = 0;
  traspSize = 0;
  traspFirst = 0;
}


//==========================================================================
//
//  VRenderLevelShared::IsNodeRendered
//
//==========================================================================
bool VRenderLevelShared::IsNodeRendered (const node_t *node) const {
  if (!node) return false;
  return (node->VisFrame == currVisFrame);
}


//==========================================================================
//
//  VRenderLevelShared::IsSubsectorRendered
//
//==========================================================================
bool VRenderLevelShared::IsSubsectorRendered (const subsector_t *sub) const {
  if (!sub) return false;
  return (sub->VisFrame == currVisFrame);
}


//==========================================================================
//
//  VRenderLevelShared::ResetVisFrameCount
//
//==========================================================================
void VRenderLevelShared::ResetVisFrameCount () {
  currVisFrame = 1;
  for (unsigned nidx = 0; nidx < (unsigned)Level->NumNodes; ++nidx) {
    Level->Nodes[nidx].VisFrame = 0;
  }
  for (unsigned nidx = 0; nidx < (unsigned)Level->NumSubsectors; ++nidx) {
    Level->Subsectors[nidx].VisFrame = 0;
  }
}


//==========================================================================
//
//  VRenderLevelShared::ResetDLightFrameCount
//
//==========================================================================
void VRenderLevelShared::ResetDLightFrameCount () {
  currDLightFrame = 1;
  for (unsigned idx = 0; idx < (unsigned)Level->NumSubsectors; ++idx) {
    Level->Subsectors[idx].dlightframe = 0;
    Level->Subsectors[idx].dlightbits = 0;
  }
}


//==========================================================================
//
//  VRenderLevelShared::ResetUpdateWorldFrame
//
//==========================================================================
void VRenderLevelShared::ResetUpdateWorldFrame () {
  updateWorldFrame = 1;
  for (unsigned f = 0; f < (unsigned)Level->NumSubsectors; ++f) Level->Subsectors[f].updateWorldFrame = 0;
}


//==========================================================================
//
//  VRenderLevelShared::ClearQueues
//
//==========================================================================
void VRenderLevelShared::ClearQueues () {
  DrawSurfList.reset();
  DrawSkyList.reset();
  DrawHorizonList.reset();
  IncQueueFrameCount();
  // this is prolly not required for advanced renderer
  if (!IsAdvancedRenderer()) {
    memset(light_chain, 0, sizeof(light_chain));
    memset(add_chain, 0, sizeof(add_chain));
  }
}


//==========================================================================
//
//  VRenderLevelShared::GetStaticLightCount
//
//==========================================================================
int VRenderLevelShared::GetStaticLightCount () const {
  return Lights.length();
}


//==========================================================================
//
//  VRenderLevelShared::GetStaticLight
//
//==========================================================================
VRenderLevelPublic::LightInfo VRenderLevelShared::GetStaticLight (int idx) const {
  LightInfo res;
  res.origin = Lights[idx].origin;
  res.radius = Lights[idx].radius;
  res.color = Lights[idx].color;
  res.active = Lights[idx].active;
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::GetDynamicLightCount
//
//==========================================================================
int VRenderLevelShared::GetDynamicLightCount () const {
  return MAX_DLIGHTS;
}


//==========================================================================
//
//  VRenderLevelShared::GetDynamicLight
//
//==========================================================================
VRenderLevelPublic::LightInfo VRenderLevelShared::GetDynamicLight (int idx) const {
  LightInfo res;
  res.origin = DLights[idx].origin;
  res.radius = DLights[idx].radius;
  res.color = DLights[idx].color;
  res.active = (res.radius > 0);
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::NewBSPVisibilityFrame
//
//==========================================================================
void VRenderLevelShared::NewBSPVisibilityFrame () {
  if (bspVisRadius) {
    if (++bspVisRadiusFrame == 0) {
      bspVisRadiusFrame = 1;
      memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
    }
  } else {
    bspVisRadiusFrame = 0;
  }
}


//==========================================================================
//
//  isCircleTouchingLine
//
//==========================================================================
static inline bool isCircleTouchingLine (const TVec &corg, const float radiusSq, const TVec &v0, const TVec &v1) {
  const TVec s0qp = corg-v0;
  if (s0qp.length2DSquared() <= radiusSq) return true;
  if ((corg-v1).length2DSquared() <= radiusSq) return true;
  const TVec s0s1 = v1-v0;
  const float a = s0s1.dot2D(s0s1);
  if (!a) return false; // if you haven't zero-length segments omit this, as it would save you 1 _mm_comineq_ss() instruction and 1 memory fetch
  const float b = s0s1.dot2D(s0qp);
  const float t = b/a; // length of projection of s0qp onto s0s1
  if (t >= 0.0f && t <= 1.0f) {
    const float c = s0qp.dot2D(s0qp);
    const float r2 = c-a*t*t;
    //print("a=%s; t=%s; r2=%s; rsq=%s", a, t, r2, radiusSq);
    return (r2 < radiusSq); // true if collides
  }
  return false;
}


//==========================================================================
//
//  PlaneAngles2D
//
//==========================================================================
static float PlaneAngles2D (const TPlane *from, const TPlane *to) {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  PlaneAngles2DFlipTo
//
//==========================================================================
static float PlaneAngles2DFlipTo (const TPlane *from, const TPlane *to) {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(-to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilitySub
//
//  `firsttravel` is used to reject invisible segs
//  it is set before first recursive call, and all segs whose planes are
//  angled with 190 or more relative to this first seg are rejected
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilitySub (const TVec &org, const float radius, const subsector_t *currsub, const seg_t *firsttravel) {
  const unsigned csubidx = (unsigned)(ptrdiff_t)(currsub-Level->Subsectors);
  // rendered means "visible"
  if (BspVis[csubidx>>3]&(1<<(csubidx&7))) return true;
  // if we came into already visited subsector, abort flooding (and return failure)
  if (bspVisRadius[csubidx].framecount == bspVisRadiusFrame) return false;
  // recurse into neighbour subsectors
  bspVisRadius[csubidx].framecount = bspVisRadiusFrame; // mark as visited
  if (currsub->numlines == 0) return false;
  const float radiusSq = radius*radius;
  const seg_t *seg = &Level->Segs[currsub->firstline];
  for (int count = currsub->numlines; count--; ++seg) {
    // skip non-portals
    const line_t *ldef = seg->linedef;
    if (ldef) {
      // not a miniseg; check if linedef is passable
      if (!(ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX))) continue; // solid line
      // check if we can touch midtex, 'cause why not?
      const sector_t *bsec = seg->backsector;
      if (!bsec) continue;
      if (org.z+radius <= bsec->floor.minz ||
          org.z-radius >= bsec->ceiling.maxz)
      {
        // cannot possibly leak through midtex, consider this wall solid
        continue;
      }
      // don't go through closed doors and raised lifts
      if (VViewClipper::IsSegAClosedSomething(nullptr/*no frustum*/, seg, &org, &radius)) continue;
    } // minisegs are portals
    // we should have partner seg
    if (!seg->partner || seg->partner == seg || seg->partner->front_sub == currsub) continue;
    // check if this seg is touching our sphere
    {
      float distSq = DotProduct(org, seg->normal)-seg->dist;
      distSq *= distSq;
      if (distSq >= radiusSq) continue;
    }
    // precise check
    if (!isCircleTouchingLine(org, radiusSq, *seg->v1, *seg->v2)) continue;
    // check plane angles
    if (r_lightflood_check_plane_angles && firsttravel) {
      if (PlaneAngles2D(firsttravel, seg) >= 180.0f && PlaneAngles2DFlipTo(firsttravel, seg) >= 180.0f) continue;
    }
    // ok, it is touching, recurse
    if (CheckBSPVisibilitySub(org, radius, seg->partner->front_sub, (firsttravel ? firsttravel : seg))) {
      //GCon->Logf("RECURSE HIT!");
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibility
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibility (const TVec &org, float radius, const subsector_t *sub) {
  if (!Level) return false; // just in case
  if (!sub) {
    sub = Level->PointInSubsector(org);
    if (!sub) return false;
  }
  const unsigned subidx = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
  // check potential visibility
  /*
  if (hasPVS) {
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    const unsigned leafnum = Level->PointInSubsector(l->origin)-Level->Subsectors;
    if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
  }
  */
/*
  // already checked?
  if (bspVisRadius[subidx].framecount == bspVisRadiusFrame) {
    if (bspVisRadius[subidx].radius <= radius) return !!bspVisRadius[subidx].vis;
  }
  // mark as "checked"
  bspVisRadius[subidx].framecount = bspVisRadiusFrame;
  bspVisRadius[subidx].radius = radius;
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) {
    bspVisRadius[subidx].radius = 1e12; // big!
    bspVisRadius[subidx].vis = BSPVisInfo::VISIBLE;
    return true;
  }
*/
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) return true;

  // use floodfill to determine (rough) potential visibility
  NewBSPVisibilityFrame();
  if (!bspVisRadius) {
    bspVisRadiusFrame = 1;
    bspVisRadius = new BSPVisInfo[Level->NumSubsectors];
    memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
  }
  return CheckBSPVisibilitySub(org, radius, sub, nullptr);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateBBoxWithSurface
//
//==========================================================================
void VRenderLevelShared::UpdateBBoxWithSurface (TVec bbox[2], const surface_t *surfs, const texinfo_t *texinfo,
                                                VEntity *SkyBox, bool CheckSkyBoxAlways)
{
  if (!surfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && SkyBox && SkyBox->eventSkyBoxGetAlways()))
  {
    return;
  }

  for (const surface_t *surf = surfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->IsVisible(vieworg)) {
      // viewer is in back side or on plane
      if (!HasBackLit) {
        const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
        HasBackLit = (dist > 0.0f && dist < CurrLightRadius);
      }
      continue;
    }
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    ++LitSurfaces;
    const TVec *vert = surf->verts;
    for (int vcount = surf->count; vcount--; ++vert) {
      bbox[0].x = min2(bbox[0].x, vert->x);
      bbox[0].y = min2(bbox[0].y, vert->y);
      bbox[0].z = min2(bbox[0].z, vert->z);
      bbox[1].x = max2(bbox[1].x, vert->x);
      bbox[1].y = max2(bbox[1].y, vert->y);
      bbox[1].z = max2(bbox[1].z, vert->z);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateBBoxWithLine
//
//==========================================================================
void VRenderLevelShared::UpdateBBoxWithLine (TVec bbox[2], VEntity *SkyBox, const drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;
  if (!seg->backsector) {
    // single sided line
    if (dseg->mid) UpdateBBoxWithSurface(bbox, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, false);
    if (dseg->topsky) UpdateBBoxWithSurface(bbox, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, false);
  } else {
    // two sided line
    if (dseg->top) UpdateBBoxWithSurface(bbox, dseg->top->surfs, &dseg->top->texinfo, SkyBox, false);
    if (dseg->topsky) UpdateBBoxWithSurface(bbox, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, false);
    if (dseg->bot) UpdateBBoxWithSurface(bbox, dseg->bot->surfs, &dseg->bot->texinfo, SkyBox, false);
    if (dseg->mid) UpdateBBoxWithSurface(bbox, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      UpdateBBoxWithSurface(bbox, sp->surfs, &sp->texinfo, SkyBox, false);
    }
  }
}


#define UPDATE_LIGHTVIS(ssindex)  do { \
  LightSubs.append((int)ssindex); \
  const vuint8 bvbit = (vuint8)(1u<<((unsigned)(ssindex)&7)); \
  const unsigned sid8 = (unsigned)(ssindex)>>3; \
  LightVis[sid8] |= bvbit; \
  if (BspVis[sid8]&bvbit) { \
    LightBspVis[sid8] |= bvbit; \
    HasLightIntersection = true; \
    LightVisSubs.append((int)ssindex); \
    if (r_advlight_opt_optimise_scissor) { \
      const subsector_t *vsub = &Level->Subsectors[ssindex]; \
      for (const subregion_t *region = vsub->regions; region; region = region->next) { \
        sec_region_t *curreg = region->secregion; \
        if (vsub->poly && r_draw_pobj) { \
          seg_t **polySeg = vsub->poly->segs; \
          for (int count = vsub->poly->numsegs; count--; ++polySeg) { \
            UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, (*polySeg)->drawsegs); \
          } \
        } \
        drawseg_t *ds = region->lines; \
        for (int count = vsub->numlines; count--; ++ds) UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, ds); \
        if (region->fakefloor) UpdateBBoxWithSurface(LitBBox, region->fakefloor->surfs, &region->fakefloor->texinfo, curreg->efloor.splane->SkyBox, true); \
        if (region->realfloor) UpdateBBoxWithSurface(LitBBox, region->realfloor->surfs, &region->realfloor->texinfo, curreg->efloor.splane->SkyBox, true); \
        if (region->fakeceil) UpdateBBoxWithSurface(LitBBox, region->fakeceil->surfs, &region->fakeceil->texinfo, curreg->eceiling.splane->SkyBox, true); \
        if (region->realceil) UpdateBBoxWithSurface(LitBBox, region->realceil->surfs, &region->realceil->texinfo, curreg->eceiling.splane->SkyBox, true); \
      } \
    } \
  } \
} while (0)


//==========================================================================
//
//  VRenderLevelShared::BuildLightVis
//
//==========================================================================
void VRenderLevelShared::BuildLightVis (int bspnum, const float *bbox) {
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    const unsigned subidx = 0;
    subsector_t *sub = &Level->Subsectors[subidx];
    if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(sub, false)) {
      LightClip.ClipLightAddSubsectorSegs(sub, false);
      return;
    }
    //LightVis[SubNum>>3] |= 1<<(SubNum&7);
    UPDATE_LIGHTVIS(subidx);
    if (CurrLightBit) {
      if (sub->dlightframe != currDLightFrame) {
        sub->dlightbits = CurrLightBit;
        sub->dlightframe = currDLightFrame;
      } else {
        sub->dlightbits |= CurrLightBit;
      }
    }
    LightClip.ClipLightAddSubsectorSegs(sub, false);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      return BuildLightVis(bsp->children[0], bsp->bbox[0]);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      return BuildLightVis(bsp->children[1], bsp->bbox[1]);
    } else {
      //unsigned side = (unsigned)bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f); //(unsigned)bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      BuildLightVis(bsp->children[side], bsp->bbox[side]);
      // possibly divide back space
      side ^= 1;
      return BuildLightVis(bsp->children[side], bsp->bbox[side]);
    }
  } else {
    const unsigned subidx = (unsigned)(bspnum&(~NF_SUBSECTOR));
    subsector_t *sub = &Level->Subsectors[subidx];
    if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(sub, false)) {
      LightClip.ClipLightAddSubsectorSegs(sub, false);
      return;
    }
    //LightVis[SubNum>>3] |= 1<<(SubNum&7);

#if 0
    bool hasGoodSurf = false;
    if (!sub->poly) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int count = sub->numlines; count--; ++seg) {
        if (seg->SphereTouches(CurrLightPos, CurrLightRadius)) {
          hasGoodSurf = true;
          break;
        }
      }
      if (!hasGoodSurf) {
        for (const subregion_t *region = sub->regions; region; region = region->next) {
          //const sec_region_t *curreg = region->secregion;
          //const drawseg_t *ds = region->lines;
          if (region->floorplane && region->floorplane->SphereTouches(CurrLightPos, CurrLightRadius)) {
            hasGoodSurf = true;
            break;
          }
          if (region->ceilplane && region->ceilplane->SphereTouches(CurrLightPos, CurrLightRadius)) {
            hasGoodSurf = true;
            break;
          }
        }
      }
      //if (!hasGoodSurf) GCon->Logf("skipped uninteresting subsector");
    } else {
      //FIXME!
      hasGoodSurf = true;
    }

    if (hasGoodSurf)
#endif
    {
      UPDATE_LIGHTVIS(subidx);
      if (CurrLightBit) {
        if (sub->dlightframe != currDLightFrame) {
          sub->dlightbits = CurrLightBit;
          sub->dlightframe = currDLightFrame;
        } else {
          sub->dlightbits |= CurrLightBit;
        }
      }
    }
    LightClip.ClipLightAddSubsectorSegs(sub, false);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalcLightVis
//
//  sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
//  returns `false` if the light is invisible
//
//  TODO: clip invisible geometry for spotlights
//
//==========================================================================
bool VRenderLevelShared::CalcLightVis (const TVec &org, const float radius, vuint32 currltbit) {
  if (radius < 2) return false;

  //bool skipShadowCheck = !r_light_opt_shadow;

  doShadows = (radius >= 8.0f);

  CurrLightPos = org;
  CurrLightRadius = radius;
  CurrLightBit = currltbit;

  LightSubs.reset(); // all affected subsectors
  LightVisSubs.reset(); // visible affected subsectors
  LitSurfaces = 0;
  HasBackLit = false;

  LitBBox[0] = TVec(FLT_MAX, FLT_MAX, FLT_MAX);
  LitBBox[1] = TVec(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  // build vis data for light
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  memset(LightVis, 0, VisSize);
  memset(LightBspVis, 0, VisSize);
  HasLightIntersection = false;
  BuildLightVis(Level->NumNodes-1, dummy_bbox);
  if (!HasLightIntersection) return false;

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::RadiusCastRay
//
//==========================================================================
bool VRenderLevelShared::RadiusCastRay (sector_t *sector, const TVec &org, const TVec &dest, float radius, bool advanced) {
#if 0
  // BSP tracing
  float dsq = length2DSquared(org-dest);
  if (dsq <= 1) return true;
  linetrace_t Trace;
  bool canHit = !!Level->TraceLine(Trace, org, dest, SPF_NOBLOCKSIGHT);
  if (canHit) return true;
  if (!advanced || radius < 12) return false;
  // check some more rays
  if (r_lights_cast_many_rays) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if ((dy|dx) == 0) continue;
        TVec np = org;
        np.x += radius*(0.73f*dx);
        np.y += radius*(0.73f*dy);
        canHit = !!Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT);
        if (canHit) return true;
      }
    }
  } else {
    // check only "head" and "feet"
    TVec np = org;
    np.y += radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
    np = org;
    np.y -= radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
  }
  return false;
#else
  // blockmap tracing
  return Level->CastEx(sector, org, dest, SPF_NOBLOCKSIGHT);
#endif
}


//==========================================================================
//
//  R_SetViewSize
//
//  Do not really change anything here, because it might be in the middle
//  of a refresh. The change will take effect next refresh.
//
//==========================================================================
void R_SetViewSize (int blocks) {
  if (blocks > 2) screen_size = blocks;
  set_resolutioon_needed = true;
}


//==========================================================================
//
//  COMMAND SizeDown
//
//==========================================================================
COMMAND(SizeDown) {
  R_SetViewSize(screenblocks-1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  COMMAND SizeUp
//
//==========================================================================
COMMAND(SizeUp) {
  R_SetViewSize(screenblocks+1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  VRenderLevelShared::ExecuteSetViewSize
//
//==========================================================================
void VRenderLevelShared::ExecuteSetViewSize () {
  set_resolutioon_needed = false;
       if (screen_size < 3) screen_size = 3;
  else if (screen_size > 11) screen_size = 11;
  screenblocks = screen_size;

       if (fov < 5.0f) fov = 5.0f;
  else if (fov > 175.0f) fov = 175.0f;
  old_fov = fov;

  if (screenblocks > 10) {
    // no status bar
    refdef.width = ScreenWidth;
    refdef.height = ScreenHeight;
    refdef.y = 0;
  } else if (GGameInfo->NetMode == NM_TitleMap) {
    // no status bar for titlemap
    refdef.width = screenblocks*ScreenWidth/10;
    refdef.height = (screenblocks*ScreenHeight/10);
    refdef.y = (ScreenHeight-refdef.height)>>1;
  } else {
    refdef.width = screenblocks*ScreenWidth/10;
    refdef.height = (screenblocks*(ScreenHeight-SB_RealHeight())/10);
    refdef.y = (ScreenHeight-SB_RealHeight()-refdef.height)>>1;
  }
  refdef.x = (ScreenWidth-refdef.width)>>1;

  PixelAspect = CalcAspect(aspect_ratio, ScreenWidth, ScreenHeight);
  prev_aspect_ratio = aspect_ratio;

  clip_base.setupViewport(refdef.width, refdef.height, fov, PixelAspect);
  refdef.fovx = clip_base.fovx;
  refdef.fovy = clip_base.fovy;
  refdef.drawworld = true;
}


//==========================================================================
//
//  R_DrawViewBorder
//
//==========================================================================
void R_DrawViewBorder () {
  if (GGameInfo->NetMode == NM_TitleMap) {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-screenblocks*480/10)/2, screenblocks*64, screenblocks*VirtualHeight/10);
  } else {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-sb_height-screenblocks*(VirtualHeight-sb_height)/10)/2, screenblocks*64, screenblocks*(VirtualHeight-sb_height)/10);
  }
}


//==========================================================================
//
//  VRenderLevelShared::TransformFrustum
//
//==========================================================================
void VRenderLevelShared::TransformFrustum () {
  //view_frustum.setup(clip_base, vieworg, viewangles, false/*no back plane*/, -1.0f/*no forward plane*/);
  view_frustum.setup(clip_base, TFrustumParam(vieworg, viewangles, viewforward, viewright, viewup), true/*create back plane*/, -1.0f/*no forward plane*/);
}


//==========================================================================
//
//  VRenderLevelShared::SetupFrame
//
//==========================================================================
void VRenderLevelShared::SetupFrame () {
  // change the view size if needed
  if (screen_size != screenblocks || !screenblocks ||
      set_resolutioon_needed || old_fov != fov ||
      aspect_ratio != prev_aspect_ratio)
  {
    ExecuteSetViewSize();
  }

  ViewEnt = cl->Camera;
  viewangles = cl->ViewAngles;
  if (r_chasecam && r_chase_front) {
    // this is used to see how weapon looks in player's hands
    viewangles.yaw = AngleMod(viewangles.yaw+180);
    viewangles.pitch = -viewangles.pitch;
  }
  AngleVectors(viewangles, viewforward, viewright, viewup);

  view_frustum.clear(); // why not?

  if (r_chasecam && cl->MO == cl->Camera) {
    vieworg = cl->MO->Origin+TVec(0.0f, 0.0f, 32.0f)-r_chase_dist*viewforward+r_chase_up*viewup+r_chase_right*viewright;
  } else {
    vieworg = cl->ViewOrg;
  }

  ExtraLight = (ViewEnt && ViewEnt->Player ? ViewEnt->Player->ExtraLight*8 : 0);
  if (cl->Camera == cl->MO) {
    ColorMap = CM_Default;
         if (cl->FixedColormap == INVERSECOLORMAP) { ColorMap = CM_Inverse; FixedLight = 255; }
    else if (cl->FixedColormap == GOLDCOLORMAP) { ColorMap = CM_Gold; FixedLight = 255; }
    else if (cl->FixedColormap == REDCOLORMAP) { ColorMap = CM_Red; FixedLight = 255; }
    else if (cl->FixedColormap == GREENCOLORMAP) { ColorMap = CM_Green; FixedLight = 255; }
    else if (cl->FixedColormap >= NUMCOLORMAPS) { FixedLight = 255; }
    else if (cl->FixedColormap) { FixedLight = 255-(cl->FixedColormap<<3); }
    else { FixedLight = 0; }
  } else {
    FixedLight = 0;
    ColorMap = 0;
  }
  // inverse colormap flash effect
  if (cl->ExtraLight == 255) {
    ExtraLight = 0;
    ColorMap = CM_Inverse;
    FixedLight = 255;
  }

  Drawer->SetupView(this, &refdef);
  ++cacheframecount;
  PortalDepth = 0;
}


//==========================================================================
//
//  VRenderLevelShared::SetupCameraFrame
//
//==========================================================================
void VRenderLevelShared::SetupCameraFrame (VEntity *Camera, VTexture *Tex, int FOV, refdef_t *rd) {
  rd->width = Tex->GetWidth();
  rd->height = Tex->GetHeight();
  rd->y = 0;
  rd->x = 0;

  PixelAspect = CalcAspect(aspect_ratio, rd->width, rd->height);

  clip_base.setupViewport(rd->width, rd->height, FOV, PixelAspect);
  rd->fovx = clip_base.fovx;
  rd->fovy = clip_base.fovy;
  rd->drawworld = true;

  ViewEnt = Camera;
  viewangles = Camera->Angles;
  AngleVectors(viewangles, viewforward, viewright, viewup);

  vieworg = Camera->Origin;

  ExtraLight = 0;
  FixedLight = 0;
  ColorMap = 0;

  Drawer->SetupView(this, rd);
  cacheframecount++;
  PortalDepth = 0;
  set_resolutioon_needed = true;
}


//==========================================================================
//
//  VRenderLevelShared::MarkLeaves
//
//==========================================================================
void VRenderLevelShared::MarkLeaves () {
  // we need this for debug automap view
  if (!Level->HasPVS()) {
    (void)IncVisFrameCount();
    r_oldviewleaf = r_viewleaf;
    return;
  }

  // no need to do anything if we are still in the same subsector
  if (r_oldviewleaf == r_viewleaf) return;

  r_oldviewleaf = r_viewleaf;
  if (!Level->HasPVS()) return;

  const vuint32 currvisframe = IncVisFrameCount();

  const vuint8 *vis = Level->LeafPVS(r_viewleaf);
  subsector_t *sub = &Level->Subsectors[0];

#if 0
  {
    const unsigned ssleft = (unsigned)Level->NumSubsectors;
    for (unsigned i = 0; i < ssleft; ++i, ++sub) {
      if (vis[i>>3]&(1<<(i&7))) {
        sub->VisFrame = currvisframe;
        node_t *node = sub->parent;
        while (node) {
          if (node->VisFrame == currvisframe) break;
          node->VisFrame = currvisframe;
          node = node->parent;
        }
      }
    }
  }
#else
  {
    unsigned ssleft = (unsigned)Level->NumSubsectors;
    if (!ssleft) return; // just in case
    // process by 8 subsectors
    while (ssleft >= 8) {
      ssleft -= 8;
      vuint8 cvb = *vis++;
      if (!cvb) {
        // everything is invisible, skip 8 subsectors
        sub += 8;
      } else {
        // something is visible
        for (unsigned bc = 8; bc--; cvb >>= 1, ++sub) {
          if (cvb&1) {
            sub->VisFrame = currvisframe;
            node_t *node = sub->parent;
            while (node && node->VisFrame != currvisframe) {
              node->VisFrame = currvisframe;
              node = node->parent;
            }
          }
        }
      }
    }
    // process last byte
    if (ssleft) {
      vuint8 cvb = *vis;
      if (cvb) {
        while (ssleft--) {
          if (cvb&1) {
            sub->VisFrame = currvisframe;
            node_t *node = sub->parent;
            while (node && node->VisFrame != currvisframe) {
              node->VisFrame = currvisframe;
              node = node->parent;
            }
          }
          if ((cvb >>= 1) == 0) break;
          ++sub;
        }
      }
    }
    /*
    for (unsigned i = 0; i < ssleft; ++i, ++sub) {
      if (vis[i>>3]&(1<<(i&7))) {
        sub->VisFrame = currvisframe;
        node_t *node = sub->parent;
        while (node) {
          if (node->VisFrame == currvisframe) break;
          node->VisFrame = currvisframe;
          node = node->parent;
        }
      }
    }
    */
  }
  /*
  else {
    // eh, we have no PVS, so just mark it all
    // we won't check for visframe ever if level has no PVS, so do nothing here
    subsector_t *sub = &Level->Subsectors[0];
    for (int i = Level->NumSubsectors-1; i >= 0; --i, ++sub) {
      sub->VisFrame = currvisframe;
      node_t *node = sub->parent;
      while (node) {
        if (node->VisFrame == currvisframe) break;
        node->VisFrame = currvisframe;
        node = node->parent;
      }
    }
  }
  */
#endif
}


//==========================================================================
//
//  R_RenderPlayerView
//
//==========================================================================
void R_RenderPlayerView () {
  GClLevel->RenderData->RenderPlayerView();
}


//==========================================================================
//
//  VRenderLevelShared::RenderPlayerView
//
//==========================================================================
void VRenderLevelShared::RenderPlayerView () {
  if (!Level->LevelInfo) return;

  int renderattempts = 2;
  bool didIt = false;

  IncUpdateWorldFrame();

  if (dbg_autoclear_automap) {
    line_t *line = &Level->Lines[0];
    for (int i = Level->NumLines; i--; ++line) {
      line->flags &= ~ML_MAPPED;
      line->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
    }
    seg_t *seg = &Level->Segs[0];
    for (int i = GClLevel->NumSegs; i--; ++seg) {
      seg->flags &= ~SF_MAPPED;
    }
  }

again:
  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  GTextureManager.Time = Level->Time;

  BuildPlayerTranslations();

  if (!didIt) {
    didIt = true;
    AnimateSky(host_frametime);
    UpdateParticles(host_frametime);
  }

  //TODO: we can separate BspVis building (and batching surfaces for rendering), and
  //      the actual rendering. this way we'll be able to do better dynlight checks

  PushDlights();

  // update camera textures that were visible in last frame
  for (int i = 0; i < Level->CameraTextures.Num(); ++i) {
    UpdateCameraTexture(Level->CameraTextures[i].Camera, Level->CameraTextures[i].TexNum, Level->CameraTextures[i].FOV);
  }

  SetupFrame();

  if (dbg_clip_dump_added_ranges) GCon->Logf("=== RENDER SCENE: (%f,%f,%f); (yaw=%f; pitch=%f)", vieworg.x, vieworg.y, vieworg.x, viewangles.yaw, viewangles.pitch);

  double stt = -Sys_Time();
  RenderScene(&refdef, nullptr);
  stt += Sys_Time();
  if (times_render_highlevel) GCon->Logf("render scene time: %f", stt);

  if (dbg_clip_dump_added_ranges) ViewClip.Dump();

  if (light_reset_surface_cache != 0) {
    light_reset_surface_cache = 0;
    if (--renderattempts <= 0) Host_Error("*** Surface cache overflow, cannot repair");
    GCon->Logf("*** Surface cache overflow, starting it all again, %d attempt%s left", renderattempts, (renderattempts != 1 ? "s" : ""));
    GentlyFlushAllCaches();
    goto again;
  }

  // draw the psprites on top of everything
  if (/*fov <= 90.0f &&*/ cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap) DrawPlayerSprites();

  Drawer->EndView();

  // draw crosshair
  if (cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap) DrawCrosshair();
}


//==========================================================================
//
//  VRenderLevelShared::UpdateCameraTexture
//
//==========================================================================
void VRenderLevelShared::UpdateCameraTexture (VEntity *Camera, int TexNum, int FOV) {
  if (!Camera) return;

  if (!GTextureManager[TexNum]->bIsCameraTexture) return;

  VCameraTexture *Tex = (VCameraTexture *)GTextureManager[TexNum];
  if (!Tex->bNeedsUpdate) return;

  refdef_t CameraRefDef;
  CameraRefDef.DrawCamera = true;

  SetupCameraFrame(Camera, Tex, FOV, &CameraRefDef);

  RenderScene(&CameraRefDef, nullptr);

  Drawer->EndView();

  Tex->CopyImage();
}


//==========================================================================
//
//  VRenderLevelShared::GetFade
//
//==========================================================================
vuint32 VRenderLevelShared::GetFade (sec_region_t *Reg) {
  if (r_fog_test) return 0xff000000|(int(255*r_fog_r)<<16)|(int(255*r_fog_g)<<8)|int(255*r_fog_b);
  if (Reg->params->Fade) return Reg->params->Fade;
  if (Level->LevelInfo->OutsideFog && Reg->eceiling.splane->pic == skyflatnum) return Level->LevelInfo->OutsideFog;
  if (Level->LevelInfo->Fade) return Level->LevelInfo->Fade;
  if (Level->LevelInfo->FadeTable == NAME_fogmap) return 0xff7f7f7fU;
  if (r_fade_light) return FADE_LIGHT; // simulate light fading using dark fog
  return 0;
}


//==========================================================================
//
//  R_DrawPic
//
//==========================================================================
void R_DrawPic (int x, int y, int handle, float Alpha) {
  if (handle < 0) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()), fScaleY*(y+Tex->GetScaledHeight()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloat
//
//==========================================================================
void R_DrawPicFloat (float x, float y, int handle, float Alpha) {
  if (handle < 0) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()), fScaleY*(y+Tex->GetScaledHeight()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicPart
//
//==========================================================================
void R_DrawPicPart (int x, int y, float pwdt, float phgt, int handle, float Alpha) {
  R_DrawPicFloatPart(x, y, pwdt, phgt, handle, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPart
//
//==========================================================================
void R_DrawPicFloatPart (float x, float y, float pwdt, float phgt, int handle, float Alpha) {
  if (handle < 0 || pwdt <= 0.0f || phgt <= 0.0f || !isFiniteF(pwdt) || !isFiniteF(phgt)) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()*pwdt), fScaleY*(y+Tex->GetScaledHeight()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*x, fScaleY*y,
    fScaleX*(x+Tex->GetScaledWidth()*pwdt),
    fScaleY*(y+Tex->GetScaledHeight()*phgt),
    0, 0, Tex->GetWidth()*pwdt, Tex->GetHeight()*phgt,
    Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicPartEx
//
//==========================================================================
void R_DrawPicPartEx (int x, int y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha) {
  R_DrawPicFloatPartEx(x, y, tx0, ty0, tx1, ty1, handle, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPartEx
//
//==========================================================================
void R_DrawPicFloatPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha) {
  float pwdt = (tx1-tx0);
  float phgt = (ty1-ty0);
  if (handle < 0 || pwdt <= 0.0f || phgt <= 0.0f) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()*pwdt), fScaleY*(y+Tex->GetScaledHeight()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*(x+Tex->GetScaledWidth()*tx0),
    fScaleY*(y+Tex->GetScaledHeight()*ty0),
    fScaleX*(x+Tex->GetScaledWidth()*tx1),
    fScaleY*(y+Tex->GetScaledHeight()*ty1),
    Tex->GetWidth()*tx0, Tex->GetHeight()*ty0, Tex->GetWidth()*tx1, Tex->GetHeight()*ty1,
    Tex, nullptr, Alpha);
}


//==========================================================================
//
//  VRenderLevelShared::PrecacheLevel
//
//  Preloads all relevant graphics for the level.
//
//==========================================================================
void VRenderLevelShared::PrecacheLevel () {
  if (cls.demoplayback) return;

  TArray<bool> texturepresent;
  int maxtex = GTextureManager.GetNumTextures();
  if (maxtex < 2) return;

  texturepresent.setLength(maxtex);
  for (int f = maxtex-1; f >= 0; --f) texturepresent[f] = false;

  for (int f = 0; f < Level->NumSectors; ++f) {
    if (Level->Sectors[f].floor.pic > 0 && Level->Sectors[f].floor.pic < maxtex) texturepresent[Level->Sectors[f].floor.pic] = true;
    if (Level->Sectors[f].ceiling.pic > 0 && Level->Sectors[f].ceiling.pic < maxtex) texturepresent[Level->Sectors[f].ceiling.pic] = true;
  }

  for (int f = 0; f < Level->NumSides; ++f) {
    if (Level->Sides[f].TopTexture > 0 && Level->Sides[f].TopTexture < maxtex) texturepresent[Level->Sides[f].TopTexture] = true;
    if (Level->Sides[f].MidTexture > 0 && Level->Sides[f].MidTexture < maxtex) texturepresent[Level->Sides[f].MidTexture] = true;
    if (Level->Sides[f].BottomTexture > 0 && Level->Sides[f].BottomTexture < maxtex) texturepresent[Level->Sides[f].BottomTexture] = true;
  }

  R_LdrMsgShowSecondary("PRECACHING TEXTURES...");
  R_PBarReset();

  // precache textures
  for (int f = 1; f < maxtex; ++f) {
    if (texturepresent[f]) {
      R_PBarUpdate("Textures", f, maxtex);
      Drawer->PrecacheTexture(GTextureManager[f]);
    }
  }
  R_PBarUpdate("Textures", maxtex, maxtex, true); // final update
}


//==========================================================================
//
//  VRenderLevelShared::GetTranslation
//
//==========================================================================
VTextureTranslation *VRenderLevelShared::GetTranslation (int TransNum) {
  return R_GetCachedTranslation(TransNum, Level);
}


//==========================================================================
//
//  VRenderLevelShared::BuildPlayerTranslations
//
//==========================================================================
void VRenderLevelShared::BuildPlayerTranslations () {
  for (TThinkerIterator<VPlayerReplicationInfo> It(Level); It; ++It) {
    if (It->PlayerNum < 0 || It->PlayerNum >= MAXPLAYERS) continue; // should not happen
    if (!It->TranslStart || !It->TranslEnd) continue;

    VTextureTranslation *Tr = PlayerTranslations[It->PlayerNum];
    if (Tr && Tr->TranslStart == It->TranslStart && Tr->TranslEnd == It->TranslEnd && Tr->Color == It->Color) continue;

    if (!Tr) {
      Tr = new VTextureTranslation;
      PlayerTranslations[It->PlayerNum] = Tr;
    }

    // don't waste time clearing if it's the same range
    if (Tr->TranslStart != It->TranslStart || Tr->TranslEnd != It->TranslEnd) Tr->Clear();

    Tr->BuildPlayerTrans(It->TranslStart, It->TranslEnd, It->Color);
  }
}


//==========================================================================
//
//  R_SetMenuPlayerTrans
//
//==========================================================================
int R_SetMenuPlayerTrans (int Start, int End, int Col) {
  if (!Start || !End) return 0;

  VTextureTranslation *Tr = PlayerTranslations[MAXPLAYERS];
  if (Tr && Tr->TranslStart == Start && Tr->TranslEnd == End && Tr->Color == Col) {
    return (TRANSL_Player<<TRANSL_TYPE_SHIFT)+MAXPLAYERS;
  }

  if (!Tr) {
    Tr = new VTextureTranslation();
    PlayerTranslations[MAXPLAYERS] = Tr;
  }

  // don't waste time clearing if it's the same range
  if (Tr->TranslStart != Start || Tr->TranslEnd == End) Tr->Clear();

  Tr->BuildPlayerTrans(Start, End, Col);
  return (TRANSL_Player<<TRANSL_TYPE_SHIFT)+MAXPLAYERS;
}


//==========================================================================
//
//  R_GetCachedTranslation
//
//==========================================================================
VTextureTranslation *R_GetCachedTranslation (int TransNum, VLevel *Level) {
  int Type = TransNum>>TRANSL_TYPE_SHIFT;
  int Index = TransNum&((1<<TRANSL_TYPE_SHIFT)-1);
  VTextureTranslation *Tr = nullptr;
  switch (Type) {
    case TRANSL_Standard:
      if (Index == 7) {
        Tr = &IceTranslation;
      } else {
        if (Index < 0 || Index >= NumTranslationTables) return nullptr;
        Tr = TranslationTables[Index];
      }
      break;
    case TRANSL_Player:
      if (Index < 0 || Index >= MAXPLAYERS+1) return nullptr;
      Tr = PlayerTranslations[Index];
      break;
    case TRANSL_Level:
      if (!Level || Index < 0 || Index >= Level->Translations.Num()) return nullptr;
      Tr = Level->Translations[Index];
      break;
    case TRANSL_BodyQueue:
      if (!Level || Index < 0 || Index >= Level->BodyQueueTrans.Num()) return nullptr;
      Tr = Level->BodyQueueTrans[Index];
      break;
    case TRANSL_Decorate:
      if (Index < 0 || Index >= DecorateTranslations.Num()) return nullptr;
      Tr = DecorateTranslations[Index];
      break;
    case TRANSL_Blood:
      if (Index < 0 || Index >= BloodTranslations.Num()) return nullptr;
      Tr = BloodTranslations[Index];
      break;
    default:
      return nullptr;
  }

  if (!Tr) return nullptr;

  for (int i = 0; i < CachedTranslations.Num(); ++i) {
    VTextureTranslation *Check = CachedTranslations[i];
    if (Check->Crc != Tr->Crc) continue;
    if (memcmp(Check->Palette, Tr->Palette, sizeof(Tr->Palette))) continue;
    return Check;
  }

  VTextureTranslation *Copy = new VTextureTranslation;
  *Copy = *Tr;
  CachedTranslations.Append(Copy);
  return Copy;
}


//==========================================================================
//
//  COMMAND TimeRefresh
//
//  For program optimization
//
//==========================================================================
COMMAND(TimeRefresh) {
  double start, stop, time, RenderTime, UpdateTime;
  float startangle;

  if (!cl) return;

  startangle = cl->ViewAngles.yaw;

  RenderTime = 0;
  UpdateTime = 0;
  start = Sys_Time();

  int renderAlloc = 0;
  int renderRealloc = 0;
  int renderFree = 0;

  int renderPeakAlloc = 0;
  int renderPeakRealloc = 0;
  int renderPeakFree = 0;

  for (int i = 0; i < 128; ++i) {
    cl->ViewAngles.yaw = (float)(i)*360.0f/128.0f;

    Drawer->StartUpdate();

#ifdef VAVOOM_CORE_COUNT_ALLOCS
    zone_malloc_call_count = 0;
    zone_realloc_call_count = 0;
    zone_free_call_count = 0;
#endif

    RenderTime -= Sys_Time();
    R_RenderPlayerView();
    RenderTime += Sys_Time();

#ifdef VAVOOM_CORE_COUNT_ALLOCS
    renderAlloc += zone_malloc_call_count;
    renderRealloc += zone_realloc_call_count;
    renderFree += zone_free_call_count;

    if (renderPeakAlloc < zone_malloc_call_count) renderPeakAlloc = zone_malloc_call_count;
    if (renderPeakRealloc < zone_realloc_call_count) renderPeakRealloc = zone_realloc_call_count;
    if (renderPeakFree < zone_free_call_count) renderPeakFree = zone_free_call_count;
#endif

    UpdateTime -= Sys_Time();
    Drawer->Update();
    UpdateTime += Sys_Time();
  }
  stop = Sys_Time();

  time = stop-start;
  GCon->Logf("%f seconds (%f fps)", time, 128/time);
  GCon->Logf("Render time %f, update time %f", RenderTime, UpdateTime);
  GCon->Logf("Render malloc calls: %d", renderAlloc);
  GCon->Logf("Render realloc calls: %d", renderRealloc);
  GCon->Logf("Render free calls: %d", renderFree);
  GCon->Logf("Render peak malloc calls: %d", renderPeakAlloc);
  GCon->Logf("Render peak realloc calls: %d", renderPeakRealloc);
  GCon->Logf("Render peak free calls: %d", renderPeakFree);

  cl->ViewAngles.yaw = startangle;
}


//==========================================================================
//
//  V_Init
//
//==========================================================================
void V_Init () {
  int DIdx = -1;
  for (int i = 0; i < DRAWER_MAX; ++i) {
    if (!DrawerList[i]) continue;
    // pick first available as default
    if (DIdx == -1) DIdx = i;
    // check for user driver selection
    if (DrawerList[i]->CmdLineArg && GArgs.CheckParm(DrawerList[i]->CmdLineArg)) DIdx = i;
  }
  if (DIdx == -1) Sys_Error("No drawers are available");
  GCon->Logf(NAME_Init, "Selected %s", DrawerList[DIdx]->Description);
  // create drawer
  Drawer = DrawerList[DIdx]->Creator();
  Drawer->Init();
}


//==========================================================================
//
//  V_Shutdown
//
//==========================================================================
void V_Shutdown () {
  if (Drawer) {
    Drawer->Shutdown();
    delete Drawer;
    Drawer = nullptr;
  }
  R_FreeModels();
  for (int i = 0; i < MAXPLAYERS+1; ++i) {
    if (PlayerTranslations[i]) {
      delete PlayerTranslations[i];
      PlayerTranslations[i] = nullptr;
    }
  }
  for (int i = 0; i < CachedTranslations.Num(); ++i) {
    delete CachedTranslations[i];
    CachedTranslations[i] = nullptr;
  }
  CachedTranslations.Clear();
  R_FreeSkyboxData();
}
