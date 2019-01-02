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

static VCvarB glsw_report_verts("glsw_report_verts", false, "Report number of shadow volume vertices?", 0);
static VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
static VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
static VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
static VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

static VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels)?", CVAR_Archive);

static VCvarB gl_dbg_adv_render_textures_surface("gl_dbg_adv_render_textures_surface", true, "Render surface textures in advanced renderer?", 0);


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
static int surfCmp (const void *a, const void *b, void *udata) {
  surface_t *sa = *(surface_t **)a;
  surface_t *sb = *(surface_t **)b;
  if (sa == sb) return 0;
  texinfo_t *ta = sa->texinfo;
  texinfo_t *tb = sb->texinfo;
  if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
  if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
  return ((int)ta->ColourMap)-((int)tb->ColourMap);
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
  slins = MAX(slins, r_ambient);
  if (slins > 255) slins = 255;
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
// VOpenGLDrawer::RenderShaderDecalsStart
//
//==========================================================================
void VOpenGLDrawer::RenderShaderDecalsStart () {
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  decalUsedStencil = false;
}


//==========================================================================
//
// VOpenGLDrawer::RenderShaderDecalsEnd
//
//==========================================================================
void VOpenGLDrawer::RenderShaderDecalsEnd () {
  if (decalUsedStencil) {
    // clear stencil buffer, as other stages may expect it cleared
    // this is not strictly necessary, but 'cmon, be polite!
    // later fix: 'cmon, it is not necessary at all!
    // later fix: it is necessary for stenciled portals; clear stencil buffer there
    glClear(GL_STENCIL_BUFFER_BIT);
  }
  //glDisable(GL_STENCIL_TEST);
}


//==========================================================================
//
// VOpenGLDrawer::RenderPrepareShaderDecals
//
//==========================================================================
void VOpenGLDrawer::RenderPrepareShaderDecals (surface_t *surf) {
  if (!r_decals_enabled) return;

  if (!surf->dcseg || !surf->dcseg->decals) return; // nothing to do

  if (gl_decal_debug_nostencil) return; // debug

  if (++decalStcVal == 0) { glClear(GL_STENCIL_BUFFER_BIT); decalStcVal = 1; } // it wrapped, so clear stencil buffer
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, /*GL_INCR*/GL_REPLACE);
  decalUsedStencil = true;
}


