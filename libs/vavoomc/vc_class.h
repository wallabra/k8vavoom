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

enum ENativeConstructor { EC_NativeConstructor };


// flags describing a class
enum EClassFlags {
  // base flags
  CLASS_Native                = 0x0001u,
  CLASS_Abstract              = 0x0002u, // class is abstract and can't be instantiated directly
  CLASS_SkipSuperStateLabels  = 0x0004u, // don't copy state labels
  CLASS_DecorateVisible       = 0x1000u, // this class, and all its children are visible to decorate code
  CLASS_Transient             = 0x8000u,
  // note that limiting is done by the main engine, VC does nothing with those flags
  CLASS_LimitInstances        = 0x10000u, // limit number of instances of this class
  CLASS_LimitInstancesWithSub = 0x20000u, // limit number of instances of this class and all its subclasses
};

// flags describing a class instance
enum EClassObjectFlags {
  CLASSOF_Native     = 0x00000001u, // native
  CLASSOF_PostLoaded = 0x00000002u, // `PostLoad()` has been called
};


struct mobjinfo_t {
public:
  // for `flags`
  enum {
    FlagNoSkill = 0x0001,
    FlagSpecial = 0x0002,
  };

  int DoomEdNum;
  vint32 GameFilter;
  VClass *Class;
  vint32 flags; // bit0: anyskill; bit1: special is set
  vint32 special;
  vint32 args[5];

//private:
  vint32 nextidx; // -1, or next info with the same DoomEdNum

//public:
  //friend VStream &operator << (VStream &, mobjinfo_t &);
};


//==========================================================================
//
//  VStateLabel
//
//==========================================================================
struct VStateLabel {
  // persistent fields
  VName Name;
  VState *State;
  TArray<VStateLabel> SubLabels;

  VStateLabel () : Name(NAME_None), State(nullptr) {}

  friend VStream &operator << (VStream &, VStateLabel &);
};


//==========================================================================
//
//  VStateLabelDef
//
//==========================================================================
struct VStateLabelDef {
  VStr Name;
  VState *State;
  TLocation Loc;
  VName GotoLabel;
  vint32 GotoOffset;

  VStateLabelDef () : State(nullptr), GotoLabel(NAME_None), GotoOffset(0) {}
};


//==========================================================================
//
//  VRepField
//
//==========================================================================
struct VRepField {
  VName Name;
  TLocation Loc;
  VMemberBase *Member;

  friend VStream &operator << (VStream &Strm, VRepField &Fld) { return Strm << Fld.Member; }
};


//==========================================================================
//
//  VRepInfo
//
//==========================================================================
struct VRepInfo {
  bool Reliable;
  VMethod *Cond;
  TArray<VRepField> RepFields;

  friend VStream &operator << (VStream &Strm, VRepInfo &RI) { return Strm << RI.Cond << RI.RepFields; }
};


//==========================================================================
//
//  VDecorateUserVarDef
//
//==========================================================================
struct VDecorateUserVarDef {
  VName name;
  TLocation loc;
  VFieldType type; // can be array too
};


//==========================================================================
//
//  VLightEffectDef
//
//==========================================================================
struct VLightEffectDef {
  VName Name;
  vuint8 Type;
  vint32 Color;
  float Radius;
  float Radius2;
  float MinLight;
  TVec Offset;
  float Chance;
  float Interval;
  float Scale;
  float ConeAngle;
  TVec ConeDir;

  //vint32 NoSelfShadow; // this will become flags
  enum {
    Flag_NoSelfShadow = 1u<<0,
    Flag_NoShadow     = 1u<<1,
  };
  vint32 Flags; // this will become flags

  inline bool IsNoSelfShadow () const noexcept { return !!(Flags&Flag_NoSelfShadow); }
  inline void SetNoSelfShadow (bool v) noexcept { if (v) Flags |= Flag_NoSelfShadow; else Flags &= ~Flag_NoSelfShadow; }

  inline bool IsNoShadow () const noexcept { return !!(Flags&Flag_NoShadow); }
  inline void SetNoShadow (bool v) noexcept { if (v) Flags |= Flag_NoShadow; else Flags &= ~Flag_NoShadow; }
};


//==========================================================================
//
//  VParticleEffectDef
//
//==========================================================================
struct VParticleEffectDef {
  VName Name;
  vuint8 Type;
  vuint8 Type2;
  vint32 Color;
  TVec Offset;
  vint32 Count;
  float OrgRnd;
  TVec Velocity;
  float VelRnd;
  float Accel;
  float Grav;
  float Duration;
  float Ramp;
};


