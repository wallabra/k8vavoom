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

class VMatrix4 {
public:
  float m[4][4];

  static const VMatrix4 Identity;

  inline VMatrix4 () noexcept {}
  inline VMatrix4 (ENoInit) noexcept {}
  VMatrix4 (float m00, float m01, float m02, float m03,
    float m10, float m11, float m12, float m13,
    float m20, float m21, float m22, float m23,
    float m30, float m31, float m32, float m33) noexcept;
  inline VMatrix4 (const float *m2) noexcept { memcpy(m, m2, sizeof(m)); }
  inline VMatrix4 (const VMatrix4 &m2) noexcept { memcpy(m, m2.m, sizeof(m)); }
  inline VMatrix4 (const TVec &v) noexcept {
    memcpy((void *)m, (const void *)Identity.m, sizeof(m));
    m[0][0] = v.x;
    m[1][1] = v.y;
    m[2][2] = v.z;
  }

  inline void SetIdentity () noexcept { memcpy((void *)m, (const void *)Identity.m, sizeof(m)); }
  inline void SetZero () noexcept { memset((void *)m, 0, sizeof(m)); }

  inline const TVec &GetRow (int idx) const noexcept { return *(const TVec *)(m[idx]); }
  //inline TVec GetRow (int idx) const noexcept { return *(TVec *)(m[idx]); }
  inline TVec GetCol (int idx) const noexcept { return TVec(m[0][idx], m[1][idx], m[2][idx]); }

  // this is for camera matrices
  inline TVec getUpVector () const noexcept { return TVec(m[0][1], m[1][1], m[2][1]); }
  inline TVec getRightVector () const noexcept { return TVec(m[0][0], m[1][0], m[2][0]); }
  inline TVec getForwardVector () const noexcept { return TVec(m[0][2], m[1][2], m[2][2]); }

  float Determinant () const noexcept;
  VMatrix4 Inverse () const noexcept;
  VMatrix4 Transpose () const noexcept;

