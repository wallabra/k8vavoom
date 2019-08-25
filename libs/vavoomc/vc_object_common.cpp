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
#include "vc_object_rtti.cpp"

#if !defined(IN_VCC)
# define VC_HOST_ERROR  Host_Error
#else
# define VC_HOST_ERROR  Sys_Error
#endif


//**************************************************************************
//
//  Basic functions
//
//**************************************************************************

IMPLEMENT_FUNCTION(VObject, get_GC_AliveObjects) { RET_INT(gcLastStats.alive); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectedObjects) { RET_INT(gcLastStats.lastCollected); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectDuration) { RET_FLOAT((float)gcLastStats.lastCollectDuration); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectTime) { RET_FLOAT((float)gcLastStats.lastCollectTime); }

IMPLEMENT_FUNCTION(VObject, get_GC_MessagesAllowed) { RET_BOOL(GGCMessagesAllowed); }
IMPLEMENT_FUNCTION(VObject, set_GC_MessagesAllowed) { P_GET_BOOL(val); GGCMessagesAllowed = val; }


static TMap<VStr, bool> setOfNameSets;


//==========================================================================
//
//  setNamePut
//
//==========================================================================
static bool setNamePut (VName setName, VName value) {
  VStr realName = VStr(va("%s \x01\x01\x01 %s", *setName, *value));
  return setOfNameSets.put(realName, true);
}


//==========================================================================
//
//  setNameCheck
//
//==========================================================================
static bool setNameCheck (VName setName, VName value) {
  VStr realName = VStr(va("%s \x01\x01\x01 %s", *setName, *value));
  return !!setOfNameSets.find(realName);
}


//==========================================================================
//
//  Object.Destroy
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Destroy) {
  P_GET_SELF;
  if (Self) {
#ifdef VCC_STANDALONE_EXECUTOR
    if (GImmediadeDelete) {
      delete Self;
    } else {
      Self->ConditionalDestroy();
    }
#else
    //delete Self;
    Self->ConditionalDestroy();
#endif
  }
}


//==========================================================================
//
//  Object.IsA
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsA) {
  P_GET_NAME(SomeName);
  P_GET_SELF;
  bool Ret = false;
  if (Self) {
    for (const VClass *c = Self->Class; c; c = c->GetSuperClass()) {
      if (c->GetVName() == SomeName) { Ret = true; break; }
    }
  }
  RET_BOOL(Ret);
}


//==========================================================================
//
//  Object.IsDestroyed
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsDestroyed) {
  P_GET_SELF;
  RET_BOOL(Self ? Self->GetFlags()&_OF_DelayedDestroy : true);
}


//==========================================================================
//
//  Object.CollectGarbage
//
//==========================================================================
// static final void CollectGarbage (optional bool destroyDelayed);
IMPLEMENT_FUNCTION(VObject, GC_CollectGarbage) {
  P_GET_BOOL_OPT(destroyDelayed, false);
#if defined(VCC_STANDALONE_EXECUTOR)
  CollectGarbage(destroyDelayed);
#else
  (void)destroyDelayed;
  CollectGarbage();
#endif
}


//**************************************************************************
//
//  Error functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Error) {
#if !defined(IN_VCC)
  VObject::VMDumpCallStack();
  if (GArgs.CheckParm("-vc-all-errors-are-fatal")) {
    Sys_Error("%s", *PF_FormatString());
  } else {
    Host_Error("%s", *PF_FormatString());
  }
#else
  Sys_Error("%s", *PF_FormatString());
#endif
}

IMPLEMENT_FUNCTION(VObject, FatalError) {
  VObject::VMDumpCallStack();
  Sys_Error("%s", *PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, AssertError) {
  P_GET_STR(msg);
  VObject::VMDumpCallStack();
  Sys_Error("Assertion failed: %s", *msg);
}


//**************************************************************************
//
//  Math functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, AngleMod360) {
  P_GET_FLOAT(an);
  if (!isFiniteF(an)) { VObject::VMDumpCallStack(); if (isNaNF(an)) Sys_Error("got NAN"); Sys_Error("got INF"); }
  RET_FLOAT(AngleMod(an));
}

IMPLEMENT_FUNCTION(VObject, AngleMod180) {
  P_GET_FLOAT(an);
  if (!isFiniteF(an)) { VObject::VMDumpCallStack(); if (isNaNF(an)) Sys_Error("got NAN"); Sys_Error("got INF"); }
  RET_FLOAT(AngleMod180(an));
}

// native static final void AngleVectors (const TAVec angles, out TVec forward, optional out TVec right, optional out TVec up);
IMPLEMENT_FUNCTION(VObject, AngleVectors) {
  // note that optional out will still get a temporary storage
  P_GET_OUT_OPT(TVec, vup);
  P_GET_OUT_OPT(TVec, vright);
  P_GET_PTR(TVec, vforward);
  P_GET_AVEC(angles);
  if (specified_vup || specified_vright) {
    check(vup);
    check(vright);
    AngleVectors(angles, *vforward, *vright, *vup);
  } else {
    AngleVector(angles, *vforward);
  }
}

IMPLEMENT_FUNCTION(VObject, AngleVector) {
  P_GET_PTR(TVec, vforward);
  P_GET_AVEC(angles);
  AngleVector(angles, *vforward);
}

