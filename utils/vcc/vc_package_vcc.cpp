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
#include "../../libs/core/core.h"
#include "../../libs/vavoomc/vc_public.h"
#include "../../libs/vavoomc/vc_local.h"


//#define vdlogf(...)  if (VObject::cliShowLoadingMessages) GLog.Logf(NAME_Init, __VA_ARGS__)
#define vdlogf(...)  do {} while (0)
#define vdlogfEx(...)  if (VObject::cliShowLoadingMessages) GLog.Logf(NAME_Init, __VA_ARGS__)


//==========================================================================
//
//  VPackage::LoadObject
//
//==========================================================================
void VPackage::LoadObject (TLocation l) {
  vdlogfEx("Loading package %s", *Name);

  // load PROGS from a specified file
  VStream *f = vc_OpenFile(va("%s.dat", *Name));
  if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (unsigned pidx = 0; ; ++pidx) {
      const char *pif = GetPkgImportFile(pidx);
      if (!pif) break;
      //VStr mainVC = va("%s/progs/%s/%s", *GPackagePath[i], *Name, pif);
      VStr mainVC = va("%s/%s/%s", *GPackagePath[i], *Name, pif);
      //GLog.Logf(": <%s>", *mainVC);
      VStream *Strm = vc_OpenFile(*mainVC);
      if (Strm) {
        LoadSourceObject(Strm, mainVC, l);
        return;
      }
    }
  }

  ParseError(l, "Can't find package %s", *Name);
}



#define PROG_MAGIC    "VPRG"
#define PROG_VERSION  (53)

struct dprograms_t {
  char magic[4]; // "VPRG"
  int version;

  int ofs_names;
  int num_names;

  int num_strings;
  int ofs_strings;

  int ofs_mobjinfo;
  int num_mobjinfo;

  int ofs_scriptids;
  int num_scriptids;

  int ofs_exportinfo;
  int ofs_exportdata;
  int num_exports;

