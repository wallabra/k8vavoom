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
#include "gamedefs.h"
#include "r_local.h"


//**************************************************************************
//
// VLMapCache
//
//**************************************************************************

//==========================================================================
//
//  VLMapCache::resetBlock
//
//==========================================================================
void VLMapCache::resetBlock (Item *block) noexcept {
  if (block->surf) {
    //GCon->Logf(NAME_Debug, "cleared surface cache pointer for surface %p", block->surf);
    block->surf->CacheSurf = nullptr;
    block->surf = nullptr;
  }
}


//==========================================================================
//
//  VLMapCache::clearCacheInfo
//
//==========================================================================
void VLMapCache::clearCacheInfo () noexcept {
}


//==========================================================================
//
//  VLMapCache::releaseAtlas
//
//==========================================================================
void VLMapCache::releaseAtlas (vuint32 id) noexcept {
}


//==========================================================================
//
//  VLMapCache::allocAtlas
//
//==========================================================================
VLMapCache::AtlasInfo VLMapCache::allocAtlas (vuint32 aid, int minwidth, int minheight) noexcept {
  AtlasInfo res;
  if (minwidth > BLOCK_WIDTH || minheight > BLOCK_HEIGHT) {
    GLog.Logf(NAME_Error, "requesting new atlas WITH INVALID SIZE! id=%u; minsize=(%d,%d)", aid, minwidth, minheight);
  } else {
    GLog.Logf("requesting new atlas; id=%u; minsize=(%d,%d)", aid, minwidth, minheight);
    if (aid < NUM_BLOCK_SURFS) {
      res.width = BLOCK_WIDTH;
      res.height = BLOCK_HEIGHT;
    }
  }
  return res;
}


//**************************************************************************
//
// VRenderLevelLightmap
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelLightmap::VRenderLevelLightmap
//
//==========================================================================
VRenderLevelLightmap::VRenderLevelLightmap (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
  , c_subdivides(0)
  , c_seg_div(0)
  //, cacheframecount(0)
  //, freeblocks(nullptr)
  , lmcache()
  , nukeLightmapsOnNextFrame(false)
  , invalidateRelight(false)
{
  NeedsInfiniteFarClip = false;
  mIsShadowVolumeRenderer = false;

  initLightChain();

  FlushCaches();
}


//==========================================================================
//
//  VRenderLevelLightmap::ClearQueues
//
//  clears render queues
//
//==========================================================================
void VRenderLevelLightmap::ClearQueues () {
  VRenderLevelShared::ClearQueues();
  LMSurfList.reset();
  advanceCacheFrame();
}


//==========================================================================
//
//  VRenderLevelLightmap::advanceCacheFrame
//
//==========================================================================
void VRenderLevelLightmap::advanceCacheFrame () {
  light_chain_head = /*add_chain_head =*/ 0;
  if (++lmcache.cacheframecount == 0) {
    lmcache.cacheframecount = 1;
    memset(light_chain_used, 0, sizeof(light_chain_used));
    //memset(add_chain_used, 0, sizeof(add_chain_used));
    //blockpool.resetFrames();
    lmcache.zeroFrames();
  }
  //NukeLightmapCache();
}


//==========================================================================
//
//  VRenderLevelLightmap::initLightChain
//
//==========================================================================
void VRenderLevelLightmap::initLightChain () {
  memset(light_block, 0, sizeof(light_block));
  memset(light_chain, 0, sizeof(light_chain));
  memset(add_block, 0, sizeof(add_block));
  //memset(add_chain, 0, sizeof(add_chain));
  memset(light_chain_used, 0, sizeof(light_chain_used));
  //memset(add_chain_used, 0, sizeof(add_chain_used));
  light_chain_head = /*add_chain_head =*/ 0;
  // force updating of all lightmaps
  //for (unsigned f = 0; f < NUM_BLOCK_SURFS; ++f) block_changed[f] = add_changed[f] = true;
  for (unsigned f = 0; f < NUM_BLOCK_SURFS; ++f) block_changed[f] = add_changed[f] = false;
}


//==========================================================================
//
//  VRenderLevelLightmap::chainLightmap
//
//==========================================================================
void VRenderLevelLightmap::chainLightmap (surfcache_t *cache) {
  vassert(cache);
  const vuint32 bnum = ((VLMapCache::Item *)cache)->atlasid;
  vassert(bnum < NUM_BLOCK_SURFS);
  if (light_chain_used[bnum].lastframe != lmcache.cacheframecount) {
    // first time, put into list
    light_chain_used[bnum].lastframe = lmcache.cacheframecount;
    light_chain_used[bnum].next = light_chain_head;
    light_chain_head = bnum+1;
    light_chain[bnum] = nullptr;
  }
  cache->chain = light_chain[bnum];
  light_chain[bnum] = cache;
  ((VLMapCache::Item *)cache)->lastframe = lmcache.cacheframecount;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainHead
//
//  lightmap chain iterator (used in renderer)
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelLightmap::GetLightChainHead () {
  return light_chain_head;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainNext
//
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelLightmap::GetLightChainNext (vuint32 bnum) {
  if (bnum--) {
    vassert(bnum < NUM_BLOCK_SURFS);
    return light_chain_used[bnum].next;
  }
  return 0;
}


//==========================================================================
//
//  VRenderLevelLightmap::IsLightBlockChanged
//
//==========================================================================
bool VRenderLevelLightmap::IsLightBlockChanged (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return block_changed[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::IsLightAddBlockChanged
//
//==========================================================================
bool VRenderLevelLightmap::IsLightAddBlockChanged (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return add_changed[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::SetLightBlockChanged
//
//==========================================================================
void VRenderLevelLightmap::SetLightBlockChanged (vuint32 bnum, bool value) {
  vassert(bnum < NUM_BLOCK_SURFS);
  block_changed[bnum] = value;
}


//==========================================================================
//
//  VRenderLevelLightmap::SetLightAddBlockChanged
//
//==========================================================================
void VRenderLevelLightmap::SetLightAddBlockChanged (vuint32 bnum, bool value) {
  vassert(bnum < NUM_BLOCK_SURFS);
  add_changed[bnum] = value;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightBlock
//
//==========================================================================
rgba_t *VRenderLevelLightmap::GetLightBlock (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return light_block[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightAddBlock
//
//==========================================================================
rgba_t *VRenderLevelLightmap::GetLightAddBlock (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return add_block[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainFirst
//
//==========================================================================
surfcache_t *VRenderLevelLightmap::GetLightChainFirst (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return light_chain[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::RenderScene
//
//==========================================================================
void VRenderLevelLightmap::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  r_viewleaf = Level->PointInSubsector(vieworg);

  TransformFrustum();

  Drawer->SetupViewOrg();

  //ClearQueues(); // moved to `PrepareWorldRender()`
  //MarkLeaves(); // moved to `PrepareWorldRender()`
  //if (!MirrorLevel && !r_disable_world_update) UpdateWorld(RD, Range);

  RenderWorld(RD, Range);

  //k8: no need to build list here, as things only processed once
  //BuildVisibleObjectsList();
  RenderMobjs(RPASS_Normal);

  DrawParticles();

  DrawTranslucentPolys();

  RenderPortals();
}
