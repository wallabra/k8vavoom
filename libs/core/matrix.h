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

class VMatrix4 {
public:
  float m[4][4];

  static const VMatrix4 Identity;

  VMatrix4 () {}
  VMatrix4 (float m00, float m01, float m02, float m03,
    float m10, float m11, float m12, float m13,
    float m20, float m21, float m22, float m23,
    float m30, float m31, float m32, float m33);
  VMatrix4 (const float *m2);
  VMatrix4 (const VMatrix4 &m2) { memcpy(m, m2.m, sizeof(m)); }

  float Determinant () const;
  VMatrix4 Inverse () const;
  VMatrix4 Transpose () const;

  inline TVec Transform (const TVec &V) const {
    TVec Out;
    Out.x = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]);
    Out.y = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]);
    Out.z = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]);
    return Out;
  }

  inline TVec Transform (const TVec &V, const float w) const {
    TVec Out;
    Out.x = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]*w);
    Out.y = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]*w);
    Out.z = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]*w);
    return Out;
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2 (const TVec &V) const {
    TVec Out;
    Out.x = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]);
    Out.y = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]);
    Out.z = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
    return Out;
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2OnlyXY (const TVec &V) const {
    TVec Out;
    Out.x = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]);
    Out.y = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]);
    Out.z = V.z; // meh
    return Out;
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline float Transform2OnlyZ (const TVec &V) const {
    return VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2 (const TVec &V, const float w) const {
    TVec Out;
    Out.x = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w);
    Out.y = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w);
    Out.z = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w);
    return Out;
  }

  // returns `w`
  inline float TransformInPlace (TVec &V) const {
    const float newx = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]);
    const float newy = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]);
    const float newz = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]);
    const float neww = VSUM4(m[3][0]*V.x, m[3][1]*V.y, m[3][2]*V.z, m[3][3]);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  inline float TransformInPlace (TVec &V, float w) const {
    const float newx = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]*w);
    const float newy = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]*w);
    const float newz = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]*w);
    const float neww = VSUM4(m[3][0]*V.x, m[3][1]*V.y, m[3][2]*V.z, m[3][3]*w);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  // this can be used to transform with OpenGL model/projection matrices
  inline float Transform2InPlace (TVec &V) const {
    const float newx = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]);
    const float newy = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]);
    const float newz = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
    const float neww = VSUM4(m[0][3]*V.x, m[1][3]*V.y, m[2][3]*V.z, m[3][3]);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  // this can be used to transform with OpenGL model/projection matrices
  inline float Transform2InPlace (TVec &V, const float w) const {
    const float newx = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w);
    const float newy = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w);
    const float newz = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w);
    const float neww = VSUM4(m[0][3]*V.x, m[1][3]*V.y, m[2][3]*V.z, m[3][3]*w);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  inline VMatrix4 &operator = (const VMatrix4 &m2) { if (&m2 != this) memcpy(m, m2.m, sizeof(m)); return *this; }

  inline float *operator [] (int i) { return m[i]; }
  inline const float *operator [] (int i) const { return m[i]; }

  //friend VMatrix4 operator * (const VMatrix4 &M1, const VMatrix4 &M2);

  VMatrix4 operator * (const VMatrix4 &M2) const;

  void ExtractFrustumLeft (TPlane &plane) const;
  void ExtractFrustumRight (TPlane &plane) const;
  void ExtractFrustumTop (TPlane &plane) const;
  void ExtractFrustumBottom (TPlane &plane) const;
  void ExtractFrustumFar (TPlane &plane) const;
  void ExtractFrustumNear (TPlane &plane) const;
};


class VRotMatrix {
public:
  float m[3][3];

  VRotMatrix (const TVec &Axis, float Angle) {
    const float s = msin(Angle);
    const float c = mcos(Angle);
    const float t = 1.0f-c;

    m[0][0] = VSUM2(t*Axis.x*Axis.x, c);
    m[0][1] = VSUM2(t*Axis.x*Axis.y, -(s*Axis.z));
    m[0][2] = VSUM2(t*Axis.x*Axis.z, s*Axis.y);

    m[1][0] = VSUM2(t*Axis.y*Axis.x, s*Axis.z);
    m[1][1] = VSUM2(t*Axis.y*Axis.y, c);
    m[1][2] = VSUM2(t*Axis.y*Axis.z, -(s*Axis.x));

    m[2][0] = VSUM2(t*Axis.z*Axis.x, -(s*Axis.y));
    m[2][1] = VSUM2(t*Axis.z*Axis.y, s*Axis.x);
    m[2][2] = VSUM2(t*Axis.z*Axis.z, c);
  }

  friend inline TVec operator * (const TVec &v, const VRotMatrix &m) {
    return TVec(
      VSUM3(m.m[0][0]*v.x, m.m[0][1]*v.y, m.m[0][2]*v.z),
      VSUM3(m.m[1][0]*v.x, m.m[1][1]*v.y, m.m[1][2]*v.z),
      VSUM3(m.m[2][0]*v.x, m.m[2][1]*v.y, m.m[2][2]*v.z)
    );
  }
};
