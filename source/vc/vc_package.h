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
//class VProgsReader;


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
#if !defined(VCC_OLD_PACKAGE_STRING_POOL)
    VStr str;
#endif
  };

  TArray<TStringInfo> StringInfo;
  int StringLookup[4096];
  int StringCount;

  static vuint32 StringHashFunc (const char *);

  void InitStringPool ();

public:
  // shared fields
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  TArray<char> Strings;
#endif

  // compiler fields
  TArray<VImportedPackage> PackagesToLoad;

  //TArray<mobjinfo_t> MobjInfo;
  //TArray<mobjinfo_t> ScriptIds;

  TArray<VConstant *> ParsedConstants;
  TArray<VStruct *> ParsedStructs;
  TArray<VClass *> ParsedClasses;
  TArray<VClass *> ParsedDecorateImportClasses;

  TMap<VName, bool> KnownEnums;

  int NumBuiltins;

/*
#if defined(IN_VCC)
  // run-time fields
  vuint16 Checksum;
  VProgsReader *Reader;
#endif
*/

public:
  VPackage ();
  VPackage (VName InName);
  virtual ~VPackage () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;

  int FindString (const char *);
  int FindString (const VStr &str);
  VConstant *FindConstant (VName Name, VName EnumName=NAME_None);

  const VStr &GetStringByIndex (int idx);

  bool IsKnownEnum (VName EnumName);
  bool AddKnownEnum (VName EnumName); // returns `true` if enum was redefined

  VClass *FindDecorateImportClass (VName) const;

  void Emit ();
#if defined(IN_VCC)
  void WriteObject (const VStr &); // binary
#endif
  void LoadObject (TLocation);

  // will delete `Strm`
  void LoadSourceObject (VStream *Strm, const VStr &filename, TLocation l);
#if defined(IN_VCC)
  // will delete `Strm`
  void LoadBinaryObject (VStream *Strm, const VStr &filename, TLocation l);
#endif

  /*
  mobjinfo_t *FindMObjInfo (vint32 id); // nullptr: not found
  VClass *FindMObj (vint32 id) const;
  VClass *FindScriptId (vint32 id) const;
  */

  friend inline VStream &operator << (VStream &Strm, VPackage *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
