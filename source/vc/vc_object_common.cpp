//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

//**************************************************************************
//
//  Basic functions
//
//**************************************************************************

IMPLEMENT_FUNCTION(VObject, get_GCMessagesAllowed) { RET_BOOL(GGCMessagesAllowed); }
IMPLEMENT_FUNCTION(VObject, set_GCMessagesAllowed) { P_GET_BOOL(val); GGCMessagesAllowed = val; }


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
      //Self->SetFlags(_OF_DelayedDestroy);
      Self->ConditionalDestroy();
    }
#else
    delete Self;
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
IMPLEMENT_FUNCTION(VObject, CollectGarbage) {
  P_GET_BOOL_OPT(destroyDelayed, false);
  CollectGarbage(destroyDelayed);
}


//**************************************************************************
//
//  Error functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Error) {
  Host_Error("%s", *PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, FatalError) {
  Sys_Error("%s", *PF_FormatString());
}


//**************************************************************************
//
//  Math functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, AngleMod360) {
  P_GET_FLOAT(an);
  RET_FLOAT(AngleMod(an));
}

IMPLEMENT_FUNCTION(VObject, AngleMod180) {
  P_GET_FLOAT(an);
  RET_FLOAT(AngleMod180(an));
}

IMPLEMENT_FUNCTION(VObject, AngleVectors) {
  P_GET_PTR(TVec, vup);
  P_GET_PTR(TVec, vright);
  P_GET_PTR(TVec, vforward);
  P_GET_PTR(TAVec, angles);
  AngleVectors(*angles, *vforward, *vright, *vup);
}

IMPLEMENT_FUNCTION(VObject, AngleVector) {
  P_GET_PTR(TVec, vec);
  P_GET_PTR(TAVec, angles);
  AngleVector(*angles, *vec);
}

IMPLEMENT_FUNCTION(VObject, VectorAngles) {
  P_GET_PTR(TAVec, angles);
  P_GET_PTR(TVec, vec);
  VectorAngles(*vec, *angles);
}

IMPLEMENT_FUNCTION(VObject, GetPlanePointZ) {
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  // /*if (plane->normal.z == 0)*/ { fprintf(stderr, "*** %p; dist=%f; normal=(%f,%f,%f)\n", plane, plane->dist, plane->normal.x, plane->normal.y, plane->normal.z); /*VObject::VMDumpCallStack(); Sys_Error("FUUUUUUU");*/ }
  RET_FLOAT(plane->GetPointZ(point));
}

IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide) {
  P_GET_PTR(TPlane, plane);
  P_GET_VEC(point);
  RET_INT(plane->PointOnSide(point));
}

IMPLEMENT_FUNCTION(VObject, RotateDirectionVector) {
  P_GET_AVEC(rot);
  P_GET_VEC(vec);

  TAVec angles;
  TVec out;

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

  float dstx = vec->x * mcos(angle) - vec->y * msin(angle);
  float dsty = vec->x * msin(angle) + vec->y * mcos(angle);

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
  RET_BOOL(plane->normal.z == 1.0);
}

//native static final bool IsPlainCeiling (const ref TPlane plane); // valid only for ceilings
IMPLEMENT_FUNCTION(VObject, IsPlainCeiling) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(plane->normal.z == -1.0);
}

//native static final bool IsSlopedFlat (const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, IsSlopedFlat) {
  P_GET_PTR(TPlane, plane);
  RET_BOOL(fabs(plane->normal.z) != 1.0);
}


//**************************************************************************
//
//  String functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, strlen) {
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

IMPLEMENT_FUNCTION(VObject, strcat) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_STR(s1 + s2);
}

IMPLEMENT_FUNCTION(VObject, strlwr) {
  P_GET_STR(s);
  RET_STR(s.ToLower());
}

IMPLEMENT_FUNCTION(VObject, strupr) {
  P_GET_STR(s);
  RET_STR(s.ToUpper());
}

