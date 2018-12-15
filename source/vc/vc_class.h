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

enum ENativeConstructor { EC_NativeConstructor };


// flags describing a class
enum EClassFlags {
  // base flags
  CLASS_Native               = 0x0001,
  CLASS_Abstract             = 0x0002, // class is abstract and can't be instantiated directly
  CLASS_SkipSuperStateLabels = 0x0004, // don't copy state labels
  CLASS_Transient            = 0x8000,
};

// flags describing a class instance
enum EClassObjectFlags {
  CLASSOF_Native     = 0x00000001, // native
  CLASSOF_PostLoaded = 0x00000002, // `PostLoad()` has been called
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
//  VDecorateStateAction
//
//==========================================================================
struct VDecorateStateAction {
  VName Name;
  VMethod *Method;
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
  vint32 Colour;
  float Radius;
  float Radius2;
  float MinLight;
  TVec Offset;
  float Chance;
  float Interval;
  float Scale;
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
  vint32 Colour;
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
  bool DoesReplacement;
  VExpression *GameExpr;
  VExpression *MobjInfoExpr;
  VExpression *ScriptIdExpr;
  TArray<VStruct *> Structs;
  TArray<VConstant *> Constants;
  TArray<VProperty *> Properties;
  TArray<VStateLabelDef> StateLabelDefs;
  bool Defined;
  bool DefinedAsDependency;

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

  TArray<VDecorateStateAction> DecorateStateActions;
  TMapNC<VName, VName> DecorateStateFieldTrans; // field translation; key is decorate name (lowercased), value is real field name

  TArray<VSpriteEffect> SpriteEffects;

  //VName LowerCaseName;
  //VClass *LowerCaseHashNext;

  static TArray<VName> GSpriteNames;
  //static VClass *GLowerCaseHashTable[LOWER_CASE_HASH_SIZE];

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

public:
  VClass (VName, VMemberBase *, const TLocation &);
  VClass (ENativeConstructor, size_t ASize, vuint32 AClassFlags, VClass *AParent, EName AName, void (*ACtor) ());
  virtual ~VClass () override;
  virtual void CompilerShutdown () override;

  // systemwide functions
  static VClass *FindClass (const char *);
  static VClass *FindClassNoCase (const char *);
  static VClass *FindClassLowerCase (VName);
  static int FindSprite (VName, bool = true);
  static void GetSpriteNames (TArray<FReplacedString> &);
  static void ReplaceSpriteNames (TArray<FReplacedString> &);
  static void StaticReinitStatesLookup ();

  static mobjinfo_t *AllocMObjId (vint32 id, int GameFilter, VClass *cls);
  static mobjinfo_t *AllocScriptId (vint32 id, int GameFilter, VClass *cls);

  static mobjinfo_t *FindMObjId (vint32 id, int GameFilter);
  static mobjinfo_t *FindMObjIdByClass (const VClass *cls, int GameFilter);
  static mobjinfo_t *FindScriptId (vint32 id, int GameFilter);

  static void RemoveMObjId (vint32 id, int GameFilter);
  static void RemoveScriptId (vint32 id, int GameFilter);
  static void RemoveMObjIdByClass (VClass *cls, int GameFilter);

  static void StaticDumpMObjInfo ();
  static void StaticDumpScriptIds ();

  virtual void Serialise (VStream &) override;
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
  VField *FindField (VName, bool bRecursive=true);
  VField *FindField (VName, const TLocation &, VClass *);
  VField *FindFieldChecked (VName);
  VProperty *FindProperty (VName);
  VMethod *FindMethod (VName Name, bool bRecursive=true);
  //VMethod *FindMethodNoCase (VName Name, bool bRecursive=true);
  VMethod *FindMethodChecked (VName);
  VMethod *FindAccessibleMethod (VName Name, VClass *self=nullptr, const TLocation *loc=nullptr);
  int GetMethodIndex (VName);
  VState *FindState (VName);
  VState *FindStateChecked (VName);
  VStateLabel *FindStateLabel (VName AName, VName SubLabel=NAME_None, bool Exact=false);
  VStateLabel *FindStateLabel (TArray<VName> &, bool);
  //VStateLabel *FindStateLabelChecked (VName, VName = NAME_None, bool=false);
  //VStateLabel *FindStateLabelChecked (TArray<VName> &, bool);
  VDecorateStateAction *FindDecorateStateAction (VName);
  VName FindDecorateStateFieldTrans (VName dcname);

  // WARNING! method with such name should exist, or return value will be invalid
  bool isRealFinalMethod (VName Name);

  bool Define ();
  bool DefineMembers ();
  void Emit ();
  bool DefineRepInfos ();
  bool DecorateDefine ();
  void DecorateEmit ();
  void DecoratePostLoad ();
  void EmitStateLabels ();
  VState *ResolveStateLabel (const TLocation &, VName, int);
  void SetStateLabel (VName, VState *);
  void SetStateLabel (const TArray<VName> &, VState *);

  inline bool IsChildOf (const VClass *SomeBaseClass) const {
    for (const VClass *c = this; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
    return false;
  }

  // accessors
  inline VClass *GetSuperClass () const { return ParentClass; }

#if !defined(IN_VCC)
  void CopyObject (const vuint8 *, vuint8 *);
  void SerialiseObject (VStream &, VObject *);
  void CleanObject (VObject *);
  void DestructObject (VObject *);
  VClass *CreateDerivedClass (VName, VMemberBase *, TArray<VDecorateUserVarDef> &, const TLocation &);
#endif
  VClass *GetReplacement ();
  VClass *GetReplacee ();
  // returns `false` if replacee cannot be set for some reason
  bool SetReplacement (VClass *cls); // assign `cls` as a replacement for this

  //void HashLowerCased ();

  // df state thingy
  void DFStateSetTexDir (const VStr &adir);
  void DFStateAddTexture (const VStr &tname, const TextureInfo &ti);
  const VStr &DFStateGetTexDir () const;
  bool DFStateGetTexture (const VStr &tname, TextureInfo &ti) const;

private:
  void CalcFieldOffsets ();
  void InitNetFields ();
  void InitReferences ();
  void InitDestructorFields ();
  void CreateVTable ();
  void InitStatesLookup ();
#if !defined(IN_VCC)
  void CreateDefaults ();
#endif

public:
  friend inline VStream &operator << (VStream &Strm, VClass *&Obj) { return Strm << *(VMemberBase **)&Obj; }

  friend class VPackage;
};
