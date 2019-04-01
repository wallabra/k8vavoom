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


// this is used to compare floats like ints which is faster
#define FASI(var) (*(const uint32_t *)&var)


extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline int compareSurfaces (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    // put steamlined masked textures on bottom (this is useful in fog rendering)
    if (sa->drawflags&surface_t::DF_MASKED) {
      if (!(sb->drawflags&surface_t::DF_MASKED)) return 1;
    } else if (sb->drawflags&surface_t::DF_MASKED) {
      if (!(sa->drawflags&surface_t::DF_MASKED)) return -1;
    }
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColourMap)-((int)tb->ColourMap);
  }

  static int drawListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
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
  if (!gl_dbg_wireframe) {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (RendLev->DrawSkyList.length() && !gl_dbg_wireframe) {
    SurfZBuf.Activate();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
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

  // draw normal surfaces
  if (RendLev->DrawSurfList.length()) {
    bool lastWasMasked = false;
    bool firstMasked = true;

    if (gl_dbg_wireframe) {
      DrawAutomap.Activate();
      glEnable(GL_BLEND);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      ShadowsAmbient.Activate();
    }

    // other passes can skip surface sorting
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    float prevsflight = -666;
    vuint32 prevlight = 0;
    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();
    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      const surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render simple ambient surface with %d vertices", surf->count);
        continue;
      }

      if (gl_dbg_wireframe) {
        float clr = (float)(count+1)/RendLev->DrawSurfList.length();
        if (clr < 0.1f) clr = 0.1f;
        glColor4f(clr, clr, clr, 1.0f);
      }

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
      if (currTexinfo->Alpha < 1.0f) continue;

      if (surf->drawflags&surface_t::DF_MASKED) {
        // masked wall
        if (!lastWasMasked) {
          // switch shader
          ShadowsAmbientMasked.Activate();
          lastWasMasked = true;
          if (firstMasked) {
            ShadowsAmbientMasked.SetTexture(0);
            firstMasked = false;
          }
          //GCon->Logf("SWITCH TO MASKED!");
        }

        bool textureChanded =
          !lastTexinfo ||
          lastTexinfo != currTexinfo ||
          lastTexinfo->Tex != currTexinfo->Tex ||
          lastTexinfo->ColourMap != currTexinfo->ColourMap;
        lastTexinfo = currTexinfo;

        if (textureChanded) {
          if (!gl_dbg_wireframe) {
            SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
            ShadowsAmbientMasked.SetTex(currTexinfo);
          }
        }
      } else {
        // normal wall
        if (lastWasMasked) {
          // switch shader
          ShadowsAmbient.Activate();
          lastWasMasked = false;
          //GCon->Logf("SWITCH TO NORMAL!");
        }
      }

      if (!gl_dbg_wireframe) {
        const float lev = getSurfLightLevel(surf);
        if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) {
          prevsflight = lev;
          prevlight = surf->Light;
          if (lastWasMasked) {
            ShadowsAmbientMasked.SetLight(
              ((surf->Light>>16)&255)*lev/255.0f,
              ((surf->Light>>8)&255)*lev/255.0f,
              (surf->Light&255)*lev/255.0f, 1.0f);
          } else {
            ShadowsAmbient.SetLight(
              ((surf->Light>>16)&255)*lev/255.0f,
              ((surf->Light>>8)&255)*lev/255.0f,
              (surf->Light&255)*lev/255.0f, 1.0f);
          }
        }
      }

      if (!gl_dbg_wireframe) {
        // normal
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
      } else {
        // wireframe
        //FIXME: this is wrong for "quality mode", where we'll subdivide surface to more triangles
        glBegin(GL_LINE_LOOP);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
      }
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
void VOpenGLDrawer::BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4]) {
  if (gl_dbg_wireframe) return;
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

  SurfShadowVolume.Activate();
  SurfShadowVolume.SetLightPos(LightPos);
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  //RestoreDepthFunc(); // no need to do this, if will be modified anyway
  // meh, just turn if off each time
  /*if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer)*/ {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
  }
  //glDisable(GL_SCISSOR_TEST);
  //glEnable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  R_GlobalPointToLocal
//
//==========================================================================
static __attribute__((unused)) inline TVec R_GlobalPointToLocal (const VMatrix4 &modelMatrix, const TVec &v) {
  TVec tmp = v-modelMatrix.GetCol(3);
  return modelMatrix.RotateVector(tmp);
}


//==========================================================================
//
//  R_LightProjectionMatrix
//
//==========================================================================
static __attribute__((unused)) void R_LightProjectionMatrix (VMatrix4 &mat, const TVec &origin, const TPlane &rearPlane) {
  float lv[4]; // TVec4

  // calculate the homogenious light vector
  lv[0] = origin.x;
  lv[1] = origin.y;
  lv[2] = origin.z;
  lv[3] = 1.0f;

  const TVec norm = rearPlane.normal;
  const float dist = -rearPlane.dist;

  //lg = rearPlane.ToVec4() * lv;
  const float lg = norm.x*lv[0]+norm.y*lv[1]+norm.z*lv[2]+dist*lv[3];

  // outer product
  mat.m[0][0] = lg-norm.x*lv[0];
  mat.m[1][0] = -norm.y*lv[0];
  mat.m[2][0] = -norm.z*lv[0];
  mat.m[3][0] = -dist*lv[0];

  mat.m[0][1] = -norm.x*lv[1];
  mat.m[1][1] = lg-norm.y*lv[1];
  mat.m[2][1] = -norm.z*lv[1];
  mat.m[3][1] = -dist*lv[1];

  mat.m[0][2] = -norm.x*lv[2];
  mat.m[1][2] = -norm.y*lv[2];
  mat.m[2][2] = lg-norm.z*lv[2];
  mat.m[3][2] = -dist*lv[2];

  mat.m[0][3] = -norm.x*lv[3];
  mat.m[1][3] = -norm.y*lv[3];
  mat.m[2][3] = -norm.z*lv[3];
  mat.m[3][3] = lg-dist*lv[3];
}


//==========================================================================
//
//  R_ProjectPointsToPlane
//
//==========================================================================
static __attribute__((unused)) void R_ProjectPointsToPlane (TVec *dest, const TVec *src, unsigned vcount,
                                    VMatrix4 &mmat, const TVec &LightPos, const TPlane &destPlane)
{
  VMatrix4 mat;
  TVec lv = R_GlobalPointToLocal(mmat, LightPos);
  R_LightProjectionMatrix(mat, lv, destPlane);
  for (unsigned f = 0; f < vcount; ++f, ++dest, ++src) {
    const float w = VSUM4(mat.m[0][3]*src->x, mat.m[1][3]*src->y, mat.m[2][3]*src->z, mat.m[3][3]);
    if (w == 0.0f) {
      *dest = *src;
    } else {
      const float oow = 1.0f/w;
      *dest = mat*(*src);
      *dest *= oow;
    }
  }
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
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  const unsigned vcount = (unsigned)surf->count;
  const TVec *sverts = surf->verts;
  const TVec *v = sverts;

  // OpenGL renders vertices with zero `w` as infinitely far -- this is exactly what we want
  // just do it in vertex shader

  if (!usingZPass && !gl_dbg_use_zpass) {
    // render far cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = vcount; i--; ) glVertex4(v[i], 0);
    glEnd();

    // render near cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i]);
    glEnd();
  } else {
    static TVec *dest = nullptr;
    static unsigned destSize = 0;
    if (destSize < vcount+1) {
      destSize = (vcount|0x7f)+1;
      dest = (TVec *)Z_Realloc(dest, destSize*sizeof(TVec));
    }

    // zpass, project to near clip plane
    VMatrix4 pmat, mmat;
    glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
    glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);

    VMatrix4 comb;
    comb.ModelProjectCombine(mmat, pmat);
    TPlane znear;
    comb.ExtractFrustumNear(znear);

    bool doProject = false;
    const float ldist = znear.Distance(LightPos); // from light to znear
    if (ldist <= 0.0f) {
      // on the back, project only if some surface vertices are on the back too
      for (unsigned f = 0; f < vcount; ++f) {
        const float sdist = znear.Distance(sverts[f]);
        if (sdist >= ldist && sdist <= 0.0f) { doProject = true; break; }
      }
    } else {
      // before camera, project only if some surface vertices are nearer (but not on the back)
      for (unsigned f = 0; f < vcount; ++f) {
        const float sdist = znear.Distance(sverts[f]);
        if (sdist <= ldist && sdist >= 0.0f) { doProject = true; break; }
      }
    }
    doProject = true;

    if (doProject) {
      //if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);

#if 0
      R_ProjectPointsToPlane(dest, sverts, vcount, mmat, LightPos, znear);

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

      // render near cap (it should be front-facing, so it will do "teh right thing")
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) glVertex(dest[f]);
      glEnd();

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) glVertex(sverts[f]);
      glEnd();
#else
      // it is always "front"
      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

      // render near cap (it should be front-facing, so it will do "teh right thing")
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) {
          //glVertex(dest[f]);
          TVec vv = (sverts[f]-LightPos).normalised();
          vv *= 32767.0f; // kind of infinity
          vv += LightPos;
          glVertex(vv);
        }
      glEnd();

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) {
          glVertex(sverts[f]);
        }
      glEnd();
