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

class VProgsReader;


// ////////////////////////////////////////////////////////////////////////// //
struct mobjinfo_t {
  int DoomEdNum;
  vint32 GameFilter;
  VClass *Class;

  friend VStream &operator << (VStream &, mobjinfo_t &);
};


// ////////////////////////////////////////////////////////////////////////// //
struct VImportedPackage {
  VName Name;
  TLocation Loc;
  VPackage *Pkg;
};


// ////////////////////////////////////////////////////////////////////////// //
class VPackage : public VMemberBase {
private:
  struct TStringInfo {
    int Offs;
    int Next;
  };

  TArray<TStringInfo> StringInfo;
  int StringLookup[256];

  static int StringHashFunc (const char *);

public:
  // shared fields
  TArray<char> Strings;

  // compiler fields
  TArray<VImportedPackage> PackagesToLoad;

  TArray<mobjinfo_t> MobjInfo;
  TArray<mobjinfo_t> ScriptIds;

  TArray<VConstant *> ParsedConstants;
  TArray<VStruct *> ParsedStructs;
  TArray<VClass *> ParsedClasses;
  TArray<VClass *> ParsedDecorateImportClasses;

  int NumBuiltins;

  // run-time fields
  vuint16 Checksum;
  VProgsReader *Reader;

public:
  VPackage ();
  VPackage (VName InName);
  virtual ~VPackage () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;

  int FindString (const char *);
  VConstant *FindConstant (VName Name, VName EnumName=NAME_None);

  VClass *FindDecorateImportClass (VName) const;

  void Emit ();
  void WriteObject (const VStr &);
  void LoadObject (TLocation);

  // will delete `Strm`
  void LoadSourceObject (VStream *Strm, const VStr &filename, TLocation l);
  // will delete `Strm`
  void LoadBinaryObject (VStream *Strm, const VStr &filename, TLocation l);

  VClass *FindMObj (vint32 id) const;
  VClass *FindScriptId (vint32 id) const;

  friend inline VStream &operator << (VStream &Strm, VPackage *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