IMPLEMENT_FUNCTION(VObject, VectorAngles) {
  P_GET_PTR(TAVec, angles);
  P_GET_VEC(vec);
  VectorAngles(vec, *angles);
}

// native static final float VectorAngleYaw (const TVec vec);
IMPLEMENT_FUNCTION(VObject, VectorAngleYaw) {
  P_GET_VEC(vec);
  RET_FLOAT(VectorAngleYaw(vec));
}

// native static final float VectorAnglePitch (const TVec vec);
IMPLEMENT_FUNCTION(VObject, VectorAnglePitch) {
  P_GET_VEC(vec);
  RET_FLOAT(VectorAnglePitch(vec));
}

// native static final TVec AngleYawVector (const float yaw);
IMPLEMENT_FUNCTION(VObject, AngleYawVector) {
  P_GET_FLOAT(yaw);
  RET_VEC(AngleVectorYaw(yaw));
}

IMPLEMENT_FUNCTION(VObject, GetPlanePointZ) {
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  const float res = plane->GetPointZ(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); Sys_Error("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

IMPLEMENT_FUNCTION(VObject, GetPlanePointZRev) {
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  const float res = plane->GetPointZRev(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); Sys_Error("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide) {
  P_GET_PTR(TPlane, plane);
  P_GET_VEC(point);
  RET_INT(plane->PointOnSide(point));
}

IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide2) {
  P_GET_PTR(TPlane, plane);
  P_GET_VEC(point);
  RET_INT(plane->PointOnSide2(point));
}

// native static final int BoxOnLineSide2DV (const TVec bmin, const TVec bmax, const TVec v1, const TVec v2);
IMPLEMENT_FUNCTION(VObject, BoxOnLineSide2DV) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  P_GET_VEC(bmax);
  P_GET_VEC(bmin);
  float tmbox[4];
  tmbox[BOX2D_TOP] = bmax.y;
  tmbox[BOX2D_BOTTOM] = bmin.y;
  tmbox[BOX2D_LEFT] = bmin.x;
  tmbox[BOX2D_RIGHT] = bmax.x;
  RET_INT(BoxOnLineSide2D(tmbox, v1, v2));
}

IMPLEMENT_FUNCTION(VObject, RotateDirectionVector) {
  P_GET_AVEC(rot);
  P_GET_VEC(vec);

  TAVec angles(0, 0, 0);
  TVec out(0, 0, 0);

  VectorAngles(vec, angles);
  angles.pitch += rot.pitch;
  angles.yaw += rot.yaw;
  angles.roll += rot.roll;
  AngleVector(angles, out);
  RET_VEC(out);
}

IMPLEMENT_FUNCTION(VObject, VectorRotateAroundZ) {
  P_GET_FLOAT(angle);
  P_GET_PTR(TVec, vec);

  angle = AngleMod(angle);
  const float dstx = vec->x*mcos(angle)-vec->y*msin(angle);
  const float dsty = vec->x*msin(angle)+vec->y*mcos(angle);

  vec->x = dstx;
  vec->y = dsty;
}

IMPLEMENT_FUNCTION(VObject, RotateVectorAroundVector) {
  P_GET_FLOAT(Angle);
  P_GET_VEC(Axis);
  P_GET_VEC(Vector);
  RET_VEC(RotateVectorAroundVector(Vector, Axis, Angle));
}

//native static final bool IsPlainFloor (const ref TPlane plane); // valid only for floors
IMPLEMENT_FUNCTION(VObject, IsPlainFloor) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(plane->normal.z == 1.0f);
}

//native static final bool IsPlainCeiling (const ref TPlane plane); // valid only for ceilings
IMPLEMENT_FUNCTION(VObject, IsPlainCeiling) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(plane->normal.z == -1.0f);
}

//native static final bool IsSlopedFlat (const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, IsSlopedFlat) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(fabsf(plane->normal.z) != 1.0f);
}

//native static final bool IsVerticalPlane (const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, IsVerticalPlane) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(plane->normal.z == 0.0f);
}


// native static final TVec PlaneProjectPoint (const ref TPlane plane, TVec v);
IMPLEMENT_FUNCTION(VObject, PlaneProjectPoint) {
  P_GET_VEC(v);
  P_GET_PTR(TPlane, plane);
  RET_VEC(v-(v-plane->normal*plane->dist).dot(plane->normal)*plane->normal);
}

// initialises vertical plane from point and direction
//native static final void PlaneForPointDir (out TPlane plane, TVec point, TVec dir);
IMPLEMENT_FUNCTION(VObject, PlaneForPointDir) {
  P_GET_VEC(dir);
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  plane->SetPointDirXY(point, dir);
}

// initialises vertical plane from point and direction
// native static final void PlaneForPointNormal (out TPlane plane, TVec point, TVec norm, optional bool normNormalised);
IMPLEMENT_FUNCTION(VObject, PlaneForPointNormal) {
  P_GET_BOOL_OPT(normalNormalised, false);
  P_GET_VEC(norm);
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  if (normalNormalised) {
    plane->SetPointNormal3D(point, norm);
  } else {
    plane->SetPointNormal3DSafe(point, norm);
  }
}

// initialises vertical plane from two points
// native static final void PlaneForLine (out TPlane plane, TVec pointA, TVec pointB);
IMPLEMENT_FUNCTION(VObject, PlaneForLine) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  P_GET_PTR(TPlane, plane);
  plane->Set2Points(v1, v2);
}


