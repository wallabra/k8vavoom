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
#include "vc_local.h"

enum {
  DTF_Unknown = -1,
  DTF_None = 0,
  DTF_Fields = 1,
  DTF_Dtor = 2,
};


//==========================================================================
//
//  VStruct::VStruct
//
//==========================================================================
VStruct::VStruct (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Struct, AName, AOuter, ALoc)
  , ParentStruct(nullptr)
  , IsVector(false)
  , StackSize(0)
  , Fields(nullptr)
  , ParentStructName(NAME_None)
  , Defined(true)
  , PostLoaded(false)
  , Size(0)
  , Alignment(0)
  , ReferenceFields(nullptr)
  , DestructorFields(nullptr)
  , AliasList()
  , AliasFrameNum(0)
  , cacheNeedDTor(DTF_Unknown)
  , cacheNeedCleanup(-1)
{
}


//==========================================================================
//
//  VStruct::CompilerShutdown
//
//==========================================================================
void VStruct::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  //AliasList.clear();
}


//==========================================================================
//
//  VStruct::ResolveAlias
//
//  returns `aname` for unknown alias, or `NAME_None` for alias loop
//
//==========================================================================
VName VStruct::ResolveAlias (VName aname, bool nocase) {
  if (aname == NAME_None) return NAME_None;
  if (!VObject::cliCaseSensitiveFields) nocase = true;
  if (++AliasFrameNum == 0x7fffffff) {
    for (auto it = AliasList.first(); it; ++it) it.getValue().aframe = 0;
    AliasFrameNum = 1;
  }
  VName res = aname;
  for (;;) {
    if (nocase) {
      bool found = false;
      for (auto it = AliasList.first(); it; ++it) {
        if (VStr::ICmp(*it.getKey(), *aname) == 0) {
          if (it.getValue().aframe == AliasFrameNum) return res; //NAME_None; // loop
          res = it.getValue().origName;
          it.getValue().aframe = AliasFrameNum;
          aname = res;
          found = true;
          break;
        }
      }
      if (!found) {
        if (!ParentStruct) return res;
        return ParentStruct->ResolveAlias(res, nocase);
      }
    } else {
      auto ai = AliasList.get(aname);
      if (!ai) {
        if (!ParentStruct) return res;
        return ParentStruct->ResolveAlias(res);
      }
      if (ai->aframe == AliasFrameNum) return NAME_None; // loop
      res = ai->origName;
      ai->aframe = AliasFrameNum;
      aname = res;
    }
  }
}


//==========================================================================
//
//  VStruct::AddMethod
//
//==========================================================================
void VStruct::AddMethod (VMethod *m) {
  if (!m) return; // just in case
  vassert(m->Outer == this);
  if (m->Name != NAME_None) {
    for (auto &&M : Methods) {
      if (M->Name == m->Name) {
        ParseError(m->Loc, "Redeclared method `%s`", *m->Name);
        ParseError(M->Loc, "Previous declaration here");
      }
    }
  }
  Methods.append(m);
}


//==========================================================================
//
//  VStruct::FindMethod
//
//==========================================================================
VMethod *VStruct::FindMethod (VName AName, bool bRecursive) {
  if (AName == NAME_None) return nullptr;
  AName = ResolveAlias(AName);
  VMethod *M = (VMethod *)StaticFindMember(AName, this, MEMBER_Method);
  if (M) return M;
  if (!bRecursive || !ParentStruct) return nullptr;
  return ParentStruct->FindMethod(AName, true);
}


