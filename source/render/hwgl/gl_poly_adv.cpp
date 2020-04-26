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
#include "gl_local.h"


extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;

static VCvarB gl_smart_dirty_rects("gl_smart_dirty_rects", true, "Use dirty rectangles list to check for stencil buffer dirtyness?", CVAR_Archive);
static VCvarB gl_smart_reject_shadow_surfaces("gl_smart_reject_shadow_surfaces", false, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

static VCvarB gl_smart_reject_shadow_segs("gl_smart_reject_shadow_segs", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);
static VCvarB gl_smart_reject_shadow_flats("gl_smart_reject_shadow_flats", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

static VCvarB gl_dbg_vbo_adv_ambient("gl_dbg_vbo_adv_ambient", false, "dump some VBO statistics for advrender abmient pass VBO utilisation?", CVAR_PreInit);


/* TODO
  clear stencil buffer before first shadow shadow rendered.
  also, check if the given surface really can cast shadow.
  note that solid segs that has no non-solid neighbours cannot cast any shadow.
  also, flat surfaces in subsectors whose neighbours doesn't change height can't cast any shadow.
*/

// ////////////////////////////////////////////////////////////////////////// //
struct DRect {
  int x0, y0;
  int x1, y1; // inclusive
};

static TArray<DRect> dirtyRects;


//==========================================================================
//
//  isDirtyRect
//
//==========================================================================
static bool isDirtyRect (const GLint arect[4]) {
  for (auto &&r : dirtyRects) {
    if (arect[VOpenGLDrawer::SCS_MAXX] < r.x0 || arect[VOpenGLDrawer::SCS_MAXY] < r.y0 ||
        arect[VOpenGLDrawer::SCS_MINX] > r.x1 || arect[VOpenGLDrawer::SCS_MINY] > r.y1)
    {
      continue;
    }
    return true;
  }
  return false;
}


//==========================================================================
//
//  appendDirtyRect
//
//==========================================================================
static void appendDirtyRect (const GLint arect[4]) {
  // remove all rects that are inside our new one
  /*
  int ridx = 0;
  while (ridx < dirtyRects.length()) {
    const DRect r = dirtyRects[ridx];
    // if new rect is inside some old one, do nothing
    if (arect[VOpenGLDrawer::SCS_MINX] >= r.x0 && arect[VOpenGLDrawer::SCS_MINY] >= r.y0 &&
        arect[VOpenGLDrawer::SCS_MAXX] <= r.x1 && arect[VOpenGLDrawer::SCS_MAXY] <= r.y1)
    {
      return;
    }
    // if old rect is inside a new one, remove old rect
    if (r.x0 >= arect[VOpenGLDrawer::SCS_MINX] && r.y0 >= arect[VOpenGLDrawer::SCS_MINY] &&
        r.x1 <= arect[VOpenGLDrawer::SCS_MAXX] && r.y1 <= arect[VOpenGLDrawer::SCS_MAXY])
    {
      dirtyRects.removeAt(ridx);
      continue;
    }
    // check next rect
    ++ridx;
  }
  */

  // append new one
  DRect &rc = dirtyRects.alloc();
  rc.x0 = arect[VOpenGLDrawer::SCS_MINX];
  rc.y0 = arect[VOpenGLDrawer::SCS_MINY];
  rc.x1 = arect[VOpenGLDrawer::SCS_MAXX];
  rc.y1 = arect[VOpenGLDrawer::SCS_MAXY];
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  SFST_Normal,
  SFST_NormalGlow,
  SFST_BMap,
  SFST_BMapGlow,
  SFST_MAX,
};


extern "C" {
  // full shader and texture sorting
  static int drawListItemCmpByShaderTexture (const void *a, const void *b, void * /*udata*/) {
    if (a == b) return 0;
    const surface_t *sa = *(const surface_t **)a;
    const surface_t *sb = *(const surface_t **)b;
    // shader class first
    const int stp = sa->shaderClass-sb->shaderClass;
    if (stp) return stp;
    // here shader classes are equal
    // invalid shader classes are sorted by element address
    if (sa->shaderClass < 0) {
      if ((uintptr_t)a < (uintptr_t)b) return -1;
      if ((uintptr_t)a > (uintptr_t)b) return 1;
      return 0;
    }
    // here both surfaces are valid to render
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    // sort by texture id (just use texture pointer)
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    // by light level/color
    if (sa->Light < sb->Light) return -1;
    if (sa->Light > sb->Light) return 1;
    // and by colormap, why not?
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }

  // only shaders and brightmap textures will be sorted
  static int drawListItemCmpByShaderBMTexture (const void *a, const void *b, void * /*udata*/) {
    if (a == b) return 0;
    const surface_t *sa = *(const surface_t **)a;
    const surface_t *sb = *(const surface_t **)b;
    // shader class first
    const int stp = sa->shaderClass-sb->shaderClass;
    if (stp) return stp;
    // here shader classes are equal
    // invalid shader classes are sorted by element address
    if (sa->shaderClass < 0) {
      if ((uintptr_t)a < (uintptr_t)b) return -1;
      if ((uintptr_t)a > (uintptr_t)b) return 1;
      return 0;
    }
    // non-brightmap classes are sorted by light, and then by element address
    if (sa->shaderClass != SFST_BMap && sa->shaderClass != SFST_BMapGlow) {
      // by light level/color
      if (sa->Light < sb->Light) return -1;
      if (sa->Light > sb->Light) return 1;
      // same light level
      if ((uintptr_t)a < (uintptr_t)b) return -1;
      if ((uintptr_t)a > (uintptr_t)b) return 1;
      return 0;
    }
    // here both surfaces are valid to render
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    // sort by texture id (just use texture pointer)
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    // by light level/color
    if (sa->Light < sb->Light) return -1;
    if (sa->Light > sb->Light) return 1;
    // and by colormap, why not?
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }
}


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
    if (surf->plvisible) break;
  }
  if (idx >= len) return; // nothing to do
  int phase = /*ClassifySurfaceShader(list[idx])*/list[idx]->shaderClass;
  int previdx = idx;
  // check surfaces
  for (; idx < len; ++idx, ++sptr) {
    const surface_t *surf = *sptr;
    if (!surf->plvisible) continue; // viewer is in back side or on plane
    vassert(surf->texinfo->Tex);
    const int newphase = /*ClassifySurfaceShader(surf)*/surf->shaderClass;
    if (newphase < phase) {
      Sys_Error("CheckListSortValidity (%s): shader order check failed at %d of %d; previdx is %d; prevphase is %d; phase is %d", listname, idx, len, previdx, phase, newphase);
    }
    previdx = idx;
    phase = newphase;
  }
}
#endif


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
  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();

  // draw horizons
  if (!gl_dbg_wireframe) {
    surface_t **surfptr = dls.DrawHorizonList.ptr();
    for (int count = dls.DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (dls.DrawSkyList.length() && !gl_dbg_wireframe) {
    SurfZBuf.Activate();
    SurfZBuf.UploadChangedUniforms();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = dls.DrawSkyList.ptr();
    for (int count = dls.DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw normal surfaces
  if (dls.DrawSurfListSolid.length() != 0 || dls.DrawSurfListMasked.length() != 0) {
    // reserve room for max number of elements in VBO, because why not?
    int currEls[SFST_MAX];
    // precalculate some crap
    memset(currEls, 0, sizeof(currEls));
    for (auto &&surf : dls.DrawSurfListSolid) {
      surf->gp.clear();
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) { surf->plvisible = 0; continue; }
      if (surf->drawflags&surface_t::DF_MASKED) { surf->plvisible = 0; continue; } // this should not end up here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) { surf->plvisible = 0; continue; } // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) { surf->plvisible = 0; continue; } // just in case

      CalcGlow(surf->gp, surf);
      surf->shaderClass = ClassifySurfaceShader(surf);
      currEls[surf->shaderClass] += surf->count;
    }

    for (auto &&surf : dls.DrawSurfListMasked) {
      surf->gp.clear();
      surf->shaderClass = -1; // so they will float up
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) { surf->plvisible = 0; continue; }
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) { surf->plvisible = 0; continue; } // this should not end up here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) { surf->plvisible = 0; continue; } // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) { surf->plvisible = 0; continue; } // just in case

      CalcGlow(surf->gp, surf);
      surf->shaderClass = ClassifySurfaceShader(surf);
      currEls[surf->shaderClass] += surf->count;
    }

    int maxEls = currEls[0];
    for (int fcnt = 1; fcnt < SFST_MAX; ++fcnt) maxEls = max2(maxEls, currEls[fcnt]);

    // do not sort surfaces by texture here, because
    // textures will be put later, and BSP sorted them by depth for us
    // other passes can skip surface sorting
    if (/*gl_sort_textures*/true) {
      // sort masked textures by shader class and texture
      timsort_r(dls.DrawSurfListMasked.ptr(), dls.DrawSurfListMasked.length(), sizeof(surface_t *), &drawListItemCmpByShaderTexture, nullptr);
      // sort solid textures too, so we can avoid shader switches
      // but do this only by shader class, to retain as much front-to-back order as possible
      timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &drawListItemCmpByShaderBMTexture, nullptr);
      #if 0
      CheckListSortValidity(dls.DrawSurfListSolid, "solid");
      CheckListSortValidity(dls.DrawSurfListMasked, "masked");
      #endif
    }

    //FIXME!
    if (gl_dbg_wireframe) {
      DrawAutomap.Activate();
      DrawAutomap.UploadChangedUniforms();
      GLEnableBlend();
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, 0);
      SelectTexture(0);
      return;
    }

    // setup samplers for all shaders
    // masked
    ShadowsAmbientMasked.Activate();
    ShadowsAmbientMasked.SetTexture(0);
    //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
    // brightmap
    ShadowsAmbientBrightmap.Activate();
    ShadowsAmbientBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    ShadowsAmbientBrightmap.SetTexture(0);
    ShadowsAmbientBrightmap.SetTextureBM(1);
    //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
    // normal
    ShadowsAmbient.Activate();
    //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);

    //ShadowsAmbient.Activate();
    //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);

    float prevsflight = -666;
    vuint32 prevlight = 0;
    texinfo_t lastTexinfo;
    lastTexinfo.initLastUsed();

    bool glTextureEnabled = false;
    glDisable(GL_TEXTURE_2D);

    // activate VBO
    GLuint attribPosition = 0; /* shut up, gcc! */
    vboAdvSurf.ensure(maxEls);
    vboAdvSurf.enableAttrib(attribPosition);

    int vboIdx = 0; // data index
    int vboCountIdx = 0; // element (counter) index
    TArray<GLsizei> vboCounters; // number of indicies in each primitive
    TArray<GLint> vboStartInds; // starting indicies
    vboCounters.setLength(dls.DrawSurfListSolid.length()+dls.DrawSurfListMasked.length()+4);
    vboStartInds.setLength(vboCounters.length());

    bool lastCullFace = true;
    glEnable(GL_CULL_FACE);

    if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "=== ambsurface VBO: maxEls=%d; maxcnt=%d ===", maxEls, vboCounters.length());

    //WARNING! don't forget to flush VBO on each shader uniform change! this includes glow changes (glow values aren't cached yet)

    #define SAMB_FLUSH_VBO()  do { \
      if (vboIdx) { \
        if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "flushing ambsurface VBO: vboIdx=%d; vboCountIdx=%d", vboIdx, vboCountIdx); \
        vboAdvSurf.uploadData(vboIdx); \
        vboAdvSurf.setupAttribNoEnable(attribPosition, 3); \
        p_glMultiDrawArrays(GL_TRIANGLE_FAN, vboStartInds.ptr(), vboCounters.ptr(), (GLsizei)vboCountIdx); \
        vboIdx = 0; \
        vboCountIdx = 0; \
      } \
    } while (0)

    #define SAMB_DO_RENDER()  \
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) { \
        if (lastCullFace) { \
          SAMB_FLUSH_VBO(); \
          lastCullFace = false; \
          glDisable(GL_CULL_FACE); \
        } \
      } else { \
        if (!lastCullFace) { \
          SAMB_FLUSH_VBO(); \
          lastCullFace = true; \
          glEnable(GL_CULL_FACE); \
        } \
      } \
      currentActiveShader->UploadChangedUniforms(); \
      /* remember counter */ \
      vboCounters.ptr()[vboCountIdx] = (GLsizei)surf->count; \
      /* remember first index */ \
      vboStartInds.ptr()[vboCountIdx] = (GLint)vboIdx; \
      /* vectors will be put here */ \
      TVec *vp = vboAdvSurf.data.ptr()+vboIdx; \
      /* fill arrays */ \
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) { \
        *vp++ = surf->verts[i].vec(); \
      } \
      /* advance array positions */ \
      vboIdx += surf->count; \
      ++vboCountIdx;

    #define SAMB_DO_HEAD_LIGHT(shader_)  \
      const surface_t *surf = *sptr; \
      /* setup new light if necessary */ \
      const float lev = getSurfLightLevel(surf); \
      if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) { \
        SAMB_FLUSH_VBO(); \
        prevsflight = lev; \
        prevlight = surf->Light; \
        (shader_).SetLight( \
          ((prevlight>>16)&255)*lev/255.0f, \
          ((prevlight>>8)&255)*lev/255.0f, \
          (prevlight&255)*lev/255.0f, 1.0f); \
      }

    #define SAMB_CHECK_TEXTURE_BM(shader_)  \
      const texinfo_t *currTexinfo = surf->texinfo; \
      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame); \
      if (textureChanded) { \
        SAMB_FLUSH_VBO(); \
        lastTexinfo.updateLastUsed(*currTexinfo); \
        SelectTexture(1); \
        SetBrightmapTexture(currTexinfo->Tex->Brightmap); \
        SelectTexture(0); \
        /* set normal texture */ \
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap); \
        (shader_).SetTex(currTexinfo); \
      }

    #define SAMB_CHECK_TEXTURE_NORMAL(shader_)  \
      const texinfo_t *currTexinfo = surf->texinfo; \
      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame); \
      if (textureChanded) { \
        SAMB_FLUSH_VBO(); \
        lastTexinfo.updateLastUsed(*currTexinfo); \
        /* set normal texture */ \
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap); \
        (shader_).SetTex(currTexinfo); \
      }

    // solid textures
    if (dls.DrawSurfListSolid.length() != 0) {
      const int len = dls.DrawSurfListSolid.length();
      const surface_t *const *sptr = dls.DrawSurfListSolid.ptr();
      // find first valid surface
      int idx;
      for (idx = 0; idx < len; ++idx, ++sptr) {
        const surface_t *surf = *sptr;
        if (surf->plvisible) break;
      }

      // normal textures
      prevsflight = -666; // force light setup
      if (idx < len && (*sptr)->shaderClass == SFST_Normal) {
        ShadowsAmbient.Activate();
        attribPosition = ShadowsAmbient.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
        if (glTextureEnabled) { glTextureEnabled = false; glDisable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_Normal; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbient)
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // normal glowing textures
      if (idx < len && (*sptr)->shaderClass == SFST_NormalGlow) {
        ShadowsAmbient.Activate();
        attribPosition = ShadowsAmbient.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (glTextureEnabled) { glTextureEnabled = false; glDisable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_NormalGlow; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbient)
          SAMB_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbient, surf->gp);
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_BMap) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMap; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SAMB_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap)
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap glow textures
      if (idx < len && (*sptr)->shaderClass == SFST_BMapGlow) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMapGlow; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SAMB_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap)
          SAMB_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, surf->gp);
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }
    }

    // masked textures
    if (dls.DrawSurfListMasked.length() != 0) {
      const int len = dls.DrawSurfListMasked.length();
      const surface_t *const *sptr = dls.DrawSurfListMasked.ptr();
      // find first valid surface
      int idx;
      for (idx = 0; idx < len; ++idx, ++sptr) {
        const surface_t *surf = *sptr;
        if (surf->plvisible) break;
      }

      // normal textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_Normal) {
        ShadowsAmbientMasked.Activate();
        attribPosition = ShadowsAmbientMasked.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_Normal; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientMasked)
          SAMB_CHECK_TEXTURE_NORMAL(ShadowsAmbientMasked)
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // normal glowing textures
      if (idx < len && (*sptr)->shaderClass == SFST_NormalGlow) {
        ShadowsAmbientMasked.Activate();
        attribPosition = ShadowsAmbientMasked.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_NormalGlow; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientMasked)
          SAMB_CHECK_TEXTURE_NORMAL(ShadowsAmbientMasked)
          SAMB_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientMasked, surf->gp);
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_BMap) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMap; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SAMB_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap)
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap glow textures
      if (idx < len && (*sptr)->shaderClass == SFST_BMapGlow) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMapGlow; ++idx, ++sptr) {
          SAMB_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SAMB_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap)
          SAMB_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, surf->gp);
          SAMB_DO_RENDER()
        }
        SAMB_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }
    }

    // deactivate VBO
    vboAdvSurf.deactivate();

    if (!lastCullFace) glEnable(GL_CULL_FACE);
    if (!glTextureEnabled) glEnable(GL_TEXTURE_2D);
  }

  // restore depth function
  //if (gl_prefill_zbuffer) RestoreDepthFunc();

  SelectTexture(1);
  glBindTexture(GL_TEXTURE_2D, 0);
  SelectTexture(0);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginShadowVolumesPass
