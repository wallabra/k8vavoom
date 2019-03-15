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
#include "gl_local.h"
#include "render/r_local.h"

extern VCvarB r_decals_enabled;
extern VCvarI r_ambient;
extern VCvarB r_allow_ambient;

static VCvarB r_adv_masked_wall_vertex_light("r_adv_masked_wall_vertex_light", true, "Estimate lighting of masked wall using its vertices?", CVAR_Archive);

static VCvarB r_adv_limit_extrude("r_adv_limit_extrude", false, "Don't extrude shadow volumes further than light radius goes?", CVAR_Archive);

static VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
static VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
static VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
static VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

static VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels)?", CVAR_Archive|CVAR_PreInit);

static VCvarB gl_dbg_adv_render_textures_surface("gl_dbg_adv_render_textures_surface", true, "Render surface textures in advanced renderer?", CVAR_PreInit);
// this makes shadows glitch for some reason with fp z-buffer (investigate!)
static VCvarB gl_dbg_adv_render_offset_shadow_volume("gl_dbg_adv_render_offset_shadow_volume", false, "Offset shadow volumes?", CVAR_PreInit);
static VCvarB gl_dbg_adv_render_never_offset_shadow_volume("gl_dbg_adv_render_never_offset_shadow_volume", false, "Never offseting shadow volumes?", CVAR_Archive|CVAR_PreInit);

static VCvarB gl_dbg_render_stack_portal_bounds("gl_dbg_render_stack_portal_bounds", false, "Render sector stack portal bounds.", 0/*CVAR_Archive*/);

static VCvarB gl_use_stencil_quad_clear("gl_use_stencil_quad_clear", false, "Draw quad to clear stencil buffer instead of 'glClear'?", CVAR_Archive|CVAR_PreInit);

VCvarB r_decals_wall_masked("r_decals_wall_masked", true, "Render decals on masked walls?", CVAR_Archive);
VCvarB r_decals_wall_alpha("r_decals_wall_alpha", true, "Render decals on translucent walls?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
vuint32 glWDPolyTotal = 0;
vuint32 glWDVertexTotal = 0;
vuint32 glWDTextureChangesTotal = 0;


// ////////////////////////////////////////////////////////////////////////// //
struct SurfListItem {
  surface_t *surf;
  surfcache_t *cache;
};

VOpenGLDrawer::SurfListItem *VOpenGLDrawer::surfList = nullptr;
vuint32 VOpenGLDrawer::surfListUsed = 0;
vuint32 VOpenGLDrawer::surfListSize = 0;


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline int compareSurfaces (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColourMap)-((int)tb->ColourMap);
  }

  static int surfListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(((const SurfListItem *)a)->surf, ((const SurfListItem *)b)->surf);
  }

  static int drawListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
  }
}


//==========================================================================
//
//  getSurfLightLevel
//
//==========================================================================
static inline float getSurfLightLevel (const surface_t *surf) {
  if (!surf || !r_allow_ambient) return 0;
  int slins = (surf->Light>>24)&0xff;
  if (slins < r_ambient) slins = clampToByte(r_ambient);
  //slins = MAX(slins, r_ambient);
  //if (slins > 255) slins = 255;
  return float(slins)/255.0f;
}


//==========================================================================
//
//  glVertex
//
//==========================================================================
static inline void glVertex (const TVec &v) {
  glVertex3f(v.x, v.y, v.z);
}


//==========================================================================
//
//  VOpenGLDrawer::RenderPrepareShaderDecals
//
//==========================================================================
void VOpenGLDrawer::RenderPrepareShaderDecals (surface_t *surf) {
  if (!r_decals_enabled) return;
  if (RendLev->PortalDepth) return; //FIXME: not yet

  if (!surf->dcseg || !surf->dcseg->decals) return; // nothing to do

  if (gl_decal_debug_nostencil) return; // debug

  if (!decalUsedStencil) decalStcVal = (IsStencilBufferDirty() ? 255 : 0);

  if (++decalStcVal == 0) {
    // it wrapped, so clear stencil buffer
    ClearStencilBuffer();
    decalStcVal = 1;
  }
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, /*GL_INCR*/GL_REPLACE);
  decalUsedStencil = true;
  NoteStencilBufferDirty(); // it will be dirtied
}


