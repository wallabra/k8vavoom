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


const VMatrix4 VMatrix4::Identity(
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
);


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


//==========================================================================
//
//  VMatrix4::VMatrix4
//
//==========================================================================
VMatrix4::VMatrix4 (const float *m2) {
  m[0][0] = m2[0];
  m[0][1] = m2[1];
  m[0][2] = m2[2];
  m[0][3] = m2[3];
  m[1][0] = m2[4];
  m[1][1] = m2[5];
  m[1][2] = m2[6];
  m[1][3] = m2[7];
  m[2][0] = m2[8];
  m[2][1] = m2[9];
  m[2][2] = m2[10];
  m[2][3] = m2[11];
  m[3][0] = m2[12];
  m[3][1] = m2[13];
  m[3][2] = m2[14];
  m[3][3] = m2[15];
}


//==========================================================================
//
//  operator VMatrix4 * VMatrix4
//
//==========================================================================
VMatrix4 operator * (const VMatrix4 &in1, const VMatrix4 &in2) {
  VMatrix4 out;
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
#ifdef USE_NEUMAIER_KAHAN
      out[i][j] = neumsum4(in1[i][0]*in2[0][j], in1[i][1]*in2[1][j], in1[i][2]*in2[2][j], in1[i][3]*in2[3][j]);
#else /* USE_NEUMAIER_KAHAN */
      out[i][j] = in1[i][0]*in2[0][j]+in1[i][1]*in2[1][j]+in1[i][2]*in2[2][j]+in1[i][3]*in2[3][j];
#endif /* USE_NEUMAIER_KAHAN */
    }
  }
  return out;
}


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
#ifdef USE_NEUMAIER_KAHAN
    neumsum3(
      m[r0][c0]*neumsum2(m[r1][c1]*m[r2][c2], -(m[r2][c1]*m[r1][c2])),
      -(m[r0][c1]*neumsum2(m[r1][c0]*m[r2][c2], -(m[r2][c0]*m[r1][c2]))),
      m[r0][c2]*neumsum2(m[r1][c0]*m[r2][c1], -(m[r2][c0]*m[r1][c1])))
#else /* USE_NEUMAIER_KAHAN */
    m[r0][c0]*(m[r1][c1]*m[r2][c2]-m[r2][c1]*m[r1][c2])-
    m[r0][c1]*(m[r1][c0]*m[r2][c2]-m[r2][c0]*m[r1][c2])+
    m[r0][c2]*(m[r1][c0]*m[r2][c1]-m[r2][c0]*m[r1][c1]);
#endif /* USE_NEUMAIER_KAHAN */
  ;
}


//==========================================================================
//
//  VMatrix4::Determinant
//
//==========================================================================
float VMatrix4::Determinant () const {
  return
#ifdef USE_NEUMAIER_KAHAN
    neumsum4(
      m[0][0]*MINOR(*this, 1, 2, 3, 1, 2, 3),
      -(m[0][1]*MINOR(*this, 1, 2, 3, 0, 2, 3)),
      m[0][2]*MINOR(*this, 1, 2, 3, 0, 1, 3),
      -(m[0][3]*MINOR(*this, 1, 2, 3, 0, 1, 2)))
#else /* USE_NEUMAIER_KAHAN */
    m[0][0]*MINOR(*this, 1, 2, 3, 1, 2, 3)-
    m[0][1]*MINOR(*this, 1, 2, 3, 0, 2, 3)+
    m[0][2]*MINOR(*this, 1, 2, 3, 0, 1, 3)-
    m[0][3]*MINOR(*this, 1, 2, 3, 0, 1, 2)
#endif /* USE_NEUMAIER_KAHAN */
  ;
}