//
//  setup general rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginShadowVolumesPass () {
  //glEnable(GL_STENCIL_TEST);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  // reset last known scissor
  glGetIntegerv(GL_VIEWPORT, lastSVVport);
  memcpy(lastSVScissor, lastSVVport, sizeof(lastSVScissor));
  if (gl_smart_dirty_rects) dirtyRects.reset();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//  setup rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const TVec &aconeDir, const float aconeAngle) {
  wasRenderedShadowSurface = false;
  if (gl_dbg_wireframe) return;
  //GCon->Logf("*** VOpenGLDrawer::BeginLightShadowVolumes(): stencil_dirty=%d", (int)IsStencilBufferDirty());
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
      GLDisableBlend();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      glMatrixMode(GL_PROJECTION); glPushMatrix(); //glLoadIdentity();
      glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
      //glMatrixMode(GL_TEXTURE); glPushMatrix();
      //glMatrixMode(GL_COLOR); glPushMatrix();

      p_glUseProgramObjectARB(0);
      currentActiveShader = nullptr;
      glStencilFunc(GL_ALWAYS, 0x0, 0xff);
      glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

      SetOrthoProjection(0, Drawer->getWidth(), Drawer->getHeight(), 0);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(Drawer->getWidth(), 0);
        glVertex2i(Drawer->getWidth(), Drawer->getHeight());
        glVertex2i(0, Drawer->getHeight());
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
      if (!IsStencilBufferDirty()) {
        // check if current scissor rect is not inside the previous one
        // if it is not inside, we still have to clear stencil buffer
        if (currentSVScissor[SCS_MINX] < lastSVScissor[SCS_MINX] ||
            currentSVScissor[SCS_MINY] < lastSVScissor[SCS_MINY] ||
            currentSVScissor[SCS_MAXX] > lastSVScissor[SCS_MAXX] ||
            currentSVScissor[SCS_MAXY] > lastSVScissor[SCS_MAXY])
        {
          //GCon->Log("*** VOpenGLDrawer::BeginLightShadowVolumes(): force scissor clrear");
          if (gl_smart_dirty_rects) {
            if (isDirtyRect(currentSVScissor)) {
              //GCon->Log("*** VOpenGLDrawer::BeginLightShadowVolumes(): force scissor clrear");
              NoteStencilBufferDirty();
            }
          } else {
            NoteStencilBufferDirty();
          }
        }
      }
      ClearStencilBuffer();
    }
  } else {
    glDisable(GL_SCISSOR_TEST);
    ClearStencilBuffer();
  }
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  GLDisableBlend();
  glDisable(GL_CULL_FACE);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);
  glEnable(GL_STENCIL_TEST);

  if (!CanUseRevZ()) {
    // normal
    // shadow volume offseting is done in the main renderer
    glDepthFunc(GL_LESS);
    //glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    // shadow volume offseting is done in the main renderer
    glDepthFunc(GL_GREATER);
    //glDepthFunc(GL_GEQUAL);
  }
  // face, stencil-fail, depth-fail, depth-pass

  usingZPass = useZPass;

  if (gl_dbg_use_zpass || useZPass) {
    // a typical setup for z-pass method
    p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
    p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
  } else {
    // a typical setup for z-fail method
    p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
    p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
  }

  coneDir = aconeDir;
  coneAngle = (aconeAngle <= 0.0f || aconeAngle >= 360.0f ? 0.0f : aconeAngle);

  if (coneAngle && aconeDir.isValid() && !aconeDir.isZero()) {
    spotLight = true;
    coneDir.normaliseInPlace();
  } else {
    spotLight = false;
  }
  SurfShadowVolume.Activate();
  SurfShadowVolume.SetLightPos(LightPos);
  SurfShadowVolume.UploadChangedUniforms();

  // remember current scissor rect
  memcpy(lastSVScissor, currentSVScissor, sizeof(lastSVScissor));
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  //GCon->Logf("*** VOpenGLDrawer::EndLightShadowVolumes(): stencil_dirty=%d", (int)IsStencilBufferDirty());
  //RestoreDepthFunc(); // no need to do this, if will be modified anyway
  // meh, just turn if off each time
  #if 0
  //FIXME: done in main renderer now
  /*if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer)*/ {
    GLDisableOffset();
  }
  #endif
  //glDisable(GL_SCISSOR_TEST);
  //glEnable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  CanSurfaceSegCastShadow
