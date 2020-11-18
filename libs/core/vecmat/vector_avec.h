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

// angles
class /*__attribute__((packed))*/ TAVec {
public:
  float pitch; // up/down
  float yaw; // left/right
  float roll; // around screen center

  inline TAVec () noexcept {}
  inline TAVec (ENoInit) noexcept {}
  //nope;TAVec () noexcept : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
  inline TAVec (float APitch, float AYaw, float ARoll=0.0f) noexcept : pitch(APitch), yaw(AYaw), roll(ARoll) {}

  inline bool isValid () const noexcept { return (isFiniteF(pitch) && isFiniteF(yaw) && isFiniteF(roll)); }
  inline bool isZero () const noexcept { return (pitch == 0.0f && yaw == 0.0f && roll == 0.0f); }
  inline bool isZeroSkipRoll () const noexcept { return (pitch == 0.0f && yaw == 0.0f); }

  friend VStream &operator << (VStream &Strm, TAVec &v) {
    return Strm << v.pitch << v.yaw << v.roll;
  }
};

static_assert(__builtin_offsetof(TAVec, yaw) == __builtin_offsetof(TAVec, pitch)+sizeof(float), "TAVec layout fail (0)");
static_assert(__builtin_offsetof(TAVec, roll) == __builtin_offsetof(TAVec, yaw)+sizeof(float), "TAVec layout fail (1)");
static_assert(sizeof(TAVec) == sizeof(float)*3, "TAVec layout fail (2)");

inline vuint32 GetTypeHash (const TAVec &v) noexcept { return joaatHashBuf(&v, 3*sizeof(float)); }

static VVA_OKUNUSED inline bool operator == (const TAVec &v1, const TAVec &v2) noexcept { return (v1.pitch == v2.pitch && v1.yaw == v2.yaw && v1.roll == v2.roll); }
static VVA_OKUNUSED inline bool operator != (const TAVec &v1, const TAVec &v2) noexcept { return (v1.pitch != v2.pitch || v1.yaw != v2.yaw || v1.roll != v2.roll); }
