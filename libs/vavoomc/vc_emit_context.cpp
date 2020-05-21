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


VStatementInfo StatementInfo[NUM_OPCODES] = {
#define DECLARE_OPC(name, args)   { #name, OPCARGS_##args, 0}
#define OPCODE_INFO
#include "vc_progdefs.h"
};

VStatementBuiltinInfo StatementBuiltinInfo[] = {
#define BUILTIN_OPCODE_INFO
#define DECLARE_OPC_BUILTIN(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

VStatementBuiltinInfo StatementDictDispatchInfo[] = {
#define DICTDISPATCH_OPCODE_INFO
#define DECLARE_OPC_DICTDISPATCH(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

VStatementBuiltinInfo StatementDynArrayDispatchInfo[] = {
#define DYNARRDISPATCH_OPCODE_INFO
#define DECLARE_OPC_DYNARRDISPATCH(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"


//==========================================================================
//
//  VEmitContext::VEmitContext
//
//==========================================================================
VEmitContext::VEmitContext (VMemberBase *Member)
  : CurrentFunc(nullptr)
  , SelfClass(nullptr)
  , SelfStruct(nullptr)
  , Package(nullptr)
  , IndArray(nullptr)
  , OuterClass(nullptr)
  , FuncRetType(TYPE_Unknown)
  //, localsofs(0)
  , InDefaultProperties(false)
  , VCallsDisabled(false)
{
  // find the class
  VMemberBase *CM = Member;
  while (CM && CM->MemberType != MEMBER_Class) CM = CM->Outer;
  OuterClass = SelfClass = (VClass *)CM;

  VMemberBase *PM = Member;
  while (PM != nullptr && PM->MemberType != MEMBER_Package) PM = PM->Outer;
  Package = (VPackage *)PM;

  // check for struct method
  if (Member != nullptr && Member->MemberType == MEMBER_Method) {
    VMethod *mt = (VMethod *)Member;
    if (mt->Flags&FUNC_StructMethod) {
      VMemberBase *SM = Member;
      while (SM && SM->MemberType != MEMBER_Struct) {
        if (SM == SelfClass) { SM = nullptr; break; }
        SM = SM->Outer;
      }
      if (SM) {
        //GLog.Logf(NAME_Debug, "compiling struct method `%s`", *Member->GetFullName());
        //SelfClass = nullptr; // we still need outer class
        SelfStruct = (VStruct *)SM;
      }
    }
  }

  if (Member != nullptr && Member->MemberType == MEMBER_Method) {
    CurrentFunc = (VMethod *)Member;
    //CurrentFunc->SelfTypeClass = nullptr;
    CurrentFunc->Instructions.Clear();
    //k8: nope, don't do this. many (most!) states with actions are creating anonymous functions
    //    with several statements only. this is ALOT of functions. and each get several kb of RAM.
    //    hello, OOM on any moderately sized mod. yay.
    //CurrentFunc->Instructions.Resize(1024);
    FuncRetType = CurrentFunc->ReturnType;
    if (SelfStruct) {
      vassert(CurrentFunc->Flags&FUNC_StructMethod);
    } else {
      vassert(!(CurrentFunc->Flags&FUNC_StructMethod));
    }
    // process `self(ClassName)`
    if (CurrentFunc->SelfTypeName != NAME_None) {
      if (SelfStruct) {
        ParseError(CurrentFunc->Loc, "You cannot force self for struct method `%s`", *CurrentFunc->GetFullName());
        CurrentFunc->SelfTypeName = NAME_None;
      } else {
        VClass *newSelfClass = VMemberBase::StaticFindClass(CurrentFunc->SelfTypeName);
        if (!newSelfClass) {
          ParseError(CurrentFunc->Loc, "No such class `%s` for forced self in method `%s`", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
        } else {
          if (newSelfClass) {
            VClass *cc = SelfClass;
            while (cc && cc != newSelfClass) cc = cc->ParentClass;
                 if (!cc) ParseError(CurrentFunc->Loc, "Forced self `%s` for class `%s`, which is not super (method `%s`)", *CurrentFunc->SelfTypeName, SelfClass->GetName(), *CurrentFunc->GetFullName());
            //else if (cc == SelfClass) ParseWarning(CurrentFunc->Loc, "Forced self `%s` for the same class (old=%s; new=%s) (method `%s`)", *CurrentFunc->SelfTypeName, *SelfClass->GetFullName(), *cc->GetFullName(), *CurrentFunc->GetFullName());
            //else GLog.Logf(NAME_Debug, "%s: forced class `%s` for class `%s` (method `%s`)", *CurrentFunc->Loc.toStringNoCol(), *CurrentFunc->SelfTypeName, SelfClass->GetName(), *CurrentFunc->GetFullName());
            if (!cc->Defined) ParseError(CurrentFunc->Loc, "Forced self class `%s` is not defined for method `%s`", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
            SelfClass = cc;
            if (CurrentFunc->SelfTypeClass && CurrentFunc->SelfTypeClass != cc) Sys_Error("internal compiler error (SelfTypeName)");
            CurrentFunc->SelfTypeClass = cc;
          } else {
            ParseError(CurrentFunc->Loc, "Forced self `%s` for nothing (wtf?!) (method `%s`)", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
          }
        }
      }
    }
  }

  stackInit();
}


//==========================================================================
//
//  VEmitContext::EndCode
//
//==========================================================================
void VEmitContext::EndCode () {
  //if (lastFin) VCFatalError("Internal compiler error: unbalanced finalizers");
  //if (lastBC) VCFatalError("Internal compiler error: unbalanced break/cont");
  //if (scopeList.length() != 0) VCFatalError("Internal compiler error: unbalanced scopes");

  // fix-up labels.
  for (int i = 0; i < Fixups.length(); ++i) {
    if (Labels[Fixups[i].LabelIdx] < 0) VCFatalError("Label was not marked");
    if (Fixups[i].Arg == 1) {
      CurrentFunc->Instructions[Fixups[i].Pos].Arg1 = Labels[Fixups[i].LabelIdx];
    } else {
      CurrentFunc->Instructions[Fixups[i].Pos].Arg2 = Labels[Fixups[i].LabelIdx];
    }
  }

#ifdef OPCODE_STATS
  for (int i = 0; i < CurrentFunc->Instructions.length(); ++i) {
    ++StatementInfo[CurrentFunc->Instructions[i].Opcode].usecount;
  }
#endif

  // dummy finishing instruction should always present
  FInstruction &Dummy = CurrentFunc->Instructions.Alloc();
  Dummy.Opcode = OPC_Done;

  CurrentFunc->Instructions.condense();

  //CurrentFunc->DumpAsm();
}


//==========================================================================
//
//  VEmitContext::ClearLocalDefs
//
//==========================================================================
void VEmitContext::ClearLocalDefs () {
  LocalDefs.Clear();
  stackInit();
}


//==========================================================================
//
//  VEmitContext::stackInit
//
//==========================================================================
void VEmitContext::stackInit () {
  memset(slotInfo, 0, sizeof(slotInfo));
}


//==========================================================================
//
//  VEmitContext::stackAlloc
//
//  size in stack slots; returns -1 on error
//
//==========================================================================
int VEmitContext::stackAlloc (int size, bool *reused) {
  if (size < 0 || size > MaxStackSlots) return -1;

  if (size == 0) {
    int seenFree = -1;
    for (int f = 0; f < MaxStackSlots; ++f) {
      if (slotInfo[f] == SlotUnused) {
        if (reused) *reused = false;
        return f;
      }
      if (seenFree < 0 && slotInfo[f] == SlotFree) seenFree = f;
    }
    if (reused) *reused = true;
    return seenFree;
  }

  int spos = 0;
  while (spos < MaxStackSlots) {
    unsigned char csl = slotInfo[spos];
    if (csl != SlotUsed) {
      int send = spos+1;
      while (send < MaxStackSlots) {
        const unsigned char ssl = slotInfo[send];
        if (ssl == SlotUsed) break;
        csl |= ssl;
        ++send;
      }
      // does it fit?
      if (send-spos >= size) {
        // mark used
        memset(slotInfo+spos, SlotUsed, size);
        if (reused) *reused = !!csl;
        return spos;
      }
    } else {
      ++spos;
    }
  }

  // no such free slot
  return -1;
}


//==========================================================================
//
//  VEmitContext::stackFree
//
//==========================================================================
void VEmitContext::stackFree (int pos, int size) {
  vassert(pos >= 0);
  vassert(size >= 0);
  vassert(pos < MaxStackSlots);
  vassert(size <= MaxStackSlots);
  vassert(pos+size <= MaxStackSlots);
  if (size == 0) return;

  for (int f = pos; f < pos+size; ++f) {
    vassert(slotInfo[f] == SlotUsed);
    slotInfo[f] = SlotFree;
  }
}


//==========================================================================
//
//  VEmitContext::AllocateLocalSlot
//
//  allocate stack slot for this local
//  returns `true` if we got a second-hand stack slot
//
//==========================================================================
void VEmitContext::AllocateLocalSlot (int idx) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  if (loc.invalid) return; // already dead
  vassert(loc.Offset == -666);
  vassert(loc.stackSize >= 0);
  bool realloced = true; // just in case
  const int ofs = stackAlloc(loc.stackSize, &realloced);
  if (ofs < 0) {
    ParseError(loc.Loc, "too many local vars");
    loc.Offset = 1;
    loc.invalid = true;
    loc.reused = false; // it doesn't matter
  } else {
    loc.Offset = ofs;
    loc.reused = realloced;
  }
}


//==========================================================================
//
//  VEmitContext::ReleaseLocalSlot
//
//==========================================================================
void VEmitContext::ReleaseLocalSlot (int idx) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  if (loc.invalid) return; // already dead
  vassert(loc.Offset >= 0);
  vassert(loc.stackSize >= 0);
  stackFree(loc.Offset, loc.stackSize);
  loc.Offset = -666;
}


//==========================================================================
//
//  VEmitContext::ReserveStack
//
//  reserve stack slots; used to setup function arguments
//
//==========================================================================
int VEmitContext::ReserveStack (int size) {
  vassert(size >= 0 && size <= MaxStackSlots);
  int pos = 0;
  while (pos < MaxStackSlots) {
    const unsigned char csl = slotInfo[pos];
    if (csl == SlotUnused) break;
    if (csl == SlotFree) VCFatalError("cannot reserve stack after any local allocation");
    ++pos;
  }
  if (pos+size > MaxStackSlots) return -1; // oops
  // check stack integrity
  for (int f = pos; f < pos+size; ++f) {
    const unsigned char csl = slotInfo[pos];
    if (csl != SlotUnused) VCFatalError("cannot reserve stack after any local allocation");
  }
  if (size > 0) memset(slotInfo+pos, SlotUsed, size);
  return pos;
}


//==========================================================================
//
//  VEmitContext::ReserveLocalSlot
//
//  reserve slot for this local; used to setup function arguments
//
//==========================================================================
void VEmitContext::ReserveLocalSlot (int idx) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  vassert(!loc.invalid);
  vassert(!loc.reused);
  vassert(loc.Offset == -666);
  vassert(loc.stackSize >= 0);
  //int size = (loc.ParamFlags&(FPARM_Out|FPARM_Ref) ? 1 : loc.stackSize);
  const int ofs = ReserveStack(loc.stackSize);
  if (ofs < 0) {
    ParseError(loc.Loc, "too many local vars");
    loc.Offset = 1;
    loc.invalid = true;
  } else {
    loc.Offset = ofs;
  }
}


//==========================================================================
//
//  VEmitContext::CalcUsedStackSize
//
//==========================================================================
int VEmitContext::CalcUsedStackSize () const noexcept {
  for (int res = MaxStackSlots-1; res >= 0; --res) {
    if (slotInfo[res] != SlotUnused) return res+1;
  }
  return 0;
}


//==========================================================================
//
//  VEmitContext::NewLocal
//
//==========================================================================
VLocalVarDef &VEmitContext::NewLocal (VName aname, const VFieldType &atype, const TLocation &aloc, vuint32 pflags) {
  const int ssz = (pflags&(FPARM_Out|FPARM_Ref) ? 1 : atype.GetStackSize()/4);

  VLocalVarDef &loc = LocalDefs.Alloc();
  loc.Name = aname;
  loc.Loc = aloc;
  loc.Offset = -666;
  loc.Type = atype;
  loc.Visible = true;
  loc.WasRead = false;
  loc.WasWrite = false;
  loc.ParamFlags = pflags;
  loc.ldindex = LocalDefs.length()-1;
  loc.reused = false;
  loc.stackSize = ssz;
  loc.invalid = false;

  return loc;
}


//==========================================================================
//
//  VEmitContext::GetLocalByIndex
//
//==========================================================================
VLocalVarDef &VEmitContext::GetLocalByIndex (int idx) {
  if (idx < 0 || idx >= LocalDefs.length()) Sys_Error("VC INTERNAL COMPILER ERROR IN `VEmitContext::GetLocalByIndex()`");
  return LocalDefs[idx];
}


//==========================================================================
//
//  VEmitContext::CheckForLocalVar
//
//==========================================================================
int VEmitContext::CheckForLocalVar (VName Name) {
  if (Name == NAME_None) return -1;
  for (int i = LocalDefs.length()-1; i >= 0; --i) {
    const VLocalVarDef &loc = LocalDefs[i];
    if (!loc.Visible) continue;
    if (loc.Name == Name) return i;
  }
  return -1;
}


//==========================================================================
//
//  VEmitContext::GetLocalVarType
//
//==========================================================================
VFieldType VEmitContext::GetLocalVarType (int idx) {
  if (idx < 0 || idx >= LocalDefs.length()) return VFieldType(TYPE_Unknown);
  return LocalDefs[idx].Type;
}


//==========================================================================
//
//  VEmitContext::DefineLabel
//
//==========================================================================
VLabel VEmitContext::DefineLabel () {
  Labels.Append(-1);
  return VLabel(Labels.length()-1);
}


//==========================================================================
//
//  VEmitContext::MarkLabel
//
//==========================================================================
void VEmitContext::MarkLabel (VLabel l) {
  if (l.Index < 0 || l.Index >= Labels.length()) VCFatalError("Bad label index %d", l.Index);
  if (Labels[l.Index] >= 0) VCFatalError("Label has already been marked");
  Labels[l.Index] = CurrentFunc->Instructions.length();
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_None) VCFatalError("Opcode doesn't take 0 params");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = 0;
  I.Arg2 = 0;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, int parm1, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Byte &&
      StatementInfo[statement].Args != OPCARGS_Short &&
      StatementInfo[statement].Args != OPCARGS_Int &&
      StatementInfo[statement].Args != OPCARGS_String)
  {
    VCFatalError("Opcode doesn't take 1 param");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = parm1;
  I.Arg2 = 0;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, float FloatArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Int) VCFatalError("Opcode does't take float argument");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = *(vint32 *)&FloatArg;
  I.Arg1IsFloat = true;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VName NameArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Name) VCFatalError("Opcode does't take name argument");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.NameArg = NameArg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VMemberBase *Member, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Member &&
      StatementInfo[statement].Args != OPCARGS_FieldOffset &&
      StatementInfo[statement].Args != OPCARGS_VTableIndex)
  {
    VCFatalError("Opcode does't take member as argument");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Member = Member;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VMemberBase *Member, int Arg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_VTableIndex_Byte &&
      StatementInfo[statement].Args != OPCARGS_FieldOffset_Byte &&
      StatementInfo[statement].Args != OPCARGS_Member_Int)
  {
    VCFatalError("Opcode does't take member and byte as argument");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Member = Member;
  I.Arg2 = Arg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_TypeSize &&
      StatementInfo[statement].Args != OPCARGS_Type &&
      StatementInfo[statement].Args != OPCARGS_A2DDimsAndSize)
  {
    VCFatalError("Opcode doesn't take type as argument");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, int Arg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Type_Int &&
      StatementInfo[statement].Args != OPCARGS_ArrElemType_Int &&
      StatementInfo[statement].Args != OPCARGS_TypeAD)
  {
    VCFatalError("Opcode doesn't take type as argument");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.Arg2 = Arg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, const VFieldType &TypeArg1, int OpCode, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_TypeDD) {
    VCFatalError("Opcode doesn't take types as argument");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.TypeArg1 = TypeArg1;
  I.Arg2 = OpCode;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VLabel Lbl, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_BranchTarget) VCFatalError("Opcode does't take label as argument");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = 0;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = CurrentFunc->Instructions.length()-1;
  Fix.Arg = 1;
  Fix.LabelIdx = Lbl.Index;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, int parm1, VLabel Lbl, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_ByteBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_ShortBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_IntBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_NameBranchTarget)
  {
    VCFatalError("Opcode does't take 2 params");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = parm1;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = CurrentFunc->Instructions.length()-1;
  Fix.Arg = 2;
  Fix.LabelIdx = Lbl.Index;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddBuiltin
//
//==========================================================================
void VEmitContext::AddBuiltin (int b, const TLocation &aloc) {
  //if (StatementInfo[statement].Args != OPCARGS_Builtin) VCFatalError("Opcode does't take builtin");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = OPC_Builtin;
  I.Arg1 = b;
  I.Arg2 = 0;
}


//==========================================================================
//
//  VEmitContext::EmitPushNumber
//
//==========================================================================
void VEmitContext::EmitPushNumber (int Val, const TLocation &aloc) {
       if (Val == 0) AddStatement(OPC_PushNumber0, aloc);
  else if (Val == 1) AddStatement(OPC_PushNumber1, aloc);
  else if (Val >= 0 && Val < 256) AddStatement(OPC_PushNumberB, Val, aloc);
  //else if (Val >= MIN_VINT16 && Val <= MAX_VINT16) AddStatement(OPC_PushNumberS, Val, aloc);
  else AddStatement(OPC_PushNumber, Val, aloc);
}


//==========================================================================
//
//  VEmitContext::EmitLocalAddress
//
//==========================================================================
void VEmitContext::EmitLocalAddress (int Ofs, const TLocation &aloc) {
       if (Ofs == 0) AddStatement(OPC_LocalAddress0, aloc);
  else if (Ofs == 1) AddStatement(OPC_LocalAddress1, aloc);
  else if (Ofs == 2) AddStatement(OPC_LocalAddress2, aloc);
  else if (Ofs == 3) AddStatement(OPC_LocalAddress3, aloc);
  else if (Ofs == 4) AddStatement(OPC_LocalAddress4, aloc);
  else if (Ofs == 5) AddStatement(OPC_LocalAddress5, aloc);
  else if (Ofs == 6) AddStatement(OPC_LocalAddress6, aloc);
  else if (Ofs == 7) AddStatement(OPC_LocalAddress7, aloc);
  else if (Ofs < 256) AddStatement(OPC_LocalAddressB, Ofs, aloc);
  else if (Ofs < MAX_VINT16) AddStatement(OPC_LocalAddressS, Ofs, aloc);
  else AddStatement(OPC_LocalAddress, Ofs, aloc);
}


//==========================================================================
//
//  VEmitContext::EmitPushPointedCode
//
//==========================================================================
void VEmitContext::EmitPushPointedCode (VFieldType type, const TLocation &aloc) {
  switch (type.Type) {
    case TYPE_Int:
    case TYPE_Float:
    case TYPE_Name:
      AddStatement(OPC_PushPointed, aloc);
      break;
    case TYPE_Byte:
      AddStatement(OPC_PushPointedByte, aloc);
      break;
    case TYPE_Bool:
           if (type.BitMask&0x000000ff) AddStatement(OPC_PushBool0, (int)(type.BitMask), aloc);
      else if (type.BitMask&0x0000ff00) AddStatement(OPC_PushBool1, (int)(type.BitMask>>8), aloc);
      else if (type.BitMask&0x00ff0000) AddStatement(OPC_PushBool2, (int)(type.BitMask>>16), aloc);
      else AddStatement(OPC_PushBool3, (int)(type.BitMask>>24), aloc);
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      AddStatement(OPC_PushPointedPtr, aloc);
      break;
    case TYPE_Vector:
      AddStatement(OPC_VPushPointed, aloc);
      break;
    case TYPE_String:
      AddStatement(OPC_PushPointedStr, aloc);
      break;
    case TYPE_Delegate:
      AddStatement(OPC_PushPointedDelegate, aloc);
      break;
    case TYPE_SliceArray:
      AddStatement(OPC_PushPointedSlice, aloc);
      break;
    default:
      ParseError(aloc, "Bad push pointed");
      break;
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalValue
//
//==========================================================================
void VEmitContext::EmitLocalValue (int lcidx, const TLocation &aloc, int xofs) {
  if (lcidx < 0 || lcidx >= LocalDefs.length()) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalValue): lcidx=%d; max=%d", lcidx, LocalDefs.length());
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalValue): ofs=%d (lcidx=%d; name=`%s`; %s)", Ofs, lcidx, *loc.Name, *loc.Loc.toStringNoCol());
  if (Ofs < 256 && loc.Type.Type != TYPE_Delegate) {
    switch (loc.Type.Type) {
      case TYPE_Vector:
        AddStatement(OPC_VLocalValueB, Ofs, aloc);
        break;
      case TYPE_String:
        AddStatement(OPC_StrLocalValueB, Ofs, aloc);
        break;
      case TYPE_Bool:
        if (loc.Type.BitMask != 1) ParseError(aloc, "Strange local bool mask");
        /* fallthrough */
      default:
        if (Ofs >= 0 && Ofs <= 7) AddStatement(OPC_LocalValue0+Ofs, aloc);
        else AddStatement(OPC_LocalValueB, Ofs, aloc);
        break;
    }
  } else {
    EmitLocalAddress(loc.Offset, aloc);
    EmitPushPointedCode(loc.Type, aloc);
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalPtrValue
//
//==========================================================================
void VEmitContext::EmitLocalPtrValue (int lcidx, const TLocation &aloc, int xofs) {
  if (lcidx < 0 || lcidx >= LocalDefs.length()) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalPtrValue): lcidx=%d; max=%d", lcidx, LocalDefs.length());
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalPtrValue): ofs=%d (lcidx=%d; name=`%s`; %s)", Ofs, lcidx, *loc.Name, *loc.Loc.toStringNoCol());
  if (Ofs < 256) {
    if (Ofs >= 0 && Ofs <= 7) AddStatement(OPC_LocalValue0+Ofs, aloc);
    else AddStatement(OPC_LocalValueB, Ofs, aloc);
  } else {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_PushPointedPtr, aloc);
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalZero
//
//==========================================================================
void VEmitContext::EmitLocalZero (int locidx, const TLocation &aloc) {
  const VLocalVarDef &loc = LocalDefs[locidx];

  // don't touch out/ref parameters
  if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) return;

  if (loc.Type.Type == TYPE_Struct) {
    EmitLocalAddress(loc.Offset, aloc);
    //GLog.Logf(NAME_Debug, "ZEROSTRUCT<%s>: size=%d (%s:%d)", *loc.Type.Struct->Name, loc.Type.GetSize(), *loc.Type.GetName(), loc.Type.Type);
    // we cannot use `OPC_ZeroByPtr` here, because struct size is not calculated yet
    //AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
    AddStatement(OPC_ZeroPointedStruct, loc.Type.Struct, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Array) {
    //k8: can we use size caluclation here?
    // i don't think so, because struct size is not calculated yet (it is done in postload [why, btw?])
    // so until i fix that, clear each array element separately
    for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
      EmitLocalAddress(loc.Offset, aloc);
      EmitPushNumber(j, aloc);
      AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
      if (loc.Type.ArrayInnerType == TYPE_Struct) {
        AddStatement(OPC_ZeroPointedStruct, loc.Type.Struct, aloc);
      } else {
        AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
      }
    }
    return;
  }

  if (loc.Type.Type == TYPE_String ||
      loc.Type.Type == TYPE_Dictionary ||
      loc.Type.Type == TYPE_DynamicArray)
  {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ZeroSlotsByPtr, loc.Type.GetStackSize(), aloc);
    return;
  }

  EmitLocalAddress(loc.Offset, aloc);
  AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
}


