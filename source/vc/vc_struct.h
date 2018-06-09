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


class VStruct : public VMemberBase {
public:
  // persistent fields
  VStruct *ParentStruct;
  vuint8 IsVector;
  // size in stack units when used as local variable
  vint32 StackSize;
  // structure fields
  VField *Fields;

  // compiler fields
  VName ParentStructName;
  TLocation ParentStructLoc;
  bool Defined;

  // run-time fields
  bool PostLoaded;
  vint32 Size;
  vuint8 Alignment;
  VField *ReferenceFields;
  VField *DestructorFields;

  struct AliasInfo {
    VName aliasName;
    VName origName;
    TLocation loc;
    int aframe; // for loop checking
  };

  TMap<VName, AliasInfo> AliasList; // key: alias
  int AliasFrameNum;

  VName ResolveAlias (VName aname); // returns `aname` for unknown alias, or `NAME_None` for alias loop

public:
  VStruct (VName, VMemberBase *, TLocation);

  virtual void Serialise (VStream &) override;
  virtual void PostLoad () override;

  void AddField (VField *f);
  VField *FindField (VName);
  bool NeedsDestructor () const;
  bool Define ();
  bool DefineMembers ();

  void CalcFieldOffsets ();
  void InitReferences ();
  void InitDestructorFields ();
#if !defined(IN_VCC)
  void CopyObject (const vuint8 *, vuint8 *);
  void SerialiseObject (VStream &, vuint8 *);
  void CleanObject (vuint8 *);
  void DestructObject (vuint8 *);
  bool IdenticalObject (const vuint8 *, const vuint8 *);
#endif
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  bool NetSerialiseObject (VStream &, VNetObjectsMap *, vuint8 *);
#endif

  friend inline VStream &operator << (VStream &Strm, VStruct *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