#ifdef USE_NEUMAIER_KAHAN
# define VMAT_XSUM_2(value0,value1)  neumsum2(value0, value1)
# define VMAT_XSUM_3(value0,value1,value2)  neumsum3(value0, value1, value2)
# define VMAT_XSUM_4(value0,value1,value2,value3)  neumsum4(value0, value1, value2, value3)
#else /* USE_NEUMAIER_KAHAN */
# define VMAT_XSUM_2(value0,value1)  ((value0)+(value1))
# define VMAT_XSUM_3(value0,value1,value2)  ((value0)+(value1)+(value2))
# define VMAT_XSUM_4(value0,value1,value2,value3)  ((value0)+(value1)+(value2)+(value3))
#endif /* USE_NEUMAIER_KAHAN */


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

  float v0 = VMAT_XSUM_2(m20*m31, -(m21*m30));
  float v1 = VMAT_XSUM_2(m20*m32, -(m22*m30));
  float v2 = VMAT_XSUM_2(m20*m33, -(m23*m30));
  float v3 = VMAT_XSUM_2(m21*m32, -(m22*m31));
  float v4 = VMAT_XSUM_2(m21*m33, -(m23*m31));
  float v5 = VMAT_XSUM_2(m22*m33, -(m23*m32));

  const float t00 = +VMAT_XSUM_3(v5*m11, -(v4*m12), v3*m13);
  const float t10 = -VMAT_XSUM_3(v5*m10, -(v2*m12), v1*m13);
  const float t20 = +VMAT_XSUM_3(v4*m10, -(v2*m11), v0*m13);
  const float t30 = -VMAT_XSUM_3(v3*m10, -(v1*m11), v0*m12);

  float invDet = 1.0f/VMAT_XSUM_4(t00*m00, t10*m01, t20*m02, t30*m03);
  if (!isFiniteF(invDet)) invDet = 0.0f;

  const float d00 = t00*invDet;
  const float d10 = t10*invDet;
  const float d20 = t20*invDet;
  const float d30 = t30*invDet;

  const float d01 = -VMAT_XSUM_3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d11 = +VMAT_XSUM_3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d21 = -VMAT_XSUM_3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d31 = +VMAT_XSUM_3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VMAT_XSUM_2(m10*m31, -(m11*m30));
  v1 = VMAT_XSUM_2(m10*m32, -(m12*m30));
  v2 = VMAT_XSUM_2(m10*m33, -(m13*m30));
  v3 = VMAT_XSUM_2(m11*m32, -(m12*m31));
  v4 = VMAT_XSUM_2(m11*m33, -(m13*m31));
  v5 = VMAT_XSUM_2(m12*m33, -(m13*m32));

  const float d02 = +VMAT_XSUM_3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d12 = -VMAT_XSUM_3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d22 = +VMAT_XSUM_3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d32 = -VMAT_XSUM_3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VMAT_XSUM_2(m21*m10, -(m20*m11));
  v1 = VMAT_XSUM_2(m22*m10, -(m20*m12));
  v2 = VMAT_XSUM_2(m23*m10, -(m20*m13));
  v3 = VMAT_XSUM_2(m22*m11, -(m21*m12));
  v4 = VMAT_XSUM_2(m23*m11, -(m21*m13));
  v5 = VMAT_XSUM_2(m23*m12, -(m22*m13));

  const float d03 = -VMAT_XSUM_3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d13 = +VMAT_XSUM_3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d23 = -VMAT_XSUM_3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d33 = +VMAT_XSUM_3(v3*m00, -(v1*m01), v0*m02)*invDet;

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
//  VMatrix4::Transform
//
//==========================================================================
TVec VMatrix4::Transform (const TVec &V) const {
  TVec Out;
  Out.x = VMAT_XSUM_4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]);
  Out.y = VMAT_XSUM_4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]);
  Out.z = VMAT_XSUM_4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]);
  return Out;
}


//==========================================================================
//
//  VMatrix4::Transform2
//
//==========================================================================
TVec VMatrix4::Transform2 (const TVec &V) const {
  TVec Out;
  Out.x = VMAT_XSUM_4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]);
  Out.y = VMAT_XSUM_4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]);
  Out.z = VMAT_XSUM_4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
  return Out;
}