  inline TVec Transform (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]),
      VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]),
      VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]));
  }

  inline TVec Transform (const TVec &V, const float w) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]*w),
      VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]*w),
      VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]*w));
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2 (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]),
      VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]));
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2OnlyXY (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]),
      V.z); // meh
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline float Transform2OnlyZ (const TVec &V) const noexcept {
    return VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
  }

  // this can be used to transform with OpenGL model/projection matrices
  inline TVec Transform2 (const TVec &V, const float w) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w),
      VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w));
  }

  // returns `w`
  inline float TransformInPlace (TVec &V) const noexcept {
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
  inline float TransformInPlace (TVec &V, float w) const noexcept {
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
  inline float Transform2InPlace (TVec &V) const noexcept {
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
  inline float Transform2InPlace (TVec &V, const float w) const noexcept {
    const float newx = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w);
    const float newy = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w);
    const float newz = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w);
    const float neww = VSUM4(m[0][3]*V.x, m[1][3]*V.y, m[2][3]*V.z, m[3][3]*w);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // ignore translation column
  inline TVec RotateVector (const TVec &V) const noexcept {
    return TVec(
      VSUM3(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z),
      VSUM3(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z),
      VSUM3(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z));
  }

  // ignore rotation part
  inline TVec TranslateVector (const TVec &V) const noexcept { return TVec(VSUM2(V.x, m[3][0]), VSUM2(V.y, m[3][1]), VSUM2(V.z, m[3][2])); }

  inline VMatrix4 &operator = (const VMatrix4 &m2) noexcept { if (&m2 != this) memcpy(m, m2.m, sizeof(m)); return *this; }

  inline float *operator [] (int i) noexcept { return m[i]; }
  inline const float *operator [] (int i) const noexcept { return m[i]; }

  VMatrix4 operator * (const VMatrix4 &mt) const noexcept {
    return VMatrix4(
      VSUM4(m[0][0]*mt.m[0][0],m[1][0]*mt.m[0][1],m[2][0]*mt.m[0][2],m[3][0]*mt.m[0][3]),VSUM4(m[0][1]*mt.m[0][0],m[1][1]*mt.m[0][1],m[2][1]*mt.m[0][2],m[3][1]*mt.m[0][3]),VSUM4(m[0][2]*mt.m[0][0],m[1][2]*mt.m[0][1],m[2][2]*mt.m[0][2],m[3][2]*mt.m[0][3]),VSUM4(m[0][3]*mt.m[0][0],m[1][3]*mt.m[0][1],m[2][3]*mt.m[0][2],m[3][3]*mt.m[0][3]),
      VSUM4(m[0][0]*mt.m[1][0],m[1][0]*mt.m[1][1],m[2][0]*mt.m[1][2],m[3][0]*mt.m[1][3]),VSUM4(m[0][1]*mt.m[1][0],m[1][1]*mt.m[1][1],m[2][1]*mt.m[1][2],m[3][1]*mt.m[1][3]),VSUM4(m[0][2]*mt.m[1][0],m[1][2]*mt.m[1][1],m[2][2]*mt.m[1][2],m[3][2]*mt.m[1][3]),VSUM4(m[0][3]*mt.m[1][0],m[1][3]*mt.m[1][1],m[2][3]*mt.m[1][2],m[3][3]*mt.m[1][3]),
      VSUM4(m[0][0]*mt.m[2][0],m[1][0]*mt.m[2][1],m[2][0]*mt.m[2][2],m[3][0]*mt.m[2][3]),VSUM4(m[0][1]*mt.m[2][0],m[1][1]*mt.m[2][1],m[2][1]*mt.m[2][2],m[3][1]*mt.m[2][3]),VSUM4(m[0][2]*mt.m[2][0],m[1][2]*mt.m[2][1],m[2][2]*mt.m[2][2],m[3][2]*mt.m[2][3]),VSUM4(m[0][3]*mt.m[2][0],m[1][3]*mt.m[2][1],m[2][3]*mt.m[2][2],m[3][3]*mt.m[2][3]),
      VSUM4(m[0][0]*mt.m[3][0],m[1][0]*mt.m[3][1],m[2][0]*mt.m[3][2],m[3][0]*mt.m[3][3]),VSUM4(m[0][1]*mt.m[3][0],m[1][1]*mt.m[3][1],m[2][1]*mt.m[3][2],m[3][1]*mt.m[3][3]),VSUM4(m[0][2]*mt.m[3][0],m[1][2]*mt.m[3][1],m[2][2]*mt.m[3][2],m[3][2]*mt.m[3][3]),VSUM4(m[0][3]*mt.m[3][0],m[1][3]*mt.m[3][1],m[2][3]*mt.m[3][2],m[3][3]*mt.m[3][3])
    );
  }

  inline VMatrix4 operator - (void) const noexcept {
    /*
    VMatrix4 res = VMatrix4(*this);
    for (unsigned i = 0; i < 4; ++i) {
      for (unsigned j = 0; j < 4; ++j) {
        res.mt[i][j] = -res.mt[i][j];
      }
    }
    return res;
    */
    return VMatrix4(
      -m[0][0], -m[0][1], -m[0][2], -m[0][3],
      -m[1][0], -m[1][1], -m[1][2], -m[1][3],
      -m[2][0], -m[2][1], -m[2][2], -m[2][3],
      -m[3][0], -m[3][1], -m[3][2], -m[3][3]
    );
  }

  inline VMatrix4 operator + (void) const noexcept { return VMatrix4(*this); }

  static void CombineAndExtractFrustum (const VMatrix4 &model, const VMatrix4 &proj, TPlane planes[6]) noexcept;

  // combine the two matrices (multiply projection by modelview)
  // destroys (ignores) current matrix
  void ModelProjectCombine (const VMatrix4 &model, const VMatrix4 &proj) noexcept;

  // this expects result of `ModelProjectCombine()`
  void ExtractFrustum (TPlane planes[6]) const noexcept;

  // the following expects result of `ModelProjectCombine()`
  void ExtractFrustumLeft (TPlane &plane) const noexcept;
  void ExtractFrustumRight (TPlane &plane) const noexcept;
  void ExtractFrustumTop (TPlane &plane) const noexcept;
  void ExtractFrustumBottom (TPlane &plane) const noexcept;
  void ExtractFrustumFar (TPlane &plane) const noexcept;
  void ExtractFrustumNear (TPlane &plane) const noexcept;

  // more matrix operations (not used in k8vavoom, but nice to have)
  // this is for OpenGL coordinate system
  static VMatrix4 RotateX (float angle) noexcept;
  static VMatrix4 RotateY (float angle) noexcept;
  static VMatrix4 RotateZ (float angle) noexcept;

  static VMatrix4 Translate (const TVec &v) noexcept;
  static VMatrix4 TranslateNeg (const TVec &v) noexcept;

  static VMatrix4 Scale (const TVec &v) noexcept;
  static VMatrix4 Rotate (const TVec &v) noexcept; // x, y and z are angles; does x, then y, then z

  // for camera; x is pitch (up/down); y is yaw (left/right); z is roll (tilt)
  static VMatrix4 RotateZXY (const TVec &v) noexcept;
  static VMatrix4 RotateZXY (const TAVec &v) noexcept;

  // same as `glFrustum()`
  static VMatrix4 Frustum (float left, float right, float bottom, float top, float nearVal, float farVal) noexcept;
  // same as `glOrtho()`
  static VMatrix4 Ortho (float left, float right, float bottom, float top, float nearVal, float farVal) noexcept;
  // same as `gluPerspective()`
  // sets the frustum to perspective mode
  // fovY   - Field of vision in degrees in the y direction
  // aspect - Aspect ratio of the viewport
  // zNear  - The near clipping distance
  // zFar   - The far clipping distance
  static VMatrix4 Perspective (float fovY, float aspect, float zNear, float zFar) noexcept;

  static VMatrix4 LookAtFucked (const TVec &eye, const TVec &center, const TVec &up) noexcept;

  // does `gluLookAt()`
  VMatrix4 lookAt (const TVec &eye, const TVec &center, const TVec &up) const noexcept;

  // rotate matrix to face along the target direction
  // this function will clear the previous rotation and scale, but it will keep the previous translation
  // it is for rotating object to look at the target, NOT for camera
  VMatrix4 &lookingAt (const TVec &target) noexcept;
  VMatrix4 &lookingAt (const TVec &target, const TVec &upVec) noexcept;

  inline VMatrix4 lookAt (const TVec &target) const noexcept { auto res = VMatrix4(*this); return res.lookingAt(target); }
  inline VMatrix4 lookAt (const TVec &target, const TVec &upVec) const noexcept { auto res = VMatrix4(*this); return res.lookingAt(target, upVec); }

  VMatrix4 &rotate (float angle, const TVec &axis) noexcept;
  VMatrix4 &rotateX (float angle) noexcept;
  VMatrix4 &rotateY (float angle) noexcept;
  VMatrix4 &rotateZ (float angle) noexcept;

  inline VMatrix4 rotated (float angle, const TVec &axis) const noexcept { auto res = VMatrix4(*this); return res.rotate(angle, axis); }
  inline VMatrix4 rotatedX (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateX(angle); }
  inline VMatrix4 rotatedY (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateY(angle); }
  inline VMatrix4 rotatedZ (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateZ(angle); }

  // retrieve angles in degree from rotation matrix, M = Rx*Ry*Rz, in degrees
  // Rx: rotation about X-axis, pitch
  // Ry: rotation about Y-axis, yaw (heading)
  // Rz: rotation about Z-axis, roll
  TAVec getAngles () const noexcept;

  inline VMatrix4 transposed () const noexcept {
    return VMatrix4(
      m[0][0], m[1][0], m[2][0], m[3][0],
      m[0][1], m[1][1], m[2][1], m[3][1],
      m[0][2], m[1][2], m[2][2], m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    );
  }

  // blends two matrices together, at a given percentage (range is [0..1]), blend==0: m2 is ignored
  // WARNING! it won't sanitize `blend`
  VMatrix4 blended (const VMatrix4 &m2, float blend) const noexcept;

  //WARNING: this must be tested for row/col
  // partially ;-) taken from DarkPlaces
  // this assumes uniform scaling
  VMatrix4 invertedSimple () const noexcept;
  //FIXME: make this fast pasta!
  inline VMatrix4 &invertSimple () noexcept { VMatrix4 i = invertedSimple(); memcpy(m, i.m, sizeof(m)); return *this; }

  // compute the inverse of 4x4 Euclidean transformation matrix
  //
  // Euclidean transformation is translation, rotation, and reflection.
  // With Euclidean transform, only the position and orientation of the object
  // will be changed. Euclidean transform does not change the shape of an object
  // (no scaling). Length and angle are prereserved.
  //
  // Use inverseAffine() if the matrix has scale and shear transformation.
  VMatrix4 &invertEuclidean () noexcept;

  // compute the inverse of a 4x4 affine transformation matrix
  //
  // Affine transformations are generalizations of Euclidean transformations.
  // Affine transformation includes translation, rotation, reflection, scaling,
  // and shearing. Length and angle are NOT preserved.
  VMatrix4 &invertAffine () noexcept;

  // compute the inverse of a general 4x4 matrix using Cramer's Rule
  // if cannot find inverse, return indentity matrix
  VMatrix4 &invertGeneral () noexcept;

  // general matrix inversion
  VMatrix4 &invert () noexcept;

  void toQuaternion (float quat[4]) const noexcept;
};

