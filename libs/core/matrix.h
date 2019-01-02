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
  TVec Transform (const TVec &V) const;
  TVec Transform2 (const TVec &V) const;

  inline VMatrix4 &operator = (const VMatrix4 &m2) { if (&m2 != this) memcpy(m, m2.m, sizeof(m)); return *this; }

  inline float *operator [] (int i) { return m[i]; }
  inline const float *operator [] (int i) const { return m[i]; }

  friend VMatrix4 operator * (const VMatrix4 &M1, const VMatrix4 &M2);
};


class VRotMatrix {
public:
  float m[3][3];

  VRotMatrix (const TVec &Axis, float Angle) {
    float s = msin(Angle);
    float c = mcos(Angle);
    float t = 1-c;

    m[0][0] = t*Axis.x*Axis.x+c;
    m[0][1] = t*Axis.x*Axis.y-s*Axis.z;
    m[0][2] = t*Axis.x*Axis.z+s*Axis.y;

    m[1][0] = t*Axis.y*Axis.x+s*Axis.z;
    m[1][1] = t*Axis.y*Axis.y+c;
    m[1][2] = t*Axis.y*Axis.z-s*Axis.x;

    m[2][0] = t*Axis.z*Axis.x-s*Axis.y;
    m[2][1] = t*Axis.z*Axis.y+s*Axis.x;
    m[2][2] = t*Axis.z*Axis.z+c;
  }

  friend inline TVec operator * (const TVec &v, const VRotMatrix &m) {
    return TVec(
      m.m[0][0]*v.x+m.m[0][1]*v.y+m.m[0][2]*v.z,
      m.m[1][0]*v.x+m.m[1][1]*v.y+m.m[1][2]*v.z,
      m.m[2][0]*v.x+m.m[2][1]*v.y+m.m[2][2]*v.z);
  }
};