//**************************************************************************
//
//  String functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, strlenutf8) {
  P_GET_STR(s);
  RET_INT(s.Utf8Length());
}

IMPLEMENT_FUNCTION(VObject, strcmp) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_INT(s1.Cmp(s2));
}

IMPLEMENT_FUNCTION(VObject, stricmp) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_INT(s1.ICmp(s2));
}

IMPLEMENT_FUNCTION(VObject, nameicmp) {
  P_GET_NAME(s2);
  P_GET_NAME(s1);
  RET_INT(VStr::ICmp(*s1, *s2));
}

IMPLEMENT_FUNCTION(VObject, namestricmp) {
  P_GET_STR(s2);
  P_GET_NAME(s1);
  RET_INT(s2.ICmp(*s1));
}

IMPLEMENT_FUNCTION(VObject, strlwr) {
  P_GET_STR(s);
  RET_STR(s.ToLower());
}

IMPLEMENT_FUNCTION(VObject, strupr) {
  P_GET_STR(s);
  RET_STR(s.ToUpper());
}

IMPLEMENT_FUNCTION(VObject, substrutf8) {
  P_GET_INT(Len);
  P_GET_INT(Start);
  P_GET_STR(Str);
  RET_STR(Str.Utf8Substring(Start, Len));
}

//native static final string strmid (string Str, int Start, optional int Len);
IMPLEMENT_FUNCTION(VObject, strmid) {
  P_GET_INT_OPT(len, 0);
  P_GET_INT(start);
  P_GET_STR(s);
  if (!specified_len) { if (start < 0) start = 0; len = s.length(); }
  RET_STR(s.mid(start, len));
}

//native static final string strleft (string Str, int len);
IMPLEMENT_FUNCTION(VObject, strleft) {
  P_GET_INT(len);
  P_GET_STR(s);
  RET_STR(s.left(len));
}

//native static final string strright (string Str, int len);
IMPLEMENT_FUNCTION(VObject, strright) {
  P_GET_INT(len);
  P_GET_STR(s);
  RET_STR(s.right(len));
}

//native static final string strrepeat (int len, optinal int ch);
IMPLEMENT_FUNCTION(VObject, strrepeat) {
  P_GET_INT_OPT(ch, 32);
  P_GET_INT(len);
  VStr s;
  s.setLength(len, ch);
  RET_STR(s);
}

// Creates one-char non-utf8 string from the given char code&0xff; 0 is allowed
//native static final string strFromChar (int ch);
IMPLEMENT_FUNCTION(VObject, strFromChar) {
  P_GET_INT(ch);
  ch &= 0xff;
  VStr s((char)ch);
  RET_STR(s);
}

// Creates one-char utf8 string from the given char code (or empty string if char code is invalid); 0 is allowed
//native static final string strFromCharUtf8 (int ch);
IMPLEMENT_FUNCTION(VObject, strFromCharUtf8) {
  P_GET_INT(ch);
  VStr s;
  if (ch >= 0 && ch <= 0x10FFFF) s.utf8Append((vuint32)ch);
  RET_STR(s);
}

//native static final string strFromInt (int v);
IMPLEMENT_FUNCTION(VObject, strFromInt) {
  P_GET_INT(v);
  VStr s(v);
  RET_STR(s);
}

//native static final string strFromFloat (float v);
IMPLEMENT_FUNCTION(VObject, strFromFloat) {
  P_GET_FLOAT(v);
  VStr s(v);
  RET_STR(s);
}

//native static final bool globmatch (string str, string pat, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, globmatch) {
  P_GET_BOOL_OPT(caseSens, true);
  P_GET_STR(pat);
  P_GET_STR(str);
  RET_BOOL(VStr::globmatch(*str, *pat, caseSens));
}


IMPLEMENT_FUNCTION(VObject, va) {
  RET_STR(PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, atoi) {
  P_GET_OUT_OPT_NOSP(int, err); // this is actually bool
  P_GET_STR(str);
  int res = 0;
  if (str.convertInt(&res)) {
    if (err) *err = 0;
    RET_INT(res);
  } else {
    if (err) *err = 1;
    RET_INT(0);
  }
}

IMPLEMENT_FUNCTION(VObject, atof) {
  P_GET_OUT_OPT_NOSP(int, err); // this is actually bool
  P_GET_STR(str);
  float res = 0;
  if (str.convertFloat(&res)) {
    *err = 0;
    RET_FLOAT(res);
  } else {
    *err = 1;
    RET_FLOAT(0);
  }
}

//native static final bool StrStartsWith (string Str, string Check, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, StrStartsWith) {
  P_GET_BOOL_OPT(caseSens, true);
  P_GET_STR(Check);
  P_GET_STR(Str);
  if (caseSens) {
    RET_BOOL(Str.StartsWith(Check));
  } else {
    RET_BOOL(Str.startsWithNoCase(Check));
  }
}

//native static final bool StrEndsWith (string Str, string Check, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, StrEndsWith) {
  P_GET_BOOL_OPT(caseSens, true);
  P_GET_STR(Check);
  P_GET_STR(Str);
  if (caseSens) {
    RET_BOOL(Str.EndsWith(Check));
  } else {
    RET_BOOL(Str.endsWithNoCase(Check));
  }
}

