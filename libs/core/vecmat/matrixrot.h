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