//
//==========================================================================
static bool CanSurfaceSegCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_shadow_segs) return true;

  // solid segs that has no non-solid neighbours cannot cast any shadow
  const seg_t *seg = surf->seg;
  const line_t *ldef = seg->linedef;
  if (!ldef) {
    // miniseg; wutafuck? it should not have any surface!
    GCon->Log(NAME_Error, "miniseg should not have any surfaces!");
    return true;
  }

  // we cannot do anything sane for 3D floors
  const subsector_t *sub = surf->subsector;
  if (!sub) return true;

  const sector_t *sector = sub->sector;
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // if this is a two-sided line, don't reject it
  if (ldef->flags&ML_TWOSIDED) {
    /*
    if (!seg->partner) return false; // just in case
    const sector_t *backsec = seg->partner->frontsub->sector;
    vassert(backsec);

    // here we can check if this is top/bottom texture, and if it can cast shadow
    // to check this, see if light can touch surface edge, and consider this seg one-sided, if it isn't

    // calculate coordinates of bottom texture (if any)
    if (surf->typeFlags&surface_t::TF_BOTTOM) {
      // just in case: if back sector floor should be higher that than our floor
      float minz = sector->floor.minz;
      float maxz = backsec->floor.maxz;
      if (maxz <= minz) return false; // bottom texture shouldn't be visible anyway
      GCon->Logf("*** BOTTOM CHECK! minz=%g; maxz=%g", minz, maxz);
      GCon->Logf("   lz=%g; llow=%g; lhigh=%g", LightPos.z, LightPos.z-Radius, LightPos.z+Radius);
      // if light is fully inside or outside, this seg cannot cast shadow
      // fully outside?
      if (LightPos.z+Radius <= minz || LightPos.z-Radius >= maxz) return false;
      // fully inside?
      if (LightPos.z+Radius > maxz) {
        return true;
      } else {
        GCon->Logf("*** BOTTOM REJECT!");
      }
    } else {
      return true;
    }
    */
    return true;
  }

  // if this is not a two-sided line, only first and last segs can cast shadows
  //!!!if ((int)(ptrdiff_t)(ldef-GClLevel->Lines) == 42) GCon->Log("********* 42 ************");
  if (*seg->v1 != *ldef->v1 && *seg->v2 != *ldef->v2 &&
      *seg->v2 != *ldef->v1 && *seg->v1 != *ldef->v2)
  {
    //!!!GCon->Log("*** skipped useless shadow segment (0)");
    return true;
  }

  // if all neighbour lines are one-sided, and doesn't make a sharp turn, this seg cannot cast a shadow

  // check v1
  const line_t *const *lnx = ldef->v1lines;
  for (int cc = ldef->v1linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  // check v2
  lnx = ldef->v2lines;
  for (int cc = ldef->v2linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  //!!!GCon->Log("*** skipped useless shadow segment (1)");
  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceFlatCastShadow
//
//==========================================================================
static bool CanSurfaceFlatCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_shadow_flats) return true;

  // flat surfaces in subsectors whose neighbours doesn't change height can't cast any shadow
  const subsector_t *sub = surf->subsector;
  if (sub->numlines == 0) return true; // just in case

  const sector_t *sector = sub->sector;
  // sadly, we cannot optimise for sectors with 3D (extra) floors
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // do we have any 3D floors in this sector?
  if (sector->SectorFlags&sector_t::SF_HasExtrafloors) {
    // check if we're doing top ceiling, or bottom floor
    // (this should always be the case, but...)
    if (surf->plane.normal == sector->floor.normal) {
      if (surf->plane.dist != sector->floor.dist) return true;
    } else if (surf->plane.normal == sector->ceiling.normal) {
      if (surf->plane.dist != sector->ceiling.dist) return true;
    } else {
      return true;
    }
  }

  const seg_t *seg = sub->firstseg;
  for (int cnt = sub->numlines; cnt--; ++seg) {
    const seg_t *backseg = seg->partner;
    if (!backseg) continue;
    const subsector_t *sub2 = backseg->frontsub;
    if (sub2 == sub) continue;
    // different subsector
    const sector_t *bsec = sub2->sector;
    if (bsec == sector) continue;
    // different sector
    if (!backseg->SphereTouches(LightPos, Radius)) continue;
    // and light sphere touches it, check heights
    if (surf->typeFlags&surface_t::TF_FLOOR) {
      // if current sector floor is lower than the neighbour sector floor,
      // it means that our current floor cannot cast a shadow there
      //if (sector->floor.minz <= bsec->floor.maxz) continue;
      if (bsec->floor.minz == sector->floor.minz &&
          bsec->floor.maxz == sector->floor.maxz)
      {
        continue;
      }
    } else if (surf->typeFlags&surface_t::TF_CEILING) {
      // if current sector ceiling is higher than the neighbour sector ceiling,
      // it means that our current ceiling cannot cast a shadow there
      //if (sector->ceiling.maxz >= bsec->ceiling.minz) continue;
      // this is wrong; see Doom2:MAP02, room with two holes -- shot a fireball inside one hole
      // this is wrong because we have two sectors with the same ceiling height, and then a hole
      // so first sector ceiling is lit, and should block the light, but it is ignored
      if (bsec->ceiling.minz == sector->ceiling.minz &&
          bsec->ceiling.maxz == sector->ceiling.maxz)
      {
        continue;
      }
    } else {
      GCon->Log("oops; non-floor and non-ceiling flat surface");
    }
    /*
    if (FASI(bsec->floor.minz) == FASI(sector->floor.minz) &&
        FASI(bsec->floor.maxz) == FASI(sector->floor.maxz) &&
        FASI(bsec->ceiling.minz) == FASI(sector->ceiling.minz) &&
        FASI(bsec->ceiling.maxz) == FASI(sector->ceiling.maxz))
    {
      continue;
    }
    */
    return true;
  }

  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceCastShadow
//
//==========================================================================
static bool CanSurfaceCastShadow (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (surf->seg) {
    return CanSurfaceSegCastShadow(surf, LightPos, Radius);
  } else if (surf->subsector) {
    return CanSurfaceFlatCastShadow(surf, LightPos, Radius);
  }
  // just in case
  return true;
}


#include "gl_poly_adv_zpass.cpp"


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
//  FIXME: gozzo 3d-shit extra should be rendered in both directions?
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  if (gl_smart_reject_shadow_surfaces && !CanSurfaceCastShadow(surf, LightPos, Radius)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  if (spotLight) {
    // reject all surfaces behind a spotlight
    //TODO: build spotlight frustum, and perform a rejection with it
    //      or even better: perform such rejection earilier
    TPlane pl;
    pl.SetPointNormal3D(LightPos, coneDir);
    const SurfVertex *vv = sverts;
    bool splhit = false;
    /*
    for (unsigned f = vcount; f--; ++vv) {
      if (vv->vec().isInSpotlight(LightPos, coneDir, coneAngle)) { splhit = true; break; }
    }
    */
    for (unsigned f = vcount; f--; ++vv) {
      if (!pl.PointOnSide(vv->vec())) { splhit = true; break; }
    }
    if (!splhit) return;
  }

  //GCon->Logf("***   VOpenGLDrawer::RenderSurfaceShadowVolume()");
  if (!wasRenderedShadowSurface && gl_smart_dirty_rects) {
    appendDirtyRect(currentSVScissor);
  }
  wasRenderedShadowSurface = true;
  NoteStencilBufferDirty();

  if (usingZPass || gl_dbg_use_zpass) {
    RenderSurfaceShadowVolumeZPassIntr(surf, LightPos, Radius);
  } else {
    // OpenGL renders vertices with zero `w` as infinitely far -- this is exactly what we want
    // just do it in vertex shader

    currentActiveShader->UploadChangedUniforms();
    //currentActiveShader->UploadChangedAttrs();

    // render far cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = vcount; i--; ) glVertex4(v[i].vec(), 0);
    glEnd();

    // render near cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i].vec());
    glEnd();

    // render side caps
    glBegin(GL_TRIANGLE_STRIP);
      for (unsigned i = 0; i < vcount; ++i) {
        glVertex(sverts[i].vec());
        glVertex4(v[i].vec(), 0);
      }
      glVertex(sverts[0].vec());
      glVertex4(v[0].vec(), 0);
    glEnd();
  }
}


#define SETUP_LIGHT_SHADER(shad_)  do { \
  (shad_).Activate(); \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_).SetTexture(0); \
  if (!gl_dbg_advlight_debug) { \
    (shad_).SetLightMin(LightMin); \
  } else { \
    Color = gl_dbg_advlight_color; \
  } \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \
} while (0)


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//  setup rendering parameters for lighted surface rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) {
  if (gl_dbg_wireframe) return;
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes
  glDisable(GL_TEXTURE_2D);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // do not use stencil test if we rendered no shadow surfaces
  if (doShadow && IsStencilBufferDirty()/*wasRenderedShadowSurface*/) {
    if (gl_dbg_use_zpass > 1) {
      glStencilFunc(GL_EQUAL, 0x1, 0xff);
    } else {
      glStencilFunc(GL_EQUAL, 0x0, 0xff);
    }
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glEnable(GL_STENCIL_TEST);
  } else {
    glDisable(GL_STENCIL_TEST);
  }

  /*
  if (doShadow && !wasRenderedShadowSurface) {
    Color = 0xffff0000u;
  }
  */

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  GLEnableBlend();
  //glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
  //p_glBlendEquation(GL_MAX_EXT);

  glDepthFunc(GL_EQUAL);

  if (spotLight) {
    if (!gl_dbg_advlight_debug) {
      SETUP_LIGHT_SHADER(ShadowsLightSpot);
      ShadowsLightSpot.SetConeDirection(coneDir);
      ShadowsLightSpot.SetConeAngle(coneAngle);
    } else {
      SETUP_LIGHT_SHADER(ShadowsLightSpotDbg);
      ShadowsLightSpotDbg.SetConeDirection(coneDir);
      ShadowsLightSpotDbg.SetConeAngle(coneAngle);
    }
  } else {
    if (!gl_dbg_advlight_debug) {
      SETUP_LIGHT_SHADER(ShadowsLight);
    } else {
      SETUP_LIGHT_SHADER(ShadowsLightDbg);
    }
  }
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
  if (gl_dbg_wireframe) return;
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColorMap);

  if (spotLight) {
    if (!gl_dbg_advlight_debug) {
      ShadowsLightSpot.SetTex(tex);
      ShadowsLightSpot.SetSurfNormal(surf->GetNormal());
      ShadowsLightSpot.SetSurfDist(surf->GetDist());
    } else {
      ShadowsLightSpotDbg.SetTex(tex);
      ShadowsLightSpotDbg.SetSurfNormal(surf->GetNormal());
      ShadowsLightSpotDbg.SetSurfDist(surf->GetDist());
    }
  } else {
    if (!gl_dbg_advlight_debug) {
      ShadowsLight.SetTex(tex);
      ShadowsLight.SetSurfNormal(surf->GetNormal());
      ShadowsLight.SetSurfDist(surf->GetDist());
    } else {
      ShadowsLightDbg.SetTex(tex);
      ShadowsLightDbg.SetSurfNormal(surf->GetNormal());
      ShadowsLightDbg.SetSurfDist(surf->GetDist());
    }
  }

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  currentActiveShader->UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
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
  if (gl_dbg_wireframe) return;
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  //p_glBlendEquation(GL_FUNC_ADD);

  // copy ambient light texture to FBO, so we can use it to light decals
  auto mfbo = GetMainFBO();
  mfbo->blitTo(&ambLightFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, ambLightFBO.getWidth(), ambLightFBO.getHeight(), GL_NEAREST);
  mfbo->activate();

  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLDisableOffset();
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();


  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  GLEnableBlend();

  if (!gl_dbg_adv_render_surface_textures) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  ShadowsTextureMasked.Activate();
  ShadowsTextureMasked.SetTexture(0);

  ShadowsTexture.Activate();
  ShadowsTexture.SetTexture(0);

  //GLDisableBlend();

  // sort by textures
  if (/*gl_sort_textures*/true) {
    // sort surfaces with solid textures, because here we need them sorted
    timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &drawListItemCmpByShaderTexture, nullptr);
  }

  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTexture.Activate();
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
      if (textureChanded) {
        // update dynamic texture
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsTexture.SetTex(currTexinfo);
      }

      bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decalhead);

      // fill stencil buffer for decals
      if (doDecals) RenderPrepareShaderDecals(surf);

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
          /*
          p_glVertexAttrib2fARB(ShadowsTexture_TexCoordLoc,
            (DotProduct(surf->verts[i].vec(), currTexinfo->saxis)+currTexinfo->soffs)*tex_iw,
            (DotProduct(surf->verts[i].vec(), currTexinfo->taxis)+currTexinfo->toffs)*tex_ih);
          */
          glVertex(surf->verts[i].vec());
        }
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

      if (doDecals) {
        if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColorMap)) {
          ShadowsTexture.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //GLEnableBlend();
          lastTexinfo.resetLastUsed(); // resetup texture
        }
      }
    }
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTextureMasked.Activate();
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) continue; // not here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
      if (textureChanded) {
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsTextureMasked.SetTex(currTexinfo);
      }

      bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decalhead);

      // fill stencil buffer for decals
      if (doDecals) RenderPrepareShaderDecals(surf);

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
          /*
          p_glVertexAttrib2fARB(ShadowsTexture_TexCoordLoc,
            (DotProduct(surf->verts[i].vec(), currTexinfo->saxis)+currTexinfo->soffs)*tex_iw,
            (DotProduct(surf->verts[i].vec(), currTexinfo->taxis)+currTexinfo->toffs)*tex_ih);
          */
          glVertex(surf->verts[i].vec());
        }
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

      if (doDecals) {
        if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColorMap)) {
          ShadowsTextureMasked.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //GLEnableBlend();
          lastTexinfo.resetLastUsed(); // resetup texture
        }
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
  if (gl_dbg_wireframe) return;
  GLEnableBlend();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  glDepthMask(GL_FALSE); // no z-buffer writes

  // draw surfaces
  //ShadowsFog.Activate();
  //ShadowsFog.SetFogType();

  if (!gl_dbg_adv_render_surface_fog) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  /*
  ShadowsFog.SetTexture(0);
  ShadowsFog.SetFogFade(lastFade, 1.0f);
  */

  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFog.Activate();
    ShadowsFog.SetFogFade(0, 1.0f);
    vuint32 lastFade = 0;
    glDisable(GL_TEXTURE_2D);
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->Fade) continue;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      if (lastFade != surf->Fade) {
        lastFade = surf->Fade;
        ShadowsFog.SetFogFade(surf->Fade, 1.0f);
      }

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
    glEnable(GL_TEXTURE_2D);
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFogMasked.Activate();
    ShadowsFogMasked.SetFogFade(0, 1.0f);
    ShadowsFogMasked.SetTexture(0);
    vuint32 lastFade = 0;
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->Fade) continue;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) continue; // not here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      if (lastFade != surf->Fade) {
        lastFade = surf->Fade;
        ShadowsFogMasked.SetFogFade(surf->Fade, 1.0f);
      }

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
      if (textureChanded) {
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsFogMasked.SetTex(currTexinfo);
      }

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
  }

  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  //GLDisableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
  // back to normal z-buffering
  glDepthMask(GL_TRUE); // allow z-buffer writes
  RestoreDepthFunc();
}