IMPLEMENT_FUNCTION(VObject, StrReplace) {
  P_GET_STR(Replacement);
  P_GET_STR(Search);
  P_GET_STR(Str);
  RET_STR(Str.Replace(Search, Replacement));
}

//native static final int strIndexOf (const string str, const string pat, optional int startpos, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, strIndexOf) {
  P_GET_BOOL_OPT(caseSens, true);
  P_GET_INT_OPT(startpos, 0);
  P_GET_STR(pat);
  P_GET_STR(str);
  if (!caseSens) { pat = pat.toLowerCase(); str = str.toLowerCase(); }
  RET_INT(str.indexOf(pat, startpos));
}

//native static final int strLastIndexOf (const string str, const string pat, optional int startpos, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, strLastIndexOf) {
  P_GET_BOOL_OPT(caseSens, true);
  P_GET_INT_OPT(startpos, 0);
  P_GET_STR(pat);
  P_GET_STR(str);
  if (!caseSens) { pat = pat.toLowerCase(); str = str.toLowerCase(); }
  RET_INT(str.lastIndexOf(pat, startpos));
}


//**************************************************************************
//
//  Random numbers
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Random) {
  RET_FLOAT(Random());
}

IMPLEMENT_FUNCTION(VObject, FRandomFull) {
  RET_FLOAT(RandomFull());
}

// native static final float FRandomBetween (float minv, float maxv);
IMPLEMENT_FUNCTION(VObject, FRandomBetween) {
  P_GET_FLOAT(maxv);
  P_GET_FLOAT(minv);
  RET_FLOAT(RandomBetween(minv, maxv));
}

IMPLEMENT_FUNCTION(VObject, GenRandomSeedU32) {
  vint32 rn;
  do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
  RET_INT(rn);
}

IMPLEMENT_FUNCTION(VObject, GenRandomU31) {
  RET_INT(GenRandomU31());
}

IMPLEMENT_FUNCTION(VObject, P_Random) {
/*
#if defined(VCC_STANDALONE_EXECUTOR)
  vuint8 b;
  ed25519_randombytes(&b, sizeof(b));
  RET_INT(b);
#else
*/
  RET_INT(P_Random());
}


// native static final void bjprngSeed (out BJPRNGCtx ctx, int aseed);
IMPLEMENT_FUNCTION(VObject, bjprngSeed) {
  P_GET_INT(aseed);
  P_GET_PTR(BJPRNGCtx, ctx);
  if (ctx) bjprng_raninit(ctx, (vuint32)aseed);
}

// full 32-bit value (so it can be negative)
//native static final int bjprngNext (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNext) {
  P_GET_PTR(BJPRNGCtx, ctx);
  RET_INT(ctx ? bjprng_ranval(ctx) : 0);
}

// [0..1) (WARNING! not really uniform!)
//native static final float bjprngNextFloat (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNextFloat) {
  P_GET_PTR(BJPRNGCtx, ctx);
  if (ctx) {
    for (;;) {
      float v = ((double)bjprng_ranval(ctx))/((double)0xffffffffu);
      if (v < 1.0f) { RET_FLOAT(v); return; }
    }
  } else {
    RET_FLOAT(0);
  }
}


//==========================================================================
//
//  Printing in console
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, print) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s);
  PR_DoWriteBuf(nullptr);
}

IMPLEMENT_FUNCTION(VObject, dprint) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, true); // debug
  PR_DoWriteBuf(nullptr, true); // debug
}

IMPLEMENT_FUNCTION(VObject, printwarn) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Warning);
  PR_DoWriteBuf(nullptr, false, NAME_Warning);
}

IMPLEMENT_FUNCTION(VObject, printerror) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Error);
  PR_DoWriteBuf(nullptr, false, NAME_Error);
}

IMPLEMENT_FUNCTION(VObject, printdebug) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Debug);
  PR_DoWriteBuf(nullptr, false, NAME_Debug);
}


//==========================================================================
//
//  Class methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindClass) {
  P_GET_NAME(Name);
  RET_PTR(VClass::FindClass(*Name));
}

IMPLEMENT_FUNCTION(VObject, FindClassNoCase) {
  P_GET_NAME(Name);
  RET_PTR(VClass::FindClassNoCase(*Name));
}

IMPLEMENT_FUNCTION(VObject, FindClassNoCaseStr) {
  P_GET_STR(Name);
  RET_PTR(VClass::FindClassNoCase(*Name));
}

//native static final class FindClassNoCaseEx (class BaseClass, name Name);
IMPLEMENT_FUNCTION(VObject, FindClassNoCaseEx) {
  P_GET_NAME(Name);
  P_GET_PTR(VClass, BaseClass);
  VClass *cls = VClass::FindClassNoCase(*Name);
  if (cls && BaseClass && !cls->IsChildOf(BaseClass)) {
    GLog.Logf(NAME_Warning, "FindClassNoCaseEx: class `%s` is not a child of `%s`", cls->GetName(), BaseClass->GetName());
    cls = nullptr;
  }
  RET_PTR(cls);
}

//native static final class FindClassNoCaseExStr (class BaseClass, string Name);
IMPLEMENT_FUNCTION(VObject, FindClassNoCaseExStr) {
  P_GET_STR(Name);
  P_GET_PTR(VClass, BaseClass);
  VClass *cls = VClass::FindClassNoCase(*Name);
  if (cls && BaseClass && !cls->IsChildOf(BaseClass)) {
    GLog.Logf(NAME_Warning, "FindClassNoCaseExStr: class `%s` is not a child of `%s`", cls->GetName(), BaseClass->GetName());
    cls = nullptr;
  }
  RET_PTR(cls);
}