IMPLEMENT_FUNCTION(VObject, substr) {
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

IMPLEMENT_FUNCTION(VObject, va) {
  RET_STR(PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, atoi) {
  P_GET_STR(str);
  RET_INT(atoi(*str));
}

IMPLEMENT_FUNCTION(VObject, atof) {
  P_GET_STR(str);
  RET_FLOAT(atof(*str));
}

IMPLEMENT_FUNCTION(VObject, StrStartsWith) {
  P_GET_STR(Check);
  P_GET_STR(Str);
  RET_BOOL(Str.StartsWith(Check));
}

IMPLEMENT_FUNCTION(VObject, StrEndsWith) {
  P_GET_STR(Check);
  P_GET_STR(Str);
  RET_BOOL(Str.EndsWith(Check));
}

IMPLEMENT_FUNCTION(VObject, StrReplace) {
  P_GET_STR(Replacement);
  P_GET_STR(Search);
  P_GET_STR(Str);
  RET_STR(Str.Replace(Search, Replacement));
}


//**************************************************************************
//
//  Random numbers
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Random) {
#if defined(VCC_STANDALONE_EXECUTOR)
  vuint32 rn;
  float res;
  for (;;) {
    ed25519_randombytes(&rn, sizeof(rn));
    res = (float)(rn&0x3ffff)/(float)(0x3ffff);
    if (res < 1.0) break;
  }
  RET_FLOAT(res);
#else
  RET_FLOAT(Random());
#endif
}

IMPLEMENT_FUNCTION(VObject, FRandomFull) {
#if defined(VCC_STANDALONE_EXECUTOR)
  vuint32 rn;
  ed25519_randombytes(&rn, sizeof(rn));
  float res = (float)(rn&0x3ffff)/(float)(0x3ffff);
  RET_FLOAT(res);
#else
  RET_FLOAT(RandomFull());
#endif
}

IMPLEMENT_FUNCTION(VObject, GenRandomSeedU32) {
  vint32 rn;
  do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
  RET_INT(rn);
}

IMPLEMENT_FUNCTION(VObject, P_Random) {
  RET_INT(rand()&0xff);
}


// http://burtleburtle.net/bob/rand/smallprng.html
struct BJPRNGCtx {
  vuint32 a, b, c, d;
};

#define bjprng_rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static inline vuint32 ranval (BJPRNGCtx *x) {
  vuint32 e = x->a-bjprng_rot(x->b, 27);
  x->a = x->b^bjprng_rot(x->c, 17);
  x->b = x->c+x->d;
  x->c = x->d+e;
  x->d = e+x->a;
  return x->d;
}

static inline void raninit (BJPRNGCtx *x, vuint32 seed) {
  x->a = 0xf1ea5eed;
  x->b = x->c = x->d = seed;
  for (unsigned i = 0; i < 20; ++i) (void)ranval(x);
}


// native static final void bjprngSeed (out BJPRNGCtx ctx, int aseed);
IMPLEMENT_FUNCTION(VObject, bjprngSeed) {
  P_GET_INT(aseed);
  P_GET_PTR(BJPRNGCtx, ctx);
  if (ctx) raninit(ctx, (vuint32)aseed);
}

// full 32-bit value (so it can be negative)
//native static final int bjprngNext (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNext) {
  P_GET_PTR(BJPRNGCtx, ctx);
  RET_INT(ctx ? ranval(ctx) : 0);
}

// [0..1) (WARNING! not really uniform!)
//native static final float bjprngNextFloat (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNextFloat) {
  P_GET_PTR(BJPRNGCtx, ctx);
  if (ctx) {
    for (;;) {
      float v = ((double)ranval(ctx))/((double)0xffffffffu);
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
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Log(PF_FormatString());
#else
  fprintf(stdout, "%s\n", *PF_FormatString());
#endif
}

IMPLEMENT_FUNCTION(VObject, dprint) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Log(NAME_Dev, PF_FormatString());
#else
  fprintf(stderr, "%s\n", *PF_FormatString());
#endif
}