#endif

      //if (hasBoundsTest && gl_enable_depth_bounds) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
    }
  }
#if 0
  else if (!usingZPass) {
    // zp+ implementation
    // this is "forced" zpass, where we may need a projected near cap
    // let's do it: translate, and rotate if the light is in positive half-space of znear
    VMatrix4 pmat, mmat;
    glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
    glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);
    // create new model->world matrix
    float alpha;
    VMatrix4 trans = VMatrix4::Translate(-LightPos);
    VMatrix4 newmmat = trans*mmat;
    //GCon->Logf("=== %u", view_frustum.planes[TFrustum::Near].clipflag);
    if (view_frustum.planes[TFrustum::Near].PointOnSide(LightPos)) {
      newmmat = mmat*VMatrix4::RotateY(180.0f);
      alpha = -1.0f;
    } else {
      alpha = 1.0f;
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(newmmat[0]);
    // create new projection matrix
    VMatrix4 newpmat = pmat;

    //GCon->Logf("alpha=%f", alpha);

    newpmat[0][0] *= alpha;
    /*
    newpmat.SetIdentity();
    newpmat[0][0] = alpha/rd->fovx;
    newpmat[1][1] = 1.0f/rd->fovy;
    newpmat[2][3] = -1.0f;
    newpmat[3][3] = 0.0f;
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      newpmat[2][2] = -1.0f;
      newpmat[3][2] = -2.0f;
    } else {
      newpmat[2][2] = -(maxdist+1.0f)/(maxdist-1.0f);
      newpmat[3][2] = -2.0f*maxdist/(maxdist-1.0f);
    }
    */

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(newpmat[0]);

    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);

    // render near cap (it should be front-facing, so it will do "teh right thing")
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i]);
    glEnd();
    {
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
      p_glUseProgramObjectARB(DrawAutomap_Program);
      //glEnable(GL_LINE_SMOOTH);
      //glEnable(GL_BLEND);
      glColor3f(1.0f, 1.0f, 0.0f);
      glBegin(GL_LINES);
        for (unsigned i = 0; i < vcount; ++i) {
          glVertex(sverts[i]);
          glVertex(sverts[(i+1)%vcount]);
        }
      glEnd();
      glColor3f(1.0f, 1.0f, 1.0f);
      p_glUseProgramObjectARB(SurfShadowVolume_Program);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }

    if (hasBoundsTest && gl_enable_depth_bounds) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

    // restore matrices
    glLoadMatrixf(pmat[0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(mmat[0]);
  }
#endif

  glBegin(GL_TRIANGLE_STRIP);
    for (unsigned i = 0; i < vcount; ++i) {
      glVertex(sverts[i]);
      glVertex4(v[i], 0);
    }
    glVertex(sverts[0]);
    glVertex4(v[0], 0);
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
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Colour, bool doShadow) {
  if (gl_dbg_wireframe) return;
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes
  glDisable(GL_TEXTURE_2D);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  if (gl_dbg_use_zpass > 1) {
    glStencilFunc(GL_EQUAL, 0x1, 0xff);
  } else {
    glStencilFunc(GL_EQUAL, 0x0, 0xff);
  }
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // do not use stencil test if we rendered no shadow surfaces
  if (doShadow && IsStencilBufferDirty()) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
  //glBlendEquation(GL_MAX_EXT);

  glDepthFunc(GL_EQUAL);

  if (!gl_dbg_advlight_debug) {
    ShadowsLight.Activate();
    ShadowsLight.SetLightPos(LightPos);
    ShadowsLight.SetLightRadius(Radius);
    ShadowsLight.SetLightMin(LightMin);
    ShadowsLight.SetLightColour(((Colour>>16)&255)/255.0f, ((Colour>>8)&255)/255.0f, (Colour&255)/255.0f);
    ShadowsLight.SetViewOrigin(vieworg.x, vieworg.y, vieworg.z);
    ShadowsLight.SetTexture(0);
  } else {
    ShadowsLightDbg.Activate();
    ShadowsLightDbg.SetLightPos(LightPos);
    ShadowsLightDbg.SetLightRadius(Radius);
    Colour = gl_dbg_advlight_color;
    ShadowsLightDbg.SetLightColour(((Colour>>16)&255)/255.0f, ((Colour>>8)&255)/255.0f, (Colour&255)/255.0f);
    ShadowsLightDbg.SetViewOrigin(vieworg.x, vieworg.y, vieworg.z);
    ShadowsLightDbg.SetTexture(0);
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
  //if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render light surface with %d vertices", surf->count);
    return;
  }

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  if (!gl_dbg_advlight_debug) {
    ShadowsLight.SetTex(tex);
    ShadowsLight.SetSurfNormal(surf->plane->normal);
    ShadowsLight.SetSurfDist(surf->plane->dist);
  } else {
    ShadowsLightDbg.SetTex(tex);
    ShadowsLightDbg.SetSurfNormal(surf->plane->normal);
    ShadowsLightDbg.SetSurfDist(surf->plane->dist);
  }

  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
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
  if (gl_dbg_wireframe) return;
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  //glBlendEquation(GL_FUNC_ADD);

  // copy ambient light texture to FBO, so we can use it to light decals
  if (p_glBlitFramebuffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ambLightFBO);
    p_glBlitFramebuffer(0, 0, ScreenWidth, ScreenHeight, 0, 0, ScreenWidth, ScreenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
  } else {
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

  bool lastWasMasked = false;
  bool firstMasked = true;

  ShadowsTexture.Activate();
  ShadowsTexture.SetTexture(0);

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

    if (surf->drawflags&surface_t::DF_MASKED) {
      // masked wall
      if (!lastWasMasked) {
        // switch shader
        ShadowsTextureMasked.Activate();
        if (firstMasked) {
          ShadowsTextureMasked.SetTexture(0);
          firstMasked = false;
        }
        lastWasMasked = true;
        textureChanded = true; //FIXME: hold two of those
      }
    } else {
      // normal wall
      if (lastWasMasked) {
        // switch shader
        ShadowsTexture.Activate();
        lastWasMasked = false;
        textureChanded = true; //FIXME: hold two of those
      }
    }

    if (textureChanded) {
      SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
      if (lastWasMasked) ShadowsTextureMasked.SetTex(currTexinfo); else ShadowsTexture.SetTex(currTexinfo);
    }

    bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decals);

    // fill stencil buffer for decals
    if (doDecals) RenderPrepareShaderDecals(surf);

    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
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
        if (lastWasMasked) ShadowsTextureMasked.Activate(); else ShadowsTexture.Activate();
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
  if (gl_dbg_wireframe) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  glDepthMask(GL_FALSE); // no z-buffer writes

  // draw surfaces
  //ShadowsFog.Activate();
  //ShadowsFog.SetFogType();

  if (RendLev->DrawSurfList.length() == 0) return;

  int activated = -1; // -1: none; 0: normal; 1: masked
  bool fadeSet[2] = { false, false };
  vuint32 lastFade[2] = { 0, 0 }; //RendLev->DrawSurfList[0]->Fade;

  /*
  ShadowsFog.SetTexture(0);
  ShadowsFog.SetFogFade(lastFade, 1.0f);
  */

  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  const texinfo_t *lastTexinfo = nullptr;
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

    if (surf->drawflags&surface_t::DF_MASKED) {
      if (activated != 1) {
        activated = 1;
        ShadowsFogMasked.Activate();
        ShadowsFogMasked.SetTexture(0);
      }
      if (!fadeSet[activated] && lastFade[activated] != surf->Fade) {
        fadeSet[activated] = true;
        lastFade[activated] = surf->Fade;
        ShadowsFogMasked.SetFogFade(surf->Fade, 1.0f);
      }

      bool textureChanded =
        !lastTexinfo ||
        lastTexinfo != currTexinfo ||
        lastTexinfo->Tex != currTexinfo->Tex ||
        lastTexinfo->ColourMap != currTexinfo->ColourMap;
      lastTexinfo = currTexinfo;

      if (textureChanded) {
        SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
        ShadowsFogMasked.SetTex(currTexinfo);
      }
    } else {
      if (activated != 0) {
        activated = 0;
        ShadowsFog.Activate();
      }
      if (!fadeSet[activated] && lastFade[activated] != surf->Fade) {
        fadeSet[activated] = true;
        lastFade[activated] = surf->Fade;
        ShadowsFog.SetFogFade(surf->Fade, 1.0f);
      }
    }

    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
  }

  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
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
  RestoreDepthFunc();
}
