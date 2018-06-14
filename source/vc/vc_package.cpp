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

#include "vc_local.h"


//==========================================================================
//
//  VProgsWriter
//
//==========================================================================
class VProgsWriter : public VStream {
private:
  FILE *File;

public:
  vint32 *NamesMap;
  vint32 *MembersMap;
  TArray<VName> Names;
  TArray<VProgsImport> Imports;
  TArray<VProgsExport> Exports;

  VProgsWriter (FILE *InFile) : File(InFile) {
    bLoading = false;
    NamesMap = new vint32[VName::GetNumNames()];
    for (int i = 0; i < VName::GetNumNames(); ++i) NamesMap[i] = -1;
    MembersMap = new vint32[VMemberBase::GMembers.Num()];
    memset(MembersMap, 0, VMemberBase::GMembers.Num()*sizeof(vint32));
  }

  // stream interface
  virtual void Seek (int InPos) override { if (fseek(File, InPos, SEEK_SET)) bError = true; }
  virtual int Tell () override { return ftell(File); }
  virtual int TotalSize () override {
    int CurPos = ftell(File);
    fseek(File, 0, SEEK_END);
    int Size = ftell(File);
    fseek(File, CurPos, SEEK_SET);
    return Size;
  }
  virtual bool AtEnd () override { return !!feof(File); }
  virtual bool Close () override { return !bError; }
  virtual void Serialise (void *V, int Length) override { if (fwrite(V, Length, 1, File) != 1) bError = true; }
  virtual void Flush () override { if (fflush(File)) bError = true; }

  VStream &operator << (VName &Name) {
    int TmpIdx = NamesMap[Name.GetIndex()];
    *this << STRM_INDEX(TmpIdx);
    return *this;
  }

  VStream &operator << (VMemberBase *&Ref) {
    int TmpIdx = (Ref ? MembersMap[Ref->MemberIndex] : 0);
    *this << STRM_INDEX(TmpIdx);
    return *this;
  }

  int GetMemberIndex (VMemberBase *Obj) {
    if (!Obj) return 0;
    if (!MembersMap[Obj->MemberIndex]) {
      MembersMap[Obj->MemberIndex] = -Imports.Append(VProgsImport(Obj, GetMemberIndex(Obj->Outer)))-1;
    }
    return MembersMap[Obj->MemberIndex];
  }

  void AddExport (VMemberBase *Obj) {
    MembersMap[Obj->MemberIndex] = Exports.Append(VProgsExport(Obj))+1;
  }
};


//==========================================================================
//
//  VImportsCollector
//
//==========================================================================
class VImportsCollector : public VStream {
  VProgsWriter &Writer;
  VPackage *Package;

public:
  VImportsCollector (VProgsWriter &AWriter, VPackage *APackage) : Writer(AWriter) , Package(APackage) {
    bLoading = false;
  }

  VStream &operator << (VName &Name) {
    if (Writer.NamesMap[Name.GetIndex()] == -1) Writer.NamesMap[Name.GetIndex()] = Writer.Names.Append(Name);
    return *this;
  }

  VStream &operator << (VMemberBase *&Ref) {
    if (Ref != Package) Writer.GetMemberIndex(Ref);
    return *this;
  }
};


//==========================================================================
//
//  VProgsReader
//
//==========================================================================
class VProgsReader : public VStream {
private:
  VStream *Stream;

public:
  VName *NameRemap;
  int NumImports;
  VProgsImport *Imports;
  int NumExports;
  VProgsExport *Exports;

  VProgsReader (VStream *InStream)
    : Stream(InStream)
    , NameRemap(0)
    , NumImports(0)
    , NumExports(0)
    , Exports(0)
  {
    bLoading = true;
  }

  virtual ~VProgsReader ()  override{
    delete[] NameRemap;
    delete[] Imports;
    delete[] Exports;
    delete Stream;
  }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  VStream &operator << (VName &Name) {
    int NameIndex;
    *this << STRM_INDEX(NameIndex);
    Name = NameRemap[NameIndex];
    return *this;
  }