static inline __attribute__((unused)) TVec operator * (const VMatrix4 &mt, const TVec &v) noexcept {
  return TVec(
    VSUM4(mt.m[0][0]*v.x, mt.m[1][0]*v.y, mt.m[2][0]*v.z, mt.m[3][0]),
    VSUM4(mt.m[0][1]*v.x, mt.m[1][1]*v.y, mt.m[2][1]*v.z, mt.m[3][1]),
    VSUM4(mt.m[0][2]*v.x, mt.m[1][2]*v.y, mt.m[2][2]*v.z, mt.m[3][2]));
}

static inline __attribute__((unused)) TVec operator * (const TVec &v, const VMatrix4 &mt) noexcept {
  return TVec(
    VSUM4(mt.m[0][0]*v.x, mt.m[0][1]*v.y, mt.m[0][2]*v.z, mt.m[0][3]),
    VSUM4(mt.m[1][0]*v.x, mt.m[1][1]*v.y, mt.m[1][2]*v.z, mt.m[1][3]),
    VSUM4(mt.m[2][0]*v.x, mt.m[2][1]*v.y, mt.m[2][2]*v.z, mt.m[2][3]));
}


// ////////////////////////////////////////////////////////////////////////// //
class VRotMatrix {
public:
  float m[3][3];

  inline VRotMatrix (const TVec &Axis, float Angle) noexcept {
    //const float s = msin(Angle);
    //const float c = mcos(Angle);
    float s, c;
    msincos(Angle, &s, &c);
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

  friend inline TVec operator * (const TVec &v, const VRotMatrix &mt) noexcept {
    return TVec(
      VSUM3(mt.m[0][0]*v.x, mt.m[0][1]*v.y, mt.m[0][2]*v.z),
      VSUM3(mt.m[1][0]*v.x, mt.m[1][1]*v.y, mt.m[1][2]*v.z),
      VSUM3(mt.m[2][0]*v.x, mt.m[2][1]*v.y, mt.m[2][2]*v.z)
    );
  }
};
