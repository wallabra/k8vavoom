//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**  Rendering main loop and setup functions, utility functions (BSP,
//**  geometry, trigonometry). See tables.c, too.
//**
//**************************************************************************
//#define RADVLIGHT_GRID_OPTIMIZER

#include "gamedefs.h"
#include "r_local.h"

//#define VAVOOM_DEBUG_PORTAL_POOL
//#define VAVOOM_RENDER_TIMES


void R_FreeSkyboxData ();


int screenblocks = 0;

TVec vieworg;
TVec viewforward;
TVec viewright;
TVec viewup;
TAVec viewangles;

TClipPlane view_clipplanes[5];

int r_visframecount;

VCvarI r_fog("r_fog", "0", "Fog mode (0:GL_LINEAR; 1:GL_LINEAR; 2:GL_EXP; 3:GL_EXP2; add 4 to get \"nicer\" fog).");
VCvarB r_fog_test("r_fog_test", false, "Is fog testing enabled?");
VCvarF r_fog_r("r_fog_r", "0.5", "Fog color: red component.");
VCvarF r_fog_g("r_fog_g", "0.5", "Fog color: green component.");
VCvarF r_fog_b("r_fog_b", "0.5", "Fog color: blue component.");
VCvarF r_fog_start("r_fog_start", "1.0", "Fog start distance.");
VCvarF r_fog_end("r_fog_end", "2048.0", "Fog end distance.");
VCvarF r_fog_density("r_fog_density", "0.5", "Fog density.");

VCvarI aspect_ratio("r_aspect_ratio", "1", "Aspect ratio correction mode ([0..3]: normal/4:3/16:9/16:10).", CVAR_Archive);
VCvarB r_interpolate_frames("r_interpolate_frames", true, "Use frame interpolation for smoother rendering?", CVAR_Archive);
VCvarB r_vsync("r_vsync", false, "VSync mode.", CVAR_Archive);
VCvarB r_fade_light("r_fade_light", "0", "Fade lights?", CVAR_Archive);
VCvarF r_fade_factor("r_fade_factor", "4.0", "Fade actor lights?", CVAR_Archive);
VCvarF r_sky_bright_factor("r_sky_bright_factor", "1.0", "Skybright actor factor.", CVAR_Archive);

static VCvarB r_lights_cast_many_rays("r_lights_cast_many_rays", false, "Cast more rays to better check light visibility (usually doesn't make visuals any better)?", CVAR_Archive);

extern VCvarF r_lights_radius;
extern VCvarF r_lights_radius_sight_check;
extern VCvarI r_hashlight_static_div;
extern VCvarI r_hashlight_dynamic_div;
extern VCvarB r_dynamic_clip_more;

VDrawer *Drawer;

refdef_t refdef;

float PixelAspect;

bool MirrorFlip = false;
bool MirrorClip = false;


static FDrawerDesc *DrawerList[DRAWER_MAX];

VCvarI screen_size("screen_size", "11", "Screen size.", CVAR_Archive);
bool set_resolutioon_needed = true;

// angles in the SCREENWIDTH wide window
VCvarF fov("fov", "90", "Field of vision.");

TVec clip_base[4];

//
//  Translation tables
//
VTextureTranslation *PlayerTranslations[MAXPLAYERS + 1];
static TArray<VTextureTranslation*> CachedTranslations;

// if true, load all graphics at start
VCvarB precache("precache", true, "Load all graphics at startup (instead of on-demand)?", CVAR_Archive);

