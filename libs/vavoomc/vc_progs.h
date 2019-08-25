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


// ////////////////////////////////////////////////////////////////////////// //
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
extern volatile unsigned vmAbortBySignal;



// ////////////////////////////////////////////////////////////////////////// //
// stack routines
inline void PR_Push (int value) {
#if !defined(IN_VCC)
  pr_stackPtr->i = value;
  ++pr_stackPtr;
#endif
}


inline int PR_Pop () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  return pr_stackPtr->i;
#else
  return 0;
#endif
}


inline void PR_Pushf (float value) {
#if !defined(IN_VCC)
  pr_stackPtr->f = value;
  ++pr_stackPtr;
#endif
}


inline float PR_Popf () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  return pr_stackPtr->f;
#else
  return 0;
#endif
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
#if !defined(IN_VCC)
  pr_stackPtr->i = value.GetIndex();
  ++pr_stackPtr;
#endif
}


inline VName PR_PopName () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  return *(VName*)&pr_stackPtr->i;
#else
  return NAME_None;
#endif
}


inline void PR_PushPtr (void *value) {
#if !defined(IN_VCC)
  pr_stackPtr->p = value;
  ++pr_stackPtr;
#endif
}


inline void *PR_PopPtr () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  return pr_stackPtr->p;
#else
  return nullptr;
#endif
}


inline VObject *PR_PopRef () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  return (VObject *)(pr_stackPtr->p);
#else
  return nullptr;
#endif
}


inline void PR_PushStr (VStr value) {
#if !defined(IN_VCC)
  pr_stackPtr->p = nullptr;
  *(VStr*)&pr_stackPtr->p = value;
  pr_stackPtr++;
#endif
}


inline VStr PR_PopStr () {
#if !defined(IN_VCC)
  --pr_stackPtr;
  VStr Ret = *(VStr*)&pr_stackPtr->p;
  ((VStr*)&pr_stackPtr->p)->Clean();
  return Ret;
#else
  return VStr();
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
extern VStr PF_FormatString ();

class VFieldType;
extern void PR_WriteOne (const VFieldType &type);
extern void PR_WriteFlush ();

// if `buf` is `nullptr`, it means "flush"
extern void (*PR_WriterCB) (const char *buf, bool debugPrint, VName wrname);

// calls `PR_WriterCB` if it is not empty, or does default printing
// if `buf` is `nullptr`, it means "flush"
extern void PR_DoWriteBuf (const char *buf, bool debugPrint=false, VName wrname=NAME_None);