//==========================================================================
//
//  VStruct::FindDtor
//
//==========================================================================
VMethod *VStruct::FindDtor (bool bRecursive) {
  VName dName = ResolveAlias("dtor");
  VMethod *M = (VMethod *)StaticFindMember(dName, this, MEMBER_Method);
  if (M) return M;
  if (bRecursive) {
    if (ParentStruct) return ParentStruct->FindDtor(true);
    // for non-defined structs
    if (ParentStructName != NAME_None) {
      VFieldType type = StaticFindType((Outer->MemberType == MEMBER_Class ? (VClass *)Outer : nullptr), ParentStructName);
      if (type.Type == TYPE_Struct) return type.Struct->FindDtor(true);
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VStruct::FindAccessibleMethod
//
//==========================================================================
VMethod *VStruct::FindAccessibleMethod (VName AName, VStruct *self, const TLocation *loc) {
  if (AName == NAME_None) return nullptr;
  AName = ResolveAlias(AName);
  VMethod *M = (VMethod *)StaticFindMember(AName, this, MEMBER_Method);
  if (M) {
    //fprintf(stderr, "FAM: <%s>; self=%s; this=%s; child=%d; loc=%p\n", *AName, (self ? *self->AName : "<none>"), *this->AName, (int)(self ? self->IsChildOf(this) : false), loc);
    if (loc) {
      //fprintf(stderr, "  FAM: <%s>; self=%s; this=%s; child=%d; flags=0x%04x\n", *AName, (self ? *self->AName : "<none>"), *this->AName, (int)(self ? self->IsChildOf(this) : false), M->Flags);
      if ((M->Flags&FUNC_Private) && this != self) ParseError(*loc, "Method `%s::%s` is private", *AName, *M->Name);
      if ((M->Flags&FUNC_Protected) && (!self || !self->IsA(this))) ParseError(*loc, "Method `%s::%s` is protected", *AName, *M->Name);
      return M;
    } else {
      if (!self) {
        if ((M->Flags&(FUNC_Private|FUNC_Protected)) == 0) return M;
      } else {
        if (M->Flags&FUNC_Private) {
          if (self == this) return M;
        } else if (M->Flags&FUNC_Protected) {
          if (self->IsA(this)) return M;
        } else {
          return M;
        }
      }
    }
  }
  return (ParentStruct ? ParentStruct->FindAccessibleMethod(AName, self, loc) : nullptr);
}


//==========================================================================
//
//  VStruct::AddField
//
//==========================================================================
void VStruct::AddField (VField *f) {
  VField *Prev = nullptr;
  for (VField *Check = Fields; Check; Prev = Check, Check = Check->Next) {
    if (f->Name != NAME_None && f->Name == Check->Name) {
      ParseError(f->Loc, "Redeclared field `%s`", *f->Name);
      ParseError(Check->Loc, "Previous declaration here");
    }
  }

  if (Prev) Prev->Next = f; else Fields = f;
  f->Next = nullptr;

  cacheNeedDTor = DTF_Unknown;
  cacheNeedCleanup = -1;
}


//==========================================================================
//
//  VStruct::FindField
//
//==========================================================================
VField *VStruct::FindField (VName FieldName) {
  if (FieldName == NAME_None) return nullptr;
  FieldName = ResolveAlias(FieldName);
  for (VField *fi = Fields; fi; fi = fi->Next) if (fi->Name == FieldName) return fi;
  if (ParentStruct) return ParentStruct->FindField(FieldName);
  return nullptr;
}


//==========================================================================
//
//  VStruct::UpdateDTorCache
//
//==========================================================================
void VStruct::UpdateDTorCache () {
  cacheNeedDTor = DTF_None;
  if (FindDtor()) cacheNeedDTor |= DTF_Dtor;
  for (VField *F = Fields; F; F = F->Next) {
    if (F->NeedsDestructor()) {
      cacheNeedDTor |= DTF_Fields;
      break;
    }
  }
  if (ParentStruct) {
    (void)ParentStruct->NeedsDestructor();
    cacheNeedDTor |= ParentStruct->cacheNeedDTor;
  }
}


//==========================================================================
//
//  VStruct::NeedsDestructor
//
//==========================================================================
bool VStruct::NeedsDestructor () {
  if (cacheNeedDTor == DTF_Unknown) UpdateDTorCache();
  return (cacheNeedDTor != DTF_None);
}


//==========================================================================
//
//  VStruct::NeedsFieldsDestruction
//
//==========================================================================
bool VStruct::NeedsFieldsDestruction () {
  if (cacheNeedDTor == DTF_Unknown) UpdateDTorCache();
  return !!(cacheNeedDTor&DTF_Fields);
}


//==========================================================================
//
//  VStruct::NeedsMethodDestruction
//
//==========================================================================
bool VStruct::NeedsMethodDestruction () {
  if (cacheNeedDTor == DTF_Unknown) UpdateDTorCache();
  return !!(cacheNeedDTor&DTF_Dtor);
}


//==========================================================================
//
//  VStruct::IsA
//
//==========================================================================
bool VStruct::IsA (const VStruct *s) const {
  if (!s) return false;
  for (const VStruct *me = this; me; me = me->ParentStruct) {
    if (s == me) return true;
  }
  return false;
}


//==========================================================================
//
//  VStruct::Define
//
//==========================================================================
bool VStruct::Define () {
  if (ParentStructName != NAME_None) {
    VFieldType type = StaticFindType((Outer->MemberType == MEMBER_Class ? (VClass *)Outer : nullptr), ParentStructName);
    if (type.Type != TYPE_Struct) {
      ParseError(ParentStructLoc, "%s is not a struct type", *ParentStructName);
    } else {
      ParentStruct = type.Struct;
    }
  }

  if (ParentStruct && !ParentStruct->Defined) {
    ParseError(ParentStructLoc, "Parent struct must be declared before `%s`", *Name);
    return false;
  }

  Defined = true;
  return true;
}


//==========================================================================
//
//  VStruct::DefineMembers
//
//==========================================================================
bool VStruct::DefineMembers () {
  bool Ret = true;

  // check for duplicate names
  TMapNC<VName, VMemberBase *> fmMap;

  // check (and remember) fields
  for (VStruct *st = this; st; st = st->ParentStruct) {
    for (VField *fi = Fields; fi; fi = fi->Next) {
      if (fi->Name == NAME_None) continue; // just in case, anonymous fields
      auto np = fmMap.find(fi->Name);
      if (np) {
        ParseError((*np)->Loc, "Redeclared field");
        ParseError(fi->Loc, "Previous declaration here");
      }
    }
  }

  // check methods
  for (auto &&mt : Methods) {
    if (mt->Name == NAME_None || VStr::strEqu(*mt->Name, "ctor") || VStr::strEqu(*mt->Name, "dtor")) continue; // just in case, anonymous fields
    auto np = fmMap.find(mt->Name);
    if (np) {
      ParseError(mt->Loc, "Field/method name conflict (%s) (previous it at %s)", *mt->Name, *(*np)->Loc.toStringNoCol());
    }
    if (ParentStruct) {
      VMethod *M = ParentStruct->FindMethod(mt->Name);
      if (M) {
        ParseError(mt->Loc, "Redeclared method `%s` (previous it at %s)", *mt->Name, *M->Loc.toStringNoCol());
      }
    }
  }

  // free memory, why not
  fmMap.clear();

  // define fields
  vint32 size = 0;
  if (ParentStruct) size = ParentStruct->StackSize*4;
  VField *PrevBool = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (!fi->Define()) Ret = false;
    if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000) {
      fi->Type.BitMask = PrevBool->Type.BitMask<<1;
    } else {
      size += fi->Type.GetStackSize();
    }
    PrevBool = (fi->Type.Type == TYPE_Bool ? fi : nullptr);
  }

  // validate vector type
  if (IsVector) {
    int fc = 0;
    for (VField *f = Fields; f; f = f->Next) {
      if (f->Type.Type != TYPE_Float) {
        ParseError(f->Loc, "Vector can have only float fields");
        Ret = false;
      }
      ++fc;
    }
    if (fc != 3) {
      ParseError(Loc, "Vector must have exactly 3 float fields");
      Ret = false;
    }
  }

  StackSize = (size+3)/4;

  // define methods
  for (auto &&mt : Methods) if (!mt->Define()) Ret = false;

  // check if destructor is valid
  VMethod *dtor = FindDtor();
  if (dtor) {
    if (!dtor->IsStructMethod() || dtor->IsIterator() || dtor->IsVarArgs() ||
        dtor->NumParams != 0 || dtor->ReturnType.Type != TYPE_Void)
    {
      ParseError(dtor->Loc, "Invalid destructor signature for struct `%s`", *Name);
      Ret = false;
    }
  }

  //GLog.Logf(NAME_Debug, "VStruct::DefineMembers:<%s>; size=%d", *Name, Size);
  return Ret;
}


//==========================================================================
//
//  VStruct::Emit
//
//==========================================================================
void VStruct::Emit () {
  // emit method code
  for (auto &&mt : Methods) mt->Emit();
}


//==========================================================================
//
//  VStruct::PostLoad
//
//==========================================================================
void VStruct::PostLoad () {
  if (PostLoaded) return; // already done

  // make sure parent struct has been set up
  if (ParentStruct) ParentStruct->PostLoad();

  // calculate field offsets and class size
  CalcFieldOffsets();

  // set up list of reference fields
  InitReferences();

  // set up list of destructor fields
  InitDestructorFields();

  PostLoaded = true;
}


//==========================================================================
//
//  VStruct::CalcFieldOffsets
//
//==========================================================================
void VStruct::CalcFieldOffsets () {
  int size = (ParentStruct ? ParentStruct->Size : 0);
  Alignment = (ParentStruct ? ParentStruct->Alignment : 0);
  VField *PrevField = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Bool && PrevField &&
        PrevField->Type.Type == TYPE_Bool &&
        PrevField->Type.BitMask != 0x80000000)
    {
      vuint32 bit_mask = PrevField->Type.BitMask << 1;
      if (fi->Type.BitMask != bit_mask) VPackage::InternalFatalError("Wrong bit mask");
      fi->Type.BitMask = bit_mask;
      fi->Ofs = PrevField->Ofs;
    } else {
      if (fi->Type.Type == TYPE_Struct ||
          (fi->Type.Type == TYPE_Array && fi->Type.ArrayInnerType == TYPE_Struct))
      {
        // make sure struct size has been calculated
        fi->Type.Struct->PostLoad();
      }
      // align field offset
      int FldAlign = fi->Type.GetAlignment();
      size = (size+FldAlign-1)&~(FldAlign-1);
      // structure itself has the bigest alignment
      if (Alignment < FldAlign) Alignment = FldAlign;
      fi->Ofs = size;
      size += fi->Type.GetSize();
    }
    PrevField = fi;
  }
  if (!Alignment) Alignment = 1;
  Size = (size+Alignment-1)&~(Alignment-1);
}