IMPLEMENT_FUNCTION(VObject, ClassIsChildOf) {
  P_GET_PTR(VClass, BaseClass);
  P_GET_PTR(VClass, SomeClass);
  RET_BOOL(SomeClass && BaseClass ? SomeClass->IsChildOf(BaseClass) : false);
}

IMPLEMENT_FUNCTION(VObject, GetClassName) {
  P_GET_PTR(VClass, SomeClass);
  RET_NAME(SomeClass ? SomeClass->Name : NAME_None);
}

IMPLEMENT_FUNCTION(VObject, GetClassLocationStr) {
  P_GET_PTR(VClass, SomeClass);
  RET_STR(SomeClass ? SomeClass->Loc.toStringNoCol() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, GetFullClassName) {
  P_GET_PTR(VClass, SomeClass);
  RET_STR(SomeClass ? SomeClass->GetFullName() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, IsAbstractClass) {
  P_GET_PTR(VClass, SomeClass);
  if (SomeClass) {
    RET_BOOL(!!(SomeClass->ClassFlags&CLASS_Abstract));
  } else {
    RET_BOOL(false);
  }
}

IMPLEMENT_FUNCTION(VObject, IsNativeClass) {
  P_GET_PTR(VClass, SomeClass);
  if (SomeClass) {
    RET_BOOL(!!(SomeClass->ClassFlags&CLASS_Native));
  } else {
    RET_BOOL(false);
  }
}

IMPLEMENT_FUNCTION(VObject, GetClassParent) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->ParentClass : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacement) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacement() : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetCompatibleClassReplacement) {
  P_GET_PTR(VClass, SomeClass);
  P_GET_PTR(VClass, CType);
  VClass *nc = (SomeClass ? SomeClass->GetReplacement() : nullptr);
  if (nc && CType && !nc->IsChildOf(CType)) nc = nullptr;
  RET_PTR(nc);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacee) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacee() : nullptr);
}

IMPLEMENT_FUNCTION(VObject, FindClassState) {
  P_GET_BOOL_OPT(Exact, false);
  P_GET_NAME_OPT(SubLabel, NAME_None);
  P_GET_NAME(StateName);
  P_GET_PTR(VClass, Cls);
  VStateLabel *Lbl = Cls->FindStateLabel(StateName, SubLabel, Exact);
  RET_PTR(Lbl ? Lbl->State : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassNumOwnedStates) {
  P_GET_PTR(VClass, Cls);
  int Ret = 0;
  if (Cls) for (VState *S = Cls->States; S; S = S->Next) ++Ret;
  RET_INT(Ret);
}

IMPLEMENT_FUNCTION(VObject, GetClassFirstState) {
  P_GET_PTR(VClass, Cls);
  RET_PTR(Cls ? Cls->States : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassGameObjName) {
  P_GET_PTR(VClass, SomeClass);
  RET_NAME(SomeClass ? SomeClass->ClassGameObjName : NAME_None);
}


//native final static class FindMObjId (int id, optional name pkgname);
/*
IMPLEMENT_FUNCTION(VObject, FindPkgMObjId) {
  P_GET_NAME_OPT(pkgname, NAME_None);
  P_GET_INT(id);
  RET_REF(VMemberBase::StaticFindMObj(id, pkgname));
}

IMPLEMENT_FUNCTION(VObject, FindPkgScriptId) {
  P_GET_NAME_OPT(pkgname, NAME_None);
  P_GET_INT(id);
  RET_REF(VMemberBase::StaticFindScriptId(id, pkgname));
}
*/

IMPLEMENT_FUNCTION(VObject, FindClassByGameObjName) {
  P_GET_NAME_OPT(pkgname, NAME_None);
  P_GET_NAME(aname);
  RET_REF(VMemberBase::StaticFindClassByGameObjName(aname, pkgname));
}


//==========================================================================
//
//  State methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, GetStateLocationStr) {
  P_GET_PTR(VState, State);
  RET_STR(State ? State->Loc.toStringNoCol() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, GetFullStateName) {
  P_GET_PTR(VState, State);
  RET_STR(State ? State->GetFullName() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, StateIsInRange) {
  P_GET_INT(MaxDepth);
  P_GET_PTR(VState, End);
  P_GET_PTR(VState, Start);
  P_GET_PTR(VState, State);
  RET_BOOL(State ? State->IsInRange(Start, End, MaxDepth) : false);
}

IMPLEMENT_FUNCTION(VObject, StateIsInSequence) {
  P_GET_PTR(VState, Start);
  P_GET_PTR(VState, State);
  RET_BOOL(State ? State->IsInSequence(Start) : false);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteName) {
  P_GET_PTR(VState, State);
  RET_NAME(State ? State->SpriteName : NAME_None);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrame) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Frame : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameWidth) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameWidth : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameHeight) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameHeight : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameSize) {
  P_GET_REF(int, h);
  P_GET_REF(int, w);
  P_GET_PTR(VState, State);
  if (State) {
    *w = State->frameWidth;
    *h = State->frameHeight;
  } else {
    *w = 0;
    *h = 0;
  }
}

