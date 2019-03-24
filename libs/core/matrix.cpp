//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"


// ////////////////////////////////////////////////////////////////////////// //
const VMatrix4 VMatrix4::Identity(
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
);


// ////////////////////////////////////////////////////////////////////////// //
/*
//==========================================================================
//
//  glhProjectf
//
//==========================================================================
static inline bool glhProjectf (const TVec &point, const VMatrix4 &modelview, const VMatrix4 &projection, const int *viewport, float *windowCoordinate) {
  // our `w` is always 1
  TVec inworld = modelview.Transform2(point);
  //if (inworld.z >= 0.0f) return false; // the w value
  if (inworld.z > -1.0f) inworld.z = -1.0f; // -znear
  TVec proj = projection.Transform2OnlyXY(inworld); // we don't care about z here
  const float pjw = -1.0f/inworld.z;
  proj.x *= pjw;
  proj.y *= pjw;
  windowCoordinate[0] = (proj.x*0.5f+0.5f)*viewport[2]+viewport[0];
  windowCoordinate[1] = (proj.y*0.5f+0.5f)*viewport[3]+viewport[1];
  return true;
}


//==========================================================================
//
//  glhProjectfZOld
//
//==========================================================================
static inline __attribute__((unused)) float glhProjectfZOld (const TVec &point, const float zofs, const VMatrix4 &modelview, const VMatrix4 &projection) {
#if 0
  TVec inworld = point;
  const float iww = modelview.Transform2InPlace(inworld);
  if (inworld.z >= 0.0f) return 0.0f;
  TVec proj = projection.Transform2(inworld, iww);
#else
  // our `w` is always 1
  TVec inworld = modelview.Transform2(point);
  inworld.z += zofs;
  //if (inworld.z >= 0.0f) return 0.0f;
  //if (inworld.z > -0.001f) inworld.z = -0.001f;
  if (inworld.z > -1.0f) inworld.z = -1.0f;
  float projz = projection.Transform2OnlyZ(inworld);
#endif
  const float pjw = -1.0f/inworld.z;
  //GCon->Logf("iwz=%f; pjw=%f; projz=%f; depthz=%f; calcz=%f", inworld.z, pjw, projz, projz*pjw, (1.0f+projz*pjw)*0.5f);
  projz *= pjw;
  return (1.0f+projz)*0.5f;
}


//==========================================================================
//
//  glhProjectfZ
//
//==========================================================================
static inline __attribute__((unused)) float glhProjectfZ (const TVec &point, const float zofs,
                                                          const VMatrix4 &modelview, const VMatrix4 &projection,
                                                          bool hasClip, bool revZ)
{
  // our `w` is always 1
  TVec inworld = modelview.Transform2(point);
  inworld.z += zofs;
  //if (inworld.z > -0.001f) inworld.z = -0.001f;
  if (inworld.z > -1.0f) inworld.z = -1.0f; // -znear
  float pjw = -1.0f/inworld.z;
  // for reverse z, projz is always 1, so we can simply use pjw
  if (!revZ) {
    const float projz = projection.Transform2OnlyZ(inworld);
    //GCon->Logf("iwz=%f; pjw=%f; projz=%f; depthz=%f; calcz=%f", inworld.z, pjw, projz, projz*pjw, (1.0f+projz*pjw)*0.5f);
    //projz *= pjw;
    pjw *= projz;
  }
  return (hasClip ? pjw : (1.0f+pjw)*0.5f);
}
*/


// ////////////////////////////////////////////////////////////////////////// //
//==========================================================================
//
//  VMatrix4::VMatrix4
//
//==========================================================================
VMatrix4::VMatrix4 (float m00, float m01, float m02, float m03,
  float m10, float m11, float m12, float m13,
  float m20, float m21, float m22, float m23,
  float m30, float m31, float m32, float m33)
{
  m[0][0] = m00;
  m[0][1] = m01;
  m[0][2] = m02;
  m[0][3] = m03;
  m[1][0] = m10;
  m[1][1] = m11;
  m[1][2] = m12;
  m[1][3] = m13;
  m[2][0] = m20;
  m[2][1] = m21;
  m[2][2] = m22;
  m[2][3] = m23;
  m[3][0] = m30;
  m[3][1] = m31;
  m[3][2] = m32;
  m[3][3] = m33;
}


/*
//==========================================================================
//
//  VMatrix4::getRow
//
//==========================================================================
TVec VMatrix4::getRow (int idx) const {
  if (idx < 0 || idx > 3) return TVec(0, 0, 0);
  return TVec(m[0][idx], m[1][idx], m[2][idx]);
}


//==========================================================================
//
//  VMatrix4::getCol
//
//==========================================================================
TVec VMatrix4::getCol (int idx) const {
  if (idx < 0 || idx > 3) return TVec(0, 0, 0);
  return TVec(m[idx][0], m[idx][1], m[idx][2]);
}
*/


//==========================================================================
//
//  operator VMatrix4 * VMatrix4
//
//==========================================================================
/*
VMatrix4 VMatrix4::operator * (const VMatrix4 &in2) const {
  VMatrix4 out;
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      out[i][j] = VSUM4(m[i][0]*in2.m[0][j], m[i][1]*in2.m[1][j], m[i][2]*in2.m[2][j], m[i][3]*in2.m[3][j]);
    }
  }
  return out;
}
*/


//==========================================================================
//
//  MINOR
//
//==========================================================================
inline static float MINOR (const VMatrix4 &m,
  const size_t r0, const size_t r1, const size_t r2,
  const size_t c0, const size_t c1, const size_t c2)
{
  return
    VSUM3(
        m[r0][c0]*VSUM2(m[r1][c1]*m[r2][c2], -(m[r2][c1]*m[r1][c2])),
      -(m[r0][c1]*VSUM2(m[r1][c0]*m[r2][c2], -(m[r2][c0]*m[r1][c2]))),
        m[r0][c2]*VSUM2(m[r1][c0]*m[r2][c1], -(m[r2][c0]*m[r1][c1])));
  ;
}


//==========================================================================
//
//  VMatrix4::Determinant
//
//==========================================================================
float VMatrix4::Determinant () const {
  return
    VSUM4(
      m[0][0]*MINOR(*this, 1, 2, 3, 1, 2, 3),
    -(m[0][1]*MINOR(*this, 1, 2, 3, 0, 2, 3)),
      m[0][2]*MINOR(*this, 1, 2, 3, 0, 1, 3),
    -(m[0][3]*MINOR(*this, 1, 2, 3, 0, 1, 2)))
  ;
}