//==========================================================================
//
// VOpenGLDrawer::RenderFinishShaderDecals
//
//==========================================================================
bool VOpenGLDrawer::RenderFinishShaderDecals (surface_t *surf, bool lmap, bool advanced, surfcache_t *cache, int cmap) {
  if (!r_decals_enabled) return false;

  if (!surf->dcseg || !surf->dcseg->decals) return false; // nothing to do

  texinfo_t *tex = surf->texinfo;

  if (advanced) {
    p_glUseProgramObjectARB(SurfAdvDecalProgram);
    p_glUniform1iARB(SurfAdvDecalTextureLoc, 0);
  } else {
    if (lmap) {
      p_glUseProgramObjectARB(SurfDecalProgram);
      p_glUniform1iARB(SurfDecalTextureLoc, 0);
      p_glUniform1iARB(SurfDecalFogTypeLoc, r_fog&3);

      p_glUniform1iARB(SurfDecalLightMapLoc, 1);
      p_glUniform1iARB(SurfDecalSpecularMapLoc, 2);

      p_glUniform3fvARB(SurfDecalSAxisLoc, 1, &tex->saxis.x);
      p_glUniform1fARB(SurfDecalSOffsLoc, tex->soffs);
      p_glUniform3fvARB(SurfDecalTAxisLoc, 1, &tex->taxis.x);
      p_glUniform1fARB(SurfDecalTOffsLoc, tex->toffs);
      p_glUniform1fARB(SurfDecalTexMinSLoc, surf->texturemins[0]);
      p_glUniform1fARB(SurfDecalTexMinTLoc, surf->texturemins[1]);
      p_glUniform1fARB(SurfDecalCacheSLoc, cache->s);
      p_glUniform1fARB(SurfDecalCacheTLoc, cache->t);

      if (surf->Fade) {
        p_glUniform1iARB(SurfDecalFogEnabledLoc, GL_TRUE);
        p_glUniform4fARB(SurfDecalFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
        p_glUniform1fARB(SurfDecalFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
        p_glUniform1fARB(SurfDecalFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
        p_glUniform1fARB(SurfDecalFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0*r_fade_factor : r_fog_end));
      } else {
        p_glUniform1iARB(SurfDecalFogEnabledLoc, GL_FALSE);
      }
    } else {
      p_glUseProgramObjectARB(SurfDecalNoLMapProgram);
      p_glUniform1iARB(SurfDecalNoLMapTextureLoc, 0);
      p_glUniform1iARB(SurfDecalNoLMapFogTypeLoc, r_fog&3);

      if (surf->Fade) {
        p_glUniform1iARB(SurfDecalNoLMapFogEnabledLoc, GL_TRUE);
        p_glUniform4fARB(SurfDecalNoLMapFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
        p_glUniform1fARB(SurfDecalNoLMapFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
        p_glUniform1fARB(SurfDecalNoLMapFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
        p_glUniform1fARB(SurfDecalNoLMapFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0*r_fade_factor : r_fog_end));
      } else {
        p_glUniform1iARB(SurfDecalNoLMapFogEnabledLoc, GL_FALSE);
      }
    }
  }

  glDepthMask(GL_FALSE);
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST); // just in case

  //glEnable(GL_DEPTH_TEST);
  //glDisable(GL_BLEND);

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

    if (advanced) {
      p_glUniform1fARB(SurfAdvDecalSplatAlphaLoc, dc->alpha);
      if (!tex1set) {
        tex1set = true;
        p_glActiveTextureARB(GL_TEXTURE0+1);
        glBindTexture(GL_TEXTURE_2D, ambLightFBOColorTid);
        p_glActiveTextureARB(GL_TEXTURE0);
      }
      //p_glUniform4fARB(SurfAdvDecalLightLoc, 0, 0, 0, (dc->flags&decal_t::Fullbright ? 1.0f : 0.0f));
      p_glUniform4fARB(SurfAdvDecalFullBright, 0, 0, 0, (dc->flags&decal_t::Fullbright ? 1.0f : 0.0f));
      p_glUniform1iARB(SurfAdvDecalAmbLightTextureLoc, 1);
      p_glUniform2fARB(SurfAdvDecalScreenSize, (float)ScreenWidth, (float)ScreenHeight);
    } else {
      const float lev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));
      if (lmap) {
        p_glUniform4fARB(SurfDecalLightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
        p_glUniform1fARB(SurfDecalSplatAlphaLoc, dc->alpha);
      } else {
        p_glUniform4fARB(SurfDecalNoLMapLightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
        p_glUniform1fARB(SurfDecalNoLMapSplatAlphaLoc, dc->alpha);
      }
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
  if (!advanced) glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_TRUE);

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

  //return false;

  if (textureChanged) {
    SetTexture(textr->Tex, textr->ColourMap);
    ++glWDTextureChangesTotal;
  }

  p_glUniform3fvARB(SurfSimpleSAxisLoc, 1, &textr->saxis.x);
  p_glUniform1fARB(SurfSimpleSOffsLoc, textr->soffs);
  p_glUniform1fARB(SurfSimpleTexIWLoc, tex_iw);
  p_glUniform3fvARB(SurfSimpleTAxisLoc, 1, &textr->taxis.x);
  p_glUniform1fARB(SurfSimpleTOffsLoc, textr->toffs);
  p_glUniform1fARB(SurfSimpleTexIHLoc, tex_ih);

  const float lev = getSurfLightLevel(surf);
  p_glUniform4fARB(SurfSimpleLightLoc, ((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
  if (surf->Fade) {
    p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_TRUE);
    p_glUniform4fARB(SurfSimpleFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
    p_glUniform1fARB(SurfSimpleFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3f : r_fog_density));
    p_glUniform1fARB(SurfSimpleFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0f : r_fog_start));
    p_glUniform1fARB(SurfSimpleFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0f * r_fade_factor : r_fog_end));
  } else {
    p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_FALSE);
  }

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
    if (RenderFinishShaderDecals(surf, false, false, nullptr, textr->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      p_glUseProgramObjectARB(SurfSimpleProgram);
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderLMapSurface
//
//==========================================================================
bool VOpenGLDrawer::RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache) {
  texinfo_t *tex = surf->texinfo;

  if (textureChanged) {
    SetTexture(tex->Tex, tex->ColourMap);
    ++glWDTextureChangesTotal;
  }

  p_glUniform3fvARB(SurfLightmapSAxisLoc, 1, &tex->saxis.x);
  p_glUniform1fARB(SurfLightmapSOffsLoc, tex->soffs);
  p_glUniform1fARB(SurfLightmapTexIWLoc, tex_iw);
  p_glUniform3fvARB(SurfLightmapTAxisLoc, 1, &tex->taxis.x);
  p_glUniform1fARB(SurfLightmapTOffsLoc, tex->toffs);
  p_glUniform1fARB(SurfLightmapTexIHLoc, tex_ih);
  p_glUniform1fARB(SurfLightmapTexMinSLoc, surf->texturemins[0]);
  p_glUniform1fARB(SurfLightmapTexMinTLoc, surf->texturemins[1]);
  p_glUniform1fARB(SurfLightmapCacheSLoc, cache->s);
  p_glUniform1fARB(SurfLightmapCacheTLoc, cache->t);

  if (surf->Fade) {
    p_glUniform1iARB(SurfLightmapFogEnabledLoc, GL_TRUE);
    p_glUniform4fARB(SurfLightmapFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
    p_glUniform1fARB(SurfLightmapFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
    p_glUniform1fARB(SurfLightmapFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
    p_glUniform1fARB(SurfLightmapFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end));
  } else {
    p_glUniform1iARB(SurfLightmapFogEnabledLoc, GL_FALSE);
  }

  bool doDecals = tex->Tex && !tex->noDecals && surf->dcseg && surf->dcseg->decals;

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
    if (RenderFinishShaderDecals(surf, true, false, cache, tex->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      p_glUseProgramObjectARB(SurfLightmapProgram);
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
  guard(VOpenGLDrawer::WorldDrawing);
  surfcache_t *cache;
  surface_t *surf;
  texinfo_t *prevTR;

  // first draw horizons
  if (RendLev->HorizonPortalsHead) {
    for (surf = RendLev->HorizonPortalsHead; surf; surf = surf->DrawNext) {
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // for sky areas we just write to the depth buffer to prevent drawing polygons behind the sky
  if (RendLev->SkyPortalsHead) {
    p_glUseProgramObjectARB(SurfZBufProgram);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    for (surf = RendLev->SkyPortalsHead; surf; surf = surf->DrawNext) {
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      glBegin(GL_POLYGON);
      for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  RenderShaderDecalsStart();

  // draw surfaces without lightmaps
  if (RendLev->SimpleSurfsHead) {
    p_glUseProgramObjectARB(SurfSimpleProgram);
    p_glUniform1iARB(SurfSimpleTextureLoc, 0);
    p_glUniform1iARB(SurfSimpleFogTypeLoc, r_fog&3);

    if (!gl_sort_textures) {
      for (surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
        if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
        RenderSimpleSurface(true, surf);
      }
    } else {
      surfListClear();
      for (surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
        if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
        surfListAppend(surf);
      }
      if (surfListUsed > 0) {
        timsort_r(surfList, surfListUsed, sizeof(surfList[0]), &surfCmp, nullptr);
        prevTR = nullptr;
        for (vuint32 sidx = 0; sidx < surfListUsed; ++sidx) {
          surf = surfList[sidx].surf;
          bool texChanded = false;
          texinfo_t *textr = surf->texinfo;
          if (!prevTR || prevTR->Tex != textr->Tex || prevTR->ColourMap != textr->ColourMap) {
            prevTR = textr;
            texChanded = true;
          }
          if (RenderSimpleSurface(texChanded, surf)) prevTR = nullptr;
        }
      }
    }
  }

  p_glUseProgramObjectARB(SurfLightmapProgram);
  p_glUniform1iARB(SurfLightmapTextureLoc, 0);
  p_glUniform1iARB(SurfLightmapLightMapLoc, 1);
  p_glUniform1iARB(SurfLightmapSpecularMapLoc, 2);
  p_glUniform1iARB(SurfLightmapFogTypeLoc, r_fog&3);

  // draw surfaces with lightmaps
  prevTR = nullptr;
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
      for (cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
        surf = cache->surf;
        if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
        RenderLMapSurface(true, surf, cache);
      }
    } else {
      surfListClear();
      for (cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
        surf = cache->surf;
        if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
        surfListAppend(surf, cache);
      }
      if (surfListUsed > 0) {
        timsort_r(surfList, surfListUsed, sizeof(surfList[0]), &surfCmp, nullptr);
        for (vuint32 sidx = 0; sidx < surfListUsed; ++sidx) {
          surf = surfList[sidx].surf;
          bool texChanded = false;
          texinfo_t *textr = surf->texinfo;
          if (!prevTR || prevTR->Tex != textr->Tex || prevTR->ColourMap != textr->ColourMap) {
            prevTR = textr;
            texChanded = true;
          }
          if (RenderLMapSurface(texChanded, surf, surfList[sidx].cache)) prevTR = nullptr;
        }
      }
    }
  }

  RenderShaderDecalsEnd();

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldAmbientPass
//
//==========================================================================
void VOpenGLDrawer::DrawWorldAmbientPass () {
  guard(VOpenGLDrawer::DrawWorldAmbientPass);

  // first draw horizons
  if (RendLev->HorizonPortalsHead) {
    for (surface_t *surf = RendLev->HorizonPortalsHead; surf; surf = surf->DrawNext) {
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  if (RendLev->SkyPortalsHead) {
    p_glUseProgramObjectARB(SurfZBufProgram);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    for (surface_t *surf = RendLev->SkyPortalsHead; surf; surf = surf->DrawNext) {
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      glBegin(GL_POLYGON);
      for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  p_glUseProgramObjectARB(ShadowsAmbientProgram);
  p_glUniform1iARB(ShadowsAmbientTextureLoc, 0);
  for (surface_t *surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

    texinfo_t *tex = surf->texinfo;
    SetTexture(tex->Tex, tex->ColourMap);
    p_glUniform3fvARB(ShadowsAmbientSAxisLoc, 1, &tex->saxis.x);
    p_glUniform1fARB(ShadowsAmbientSOffsLoc, tex->soffs);
    p_glUniform1fARB(ShadowsAmbientTexIWLoc, tex_iw);
    p_glUniform3fvARB(ShadowsAmbientTAxisLoc, 1, &tex->taxis.x);
    p_glUniform1fARB(ShadowsAmbientTOffsLoc, tex->toffs);
    p_glUniform1fARB(ShadowsAmbientTexIHLoc, tex_ih);

    const float lev = getSurfLightLevel(surf);
    p_glUniform4fARB(ShadowsAmbientLightLoc,
      ((surf->Light>>16)&255)*lev/255.0f,
      ((surf->Light>>8)&255)*lev/255.0f,
      (surf->Light&255)*lev/255.0f, 1.0f);

    glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
  }

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::BeginShadowVolumesPass
//
//==========================================================================
void VOpenGLDrawer::BeginShadowVolumesPass () {
  guard(VOpenGLDrawer::BeginShadowVolumesPass);
  // set up for shadow volume rendering
  glEnable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//==========================================================================
static int swcount = 0;

void VOpenGLDrawer::BeginLightShadowVolumes () {
  guard(VOpenGLDrawer::BeginLightShadowVolumes);
  // set up for shadow volume rendering
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);

  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);

  if (!CanUseRevZ()) {
    // normal
    //glPolygonOffset(1.0f, 10.0f); //k8: this seems to be unnecessary
    glDepthFunc(GL_LESS);
    //glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    //glPolygonOffset(-1.0f, -10.0f); //k8: this seems to be unnecessary
    glDepthFunc(GL_GREATER);
    //glDepthFunc(GL_GEQUAL);
  }
  p_glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
  p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

  p_glUseProgramObjectARB(SurfZBufProgram);
  swcount = 0;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  guard(VOpenGLDrawer::EndLightShadowVolumes);
  if (glsw_report_verts) GCon->Logf("swcount=%d", swcount);
  RestoreDepthFunc();
  glPolygonOffset(0.0f, 0.0f);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowVolume
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolume (surface_t *surf, TVec &LightPos, float Radius, bool LightCanCross) {
  guard(VOpenGLDrawer::RenderSurfaceShadowVolume);
  if (surf->count < 1) return; // just in case
  if (surf->plane->PointOnSide(vieworg) && LightCanCross) return; // viewer is in back side or on plane
  float dist = DotProduct(LightPos, surf->plane->normal)-surf->plane->dist;
  if ((dist <= 0.0 && !LightCanCross) || dist < -Radius || dist > Radius) return; // light is too far away

  static TVec *poolVec = nullptr;
  static int poolVecSize = 0;

  //TArray<TVec> v;
  //v.SetNum(surf->count);

  if (poolVecSize < surf->count) {
    poolVecSize = (surf->count|0xfff)+1;
    poolVec = (TVec *)Z_Realloc(poolVec, poolVecSize*sizeof(TVec));
  }

  TVec *v = poolVec;

  swcount += surf->count*4;

  for (int i = 0; i < surf->count; ++i) {
    v[i] = Normalise(surf->verts[i]-LightPos);
    v[i] *= M_INFINITY;
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

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (TVec &LightPos, float Radius, vuint32 Colour) {
  guard(VOpenGLDrawer::BeginLightPass);
  //glDepthFunc(GL_LEQUAL);
  RestoreDepthFunc();

  glDisable(GL_POLYGON_OFFSET_FILL);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glStencilFunc(GL_EQUAL, 0x0, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);

  p_glUseProgramObjectARB(ShadowsLightProgram);
  p_glUniform3fARB(ShadowsLightLightPosLoc, LightPos.x, LightPos.y, LightPos.z);
  p_glUniform1fARB(ShadowsLightLightRadiusLoc, Radius);
  p_glUniform3fARB(ShadowsLightLightColourLoc,
    ((Colour>>16)&255)/255.0,
    ((Colour>>8)&255)/255.0,
    (Colour&255)/255.0);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSurfaceLight
//
//  this blends surfaces from light sources to ambient map.
//
//==========================================================================
void VOpenGLDrawer::DrawSurfaceLight (surface_t *Surf, TVec &LightPos, float Radius, bool LightCanCross) {
  guard(VOpenGLDrawer::DrawSurfaceLight);

  if (Surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  float dist = DotProduct(LightPos, Surf->plane->normal) - Surf->plane->dist;
  if ((dist <= 0.0 && !LightCanCross) || dist < -Radius || dist > Radius) return; // light is too far away

  p_glUniform1iARB(ShadowsAmbientTextureLoc, 0);
  texinfo_t *tex = Surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);
  p_glUniform3fvARB(ShadowsAmbientSAxisLoc, 1, &tex->saxis.x);
  p_glUniform1fARB(ShadowsAmbientSOffsLoc, tex->soffs);
  p_glUniform1fARB(ShadowsAmbientTexIWLoc, tex_iw);
  p_glUniform3fvARB(ShadowsAmbientTAxisLoc, 1, &tex->taxis.x);
  p_glUniform1fARB(ShadowsAmbientTOffsLoc, tex->toffs);
  p_glUniform1fARB(ShadowsAmbientTexIHLoc, tex_ih);
  p_glVertexAttrib3fvARB(ShadowsLightSurfNormalLoc, &Surf->plane->normal.x);
  p_glVertexAttrib1fvARB(ShadowsLightSurfDistLoc, &Surf->plane->dist);
  p_glUniform3fARB(ShadowsLightViewOrigin, vieworg.x, vieworg.y, vieworg.z);

  glBegin(GL_POLYGON);
  for (int i = 0; i < Surf->count; ++i) glVertex(Surf->verts[i]);
  glEnd();

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldTexturesPass
//
//  This renders textured level with ambient (aka sector) lighting
//
//==========================================================================
void VOpenGLDrawer::DrawWorldTexturesPass () {
  guard(VOpenGLDrawer::DrawWorldTexturesPass);
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);

  // copy ambient light texture to FBO, so we can use it to light decals
  /*if (1)*/ {
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, ambLightFBO);
    glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(1.0, 1.0, 1.0, 1.0);
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

  if (!gl_dbg_adv_render_textures_surface) return;

  p_glUseProgramObjectARB(ShadowsTextureProgram);
  p_glUniform1iARB(ShadowsTextureTextureLoc, 0);

  RenderShaderDecalsStart();

  for (surface_t *surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    // this is for advanced renderer only
    texinfo_t *tex = surf->texinfo;
    SetTexture(tex->Tex, tex->ColourMap);

    bool doDecals = tex->Tex && !tex->noDecals && surf->dcseg && surf->dcseg->decals;

    // fill stencil buffer for decals
    if (doDecals) RenderPrepareShaderDecals(surf);

    glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) {
      p_glVertexAttrib2fARB(ShadowsTextureTexCoordLoc,
        (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();

    if (doDecals) {
      if (RenderFinishShaderDecals(surf, false, true, nullptr, tex->ColourMap)) {
        p_glUseProgramObjectARB(ShadowsTextureProgram);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        glEnable(GL_BLEND);
      }
    }
  }

  RenderShaderDecalsEnd();

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldFogPass
//
//==========================================================================
void VOpenGLDrawer::DrawWorldFogPass () {
  guard(VOpenGLDrawer::DrawWorldFogPass);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // draw surfaces
  p_glUseProgramObjectARB(ShadowsFogProgram);
  p_glUniform1iARB(ShadowsFogFogTypeLoc, r_fog&3);

  for (surface_t *surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
    if (!surf->Fade) continue;
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

    p_glUniform4fARB(ShadowsFogFogColourLoc,
      ((surf->Fade >> 16) & 255) / 255.0,
      ((surf->Fade >> 8) & 255) / 255.0,
      (surf->Fade & 255) / 255.0, 1.0);
    p_glUniform1fARB(ShadowsFogFogDensityLoc, surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density);
    p_glUniform1fARB(ShadowsFogFogStartLoc, surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start);
    p_glUniform1fARB(ShadowsFogFogEndLoc, surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);

    glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
  }
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  guard(VOpenGLDrawer::EndFogPass);
  glDisable(GL_BLEND);

  // back to normal z-buffering
  glDepthMask(GL_TRUE);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DoHorizonPolygon
//
//==========================================================================
void VOpenGLDrawer::DoHorizonPolygon (surface_t *Surf) {
  guard(VOpenGLDrawer::DoHorizonPolygon);
  float Dist = 4096.0f;
  TVec v[4];
  if (Surf->HorizonPlane->normal.z > 0.0f) {
    v[0] = Surf->verts[0];
    v[3] = Surf->verts[3];
    TVec HDir = -Surf->plane->normal;

    TVec Dir1 = Normalise(vieworg - Surf->verts[1]);
    TVec Dir2 = Normalise(vieworg - Surf->verts[2]);
    float Mul1 = 1.0f/DotProduct(HDir, Dir1);
    v[1] = Surf->verts[1]+Dir1*Mul1*Dist;
    float Mul2 = 1.0f/DotProduct(HDir, Dir2);
    v[2] = Surf->verts[2]+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[1] = Surf->verts[1]+Dir1*Mul1*Dist*(Surf->verts[1].z-Surf->verts[0].z)/(Surf->verts[1].z-v[1].z);
      v[2] = Surf->verts[2]+Dir2*Mul2*Dist*(Surf->verts[2].z-Surf->verts[3].z)/(Surf->verts[2].z-v[2].z);
    }
  } else {
    v[1] = Surf->verts[1];
    v[2] = Surf->verts[2];
    TVec HDir = -Surf->plane->normal;

    TVec Dir1 = Normalise(vieworg-Surf->verts[0]);
    TVec Dir2 = Normalise(vieworg-Surf->verts[3]);
    float Mul1 = 1.0f/DotProduct(HDir, Dir1);
    v[0] = Surf->verts[0]+Dir1*Mul1*Dist;
    float Mul2 = 1.0f/DotProduct(HDir, Dir2);
    v[3] = Surf->verts[3]+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[0] = Surf->verts[0]+Dir1*Mul1*Dist*(Surf->verts[1].z-Surf->verts[0].z)/(v[0].z-Surf->verts[0].z);
      v[3] = Surf->verts[3]+Dir2*Mul2*Dist*(Surf->verts[2].z-Surf->verts[3].z)/(v[3].z-Surf->verts[3].z);
    }
  }

  texinfo_t *Tex = Surf->texinfo;
  SetTexture(Tex->Tex, Tex->ColourMap);

  p_glUseProgramObjectARB(SurfSimpleProgram);
  p_glUniform1iARB(SurfSimpleTextureLoc, 0);
  p_glUniform1iARB(SurfSimpleFogTypeLoc, r_fog & 3);

  p_glUniform3fvARB(SurfSimpleSAxisLoc, 1, &Tex->saxis.x);
  p_glUniform1fARB(SurfSimpleSOffsLoc, Tex->soffs);
  p_glUniform1fARB(SurfSimpleTexIWLoc, tex_iw);
  p_glUniform3fvARB(SurfSimpleTAxisLoc, 1, &Tex->taxis.x);
  p_glUniform1fARB(SurfSimpleTOffsLoc, Tex->toffs);
  p_glUniform1fARB(SurfSimpleTexIHLoc, tex_ih);

  const float lev = getSurfLightLevel(Surf);
  p_glUniform4fARB(SurfSimpleLightLoc, ((Surf->Light>>16)&255)*lev/255.0f, ((Surf->Light>>8)&255)*lev/255.0f, (Surf->Light&255)*lev/255.0f, 1.0f);
  if (Surf->Fade) {
    p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_TRUE);
    p_glUniform4fARB(SurfSimpleFogColourLoc, ((Surf->Fade>>16)&255)/255.0, ((Surf->Fade>>8)&255)/255.0, (Surf->Fade&255)/255.0, 1.0);
    p_glUniform1fARB(SurfSimpleFogDensityLoc, Surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density);
    p_glUniform1fARB(SurfSimpleFogStartLoc, Surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start);
    p_glUniform1fARB(SurfSimpleFogEndLoc, Surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);
  } else {
    p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_FALSE);
  }

  // draw it
  glDepthMask(GL_FALSE);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 4; ++i) glVertex(v[i]);
  glEnd();
  glDepthMask(GL_TRUE);

  // write to the depth buffer
  p_glUseProgramObjectARB(SurfZBufProgram);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glBegin(GL_POLYGON);
  for (int i = 0; i < Surf->count; ++i) glVertex(Surf->verts[i]);
  glEnd();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSkyPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                                    float offs1, VTexture *Texture2, float offs2, int CMap)
{
  guard(VOpenGLDrawer::DrawSkyPolygon);
  int sidx[4];

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
  texinfo_t *tex = surf->texinfo;

  if (Texture2->Type != TEXTYPE_Null) {
    SetTexture(Texture1, CMap);
    SelectTexture(1);
    SetTexture(Texture2, CMap);
    SelectTexture(0);

    p_glUseProgramObjectARB(SurfDSkyProgram);
    p_glUniform1iARB(SurfDSkyTextureLoc, 0);
    p_glUniform1iARB(SurfDSkyTexture2Loc, 1);
    p_glUniform1fARB(SurfDSkyBrightnessLoc, r_sky_bright_factor);

    glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) {
      p_glVertexAttrib2fARB(SurfDSkyTexCoordLoc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
        (DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
      p_glVertexAttrib2fARB(SurfDSkyTexCoord2Loc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs2) * tex_iw,
        (DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  } else {
    SetTexture(Texture1, CMap);

    p_glUseProgramObjectARB(SurfSkyProgram);
    p_glUniform1iARB(SurfSkyTextureLoc, 0);
    p_glUniform1fARB(SurfSkyBrightnessLoc, r_sky_bright_factor);

    glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) {
      p_glVertexAttrib2fARB(SurfSkyTexCoordLoc,
        (DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
        (DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  }

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) {
  guard(VOpenGLDrawer::DrawMaskedPolygon);
  if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane

  texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  p_glUseProgramObjectARB(SurfMaskedProgram);
  p_glUniform1iARB(SurfMaskedTextureLoc, 0);
  p_glUniform1iARB(SurfMaskedFogTypeLoc, r_fog&3);

  if (blend_sprites || Additive || Alpha < 1.0) {
    p_glUniform1fARB(SurfMaskedAlphaRefLoc, getAlphaThreshold());
    glEnable(GL_BLEND);
    if (Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  } else {
    p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.555);
    Alpha = 1.0f;
  }

  if (surf->lightmap != nullptr || surf->dlightframe == r_dlightframecount) {
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
    double iscale = 1.0/(size*255*256);
    p_glUniform4fARB(SurfMaskedLightLoc, r*iscale, g*iscale, b*iscale, Alpha);
  } else {
    const float lev = getSurfLightLevel(surf);
    p_glUniform4fARB(SurfMaskedLightLoc,
      ((surf->Light>>16)&255)*lev/255.0f,
      ((surf->Light>>8)&255)*lev/255.0f,
      (surf->Light&255)*lev/255.0f, Alpha);
  }
  if (surf->Fade) {
    p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_TRUE);
    p_glUniform4fARB(SurfMaskedFogColourLoc,
      ((surf->Fade>>16)&255)/255.0f,
      ((surf->Fade>>8)&255)/255.0f,
      (surf->Fade&255)/255.0f, Alpha);
    p_glUniform1fARB(SurfMaskedFogDensityLoc, surf->Fade == FADE_LIGHT ? 0.3f : r_fog_density);
    p_glUniform1fARB(SurfMaskedFogStartLoc, surf->Fade == FADE_LIGHT ? 1.0f : r_fog_start);
    p_glUniform1fARB(SurfMaskedFogEndLoc, surf->Fade == FADE_LIGHT ? 1024.0f * r_fade_factor : r_fog_end);
  } else {
    p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_FALSE);
  }


  glBegin(GL_POLYGON);
  for (int i = 0; i < surf->count; ++i) {
    p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
      (DotProduct(surf->verts[i], tex->saxis) + tex->soffs) * tex_iw,
      (DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
    glVertex(surf->verts[i]);
  }
  glEnd();

  if (blend_sprites || Additive || Alpha < 1.0) glDisable(GL_BLEND);
  if (Additive) {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  unguard;
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
                                       const TVec &saxis, const TVec &taxis, const TVec &texorg)
{
  guard(VOpenGLDrawer::DrawSpritePolygon);
  if (!Tex) return; // just in case

  TVec texpt(0, 0, 0);

  SetSpriteLump(Tex, Translation, CMap, true);
  SetupTextureFiltering(sprite_filter);

  p_glUseProgramObjectARB(SurfMaskedProgram);
  p_glUniform1iARB(SurfMaskedTextureLoc, 0);
  p_glUniform1iARB(SurfMaskedFogTypeLoc, r_fog&3);

  if (blend_sprites || Additive || Alpha < 1.0f) {
    p_glUniform1fARB(SurfMaskedAlphaRefLoc, getAlphaThreshold());
    glEnable(GL_BLEND);
    if (Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  } else {
    p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.555f);
    Alpha = 1.0f;
  }

  p_glUniform4fARB(SurfMaskedLightLoc,
    ((light >> 16) & 255) / 255.0,
    ((light >> 8) & 255) / 255.0,
    (light & 255) / 255.0, Alpha);
  if (Fade) {
    p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_TRUE);
    p_glUniform4fARB(SurfMaskedFogColourLoc,
      ((Fade >> 16) & 255) / 255.0,
      ((Fade >> 8) & 255) / 255.0,
      (Fade & 255) / 255.0, Alpha);
    p_glUniform1fARB(SurfMaskedFogDensityLoc, Fade == FADE_LIGHT ? 0.3 : r_fog_density);
    p_glUniform1fARB(SurfMaskedFogStartLoc, Fade == FADE_LIGHT ? 1.0 : r_fog_start);
    p_glUniform1fARB(SurfMaskedFogEndLoc, Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);
  } else {
    p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_FALSE);
  }

#if 0
  if (Alpha >= 1.0f) {
    /*
    GLint odf = GL_LEQUAL;
    glGetIntegerv(GL_DEPTH_FUNC, &odf);
    glDepthFunc(!CanUseRevZ() ? GL_LESS : GL_GREATER);
    //glDepthFunc(GL_ALWAYS);
    */
    int nn = Tex->Name.GetIndex()%400;
    float ofs = ((float)nn)/400.0f;
    glPolygonOffset(/*0.75f*/-ofs, -4);
    glEnable(GL_POLYGON_OFFSET_FILL);
  }
#endif

  glBegin(GL_QUADS);

  texpt = cv[0]-texorg;
  p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
    DotProduct(texpt, saxis)*tex_iw,
    DotProduct(texpt, taxis)*tex_ih);
  glVertex(cv[0]);

  texpt = cv[1]-texorg;
  p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
    DotProduct(texpt, saxis)*tex_iw,
    DotProduct(texpt, taxis)*tex_ih);
  glVertex(cv[1]);

  texpt = cv[2]-texorg;
  p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
    DotProduct(texpt, saxis)*tex_iw,
    DotProduct(texpt, taxis)*tex_ih);
  glVertex(cv[2]);

  texpt = cv[3]-texorg;
  p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
    DotProduct(texpt, saxis)*tex_iw,
    DotProduct(texpt, taxis)*tex_ih);
  glVertex(cv[3]);

  glEnd();

#if 0
  if (Alpha >= 1.0f) {
    //glDepthFunc(odf);
    glPolygonOffset(0, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
#endif

  if (blend_sprites || Additive || Alpha < 1.0) glDisable(GL_BLEND);
  if (Additive) {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::StartParticles
//
//==========================================================================
void VOpenGLDrawer::StartParticles () {
  guard(VOpenGLDrawer::StartParticles);
  glEnable(GL_BLEND);
  p_glUseProgramObjectARB(SurfPartProgram);
  p_glUniform1iARB(SurfPartSmoothParticleLoc, gl_smooth_particles);
  glBegin(GL_QUADS);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawParticle
//
//==========================================================================
void VOpenGLDrawer::DrawParticle (particle_t *p) {
  guard(VOpenGLDrawer::DrawParticle);
  float r = ((p->colour >> 16) & 255) / 255.0;
  float g = ((p->colour >> 8) & 255) / 255.0;
  float b = (p->colour & 255) / 255.0;
  float a = ((p->colour >> 24) & 255) / 255.0;
  p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
  p_glVertexAttrib2fARB(SurfPartTexCoordLoc, -1, -1);
  glVertex(p->org - viewright * p->Size + viewup * p->Size);
  p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
  p_glVertexAttrib2fARB(SurfPartTexCoordLoc, 1, -1);
  glVertex(p->org + viewright * p->Size + viewup * p->Size);
  p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
  p_glVertexAttrib2fARB(SurfPartTexCoordLoc, 1, 1);
  glVertex(p->org + viewright * p->Size - viewup * p->Size);
  p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
  p_glVertexAttrib2fARB(SurfPartTexCoordLoc, -1, 1);
  glVertex(p->org - viewright * p->Size - viewup * p->Size);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndParticles
//
//==========================================================================
void VOpenGLDrawer::EndParticles () {
  guard(VOpenGLDrawer::EndParticles);
  glEnd();
  glDisable(GL_BLEND);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::StartPortal
//
//==========================================================================
bool VOpenGLDrawer::StartPortal (VPortal *Portal, bool UseStencil) {
  guard(VOpenGLDrawer::StartPortal);
  if (UseStencil) {
    // doesn't work for now
    if (RendLev->NeedsInfiniteFarClip) return false;

    // disable drawing
    p_glUseProgramObjectARB(SurfZBufProgram);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    // set up stencil test
    if (!RendLev->PortalDepth) glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    // mark the portal area
    DrawPortalArea(Portal);

    // set up stencil test for portal
    glStencilFunc(GL_EQUAL, RendLev->PortalDepth+1, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    if (Portal->NeedsDepthBuffer()) {
      glDepthMask(GL_TRUE);
      // clear depth buffer
      glDepthRange(1, 1);
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();
      glDepthRange(0, 1);
    } else {
      glDepthMask(GL_FALSE);
      glDisable(GL_DEPTH_TEST);
    }

    // enable drawing
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    ++RendLev->PortalDepth;
  } else {
    if (!Portal->NeedsDepthBuffer()) {
      glDepthMask(GL_FALSE);
      glDisable(GL_DEPTH_TEST);
    }
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPortalArea
//
//==========================================================================
void VOpenGLDrawer::DrawPortalArea (VPortal *Portal) {
  guard(VOpenGLDrawer::DrawPortalArea);
  for (int i = 0; i < Portal->Surfs.Num(); ++i) {
    const surface_t *Surf = Portal->Surfs[i];
    glBegin(GL_POLYGON);
    for (int j = 0; j < Surf->count; ++j) glVertex(Surf->verts[j]);
    glEnd();
  }
  unguard;
}


//==========================================================================
//
//  VSoftwareDrawer::EndPortal
//
//==========================================================================
void VOpenGLDrawer::EndPortal (VPortal *Portal, bool UseStencil) {
  guard(VOpenGLDrawer::EndPortal);

  p_glUseProgramObjectARB(SurfZBufProgram);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  if (UseStencil) {
    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      glDepthRange(1, 1);
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();
      glDepthRange(0, 1);
    } else {
      glDepthMask(GL_TRUE);
      glEnable(GL_DEPTH_TEST);
    }

    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

    // draw proper z-buffer for the portal area
    glDepthFunc(GL_ALWAYS);
    DrawPortalArea(Portal);
    //glDepthFunc(GL_LEQUAL);
    RestoreDepthFunc();

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    --RendLev->PortalDepth;
    glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
    if (!RendLev->PortalDepth) glDisable(GL_STENCIL_TEST);
  } else {
    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      glClear(GL_DEPTH_BUFFER_BIT);
    } else {
      glDepthMask(GL_TRUE);
      glEnable(GL_DEPTH_TEST);
    }

    // draw proper z-buffer for the portal area
    DrawPortalArea(Portal);
  }

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  unguard;
}