IMPLEMENT_FUNCTION(VObject, GetStateDuration) {
  P_GET_PTR(VState, State);
  RET_FLOAT(State ? State->Time : 0.0f);
}

IMPLEMENT_FUNCTION(VObject, GetStatePlus) {
  P_GET_BOOL_OPT(IgnoreJump, true);
  P_GET_INT(Offset);
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->GetPlus(Offset, IgnoreJump) : nullptr);
}

IMPLEMENT_FUNCTION(VObject, StateHasAction) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? !!State->Function : false);
}

IMPLEMENT_FUNCTION(VObject, CallStateAction) {
#if !defined(IN_VCC)
  P_GET_PTR(VState, State);
  P_GET_PTR(VObject, obj);
  if (State && State->Function) {
    P_PASS_REF(obj);
    (void)ExecuteFunction(State->Function);
  }
#endif
}

// by execution
IMPLEMENT_FUNCTION(VObject, GetNextState) {
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->NextState : nullptr);
}

// by declaration
IMPLEMENT_FUNCTION(VObject, GetNextStateInProg) {
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->Next : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsX) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameOfsX : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsY) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameOfsY : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOffset) {
  P_GET_REF(int, dy);
  P_GET_REF(int, dx);
  P_GET_PTR(VState, State);
  if (State) {
    *dx = State->frameOfsX;
    *dy = State->frameOfsY;
  } else {
    *dx = 0;
    *dy = 0;
  }
}

IMPLEMENT_FUNCTION(VObject, GetStateMisc1) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Misc1 : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateMisc2) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Misc2 : 0);
}

IMPLEMENT_FUNCTION(VObject, SetStateMisc1) {
  P_GET_INT(v);
  P_GET_PTR(VState, State);
  if (State) State->Misc1 = v;
}

IMPLEMENT_FUNCTION(VObject, SetStateMisc2) {
  P_GET_INT(v);
  P_GET_PTR(VState, State);
  if (State) State->Misc2 = v;
}

//native static final StateFrameType GetStateTicKind (state State);
IMPLEMENT_FUNCTION(VObject, GetStateTicKind) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->TicType : 0);
}

//native static final int GetStateArgN (state State, int argn);
IMPLEMENT_FUNCTION(VObject, GetStateArgN) {
  P_GET_INT(argn);
  P_GET_PTR(VState, State);
  if (State && argn >= 0 && argn <= 1) {
    RET_INT(argn == 0 ? State->Arg1 : State->Arg2);
  } else {
    RET_INT(0);
  }
}

//native static final void SetStateArgN (state State, int argn, int v);
IMPLEMENT_FUNCTION(VObject, SetStateArgN) {
  P_GET_INT(v);
  P_GET_INT(argn);
  P_GET_PTR(VState, State);
  if (State && argn >= 0 && argn <= 1) {
    if (argn == 0) State->Arg1 = v; else State->Arg2 = v;
  }
}

IMPLEMENT_FUNCTION(VObject, GetStateFRN) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameAction : 0);
}

IMPLEMENT_FUNCTION(VObject, SetStateFRN) {
  P_GET_INT(v);
  P_GET_PTR(VState, State);
  if (State) State->frameAction = v;
}

IMPLEMENT_FUNCTION(VObject, AllClasses) {
  P_GET_PTR(VClass*, Class);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VClassesIterator(BaseClass, Class));
}

IMPLEMENT_FUNCTION(VObject, AllObjects) {
  P_GET_PTR(VObject*, Obj);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VObjectsIterator(BaseClass, Obj));
}

IMPLEMENT_FUNCTION(VObject, AllClassStates) {
  P_GET_PTR(VState *, aout);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VClassStatesIterator(BaseClass, aout));
}


//==========================================================================
//
//  Misc
//
//==========================================================================
// default `skipReplacement` is:
//   `false` for k8vavoom and VCC
//   `false` for vccrun
// native static final spawner Object SpawnObject (class cid, optional bool skipReplacement);
IMPLEMENT_FUNCTION(VObject, SpawnObject) {
#ifdef VCC_STANDALONE_EXECUTOR
  P_GET_BOOL_OPT(skipReplacement, false);
#else
  P_GET_BOOL_OPT(skipReplacement, /*true*/false);
#endif
  P_GET_PTR(VClass, Class);
  if (!Class) { VMDumpCallStack(); Sys_Error("Cannot spawn `none`"); }
  if (skipReplacement) {
    if (Class->ClassFlags&CLASS_Abstract) { VMDumpCallStack(); Sys_Error("Cannot spawn abstract object"); }
  } else {
    if (Class->GetReplacement()->ClassFlags&CLASS_Abstract) { VMDumpCallStack(); Sys_Error("Cannot spawn abstract object"); }
  }
  RET_REF(VObject::StaticSpawnObject(Class, skipReplacement));
}


#include <time.h>
#include <sys/time.h>

/*
#if 0
#if !defined(VCC_STANDALONE_EXECUTOR)
#ifdef _WIN32
static struct tm *localtime_r (const time_t * timep, struct tm *result) {
  // Note: Win32 localtime() is thread-safe
  memcpy(result, localtime(timep), sizeof(struct tm));
  return result;
}
#endif
#endif
#endif
*/


struct TTimeVal {
  vint32 secs; // actually, unsigned
  vint32 usecs;
  // for 2030+
  vint32 secshi;
};


