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


// ////////////////////////////////////////////////////////////////////////// //
vuint32 glWDPolyTotal = 0;
vuint32 glWDVertexTotal = 0;
vuint32 glWDTextureChangesTotal = 0;


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
    return compareSurfaces(((const VOpenGLDrawer::SurfListItem *)a)->surf, ((const VOpenGLDrawer::SurfListItem *)b)->surf);
  }

  static int drawListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSkyPolygon
//
//  used in r_sky
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

    SurfDSky.Activate();
    SurfDSky.SetTexture(0);
    SurfDSky.SetTexture2(1);
    SurfDSky.SetBrightness(r_sky_bright_factor);

    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      SurfDSky.SetTexCoord(
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs1)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      SurfDSky.SetTexCoord2(
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs2)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  } else {
    SetTexture(Texture1, CMap);

    SurfSky.Activate();
    SurfSky.SetTexture(0);
    SurfSky.SetBrightness(r_sky_bright_factor);

    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      SurfSky.SetTexCoord(
        (DotProduct(surf->verts[sidx[i]], tex->saxis)+tex->soffs-offs1)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      glVertex(surf->verts[i]);
    }
    glEnd();
  }
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
    TVec HDir = -surf->GetNormal();

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
    TVec HDir = -surf->GetNormal();

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

  SurfSimple.Activate();
  SurfSimple.SetTexture(0);
  //SurfSimple_Locs.storeFogType();
  SurfSimple.SetTex(Tex);

  const float lev = getSurfLightLevel(surf);
  SurfSimple.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
  SurfSimple.SetFogFade(surf->Fade, 1.0f);

  // draw it
  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < 4; ++i) glVertex(v[i]);
  glEnd();
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  glDepthMask(oldDepthMask);

  // write to the depth buffer
  SurfZBuf.Activate();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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

  bool doBrightmap = (r_brightmaps && textr->Tex->Brightmap);

  GlowParams gp;
  CalcGlow(gp, surf);

  if (textureChanged) {
    if (doBrightmap) {
      SurfSimpleBrightmap.Activate();
      SurfSimpleBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfSimpleBrightmap.SetTexture(0);
      SurfSimpleBrightmap.SetTextureBM(1);
      p_glActiveTextureARB(GL_TEXTURE0+1);
      SetTexture(textr->Tex->Brightmap, 0);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      p_glActiveTextureARB(GL_TEXTURE0);
      SetTexture(textr->Tex, textr->ColourMap);
      SurfSimpleBrightmap.SetTex(textr);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfSimpleBrightmap, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimpleBrightmap);
      }
    } else {
      SetTexture(textr->Tex, textr->ColourMap);
      SurfSimple.Activate();
      SurfSimple.SetTex(textr);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfSimple, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimple);
      }
    }
    ++glWDTextureChangesTotal;
  }

  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render simple surface with %d vertices", surf->count);
    return false;
  }


  float lev = getSurfLightLevel(surf);
  if (doBrightmap) {
    SurfSimpleBrightmap.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfSimpleBrightmap.SetFogFade(surf->Fade, 1.0f);
  } else {
    SurfSimple.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfSimple.SetFogFade(surf->Fade, 1.0f);
  }

  bool doDecals = textr->Tex && !textr->noDecals && surf->seg && surf->seg->decals;

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  ++glWDPolyTotal;
  //glBegin(GL_POLYGON);
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < surf->count; ++i) {
      ++glWDVertexTotal;
      glVertex(surf->verts[i]);
    }
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, textr->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      if (doBrightmap) SurfSimpleBrightmap.Activate(); else SurfSimple.Activate();
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

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  GlowParams gp;
  CalcGlow(gp, surf);

  if (textureChanged) {
    if (doBrightmap) {
      SurfLightmapBrightmap.Activate();
      SurfLightmapBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfLightmapBrightmap.SetTexture(0);
      SurfLightmapBrightmap.SetLightMap(1);
      SurfLightmapBrightmap.SetSpecularMap(2);
      SurfLightmapBrightmap.SetTextureBM(3);
      p_glActiveTextureARB(GL_TEXTURE0+3);
      SetTexture(tex->Tex->Brightmap, 0);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      p_glActiveTextureARB(GL_TEXTURE0);
      SetTexture(tex->Tex, tex->ColourMap);
      SurfLightmapBrightmap.SetTex(tex);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfLightmapBrightmap, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapBrightmap);
      }
    } else {
      SetTexture(tex->Tex, tex->ColourMap);
      SurfLightmap.Activate();
      SurfLightmap.SetTex(tex);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfLightmap, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmap);
      }
    }
    ++glWDTextureChangesTotal;
  }

  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render lmap surface with %d vertices", surf->count);
    return false;
  }

  float lev = getSurfLightLevel(surf);
  if (r_glow_flat && !surf->seg && tex->Tex->glowing) {
    lev = 1.0f;
    if (doBrightmap) SurfLightmapBrightmap.SetFullBright(1.0); else SurfLightmap.SetFullBright(1.0);
  } else {
    if (doBrightmap) SurfLightmapBrightmap.SetFullBright(0.0); else SurfLightmap.SetFullBright(0.0);
  }
  if (doBrightmap) {
    SurfLightmapBrightmap.SetLMap(surf, cache);
    SurfLightmapBrightmap.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfLightmapBrightmap.SetFogFade(surf->Fade, 1.0f);
  } else {
    SurfLightmap.SetLMap(surf, cache);
    SurfLightmap.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfLightmap.SetFogFade(surf->Fade, 1.0f);
  }

  bool doDecals = (tex->Tex && !tex->noDecals && surf->seg && surf->seg->decals);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  ++glWDPolyTotal;
  //glBegin(GL_POLYGON);
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < surf->count; ++i) {
      ++glWDVertexTotal;
      glVertex(surf->verts[i]);
    }
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_LIGHTMAP, surf, cache, tex->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      if (doBrightmap) SurfLightmapBrightmap.Activate(); else SurfLightmap.Activate();
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::WorldDrawing
//
//  lightmapped rendering
//
//==========================================================================
void VOpenGLDrawer::WorldDrawing () {
  // first draw horizons
  {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->IsVisible(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // for sky areas we just write to the depth buffer to prevent drawing polygons behind the sky
  {
    SurfZBuf.Activate();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->IsVisible(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render sky portal surface with %d vertices", surf->count);
        continue;
      }
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw surfaces without lightmaps
  if (RendLev->DrawSurfList.length()) {
    // sort by texture, to minimise texture switches
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    SurfSimple.Activate();
    SurfSimple.SetTexture(0);
    //SurfSimple_Locs.storeFogType();

    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();
    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->IsVisible(vieworg)) continue; // viewer is in back side or on plane
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
    SurfLightmap.Activate();
    SurfLightmap.SetTexture(0);
    SurfLightmap.SetLightMap(1);
    SurfLightmap.SetSpecularMap(2);
    //SurfLightmap_Locs.storeFogType();

    const texinfo_t *lastTexinfo = nullptr;
    for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb) {
      if (!RendLev->light_chain[lb]) continue;

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      if (anisotropyExists) {
        glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
          (gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
        );
      }

      if (RendLev->block_changed[lb]) {
        RendLev->block_changed[lb] = false;
        glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->light_block[lb]);
        RendLev->add_changed[lb] = true;
      }

      SelectTexture(2);
      glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      if (anisotropyExists) {
        glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
          (float)(gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
        );
      }

      if (RendLev->add_changed[lb]) {
        RendLev->add_changed[lb] = false;
        glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->add_block[lb]);
      }

      SelectTexture(0);

      if (!gl_sort_textures) {
        for (surfcache_t *cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
          surface_t *surf = cache->surf;
          if (!surf->IsVisible(vieworg)) continue; // viewer is in back side or on plane
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
          if (!surf->IsVisible(vieworg)) continue; // viewer is in back side or on plane
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
