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
#ifndef VAVOOM_VAVOOMC_SHARED_HEADER
#define VAVOOM_VAVOOMC_SHARED_HEADER


// ////////////////////////////////////////////////////////////////////////// //
class VMemberBase;
class VObject;
class VPackage;
class VField;
class VMethod;
class VState;
class VConstant;
class VStruct;
class VClass;


// ////////////////////////////////////////////////////////////////////////// //
union VStack {
  vint32 i;
  vuint32 u;
  float f;
  void *p;
};


// ////////////////////////////////////////////////////////////////////////// //
// this is used in vector swizzling
enum VCVectorSwizzleElem {
  VCVSE_Zero = 0,
  VCVSE_One = 1,
  VCVSE_X = 2,
  VCVSE_Y = 3,
  VCVSE_Z = 4,
  VCVSE_Negate = 0x08,
  VCVSE_Mask = 0x0f,
  VCVSE_ElementMask = 0x07,
  VCVSE_Shift = 4,
};


// ////////////////////////////////////////////////////////////////////////// //
class VNetObjectsMapBase {
public:
  virtual bool SerialiseName (VStream &, VName &) = 0;
  virtual bool SerialiseObject (VStream &, VObject *&) = 0;
  virtual bool SerialiseClass (VStream &, VClass *&) = 0;
  virtual bool SerialiseState (VStream &, VState *&) = 0;
};


#endif
