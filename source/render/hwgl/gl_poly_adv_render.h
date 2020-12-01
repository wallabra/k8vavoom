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
#ifndef VAVOOM_GL_ADVRENDER_H
#define VAVOOM_GL_ADVRENDER_H

extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;

extern VCvarB gl_dbg_vbo_adv_ambient;

extern VCvarB gl_smart_reject_shadows;
extern VCvarB gl_smart_reject_svol_segs;
extern VCvarB gl_smart_reject_svol_flats;


enum {
  SFST_Normal,
  SFST_NormalGlow,
  SFST_BMap,
  SFST_BMapGlow,
  SFST_MAX,
};


//==========================================================================
//
//  ClassifySurfaceShader
//
//  do not call on invisible or texture-less surfaces
//
//==========================================================================
static VVA_OKUNUSED inline int ClassifySurfaceShader (const surface_t *surf) {
  if (r_brightmaps && surf->texinfo->Tex->Brightmap) return (surf->gp.isActive() ? SFST_BMapGlow : SFST_BMap);
  return (surf->gp.isActive() ? SFST_NormalGlow : SFST_Normal);
}


//==========================================================================
//
//  CheckListSortValidity
//
//==========================================================================
#if 0
static void CheckListSortValidity (TArray<surface_t *> &list, const char *listname) {
  const int len = list.length();
  const surface_t *const *sptr = list.ptr();
  // find first valid surface
  int idx;
  for (idx = 0; idx < len; ++idx, ++sptr) {
    const surface_t *surf = *sptr;
    if (surf->IsPlVisible()) break;
  }
  if (idx >= len) return; // nothing to do
  int phase = list[idx]->shaderClass;
  int previdx = idx;
  // check surfaces
  for (; idx < len; ++idx, ++sptr) {
    const surface_t *surf = *sptr;
    if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
    vassert(surf->texinfo->Tex);
    const int newphase = surf->shaderClass;
    if (newphase < phase) {
      Sys_Error("CheckListSortValidity (%s): shader order check failed at %d of %d; previdx is %d; prevphase is %d; phase is %d", listname, idx, len, previdx, phase, newphase);
    }
    previdx = idx;
    phase = newphase;
  }
}
#endif


//WARNING! don't forget to flush VBO on each shader uniform change! this includes glow changes (glow values aren't cached yet)
#define SADV_FLUSH_VBO()  do { \
  if (vboCountIdx) { \
    if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "flushing ambsurface VBO: vboCountIdx=%d", vboCountIdx); \
    vboAdvSurf.setupAttribNoEnable(attribPosition, 3); \
    p_glMultiDrawArrays(GL_TRIANGLE_FAN, vboStartInds.ptr(), vboCounters.ptr(), (GLsizei)vboCountIdx); \
    vboCountIdx = 0; \
  } \
} while (0)


#define SADV_DO_RENDER()  do { \
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) { \
    if (lastCullFace) { \
      SADV_FLUSH_VBO(); \
      lastCullFace = false; \
      glDisable(GL_CULL_FACE); \
    } \
  } else { \
    if (!lastCullFace) { \
      SADV_FLUSH_VBO(); \
      lastCullFace = true; \
      glEnable(GL_CULL_FACE); \
    } \
  } \
  currentActiveShader->UploadChangedUniforms(); \
  /* remember counter */ \
  vboCounters.ptr()[vboCountIdx] = (GLsizei)surf->count; \
  /* remember first index */ \
  vboStartInds.ptr()[vboCountIdx] = (GLint)surf->firstIndex; \
  /* advance array positions */ \
  ++vboCountIdx; \
} while (0)


#define SADV_CHECK_TEXTURE(shader_)  \
  const texinfo_t *currTexinfo = surf->texinfo; \
  do { \
    const bool textureChanged = lastTexinfo.needChange(*currTexinfo, updateFrame); \
    if (textureChanged) { \
      SADV_FLUSH_VBO(); \
      lastTexinfo.updateLastUsed(*currTexinfo); \
      SetTexture(currTexinfo->Tex, currTexinfo->ColorMap); \
      (shader_).SetTex(currTexinfo); \
    } \
  } while (0)


// this also sorts by fade, so we can avoid resorting in fog pass
int glAdvRenderDrawListItemCmpByTextureAndFade (const void *a, const void *b, void * /*udata*/);

// full shader and texture sorting
int glAdvRenderDrawListItemCmpByShaderTexture (const void *a, const void *b, void * /*udata*/);

// only shaders and brightmap textures will be sorted
int glAdvRenderDrawListItemCmpByShaderBMTexture (const void *a, const void *b, void * /*udata*/);


#endif