//==========================================================================
//
//  VSpriteEffect
//
//==========================================================================
struct VSpriteEffect {
  vint32 SpriteIndex;
  vint8 Frame;
  VLightEffectDef *LightDef;
  VParticleEffectDef *PartDef;
};


//==========================================================================
//
//  VClass
//
//==========================================================================
class VClass : public VMemberBase {
public:
  enum { LOWER_CASE_HASH_SIZE = 8 * 1024 };

  struct TextureInfo {
    VStr texImage;
    int frameWidth;
    int frameHeight;
    int frameOfsX;
    int frameOfsY;
  };

  enum ReplaceType {
    Replace_None = 0,
    // decorate-style replace
    Replace_Normal,
    // HACK: find the latest child, and replace it.
    //       this will obviously not work if class has many children,
    //       but still useful to replace things like `LineSpecialLevelInfo`,
    //       because each game creates its own subclass of that.
    //       actually, we will replace subclass with the longest subclassing chain.
    Replace_LatestChild,
    // VavoomC replacement, affect parents (but doesn't force replaces)
    Replace_NextParents,
    Replace_NextParents_LastChild,
    // this does decorate-style replacement, and also replaces
    // all parents for direct children
    Replace_Parents,
    Replace_Parents_LatestChild,
  };

  inline bool DoesLastChildReplacement () const {
    return
      DoesReplacement == ReplaceType::Replace_LatestChild ||
      DoesReplacement == ReplaceType::Replace_NextParents_LastChild ||
      DoesReplacement == ReplaceType::Replace_Parents_LatestChild;
  }

  inline bool DoesParentReplacement () const { return (DoesReplacement >= ReplaceType::Replace_NextParents); }
  inline bool DoesForcedParentReplacement () const { return (DoesReplacement >= ReplaceType::Replace_Parents); }

public:
  // persistent fields
  VClass *ParentClass;
  VField *Fields;
  VState *States;
  TArray<VMethod *> Methods;
  VMethod *DefaultProperties;
  TArray<VRepInfo> RepInfos;
  TArray<VStateLabel> StateLabels;
  VName ClassGameObjName; // class can has a "game object name" (used in Spelunky Remake, for example)

  // compiler fields
  VName ParentClassName;
  TLocation ParentClassLoc;
  ReplaceType DoesReplacement;
  VExpression *GameExpr;
  VExpression *MobjInfoExpr;
  VExpression *ScriptIdExpr;
  TArray<VStruct *> Structs;
  TArray<VConstant *> Constants;
  TArray<VProperty *> Properties;
  TArray<VStateLabelDef> StateLabelDefs;
  bool Defined;
  bool DefinedAsDependency;

  // this is built when class postloaded
  // it contains all methods (including those from parents)
  TMapNC<VName, VMethod *> MethodMap;
  // contains both commands and autocompleters
  TMap<VStr, VMethod *> ConCmdListMts; // names are lowercased

  // new-style state options and textures
  TMapDtor<VStr, TextureInfo> dfStateTexList;
  VStr dfStateTexDir;
  vint32 dfStateTexDirSet;

  // internal per-object variables
  vuint32 ObjectFlags; // private EClassObjectFlags used by object manager
  VClass *LinkNext; // next class in linked list

  vint32 ClassSize;
  vint32 ClassUnalignedSize;
  vuint32 ClassFlags; // EClassFlags
  VMethod **ClassVTable;
  void (*ClassConstructor) ();

  vint32 ClassNumMethods;

  VField *ReferenceFields;
  VField *DestructorFields;
  VField *NetFields;
  VMethod *NetMethods;
  VState *NetStates;
  TArray<VState *> StatesLookup;
  vint32 NumNetFields;

  vuint8 *Defaults;

  VClass *Replacement;
  VClass *Replacee;

  TMap<VStrCI, VMethod *> DecorateStateActions;
  bool DecorateStateActionsBuilt;
  TMapNC<VName, VName> DecorateStateFieldTrans; // field translation; key is decorate name (lowercased), value is real field name

  TArray<VSpriteEffect> SpriteEffects;

  struct AliasInfo {
    VName aliasName;
    VName origName;
    TLocation loc;
    int aframe; // for loop checking
  };

  TMap<VName, AliasInfo> AliasList; // key: alias
  int AliasFrameNum;

  VName ResolveAlias (VName aname, bool nocase=false); // returns `aname` for unknown alias, or `NAME_None` for alias loop

  TMap<VName, bool> KnownEnums;