//==========================================================================
//
//  VOpenGLDrawer::RenderFinishShaderDecals
//
//  returns `true` if caller should restore vertex program and other params
//  (i.e. some actual decals were rendered)
//
//==========================================================================
bool VOpenGLDrawer::RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap) {
  if (!r_decals_enabled) return false;

  if (!surf->dcseg || !surf->dcseg->decals) return false; // nothing to do

  texinfo_t *tex = surf->texinfo;

  // setup shader
  switch (dtype) {
    case DT_SIMPLE:
      p_glUseProgramObjectARB(SurfDecalNoLMap_Program);
      p_glUniform1iARB(SurfDecalNoLMap_TextureLoc, 0);
      SurfDecalNoLMap_Locs.storeFogType();
      SurfDecalNoLMap_Locs.storeFogFade(surf->Fade, 1.0f);
      break;
    case DT_LIGHTMAP:
      p_glUseProgramObjectARB(SurfDecalLMap_Program);
      p_glUniform1iARB(SurfDecalLMap_TextureLoc, 0);
      SurfDecalLMap_Locs.storeFogType();
      SurfDecalLMap_Locs.storeFogFade(surf->Fade, 1.0f);
      SurfDecalLMap_Locs.storeLMap(1);
      p_glUniform1iARB(SurfDecalLMap_SpecularMapLoc, 2);
      SurfDecalLMap_Locs.storeLMapOnlyParams(tex, surf, cache);
      break;
    case DT_ADVANCED:
      p_glUseProgramObjectARB(SurfAdvDecal_Program);
      p_glUniform1iARB(SurfAdvDecal_TextureLoc, 0);
      break;
    default:
      abort();
  }

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
  GLint oldBlendEnabled;
  glGetIntegerv(GL_BLEND, &oldBlendEnabled);

  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  if (!oldBlendEnabled) glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST); // just in case

  if (gl_decal_debug_nostencil) glDisable(GL_STENCIL_TEST);
  if (gl_decal_debug_noalpha) glDisable(GL_BLEND);

  // also, clear dead decals here, 'cause why not?
  decal_t *prev = nullptr;
  decal_t *dc = surf->dcseg->decals;

  int rdcount = 0;
  static int maxrdcount = 0;
  if (gl_decal_reset_max) { maxrdcount = 0; gl_decal_reset_max = false; }

  bool tex1set = false;
  int currTexId = -1; // don't call `SetTexture()` repeatedly

  while (dc) {
    // "0" means "no texture found", so remove it too
    if (dc->texture <= 0 || dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0) {
      // remove it
      decal_t *n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      } else {
        prev = dc;
      }
      dc = n;
      continue;
    }

    int dcTexId = dc->texture;
    auto dtex = GTextureManager[dcTexId];
    if (!dtex || dtex->Width < 1 || dtex->Height < 1) {
      // remove it
      decal_t *n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      } else {
        prev = dc;
      }
      dc = n;
      continue;
    }

    // use origScale to get the original starting point
    //float txwC = dtex->SOffset*dc->origScaleX;
    float txhC = dtex->TOffset*dc->origScaleY;
    float txwC = dtex->SOffset*dc->scaleX;
    //float txhC = dtex->TOffset*dc->scaleY;

    float txw = dtex->GetWidth()*dc->scaleX;
    float txh = dtex->GetHeight()*dc->scaleY;
    //float txw = dtex->GetWidth()*dc->origScaleX;
    //float txh = dtex->GetHeight()*dc->origScaleY;

    if (txw < 1 || txh < 1) {
      // remove it, if it is not animated
      decal_t *n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      }
      dc = n;
      continue;
    }

    // setup shader
    switch (dtype) {
      case DT_SIMPLE:
        {
          const float lev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));
          p_glUniform4fARB(SurfDecalNoLMap_LightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
          p_glUniform1fARB(SurfDecalNoLMap_SplatAlphaLoc, dc->alpha);
        }
        break;
      case DT_LIGHTMAP:
        {
          const float lev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));
          p_glUniform4fARB(SurfDecalLMap_LightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
          p_glUniform1fARB(SurfDecalLMap_SplatAlphaLoc, dc->alpha);
        }
        break;
      case DT_ADVANCED:
        {
          p_glUniform1fARB(SurfAdvDecal_SplatAlphaLoc, dc->alpha);
          if (!tex1set) {
            tex1set = true;
            p_glActiveTextureARB(GL_TEXTURE0+1);
            glBindTexture(GL_TEXTURE_2D, ambLightFBOColorTid);
            p_glActiveTextureARB(GL_TEXTURE0);
          }
          p_glUniform4fARB(SurfAdvDecal_FullBright, 0, 0, 0, (dc->flags&decal_t::Fullbright ? 1.0f : 0.0f));
          p_glUniform1iARB(SurfAdvDecal_AmbLightTextureLoc, 1);
          p_glUniform2fARB(SurfAdvDecal_ScreenSize, (float)ScreenWidth, (float)ScreenHeight);
        }
        break;
    }

    if (currTexId != dcTexId) {
      currTexId = dcTexId;
      SetTexture(dtex, /*tex->ColourMap*/cmap); // this sets `tex_iw` and `tex_ih`
    }

    TVec lv1 = *(dc->seg->side ? dc->seg->linedef->v2 : dc->seg->linedef->v1);
    TVec lv2 = *(dc->seg->side ? dc->seg->linedef->v1 : dc->seg->linedef->v2);

    TVec dir = (lv2-lv1)/dc->linelen;
    //fprintf(stderr, "txwC=%f\n", txwC);
    float xstofs = dc->xdist-txwC+dc->ofsX;
    TVec v0 = lv1+dir*xstofs;
    TVec v2 = lv1+dir*(xstofs+txw);

    //float dcz = dc->curz+txh2-dc->ofsY;
    //float dcz = dc->curz+dc->ofsY-txhC;
    //float dcz = dc->curz-txhC+dc->ofsY;
    float dcz = dc->curz+txhC+dc->ofsY;
    // fix Z, if necessary
    if (dc->flags&decal_t::SlideFloor) {
      // should slide with back floor
      dcz += dc->bsec->floor.TexZ;
    } else if (dc->flags&decal_t::SlideCeil) {
      // should slide with back ceiling
      dcz += dc->bsec->ceiling.TexZ;
    }

    float texx0 = (dc->flags&decal_t::FlipX ? 1.0f : 0.0f);
    float texx1 = (dc->flags&decal_t::FlipX ? 0.0f : 1.0f);
    float texy1 = (dc->flags&decal_t::FlipY ? 1.0f : 0.0f);
    float texy0 = (dc->flags&decal_t::FlipY ? 0.0f : 1.0f);

    glBegin(GL_QUADS);
      glTexCoord2f(texx0, texy0); glVertex3f(v0.x, v0.y, dcz-txh);
      glTexCoord2f(texx0, texy1); glVertex3f(v0.x, v0.y, dcz);
      glTexCoord2f(texx1, texy1); glVertex3f(v2.x, v2.y, dcz);
      glTexCoord2f(texx1, texy0); glVertex3f(v2.x, v2.y, dcz-txh);
    glEnd();

    prev = dc;
    dc = dc->next;

    ++rdcount;
  }

  if (rdcount > maxrdcount) {
    maxrdcount = rdcount;
    if (gl_decal_dump_max) GCon->Logf("*** max decals on seg: %d", maxrdcount);
  }

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  if (tex1set) {
    p_glActiveTextureARB(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, 0);
    p_glActiveTextureARB(GL_TEXTURE0);
  }

  //if (dtype != DT_ADVANCED) glDisable(GL_BLEND);
  //if (oldBlendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  if (!oldBlendEnabled) glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(oldDepthMask);

  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSimpleSurface
//
//  returns `true` if we need to re-setup texture
//
//==========================================================================
bool VOpenGLDrawer::RenderSimpleSurface (bool textureChanged, surface_t *surf) {
  texinfo_t *textr = surf->texinfo;

  if (textureChanged) {
    SetTexture(textr->Tex, textr->ColourMap);
    ++glWDTextureChangesTotal;
  }

  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render simple surface with %d vertices", surf->count);
    return false;
  }

  SurfSimple_Locs.storeTextureParams(textr);

  const float lev = getSurfLightLevel(surf);
  p_glUniform4fARB(SurfSimple_LightLoc, ((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);

  SurfSimple_Locs.storeFogFade(surf->Fade, 1.0f);

  bool doDecals = textr->Tex && !textr->noDecals && surf->dcseg && surf->dcseg->decals;

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  ++glWDPolyTotal;
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < surf->count; ++i) {
    ++glWDVertexTotal;
    glVertex(surf->verts[i]);
  }
  glEnd();

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, textr->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      p_glUseProgramObjectARB(SurfSimple_Program);
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderLMapSurface
//
//  returns `true` if we need to re-setup texture
//
//==========================================================================
bool VOpenGLDrawer::RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache) {
  texinfo_t *tex = surf->texinfo;

  if (textureChanged) {
    SetTexture(tex->Tex, tex->ColourMap);
    ++glWDTextureChangesTotal;
  }

  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render lmap surface with %d vertices", surf->count);
    return false;
  }

  SurfLightmap_Locs.storeTextureLMapParams(tex, surf, cache);
  SurfLightmap_Locs.storeFogFade(surf->Fade, 1.0f);

  bool doDecals = (tex->Tex && !tex->noDecals && surf->dcseg && surf->dcseg->decals);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  ++glWDPolyTotal;
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < surf->count; ++i) {
    ++glWDVertexTotal;
    glVertex(surf->verts[i]);
  }
  glEnd();

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_LIGHTMAP, surf, cache, tex->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      p_glUseProgramObjectARB(SurfLightmap_Program);
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::UpdateAndUploadSurfaceTexture
//
//  update/(re)generate texture if necessary
//
//==========================================================================
void VOpenGLDrawer::UpdateAndUploadSurfaceTexture (surface_t *surf) {
  texinfo_t *textr = surf->texinfo;
  auto Tex = textr->Tex;
  if (Tex->CheckModified()) FlushTexture(Tex);
  auto CMap = textr->ColourMap;
  if (CMap) {
    VTexture::VTransData *TData = Tex->FindDriverTrans(nullptr, CMap);
    if (!TData) {
      TData = &Tex->DriverTranslated.Alloc();
      TData->Handle = 0;
      TData->Trans = nullptr;
      TData->ColourMap = CMap;
      GenerateTexture(Tex, (GLuint*)&TData->Handle, nullptr, CMap, false);
    }
  } else if (!Tex->DriverHandle) {
    GenerateTexture(Tex, &Tex->DriverHandle, nullptr, 0, false);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::WorldDrawing
//
//==========================================================================
void VOpenGLDrawer::WorldDrawing () {
  // first draw horizons
  {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // for sky areas we just write to the depth buffer to prevent drawing polygons behind the sky
  {
    p_glUseProgramObjectARB(SurfZBuf_Program);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render sky portal surface with %d vertices", surf->count);
        continue;
      }
      glBegin(GL_POLYGON);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw surfaces without lightmaps
  if (RendLev->DrawSurfList.length()) {
    // sort by texture, to minimise texture switches
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    p_glUseProgramObjectARB(SurfSimple_Program);

    SurfSimple_Locs.storeTexture(0);
    SurfSimple_Locs.storeFogType();

    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();
    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo) continue; // just in case
      bool textureChanded =
        !lastTexinfo ||
        lastTexinfo != currTexinfo ||
        lastTexinfo->Tex != currTexinfo->Tex ||
        lastTexinfo->ColourMap != currTexinfo->ColourMap;
      lastTexinfo = currTexinfo;
      if (RenderSimpleSurface(textureChanded, surf)) lastTexinfo = nullptr;
    }
  }

  // draw surfaces with lightmaps
  {
    p_glUseProgramObjectARB(SurfLightmap_Program);
    SurfLightmap_Locs.storeTexture(0);
    SurfLightmap_Locs.storeLMap(1);
    SurfLightmap_Locs.storeFogType();
    p_glUniform1iARB(SurfLightmap_SpecularMapLoc, 2);

    const texinfo_t *lastTexinfo = nullptr;
    for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb) {
      if (!RendLev->light_chain[lb]) continue;

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      if (RendLev->block_changed[lb]) {
        RendLev->block_changed[lb] = false;
        glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->light_block[lb]);
        RendLev->add_changed[lb] = true;
      }

      SelectTexture(2);
      glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      if (RendLev->add_changed[lb]) {
        RendLev->add_changed[lb] = false;
        glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->add_block[lb]);
      }

      SelectTexture(0);

      if (!gl_sort_textures) {
        for (surfcache_t *cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
          surface_t *surf = cache->surf;
          if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
          const texinfo_t *currTexinfo = surf->texinfo;
          bool textureChanded =
            !lastTexinfo ||
            lastTexinfo != currTexinfo ||
            lastTexinfo->Tex != currTexinfo->Tex ||
            lastTexinfo->ColourMap != currTexinfo->ColourMap;
          lastTexinfo = currTexinfo;
          if (RenderLMapSurface(textureChanded, surf, cache)) lastTexinfo = nullptr;
        }
      } else {
        surfListClear();
        for (surfcache_t *cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
          surface_t *surf = cache->surf;
          if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
          surfListAppend(surf, cache);
        }
        if (surfListUsed > 0) {
          timsort_r(surfList, surfListUsed, sizeof(surfList[0]), &surfListItemCmp, nullptr);
          for (vuint32 sidx = 0; sidx < surfListUsed; ++sidx) {
            surface_t *surf = surfList[sidx].surf;
            const texinfo_t *currTexinfo = surf->texinfo;
            bool textureChanded =
              !lastTexinfo ||
              lastTexinfo != currTexinfo ||
              lastTexinfo->Tex != currTexinfo->Tex ||
              lastTexinfo->ColourMap != currTexinfo->ColourMap;
            lastTexinfo = currTexinfo;
            if (RenderLMapSurface(textureChanded, surf, surfList[sidx].cache)) lastTexinfo = nullptr;
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldAmbientPass
//
//  this renders sector ambient light based on sector light level
//  it can be optimised: we don't need to do any texture interpolation for
//  textures without transparent pixels
//
//==========================================================================
void VOpenGLDrawer::DrawWorldAmbientPass () {
  // draw horizons
  {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (RendLev->DrawSkyList.length()) {
    p_glUseProgramObjectARB(SurfZBuf_Program);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render sky portal surface with %d vertices", surf->count);
        continue;
      }
      glBegin(GL_POLYGON);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw normal surfaces
  if (RendLev->DrawSurfList.length()) {
    p_glUseProgramObjectARB(ShadowsAmbient_Program);
    ShadowsAmbient_Locs.storeTexture(0);

    // other passes can skip surface sorting
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();
    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render simple ambient surface with %d vertices", surf->count);
        continue;
      }

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
      if (currTexinfo->Alpha < 1.0f) continue;
      bool textureChanded =
        !lastTexinfo ||
        lastTexinfo != currTexinfo ||
        lastTexinfo->Tex != currTexinfo->Tex ||
        lastTexinfo->ColourMap != currTexinfo->ColourMap;
      lastTexinfo = currTexinfo;

      if (textureChanded) {
        SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
        ShadowsAmbient_Locs.storeTextureParams(currTexinfo);
      }

      const float lev = getSurfLightLevel(surf);
      p_glUniform4fARB(ShadowsAmbient_LightLoc,
        ((surf->Light>>16)&255)*lev/255.0f,
        ((surf->Light>>8)&255)*lev/255.0f,
        (surf->Light&255)*lev/255.0f, 1.0f);

      glBegin(GL_POLYGON);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginShadowVolumesPass
//
//  setup general rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginShadowVolumesPass () {
  glEnable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//  setup rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowVolumes (bool hasScissor, const int scoords[4]) {
  glDisable(GL_TEXTURE_2D);
  if (hasScissor) {
    if (gl_use_stencil_quad_clear) {
      //GLog.Logf("SCISSOR CLEAR: (%d,%d)-(%d,%d)", scoords[0], scoords[1], scoords[2], scoords[3]);
      //GLint oldStencilTest;
      //glGetIntegerv(GL_STENCIL_TEST, &oldStencilTest);
      GLint glmatmode;
      glGetIntegerv(GL_MATRIX_MODE, &glmatmode);
      GLint oldDepthTest;
      glGetIntegerv(GL_DEPTH_TEST, &oldDepthTest);
      GLint oldDepthMask;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

      //glDisable(GL_STENCIL_TEST);
      glEnable(GL_SCISSOR_TEST);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_BLEND);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
      glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
      //glMatrixMode(GL_TEXTURE); glPushMatrix();
      //glMatrixMode(GL_COLOR); glPushMatrix();

      p_glUseProgramObjectARB(0);
      glStencilFunc(GL_ALWAYS, 0x0, 0xff);
      glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

      glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(ScreenWidth, 0);
        glVertex2i(ScreenWidth, ScreenHeight);
        glVertex2i(0, ScreenHeight);
      glEnd();
      //glBindTexture(GL_TEXTURE_2D, 0);

      //glDisable(GL_STENCIL_TEST);
      //if (oldStencilTest) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
      glMatrixMode(GL_PROJECTION); glPopMatrix();
      glMatrixMode(GL_MODELVIEW); glPopMatrix();
      glMatrixMode(glmatmode);
      glDepthMask(oldDepthMask);
      if (oldDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    } else {
      glEnable(GL_SCISSOR_TEST);
      ClearStencilBuffer();
    }
  } else {
    glDisable(GL_SCISSOR_TEST);
    ClearStencilBuffer();
  }
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //!glEnable(GL_POLYGON_OFFSET_FILL);

  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);

  if (!CanUseRevZ()) {
    // normal
    //k8: this seems to be unnecessary
    if (!gl_dbg_adv_render_never_offset_shadow_volume) {
      if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer) {
        glPolygonOffset(1.0f, 10.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
      }
    }
    glDepthFunc(GL_LESS);
    //glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    //k8: this seems to be unnecessary
    if (!gl_dbg_adv_render_never_offset_shadow_volume) {
      if (gl_dbg_adv_render_offset_shadow_volume) {
        glPolygonOffset(-1.0f, -10.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
      }
    }
    glDepthFunc(GL_GREATER);
    //glDepthFunc(GL_GEQUAL);
  }
  p_glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
  p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

  p_glUseProgramObjectARB(SurfZBuf_Program);
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  RestoreDepthFunc();
  // meh, just turn if off each time
  /*if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer)*/ {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
  }
  //glDisable(GL_SCISSOR_TEST);
  glEnable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowVolume
//
//  `LightCanCross` means that light can span over this surface
//  light can span over two-sided midtex, for example, but not over
//  one-sided wall
//    <0: horizon
//    >0: two-sided wall
//    =0: one-sided wall
//
//  most checks are done in caller
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius, int LightCanCross) {
  if (surf->count < 3) return; // just in case

  //FIXME: move this to drawer class
  static TVec *poolVec = nullptr;
  static int poolVecSize = 0;

  if (poolVecSize < surf->count) {
    poolVecSize = (surf->count|0xfff)+1;
    poolVec = (TVec *)Z_Realloc(poolVec, poolVecSize*sizeof(TVec));
  }

  TVec *v = poolVec;

  // there is no need to extrude light further than light raduis
  const float mult = (r_adv_limit_extrude ? Radius+128 : M_INFINITY);
  for (int i = 0; i < surf->count; ++i) {
    v[i] = (surf->verts[i]-LightPos).normalised();
    v[i] *= mult;
    v[i] += LightPos;
  }

  glBegin(GL_POLYGON);
  for (int i = surf->count-1; i >= 0; --i) glVertex(v[i]);
  glEnd();

  glBegin(GL_POLYGON);
  for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();

  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i < surf->count; ++i) {
    glVertex(surf->verts[i]);
    glVertex(v[i]);
  }
  glVertex(surf->verts[0]);
  glVertex(v[0]);
  glEnd();

  NoteStencilBufferDirty();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//  setup rendering parameters for lighted surface rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, vuint32 Colour, bool doShadow) {
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glStencilFunc(GL_EQUAL, 0x0, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // do not use stencil test if we rendered no shadow surfaces
  if (doShadow && IsStencilBufferDirty()) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);

  p_glUseProgramObjectARB(ShadowsLight_Program);
  p_glUniform3fARB(ShadowsLight_LightPosLoc, LightPos.x, LightPos.y, LightPos.z);
  p_glUniform1fARB(ShadowsLight_LightRadiusLoc, Radius);
  p_glUniform3fARB(ShadowsLight_LightColourLoc,
    ((Colour>>16)&255)/255.0f,
    ((Colour>>8)&255)/255.0f,
    (Colour&255)/255.0f);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSurfaceLight
//
//  this blends surfaces from light sources to ambient map.
//
//  `LightCanCross` means that light can span over this surface
//  light can span over two-sided midtex, for example, but not over
//  one-sided wall
//    <0: horizon
//    >0: two-sided wall
//    =0: one-sided wall
//
//  most checks are done in caller
//
//==========================================================================
void VOpenGLDrawer::DrawSurfaceLight (surface_t *surf) {
  //if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render light surface with %d vertices", surf->count);
    return;
  }

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  ShadowsLight_Locs.storeTexture(0);
  ShadowsLight_Locs.storeTextureParams(tex);
  p_glVertexAttrib3fvARB(ShadowsLight_SurfNormalLoc, &surf->plane->normal.x);
  p_glVertexAttrib1fvARB(ShadowsLight_SurfDistLoc, &surf->plane->dist);
  p_glUniform3fARB(ShadowsLight_ViewOriginLoc, vieworg.x, vieworg.y, vieworg.z);

  glBegin(GL_POLYGON);
  for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldTexturesPass
//
//  this renders textured level with ambient lighting applied
//  this is for advanced renderer only
//  depth mask should be off
//
//==========================================================================
void VOpenGLDrawer::DrawWorldTexturesPass () {
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes

  // copy ambient light texture to FBO, so we can use it to light decals
  {
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, ambLightFBO);
    glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    //glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE/*GL_TRUE*/);
    p_glUseProgramObjectARB(0);

    glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
      glTexCoord2f(1.0f, 1.0f); glVertex2i(ScreenWidth, 0);
      glTexCoord2f(1.0f, 0.0f); glVertex2i(ScreenWidth, ScreenHeight);
      glTexCoord2f(0.0f, 0.0f); glVertex2i(0, ScreenHeight);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
    glPopAttrib();
  }


  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glEnable(GL_BLEND);

  if (!gl_dbg_adv_render_textures_surface || RendLev->DrawSurfList.length() == 0) return;

  p_glUseProgramObjectARB(ShadowsTexture_Program);
  //p_glUniform1iARB(ShadowsTexture_TextureLoc, 0);
  ShadowsTexture_Locs.storeTexture(0);

  // no need to sort surfaces there, it is already done in ambient pass
  const texinfo_t *lastTexinfo = nullptr;
  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render texture surface with %d vertices", surf->count);
      continue;
    }

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f) continue;

    bool textureChanded =
      !lastTexinfo ||
      lastTexinfo != currTexinfo ||
      lastTexinfo->Tex != currTexinfo->Tex ||
      lastTexinfo->ColourMap != currTexinfo->ColourMap;
    lastTexinfo = currTexinfo;

    if (textureChanded) {
      SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
      ShadowsTexture_Locs.storeTextureParams(currTexinfo);
    }

    bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->dcseg && surf->dcseg->decals);

    // fill stencil buffer for decals
    if (doDecals) RenderPrepareShaderDecals(surf);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      /*
      p_glVertexAttrib2fARB(ShadowsTexture_TexCoordLoc,
        (DotProduct(surf->verts[i], currTexinfo->saxis)+currTexinfo->soffs)*tex_iw,
        (DotProduct(surf->verts[i], currTexinfo->taxis)+currTexinfo->toffs)*tex_ih);
      */
      glVertex(surf->verts[i]);
    }
    glEnd();

    if (doDecals) {
      if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColourMap)) {
        p_glUseProgramObjectARB(ShadowsTexture_Program);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        //glEnable(GL_BLEND);
        lastTexinfo = nullptr; // resetup texture
      }
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldFogPass
//
//==========================================================================
void VOpenGLDrawer::DrawWorldFogPass () {
  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); // no z-buffer writes

  // draw surfaces
  p_glUseProgramObjectARB(ShadowsFog_Program);
  //p_glUniform1iARB(ShadowsFog_FogTypeLoc, r_fog&3);
  ShadowsFog_Locs.storeFogType();

  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (!surf->Fade) continue;
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render fog surface with %d vertices", surf->count);
      continue;
    }

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f) continue;

    ShadowsFog_Locs.storeFogFade(surf->Fade, 1.0f);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  glDisable(GL_BLEND);
  // back to normal z-buffering
  glDepthMask(GL_TRUE); // allow z-buffer writes
}


