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
  typedef TSuperClass Super;\
  typedef TClass ThisClass;\
  static VClass *StaticClass() \
    { return &PrivateStaticClass; }

// Declare a concrete class.
#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Native) \
  virtual ~TClass() \
    { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) \
    { return Strm << *(VObject**)&Obj; } \
  static void InternalConstructor() \
    { new TClass(); }

// Declare an abstract class.
#define DECLARE_ABSTRACT_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Abstract) \
  virtual ~TClass() \
    { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) \
    { return Strm << *(VObject**)&Obj; }

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
  FBuiltinInfo TClass::funcinfo##Func(#Func, TClass::StaticClass(), \
    TClass::exec##Func); \
  void TClass::exec##Func()

// ENUMERATIONS ------------------------------------------------------------

//
// Flags describing an object instance.
//
enum EObjectFlags {
  _OF_Destroyed      = 0x00000001, // object Destroy has already been called
  _OF_DelayedDestroy = 0x00000002,
  _OF_CleanupRef     = 0x00000004,
};

// TYPES -------------------------------------------------------------------

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

private:
  // internal per-object variables
  VMethod **vtable;
  vint32 Index; // index of object into table
  vuint32 ObjectFlags; // private EObjectFlags used by object manager
  VClass *Class; // class the object belongs to

  // private systemwide variables
  static bool GObjInitialised;
  static TArray<VObject*> GObjObjects; // list of all objects.
  static TArray<int> GObjAvailable; // available object indices
  static VObject *GObjHash[4096]; // object hash
  static int GNumDeleted;
  static bool GInGarbageCollection;
  static void *GNewObject; // for internal constructors

public:
#ifdef VCC_STANDALONE_EXECUTOR
  static bool GImmediadeDelete;
#endif
  static bool GGCMessagesAllowed;
  static bool (*onExecuteNetMethodCB) (VObject *obj, VMethod *func); // return `false` to do normal execution

public:
  // constructors
  VObject ();
  static void InternalConstructor () { new VObject(); }

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
  static void StaticExit ();
  static VObject *StaticSpawnObject (VClass *);
  static void CollectGarbage (bool destroyDelayed=false);
  static VObject *GetIndexObject (int);
  static int GetObjectsCount ();

  static VStack ExecuteFunction (VMethod *);
  static void VMDumpCallStack ();
  static void DumpProfile ();

  // functions
  bool ConditionalDestroy ();
  bool IsA (VClass *SomeBaseClass) const;

  // accessors
  inline VClass *GetClass () const { return Class; }
  inline vuint32 GetFlags () const { return ObjectFlags; }
  inline void SetFlags (vuint32 NewFlags) { ObjectFlags |= NewFlags; }
  inline void ClearFlags (vuint32 NewFlags) { ObjectFlags &= ~NewFlags; }
  inline vuint32 GetIndex () const { return Index; }
  inline VMethod *GetVFunctionIdx (int InIndex) const { return vtable[InIndex]; }

  VMethod *GetVFunction (VName FuncName) const;

  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(IsA)
  DECLARE_FUNCTION(IsDestroyed)
  DECLARE_FUNCTION(CollectGarbage)
#ifdef VCC_STANDALONE_EXECUTOR
  DECLARE_FUNCTION(get_ImmediateDelete)
  DECLARE_FUNCTION(set_ImmediateDelete)
#endif
  DECLARE_FUNCTION(get_GCMessagesAllowed)
  DECLARE_FUNCTION(set_GCMessagesAllowed)

  // error functions
  DECLARE_FUNCTION(Error)
  DECLARE_FUNCTION(FatalError)

#ifndef VCC_STANDALONE_EXECUTOR
  // cvar functions
  DECLARE_FUNCTION(CvarExists)
  DECLARE_FUNCTION(CreateCvar)
  DECLARE_FUNCTION(GetCvar)
  DECLARE_FUNCTION(SetCvar)
  DECLARE_FUNCTION(GetCvarF)
  DECLARE_FUNCTION(SetCvarF)
  DECLARE_FUNCTION(GetCvarS)
  DECLARE_FUNCTION(SetCvarS)
  DECLARE_FUNCTION(GetCvarB)