  // increments in `VObject::StaticSpawn()`
  // decrements in `VObject::Destroy()`
  int InstanceCount; // number of alive instances of this class
  int InstanceCountWithSub; // number of alive instances of this class and its subclasses (includes `InstanceCount`)

  // used by the main engine only
  int InstanceLimit;
  int InstanceLimitWithSub;
  VStr InstanceLimitCvar;
  VStr InstanceLimitWithSubCvar;
  // this is used to reroute limit counters from this class to base class
  VClass *InstanceLimitBaseClass;
  // in the main engine thinker this list will be filled with all alive instances
  TArray<VObject *> InstanceLimitList;

private:
  static TArray<VName> GSpriteNames;
  static TMapNC<VName, int> GSpriteNamesMap;

  static void RebuildSpriteMap ();

public:
  // property renames for various types
  // put here, because all methods belongs to classes
  TMap<VStr, VStr> StringProps;
  TMap<VStr, VStr> NameProps;

  // returns empty string if not found
  VStr FindInPropMap (EType type, VStr prname) noexcept;

public:
  static void InitSpriteList ();

  static inline int GetSpriteCount () noexcept { return GSpriteNames.length(); }
  static inline VName GetSpriteNameAt (int idx) noexcept { return (idx >= 0 && idx < GSpriteNames.length() ? GSpriteNames[idx] : NAME_None); }

public:
  inline bool GetLimitInstances () const noexcept { return (ClassFlags&CLASS_LimitInstances); }
  inline bool GetLimitInstancesWithSub () const noexcept { return (ClassFlags&CLASS_LimitInstancesWithSub); }

  inline void SetLimitInstances (bool v) noexcept { if (v) ClassFlags |= CLASS_LimitInstances; else ClassFlags &= ~CLASS_LimitInstances; }
  inline void SetLimitInstancesWithSub (bool v) noexcept { if (v) ClassFlags |= CLASS_LimitInstancesWithSub; else ClassFlags &= ~CLASS_LimitInstancesWithSub; }

public:
  VClass (VName, VMemberBase *, const TLocation &);
  VClass (ENativeConstructor, size_t ASize, vuint32 AClassFlags, VClass *AParent, EName AName, void (*ACtor) ());
  virtual ~VClass () override;
  virtual void CompilerShutdown () override;

  // systemwide functions
  static VClass *FindClass (const char *);
  static VClass *FindClassNoCase (const char *);
  static int FindSprite (VName, bool = true);
  static void GetSpriteNames (TArray<FReplacedString> &);
  static void ReplaceSpriteNames (TArray<FReplacedString> &);
  static void StaticReinitStatesLookup ();

  static mobjinfo_t *AllocMObjId (vint32 id, int GameFilter, VClass *cls);
  static mobjinfo_t *AllocScriptId (vint32 id, int GameFilter, VClass *cls);

  static mobjinfo_t *FindMObjId (vint32 id, int GameFilter);
  static mobjinfo_t *FindMObjIdByClass (const VClass *cls, int GameFilter);
  static mobjinfo_t *FindScriptId (vint32 id, int GameFilter);

  static void ReplaceMObjIdByClass (VClass *cls, vint32 id, int GameFilter);

  static void RemoveMObjId (vint32 id, int GameFilter);
  static void RemoveScriptId (vint32 id, int GameFilter);
  static void RemoveMObjIdByClass (VClass *cls, int GameFilter);

  static void StaticDumpMObjInfo ();
  static void StaticDumpScriptIds ();

  // calculates field offsets, creates VTable, and fills other such info
  // must be called after `Define()` and `Emit()`
  virtual void PostLoad () override;
  virtual void Shutdown () override;

  void AddConstant (VConstant *);
  void AddField (VField *);
  void AddProperty (VProperty *);
  void AddState (VState *);
  void AddMethod (VMethod *);

  bool IsKnownEnum (VName EnumName);
  bool AddKnownEnum (VName EnumName); // returns `true` if enum was redefined

  VConstant *FindConstant (VName Name, VName EnumName=NAME_None);
  VConstant *FindPackageConstant (VMemberBase *pkg, VName Name, VName EnumName=NAME_None);

  VField *FindField (VName, bool bRecursive=true);
  VField *FindField (VName, const TLocation &, VClass *);
  VField *FindFieldChecked (VName);

  VProperty *FindProperty (VName Name);
  VProperty *FindDecoratePropertyExact (VStr Name);
  VProperty *FindDecorateProperty (VStr Name);

  VConstant *FindDecorateConstantExact (VStr Name);
  VConstant *FindDecorateConstant (VStr Name);