  VStream &operator << (VMemberBase *&Ref) {
    int ObjIndex;
    *this << STRM_INDEX(ObjIndex);
    if (ObjIndex > 0) {
      check(ObjIndex <= NumExports);
      Ref = Exports[ObjIndex-1].Obj;
    } else if (ObjIndex < 0) {
      check(-ObjIndex <= NumImports);
      Ref = Imports[-ObjIndex-1].Obj;
    } else {
      Ref = nullptr;
    }
    return *this;
  }

  VMemberBase *GetImport (int Index) {
    VProgsImport &I = Imports[Index];
    if (!I.Obj) {
      if (I.Type == MEMBER_Package) {
        I.Obj = VMemberBase::StaticLoadPackage(I.Name, TLocation());
      } else if (I.Type == MEMBER_DecorateClass) {
        for (int i = 0; i < VMemberBase::GDecorateClassImports.Num(); ++i) {
          if (VMemberBase::GDecorateClassImports[i]->Name == I.Name) {
            I.Obj = VMemberBase::GDecorateClassImports[i];
            break;
          }
        }
        if (!I.Obj) I.Obj = VClass::FindClass(*I.Name);
        if (!I.Obj) {
          VClass *Tmp = new VClass(I.Name, nullptr, TLocation());
          Tmp->MemberType = MEMBER_DecorateClass;
          Tmp->ParentClassName = I.ParentClassName;
          VMemberBase::GDecorateClassImports.Append(Tmp);
          I.Obj = Tmp;
        }
      } else {
        I.Obj = VMemberBase::StaticFindMember(I.Name, GetImport(-I.OuterIndex-1), I.Type);
      }
    }
    return I.Obj;
  }

  void ResolveImports () {
    for (int i = 0; i < NumImports; ++i) GetImport(i);
  }
};


//==========================================================================
//
//  operator VStream << mobjinfo_t
//
//==========================================================================
VStream &operator << (VStream &Strm, mobjinfo_t &MI) {
  return Strm << STRM_INDEX(MI.DoomEdNum)
    << STRM_INDEX(MI.GameFilter)
    << MI.Class;
}


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage()
  : VMemberBase(MEMBER_Package, NAME_None, nullptr, TLocation())
  , NumBuiltins(0)
  , Checksum(0)
  , Reader(nullptr)
{
  // strings
  memset(StringLookup, 0, 256 * 4);
  // 1-st string is empty
  StringInfo.Alloc();
  StringInfo[0].Offs = 0;
  StringInfo[0].Next = 0;
  Strings.SetNum(4);
  memset(Strings.Ptr(), 0, 4);
}


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage(VName AName)
  : VMemberBase(MEMBER_Package, AName, nullptr, TLocation())
  , NumBuiltins(0)
  , Checksum(0)
  , Reader(nullptr)
{
  // strings
  memset(StringLookup, 0, 256 * 4);
  // 1-st string is empty
  StringInfo.Alloc();
  StringInfo[0].Offs = 0;
  StringInfo[0].Next = 0;
  Strings.SetNum(4);
  memset(Strings.Ptr(), 0, 4);
}


//==========================================================================
//
//  VPackage::~VPackage
//
//==========================================================================
VPackage::~VPackage () {
}


//==========================================================================
//
//  VPackage::Serialise
//
//==========================================================================
void VPackage::Serialise (VStream &Strm) {
  guard(VPackage::Serialise);
  VMemberBase::Serialise(Strm);
  unguard;
}


//==========================================================================
//
//  VPackage::StringHashFunc
//
//==========================================================================
int VPackage::StringHashFunc (const char *str) {
  return (str && str[0] ? (str[0]^(str[1]<<4))&0xff : 0);
}


//==========================================================================
//
//  VPackage::FindString
//
//  Return offset in strings array.
//
//==========================================================================
int VPackage::FindString (const char *str) {
  guard(VPackage::FindString);
  if (!str || !str[0]) return 0;

  int hash = StringHashFunc(str);
  for (int i = StringLookup[hash]; i; i = StringInfo[i].Next) {
    if (VStr::Cmp(&Strings[StringInfo[i].Offs], str) == 0) {
      return StringInfo[i].Offs;
    }
  }

  // add new string
  TStringInfo &SI = StringInfo.Alloc();
  int AddLen = VStr::Length(str)+1;
  while (AddLen&3) ++AddLen;
  int Ofs = Strings.Num();
  Strings.SetNum(Strings.Num()+AddLen);
  memset(&Strings[Ofs], 0, AddLen);
  SI.Offs = Ofs;
  SI.Next = StringLookup[hash];
  StringLookup[hash] = StringInfo.Num()-1;
  VStr::Cpy(&Strings[Ofs], str);
  return SI.Offs;
  unguard;
}