//==========================================================================
//
//  VMatrix4::Inverse
//
//==========================================================================
VMatrix4 VMatrix4::Inverse () const {
  const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
  const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
  const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
  const float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

  float v0 = VSUM2(m20*m31, -(m21*m30));
  float v1 = VSUM2(m20*m32, -(m22*m30));
  float v2 = VSUM2(m20*m33, -(m23*m30));
  float v3 = VSUM2(m21*m32, -(m22*m31));
  float v4 = VSUM2(m21*m33, -(m23*m31));
  float v5 = VSUM2(m22*m33, -(m23*m32));

  const float t00 = +VSUM3(v5*m11, -(v4*m12), v3*m13);
  const float t10 = -VSUM3(v5*m10, -(v2*m12), v1*m13);
  const float t20 = +VSUM3(v4*m10, -(v2*m11), v0*m13);
  const float t30 = -VSUM3(v3*m10, -(v1*m11), v0*m12);

  float invDet = 1.0f/VSUM4(t00*m00, t10*m01, t20*m02, t30*m03);
  if (!isFiniteF(invDet)) invDet = 0.0f;

  const float d00 = t00*invDet;
  const float d10 = t10*invDet;
  const float d20 = t20*invDet;
  const float d30 = t30*invDet;

  const float d01 = -VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d11 = +VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d21 = -VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d31 = +VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VSUM2(m10*m31, -(m11*m30));
  v1 = VSUM2(m10*m32, -(m12*m30));
  v2 = VSUM2(m10*m33, -(m13*m30));
  v3 = VSUM2(m11*m32, -(m12*m31));
  v4 = VSUM2(m11*m33, -(m13*m31));
  v5 = VSUM2(m12*m33, -(m13*m32));

  const float d02 = +VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d12 = -VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d22 = +VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d32 = -VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VSUM2(m21*m10, -(m20*m11));
  v1 = VSUM2(m22*m10, -(m20*m12));
  v2 = VSUM2(m23*m10, -(m20*m13));
  v3 = VSUM2(m22*m11, -(m21*m12));
  v4 = VSUM2(m23*m11, -(m21*m13));
  v5 = VSUM2(m23*m12, -(m22*m13));

  const float d03 = -VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d13 = +VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d23 = -VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d33 = +VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  return VMatrix4(
    d00, d01, d02, d03,
    d10, d11, d12, d13,
    d20, d21, d22, d23,
    d30, d31, d32, d33);
}


//==========================================================================
//
//  VMatrix4::Transpose
//
//==========================================================================
VMatrix4 VMatrix4::Transpose () const {
  VMatrix4 Out;
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      Out[j][i] = m[i][j];
    }
  }
  return Out;
}