//==========================================================================
//
//  VStruct::InitReferences
//
//==========================================================================
void VStruct::InitReferences () {
  // invalidate caches (just in case)
  cacheNeedDTor = DTF_Unknown;
  cacheNeedCleanup = -1;
  ReferenceFields = nullptr;
  if (ParentStruct) ReferenceFields = ParentStruct->ReferenceFields;
  for (VField *F = Fields; F; F = F->Next) {
    switch (F->Type.Type) {
      case TYPE_Reference:
      case TYPE_Delegate:
        F->NextReference = ReferenceFields;
        ReferenceFields = F;
        break;
      case TYPE_Struct:
        F->Type.Struct->PostLoad();
        if (F->Type.Struct->ReferenceFields) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        }
        break;
      case TYPE_Array:
      case TYPE_DynamicArray:
        if (F->Type.ArrayInnerType == TYPE_Reference) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        } else if (F->Type.ArrayInnerType == TYPE_Struct) {
          F->Type.Struct->PostLoad();
          if (F->Type.Struct->ReferenceFields) {
            F->NextReference = ReferenceFields;
            ReferenceFields = F;
          }
        }
        break;
      case TYPE_Dictionary:
        if (F->Type.GetDictKeyType().Type == TYPE_Reference || F->Type.GetDictValueType().Type == TYPE_Reference) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        } else {
          if (F->Type.GetDictKeyType().Type == TYPE_Struct) {
            F->Type.KStruct->PostLoad();
            if (F->Type.KStruct->ReferenceFields) {
              F->NextReference = ReferenceFields;
              ReferenceFields = F;
              break;
            }
          }
          if (F->Type.GetDictValueType().Type == TYPE_Struct) {
            F->Type.Struct->PostLoad();
            if (F->Type.Struct->ReferenceFields) {
              F->NextReference = ReferenceFields;
              ReferenceFields = F;
              break;
            }
          }
        }
        break;
    }
  }
}