//==========================================================================
//
//  VPackage::FindConstant
//
//==========================================================================
VConstant *VPackage::FindConstant (VName Name) {
  guard(VPackage::FindConstant);
  VMemberBase *m = StaticFindMember(Name, this, MEMBER_Const);
  if (m) return (VConstant *)m;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VPackage::FindDecorateImportClass
//
//==========================================================================
VClass *VPackage::FindDecorateImportClass (VName AName) const {
  guard(VPackage::FindDecorateImportClass);
  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    if (ParsedDecorateImportClasses[i]->Name == AName) {
      return ParsedDecorateImportClasses[i];
    }
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VPackage::Emit
//
//==========================================================================
void VPackage::Emit () {
  guard(VPackage::Emit);

  dprintf("Importing packages\n");
  for (int i = 0; i < PackagesToLoad.Num(); ++i) {
    dprintf("  importing package '%s'...\n", *PackagesToLoad[i].Name);
    PackagesToLoad[i].Pkg = StaticLoadPackage(PackagesToLoad[i].Name, PackagesToLoad[i].Loc);
  }

  if (NumErrors) BailOut();

  dprintf("Defining constants\n");
  for (int i = 0; i < ParsedConstants.Num(); ++i) {
    dprintf("  defining constant '%s'...\n", *ParsedConstants[i]->Name);
    ParsedConstants[i]->Define();
  }

  dprintf("Defining structs\n");
  for (int i = 0; i < ParsedStructs.Num(); ++i) {
    dprintf("  defining struct '%s'...\n", *ParsedStructs[i]->Name);
    ParsedStructs[i]->Define();
  }

  dprintf("Defining classes\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    dprintf("  defining class '%s'...\n", *ParsedClasses[i]->Name);
    ParsedClasses[i]->Define();
  }

  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    dprintf("  defining decorate import class '%s'...\n", *ParsedDecorateImportClasses[i]->Name);
    ParsedDecorateImportClasses[i]->Define();
  }

  if (NumErrors) BailOut();

  dprintf("Defining struct members\n");
  for (int i = 0; i < ParsedStructs.Num(); ++i) {
    ParsedStructs[i]->DefineMembers();
  }

  dprintf("Defining class members\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    ParsedClasses[i]->DefineMembers();
  }

  if (NumErrors) BailOut();

  dprintf("Emiting classes\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    ParsedClasses[i]->Emit();
  }

  if (NumErrors) BailOut();

  unguard;
}


//==========================================================================
//
//  VPackage::WriteObject
//
//==========================================================================
void VPackage::WriteObject (const VStr &name) {
  guard(VPackage::WriteObject);
  FILE *f;
  dprograms_t progs;

  dprintf("Writing object\n");

  f = fopen(*name, "wb");
  if (!f) FatalError("Can't open file \"%s\".", *name);

  VProgsWriter Writer(f);
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->IsIn(this)) Writer.AddExport(VMemberBase::GMembers[i]);
  }

  // add decorate class imports
  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    VProgsImport I(ParsedDecorateImportClasses[i], 0);
    I.Type = MEMBER_DecorateClass;
    I.ParentClassName = ParsedDecorateImportClasses[i]->ParentClassName;
    Writer.MembersMap[ParsedDecorateImportClasses[i]->MemberIndex] = -Writer.Imports.Append(I)-1;
  }

  // collect list of imported objects and used names
  VImportsCollector Collector(Writer, this);
  for (int i = 0; i < Writer.Exports.Num(); ++i) Collector << Writer.Exports[i];
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer.Exports[i].Obj->Serialise(Collector);
  for (int i = 0; i < Writer.Imports.Num(); ++i) Collector << Writer.Imports[i];

  // now write the object file
  memset(&progs, 0, sizeof(progs));
  Writer.Serialise(&progs, sizeof(progs));

  // serialise names
  progs.ofs_names = Writer.Tell();
  progs.num_names = Writer.Names.Num();
  for (int i = 0; i < Writer.Names.Num(); ++i) Writer << *VName::GetEntry(Writer.Names[i].GetIndex());

  progs.ofs_strings = Writer.Tell();
  progs.num_strings = Strings.Num();
  Writer.Serialise(&Strings[0], Strings.Num());

  progs.ofs_mobjinfo = Writer.Tell();
  progs.num_mobjinfo = MobjInfo.Num();
  for (int i = 0; i < MobjInfo.Num(); ++i) Writer << MobjInfo[i];

  progs.ofs_scriptids = Writer.Tell();
  progs.num_scriptids = ScriptIds.Num();
  for (int i = 0; i < ScriptIds.Num(); ++i) Writer << ScriptIds[i];

  // serialise imports
  progs.num_imports = Writer.Imports.Num();
  progs.ofs_imports = Writer.Tell();
  for (int i = 0; i < Writer.Imports.Num(); ++i) Writer << Writer.Imports[i];

  progs.num_exports = Writer.Exports.Num();

  // serialise object infos
  progs.ofs_exportinfo = Writer.Tell();
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer << Writer.Exports[i];

  // serialise objects
  progs.ofs_exportdata = Writer.Tell();
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer.Exports[i].Obj->Serialise(Writer);