#endif

  // math functions
  DECLARE_FUNCTION(AngleMod360)
  DECLARE_FUNCTION(AngleMod180)
  DECLARE_FUNCTION(AngleVectors)
  DECLARE_FUNCTION(AngleVector)
  DECLARE_FUNCTION(VectorAngles)
  DECLARE_FUNCTION(GetPlanePointZ)
  DECLARE_FUNCTION(PointOnPlaneSide)
  DECLARE_FUNCTION(RotateDirectionVector)
  DECLARE_FUNCTION(VectorRotateAroundZ)
  DECLARE_FUNCTION(RotateVectorAroundVector)

  // string functions
  DECLARE_FUNCTION(strlen)
  DECLARE_FUNCTION(strcmp)
  DECLARE_FUNCTION(stricmp)
  DECLARE_FUNCTION(strcat)
  DECLARE_FUNCTION(strlwr)
  DECLARE_FUNCTION(strupr)
  DECLARE_FUNCTION(substr)
  DECLARE_FUNCTION(strmid)
  DECLARE_FUNCTION(strleft)
  DECLARE_FUNCTION(strright)
  DECLARE_FUNCTION(strrepeat)
  DECLARE_FUNCTION(strFromChar)
  DECLARE_FUNCTION(strFromCharUtf8)
  DECLARE_FUNCTION(strFromInt)
  DECLARE_FUNCTION(strFromFloat)
  DECLARE_FUNCTION(va)
  DECLARE_FUNCTION(atoi)
  DECLARE_FUNCTION(atof)
  DECLARE_FUNCTION(StrStartsWith)
  DECLARE_FUNCTION(StrEndsWith)
  DECLARE_FUNCTION(StrReplace)

  // random numbers
  DECLARE_FUNCTION(Random)
  DECLARE_FUNCTION(P_Random)

#ifndef VCC_STANDALONE_EXECUTOR
  // textures
  DECLARE_FUNCTION(CheckTextureNumForName)
  DECLARE_FUNCTION(TextureNumForName)
  DECLARE_FUNCTION(CheckFlatNumForName)
  DECLARE_FUNCTION(FlatNumForName)
  DECLARE_FUNCTION(TextureHeight)
  DECLARE_FUNCTION(GetTextureName)
#endif

  // printing in console
  DECLARE_FUNCTION(print)
  DECLARE_FUNCTION(dprint)

  // type conversions
  DECLARE_FUNCTION(itof)
  DECLARE_FUNCTION(ftoi)
  DECLARE_FUNCTION(StrToName)
  DECLARE_FUNCTION(NameToStr)

#ifndef VCC_STANDALONE_EXECUTOR
  // console command functions
  DECLARE_FUNCTION(Cmd_CheckParm)
  DECLARE_FUNCTION(Cmd_GetArgC)
  DECLARE_FUNCTION(Cmd_GetArgV)
  DECLARE_FUNCTION(CmdBuf_AddText)
#endif

  // class methods
  DECLARE_FUNCTION(FindClass)
  DECLARE_FUNCTION(FindClassLowerCase)
  DECLARE_FUNCTION(ClassIsChildOf)
  DECLARE_FUNCTION(GetClassName)
  DECLARE_FUNCTION(GetClassParent)
  DECLARE_FUNCTION(GetClassReplacement)
  DECLARE_FUNCTION(GetClassReplacee)
  DECLARE_FUNCTION(FindClassState)
  DECLARE_FUNCTION(GetClassNumOwnedStates)
  DECLARE_FUNCTION(GetClassFirstState)

  // state methods
  DECLARE_FUNCTION(StateIsInRange)
  DECLARE_FUNCTION(StateIsInSequence)
  DECLARE_FUNCTION(GetStateSpriteName)
  DECLARE_FUNCTION(GetStateSpriteFrame)
  DECLARE_FUNCTION(GetStateSpriteFrameWidth)
  DECLARE_FUNCTION(GetStateSpriteFrameHeight)
  DECLARE_FUNCTION(GetStateSpriteFrameSize)
  DECLARE_FUNCTION(GetStateDuration)
  DECLARE_FUNCTION(GetStatePlus)
  DECLARE_FUNCTION(GetNextState)
  DECLARE_FUNCTION(GetNextStateInProg)
  DECLARE_FUNCTION(StateHasAction)
  DECLARE_FUNCTION(CallStateAction)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsX)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsY)
  DECLARE_FUNCTION(GetStateSpriteFrameOffset)
  DECLARE_FUNCTION(GetStateMisc1)
  DECLARE_FUNCTION(GetStateMisc2)
  DECLARE_FUNCTION(SetStateMisc1)
  DECLARE_FUNCTION(SetStateMisc2)