//==========================================================================
//
//  VEmitContext::EmitLocalDtor
//
//==========================================================================
void VEmitContext::EmitLocalDtor (int locidx, const TLocation &aloc) {
  const VLocalVarDef &loc = LocalDefs[locidx];

  // don't touch out/ref parameters
  if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) return;

  if (loc.Type.Type == TYPE_String) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ClearPointedStr, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Dictionary) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_DictDispatch, loc.Type.GetDictKeyType(), loc.Type.GetDictValueType(), OPC_DictDispatch_ClearPointed, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_DynamicArray) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_PushNumber0, aloc);
    AddStatement(OPC_DynArrayDispatch, loc.Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNum, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Struct) {
    // call struct dtors
    if (loc.Type.Struct->NeedsMethodDestruction()) {
      for (VStruct *st = loc.Type.Struct; st; st = st->ParentStruct) {
        VMethod *mt = st->FindDtor(false); // non-recursive
        if (mt) {
          vassert(mt->NumParams == 0);
          // emit `self`
          if (!mt->IsStatic()) EmitLocalAddress(loc.Offset, aloc);
          // emit call
          AddStatement(OPC_Call, mt, 1/*SelfOffset*/, aloc);
        }
      }
    }
    // clear fields
    if (loc.Type.Struct->NeedsFieldsDestruction()) {
      EmitLocalAddress(loc.Offset, aloc);
      AddStatement(OPC_ClearPointedStruct, loc.Type.Struct, aloc);
    }
    return;
  }

  if (loc.Type.Type == TYPE_Array) {
    if (loc.Type.ArrayInnerType == TYPE_String) {
      for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
        EmitLocalAddress(loc.Offset, aloc);
        EmitPushNumber(j, aloc);
        AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
        AddStatement(OPC_ClearPointedStr, aloc);
      }
    } else if (loc.Type.ArrayInnerType == TYPE_Struct) {
      // call struct dtors
      if (loc.Type.Struct->NeedsMethodDestruction()) {
        for (VStruct *st = loc.Type.Struct; st; st = st->ParentStruct) {
          VMethod *mt = st->FindDtor(false); // non-recursive
          if (mt) {
            vassert(mt->NumParams == 0);
            for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
              // emit `self`
              if (!mt->IsStatic()) {
                EmitLocalAddress(loc.Offset, aloc);
                EmitPushNumber(j, aloc);
                AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
              }
              // emit call
              AddStatement(OPC_Call, mt, 1/*SelfOffset*/, aloc);
            }
          }
        }
      }
      // clear
      if (loc.Type.Struct->NeedsFieldsDestruction()) {
        for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
          EmitLocalAddress(loc.Offset, aloc);
          EmitPushNumber(j, aloc);
          AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
          AddStatement(OPC_ClearPointedStruct, loc.Type.Struct, aloc);
        }
      }
    }
    return;
  }
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
VArrayElement *VEmitContext::SetIndexArray (VArrayElement *el) {
  auto res = IndArray;
  IndArray = el;
  return res;
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
void VEmitContext::EmitGotoTo (VName lblname, const TLocation &aloc) {
  if (lblname == NAME_None) VCFatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
  for (int f = GotoLabels.length()-1; f >= 0; --f) {
    if (GotoLabels[f].name == lblname) {
      AddStatement(OPC_Goto, GotoLabels[f].jlbl, aloc);
      return;
    }
  }
  // add new goto label
  VGotoListItem it;
  it.jlbl = DefineLabel();
  it.name = lblname;
  it.loc = aloc;
  it.defined = false;
  GotoLabels.append(it);
  AddStatement(OPC_Goto, it.jlbl, aloc);
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
void VEmitContext::EmitGotoLabel (VName lblname, const TLocation &aloc) {
  if (lblname == NAME_None) VCFatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
  for (int f = GotoLabels.length()-1; f >= 0; --f) {
    if (GotoLabels[f].name == lblname) {
      if (GotoLabels[f].defined) {
        ParseError(aloc, "Duplicate label `%s` (previous is at %s:%d)", *lblname, *GotoLabels[f].loc.GetSource(), GotoLabels[f].loc.GetLine());
      } else {
        MarkLabel(GotoLabels[f].jlbl);
      }
      return;
    }
  }
  // add new goto label
  VGotoListItem it;
  it.jlbl = DefineLabel();
  it.name = lblname;
  it.loc = aloc;
  it.defined = true;
  MarkLabel(it.jlbl);
  GotoLabels.append(it);
}