//==========================================================================
//
//  Type conversions
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, itof) {
  P_GET_INT(x);
  RET_FLOAT((float)x);
}

IMPLEMENT_FUNCTION(VObject, ftoi) {
  P_GET_FLOAT(x);
  RET_INT((vint32)x);
}

IMPLEMENT_FUNCTION(VObject, StrToName) {
  P_GET_STR(str);
  RET_NAME(VName(*str));
}

IMPLEMENT_FUNCTION(VObject, NameToStr) {
  P_GET_NAME(Name);
  RET_STR(*Name);
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

IMPLEMENT_FUNCTION(VObject, FindClassLowerCase) {
  P_GET_NAME(Name);
  RET_PTR(VClass::FindClassLowerCase(Name));
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

IMPLEMENT_FUNCTION(VObject, GetClassParent) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->ParentClass : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacement) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacement() : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacee) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacee() : nullptr);
}

IMPLEMENT_FUNCTION(VObject, FindClassState) {
  P_GET_NAME(StateName);
  P_GET_PTR(VClass, Cls);
  VStateLabel *Lbl = Cls->FindStateLabel(StateName);
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
IMPLEMENT_FUNCTION(VObject, FindMObjId) {
  P_GET_NAME_OPT(pkgname, NAME_None);
  P_GET_INT(id);
  RET_REF(VMemberBase::StaticFindMObj(id, pkgname));
}

IMPLEMENT_FUNCTION(VObject, FindScriptId) {
  P_GET_NAME_OPT(pkgname, NAME_None);
  P_GET_INT(id);
  RET_REF(VMemberBase::StaticFindScriptId(id, pkgname));
}

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
  RET_FLOAT(State ? State->Time : 0.0);
}

IMPLEMENT_FUNCTION(VObject, GetStatePlus) {
  P_GET_BOOL_OPT(IgnoreJump, false);
  P_GET_INT(Offset);
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->GetPlus(Offset, IgnoreJump) : nullptr);
}

IMPLEMENT_FUNCTION(VObject, StateHasAction) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? !!State->Function : false);
}

IMPLEMENT_FUNCTION(VObject, CallStateAction) {
  P_GET_PTR(VState, State);
  P_GET_PTR(VObject, obj);
  if (State && State->Function) {
    P_PASS_REF(obj);
    ExecuteFunction(State->Function);
  }
}

IMPLEMENT_FUNCTION(VObject, GetNextState) {
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->NextState : nullptr);
}

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
//   `true` for VaVoom and VCC
//   `false` for vccrun
// native static final spawner Object SpawnObject (class cid, optional bool skipReplacement);
IMPLEMENT_FUNCTION(VObject, SpawnObject) {
#ifdef VCC_STANDALONE_EXECUTOR
  P_GET_BOOL_OPT(skipReplacement, false);
#else
  P_GET_BOOL_OPT(skipReplacement, true);
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

#if !defined(VCC_STANDALONE_EXECUTOR)
#ifdef _WIN32
static struct tm *localtime_r (const time_t * timep, struct tm *result) {
  /* Note: Win32 localtime() is thread-safe */
  memcpy(result, localtime(timep), sizeof(struct tm));
  return result;
}
#endif
#endif


struct TTimeVal {
  int secs; // actually, unsigned
  int usecs;
  // for 2030+
  int secshi;
};


struct TDateTime {
  int sec; // [0..60] (yes, *sometimes* it can be 60)
  int min; // [0..59]
  int hour; // [0..23]
  int month; // [0..11]
  int year; // normal value, i.e. 2042 for 2042
  int mday; // [1..31] -- day of the month
  //
  int wday; // [0..6] -- day of the week (0 is sunday)
  int yday; // [0..365] -- day of the year
  int isdst; // is daylight saving time?
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
    memset(tmres, 0, sizeof(*tmres));
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
    memset(tvres, 0, sizeof(*tvres));
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