  int ofs_imports;
  int num_imports;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VProgsImport {
  vuint8 Type;
  VName Name;
  vint32 OuterIndex;
  VName ParentClassName;  // for decorate class imports

  VMemberBase *Obj;

  VProgsImport () : Type(0), Name(NAME_None), OuterIndex(0), Obj(0) {}
  VProgsImport (VMemberBase *InObj, vint32 InOuterIndex);

  friend VStream &operator << (VStream &Strm, VProgsImport &I) {
    Strm << I.Type << I.Name << STRM_INDEX(I.OuterIndex);
    if (I.Type == MEMBER_DecorateClass) Strm << I.ParentClassName;
    return Strm;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VProgsExport {
  vuint8 Type;
  VName Name;

  VMemberBase *Obj;

  VProgsExport () : Type(0), Name(NAME_None), Obj(0) {}
  VProgsExport(VMemberBase *InObj);

  friend VStream &operator << (VStream &Strm, VProgsExport &E) {
    return Strm << E.Type << E.Name;
  }
};


//==========================================================================
//
//  VProgsImport::VProgsImport
//
//==========================================================================
VProgsImport::VProgsImport (VMemberBase *InObj, vint32 InOuterIndex)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , OuterIndex(InOuterIndex)
  , Obj(InObj)
{
}


//==========================================================================
//
//  VProgsExport::VProgsExport
//
//==========================================================================
VProgsExport::VProgsExport (VMemberBase *InObj)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , Obj(InObj)
{
}


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

  virtual ~VProgsWriter () override { Close(); }

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

  virtual void io (VName &Name) override {
    int TmpIdx = NamesMap[Name.GetIndex()];
    *this << STRM_INDEX(TmpIdx);
  }

  virtual void io (VMemberBase *&Ref) override {
    int TmpIdx = (Ref ? MembersMap[Ref->MemberIndex] : 0);
    *this << STRM_INDEX(TmpIdx);
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

  virtual void io (VName &Name) override {
    if (Writer.NamesMap[Name.GetIndex()] == -1) Writer.NamesMap[Name.GetIndex()] = Writer.Names.Append(Name);
  }

  virtual void io (VMemberBase *&Ref) override {
    if (Ref != Package) Writer.GetMemberIndex(Ref);
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
    Close();
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

  virtual void io (VName &Name) override {
    int NameIndex;
    *this << STRM_INDEX(NameIndex);
    Name = NameRemap[NameIndex];
  }

  virtual void io (VMemberBase *&Ref) override {
    int ObjIndex;
    *this << STRM_INDEX(ObjIndex);
    if (ObjIndex > 0) {
      vassert(ObjIndex <= NumExports);
      Ref = Exports[ObjIndex-1].Obj;
    } else if (ObjIndex < 0) {
      vassert(-ObjIndex <= NumImports);
      Ref = Imports[-ObjIndex-1].Obj;
    } else {
      Ref = nullptr;
    }
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
//  VPackage::WriteObject
//
//==========================================================================
void VPackage::WriteObject (VStr name) {
  FILE *f;
  dprograms_t progs;

  vdlogf("Writing object");

  f = fopen(*name, "wb");
  if (!f) VCFatalError("Can't open file \"%s\".", *name);

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
  for (int i = 0; i < Writer.Names.Num(); ++i) {
    //Writer << *VName::GetEntry(Writer.Names[i].GetIndex());
    const char *EName = *Writer.Names[i];
    vuint8 len = (vuint8)VStr::length(EName);
    Writer << len;
    if (len) Writer.Serialise((void *)EName, len);
  }

  progs.ofs_strings = Writer.Tell();
  progs.num_strings = StringInfo.length();
  vint32 count = progs.num_strings;
  Writer << count;
  for (int stridx = 0; stridx < StringInfo.length(); ++stridx) {
    Writer << StringInfo[stridx].str;
  }

  //FIXME
  //progs.ofs_mobjinfo = Writer.Tell();
  //progs.num_mobjinfo = VClass::GMobjInfos.Num();
  //for (int i = 0; i < VClass::GMobjInfos.Num(); ++i) Writer << VClass::GMobjInfos[i];

  //progs.ofs_scriptids = Writer.Tell();
  //progs.num_scriptids = VClass::GScriptIds.Num();
  //for (int i = 0; i < VClass::GScriptIds.Num(); ++i) Writer << VClass::GScriptIds[i];

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

  // print statistics
  vdlogf("            count   size");
  vdlogf("Header     %6d %6ld", 1, (long int)sizeof(progs));
  vdlogf("Names      %6d %6d", Writer.Names.Num(), progs.ofs_strings - progs.ofs_names);
  vdlogf("Strings    %6d", StringInfo.Num());
  vdlogf("Builtins   %6d", NumBuiltins);
  //vdlogf("Mobj info  %6d %6d", VClass::GMobjInfos.Num(), progs.ofs_scriptids - progs.ofs_mobjinfo);
  //vdlogf("Script Ids %6d %6d", VClass::GScriptIds.Num(), progs.ofs_imports - progs.ofs_scriptids);
  vdlogf("Imports    %6d %6d", Writer.Imports.Num(), progs.ofs_exportinfo - progs.ofs_imports);
  vdlogf("Exports    %6d %6d", Writer.Exports.Num(), progs.ofs_exportdata - progs.ofs_exportinfo);
  vdlogf("Type data  %6d %6d", Writer.Exports.Num(), Writer.Tell() - progs.ofs_exportdata);
  vdlogf("TOTAL SIZE       %7d", Writer.Tell());

  // write header
  memcpy(progs.magic, PROG_MAGIC, 4);
  progs.version = PROG_VERSION;
  Writer.Seek(0);
  Writer.Serialise(progs.magic, 4);
  for (int i = 1; i < (int)sizeof(progs)/4; ++i) Writer << ((int *)&progs)[i];

  #ifdef OPCODE_STATS
  vdlogf("\n-----------------------------------------------\n");
  for (int i = 0; i < NUM_OPCODES; ++i) {
    vdlogf("%-16s %d", StatementInfo[i].name, StatementInfo[i].usecount);
  }
  vdlogf("%d opcodes", NUM_OPCODES);
  #endif

  fclose(f);
}


//==========================================================================
//
// VPackage::LoadBinaryObject
//
// will delete `Strm`
//
//==========================================================================
void VPackage::LoadBinaryObject (VStream *Strm, VStr filename, TLocation l) {
  if (!Strm) return;

  VProgsReader *Reader = new VProgsReader(Strm);

  // calcutate CRC
  /*
  #if !defined(IN_VCC)
  auto crc = TCRC16();
  for (int i = 0; i < Reader->TotalSize(); ++i) crc += Streamer<vuint8>(*Reader);
  #endif
  */

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
    /*
    VNameEntry E;
    *Reader << E;
    NameRemap[i] = E.Name;
    */
    char EName[NAME_SIZE+1];
    vuint8 len = 0;
    *Reader << len;
    vassert(len <= NAME_SIZE);
    if (len) Reader->Serialise(EName, len);
    EName[len] = 0;
    NameRemap[i] = VName(EName);
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

  /*
  #if !defined(IN_VCC)
  Checksum = crc;
  this->Reader = Reader;
  #endif
  */

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
        /*
        #if !defined(IN_VCC)
        Exports[i].Obj = VClass::FindClass(*Exports[i].Name);
        if (!Exports[i].Obj)
        #endif
        */
        {
          Exports[i].Obj = new VClass(Exports[i].Name, nullptr, TLocation());
        }
        break;
      default: ParseError(l, "Package '%s' contains corrupted data", *Name); BailOut();
    }
  }

  // read strings
  Reader->Seek(Progs.ofs_strings);
  StringInfo.clear();
  InitStringPool();
  {
    vint32 count = 0;
    *Reader << count;
    if (count < 0 || count >= 1024*1024*32) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
    for (int stridx = 0; stridx < count; ++stridx) {
      VStr s;
      *Reader << s;
      if (stridx == 0) {
        if (s.length() != 0) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
      } else {
        if (s.length() == 0 || !s[0]) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
      }
      int n = FindString(s);
      if (n != stridx) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
    }
  }

  // serialise objects
  Reader->Seek(Progs.ofs_exportdata);
  for (int i = 0; i < Progs.num_exports; ++i) {
    Exports[i].Obj->Serialise(*Reader);
    if (!Exports[i].Obj->Outer) Exports[i].Obj->Outer = this;
  }

  /*
  #if !defined(IN_VCC)
  // set up info tables
  //Reader->Seek(Progs.ofs_mobjinfo);
  //for (int i = 0; i < Progs.num_mobjinfo; ++i) *Reader << VClass::GMobjInfos.Alloc();

  //Reader->Seek(Progs.ofs_scriptids);
  //for (int i = 0; i < Progs.num_scriptids; ++i) *Reader << VClass::GScriptIds.Alloc();

  for (int i = 0; i < Progs.num_exports; ++i) Exports[i].Obj->PostLoad();

  // create default objects
  for (int i = 0; i < Progs.num_exports; ++i) {
    if (Exports[i].Obj->MemberType == MEMBER_Class) {
      VClass *vc = (VClass *)Exports[i].Obj;
      vc->CreateDefaults();
      if (!vc->Outer) vc->Outer = this;
    }
  }

  // we need to do this, 'cause k8vavoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
      }
    }
  }
  #endif
  */

  //k8: fuck you, shitplusplus: no finally
  //this->Reader = nullptr;
  delete Reader;
}