//==========================================================================
//
//  VStruct::InitDestructorFields
//
//==========================================================================
void VStruct::InitDestructorFields () {
  // invalidate caches (just in case)
  cacheNeedDTor = DTF_Unknown;
  cacheNeedCleanup = -1;
  DestructorFields = nullptr;
  if (ParentStruct) DestructorFields = ParentStruct->DestructorFields;
  for (VField *F = Fields; F; F = F->Next) {
    switch (F->Type.Type) {
      case TYPE_Reference:
      case TYPE_Delegate:
        cacheNeedCleanup = 1;
        break;
      case TYPE_String:
        cacheNeedDTor |= DTF_Fields; // anyway
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
      case TYPE_Struct:
        F->Type.Struct->PostLoad();
        if (F->Type.Struct->DestructorFields) {
          cacheNeedDTor |= DTF_Fields; // anyway
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        }
        break;
      case TYPE_Array:
        if (F->Type.ArrayInnerType == TYPE_String) {
          cacheNeedDTor |= DTF_Fields; // anyway
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        } else if (F->Type.ArrayInnerType == TYPE_Struct) {
          F->Type.Struct->PostLoad();
          if (F->Type.Struct->DestructorFields) {
            cacheNeedDTor |= DTF_Fields; // anyway
            F->DestructorLink = DestructorFields;
            DestructorFields = F;
          }
        }
        break;
      case TYPE_DynamicArray:
      case TYPE_Dictionary:
        cacheNeedDTor |= DTF_Fields; // anyway
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
    }
  }
  if (FindDtor()) cacheNeedDTor = DTF_Dtor;
}