//==========================================================================
//
//  VMatrix4::CombineAndExtractFrustum
//
//==========================================================================
void VMatrix4::CombineAndExtractFrustum (const VMatrix4 &model, const VMatrix4 &proj, TPlane planes[6]) {
  const float *projm = (const float *)proj.m;
  const float *modlm = (const float *)model.m;
  float clip[16];

  // combine the two matrices (multiply projection by modelview)
  clip[ 0] = VSUM4(modlm[ 0]*projm[ 0], modlm[ 1]*projm[ 4], modlm[ 2]*projm[ 8], modlm[ 3]*projm[12]);
  clip[ 1] = VSUM4(modlm[ 0]*projm[ 1], modlm[ 1]*projm[ 5], modlm[ 2]*projm[ 9], modlm[ 3]*projm[13]);
  clip[ 2] = VSUM4(modlm[ 0]*projm[ 2], modlm[ 1]*projm[ 6], modlm[ 2]*projm[10], modlm[ 3]*projm[14]);
  clip[ 3] = VSUM4(modlm[ 0]*projm[ 3], modlm[ 1]*projm[ 7], modlm[ 2]*projm[11], modlm[ 3]*projm[15]);

  clip[ 4] = VSUM4(modlm[ 4]*projm[ 0], modlm[ 5]*projm[ 4], modlm[ 6]*projm[ 8], modlm[ 7]*projm[12]);
  clip[ 5] = VSUM4(modlm[ 4]*projm[ 1], modlm[ 5]*projm[ 5], modlm[ 6]*projm[ 9], modlm[ 7]*projm[13]);
  clip[ 6] = VSUM4(modlm[ 4]*projm[ 2], modlm[ 5]*projm[ 6], modlm[ 6]*projm[10], modlm[ 7]*projm[14]);
  clip[ 7] = VSUM4(modlm[ 4]*projm[ 3], modlm[ 5]*projm[ 7], modlm[ 6]*projm[11], modlm[ 7]*projm[15]);

  clip[ 8] = VSUM4(modlm[ 8]*projm[ 0], modlm[ 9]*projm[ 4], modlm[10]*projm[ 8], modlm[11]*projm[12]);
  clip[ 9] = VSUM4(modlm[ 8]*projm[ 1], modlm[ 9]*projm[ 5], modlm[10]*projm[ 9], modlm[11]*projm[13]);
  clip[10] = VSUM4(modlm[ 8]*projm[ 2], modlm[ 9]*projm[ 6], modlm[10]*projm[10], modlm[11]*projm[14]);
  clip[11] = VSUM4(modlm[ 8]*projm[ 3], modlm[ 9]*projm[ 7], modlm[10]*projm[11], modlm[11]*projm[15]);

  clip[12] = VSUM4(modlm[12]*projm[ 0], modlm[13]*projm[ 4], modlm[14]*projm[ 8], modlm[15]*projm[12]);
  clip[13] = VSUM4(modlm[12]*projm[ 1], modlm[13]*projm[ 5], modlm[14]*projm[ 9], modlm[15]*projm[13]);
  clip[14] = VSUM4(modlm[12]*projm[ 2], modlm[13]*projm[ 6], modlm[14]*projm[10], modlm[15]*projm[14]);
  clip[15] = VSUM4(modlm[12]*projm[ 3], modlm[13]*projm[ 7], modlm[14]*projm[11], modlm[15]*projm[15]);

  planes[TFrustum::Right].SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
  planes[TFrustum::Left].SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
  planes[TFrustum::Bottom].SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
  planes[TFrustum::Top].SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
  planes[TFrustum::Far].SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
  planes[TFrustum::Near].SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumLeft
//
//  combine the two matrices (multiply projection by modelview)
//
//==========================================================================
void VMatrix4::ModelProjectCombine (const VMatrix4 &model, const VMatrix4 &proj) {
  const float *projm = (const float *)proj.m;
  const float *modlm = (const float *)model.m;
  float *clip = (float *)m;

  // combine the two matrices (multiply projection by modelview)
  clip[ 0] = VSUM4(modlm[ 0]*projm[ 0], modlm[ 1]*projm[ 4], modlm[ 2]*projm[ 8], modlm[ 3]*projm[12]);
  clip[ 1] = VSUM4(modlm[ 0]*projm[ 1], modlm[ 1]*projm[ 5], modlm[ 2]*projm[ 9], modlm[ 3]*projm[13]);
  clip[ 2] = VSUM4(modlm[ 0]*projm[ 2], modlm[ 1]*projm[ 6], modlm[ 2]*projm[10], modlm[ 3]*projm[14]);
  clip[ 3] = VSUM4(modlm[ 0]*projm[ 3], modlm[ 1]*projm[ 7], modlm[ 2]*projm[11], modlm[ 3]*projm[15]);

  clip[ 4] = VSUM4(modlm[ 4]*projm[ 0], modlm[ 5]*projm[ 4], modlm[ 6]*projm[ 8], modlm[ 7]*projm[12]);
  clip[ 5] = VSUM4(modlm[ 4]*projm[ 1], modlm[ 5]*projm[ 5], modlm[ 6]*projm[ 9], modlm[ 7]*projm[13]);
  clip[ 6] = VSUM4(modlm[ 4]*projm[ 2], modlm[ 5]*projm[ 6], modlm[ 6]*projm[10], modlm[ 7]*projm[14]);
  clip[ 7] = VSUM4(modlm[ 4]*projm[ 3], modlm[ 5]*projm[ 7], modlm[ 6]*projm[11], modlm[ 7]*projm[15]);

  clip[ 8] = VSUM4(modlm[ 8]*projm[ 0], modlm[ 9]*projm[ 4], modlm[10]*projm[ 8], modlm[11]*projm[12]);
  clip[ 9] = VSUM4(modlm[ 8]*projm[ 1], modlm[ 9]*projm[ 5], modlm[10]*projm[ 9], modlm[11]*projm[13]);
  clip[10] = VSUM4(modlm[ 8]*projm[ 2], modlm[ 9]*projm[ 6], modlm[10]*projm[10], modlm[11]*projm[14]);
  clip[11] = VSUM4(modlm[ 8]*projm[ 3], modlm[ 9]*projm[ 7], modlm[10]*projm[11], modlm[11]*projm[15]);

  clip[12] = VSUM4(modlm[12]*projm[ 0], modlm[13]*projm[ 4], modlm[14]*projm[ 8], modlm[15]*projm[12]);
  clip[13] = VSUM4(modlm[12]*projm[ 1], modlm[13]*projm[ 5], modlm[14]*projm[ 9], modlm[15]*projm[13]);
  clip[14] = VSUM4(modlm[12]*projm[ 2], modlm[13]*projm[ 6], modlm[14]*projm[10], modlm[15]*projm[14]);
  clip[15] = VSUM4(modlm[12]*projm[ 3], modlm[13]*projm[ 7], modlm[14]*projm[11], modlm[15]*projm[15]);
}


//==========================================================================
//
//  VMatrix4::ExtractFrustum
//
//==========================================================================
void VMatrix4::ExtractFrustum (TPlane planes[6]) const {
  const float *clip = (const float *)m;
  planes[TFrustum::Right].SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
  planes[TFrustum::Left].SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
  planes[TFrustum::Bottom].SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
  planes[TFrustum::Top].SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
  planes[TFrustum::Far].SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
  planes[TFrustum::Near].SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumLeft
//
//==========================================================================
void VMatrix4::ExtractFrustumLeft (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumRight
//
//==========================================================================
void VMatrix4::ExtractFrustumRight (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumTop
//
//==========================================================================
void VMatrix4::ExtractFrustumTop (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumBottom
//
//==========================================================================
void VMatrix4::ExtractFrustumBottom (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumFar
//
//==========================================================================
void VMatrix4::ExtractFrustumFar (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumNear
//
//==========================================================================
void VMatrix4::ExtractFrustumNear (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}


// ////////////////////////////////////////////////////////////////////////// //
VMatrix4 VMatrix4::RotateX (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  VMatrix4 res;
  res.SetZero();
  res.m[0][0] = 1.0f;
  res.m[1][1] = c;
  res.m[2][1] = -s;
  res.m[1][2] = s;
  res.m[2][2] = c;
  res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::RotateY (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  VMatrix4 res;
  res.SetZero();
  res.m[0][0] = c;
  res.m[2][0] = s;
  res.m[1][1] = 1.0f;
  res.m[0][2] = -s;
  res.m[2][2] = c;
  res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::RotateZ (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  VMatrix4 res;
  res.SetZero();
  res.m[0][0] = c;
  res.m[1][0] = -s;
  res.m[0][1] = s;
  res.m[1][1] = c;
  res.m[2][2] = 1.0f;
  res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::Translate (const TVec &v) {
  VMatrix4 res;
  res.SetIdentity();
  //res.m[0][0] = res.m[1][1] = res.m[2][2] = 1.0f;
  res.m[3][0] = v.x;
  res.m[3][1] = v.y;
  res.m[3][2] = v.z;
  //res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::TranslateNeg (const TVec &v) {
  VMatrix4 res;
  res.SetIdentity();
  //res.m[0][0] = res.m[1][1] = res.m[2][2] = 1;
  res.m[3][0] = -v.x;
  res.m[3][1] = -v.y;
  res.m[3][2] = -v.z;
  //res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::Scale (const TVec &v) {
  VMatrix4 res;
  res.SetIdentity();
  res.m[0][0] = v.x;
  res.m[1][1] = v.y;
  res.m[2][2] = v.z;
  //res.m[3][3] = 1.0f;
  return res;
}

VMatrix4 VMatrix4::Rotate (const TVec &v) {
  auto mx = VMatrix4::RotateX(v.x);
  auto my = VMatrix4::RotateY(v.y);
  auto mz = VMatrix4::RotateZ(v.z);
  return mz*my*mx;
}

// for camera; x is pitch (up/down); y is yaw (left/right); z is roll (tilt)
VMatrix4 VMatrix4::RotateZXY (const TVec &v) {
  auto mx = VMatrix4::RotateX(v.x);
  auto my = VMatrix4::RotateY(v.y);
  auto mz = VMatrix4::RotateZ(v.z);
  return mz*mx*my;
}

// for camera; x is pitch (up/down); y is yaw (left/right); z is roll (tilt)
VMatrix4 VMatrix4::RotateZXY (const TAVec &v) {
  auto mx = VMatrix4::RotateX(v.pitch);
  auto my = VMatrix4::RotateY(v.yaw);
  auto mz = VMatrix4::RotateZ(v.roll);
  return mz*mx*my;
}

// same as `glFrustum()`
VMatrix4 VMatrix4::Frustum (float left, float right, float bottom, float top, float nearVal, float farVal) {
  VMatrix4 res;
  res.SetZero();
  res.m[0][0] = 2.0f*nearVal/(right-left);
  res.m[1][1] = 2.0f*nearVal/(top-bottom); //[5]
  res.m[2][0] = (right+left)/(right-left); //[8]
  res.m[2][1] = (top+bottom)/(top-bottom); //[9]
  res.m[2][2] = -(farVal+nearVal)/(farVal-nearVal); //[10]
  res.m[2][3] = -1.0f; //[11]
  res.m[3][2] = -(2.0f*farVal*nearVal)/(farVal-nearVal); //[14]
  //res.m[3][3] = 0.0f; //[15]
  return res;
}

// same as `glOrtho()`
VMatrix4 VMatrix4::Ortho (float left, float right, float bottom, float top, float nearVal, float farVal) {
  VMatrix4 res;
  res.SetZero();
  res.m[0][0] = 2.0f/(right-left);
  res.m[1][1] = 2.0f/(top-bottom);
  res.m[2][2] = -2.0f/(farVal-nearVal);
  res.m[3][0] = -(right+left)/(right-left);
  res.m[3][1] = -(top+bottom)/(top-bottom);
  res.m[3][2] = -(farVal+nearVal)/(farVal-nearVal);
  return res;
}

// same as `gluPerspective()`
// sets the frustum to perspective mode
// fovY   - Field of vision in degrees in the y direction
// aspect - Aspect ratio of the viewport
// zNear  - The near clipping distance
// zFar   - The far clipping distance
VMatrix4 VMatrix4::Perspective (float fovY, float aspect, float zNear, float zFar) {
  const float fH = tanf(fovY/360.0f*M_PI)*zNear;
  const float fW = fH*aspect;
  return Frustum(-fW, fW, -fH, fH, zNear, zFar);
}

VMatrix4 VMatrix4::LookAtFucked (const TVec &eye, const TVec &center, const TVec &up) {
  // compute vector `N = EP-VRP` and normalize `N`
  TVec n = eye-center;
  n.normaliseInPlace();

  // compute vector `V = UP-VRP`
  // make vector `V` orthogonal to `N` and normalize `V`
  TVec v = up-center;
  const float dp = v.dot(n); //dp = (float)V3Dot(&v,&n);
  //v.x -= dp * n.x; v.y -= dp * n.y; v.z -= dp * n.z;
  v -= n*dp;
  v.normaliseInPlace();

  // compute vector `U = V x N` (cross product)
  const TVec u = v.cross(n);

  // write the vectors `U`, `V`, and `N` as the first three rows of first, second, and third columns of transformation matrix
  VMatrix4 m;

  m.m[0][0] = u.x;
  m.m[1][0] = u.y;
  m.m[2][0] = u.z;
  //m.mt.ptr[3*4+0] = 0;

  m.m[0][1] = v.x;
  m.m[1][1] = v.y;
  m.m[2][1] = v.z;
  //m.mt.ptr[3*4+1] = 0;

  m.m[0][2] = n.x;
  m.m[1][2] = n.y;
  m.m[2][2] = n.z;
  //m.mt.ptr[3*4+2] = 0;

  // compute the fourth row of transformation matrix to include the translation of `VRP` to the origin
  m.m[3][0] = -u.x*center.x-u.y*center.y-u.z*center.z;
  m.m[3][1] = -v.x*center.x-v.y*center.y-v.z*center.z;
  m.m[3][2] = -n.x*center.x-n.y*center.y-n.z*center.z;
  m.m[3][3] = 1;

  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      if (fabsf(m.m[i][j]) < 0.0001f) m.m[i][j] = 0;
    }
  }

  return m;
}

// does `gluLookAt()`
VMatrix4 VMatrix4::lookAt (const TVec &eye, const TVec &center, const TVec &up) const {
  VMatrix4 m ;
  float x[3];
  float y[3];
  float z[3];
  float mag;
  // make rotation matrix
  // Z vector
  z[0] = eye.x-center.x;
  z[1] = eye.y-center.y;
  z[2] = eye.z-center.z;
#ifdef USE_FAST_INVSQRT
  mag = z[0]*z[0]+z[1]*z[1]+z[2]*z[2];
  if (mag != 0.0f) {
    mag = fastInvSqrtf(mag);
    z[0] *= mag;
    z[1] *= mag;
    z[2] *= mag;
  } else {
    z[0] = z[1] = z[2] = 0.0f;
  }
#else
  mag = sqrtf(z[0]*z[0]+z[1]*z[1]+z[2]*z[2]);
  if (mag != 0.0f) { z[0] /= mag; z[1] /= mag; z[2] /= mag; }
#endif
  // Y vector
  y[0] = up.x;
  y[1] = up.y;
  y[2] = up.z;
  // X vector = Y cross Z
  x[0] =  y[1]*z[2]-y[2]*z[1];
  x[1] = -y[0]*z[2]+y[2]*z[0];
  x[2] =  y[0]*z[1]-y[1]*z[0];
  // Recompute Y = Z cross X
  y[0] =  z[1]*x[2]-z[2]*x[1];
  y[1] = -z[0]*x[2]+z[2]*x[0];
  y[2] =  z[0]*x[1]-z[1]*x[0];

  /* cross product gives area of parallelogram, which is < 1.0 for
   * non-perpendicular unit-length vectors; so normalize x, y here
   */
#ifdef USE_FAST_INVSQRT
  mag = x[0]*x[0]+x[1]*x[1]+x[2]*x[2];
  if (mag != 0.0f) {
    mag = fastInvSqrtf(mag);
    x[0] *= mag;
    x[1] *= mag;
    x[2] *= mag;
  } else {
    x[0] = x[1] = x[2] = 0.0f;
  }
#else
  mag = sqrtf(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
  if (mag != 0.0f) { x[0] /= mag; x[1] /= mag; x[2] /= mag; }
#endif

#ifdef USE_FAST_INVSQRT
  mag = y[0]*y[0]+y[1]*y[1]+y[2]*y[2];
  if (mag != 0.0f) {
    mag = fastInvSqrtf(mag);
    y[0] *= mag;
    y[1] *= mag;
    y[2] *= mag;
  } else {
    y[0] = y[1] = y[2] = 0.0f;
  }
#else
  mag = sqrtf(y[0]*y[0]+y[1]*y[1]+y[2]*y[2]);
  if (mag != 0.0f) { y[0] /= mag; y[1] /= mag; y[2] /= mag; }
#endif

  m.m[0][0] = x[0];
  m.m[1][0] = x[1];
  m.m[2][0] = x[2];
  m.m[3][0] = 0.0f;
  m.m[0][1] = y[0];
  m.m[1][1] = y[1];
  m.m[2][1] = y[2];
  m.m[3][1] = 0.0f;
  m.m[0][2] = z[0];
  m.m[1][2] = z[1];
  m.m[2][2] = z[2];
  m.m[3][2] = 0.0f;
  m.m[0][3] = 0.0f;
  m.m[1][3] = 0.0f;
  m.m[2][3] = 0.0f;
  m.m[3][3] = 1.0f;

  // move, and translate Eye to Origin
  return (*this)*m*Translate(-eye);
}

// rotate matrix to face along the target direction
// this function will clear the previous rotation and scale, but it will keep the previous translation
// it is for rotating object to look at the target, NOT for camera
VMatrix4 &VMatrix4::lookingAt (const TVec &target) {
  TVec position(m[3][0], m[3][1], m[3][2]);
  TVec forward = (target-position).normalised();
  TVec up(0, 0, 0);
  if (fabsf(forward.x) < 0.0001f && fabsf(forward.z) < 0.0001f) {
    up.z = (forward.y > 0.0f ? -1.0f : 1.0f);
  } else {
    up.y = 1.0f;
  }
  TVec left = up.cross(forward).normalised();
  up = forward.cross(left).normalised(); //k8: `normalized` was commented out; why?
  m[0][0] = left.x;
  m[0][1] = left.y;
  m[0][2] = left.z;
  m[1][0] = up.x;
  m[1][1] = up.y;
  m[1][2] = up.z;
  m[2][0] = forward.x;
  m[2][1] = forward.y;
  m[2][2] = forward.z;
  return *this;
}

VMatrix4 &VMatrix4::lookingAt (const TVec &target, const TVec &upVec) {
  TVec position = TVec(m[3][0], m[3][1], m[3][2]);
  TVec forward = (target-position).normalised();
  TVec left = upVec.cross(forward).normalised();
  TVec up = forward.cross(left).normalised();
  m[0][0] = left.x;
  m[0][1] = left.y;
  m[0][2] = left.z;
  m[1][0] = up.x;
  m[1][1] = up.y;
  m[1][2] = up.z;
  m[2][0] = forward.x;
  m[2][1] = forward.y;
  m[2][2] = forward.z;
  return *this;
}

VMatrix4 &VMatrix4::rotate (float angle, const TVec &axis) {
  float s, c;
  msincos(angle, &s, &c);
  const float c1 = 1-c;
  const float m0 = m[0][0], m4 = m[1][0], m8 = m[2][0], m12 = m[3][0];
  const float m1 = m[0][1], m5 = m[1][1], m9 = m[2][1], m13 = m[3][1];
  const float m2 = m[0][2], m6 = m[1][2], m10 = m[2][2], m14 = m[3][2];

  // build rotation matrix
  const float r0 = axis.x*axis.x*c1+c;
  const float r1 = axis.x*axis.y*c1+axis.z*s;
  const float r2 = axis.x*axis.z*c1-axis.y*s;
  const float r4 = axis.x*axis.y*c1-axis.z*s;
  const float r5 = axis.y*axis.y*c1+c;
  const float r6 = axis.y*axis.z*c1+axis.x*s;
  const float r8 = axis.x*axis.z*c1+axis.y*s;
  const float r9 = axis.y*axis.z*c1-axis.x*s;
  const float r10= axis.z*axis.z*c1+c;

  // multiply rotation matrix
  m[0][0] = r0*m0+r4*m1+r8*m2;
  m[0][1] = r1*m0+r5*m1+r9*m2;
  m[0][2] = r2*m0+r6*m1+r10*m2;
  m[1][0] = r0*m4+r4*m5+r8*m6;
  m[1][1] = r1*m4+r5*m5+r9*m6;
  m[1][2] = r2*m4+r6*m5+r10*m6;
  m[2][0] = r0*m8+r4*m9+r8*m10;
  m[2][1] = r1*m8+r5*m9+r9*m10;
  m[2][2] = r2*m8+r6*m9+r10*m10;
  m[3][0] = r0*m12+r4*m13+r8*m14;
  m[3][1] = r1*m12+r5*m13+r9*m14;
  m[3][2] = r2*m12+r6*m13+r10*m14;

  return *this;
}

VMatrix4 &VMatrix4::rotateX (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  const float m1 = m[0][1], m2 = m[0][2];
  const float m5 = m[1][1], m6 = m[1][2];
  const float m9 = m[2][1], m10 = m[2][2];
  const float m13 = m[3][1], m14 = m[3][2];

  m[0][1] = m1*c+m2*-s;
  m[0][2] = m1*s+m2*c;
  m[1][1] = m5*c+m6*-s;
  m[1][2] = m5*s+m6*c;
  m[2][1] = m9*c+m10*-s;
  m[2][2] = m9*s+m10*c;
  m[3][1] = m13*c+m14*-s;
  m[3][2] = m13*s+m14*c;

  return *this;
}

VMatrix4 &VMatrix4::rotateY (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  const float m0 = m[0][0], m2 = m[0][2];
  const float m4 = m[1][0], m6 = m[1][2];
  const float m8 = m[2][0], m10 = m[2][2];
  const float m12 = m[3][0], m14 = m[3][2];

  m[0][0] = m0*c+m2*s;
  m[0][2] = m0*-s+m2*c;
  m[1][0] = m4*c+m6*s;
  m[1][2] = m4*-s+m6*c;
  m[2][0] = m8*c+m10*s;
  m[2][2] = m8*-s+m10*c;
  m[3][0] = m12*c+m14*s;
  m[3][2] = m12*-s+m14*c;

  return *this;
}

VMatrix4 &VMatrix4::rotateZ (float angle) {
  float s, c;
  msincos(angle, &s, &c);
  const float m0 = m[0][0], m1 = m[0][1];
  const float m4 = m[1][0], m5 = m[1][1];
  const float m8 = m[2][0], m9 = m[2][1];
  const float m12 = m[3][0], m13 = m[3][1];

  m[0][0] = m0*c+m1*-s;
  m[0][1] = m0*s+m1*c;
  m[1][0] = m4*c+m5*-s;
  m[1][1] = m4*s+m5*c;
  m[2][0] = m8*c+m9*-s;
  m[2][1] = m8*s+m9*c;
  m[3][0] = m12*c+m13*-s;
  m[3][1] = m12*s+m13*c;

  return *this;
}

/*
//k8: wtf is this?!
VMatrix4 &VMatrix4::translate() (const TVec &v) {
  m[0][0] += m[0][3]*v.x; m[1][0] += m[1][3]*v.x; m[2][0] += m[2][3]*v.x; m[3][0] += m[3][3]*v.x;
  m[0][1] += m[0][3]*v.y; m[1][1] += m[1][3]*v.y; m[2][1] += m[2][3]*v.y; m[3][1] += m[3][3]*v.y;
  m[0][2] += m[0][3]*v.z; m[1][2] += m[1][3]*v.z; m[2][2] += m[2][3]*v.z; m[3][2] += m[3][3]*v.z;
  return *this;
}

VMatrix4 &VMatrix4::translateNeg() (const TVec &v) {
  m[0][0] -= m[0][3]*v.x; m[1][0] -= m[1][3]*v.x; m[2][0] -= m[2][3]*v.x; m[3][0] -= m[3][3]*v.x;
  m[0][1] -= m[0][3]*v.y; m[1][1] -= m[1][3]*v.y; m[2][1] -= m[2][3]*v.y; m[3][1] -= m[3][3]*v.y;
  m[0][2] -= m[0][3]*v.z; m[1][2] -= m[1][3]*v.z; m[2][2] -= m[2][3]*v.z; m[3][2] -= m[3][3]*v.z;
  return *this;
}
*/

/*
ref mat4 translate() (in auto ref vec3 v) {
  mt.ptr[12] += v.x;
  mt.ptr[13] += v.y;
  mt.ptr[14] += v.z;
  return this;
}

ref mat4 translateNeg() (in auto ref vec3 v) {
  mt.ptr[12] -= v.x;
  mt.ptr[13] -= v.y;
  mt.ptr[14] -= v.z;
  return this;
}
*/

/*
//k8: wtf is this?!
ref VMatrix4 scale() (const TVec &v) {
  m[0][0] *= v.x; m[1][0] *= v.x; m[2][0] *= v.x; m[3][0] *= v.x;
  m[0][1] *= v.y; m[1][1] *= v.y; m[2][1] *= v.y; m[3][1] *= v.y;
  m[0][2] *= v.z; m[1][2] *= v.z; m[2][2] *= v.z; m[3][2] *= v.z;
  return this;
}
*/

/*
  inline VMatrix4 translated (const TVec &v) const { pragma(inline, true); auto res = VMatrix4(this); return res.translate(v); }
  inline VMatrix4 translatedNeg (const TVec &v) const { pragma(inline, true); auto res = VMatrix4(this); return res.translateNeg(v); }
  inline VMatrix4 scaled (const TVec &v) const { pragma(inline, true); auto res = VMatrix4(this); return res.scale(v); }
*/

// retrieve angles in degree from rotation matrix, M = Rx*Ry*Rz, in degrees
// Rx: rotation about X-axis, pitch
// Ry: rotation about Y-axis, yaw (heading)
// Rz: rotation about Z-axis, roll
TAVec VMatrix4::getAngles () const {
  float pitch, roll;
  float yaw = RAD2DEGF(asinf(m[2][0]));
  if (m[2][2] < 0.0f) {
    if (yaw >= 0.0f) yaw = 180.0f-yaw; else yaw = -180.0f-yaw;
  }
  if (m[0][0] > -0.0001f && m[0][0] < 0.0001f) {
    roll = 0.0f;
    pitch = RAD2DEGF(atan2f(m[0][1], m[1][1]));
  } else {
    roll = RAD2DEGF(atan2(-m[1][0], m[0][0]));
    pitch = RAD2DEGF(atan2f(-m[2][1], m[2][2]));
  }
  return TAVec(pitch, yaw, roll);
}

// blends two matrices together, at a given percentage (range is [0..1]), blend==0: m2 is ignored
// WARNING! it won't sanitize `blend`
VMatrix4 VMatrix4::blended (const VMatrix4 &m2, float blend) const {
  const float ib = 1.0f-blend;
  VMatrix4 res;
  res.m[0][0] = m[0][0]*ib+m2.m[0][0]*blend;
  res.m[0][1] = m[0][1]*ib+m2.m[0][1]*blend;
  res.m[0][2] = m[0][2]*ib+m2.m[0][2]*blend;
  res.m[0][3] = m[0][3]*ib+m2.m[0][3]*blend;
  res.m[1][0] = m[1][0]*ib+m2.m[1][0]*blend;
  res.m[1][1] = m[1][1]*ib+m2.m[1][1]*blend;
  res.m[1][2] = m[1][2]*ib+m2.m[1][2]*blend;
  res.m[1][3] = m[1][3]*ib+m2.m[1][3]*blend;
  res.m[2][0] = m[2][0]*ib+m2.m[2][0]*blend;
  res.m[2][1] = m[2][1]*ib+m2.m[2][1]*blend;
  res.m[2][2] = m[2][2]*ib+m2.m[2][2]*blend;
  res.m[2][3] = m[2][3]*ib+m2.m[2][3]*blend;
  res.m[3][0] = m[3][0]*ib+m2.m[3][0]*blend;
  res.m[3][1] = m[3][1]*ib+m2.m[3][1]*blend;
  res.m[3][2] = m[3][2]*ib+m2.m[3][2]*blend;
  res.m[3][3] = m[3][3]*ib+m2.m[3][3]*blend;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// compute cofactor of 3x3 minor matrix without sign
// input params are 9 elements of the minor matrix
// NOTE: The caller must know its sign
static inline float getCofactor (float m0, float m1, float m2, float m3, float m4, float m5, float m6, float m7, float m8) {
  return m0*(m4*m8-m5*m7)-m1*(m3*m8-m5*m6)+m2*(m3*m7-m4*m6);
}

static inline float determinant (const float m[4][4]) {
  return m[0][0]*getCofactor(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3])-
         m[0][1]*getCofactor(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3])+
         m[0][2]*getCofactor(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3])-
         m[0][3]*getCofactor(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
}


// ////////////////////////////////////////////////////////////////////////// //
//WARNING: this must be tested for row/col
// partially ;-) taken from DarkPlaces
// this assumes uniform scaling
VMatrix4 VMatrix4::invertedSimple () const {
  // we only support uniform scaling, so assume the first row is enough
  // (note the lack of sqrt here, because we're trying to undo the scaling,
  // this means multiplying by the inverse scale twice - squaring it, which
  // makes the sqrt a waste of time)
#if 1
  const float scale = 1.0f/VSUM3(m[0][0]*m[0][0], m[1][0]*m[1][0], m[2][0]*m[2][0]);
#else
  mixin(ImportCoreMath!(float, "sqrt"));
  float scale = 3.0f/sqrt(
    m[0][0]*m[0][0]+m[1][0]*m[1][0]+m[2][0]*m[2][0]+
    m[0][1]*m[0][1]+m[1][1]*m[1][1]+m[2][1]*m[2][1]+
    m[0][2]*m[0][2]+m[1][2]*m[1][2]+m[2][2]*m[2][2]
  );
  scale *= scale;
#endif

  VMatrix4 res;

  // invert the rotation by transposing and multiplying by the squared recipricol of the input matrix scale as described above
  res.m[0][0] = m[0][0]*scale;
  res.m[1][0] = m[0][1]*scale;
  res.m[2][0] = m[0][2]*scale;
  res.m[0][1] = m[1][0]*scale;
  res.m[1][1] = m[1][1]*scale;
  res.m[2][1] = m[1][2]*scale;
  res.m[0][2] = m[2][0]*scale;
  res.m[1][2] = m[2][1]*scale;
  res.m[2][2] = m[2][2]*scale;

  // invert the translate
  res.m[3][0] = -VSUM3(m[3][0]*res.m[0][0], m[3][1]*res.m[1][0], m[3][2]*res.m[2][0]);
  res.m[3][1] = -VSUM3(m[3][0]*res.m[0][1], m[3][1]*res.m[1][1], m[3][2]*res.m[2][1]);
  res.m[3][2] = -VSUM3(m[3][0]*res.m[0][2], m[3][1]*res.m[1][2], m[3][2]*res.m[2][2]);

  // don't know if there's anything worth doing here
  res.m[0][3] = 0.0f;
  res.m[1][3] = 0.0f;
  res.m[2][3] = 0.0f;
  res.m[3][3] = 1.0f;

  return res;
}

// ////////////////////////////////////////////////////////////////////////////
// compute the inverse of 4x4 Euclidean transformation matrix
//
// Euclidean transformation is translation, rotation, and reflection.
// With Euclidean transform, only the position and orientation of the object
// will be changed. Euclidean transform does not change the shape of an object
// (no scaling). Length and angle are prereserved.
//
// Use inverseAffine() if the matrix has scale and shear transformation.
VMatrix4 &VMatrix4::invertEuclidean () {
  float tmp;
  tmp = m[0][1]; m[0][1] = m[1][0]; m[1][0] = tmp;
  tmp = m[0][2]; m[0][2] = m[2][0]; m[2][0] = tmp;
  tmp = m[1][2]; m[1][2] = m[2][1]; m[2][1] = tmp;
  const float x = m[3][0];
  const float y = m[3][1];
  const float z = m[3][2];
  m[3][0] = -(m[0][0]*x+m[1][0]*y+m[2][0]*z);
  m[3][1] = -(m[0][1]*x+m[1][1]*y+m[2][1]*z);
  m[3][2] = -(m[0][2]*x+m[1][2]*y+m[2][2]*z);
  return *this;
}

// compute the inverse of a 4x4 affine transformation matrix
//
// Affine transformations are generalizations of Euclidean transformations.
// Affine transformation includes translation, rotation, reflection, scaling,
// and shearing. Length and angle are NOT preserved.
VMatrix4 &VMatrix4::invertAffine () {
  // R^-1
  // inverse 3x3 matrix
  float r[9];
  r[0] = m[0][0];
  r[1] = m[0][1];
  r[2] = m[0][2];
  r[3] = m[1][0];
  r[4] = m[1][1];
  r[5] = m[1][2];
  r[6] = m[2][0];
  r[7] = m[2][1];
  r[8] = m[2][2];
  {
    float tmp[9];
    tmp[0] = VSUM2(r[4]*r[8], -(r[5]*r[7]));
    tmp[1] = VSUM2(r[2]*r[7], -(r[1]*r[8]));
    tmp[2] = VSUM2(r[1]*r[5], -(r[2]*r[4]));
    tmp[3] = VSUM2(r[5]*r[6], -(r[3]*r[8]));
    tmp[4] = VSUM2(r[0]*r[8], -(r[2]*r[6]));
    tmp[5] = VSUM2(r[2]*r[3], -(r[0]*r[5]));
    tmp[6] = VSUM2(r[3]*r[7], -(r[4]*r[6]));
    tmp[7] = VSUM2(r[1]*r[6], -(r[0]*r[7]));
    tmp[8] = VSUM2(r[0]*r[4], -(r[1]*r[3]));
    // check determinant if it is 0
    const float determinant = r[0]*tmp[0]+r[1]*tmp[3]+r[2]*tmp[6];
    if (fabsf(determinant) <= 0.0001f) {
      // cannot inverse, make it idenety matrix
      memset(r, 0, sizeof(r));
      r[0] = r[4] = r[8] = 1.0f;
    } else {
      // divide by the determinant
      const float invDeterminant = 1.0f/determinant;
      r[0] = invDeterminant*tmp[0];
      r[1] = invDeterminant*tmp[1];
      r[2] = invDeterminant*tmp[2];
      r[3] = invDeterminant*tmp[3];
      r[4] = invDeterminant*tmp[4];
      r[5] = invDeterminant*tmp[5];
      r[6] = invDeterminant*tmp[6];
      r[7] = invDeterminant*tmp[7];
      r[8] = invDeterminant*tmp[8];
    }
  }

  m[0][0] = r[0]; m[0][1] = r[1]; m[0][2] = r[2];
  m[1][0] = r[3]; m[1][1] = r[4]; m[1][2] = r[5];
  m[2][0] = r[6]; m[2][1] = r[7]; m[2][2] = r[8];

  // -R^-1 * T
  const float x = m[3][0];
  const float y = m[3][1];
  const float z = m[3][2];
  m[3][0] = -VSUM3(r[0]*x, r[3]*y, r[6]*z);
  m[3][1] = -VSUM3(r[1]*x, r[4]*y, r[7]*z);
  m[3][2] = -VSUM3(r[2]*x, r[5]*y, r[8]*z);

  // last row should be unchanged (0,0,0,1)
  //mt.ptr[3] = mt.ptr[7] = mt.ptr[11] = 0.0f;
  //mt.ptr[15] = 1.0f;

  return *this;
}

VMatrix4 &VMatrix4::invert () {
  // if the 4th row is [0,0,0,1] then it is affine matrix and
  // it has no projective transformation
  if (m[0][3] == 0.0f && m[1][3] == 0.0f && m[2][3] == 0.0f && m[3][3] == 1.0f) {
    return invertAffine();
  } else {
    return invertGeneral();
  }
}

///////////////////////////////////////////////////////////////////////////////
// compute the inverse of a general 4x4 matrix using Cramer's Rule
// if cannot find inverse, return indentity matrix
VMatrix4 &VMatrix4::invertGeneral () {
  const float cofactor0 = getCofactor(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
  const float cofactor1 = getCofactor(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
  const float cofactor2 = getCofactor(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
  const float cofactor3 = getCofactor(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);

  const float determinant = m[0][0]*cofactor0-m[0][1]*cofactor1+m[0][2]*cofactor2-m[0][3]*cofactor3;
  if (fabsf(determinant) <= 0.0001f) { SetIdentity(); return *this; }

  const float cofactor4 = getCofactor(m[0][1], m[0][2], m[0][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
  const float cofactor5 = getCofactor(m[0][0], m[0][2], m[0][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
  const float cofactor6 = getCofactor(m[0][0], m[0][1], m[0][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
  const float cofactor7 = getCofactor(m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);

  const float cofactor8 = getCofactor(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3],  m[3][1], m[3][2], m[3][3]);
  const float cofactor9 = getCofactor(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3],  m[3][0], m[3][2], m[3][3]);
  const float cofactor10= getCofactor(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3],  m[3][0], m[3][1], m[3][3]);
  const float cofactor11= getCofactor(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2],  m[3][0], m[3][1], m[3][2]);

  const float cofactor12= getCofactor(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3],  m[2][1], m[2][2], m[2][3]);
  const float cofactor13= getCofactor(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3],  m[2][0], m[2][2], m[2][3]);
  const float cofactor14= getCofactor(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3],  m[2][0], m[2][1], m[2][3]);
  const float cofactor15= getCofactor(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2],  m[2][0], m[2][1], m[2][2]);

  const float invDeterminant = 1.0f/determinant;
  m[0][0] =  invDeterminant*cofactor0;
  m[0][1] = -invDeterminant*cofactor4;
  m[0][2] =  invDeterminant*cofactor8;
  m[0][3] = -invDeterminant*cofactor12;

  m[1][0] = -invDeterminant*cofactor1;
  m[1][1] =  invDeterminant*cofactor5;
  m[1][2] = -invDeterminant*cofactor9;
  m[1][3] =  invDeterminant*cofactor13;

  m[2][0] =  invDeterminant*cofactor2;
  m[2][1] = -invDeterminant*cofactor6;
  m[2][2]=  invDeterminant*cofactor10;
  m[2][3]= -invDeterminant*cofactor14;

  m[3][0]= -invDeterminant*cofactor3;
  m[3][1]=  invDeterminant*cofactor7;
  m[3][2]= -invDeterminant*cofactor11;
  m[3][3]=  invDeterminant*cofactor15;

  return *this;
}

void VMatrix4::toQuaternion (float quat[4]) const {
  const float tr = m[0][0]+m[1][1]+m[2][2];
  // check the diagonal
  if (tr > 0.0f) {
    float s = sqrtf(tr+1.0f);
    quat[3] = s/2.0f;
    s = 0.5f/s;
    quat[0] = VSUM2(m[2][1], -m[1][2])*s;
    quat[1] = VSUM2(m[0][2], -m[2][0])*s;
    quat[2] = VSUM2(m[1][0], -m[0][1])*s;
  } else {
    // diagonal is negative
    static const unsigned nxt[3] = {1, 2, 0};
    unsigned i = 0;
    if (m[1][1] > m[0][0]) i = 1;
    if (m[2][2] > m[i][i]) i = 2;
    unsigned j = nxt[i];
    unsigned k = nxt[j];
    float s = sqrtf((m[i][i]-(m[j][j]+m[k][k]))+1.0f);
    quat[i] = s*0.5f;
    if (s != 0.0f) s = 0.5f/s;
    quat[3] = VSUM2(m[k][j],-m[j][k])*s;
    quat[j] = VSUM2(m[j][i], m[i][j])*s;
    quat[k] = VSUM2(m[k][i], m[i][k])*s;
  }
}