  VMethod *FindMethod (VName Name, bool bRecursive=true);
  // this will follow `ParentClassName` instead of `ParentClass`
  VMethod *FindMethodNonPostLoaded (VName Name, bool bRecursive=true);
  VMethod *FindMethodNoCase (VStr Name, bool bRecursive=true); // ignores aliases
  VMethod *FindMethodChecked (VName);
  VMethod *FindAccessibleMethod (VName Name, VClass *self=nullptr, const TLocation *loc=nullptr);
  int GetMethodIndex (VName) const;

  VState *FindState (VName);
  VState *FindStateChecked (VName);
  VStateLabel *FindStateLabel (VName AName, VName SubLabel=NAME_None, bool Exact=false);
  VStateLabel *FindStateLabel (TArray<VName> &, bool);

  VMethod *FindDecorateStateActionExact (VStr actname); // but case-insensitive
  VMethod *FindDecorateStateAction (VStr actname);
  VName FindDecorateStateFieldTrans (VName dcname);

  VClass *FindBestLatestChild (VName ignoreThis);

  // WARNING! method with such name should exist, or return value will be invalid
  //          this is valid only after class was postloaded
  bool isNonVirtualMethod (VName Name);

  // resolves parent name to parent class, calls `Define()` on constants and structs,
  // sets replacement, and checks for duplicate fields/consts
  bool Define ();

  // calls `DefineMembers()` for structs, `Define()` for methods, fields and properties (including default)
  // also calls `Define()` for states, and calls `DefineRepInfos()`
  // must be called after `Define()`
  bool DefineMembers ();

  // fills `RepXXX` fields; called from `DefineMembers()`, no need to call it manually
  bool DefineRepInfos ();

  // calls `Emit()` for methods, emits state labels, calls `Emit()` for states, repinfo conditions, and default properties
  void Emit ();

  // this is special "castrated" emitter for decorate classes
  // as decorate classes cannot have replications, or new virtual methods, or such,
  // we don't need (and actually cannot) perform full `Emit()` on them
  void DecorateEmit ();

  // the same for `PostLoad()` -- it is called instead of normal `PostLoad()` for decorate classes
  void DecoratePostLoad ();

  void EmitStateLabels ();

  VState *ResolveStateLabel (const TLocation &, VName, int);
  void SetStateLabel (VName, VState *);
  void SetStateLabel (const TArray<VName> &, VState *);