//==========================================================================
//
//  VStruct::SkipSerialisedObject
//
//==========================================================================
void VStruct::SkipSerialisedObject (VStream &Strm) {
  VName psname = NAME_None;
  Strm << psname;
  if (psname != NAME_None) SkipSerialisedObject(Strm); // skip parent struct
  vint32 fldcount = 0;
  Strm << STRM_INDEX(fldcount);
  while (fldcount--) {
    VName fname = NAME_None;
    Strm << fname;
    VField::SkipSerialisedValue(Strm);
  }
}


//==========================================================================
//
//  VStruct::SerialiseObject
//
//==========================================================================
void VStruct::SerialiseObject (VStream &Strm, vuint8 *Data) {
  bool debugDump = VObject::cliShowIODebugMessages;
  if (Strm.IsLoading()) {
    // reading
    // read field count
    vint32 fldcount = -1;
    Strm << STRM_INDEX(fldcount);
    if (fldcount < 0) VPackage::IOError(va("invalid number of saved fields in struct `%s` (%d)", *Name, fldcount));
    if (fldcount == 0) return; // nothing to do
    // build field list to speedup loading
    TMapNC<VName, VField *> fldmap;
    TMapNC<VName, bool> fldseen;
    for (VStruct *st = this; st; st = st->ParentStruct) {
      for (VField *fld = st->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldmap.put(fld->Name, fld)) VPackage::IOError(va("duplicate field `%s` in struct `%s`", *fld->Name, *Name));
      }
    }
    // now load fields
    while (fldcount--) {
      VName fldname = NAME_None;
      Strm << fldname;
      auto fpp = fldmap.find(fldname);
      if (!fpp) {
        GLog.WriteLine(NAME_Warning, "saved field `%s` not found in struct `%s`, value ignored", *fldname, *Name);
        VField::SkipSerialisedValue(Strm);
      } else {
        if (fldseen.put(fldname, true)) {
          GLog.WriteLine(NAME_Warning, "duplicate saved field `%s` in struct `%s`", *fldname, *Name);
        }
        if (debugDump) GLog.WriteLine("VC I/O: loading field `%s` of struct `%s`",  *fldname, *Name);
        VField *fld = *fpp;
        VField::SerialiseFieldValue(Strm, Data+fld->Ofs, fld->Type);
      }
    }
    // show missing fields
    for (auto fit = fldmap.first(); fit; ++fit) {
      VName fldname = fit.getKey();
      if (!fldseen.has(fldname)) {
        GLog.WriteLine(NAME_Warning, "field `%s` is missing in saved data for struct `%s`", *fldname, *Name);
      }
    }
  } else {
    // writing
    // count fields, collect them into array
    // serialise fields
    TMapNC<VName, bool> fldseen;
    TArray<VField *> fldlist;
    for (VStruct *st = this; st; st = st->ParentStruct) {
      for (VField *fld = st->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldseen.put(fld->Name, true)) VPackage::IOError(va("duplicate field `%s` in struct `%s`", *fld->Name, *Name));
        fldlist.append(fld);
      }
    }
    // now write all fields in backwards order, so they'll appear in natural order in stream
    vint32 fldcount = fldlist.length();
    Strm << STRM_INDEX(fldcount);
    for (int f = fldlist.length()-1; f >= 0; --f) {
      VField *fld = fldlist[f];
      Strm << fld->Name;
      VField::SerialiseFieldValue(Strm, Data+fld->Ofs, fld->Type);
    }
  }
}


//==========================================================================
//
//  VStruct::DeepCopyObject
//
//==========================================================================
void VStruct::DeepCopyObject (vuint8 *Dst, const vuint8 *Src) {
  // copy parent struct's fields
  if (ParentStruct) ParentStruct->DeepCopyObject(Dst, Src);
  // copy fields
  for (VField *F = Fields; F; F = F->Next) {
    if (F->Flags&FIELD_Internal) continue;
    VField::CopyFieldValue(Src+F->Ofs, Dst+F->Ofs, F->Type);
  }
}


