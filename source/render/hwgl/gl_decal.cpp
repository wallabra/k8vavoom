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


//==========================================================================
//
//  VOpenGLDrawer::RenderPrepareShaderDecals
//
//==========================================================================
void VOpenGLDrawer::RenderPrepareShaderDecals (surface_t *surf) {
  if (!r_decals_enabled) return;
  if (RendLev->PortalDepth) return; //FIXME: not yet

  if (!surf->seg || !surf->seg->decals) return; // nothing to do

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

  if (!surf->seg || !surf->seg->decals) return false; // nothing to do

  texinfo_t *tex = surf->texinfo;

  // setup shader
  switch (dtype) {
    case DT_SIMPLE:
      SurfDecalNoLMap.Activate();
      SurfDecalNoLMap.SetTexture(0);
      //SurfDecalNoLMap_Locs.storeFogType();
      SurfDecalNoLMap.SetFogFade(surf->Fade, 1.0f);
      break;
    case DT_LIGHTMAP:
      SurfDecalLMap.Activate();
      SurfDecalLMap.SetTexture(0);
      //SurfDecalLMap_Locs.storeFogType();
      SurfDecalLMap.SetFogFade(surf->Fade, 1.0f);
      SurfDecalLMap.SetLightMap(1);
      SurfDecalLMap.SetSpecularMap(2);
      SurfDecalLMap.SetLMapOnly(tex, surf, cache);
      break;
    case DT_ADVANCED:
      SurfAdvDecal.Activate();
      SurfAdvDecal.SetTexture(0);
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
  decal_t *dc = surf->seg->decals;

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
        if (prev) prev->next = n; else surf->seg->decals = n;
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
        if (prev) prev->next = n; else surf->seg->decals = n;
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
        if (prev) prev->next = n; else surf->seg->decals = n;
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
          SurfDecalNoLMap.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
          SurfDecalNoLMap.SetSplatAlpha(dc->alpha);
        }
        break;
      case DT_LIGHTMAP:
        {
          //!const float lev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));
          //!p_glUniform4fARB(SurfDecalLMap_LightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
          SurfDecalLMap.SetSplatAlpha(dc->alpha);
        }
        break;
      case DT_ADVANCED:
        {
          SurfAdvDecal.SetSplatAlpha(dc->alpha);
          if (!tex1set) {
            tex1set = true;
            p_glActiveTextureARB(GL_TEXTURE0+1);
            glBindTexture(GL_TEXTURE_2D, ambLightFBOColorTid);
            p_glActiveTextureARB(GL_TEXTURE0);
          }
          SurfAdvDecal.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
          SurfAdvDecal.SetAmbLightTexture(1);
          SurfAdvDecal.SetScreenSize((float)ScreenWidth, (float)ScreenHeight);
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