  inline bool IsChildOf (const VClass *SomeBaseClass) const noexcept {
    for (const VClass *c = this; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
    return false;
  }

  inline bool IsChildOfByName (VName name) const noexcept {
    if (name == NAME_None) return false;
    for (const VClass *c = this; c; c = c->GetSuperClass()) {
      if (VStr::strEquCI(*name, *c->Name)) return true;
    }
    return false;
  }

  // accessors
  inline VClass *GetSuperClass () const noexcept { return ParentClass; }

  void DeepCopyObject (vuint8 *Dst, const vuint8 *Src);
  void CleanObject (VObject *);
  void DestructObject (VObject *);

  VClass *CreateDerivedClass (VName, VMemberBase *, TArray<VDecorateUserVarDef> &, const TLocation &);
  VClass *GetReplacement ();
  VClass *GetReplacee ();
  // returns `false` if replacee cannot be set for some reason
  bool SetReplacement (VClass *cls); // assign `cls` as a replacement for this

  // df state thingy
  void DFStateSetTexDir (VStr adir);
  void DFStateAddTexture (VStr tname, const TextureInfo &ti);
  VStr DFStateGetTexDir () const;
  bool DFStateGetTexture (VStr tname, TextureInfo &ti) const;

  // console command methods
  // returns index in lists, or -1
  VMethod *FindConCommandMethod (VStr name, bool exact=false); // skips autocompleters if `exact` is `false`
  inline VMethod *FindConCommandMethodExact (VStr name) { return FindConCommandMethod(name, true); } // doesn't skip anything

  // WARNING! don't add/remove ANY named members from callback!
  // return `FERes::FOREACH_STOP` from callback to stop (and return current member)
  // template function should accept `VClass *`, and return `FERes`
  // templated, so i can use lambdas
  // k8: don't even ask me. fuck shitplusplus.
  template<typename TDg> static VClass *ForEachChildOf (VClass *superCls, TDg &&dg) {
    decltype(dg((VClass *)nullptr)) test_ = FERes::FOREACH_NEXT;
    (void)test_;
    if (!superCls) return nullptr;
    for (auto &&m : GMembers) {
      if (m->MemberType != MEMBER_Class) continue;
      VClass *cls = (VClass *)m;
      if (superCls && !cls->IsChildOf(superCls)) continue;
      FERes res = dg(cls);
      if (res == FERes::FOREACH_STOP) return cls;
    }
    return nullptr;
  }

  template<typename TDg> static VClass *ForEachChildOf (const char *clsname, TDg &&dg) {
    if (!clsname || !clsname[0]) return nullptr;
    return ForEachChildOf(VClass::FindClass(clsname), dg);
  }

  // WARNING! don't add/remove ANY named members from callback!
  // return `FERes::FOREACH_STOP` from callback to stop (and return current member)
  // template function should accept `VClass *`, and return `FERes`
  // templated, so i can use lambdas
  // k8: don't even ask me. fuck shitplusplus.
  template<typename TDg> static VClass *ForEachClass (TDg &&dg) {
    decltype(dg((VClass *)nullptr)) test_ = FERes::FOREACH_NEXT;
    (void)test_;
    for (auto &&m : GMembers) {
      if (m->MemberType != MEMBER_Class) continue;
      VClass *cls = (VClass *)m;
      FERes res = dg(cls);
      if (res == FERes::FOREACH_STOP) return cls;
    }
    return nullptr;
  }

public:
  // default fields getters and setters
  inline float GetFieldFloat (VName fldname) { return FindFieldChecked(fldname)->GetFloat((const VObject *)Defaults); }
  inline TVec GetFieldVec (VName fldname) { return FindFieldChecked(fldname)->GetVec((const VObject *)Defaults); }
  inline bool GetFieldBool (VName fldname) { return FindFieldChecked(fldname)->GetBool((const VObject *)Defaults); }
  inline vint32 GetFieldInt (VName fldname) { return FindFieldChecked(fldname)->GetInt((const VObject *)Defaults); }
  inline vuint8 GetFieldByte (VName fldname) { return FindFieldChecked(fldname)->GetByte((const VObject *)Defaults); }
  inline VName GetFieldNameValue (VName fldname) { return FindFieldChecked(fldname)->GetNameValue((const VObject *)Defaults); }
  inline VStr GetFieldStr (VName fldname) { return FindFieldChecked(fldname)->GetStr((const VObject *)Defaults); }
  inline VClass *GetFieldClassValue (VName fldname) { return FindFieldChecked(fldname)->GetClassValue((const VObject *)Defaults); }
  inline VObject *GetFieldObjectValue (VName fldname) { return FindFieldChecked(fldname)->GetObjectValue((const VObject *)Defaults); }

  inline void SetFieldByte (VName fldname, vuint8 Value) { FindFieldChecked(fldname)->SetByte((VObject *)Defaults, Value); }
  inline void SetFieldInt (VName fldname, int Value) { FindFieldChecked(fldname)->SetInt((VObject *)Defaults, Value); }
  inline void SetFieldInt (VName fldname, int Value, int Idx) { FindFieldChecked(fldname)->SetInt((VObject *)Defaults, Value, Idx); }
  inline void SetFieldFloat (VName fldname, float Value) { FindFieldChecked(fldname)->SetFloat((VObject *)Defaults, Value); }
  inline void SetFieldFloat (VName fldname, float Value, int Idx) { FindFieldChecked(fldname)->SetFloat((VObject *)Defaults, Value, Idx); }
  inline void SetFieldNameValue (VName fldname, VName Value) { FindFieldChecked(fldname)->SetNameValue((VObject *)Defaults, Value); }
  inline void SetFieldStr (VName fldname, VStr Value) { FindFieldChecked(fldname)->SetStr((VObject *)Defaults, Value); }
  inline void SetFieldBool (VName fldname, int Value) { FindFieldChecked(fldname)->SetBool((VObject *)Defaults, Value); }
  inline void SetFieldVec (VName fldname, const TVec &Value) { FindFieldChecked(fldname)->SetVec((VObject *)Defaults, Value); }
  inline void SetFieldClassValue (VName fldname, VClass *Value) { FindFieldChecked(fldname)->SetClassValue((VObject *)Defaults, Value); }
  inline void SetFieldObjectValue (VName fldname, VObject *Value) { FindFieldChecked(fldname)->SetObjectValue((VObject *)Defaults, Value); }

private:
  void CalcFieldOffsets ();
  void InitNetFields ();
  void InitReferences ();
  void InitDestructorFields ();
  void CreateVTable ();
  void CreateMethodMap (); // called from `CreateVTable()`
  void InitStatesLookup ();
  void CreateDefaults ();

public:
  friend inline VStream &operator << (VStream &Strm, VClass *&Obj) { return Strm << *(VMemberBase **)&Obj; }

  friend class VPackage;
};