//==========================================================================
//
//  VStruct::NeedToCleanObject
//
//==========================================================================
bool VStruct::NeedToCleanObject () {
  if (cacheNeedCleanup >= 0) return (cacheNeedCleanup != 0);
  cacheNeedCleanup = 1;
  //for (VField *F = ReferenceFields; F; F = F->NextReference) if (VField::NeedToCleanField(F->Type)) return true;
  for (VField *F = Fields; F; F = F->Next) if (VField::NeedToCleanField(F->Type)) return true;
  if (ParentStruct && ParentStruct->NeedToCleanObject()) return true;
  cacheNeedCleanup = 0;
  return false;
}


//==========================================================================
//
//  VStruct::CleanObject
//
//==========================================================================
bool VStruct::CleanObject (vuint8 *Data) {
  bool res = false;
  for (VField *F = ReferenceFields; F; F = F->NextReference) {
    if (VField::CleanField(Data+F->Ofs, F->Type)) res = true;
  }
  return res;
}


//==========================================================================
//
//  VStruct::DestructObject
//
//==========================================================================
void VStruct::DestructObject (vuint8 *Data) {
  for (VField *F = DestructorFields; F; F = F->DestructorLink) VField::DestructField(Data+F->Ofs, F->Type);
}


//==========================================================================
//
//  VStruct::ZeroObject
//
//==========================================================================
void VStruct::ZeroObject (vuint8 *Data, bool calldtors) {
  //for (VField *F = Fields; F; F = F->Next) VField::DestructField(Data+F->Ofs, F->Type, true);
  // destruct all fields that need to be destructed, and memset the struct
  if (calldtors) {
    for (VField *F = DestructorFields; F; F = F->DestructorLink) VField::DestructField(Data+F->Ofs, F->Type);
  }
  //GLog.Logf(NAME_Debug, "memsetting struct <%s> (%d bytes)", *Name, Size);
  if (Size > 0) memset(Data, 0, Size);
}


//==========================================================================
//
//  VStruct::IdenticalObject
//
//==========================================================================
bool VStruct::IdenticalObject (const vuint8 *Val1, const vuint8 *Val2, bool vecprecise) {
  // compare parent struct's fields
  if (ParentStruct) {
    if (!ParentStruct->IdenticalObject(Val1, Val2, vecprecise)) return false;
  }
  // compare fields
  for (VField *F = Fields; F; F = F->Next) {
    if (!VField::IdenticalValue(Val1+F->Ofs, Val2+F->Ofs, F->Type, vecprecise)) return false;
  }
  return true;
}


//==========================================================================
//
//  VStruct::NetSerialiseObject
//
//==========================================================================
bool VStruct::NetSerialiseObject (VStream &Strm, VNetObjectsMapBase *Map, vuint8 *Data, bool vecprecise) {
  bool Ret = true;
  // serialise parent struct's fields
  if (ParentStruct) Ret = ParentStruct->NetSerialiseObject(Strm, Map, Data, vecprecise);
  // serialise fields
  for (VField *F = Fields; F; F = F->Next) {
    if (!VField::NetSerialiseValue(Strm, Map, Data+F->Ofs, F->Type, vecprecise)) Ret = false;
  }
  return Ret;
}


//==========================================================================
//
//  VStruct::CreateWrapperStruct
//
//==========================================================================
VStruct *VStruct::CreateWrapperStruct (VExpression *aTypeExpr, VMemberBase *AOuter, TLocation ALoc) {
  vassert(aTypeExpr);
  vassert(AOuter);
  VStruct *st = new VStruct(NAME_None, AOuter, ALoc);
  st->Defined = false;
  st->IsVector = false;
  st->Fields = nullptr;

  VField *fi = new VField(VName("__"), st, ALoc);
  fi->TypeExpr = aTypeExpr;
  if (aTypeExpr->IsDelegateType()) {
    fi->Func = ((VDelegateType *)aTypeExpr)->CreateDelegateMethod(st);
    fi->Type = VFieldType(TYPE_Delegate);
    fi->Type.Function = fi->Func;
    fi->TypeExpr = nullptr;
    delete aTypeExpr;
  }
  st->AddField(fi);

  return st;
}