#ifndef VCC_STANDALONE_EXECUTOR
  DECLARE_FUNCTION(AreStateSpritesPresent)
#endif

  // Iterators
  DECLARE_FUNCTION(AllObjects)
  DECLARE_FUNCTION(AllClasses)
  DECLARE_FUNCTION(AllClassStates)

#ifndef VCC_STANDALONE_EXECUTOR
  // misc
  DECLARE_FUNCTION(Info_ValueForKey)
  DECLARE_FUNCTION(WadLumpPresent)
#endif
  DECLARE_FUNCTION(SpawnObject)
#ifndef VCC_STANDALONE_EXECUTOR
  DECLARE_FUNCTION(FindAnimDoor)
  DECLARE_FUNCTION(GetLangString)
  DECLARE_FUNCTION(RGB)
  DECLARE_FUNCTION(RGBA)
  DECLARE_FUNCTION(GetLockDef)
  DECLARE_FUNCTION(ParseColour)
  DECLARE_FUNCTION(TextColourString)
  DECLARE_FUNCTION(StartTitleMap)
  DECLARE_FUNCTION(LoadBinaryLump)
  DECLARE_FUNCTION(IsMapPresent)
  //DECLARE_FUNCTION(Clock)
  //DECLARE_FUNCTION(Unclock)
#endif

#ifndef VCC_STANDALONE_EXECUTOR
#ifdef CLIENT
  DECLARE_FUNCTION(P_GetMapName)
  DECLARE_FUNCTION(P_GetMapIndexByLevelNum)
  DECLARE_FUNCTION(P_GetNumMaps)
  DECLARE_FUNCTION(P_GetMapInfo)
  DECLARE_FUNCTION(P_GetMapLumpName)
  DECLARE_FUNCTION(P_TranslateMap)
  DECLARE_FUNCTION(P_GetNumEpisodes)
  DECLARE_FUNCTION(P_GetEpisodeDef)
  DECLARE_FUNCTION(P_GetNumSkills)
  DECLARE_FUNCTION(P_GetSkillDef)
  DECLARE_FUNCTION(KeyNameForNum)
  DECLARE_FUNCTION(IN_GetBindingKeys)
  DECLARE_FUNCTION(IN_SetBinding)
  DECLARE_FUNCTION(SV_GetSaveString)
  DECLARE_FUNCTION(StartSearch)
  DECLARE_FUNCTION(GetSlist)

  DECLARE_FUNCTION(LoadTextLump)

  // graphics
  DECLARE_FUNCTION(SetVirtualScreen)
  DECLARE_FUNCTION(R_RegisterPic)
  DECLARE_FUNCTION(R_RegisterPicPal)
  DECLARE_FUNCTION(R_GetPicInfo)
  DECLARE_FUNCTION(R_DrawPic)
  DECLARE_FUNCTION(R_InstallSprite)
  DECLARE_FUNCTION(R_DrawSpritePatch)
  DECLARE_FUNCTION(InstallModel)
  DECLARE_FUNCTION(R_DrawModelFrame)
  DECLARE_FUNCTION(R_FillRect)

  // client side sound
  DECLARE_FUNCTION(LocalSound)
  DECLARE_FUNCTION(IsLocalSoundPlaying)
  DECLARE_FUNCTION(StopLocalSounds)

  DECLARE_FUNCTION(TranslateKey)
#endif // CLIENT

#ifdef SERVER
  // map utilites
  DECLARE_FUNCTION(SectorClosestPoint)
  DECLARE_FUNCTION(LineOpenings)
  DECLARE_FUNCTION(P_BoxOnLineSide)
  DECLARE_FUNCTION(FindThingGap)
  DECLARE_FUNCTION(FindOpening)
  DECLARE_FUNCTION(PointInRegion)

  // sound functions
  DECLARE_FUNCTION(SectorStopSound)
  DECLARE_FUNCTION(GetSoundPlayingInfo)
  DECLARE_FUNCTION(GetSoundID)
  DECLARE_FUNCTION(SetSeqTrans)
  DECLARE_FUNCTION(GetSeqTrans)
  DECLARE_FUNCTION(GetSeqSlot)
  DECLARE_FUNCTION(StopAllSounds)

  DECLARE_FUNCTION(SB_Start)
  DECLARE_FUNCTION(TerrainType)
  DECLARE_FUNCTION(GetSplashInfo)
  DECLARE_FUNCTION(GetTerrainInfo)
  DECLARE_FUNCTION(FindClassFromEditorId)
  DECLARE_FUNCTION(FindClassFromScriptId)

  DECLARE_FUNCTION(HasDecal)