static VCvarI r_level_renderer("r_level_renderer", "1", "Level renderer type (0:auto; 1:normal; 2:advanced).", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for portal data
// ////////////////////////////////////////////////////////////////////////// //
VRenderLevelShared::PPNode *VRenderLevelShared::pphead = nullptr;
VRenderLevelShared::PPNode *VRenderLevelShared::ppcurr = nullptr;
int VRenderLevelShared::ppMinNodeSize = 0;

/*
static PPMark {
  PPNode *curr;
  int currused;
};
*/
/*
  struct PPNode {
    vuint8 *mem;
    int size;
    int used;
    PPNode *next;
  };
*/


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

FDrawerDesc::FDrawerDesc(int Type, const char *AName, const char *ADescription,
  const char *ACmdLineArg, VDrawer *(*ACreator)())
: Name(AName)
, Description(ADescription)
, CmdLineArg(ACmdLineArg)
, Creator(ACreator)
{
  guard(FDrawerDesc::FDrawerDesc);
  DrawerList[Type] = this;
  unguard
}

//==========================================================================
//
//  R_Init
//
//==========================================================================

void R_Init()
{
  guard(R_Init);
  R_InitSkyBoxes();
  R_InitModels();

  for (int i = 0; i < 256; i++)
  {
    int n = i*i/255;
    /*
         if (n == 0) n = 4;
    //else if (n < 64) n += n/2;
    else if (n < 128) n += n/3;
    */
         if (n < 8) n = 8;
    if (n > 255) n = 255; else if (n < 0) n = 0;
    light_remap[i] = byte(n);
  }
  unguard;
}

//==========================================================================
//
//  R_Start
//
//==========================================================================

void R_Start(VLevel *ALevel)
{
  guard(R_Start);
  switch (r_level_renderer)
  {
  case 1:
    ALevel->RenderData = new VRenderLevel(ALevel);
    break;

  case 2:
    ALevel->RenderData = new VAdvancedRenderLevel(ALevel);
    break;

  default:
    if (Drawer->SupportsAdvancedRendering()) {
      //ALevel->RenderData = new VAdvancedRenderLevel(ALevel);
      GCon->Logf("Your GPU supports Advanced Renderer, but it is slow and unfinished, so i won't use it.");
    }
    r_level_renderer = 1; // so it will be saved on exit
    ALevel->RenderData = new VRenderLevel(ALevel);
    break;
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::VRenderLevelShared
//
//==========================================================================

VRenderLevelShared::VRenderLevelShared(VLevel *ALevel)
: VRenderLevelDrawer()
, Level(ALevel)
, ViewEnt(nullptr)
, MirrorLevel(0)
, PortalLevel(0)
, VisSize(0)
, BspVis(nullptr)
, r_viewleaf(nullptr)
, r_oldviewleaf(nullptr)
, old_fov(90.0)
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
, trans_sprites(MainTransSprites)
, free_wsurfs(nullptr)
, AllocatedWSurfBlocks(nullptr)
, AllocatedSubRegions(nullptr)
, AllocatedDrawSegs(nullptr)
, AllocatedSegParts(nullptr)
, cacheframecount(0)
{
  guard(VRenderLevelShared::VRenderLevelShared);
  memset(MainTransSprites, 0, sizeof(MainTransSprites));

  memset(light_block, 0, sizeof(light_block));
  memset(block_changed, 0, sizeof(block_changed));
  memset(light_chain, 0, sizeof(light_chain));
  memset(add_block, 0, sizeof(add_block));
  memset(add_changed, 0, sizeof(add_changed));
  memset(add_chain, 0, sizeof(add_chain));
  SimpleSurfsHead = nullptr;
  SimpleSurfsTail = nullptr;
  SkyPortalsHead = nullptr;
  SkyPortalsTail = nullptr;
  HorizonPortalsHead = nullptr;
  HorizonPortalsTail = nullptr;
  PortalDepth = 0;
  //VPortal::ResetFrame();

  VisSize = (Level->NumSubsectors + 7) >> 3;
  BspVis = new vuint8[VisSize];

  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  CreatePortalPool();

  InitParticles();
  ClearParticles();

  screenblocks = 0;

  // preload graphics
  if (precache)
  {
    PrecacheLevel();
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::RadiusCastRay
//
//==========================================================================
bool VRenderLevelShared::RadiusCastRay (const TVec &org, const TVec &dest, float radius, bool advanced) {
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
 return Level->CastCanSee(org, dest, (advanced ? radius : 0));
#endif
}


//==========================================================================
//
//  VRenderLevelShared::~VRenderLevelShared
//
//==========================================================================

VRenderLevelShared::~VRenderLevelShared()
{
  //guard(VRenderLevelShared::~VRenderLevelShared);
  //  Free fake floor data.
  for (int i = 0; i < Level->NumSectors; i++)
  {
    if (Level->Sectors[i].fakefloors)
    {
      delete Level->Sectors[i].fakefloors;
      Level->Sectors[i].fakefloors = nullptr;
    }
  }

  for (int i = 0; i < Level->NumSubsectors; i++)
  {
    for (subregion_t *r = Level->Subsectors[i].regions; r != nullptr; r = r->next)
    {
      if (r->floor != nullptr)
      {
        FreeSurfaces(r->floor->surfs);
        delete r->floor;
        r->floor = nullptr;
      }
      if (r->ceil != nullptr)
      {
        FreeSurfaces(r->ceil->surfs);
        delete r->ceil;
        r->ceil = nullptr;
      }
    }
  }

  //  Free seg parts.
  for (int i = 0; i < Level->NumSegs; i++)
  {
    for (drawseg_t *ds = Level->Segs[i].drawsegs; ds; ds = ds->next)
    {
      FreeSegParts(ds->top);
      FreeSegParts(ds->mid);
      FreeSegParts(ds->bot);
      FreeSegParts(ds->topsky);
      FreeSegParts(ds->extra);
      if (ds->HorizonTop)
      {
        Z_Free(ds->HorizonTop);
      }
      if (ds->HorizonBot)
      {
        Z_Free(ds->HorizonBot);
      }
    }
  }
  //  Free allocated wall surface blocks.
  for (void *Block = AllocatedWSurfBlocks; Block;)
  {
    void *Next = *(void**)Block;
    Z_Free(Block);
    Block = Next;
  }
  AllocatedWSurfBlocks = nullptr;

  //  Free big blocks.
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

  for (int i = 0; i < SideSkies.Num(); i++)
  {
    delete SideSkies[i];
    SideSkies[i] = nullptr;
  }

  KillPortalPool();
  //unguard;
}


//==========================================================================
//
//  R_SetViewSize
//
//  Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
//
//==========================================================================

void R_SetViewSize(int blocks)
{
  guard(R_SetViewSize);
  if (blocks > 2)
  {
    screen_size = blocks;
  }
  set_resolutioon_needed = true;
  unguard;
}

//==========================================================================
//
//  COMMAND SizeDown
//
//==========================================================================

COMMAND(SizeDown)
{
  R_SetViewSize(screenblocks - 1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"),
    TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}

//==========================================================================
//
//  COMMAND SizeUp
//
//==========================================================================

COMMAND(SizeUp)
{
  R_SetViewSize(screenblocks + 1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"),
    TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}

//==========================================================================
//
//  VRenderLevelShared::ExecuteSetViewSize
//
//==========================================================================

void VRenderLevelShared::ExecuteSetViewSize()
{
  guard(VRenderLevelShared::ExecuteSetViewSize);
  set_resolutioon_needed = false;
  if (screen_size < 3)
  {
    screen_size = 3;
  }
  if (screen_size > 11)
  {
    screen_size = 11;
  }
  screenblocks = screen_size;

  if (fov < 5.0)
  {
    fov = 5.0;
  }
  if (fov > 175.0)
  {
    fov = 175.0;
  }
  old_fov = fov;

  if (screenblocks > 10)
  {
    refdef.width = ScreenWidth;
    refdef.height = ScreenHeight;
    refdef.y = 0;
  }
  else if (GGameInfo->NetMode == NM_TitleMap)
  {
    //  No status bar for titlemap.
    refdef.width = screenblocks * ScreenWidth / 10;
    refdef.height = (screenblocks * ScreenHeight / 10);
    refdef.y = (ScreenHeight - refdef.height) >> 1;
  }
  else
  {
    refdef.width = screenblocks * ScreenWidth / 10;
    refdef.height = (screenblocks * (ScreenHeight - SB_REALHEIGHT) / 10);
    refdef.y = (ScreenHeight - SB_REALHEIGHT - refdef.height) >> 1;
  }
  refdef.x = (ScreenWidth - refdef.width) >> 1;

  if (aspect_ratio == 0)
  {
    // Original aspect ratio
    PixelAspect = ((float)ScreenHeight * 320.0) / ((float)ScreenWidth * 200.0);
  }
  else if (aspect_ratio == 1)
  {
    // 4:3 aspect ratio
    PixelAspect = ((float)ScreenHeight * 4.0) / ((float)ScreenWidth * 3.0);
  }
  else if (aspect_ratio == 2)
  {
    // 16:9 aspect ratio
    PixelAspect = ((float)ScreenHeight * 16.0) / ((float)ScreenWidth * 9.0);
  }
  else
  {
    // 16:10 aspect ratio
    PixelAspect = ((float)ScreenHeight * 16.0) / ((float)ScreenWidth * 10.0);
  }
  prev_aspect_ratio = aspect_ratio;

  refdef.fovx = tan(DEG2RAD(fov) / 2);
  refdef.fovy = refdef.fovx * refdef.height / refdef.width / PixelAspect;

  // left side clip
  clip_base[0] = Normalise(TVec(1, 1.0 / refdef.fovx, 0));

  // right side clip
  clip_base[1] = Normalise(TVec(1, -1.0 / refdef.fovx, 0));

  // top side clip
  clip_base[2] = Normalise(TVec(1, 0, -1.0 / refdef.fovy));

  // bottom side clip
  clip_base[3] = Normalise(TVec(1, 0, 1.0 / refdef.fovy));

  refdef.drawworld = true;
  unguard;
}

//==========================================================================
//
//  R_DrawViewBorder
//
//==========================================================================

void R_DrawViewBorder()
{
  guard(R_DrawViewBorder);
  if (GGameInfo->NetMode == NM_TitleMap)
  {
    GClGame->eventDrawViewBorder(320 - screenblocks * 32,
      (480 - screenblocks * 480 / 10) / 2,
      screenblocks * 64, screenblocks * 480 / 10);
  }
  else
  {
    GClGame->eventDrawViewBorder(320 - screenblocks * 32,
      (480 - sb_height - screenblocks * (480 - sb_height) / 10) / 2,
      screenblocks * 64, screenblocks * (480 - sb_height) / 10);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::TransformFrustum
//
//==========================================================================

void VRenderLevelShared::TransformFrustum()
{
  guard(VRenderLevelShared::TransformFrustum);
  for (int i = 0; i < 4; i++)
  {
    TVec &v = clip_base[i];
    TVec v2;

    v2.x = v.y * viewright.x + v.z * viewup.x + v.x * viewforward.x;
    v2.y = v.y * viewright.y + v.z * viewup.y + v.x * viewforward.y;
    v2.z = v.y * viewright.z + v.z * viewup.z + v.x * viewforward.z;

    view_clipplanes[i].Set(v2, DotProduct(vieworg, v2));

    view_clipplanes[i].next = i == 3 ? nullptr : &view_clipplanes[i + 1];
    view_clipplanes[i].clipflag = 1 << i;
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::SetupFrame
//
//==========================================================================

VCvarB      r_chasecam("r_chasecam", false, "Chasecam mode.", CVAR_Archive);
VCvarF      r_chase_dist("r_chase_dist", "32.0", "Chasecam distance.", CVAR_Archive);
VCvarF      r_chase_up("r_chase_up", "128.0", "Chasecam position: up.", CVAR_Archive);
VCvarF      r_chase_right("r_chase_right", "0", "Chasecam position: right.", CVAR_Archive);
VCvarI      r_chase_front("r_chase_front", "0", "Chasecam position: front.", CVAR_Archive);

void VRenderLevelShared::SetupFrame()
{
  guard(VRenderLevelShared::SetupFrame);
  // change the view size if needed
  if (screen_size != screenblocks || !screenblocks ||
    set_resolutioon_needed || old_fov != fov ||
    aspect_ratio != prev_aspect_ratio)
  {
    ExecuteSetViewSize();
  }

  ViewEnt = cl->Camera;
  viewangles = cl->ViewAngles;
  if (r_chasecam && r_chase_front)
  {
    //  This is used to see how weapon looks in player's hands
    viewangles.yaw = AngleMod(viewangles.yaw + 180);
    viewangles.pitch = -viewangles.pitch;
  }
  AngleVectors(viewangles, viewforward, viewright, viewup);

  if (r_chasecam && cl->MO == cl->Camera)
  {
    vieworg = cl->MO->Origin + TVec(0.0, 0.0, 32.0)
      - r_chase_dist * viewforward + r_chase_up * viewup
      + r_chase_right * viewright;
  }
  else
  {
    vieworg = cl->ViewOrg;
  }

  ExtraLight = ViewEnt->Player ? ViewEnt->Player->ExtraLight * 8 : 0;
  if (cl->Camera == cl->MO)
  {
    ColourMap = CM_Default;
    if (cl->FixedColourmap == INVERSECOLOURMAP)
    {
      ColourMap = CM_Inverse;
      FixedLight = 255;
    }
    else if (cl->FixedColourmap == GOLDCOLOURMAP)
    {
      ColourMap = CM_Gold;
      FixedLight = 255;
    }
    else if (cl->FixedColourmap == REDCOLOURMAP)
    {
      ColourMap = CM_Red;
      FixedLight = 255;
    }
    else if (cl->FixedColourmap == GREENCOLOURMAP)
    {
      ColourMap = CM_Green;
      FixedLight = 255;
    }
    else if (cl->FixedColourmap >= NUMCOLOURMAPS)
    {
      FixedLight = 255;
    }
    else if (cl->FixedColourmap)
    {
      FixedLight = 255 - (cl->FixedColourmap << 3);
    }
    else
    {
      FixedLight = 0;
    }
  }
  else
  {
    FixedLight = 0;
    ColourMap = 0;
  }
  //  Inverse colourmap flash effect.
  if (cl->ExtraLight == 255)
  {
    ExtraLight = 0;
    ColourMap = CM_Inverse;
    FixedLight = 255;
  }

  Drawer->SetupView(this, &refdef);
  cacheframecount++;
  PortalDepth = 0;
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::SetupCameraFrame
//
//==========================================================================

void VRenderLevelShared::SetupCameraFrame(VEntity *Camera, VTexture *Tex,
  int FOV, refdef_t *rd)
{
  guard(VRenderLevelShared::SetupCameraFrame);
  rd->width = Tex->GetWidth();
  rd->height = Tex->GetHeight();
  rd->y = 0;
  rd->x = 0;

  if (aspect_ratio == 0)
  {
    PixelAspect = ((float)rd->height * 320.0) / ((float)rd->width * 200.0);
  }
  else if (aspect_ratio == 1)
  {
    PixelAspect = ((float)rd->height * 4.0) / ((float)rd->width * 3.0);
  }
  else if (aspect_ratio == 2)
  {
    PixelAspect = ((float)rd->height * 16.0) / ((float)rd->width * 9.0);
  }
  else if (aspect_ratio > 2)
  {
    PixelAspect = ((float)rd->height * 16.0) / ((float)rd->width * 10.0);
  }

  rd->fovx = tan(DEG2RAD(FOV) / 2);
  rd->fovy = rd->fovx * rd->height / rd->width / PixelAspect;

  // left side clip
  clip_base[0] = Normalise(TVec(1, 1.0 / rd->fovx, 0));

  // right side clip
  clip_base[1] = Normalise(TVec(1, -1.0 / rd->fovx, 0));

  // top side clip
  clip_base[2] = Normalise(TVec(1, 0, -1.0 / rd->fovy));

  // bottom side clip
  clip_base[3] = Normalise(TVec(1, 0, 1.0 / rd->fovy));

  rd->drawworld = true;

  ViewEnt = Camera;
  viewangles = Camera->Angles;
  AngleVectors(viewangles, viewforward, viewright, viewup);

  vieworg = Camera->Origin;

  ExtraLight = 0;
  FixedLight = 0;
  ColourMap = 0;

  Drawer->SetupView(this, rd);
  cacheframecount++;
  PortalDepth = 0;
  set_resolutioon_needed = true;
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::MarkLeaves
//
//==========================================================================

void VRenderLevelShared::MarkLeaves()
{
  guard(VRenderLevelShared::MarkLeaves);
  node_t *node;
  int   i;

  if (r_oldviewleaf == r_viewleaf)
    return;

  r_visframecount++;
  r_oldviewleaf = r_viewleaf;

  const vuint8 *vis = Level->LeafPVS(r_viewleaf);

  for (i = 0; i < Level->NumSubsectors; i++)
  {
    if (vis[i >> 3] & (1 << (i & 7)))
    {
      subsector_t *sub = &Level->Subsectors[i];
      sub->VisFrame = r_visframecount;
      node = sub->parent;
      while (node)
      {
        if (node->VisFrame == r_visframecount)
          break;
        node->VisFrame = r_visframecount;
        node = node->parent;
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  R_RenderPlayerView
//
//==========================================================================

void R_RenderPlayerView()
{
  guard(R_RenderPlayerView);
  GClLevel->RenderData->RenderPlayerView();
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderPlayerView
//
//==========================================================================

void VRenderLevelShared::RenderPlayerView()
{
  guard(VRenderLevelShared::RenderPlayerView);
  if (!Level->LevelInfo)
  {
    return;
  }

  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  GTextureManager.Time = Level->Time;

  BuildPlayerTranslations();

  AnimateSky(host_frametime);

  UpdateParticles(host_frametime);
  PushDlights();

  // update camera textures that were visible in last frame
  for (int i = 0; i < Level->CameraTextures.Num(); ++i) {
    UpdateCameraTexture(Level->CameraTextures[i].Camera, Level->CameraTextures[i].TexNum, Level->CameraTextures[i].FOV);
  }

  SetupFrame();

#ifdef VAVOOM_RENDER_TIMES
  double stt = -Sys_Time();
#endif
  RenderScene(&refdef, nullptr);
#ifdef VAVOOM_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("render scene time: %f", stt);
#endif

  // draw the psprites on top of everything
  if (fov <= 90.0 && cl->MO == cl->Camera &&
    GGameInfo->NetMode != NM_TitleMap)
  {
    DrawPlayerSprites();
  }

  Drawer->EndView();

  // Draw crosshair
  if (cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap)
  {
    DrawCrosshair();
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::UpdateCameraTexture
//
//==========================================================================

void VRenderLevelShared::UpdateCameraTexture(VEntity *Camera, int TexNum,
  int FOV)
{
  guard(VRenderLevelShared::UpdateCameraTexture);
  if (!Camera)
  {
    return;
  }

  if (!GTextureManager[TexNum]->bIsCameraTexture)
  {
    return;
  }
  VCameraTexture *Tex = (VCameraTexture*)GTextureManager[TexNum];
  if (!Tex->bNeedsUpdate)
  {
    return;
  }

  refdef_t    CameraRefDef;
  CameraRefDef.DrawCamera = true;

  SetupCameraFrame(Camera, Tex, FOV, &CameraRefDef);

  RenderScene(&CameraRefDef, nullptr);

  Drawer->EndView();

  Tex->CopyImage();
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::GetFade
//
//==========================================================================

vuint32 VRenderLevelShared::GetFade(sec_region_t *Reg)
{
  guard(VRenderLevelShared::GetFade);
  if (r_fog_test)
  {
    return 0xff000000 | (int(255 * r_fog_r) << 16) |
      (int(255 * r_fog_g) << 8) | int(255 * r_fog_b);
  }
  if (Reg->params->Fade)
  {
    return Reg->params->Fade;
  }
  if (Level->LevelInfo->OutsideFog && Reg->ceiling->pic == skyflatnum)
  {
    return Level->LevelInfo->OutsideFog;
  }
  if (Level->LevelInfo->Fade)
  {
    return Level->LevelInfo->Fade;
  }
  if (Level->LevelInfo->FadeTable == NAME_fogmap)
  {
    return 0xff7f7f7f;
  }
  if (r_fade_light)
  {
    // Simulate light fading using dark fog
    return FADE_LIGHT;
  }
  else
  {
    return 0;
  }
  unguard;
}

//==========================================================================
//
//  R_DrawPic
//
//==========================================================================

void R_DrawPic(int x, int y, int handle, float Alpha)
{
  guard(R_DrawPic);
  if (handle < 0)
  {
    return;
  }

  VTexture *Tex = GTextureManager(handle);
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()), fScaleY*(y+Tex->GetScaledHeight()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::PrecacheLevel
//
//  Preloads all relevant graphics for the level.
//
//==========================================================================

void VRenderLevelShared::PrecacheLevel()
{
  guard(VRenderLevelShared::PrecacheLevel);
  int     i;

  if (cls.demoplayback)
    return;

  char *texturepresent = (char *)Z_Malloc(GTextureManager.GetNumTextures());
  memset(texturepresent, 0, GTextureManager.GetNumTextures());

  for (i = 0; i < Level->NumSectors; i++)
  {
    texturepresent[Level->Sectors[i].floor.pic] = true;
    texturepresent[Level->Sectors[i].ceiling.pic] = true;
  }

  for (i = 0; i < Level->NumSides; i++)
  {
    texturepresent[Level->Sides[i].TopTexture] = true;
    texturepresent[Level->Sides[i].MidTexture] = true;
    texturepresent[Level->Sides[i].BottomTexture] = true;
  }

  // Precache textures.
  for (i = 1; i < GTextureManager.GetNumTextures(); i++)
  {
    if (texturepresent[i])
    {
      Drawer->PrecacheTexture(GTextureManager[i]);
    }
  }

  Z_Free(texturepresent);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::GetTranslation
//
//==========================================================================

VTextureTranslation *VRenderLevelShared::GetTranslation(int TransNum)
{
  guard(VRenderLevelShared::GetTranslation);
  return R_GetCachedTranslation(TransNum, Level);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::BuildPlayerTranslations
//
//==========================================================================

void VRenderLevelShared::BuildPlayerTranslations()
{
  guard(VRenderLevelShared::BuildPlayerTranslations);
  for (TThinkerIterator<VPlayerReplicationInfo> It(Level); It; ++It)
  {
    if (It->PlayerNum < 0 || It->PlayerNum >= MAXPLAYERS)
    {
      //  Should not happen.
      continue;
    }
    if (!It->TranslStart || !It->TranslEnd)
    {
      continue;
    }

    VTextureTranslation *Tr = PlayerTranslations[It->PlayerNum];
    if (Tr && Tr->TranslStart == It->TranslStart &&
      Tr->TranslEnd == It->TranslEnd && Tr->Colour == It->Colour)
    {
      continue;
    }

    if (!Tr)
    {
      Tr = new VTextureTranslation;
      PlayerTranslations[It->PlayerNum] = Tr;
    }
    //  Don't waste time clearing if it's the same range.
    if (Tr->TranslStart != It->TranslStart ||
      Tr->TranslEnd != It->TranslEnd)
    {
      Tr->Clear();
    }
    Tr->BuildPlayerTrans(It->TranslStart, It->TranslEnd, It->Colour);
  }
  unguard;
}

//==========================================================================
//
//  R_SetMenuPlayerTrans
//
//==========================================================================

int R_SetMenuPlayerTrans(int Start, int End, int Col)
{
  guard(R_SetMenuPlayerTrans);
  if (!Start || !End)
  {
    return 0;
  }

  VTextureTranslation *Tr = PlayerTranslations[MAXPLAYERS];
  if (Tr && Tr->TranslStart == Start && Tr->TranslEnd == End &&
    Tr->Colour == Col)
  {
    return (TRANSL_Player << TRANSL_TYPE_SHIFT) + MAXPLAYERS;
  }

  if (!Tr)
  {
    Tr = new VTextureTranslation();
    PlayerTranslations[MAXPLAYERS] = Tr;
  }
  if (Tr->TranslStart != Start || Tr->TranslEnd == End)
  {
    Tr->Clear();
  }
  Tr->BuildPlayerTrans(Start, End, Col);
  return (TRANSL_Player << TRANSL_TYPE_SHIFT) + MAXPLAYERS;
  unguard;
}

//==========================================================================
//
//  R_GetCachedTranslation
//
//==========================================================================

VTextureTranslation *R_GetCachedTranslation(int TransNum, VLevel *Level)
{
  guard(R_GetCachedTranslation);
  int Type = TransNum >> TRANSL_TYPE_SHIFT;
  int Index = TransNum & ((1 << TRANSL_TYPE_SHIFT) - 1);
  VTextureTranslation *Tr;
  switch (Type)
  {
  case TRANSL_Standard:
    if (Index == 7)
    {
      Tr = &IceTranslation;
    }
    else
    {
      if (Index < 0 || Index >= NumTranslationTables)
      {
        return nullptr;
      }
      Tr = TranslationTables[Index];
    }
    break;

  case TRANSL_Player:
    if (Index < 0 || Index >= MAXPLAYERS + 1)
    {
      return nullptr;
    }
    Tr = PlayerTranslations[Index];
    break;

  case TRANSL_Level:
    if (!Level || Index < 0 || Index >= Level->Translations.Num())
    {
      return nullptr;
    }
    Tr = Level->Translations[Index];
    break;

  case TRANSL_BodyQueue:
    if (!Level || Index < 0 || Index >= Level->BodyQueueTrans.Num())
    {
      return nullptr;
    }
    Tr = Level->BodyQueueTrans[Index];
    break;

  case TRANSL_Decorate:
    if (Index < 0 || Index >= DecorateTranslations.Num())
    {
      return nullptr;
    }
    Tr = DecorateTranslations[Index];
    break;

  case TRANSL_Blood:
    if (Index < 0 || Index >= BloodTranslations.Num())
    {
      return nullptr;
    }
    Tr = BloodTranslations[Index];
    break;

  default:
    return nullptr;
  }

  if (!Tr)
  {
    return nullptr;
  }

  for (int i = 0; i < CachedTranslations.Num(); i++)
  {
    VTextureTranslation *Check = CachedTranslations[i];
    if (Check->Crc != Tr->Crc)
    {
      continue;
    }
    if (memcmp(Check->Palette, Tr->Palette, sizeof(Tr->Palette)))
    {
      continue;
    }
    return Check;
  }

  VTextureTranslation *Copy = new VTextureTranslation;
  *Copy = *Tr;
  CachedTranslations.Append(Copy);
  return Copy;
  unguard;
}

//==========================================================================
//
//  COMMAND TimeRefresh
//
//  For program optimization
//
//==========================================================================

COMMAND(TimeRefresh)
{
  guard(COMMAND TimeRefresh);
  int     i;
  double    start, stop, time, RenderTime, UpdateTime;
  float   startangle;

  if (!cl)
  {
    return;
  }

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

  for (i = 0; i < 128; i++) {
    cl->ViewAngles.yaw = (float)(i) * 360.0 / 128.0;

    Drawer->StartUpdate();

    zone_malloc_call_count = 0;
    zone_realloc_call_count = 0;
    zone_free_call_count = 0;

    RenderTime -= Sys_Time();
    R_RenderPlayerView();
    RenderTime += Sys_Time();

    renderAlloc += zone_malloc_call_count;
    renderRealloc += zone_realloc_call_count;
    renderFree += zone_free_call_count;

    if (renderPeakAlloc < zone_malloc_call_count) renderPeakAlloc = zone_malloc_call_count;
    if (renderPeakRealloc < zone_realloc_call_count) renderPeakRealloc = zone_realloc_call_count;
    if (renderPeakFree < zone_free_call_count) renderPeakFree = zone_free_call_count;

    UpdateTime -= Sys_Time();
    Drawer->Update();
    UpdateTime += Sys_Time();
  }
  stop = Sys_Time();
  time = stop - start;
  GCon->Logf("%f seconds (%f fps)", time, 128 / time);
  GCon->Logf("Render time %f, update time %f", RenderTime, UpdateTime);
  GCon->Logf("Render malloc calls: %d", renderAlloc);
  GCon->Logf("Render realloc calls: %d", renderRealloc);
  GCon->Logf("Render free calls: %d", renderFree);
  GCon->Logf("Render peak malloc calls: %d", renderPeakAlloc);
  GCon->Logf("Render peak realloc calls: %d", renderPeakRealloc);
  GCon->Logf("Render peak free calls: %d", renderPeakFree);

  cl->ViewAngles.yaw = startangle;
  unguard;
}

//==========================================================================
//
//  V_Init
//
//==========================================================================

void V_Init()
{
  guard(V_Init);
  int DIdx = -1;
  for (int i = 0; i < DRAWER_MAX; i++)
  {
    if (!DrawerList[i])
      continue;
    //  Pick first available as default.
    if (DIdx == -1)
      DIdx = i;
    //  Check for user driver selection.
    if (DrawerList[i]->CmdLineArg && GArgs.CheckParm(DrawerList[i]->CmdLineArg))
      DIdx = i;
  }
  if (DIdx == -1)
    Sys_Error("No drawers are available");
  GCon->Logf(NAME_Init, "Selected %s", DrawerList[DIdx]->Description);
  //  Create drawer.
  Drawer = DrawerList[DIdx]->Creator();
  Drawer->Init();
  unguard;
}

//==========================================================================
//
//  V_Shutdown
//
//==========================================================================

void V_Shutdown()
{
  guard(V_Shutdown);
  if (Drawer)
  {
    Drawer->Shutdown();
    delete Drawer;
    Drawer = nullptr;
  }
  R_FreeModels();
  for (int i = 0; i < MAXPLAYERS + 1; i++)
  {
    if (PlayerTranslations[i])
    {
      delete PlayerTranslations[i];
      PlayerTranslations[i] = nullptr;
    }
  }
  for (int i = 0; i < CachedTranslations.Num(); i++)
  {
    delete CachedTranslations[i];
    CachedTranslations[i] = nullptr;
  }
  CachedTranslations.Clear();
  R_FreeSkyboxData();
  unguard;
}
