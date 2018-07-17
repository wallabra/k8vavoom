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


// ////////////////////////////////////////////////////////////////////////// //
struct dprograms_t;

union VStack {
  vint32 i;
  vuint32 u;
  float f;
  void *p;
};


// ////////////////////////////////////////////////////////////////////////// //
extern void PR_Init ();
extern void PR_OnAbort ();


// ////////////////////////////////////////////////////////////////////////// //
extern VStack *pr_stackPtr;


// ////////////////////////////////////////////////////////////////////////// //
// stack routines

inline void PR_Push (int value) {
  pr_stackPtr->i = value;
  ++pr_stackPtr;
}


inline int PR_Pop () {
  --pr_stackPtr;
  return pr_stackPtr->i;
}


inline void PR_Pushf (float value) {
  pr_stackPtr->f = value;
  ++pr_stackPtr;
}


inline float PR_Popf () {
  --pr_stackPtr;
  return pr_stackPtr->f;
}


inline void PR_Pushv (const TVec &v) {
  PR_Pushf(v.x);
  PR_Pushf(v.y);
  PR_Pushf(v.z);
}


inline void PR_Pushav (const TAVec &v) {
  PR_Pushf(v.pitch);
  PR_Pushf(v.yaw);
  PR_Pushf(v.roll);
}


inline TVec PR_Popv () {
  TVec v;
  v.z = PR_Popf();
  v.y = PR_Popf();
  v.x = PR_Popf();
  return v;
}


inline TAVec PR_Popav () {
  TAVec v;
  v.roll = PR_Popf();
  v.yaw = PR_Popf();
  v.pitch = PR_Popf();
  return v;
}


inline void PR_PushName (VName value) {
  pr_stackPtr->i = value.GetIndex();
  ++pr_stackPtr;
}


inline VName PR_PopName () {
  --pr_stackPtr;
  return *(VName*)&pr_stackPtr->i;
}


inline void PR_PushPtr (void *value) {
  pr_stackPtr->p = value;
  ++pr_stackPtr;
}


inline void *PR_PopPtr () {
  --pr_stackPtr;
  return pr_stackPtr->p;
}


inline void PR_PushStr (const VStr &value) {
  pr_stackPtr->p = nullptr;
  *(VStr*)&pr_stackPtr->p = value;
  pr_stackPtr++;
}


inline VStr PR_PopStr () {
  --pr_stackPtr;
  VStr Ret = *(VStr*)&pr_stackPtr->p;
  ((VStr*)&pr_stackPtr->p)->Clean();
  return Ret;
}


// ////////////////////////////////////////////////////////////////////////// //
extern VStr PF_FormatString ();

class VFieldType;
extern void PR_WriteOne (const VFieldType &type);
extern void PR_WriteFlush ();
