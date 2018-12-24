///**************************************************************************
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
//**
//**  Vavoom object base class.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// Define private default constructor.
#define NO_DEFAULT_CONSTRUCTOR(cls) \
  protected: cls() {} public:

// Declare the base VObject class.
#define DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags) \
public: \
  /* Identification */ \
  enum {StaticClassFlags = TStaticFlags|CLASS_Native}; \
  private: static VClass PrivateStaticClass; public: \
  typedef TSuperClass Super; \
  typedef TClass ThisClass; \
  static VClass *StaticClass() { return &PrivateStaticClass; }

// Declare a concrete class.
#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Native) \
  virtual ~TClass() { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) { return Strm << *(VObject**)&Obj; } \
  static void InternalConstructor() { new TClass(); }

// Declare an abstract class.
#define DECLARE_ABSTRACT_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Abstract) \
  virtual ~TClass() { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) { return Strm << *(VObject**)&Obj; }

// Register a class at startup time.
#define IMPLEMENT_CLASS(Pre, TClass) \
  VClass Pre##TClass::PrivateStaticClass \
  ( \
    EC_NativeConstructor, \
    sizeof(Pre##TClass), \
    Pre##TClass::StaticClassFlags, \
    Pre##TClass::Super::StaticClass(), \
    NAME_##TClass, \
    Pre##TClass::InternalConstructor \
  ); \
  VClass *autoclass##Pre##TClass = Pre##TClass::StaticClass();

#define DECLARE_FUNCTION(func) \
  static FBuiltinInfo funcinfo##func; \
  static void exec##func();

#define IMPLEMENT_FUNCTION(TClass, Func) \
  FBuiltinInfo TClass::funcinfo##Func(#Func, TClass::StaticClass(), TClass::exec##Func); \
  void TClass::exec##Func()


// flags describing an object instance
enum EObjectFlags {
  // object Destroy has already been called
  _OF_Destroyed      = 0x00000001,
  // for VaVoom: this thinker is marked for deletion on a next tick
  //             tick processor will take care of calling `Destroy()` on it
  // for VccRun: you can call `CollectGarbage(true)` to destroy that objects
  _OF_DelayedDestroy = 0x00000002,
  // this object is going to be destroyed; only GC will set this flag, and
  // you have to check it in your `ClearReferences()`
  _OF_CleanupRef     = 0x00000004, // this object is goind to be destroyed
};


//==========================================================================
//
//  VObject
//
//==========================================================================

// the base class of all objects
class VObject : public VInterface {
  // declarations
  DECLARE_BASE_CLASS(VObject, VObject, CLASS_Abstract|CLASS_Native)

  // friends
  friend class FObjectIterator;

#if defined(VCC_STANDALONE_EXECUTOR)
# define VCC_OBJECT_DEFAULT_SKIP_REPLACE_ON_SPAWN  false
#else
# define VCC_OBJECT_DEFAULT_SKIP_REPLACE_ON_SPAWN  true
#endif

public:
  struct GCStats {
    int alive; // number of currently alive objects
    int markedDead; // number of objects currently marked as dead
    int lastCollected; // number of objects collected on last non-empty cycle
    int poolSize; // total number of used (including free) slots in object pool
    int poolAllocated; // total number of allocated slots in object pool
    int firstFree; // first free slot in pool
    double lastCollectTime; // in seconds
  };

private:
  // internal per-object variables
  VMethod **vtable; // VaVoom C VMT
  vint32 Index; // index of object into table
  vuint32 UniqueId; // monotonically increasing
  vuint32 ObjectFlags; // private EObjectFlags used by object manager
  VClass *Class; // class the object belongs to

  // private systemwide variables
  //static bool GObjInitialised;
  static TArray<VObject *> GObjObjects; // list of all objects
  static int GNumDeleted;
  static bool GInGarbageCollection;

  static GCStats gcLastStats;

public:
#ifdef VCC_STANDALONE_EXECUTOR
  static bool GImmediadeDelete;
#endif
  static bool GGCMessagesAllowed;
  static bool GCDebugMessagesAllowed;
  static bool (*onExecuteNetMethodCB) (VObject *obj, VMethod *func); // return `false` to do normal execution

public:
  // constructors
  VObject ();
  static void InternalConstructor () { new VObject(); }
  virtual void PostCtor (); // this is called after defaults were blit

  // destructors
  virtual ~VObject () override;

  void *operator new (size_t);
  void *operator new (size_t, const char *, int);
  void operator delete (void *);
  void operator delete (void *, const char *, int);

  // VObject interface
  virtual void Register ();
  virtual void Destroy ();
  virtual void Serialise (VStream &);
  virtual void ClearReferences ();
  virtual bool ExecuteNetMethod (VMethod *);

  // system-wide functions
  static void StaticInit ();
  // call this before exiting from `main()`, so VC can skip some destructor duties
  static void StaticExit ();

  static VObject *StaticSpawnObject (VClass *AClass, bool skipReplacement=VCC_OBJECT_DEFAULT_SKIP_REPLACE_ON_SPAWN);

  // note that you CANNOT use `delete` on VObjects, you have to call
  // either `Destroy()` or (even better) `ConditionalDestroy()`
  // the difference is that `ConditionalDestroy()` will call `Destroy()`
  // by itself, and will do it only once
#if defined(VCC_STANDALONE_EXECUTOR)
  static void CollectGarbage (bool destroyDelayed=false);
#else
  static void CollectGarbage ();
#endif
  static VObject *GetIndexObject (int);

  static int GetObjectsCount ();

  static VFuncRes ExecuteFunction (VMethod *);
  static VFuncRes ExecuteFunctionNoArgs (VMethod *); // `self` should be on the stack
  static void VMDumpCallStack ();
  static void DumpProfile ();
  static void DumpProfileInternal (int type); // <0: only native; >0: only script; 0: everything

  // functions
  void ConditionalDestroy ();

  inline bool IsA (VClass *SomeBaseClass) const {
    for (const VClass *c = Class; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
    return false;
  }

  // accessors
  inline VClass *GetClass () const { return Class; }
  inline vuint32 GetFlags () const { return ObjectFlags; }
  //inline void SetFlags (vuint32 NewFlags) { ObjectFlags |= NewFlags; }
  void SetFlags (vuint32 NewFlags);
  inline void ClearFlags (vuint32 NewFlags) { ObjectFlags &= ~NewFlags; }
  inline vuint32 GetObjectIndex () const { return Index; }
  inline vuint32 GetUniqueId () const { return UniqueId; }

  inline VMethod *GetVFunctionIdx (int InIndex) const { return vtable[InIndex]; }
  inline VMethod *GetVFunction (VName FuncName) const { return vtable[Class->GetMethodIndex(FuncName)]; }

  static VStr NameFromVKey (int vkey);
  static int VKeyFromName (const VStr &kn);

  inline static const GCStats &GetGCStats () { return gcLastStats; }

#include "vc_object_common.h"

#if defined(VCC_STANDALONE_EXECUTOR)
# include "vc_object_vccrun.h"
#elif defined(IN_VCC)
# include "vc_object_vcc.h"
#else
# include "vc_object_vavoom.h"
#endif
};


// dynamically cast an object type-safely
template<class T> T *Cast (VObject *Src) { return (Src && Src->IsA(T::StaticClass()) ? (T*)Src : nullptr); }
template<class T, class U> T *CastChecked (U *Src) {
  if (!Src || !Src->IsA(T::StaticClass())) Sys_Error("Cast to %s failed", T::StaticClass()->GetName());
  return (T*)Src;
}


/*----------------------------------------------------------------------------
  Object iterators.
----------------------------------------------------------------------------*/

// class for iterating through all objects
class FObjectIterator {
protected:
  VClass *Class;
  int Index;

public:
  FObjectIterator (VClass *InClass=VObject::StaticClass()) : Class(InClass), Index(-1) { ++*this; }

  void operator ++ () { while (++Index < VObject::GObjObjects.Num() && (!VObject::GObjObjects[Index] || !VObject::GObjObjects[Index]->IsA(Class))) {} }
  VObject *operator * () { return VObject::GObjObjects[Index]; }
  VObject *operator -> () { return VObject::GObjObjects[Index]; }
  operator bool () { return (Index < VObject::GObjObjects.Num()); }
};


// class for iterating through all objects which inherit from a specified base class
template<class T> class TObjectIterator : public FObjectIterator {
public:
  TObjectIterator () : FObjectIterator(T::StaticClass()) {}

  T *operator * () { return (T*)FObjectIterator::operator*(); }
  T *operator -> () { return (T*)FObjectIterator::operator->(); }
};


// object creation template
template<class T> T *Spawn () { return (T*)VObject::StaticSpawnObject(T::StaticClass()); }
template<class T> T *SpawnWithReplace () { return (T*)VObject::StaticSpawnObject(T::StaticClass(), false); } // don't skip replacement

//inline vuint32 GetTypeHash (VObject *Obj) { return (Obj ? Obj->GetIndex() : 0); }
inline vuint32 GetTypeHash (VObject *Obj) { return (Obj ? Obj->GetUniqueId() : 0); }

// helper macros for implementing native VavoomC functions and calls to the VavoomC methods:
// this will make it simpler to port it to 64 bit platforms

// macros for passign arguments to VavoomC methods
#define P_PASS_INT(v)    PR_Push(v)
#define P_PASS_BYTE(v)   PR_Push(v)
#define P_PASS_FLOAT(v)  PR_Pushf(v)
#define P_PASS_BOOL(v)   PR_Push(v)
#define P_PASS_NAME(v)   PR_PushName(v)
#define P_PASS_STR(v)    PR_PushStr(v)
#define P_PASS_VEC(v)    PR_Pushv(v)
#define P_PASS_AVEC(v)   PR_Pushav(v)
#define P_PASS_REF(v)    PR_PushPtr(v)
#define P_PASS_PTR(v)    PR_PushPtr(v)
#define P_PASS_SELF      PR_PushPtr(this)

// macros for calling VavoomC methods with different return types
#define EV_RET_VOID(v)    (void)ExecuteFunction(GetVFunction(v))
#define EV_RET_INT(v)     return ExecuteFunction(GetVFunction(v)).getInt()
#define EV_RET_BYTE(v)    return ExecuteFunction(GetVFunction(v)).getInt()
#define EV_RET_FLOAT(v)   return ExecuteFunction(GetVFunction(v)).getFloat()
#define EV_RET_BOOL(v)    return !!ExecuteFunction(GetVFunction(v)).getInt()
#define EV_RET_NAME(v)    return ExecuteFunction(GetVFunction(v)).getName()
#define EV_RET_STR(v)     return ExecuteFunction(GetVFunction(v)).getStr()
#define EV_RET_VEC(v)     return ExecuteFunction(GetVFunction(v)).getVector()
//#define EV_RET_AVEC(v)    Sys_Error("Not implemented") /*ExecuteFunction(GetVFunction(v))*/
#define EV_RET_REF(t, v)  return (t *)ExecuteFunction(GetVFunction(v)).getObject()
#define EV_RET_PTR(t, v)  return (t *)ExecuteFunction(GetVFunction(v)).getClass()

#define EV_RET_VOID_IDX(v)    (void)ExecuteFunction(GetVFunctionIdx(v))
#define EV_RET_INT_IDX(v)     return ExecuteFunction(GetVFunctionIdx(v)).getInt()
#define EV_RET_BYTE_IDX(v)    return ExecuteFunction(GetVFunctionIdx(v)).getInt()
#define EV_RET_FLOAT_IDX(v)   return ExecuteFunction(GetVFunctionIdx(v)).getFloat()
#define EV_RET_BOOL_IDX(v)    return !!ExecuteFunction(GetVFunctionIdx(v)).getInt()
#define EV_RET_NAME_IDX(v)    return ExecuteFunction(GetVFunctionIdx(v)).getName()
#define EV_RET_STR_IDX(v)     return ExecuteFunction(GetVFunctionIdx(v)).getStr()
#define EV_RET_VEC_IDX(v)     return ExecuteFunction(GetVFunctionIdx(v)).getVector()
//#define EV_RET_AVEC_IDX(v)    return ExecuteFunction(GetVFunctionIdx(v)).getVector()
#define EV_RET_REF_IDX(t, v)  return (t *)ExecuteFunction(GetVFunctionIdx(v)).getObject()
#define EV_RET_PTR_IDX(t, v)  return (t *)ExecuteFunction(GetVFunctionIdx(v)).getClass()

// parameter get macros; parameters must be retrieved in backwards order
#define P_GET_INT(v)     vint32 v = PR_Pop()
#define P_GET_BYTE(v)    vuint8 v = PR_Pop()
#define P_GET_FLOAT(v)   float v = PR_Popf()
#define P_GET_BOOL(v)    bool v = !!PR_Pop()
#define P_GET_NAME(v)    VName v = PR_PopName()
#define P_GET_STR(v)     VStr v = PR_PopStr()
#define P_GET_VEC(v)     TVec v = PR_Popv()
#define P_GET_AVEC(v)    TAVec v = PR_Popav()
#define P_GET_REF(c, v)  c *v = (c*)PR_PopPtr()
#define P_GET_PTR(t, v)  t *v = (t*)PR_PopPtr()
#define P_GET_SELF       ThisClass *Self = (ThisClass*)PR_PopPtr()

#define P_GET_INT_OPT(v, d)     bool specified_##v = !!PR_Pop(); vint32 v = PR_Pop(); if (!specified_##v) v = d
#define P_GET_BYTE_OPT(v, d)    bool specified_##v = !!PR_Pop(); vuint8 v = PR_Pop(); if (!specified_##v) v = d
#define P_GET_FLOAT_OPT(v, d)   bool specified_##v = !!PR_Pop(); float v = PR_Popf(); if (!specified_##v) v = d
#define P_GET_BOOL_OPT(v, d)    bool specified_##v = !!PR_Pop(); bool v = !!PR_Pop(); if (!specified_##v) v = d
#define P_GET_NAME_OPT(v, d)    bool specified_##v = !!PR_Pop(); VName v = PR_PopName(); if (!specified_##v) v = d
#define P_GET_STR_OPT(v, d)     bool specified_##v = !!PR_Pop(); VStr v = PR_PopStr(); if (!specified_##v) v = d
#define P_GET_VEC_OPT(v, d)     bool specified_##v = !!PR_Pop(); TVec v = PR_Popv(); if (!specified_##v) v = d
#define P_GET_AVEC_OPT(v, d)    bool specified_##v = !!PR_Pop(); TAVec v = PR_Popav(); if (!specified_##v) v = d
#define P_GET_REF_OPT(c, v, d)  bool specified_##v = !!PR_Pop(); c *v = (c*)PR_PopPtr(); if (!specified_##v) v = d
#define P_GET_PTR_OPT(t, v, d)  bool specified_##v = !!PR_Pop(); t *v = (t*)PR_PopPtr(); if (!specified_##v) v = d

// method return macros
#define RET_INT(v)    PR_Push(v)
#define RET_BYTE(v)   PR_Push(v)
#define RET_FLOAT(v)  PR_Pushf(v)
#define RET_BOOL(v)   PR_Push(v)
#define RET_NAME(v)   PR_PushName(v)
#define RET_STR(v)    PR_PushStr(v)
#define RET_VEC(v)    PR_Pushv(v)
#define RET_AVEC(v)   PR_Pushav(v)
#define RET_REF(v)    PR_PushPtr(v)
#define RET_PTR(v)    PR_PushPtr(v)


// ////////////////////////////////////////////////////////////////////////// //
class VScriptIterator : public VInterface {
public:
  virtual bool GetNext() = 0;
  // by default, the following does `delete this;`
  virtual void Finished ();
};


// ////////////////////////////////////////////////////////////////////////// //
#if !defined(IN_VCC)
static __attribute__((unused)) inline void vobj_get_param (int &n) { n = PR_Pop(); }
static __attribute__((unused)) inline void vobj_get_param (float &n) { n = PR_Popf(); }
static __attribute__((unused)) inline void vobj_get_param (double &n) { n = PR_Popf(); }
static __attribute__((unused)) inline void vobj_get_param (bool &n) { n = !!PR_Pop(); }
static __attribute__((unused)) inline void vobj_get_param (VStr &n) { n = PR_PopStr(); }
static __attribute__((unused)) inline void vobj_get_param (VName &n) { n = PR_PopName(); }
static __attribute__((unused)) inline void vobj_get_param (int *&n) { n = (int *)PR_PopPtr(); }
template<class C> static __attribute__((unused)) inline void vobj_get_param (C *&n) { n = (C *)PR_PopPtr(); }

template<typename T, typename... Args> static __attribute__((unused)) inline void vobj_get_param (T &n, Args&... args) {
  vobj_get_param(args...);
  vobj_get_param(n);
}


template<typename... Args> static __attribute__((unused)) inline void vobjGetParam (Args&... args) {
  vobj_get_param(args...);
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
// keys and buttons
enum {
  K_ESCAPE = 27,
  K_ENTER = 13,
  K_TAB = 9,
  K_BACKSPACE = 8,

  K_SPACE = 32,

  K_N0 = 48, K_N1, K_N2, K_N3, K_N4, K_N5, K_N6, K_N7, K_N8, K_N9,

  K_a = 97, K_b, K_c, K_d, K_e, K_f,  K_g, K_h, K_i, K_j, K_k, K_l,
  K_m, K_n, K_o, K_p, K_q, K_r, K_s, K_t, K_u, K_v, K_w, K_x, K_y, K_z,

  K_FIRST_CONTROL_KEYCODE = 0x80,

  K_UPARROW = 0x80, K_LEFTARROW, K_RIGHTARROW, K_DOWNARROW,
  K_INSERT, K_DELETE, K_HOME, K_END, K_PAGEUP, K_PAGEDOWN,

  K_PAD0, K_PAD1, K_PAD2, K_PAD3, K_PAD4, K_PAD5, K_PAD6, K_PAD7, K_PAD8, K_PAD9,

  K_NUMLOCK,
  K_PADDIVIDE, K_PADMULTIPLE,
  K_PADMINUS, K_PADPLUS,
  K_PADENTER, K_PADDOT,

  K_CAPSLOCK,
  K_BACKQUOTE,

  K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,

  K_LSHIFT, K_RSHIFT,
  K_LCTRL, K_RCTRL,
  K_LALT, K_RALT,

  K_LWIN, K_RWIN,
  K_MENU,

  K_PRINTSCRN,
  K_SCROLLLOCK,
  K_PAUSE,

  K_MOUSE1, K_MOUSE2, K_MOUSE3, K_MOUSE4, K_MOUSE5,
  K_MOUSE6, K_MOUSE7, K_MOUSE8, K_MOUSE9,
  K_MWHEELUP, K_MWHEELDOWN,

  K_JOY1, K_JOY2, K_JOY3, K_JOY4, K_JOY5, K_JOY6, K_JOY7, K_JOY8, K_JOY9,
  K_JOY10, K_JOY11, K_JOY12, K_JOY13, K_JOY14, K_JOY15, K_JOY16,

  K_MOUSE_FIRST = K_MOUSE1,
  K_MOUSE_LAST = K_MWHEELDOWN,

  K_MOUSE_BUTTON_FIRST = K_MOUSE1,
  K_MOUSE_BUTTON_LAST = K_MOUSE9,

  K_JOY_FIRST = K_JOY1,
  K_JOY_LAST = K_JOY16,

  K_KEYCODE_MAX,
};

static_assert(K_KEYCODE_MAX < 256, "too many keycodes");

// input event types
enum {
  ev_keydown,
  ev_keyup,
  ev_mouse,
  ev_joystick,
  // extended events for vcc_run
  ev_winfocus, // data1: focused
  ev_timer, // data1: timer id
  ev_closequery, // data1: !=0 -- system shutdown
  ev_user = 666,
};

enum {
  EFlag_Eaten = 1U<<0,
  EFlag_Cancelled = 1U<<1,
  EFlag_Bubbling = 1U<<2, // this event is "bubbling up"
};

enum {
  bCtrl = 1U<<0,
  bAlt = 1U<<1,
  bShift = 1U<<2,
  bHyper = 1U<<3,
  bLMB = 1U<<4,
  bMMB = 1U<<5,
  bRMB = 1U<<6,
  bCtrlLeft = 1U<<7,
  bAltLeft = 1U<<8,
  bShiftLeft = 1U<<9,
  bCtrlRight = 1U<<10,
  bAltRight = 1U<<11,
  bShiftRight = 1U<<12,
};

// event structure
struct event_t {
  vint32 type; // event type
  vint32 data1; // keys / mouse / joystick buttons
  vint32 data2; // mouse / joystick x move
  vint32 data3; // mouse / joystick y move
  VObject *obj;
  VObject *dest;
  vuint32 flags; // EFlag_XXX
  vuint32 modflags;
};