#if !defined(VCC_STANDALONE_EXECUTOR)
  // print statistics
  dprintf("            count   size\n");
  dprintf("Header     %6d %6ld\n", 1, (long int)sizeof(progs));
  dprintf("Names      %6d %6d\n", Writer.Names.Num(), progs.ofs_strings - progs.ofs_names);
  dprintf("Strings    %6d %6d\n", StringInfo.Num(), Strings.Num());
  dprintf("Builtins   %6d\n", NumBuiltins);
  dprintf("Mobj info  %6d %6d\n", MobjInfo.Num(), progs.ofs_scriptids - progs.ofs_mobjinfo);
  dprintf("Script Ids %6d %6d\n", ScriptIds.Num(), progs.ofs_imports - progs.ofs_scriptids);
  dprintf("Imports    %6d %6d\n", Writer.Imports.Num(), progs.ofs_exportinfo - progs.ofs_imports);
  dprintf("Exports    %6d %6d\n", Writer.Exports.Num(), progs.ofs_exportdata - progs.ofs_exportinfo);
  dprintf("Type data  %6d %6d\n", Writer.Exports.Num(), Writer.Tell() - progs.ofs_exportdata);
  dprintf("TOTAL SIZE       %7d\n", Writer.Tell());
#endif

  // write header
  memcpy(progs.magic, PROG_MAGIC, 4);
  progs.version = PROG_VERSION;
  Writer.Seek(0);
  Writer.Serialise(progs.magic, 4);
  for (int i = 1; i < (int)sizeof(progs)/4; ++i) Writer << ((int *)&progs)[i];

#ifdef OPCODE_STATS
  dprintf("\n-----------------------------------------------\n\n");
  for (int i = 0; i < NUM_OPCODES; ++i) {
    dprintf("%-16s %d\n", StatementInfo[i].name, StatementInfo[i].usecount);
  }
  dprintf("%d opcodes\n", NUM_OPCODES);
#endif

  fclose(f);
  unguard;
}