#endif // SERVER
#endif // !VCC_STANDALONE_EXECUTOR

#ifdef VCC_STANDALONE_EXECUTOR
  DECLARE_FUNCTION(CreateTimer)
  DECLARE_FUNCTION(CreateTimerWithId)
  DECLARE_FUNCTION(DeleteTimer)
  DECLARE_FUNCTION(IsTimerExists)
  DECLARE_FUNCTION(IsTimerOneShot)
  DECLARE_FUNCTION(GetTimerInterval)
  DECLARE_FUNCTION(SetTimerInterval)
  DECLARE_FUNCTION(GetTickCount)

  DECLARE_FUNCTION(fsysAppendDir)
  DECLARE_FUNCTION(fsysAppendPak)
  DECLARE_FUNCTION(fsysRemovePak)
  DECLARE_FUNCTION(fsysRemovePaksFrom)
  DECLARE_FUNCTION(fsysFindPakByPrefix)
  DECLARE_FUNCTION(fsysFileExists)
  DECLARE_FUNCTION(fsysFileFindAnyExt)
  DECLARE_FUNCTION(fsysGetPakPath)
  DECLARE_FUNCTION(fsysGetPakPrefix)
  DECLARE_FUNCTION(fsysGetLastPakId)

  DECLARE_FUNCTION(FindMObjId)
  DECLARE_FUNCTION(FindScriptId)
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

inline vuint32 GetTypeHash (VObject *Obj) { return (Obj ? Obj->GetIndex() : 0); }

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
#define EV_RET_VOID(v)    ExecuteFunction(GetVFunction(v))
#define EV_RET_INT(v)     return ExecuteFunction(GetVFunction(v)).i
#define EV_RET_BYTE(v)    return ExecuteFunction(GetVFunction(v)).i
#define EV_RET_FLOAT(v)   return ExecuteFunction(GetVFunction(v)).f
#define EV_RET_BOOL(v)    return !!ExecuteFunction(GetVFunction(v)).i
#define EV_RET_NAME(v)    vint32 ret = ExecuteFunction(GetVFunction(v)).i; return *(VName*)&ret
#define EV_RET_STR(v)     VStack Ret = ExecuteFunction(GetVFunction(v)); PR_PushPtr(Ret.p); return PR_PopStr()
#define EV_RET_VEC(v)     Sys_Error("Not implemented") /*ExecuteFunction(GetVFunction(v))*/
#define EV_RET_AVEC(v)    Sys_Error("Not implemented") /*ExecuteFunction(GetVFunction(v))*/
#define EV_RET_REF(t, v)  return (t*)ExecuteFunction(GetVFunction(v)).p
#define EV_RET_PTR(t, v)  return (t*)ExecuteFunction(GetVFunction(v)).p

#define EV_RET_VOID_IDX(v)    ExecuteFunction(GetVFunctionIdx(v))
#define EV_RET_INT_IDX(v)     return ExecuteFunction(GetVFunctionIdx(v)).i
#define EV_RET_BYTE_IDX(v)    return ExecuteFunction(GetVFunctionIdx(v)).i
#define EV_RET_FLOAT_IDX(v)   return ExecuteFunction(GetVFunctionIdx(v)).f
#define EV_RET_BOOL_IDX(v)    return !!ExecuteFunction(GetVFunctionIdx(v)).i
#define EV_RET_NAME_IDX(v)    vint32 ret = ExecuteFunction(GetVFunctionIdx(v)).i; return *(VName*)&ret
#define EV_RET_STR_IDX(v)     VStack Ret = ExecuteFunction(GetVFunctionIdx(v)); PR_PushPtr(Ret.p); return PR_PopStr()
#define EV_RET_VEC_IDX(v)     Sys_Error("Not implemented") /*ExecuteFunction(GetVFunctionIdx(v))*/
#define EV_RET_AVEC_IDX(v)    Sys_Error("Not implemented") /*ExecuteFunction(GetVFunctionIdx(v))*/
#define EV_RET_REF_IDX(t, v)  return (t*)ExecuteFunction(GetVFunctionIdx(v)).p
#define EV_RET_PTR_IDX(t, v)  return (t*)ExecuteFunction(GetVFunctionIdx(v)).p

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