//==========================================================================
//
//  VOpenGLDrawer::DoHorizonPolygon
//
//==========================================================================
void VOpenGLDrawer::DoHorizonPolygon (surface_t *surf) {
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render horizon surface with %d vertices", surf->count);
    return;
  }

  const float Dist = 4096.0f;
  TVec v[4];
  if (surf->HorizonPlane->normal.z > 0.0f) {
    v[0] = surf->verts[0];
    v[3] = surf->verts[3];
    TVec HDir = -surf->plane->normal;

    TVec Dir1 = Normalise(vieworg-surf->verts[1]);
    TVec Dir2 = Normalise(vieworg-surf->verts[2]);
    float Mul1 = 1.0f/DotProduct(HDir, Dir1);
    v[1] = surf->verts[1]+Dir1*Mul1*Dist;
    float Mul2 = 1.0f/DotProduct(HDir, Dir2);
    v[2] = surf->verts[2]+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[1] = surf->verts[1]+Dir1*Mul1*Dist*(surf->verts[1].z-surf->verts[0].z)/(surf->verts[1].z-v[1].z);
      v[2] = surf->verts[2]+Dir2*Mul2*Dist*(surf->verts[2].z-surf->verts[3].z)/(surf->verts[2].z-v[2].z);
    }
  } else {
    v[1] = surf->verts[1];
    v[2] = surf->verts[2];
    TVec HDir = -surf->plane->normal;

    TVec Dir1 = Normalise(vieworg-surf->verts[0]);
    TVec Dir2 = Normalise(vieworg-surf->verts[3]);
    float Mul1 = 1.0f/DotProduct(HDir, Dir1);
    v[0] = surf->verts[0]+Dir1*Mul1*Dist;
    float Mul2 = 1.0f/DotProduct(HDir, Dir2);
    v[3] = surf->verts[3]+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[0] = surf->verts[0]+Dir1*Mul1*Dist*(surf->verts[1].z-surf->verts[0].z)/(v[0].z-surf->verts[0].z);
      v[3] = surf->verts[3]+Dir2*Mul2*Dist*(surf->verts[2].z-surf->verts[3].z)/(v[3].z-surf->verts[3].z);
    }
  }

  texinfo_t *Tex = surf->texinfo;
  SetTexture(Tex->Tex, Tex->ColourMap);

  p_glUseProgramObjectARB(SurfSimple_Program);
  SurfSimple_Locs.storeTexture(0);
  SurfSimple_Locs.storeFogType();
  SurfSimple_Locs.storeTextureParams(Tex);

  const float lev = getSurfLightLevel(surf);
  p_glUniform4fARB(SurfSimple_LightLoc, ((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
  SurfSimple_Locs.storeFogFade(surf->Fade, 1.0f);

  // draw it
  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glBegin(GL_POLYGON);
  for (unsigned i = 0; i < 4; ++i) glVertex(v[i]);
  glEnd();
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  glDepthMask(oldDepthMask);

  // write to the depth buffer
  p_glUseProgramObjectARB(SurfZBuf_Program);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glBegin(GL_POLYGON);
  for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSkyPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                                    float offs1, VTexture *Texture2, float offs2, int CMap)
{
  int sidx[4];

  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render sky surface with %d vertices", surf->count);
    return;
  }

  SetFade(surf->Fade);
  sidx[0] = 0;
  sidx[1] = 1;
  sidx[2] = 2;
  sidx[3] = 3;
  if (!bIsSkyBox) {
    if (surf->verts[1].z > 0) {
      sidx[1] = 0;
      sidx[2] = 3;
    } else {
      sidx[0] = 1;
      sidx[3] = 2;
    }
  }
  const texinfo_t *tex = surf->texinfo;

  if (Texture2->Type != TEXTYPE_Null) {
    SetTexture(Texture1, CMap);
    SelectTexture(1);
    SetTexture(Texture2, CMap);
    SelectTexture(0);

    p_glUseProgramObjectARB(SurfDSky_Program);
    p_glUniform1iARB(SurfDSky_TextureLoc, 0);
    p_glUniform1iARB(SurfDSky_Texture2Loc, 1);
    p_glUniform1fARB(SurfDSky_BrightnessLoc, r_sky_bright_factor);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      p_glVertexAttrib2fARB(SurfDSky_TexCoordLoc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs1)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      p_glVertexAttrib2fARB(SurfDSky_TexCoord2Loc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs2)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  } else {
    SetTexture(Texture1, CMap);

    p_glUseProgramObjectARB(SurfSky_Program);
    p_glUniform1iARB(SurfSky_TextureLoc, 0);
    p_glUniform1fARB(SurfSky_BrightnessLoc, r_sky_bright_factor);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      p_glVertexAttrib2fARB(SurfSky_TexCoordLoc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs1)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) {
  if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render masked surface with %d vertices", surf->count);
    return;
  }

  texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  if (!gl_dbg_adv_render_textures_surface && RendLev->IsAdvancedRenderer()) return;

  p_glUseProgramObjectARB(SurfMasked_Program);
  p_glUniform1iARB(SurfMasked_TextureLoc, 0);
  SurfMasked_Locs.storeFogType();

  bool zbufferWriteDisabled = false;
  bool decalsAllowed = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (blend_sprites || Additive || Alpha < 1.0f) {
    //p_glUniform1fARB(SurfMaskedAlphaRefLoc, getAlphaThreshold());
    restoreBlend = true;
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, (Additive ? getAlphaThreshold() : 0.666f));
    glEnable(GL_BLEND);
    if (Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // translucent things should not modify z-buffer
    if (Additive || Alpha < 1.0f) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
    }
    if (r_decals_enabled && r_decals_wall_alpha && surf->dcseg && surf->dcseg->decals) {
      decalsAllowed = true;
    }
  } else {
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, 0.666f);
    Alpha = 1.0f;
    if (r_decals_enabled && r_decals_wall_masked && surf->dcseg && surf->dcseg->decals) {
      decalsAllowed = true;
    }
  }

  if (surf->lightmap != nullptr || (!RendLev->IsAdvancedRenderer() && surf->dlightframe == RendLev->currDLightFrame)) {
    RendLev->BuildLightMap(surf);
    int w = (surf->extents[0]>>4)+1;
    int h = (surf->extents[1]>>4)+1;
    int size = w*h;
    int r = 0;
    int g = 0;
    int b = 0;
    for (int i = 0; i < size; ++i) {
      r += 255*256-blocklightsr[i];
      g += 255*256-blocklightsg[i];
      b += 255*256-blocklightsb[i];
    }
    double iscale = 1.0f/(size*255*256);
    p_glUniform4fARB(SurfMasked_LightLoc, r*iscale, g*iscale, b*iscale, Alpha);
  } else {
    if (r_adv_masked_wall_vertex_light && RendLev->IsAdvancedRenderer()) {
      // collect vertex lighting
      //FIXME: this should be rendered in ambient pass instead
      //       also, we can subdivide surfaces for two-sided walls for
      //       better estimations
      int w = (surf->extents[0]>>4)+1;
      int h = (surf->extents[1]>>4)+1;
      float radius = MIN(w, h);
      if (radius < 0.0f) radius = 0.0f;
      int r = 0, g = 0, b = 0;
      // sector light
      if (r_allow_ambient) {
        int slins = (surf->Light>>24)&0xff;
        if (slins < r_ambient) slins = clampToByte(r_ambient);
        int lr = (surf->Light>>16)&255;
        int lg = (surf->Light>>8)&255;
        int lb = surf->Light&255;
        lr = lr*slins/255;
        lg = lg*slins/255;
        lb = lb*slins/255;
        if (r < lr) r = lr;
        if (g < lg) g = lg;
        if (b < lb) b = lb;
      }
      for (int i = 0; i < surf->count; ++i) {
        vuint32 lt0 = RendLev->LightPoint(surf->verts[i], radius, surf->plane);
        int lr = (lt0>>16)&255;
        int lg = (lt0>>8)&255;
        int lb = lt0&255;
        if (r < lr) r = lr;
        if (g < lg) g = lg;
        if (b < lb) b = lb;
      }
      p_glUniform4fARB(SurfMasked_LightLoc, r/255.0f, g/255.0f, b/255.0f, Alpha);
    } else {
      const float lev = getSurfLightLevel(surf);
      p_glUniform4fARB(SurfMasked_LightLoc,
        ((surf->Light>>16)&255)*lev/255.0f,
        ((surf->Light>>8)&255)*lev/255.0f,
        (surf->Light&255)*lev/255.0f, Alpha);
    }
  }

  SurfMasked_Locs.storeFogFade(surf->Fade, Alpha);

  bool doDecals = (decalsAllowed && tex->Tex && !tex->noDecals && surf->dcseg && surf->dcseg->decals);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  glBegin(GL_POLYGON);
  for (int i = 0; i < surf->count; ++i) {
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
      (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
    glVertex(surf->verts[i]);
  }
  glEnd();

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      //p_glUseProgramObjectARB(SurfSimpleProgram);
      //return true;
    }
  }

  if (restoreBlend) {
    glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
  }
  if (Additive) {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpritePolygon
//
//==========================================================================
void VOpenGLDrawer::DrawSpritePolygon (const TVec *cv, VTexture *Tex,
                                       float Alpha, bool Additive,
                                       VTextureTranslation *Translation, int CMap,
                                       vuint32 light, vuint32 Fade,
                                       const TVec &sprnormal, float sprpdist,
                                       const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                       int hangup)
{
  if (!Tex) return; // just in case

  TVec texpt(0, 0, 0);

  SetSpriteLump(Tex, Translation, CMap, true);
  //SetupTextureFiltering(noDepthChange ? 3 : sprite_filter);
  //SetupTextureFiltering(noDepthChange ? model_filter : sprite_filter);
  SetupTextureFiltering(sprite_filter);

  p_glUseProgramObjectARB(SurfMasked_Program);
  p_glUniform1iARB(SurfMasked_TextureLoc, 0);
  SurfMasked_Locs.storeFogType();

  bool zbufferWriteDisabled = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (blend_sprites || Additive || hangup || Alpha < 1.0f) {
    restoreBlend = true;
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, (hangup || Additive ? getAlphaThreshold() : 0.666f));
    if (hangup) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);//*hangup;
      glPolygonOffset(updir, updir);
      glEnable(GL_POLYGON_OFFSET_FILL);
    }
    glEnable(GL_BLEND);
    // translucent things should not modify z-buffer
    if (!zbufferWriteDisabled && (Additive || Alpha < 1.0f)) {
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      zbufferWriteDisabled = true;
    }
    if (Additive) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, 0.666f);
    Alpha = 1.0f;
    glDisable(GL_BLEND);
  }

  //GCon->Logf("SPRITE: light=0x%08x; fade=0x%08x", light, Fade);
  //Fade ^= 0x00ffffff;
  //light = 0xffff0000;
  //Fade = 0x3f323232;
  /*
  if (Fade != FADE_LIGHT && RendLev->IsAdvancedRenderer()) {
    Fade ^= 0x00ffffff;
  }
  */

  p_glUniform4fARB(SurfMasked_LightLoc,
    ((light>>16)&255)/255.0f,
    ((light>>8)&255)/255.0f,
    (light&255)/255.0f, Alpha);

  SurfMasked_Locs.storeFogFade(Fade, Alpha);

  glBegin(GL_QUADS);
    texpt = cv[0]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[0]);

    texpt = cv[1]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[1]);

    texpt = cv[2]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[2]);

    texpt = cv[3]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[3]);
  glEnd();

  if (restoreBlend) {
    if (hangup) {
      glPolygonOffset(0.0f, 0.0f);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
    if (Additive) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::StartParticles
//
//==========================================================================
void VOpenGLDrawer::StartParticles () {
  glEnable(GL_BLEND);
  p_glUseProgramObjectARB(gl_smooth_particles ? SurfPartSm_Program : SurfPartSq_Program);
  glBegin(GL_QUADS);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawParticle
//
//==========================================================================
void VOpenGLDrawer::DrawParticle (particle_t *p) {
  GLint lvLoc, tcLoc;
  if (gl_smooth_particles) {
    lvLoc = SurfPartSm_LightValLoc;
    tcLoc = SurfPartSm_TexCoordLoc;
  } else {
    lvLoc = SurfPartSq_LightValLoc;
    tcLoc = SurfPartSq_TexCoordLoc;
  }
  const float r = ((p->colour>>16)&255)/255.0f;
  const float g = ((p->colour>>8)&255)/255.0f;
  const float b = (p->colour&255)/255.0f;
  const float a = ((p->colour>>24)&255)/255.0f;
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, -1, -1);
  glVertex(p->org-viewright*p->Size+viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, 1, -1);
  glVertex(p->org+viewright*p->Size+viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, 1, 1);
  glVertex(p->org+viewright*p->Size-viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, -1, 1);
  glVertex(p->org-viewright*p->Size-viewup*p->Size);
}


//==========================================================================
//
//  VOpenGLDrawer::EndParticles
//
//==========================================================================
void VOpenGLDrawer::EndParticles () {
  glEnd();
  glDisable(GL_BLEND);
}


//==========================================================================
//
//  VOpenGLDrawer::StartPortal
//
//==========================================================================
bool VOpenGLDrawer::StartPortal (VPortal *Portal, bool UseStencil) {
  if (UseStencil) {
    ClearStencilBuffer();
    NoteStencilBufferDirty();

    if (/*!Portal->stackedSector*/true) {
      // doesn't work for now
      // k8: why?
      if (RendLev->NeedsInfiniteFarClip) return false;

      // disable drawing
      p_glUseProgramObjectARB(SurfZBuf_Program);
      glDisable(GL_TEXTURE_2D);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glDepthMask(GL_FALSE); // no z-buffer writes

      // set up stencil test
      /*if (!RendLev->PortalDepth)*/ glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

      // mark the portal area
      DrawPortalArea(Portal);

      // set up stencil test for portal
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth+1, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

      if (Portal->NeedsDepthBuffer()) {
        glDepthMask(GL_TRUE); // allow z-buffer writes
        // clear depth buffer
        if (CanUseRevZ()) glDepthRange(0, 0); else glDepthRange(1, 1);
        glDepthFunc(GL_ALWAYS);
        DrawPortalArea(Portal);
        //glDepthFunc(GL_LEQUAL);
        RestoreDepthFunc();
        glDepthRange(0, 1);
      } else {
        glDepthMask(GL_FALSE); // no z-buffer writes
        glDisable(GL_DEPTH_TEST);
      }
    } else {
      glDisable(GL_STENCIL_TEST);
    }

    // enable drawing
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_TEXTURE_2D);
    ++RendLev->PortalDepth;
  } else {
    if (!Portal->NeedsDepthBuffer()) {
      glDepthMask(GL_FALSE); // no z-buffer writes
      glDisable(GL_DEPTH_TEST);
    }
  }
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPortalArea
//
//==========================================================================
void VOpenGLDrawer::DrawPortalArea (VPortal *Portal) {
  for (int i = 0; i < Portal->Surfs.Num(); ++i) {
    const surface_t *surf = Portal->Surfs[i];
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render portal surface with %d vertices", surf->count);
      continue;
    }
    glBegin(GL_POLYGON);
    for (unsigned j = 0; j < (unsigned)surf->count; ++j) glVertex(surf->verts[j]);
    glEnd();
  }
}


//==========================================================================
//
//  VSoftwareDrawer::EndPortal
//
//==========================================================================
void VOpenGLDrawer::EndPortal (VPortal *Portal, bool UseStencil) {
  p_glUseProgramObjectARB(SurfZBuf_Program);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDisable(GL_TEXTURE_2D);

  if (UseStencil) {
    if (/*!Portal->stackedSector*/true) {
      if (gl_dbg_render_stack_portal_bounds && Portal->stackedSector) {
        p_glUseProgramObjectARB(0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE); // no z-buffer writes
        glColor3f(1, 0, 0);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        DrawPortalArea(Portal);

        glEnable(GL_STENCIL_TEST);
        p_glUseProgramObjectARB(SurfZBuf_Program);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE); // allow z-buffer writes
      }

      if (Portal->NeedsDepthBuffer()) {
        // clear depth buffer
        if (CanUseRevZ()) glDepthRange(0, 0); else glDepthRange(1, 1);
        glDepthFunc(GL_ALWAYS);
        DrawPortalArea(Portal);
        //glDepthFunc(GL_LEQUAL);
        RestoreDepthFunc();
        glDepthRange(0, 1);
      } else {
        glDepthMask(GL_TRUE); // allow z-buffer writes
        glEnable(GL_DEPTH_TEST);
      }

      glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

      // draw proper z-buffer for the portal area
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();

      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
      NoteStencilBufferDirty(); // just in case
    }

    --RendLev->PortalDepth;
    if (RendLev->PortalDepth == 0) glDisable(GL_STENCIL_TEST);
  } else {
    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      glClear(GL_DEPTH_BUFFER_BIT);
    } else {
      glDepthMask(GL_TRUE); // allow z-buffer writes
      glEnable(GL_DEPTH_TEST);
    }

    // draw proper z-buffer for the portal area
    DrawPortalArea(Portal);
  }

  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}