//==========================================================================
//
// VPackage::LoadSourceObject
//
// will delete `Strm`
//
//==========================================================================
void VPackage::LoadSourceObject (VStream *Strm, const VStr &filename, TLocation l) {
  guard(VPackage::LoadSourceObject);

  if (!Strm) return;

  VLexer Lex;
  VMemberBase::InitLexer(Lex);
  Lex.OpenSource(Strm, filename);
  VParser Parser(Lex, this);
  Parser.Parse();
  Emit();

#if !defined(IN_VCC)
  //fprintf(stderr, "*** PACKAGE: %s\n", *Name);
  // copy mobj infos and spawn IDs
  for (int i = 0; i < MobjInfo.Num(); ++i) VClass::GMobjInfos.Alloc() = MobjInfo[i];
  for (int i = 0; i < ScriptIds.Num(); ++i) VClass::GScriptIds.Alloc() = ScriptIds[i];
  for (int i = 0; i < GMembers.Num(); ++i) {
    if (GMembers[i]->IsIn(this)) {
      //fprintf(stderr, "  *** postload for '%s'...\n", *GMembers[i]->Name);
      GMembers[i]->PostLoad();
    }
  }

  // create default objects
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    //fprintf(stderr, "  *** creating defaults for '%s'... (%d)\n", *ParsedClasses[i]->Name, (int)ParsedClasses[i]->Defined);
    ParsedClasses[i]->CreateDefaults();
    if (!ParsedClasses[i]->Outer) {
      //fprintf(stderr, "    *** setting outer\n");
      ParsedClasses[i]->Outer = this;
    }
  }

# if !defined(VCC_STANDALONE_EXECUTOR)
  // we need to do this, 'cause VaVoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        GCon->Logf("WARNING! package `engine` has hidden class `%s` (this is harmless)", *Cls->Name);
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
      }
    }
  }
# endif
#endif

  unguard;
}