struct TDateTime {
  vint32 sec; // [0..60] (yes, *sometimes* it can be 60)
  vint32 min; // [0..59]
  vint32 hour; // [0..23]
  vint32 month; // [0..11]
  vint32 year; // normal value, i.e. 2042 for 2042
  vint32 mday; // [1..31] -- day of the month
  //
  vint32 wday; // [0..6] -- day of the week (0 is sunday)
  vint32 yday; // [0..365] -- day of the year
  vint32 isdst; // is daylight saving time?
};

//native static final bool GetTimeOfDay (out TTimeVal tv);
IMPLEMENT_FUNCTION(VObject, GetTimeOfDay) {
  P_GET_PTR(TTimeVal, tvres);
  tvres->secshi = 0;
  timeval tv;
  if (gettimeofday(&tv, nullptr)) {
    tvres->secs = 0;
    tvres->usecs = 0;
    RET_BOOL(false);
  } else {
    tvres->secs = (int)(tv.tv_sec&0xffffffff);
    tvres->usecs = (int)tv.tv_usec;
    tvres->secshi = (int)(((uint64_t)tv.tv_sec)>>32);
    RET_BOOL(true);
  }
}


//native static final bool DecodeTimeVal (out TDateTime tm, const ref TTimeVal tv);
IMPLEMENT_FUNCTION(VObject, DecodeTimeVal) {
  P_GET_PTR(TTimeVal, tvin);
  P_GET_PTR(TDateTime, tmres);
  timeval tv;
  tv.tv_sec = (((uint64_t)tvin->secs)&0xffffffff)|(((uint64_t)tvin->secshi)<<32);
  //tv.tv_usec = tvin->usecs;
  tm ctm;
  if (localtime_r(&tv.tv_sec, &ctm)) {
    tmres->sec = ctm.tm_sec;
    tmres->min = ctm.tm_min;
    tmres->hour = ctm.tm_hour;
    tmres->month = ctm.tm_mon;
    tmres->year = ctm.tm_year+1900;
    tmres->mday = ctm.tm_mday;
    tmres->wday = ctm.tm_wday;
    tmres->yday = ctm.tm_yday;
    tmres->isdst = ctm.tm_isdst;
    RET_BOOL(true);
  } else {
#if !defined(IN_VCC)
    memset(tmres, 0, sizeof(*tmres));
#endif
    RET_BOOL(false);
  }
}


