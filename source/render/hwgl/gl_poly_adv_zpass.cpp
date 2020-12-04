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
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  R_GlobalPointToLocal
//
//==========================================================================
static VVA_OKUNUSED inline TVec R_GlobalPointToLocal (const VMatrix4 &modelMatrix, const TVec &v) {
  TVec tmp = v-modelMatrix.GetCol(3);
  return modelMatrix.RotateVector(tmp);
}


//==========================================================================
//
//  R_LightProjectionMatrix
//
//==========================================================================
static VVA_OKUNUSED void R_LightProjectionMatrix (VMatrix4 &mat, const TVec &origin, const TPlane &rearPlane) {
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
static VVA_OKUNUSED void R_ProjectPointsToPlane (TVec *dest, const TVec *src, unsigned vcount,
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
//  VOpenGLDrawer::RenderSurfaceShadowVolumeZPassIntr
//
//  all necessary precondition checks are done in the caller
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolumeZPassIntr (const surface_t *surf, const TVec &LightPos, float Radius) {
  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  static SurfVertex *dest = nullptr;
  static unsigned destSize = 0;
  if (destSize < vcount+1) {
    destSize = (vcount|0x7f)+1;
    dest = (SurfVertex *)Z_Realloc(dest, destSize*sizeof(SurfVertex));
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
  const float ldist = znear.PointDistance(LightPos); // from light to znear
  if (ldist <= 0.0f) {
    // on the back, project only if some surface vertices are on the back too
    for (unsigned f = 0; f < vcount; ++f) {
      const float sdist = znear.PointDistance(sverts[f].vec());
      if (sdist >= ldist && sdist <= 0.0f) { doProject = true; break; }
    }
  } else {
    // before camera, project only if some surface vertices are nearer (but not on the back)
    for (unsigned f = 0; f < vcount; ++f) {
      const float sdist = znear.PointDistance(sverts[f].vec());
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
        TVec vv = (sverts[f].vec()-LightPos).normalised();
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
        glVertex(sverts[f].vec());
      }
    glEnd();
#endif

    //if (hasBoundsTest && gl_enable_depth_bounds) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

    p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
    p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

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
    //GCon->Logf("=== %u", viewfrustum.planes[TFrustum::Near].clipflag);
    if (viewfrustum.planes[TFrustum::Near].PointOnSide(LightPos)) {
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
    if (RendLev && RendLev->IsShadowVolumeRenderer() && !HaveDepthClamp) {
      newpmat[2][2] = -1.0f;
      newpmat[3][2] = -2.0f;
    } else {
      newpmat[2][2] = -(gl_maxdist+1.0f)/(gl_maxdist-1.0f);
      newpmat[3][2] = -2.0f*gl_maxdist/(gl_maxdist-1.0f);
    }
    */

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(newpmat[0]);

    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);

    // render near cap (it should be front-facing, so it will do "teh right thing")
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i].vec());
    glEnd();
    {
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
      p_glUseProgramObjectARB(DrawAutomap_Program);
      //glEnable(GL_LINE_SMOOTH);
      //GLEnableBlend();
      glColor3f(1.0f, 1.0f, 0.0f);
      glBegin(GL_LINES);
        for (unsigned i = 0; i < vcount; ++i) {
          glVertex(sverts[i].vec());
          glVertex(sverts[(i+1)%vcount].vec());
        }
      glEnd();
      //glColor3f(1.0f, 1.0f, 1.0f);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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