//==========================================================================
//
// VPackage::LoadBinaryObject
//
// will delete `Strm`
//
//==========================================================================
void VPackage::LoadBinaryObject (VStream *Strm, const VStr &filename, TLocation l) {
  guard(VPackage::LoadBinaryObject);

  if (!Strm) return;

  VProgsReader *Reader = new VProgsReader(Strm);

  // calcutate CRC
#if !defined(IN_VCC)
  auto crc = TCRC16();
  for (int i = 0; i < Reader->TotalSize(); ++i) crc += Streamer<vuint8>(*Reader);
#endif

  // read the header
  dprograms_t Progs;
  Reader->Seek(0);
  Reader->Serialise(Progs.magic, 4);
  for (int i = 1; i < (int)sizeof(Progs)/4; ++i) *Reader << ((int *)&Progs)[i];

  if (VStr::NCmp(Progs.magic, PROG_MAGIC, 4)) {
    ParseError(l, "Package '%s' has wrong file ID", *Name);
    BailOut();
  }
  if (Progs.version != PROG_VERSION) {
    ParseError(l, "Package '%s' has wrong version number (%i should be %i)", *Name, Progs.version, PROG_VERSION);
    BailOut();
  }

  // read names
  VName *NameRemap = new VName[Progs.num_names];
  Reader->Seek(Progs.ofs_names);
  for (int i = 0; i < Progs.num_names; ++i) {
    VNameEntry E;
    *Reader << E;
    NameRemap[i] = E.Name;
  }
  Reader->NameRemap = NameRemap;

  Reader->Imports = new VProgsImport[Progs.num_imports];
  Reader->NumImports = Progs.num_imports;
  Reader->Seek(Progs.ofs_imports);
  for (int i = 0; i < Progs.num_imports; ++i) *Reader << Reader->Imports[i];
  Reader->ResolveImports();

  VProgsExport *Exports = new VProgsExport[Progs.num_exports];
  Reader->Exports = Exports;
  Reader->NumExports = Progs.num_exports;

#if !defined(IN_VCC)
  Checksum = crc;
  this->Reader = Reader;
#endif

  // create objects
  Reader->Seek(Progs.ofs_exportinfo);
  for (int i = 0; i < Progs.num_exports; ++i) {
    *Reader << Exports[i];
    switch (Exports[i].Type) {
      case MEMBER_Package: Exports[i].Obj = new VPackage(Exports[i].Name); break;
      case MEMBER_Field: Exports[i].Obj = new VField(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Property: Exports[i].Obj = new VProperty(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Method: Exports[i].Obj = new VMethod(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_State: Exports[i].Obj = new VState(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Const: Exports[i].Obj = new VConstant(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Struct: Exports[i].Obj = new VStruct(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Class:
#if !defined(IN_VCC)
        Exports[i].Obj = VClass::FindClass(*Exports[i].Name);
        if (!Exports[i].Obj)
#endif
        {
          Exports[i].Obj = new VClass(Exports[i].Name, nullptr, TLocation());
        }
        break;
      default: ParseError(l, "Package '%s' contains corrupted data", *Name); BailOut();
    }
  }

  // read strings
  Strings.SetNum(Progs.num_strings);
  Reader->Seek(Progs.ofs_strings);
  Reader->Serialise(Strings.Ptr(), Progs.num_strings);

  // serialise objects
  Reader->Seek(Progs.ofs_exportdata);
  for (int i = 0; i < Progs.num_exports; ++i) {
    Exports[i].Obj->Serialise(*Reader);
    if (!Exports[i].Obj->Outer) Exports[i].Obj->Outer = this;
  }

#if !defined(IN_VCC)
  // set up info tables
  Reader->Seek(Progs.ofs_mobjinfo);
  for (int i = 0; i < Progs.num_mobjinfo; ++i) *Reader << VClass::GMobjInfos.Alloc();

  Reader->Seek(Progs.ofs_scriptids);
  for (int i = 0; i < Progs.num_scriptids; ++i) *Reader << VClass::GScriptIds.Alloc();

  for (int i = 0; i < Progs.num_exports; ++i) Exports[i].Obj->PostLoad();

  // create default objects
  for (int i = 0; i < Progs.num_exports; ++i) {
    if (Exports[i].Obj->MemberType == MEMBER_Class) {
      VClass *vc = (VClass *)Exports[i].Obj;
      vc->CreateDefaults();
      if (!vc->Outer) vc->Outer = this;
    }
  }

# if !defined(VCC_STANDALONE_EXECUTOR)
  // we need to do this, 'cause VaVoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
      }
    }
  }
# endif
#endif

  //k8: fuck you, shitplusplus: no finally
  this->Reader = nullptr;
  delete Reader;

  unguard;
}


//==========================================================================
//
// VPackage::LoadObject
//
//==========================================================================
#if !defined(IN_VCC)
static const char *pkgImportFiles[] = {
  "0package.vc",
  "package.vc",
  "0classes.vc",
  "classes.vc",
  nullptr
};
#endif


void VPackage::LoadObject (TLocation l) {
  guard(VPackage::LoadObject);

#if defined(IN_VCC)
  dprintf("Loading package %s\n", *Name);

  // load PROGS from a specified file
  VStream *f = fsysOpenFile(va("%s.dat", *Name));
  if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }

  for (int i = 0; i < GPackagePath.Num(); ++i) {
    VStr fname = GPackagePath[i]+"/"+Name+".dat";
    f = fsysOpenFile(*fname);
    if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }
  }

  ParseError(l, "Can't find package %s", *Name);

#elif defined(VCC_STANDALONE_EXECUTOR)
  dprintf("Loading package '%s'...\n", *Name);

  for (int i = 0; i < GPackagePath.Num(); ++i) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = GPackagePath[i]+"/"+Name+"/"+(*pif);
      VStream *Strm = fsysOpenFile(mainVC);
      if (Strm) { dprintf("  '%s'\n", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // if no package pathes specified, try "packages/"
  if (GPackagePath.length() == 0) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = VStr("packages/")+Name+"/"+(*pif);
      VStream *Strm = fsysOpenFile(mainVC);
      if (Strm) { dprintf("  '%s'\n", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // don't even try to load binary packages
  /*
  VStr mainVC = va("packages/%s.dat", *Name);
  VStream *Strm = (tnum ? fsysOpenFile(mainVC) : fsysOpenDiskFile(mainVC));
  if (Strm) { dprintf("  '%s'\n", *mainVC); LoadBinaryObject(Strm, mainVC, l); return; }
  */

  ParseError(l, "Can't find package %s", *Name);
  BailOut();

#else

  // load PROGS from a specified file
  VStr mainVC = va("progs/%s.dat", *Name);
  VStream *Strm = FL_OpenFileRead(*mainVC);
  if (!Strm) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      mainVC = va("progs/%s/%s", *Name, *pif);
      if (FL_FileExists(*mainVC)) {
        // compile package
        Strm = FL_OpenFileRead(*mainVC);
        LoadSourceObject(Strm, mainVC, l);
        return;
      }
    }
    Sys_Error("Progs package %s not found", *Name);
  }

  LoadBinaryObject(Strm, mainVC, l);

#endif

  unguard;
}