//native static final bool EncodeTimeVal (out TTimeVal tv, ref TDateTime tm, optional bool usedst);
IMPLEMENT_FUNCTION(VObject, EncodeTimeVal) {
  P_GET_BOOL_OPT(usedst, false);
  P_GET_PTR(TDateTime, tmin);
  P_GET_PTR(TTimeVal, tvres);
  tm ctm;
  memset(&ctm, 0, sizeof(ctm));
  ctm.tm_sec = tmin->sec;
  ctm.tm_min = tmin->min;
  ctm.tm_hour = tmin->hour;
  ctm.tm_mon = tmin->month;
  ctm.tm_year = tmin->year-1900;
  ctm.tm_mday = tmin->mday;
  //ctm.tm_wday = tmin->wday;
  //ctm.tm_yday = tmin->yday;
  ctm.tm_isdst = tmin->isdst;
  if (!usedst) ctm.tm_isdst = -1;
  auto tt = mktime(&ctm);
  if (tt == (time_t)-1) {
    // oops
#if !defined(IN_VCC)
    memset(tvres, 0, sizeof(*tvres));
#endif
    RET_BOOL(false);
  } else {
    // update it
    tmin->sec = ctm.tm_sec;
    tmin->min = ctm.tm_min;
    tmin->hour = ctm.tm_hour;
    tmin->month = ctm.tm_mon;
    tmin->year = ctm.tm_year+1900;
    tmin->mday = ctm.tm_mday;
    tmin->wday = ctm.tm_wday;
    tmin->yday = ctm.tm_yday;
    tmin->isdst = ctm.tm_isdst;
    // setup tvres
    tvres->secs = (int)(tt&0xffffffff);
    tvres->usecs = 0;
    tvres->secshi = (int)(((uint64_t)tt)>>32);
    RET_BOOL(true);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
//static native final string GetInputKeyName (int kcode);
IMPLEMENT_FUNCTION(VObject, GetInputKeyStrName) {
  P_GET_INT(kcode);
  RET_STR(VObject::NameFromVKey(kcode));
}


//static native final int GetInputKeyCode (string kname);
IMPLEMENT_FUNCTION(VObject, GetInputKeyCode) {
  P_GET_STR(kname);
  RET_INT(VObject::VKeyFromName(kname));
}


//**************************************************************************
//
//  Cvar functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CvarExists) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::HasVar(*name));
}

IMPLEMENT_FUNCTION(VObject, CvarGetFlags) {
  P_GET_NAME(name);
  RET_INT(VCvar::GetVarFlags(*name));
}

IMPLEMENT_FUNCTION(VObject, GetCvarDefault) {
  P_GET_NAME(name);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (!cvar) {
    RET_STR(VStr::EmptyString);
  } else {
    RET_STR(VStr(cvar->GetDefault()));
  }
}

IMPLEMENT_FUNCTION(VObject, CreateCvar) {
  P_GET_INT_OPT(flags, 0);
  P_GET_STR(help);
  P_GET_STR(def);
  P_GET_NAME(name);
  VCvar::CreateNew(name, def, help, flags);
}

IMPLEMENT_FUNCTION(VObject, GetCvar) {
  P_GET_NAME(name);
  RET_INT(VCvar::GetInt(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvar) {
  P_GET_INT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarF) {
  P_GET_NAME(name);
  RET_FLOAT(VCvar::GetFloat(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarF) {
  P_GET_FLOAT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarS) {
  P_GET_NAME(name);
  RET_STR(VCvar::GetString(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarS) {
  P_GET_STR(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarB) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::GetBool(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarB) {
  P_GET_BOOL(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value ? 1 : 0);
}

IMPLEMENT_FUNCTION(VObject, GetCvarHelp) {
  P_GET_NAME(name);
  RET_STR(VStr(VCvar::GetHelp(*name)));
}

//WARNING! must be defined by all api users separately!
/*
IMPLEMENT_FUNCTION(VObject, CvarUnlatchAll) {
#if !defined(VCC_STANDALONE_EXECUTOR)
  if (GGameInfo && GGameInfo->NetMode < NM_DedicatedServer) {
    VCvar::Unlatch();
  }
#endif
}
*/


// native static final bool SetNamePutElement (name setName, name value);
IMPLEMENT_FUNCTION(VObject, SetNamePutElement) {
  P_GET_NAME(value);
  P_GET_NAME(setName);
  RET_BOOL(setNamePut(setName, value));
}


// native static final bool SetNameCheckElement (name setName, name value);
IMPLEMENT_FUNCTION(VObject, SetNameCheckElement) {
  P_GET_NAME(value);
  P_GET_NAME(setName);
  RET_BOOL(setNameCheck(setName, value));
}


// ////////////////////////////////////////////////////////////////////////// //
static TVec RayLineIntersection2D (TVec rayO, TVec rayDir, TVec vv1, TVec vv2) {
  rayDir.z = 0; // not interested
  //rayDir = Normalise(rayDir);
  const float den = (vv2.y-vv1.y)*rayDir.x-(vv2.x-vv1.x)*rayDir.y;
  //if (fabsf(den) < 0.00001f) return rayO;
  const float e = rayO.y-vv1.y;
  const float f = rayO.x-vv1.x;
  const float invden = 1.0f/den;
  if (!isFiniteF(invden)) return rayO;
  const float ua = ((vv2.x-vv1.x)*e-(vv2.y-vv1.y)*f)*invden;
  if (ua >= 0 && ua <= 1) {
    const float ub = (rayDir.x*e-rayDir.y*f)*invden;
    if (ua != 0 || ub != 0) {
      TVec res;
      res.x = rayO.x+ua*rayDir.x;
      res.y = rayO.y+ua*rayDir.y;
      res.z = rayO.z;
      return res;
    }
  }
  return rayO;
}


// native static final TVec RayLineIntersection2D (TVec rayO, TVec rayE, TVec vv1, TVec vv2);
IMPLEMENT_FUNCTION(VObject, RayLineIntersection2D) {
  P_GET_VEC(vv2);
  P_GET_VEC(vv1);
  P_GET_VEC(rayE);
  P_GET_VEC(rayO);
  const TVec rayDir = rayE-rayO;
  RET_VEC(RayLineIntersection2D(rayO, rayDir, vv1, vv2));
}

// native static final TVec RayLineIntersection2DDir (TVec rayO, TVec rayDir, TVec vv1, TVec vv2);
IMPLEMENT_FUNCTION(VObject, RayLineIntersection2DDir) {
  P_GET_VEC(vv2);
  P_GET_VEC(vv1);
  P_GET_VEC(rayDir);
  P_GET_VEC(rayO);
  RET_VEC(RayLineIntersection2D(rayO, rayDir, vv1, vv2));
}


//==========================================================================
//
//  Events
//
//==========================================================================
// native static final bool PostEvent (const ref event_t ev);
IMPLEMENT_FUNCTION(VObject, PostEvent) {
  P_GET_PTR(event_t, ev);
  RET_BOOL(VObject::PostEvent(*ev));
}

// native static final bool InsertEvent (const ref event_t ev);
IMPLEMENT_FUNCTION(VObject, InsertEvent) {
  P_GET_PTR(event_t, ev);
  RET_BOOL(VObject::InsertEvent(*ev));
}

// native static final int CountQueuedEvents ();
IMPLEMENT_FUNCTION(VObject, CountQueuedEvents) {
  RET_INT(VObject::CountQueuedEvents());
}

// native static final bool PeekEvent (int idx, out event_t ev);
IMPLEMENT_FUNCTION(VObject, PeekEvent) {
  P_GET_PTR(event_t, ev);
  P_GET_INT(idx);
  RET_BOOL(VObject::PeekEvent(idx, ev));
}

// native static final bool GetEvent (out event_t ev);
IMPLEMENT_FUNCTION(VObject, GetEvent) {
  P_GET_PTR(event_t, ev);
  RET_BOOL(VObject::GetEvent(ev));
}

// native static final int GetEventQueueSize ();
IMPLEMENT_FUNCTION(VObject, GetEventQueueSize) {
  RET_INT(VObject::GetEventQueueSize());
}


//#include "vc_zastar.h"
#include "vc_zastar.cpp"