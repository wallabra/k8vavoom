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


// ////////////////////////////////////////////////////////////////////////// //
// VEmitContext::VAutoFin
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEmitContext::VAutoFin::VAutoFin
//
//==========================================================================
VEmitContext::VAutoFin::VAutoFin (VEmitContext::VFinalizer *afin) : fin(afin) {
  if (afin) afin->incRef();
}


//==========================================================================
//
//  VEmitContext::VAutoFin::~VAutoFin
//
//==========================================================================
VEmitContext::VAutoFin::~VAutoFin () {
  if (fin) { fin->decRef(); fin = nullptr; }
}


//==========================================================================
//
//  VEmitContext::VAutoFin::VAutoFin
//
//==========================================================================
VEmitContext::VAutoFin::VAutoFin (const VAutoFin &src) {
  if (src.fin) src.fin->incRef();
  fin = src.fin;
}


//==========================================================================
//
//  VEmitContext::VAutoFin::operator =
//
//==========================================================================
void VEmitContext::VAutoFin::operator = (const VAutoFin &src) {
  if (&src != this) {
    if (src.fin) src.fin->incRef();
    if (fin) fin->decRef();
    fin = src.fin;
  }
}



// ////////////////////////////////////////////////////////////////////////// //
// VEmitContext::VAutoBreakCont
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEmitContext::VAutoBreakCont::VAutoBreakCont
//
//==========================================================================
VEmitContext::VAutoBreakCont::VAutoBreakCont (VBreakCont *abc) : bc(abc) {
  if (abc) abc->incRef();
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::~VAutoBreakCont
//
//==========================================================================
VEmitContext::VAutoBreakCont::~VAutoBreakCont () {
  if (bc) { bc->decRef(); bc = nullptr; }
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::VAutoBreakCont
//
//==========================================================================
VEmitContext::VAutoBreakCont::VAutoBreakCont (const VAutoBreakCont &src) {
  if (src.bc) src.bc->incRef();
  bc = src.bc;
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::operator =
//
//==========================================================================
void VEmitContext::VAutoBreakCont::operator = (const VAutoBreakCont &src) {
  if (&src != this) {
    if (src.bc) src.bc->incRef();
    if (bc) bc->decRef();
    bc = src.bc;
  }
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::Mark
//
//  calls `MarkLabel()`
//
//==========================================================================
void VEmitContext::VAutoBreakCont::Mark () {
  if (bc) bc->ec->MarkLabel(bc->lbl);
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::GetLabelNoFinalizers
//
//  returns label, doesn't generate finalizing code
//
//==========================================================================
VLabel VEmitContext::VAutoBreakCont::GetLabelNoFinalizers () {
  return (bc ? bc->lbl : VLabel());
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::emitOurFins
//
//==========================================================================
void VEmitContext::VAutoBreakCont::emitOurFins () {
  if (bc) {
    VFinalizer *lastFin = nullptr;
    for (VFinalizer *fin = bc->ec->lastFin; fin; fin = fin->prev) {
      if (fin->bc == bc) lastFin = fin;
    }
    if (!lastFin) return;
    for (VFinalizer *fin = bc->ec->lastFin; fin; fin = fin->prev) {
      fin->emit();
      if (fin == lastFin) break;
    }
  }
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::emitFins
//
//  without ours
//
//==========================================================================
void VEmitContext::VAutoBreakCont::emitFins () {
  if (bc) {
    VFinalizer *lastFin = nullptr;
    for (VFinalizer *fin = bc->ec->lastFin; fin; fin = fin->prev) {
      if (fin->bc == bc) { lastFin = fin; break; }
    }
    if (!lastFin) return;
    for (VFinalizer *fin = bc->ec->lastFin; fin != lastFin; fin = fin->prev) {
      fin->emit();
    }
  }
}


//==========================================================================
//
//  VEmitContext::VAutoBreakCont::GetLabel
//
//  emit finalizers, so you can safely jump to the returned label
//  note that finalizers, registered with this object, will *NOT* be emited!
//
//==========================================================================
VLabel VEmitContext::VAutoBreakCont::GetLabel () {
  if (!bc) return VLabel();
  emitFins();
  return bc->lbl;
}


// ////////////////////////////////////////////////////////////////////////// //
// VEmitContext::VFinalizer
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEmitContext::VFinalizer::die
//
//==========================================================================
void VEmitContext::VFinalizer::die () {
  emit();
  ec->lastFin = prev;
  delete this;
}


//==========================================================================
//
//  VEmitContext::VFinalizer::emit
//
//==========================================================================
void VEmitContext::VFinalizer::emit () {
  if (st) st->EmitFinalizer(*ec);
}


// ////////////////////////////////////////////////////////////////////////// //
// VEmitContext::VBreakCont
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEmitContext::VBreakCont::die
//
//==========================================================================
void VEmitContext::VBreakCont::die () {
  // find last our finalizer
  VFinalizer *lastFin = nullptr;
  for (VFinalizer *fin = ec->lastFin; fin; fin = fin->prev) {
    if (fin->bc == this) lastFin = fin;
  }
  if (lastFin) {
    // emit code for all finalizers up to, and including ours
    for (VFinalizer *fin = ec->lastFin; fin; fin = fin->prev) {
      fin->emit();
      if (fin->bc == this) {
        // mark our finalizers as dead, autodtors will remove 'em
        fin->st = nullptr;
        fin->bc = nullptr;
      }
      if (fin == lastFin) break;
    }
  }
  ec->lastBC = prev;
  delete this;
}


//==========================================================================
//
//  VEmitContext::VBreakCont::emit
//
//==========================================================================
void VEmitContext::VBreakCont::emitFinalizers () {
  if (!ec) return;
  // find last our finalizer
  VFinalizer *lastFin = nullptr;
  for (VFinalizer *fin = ec->lastFin; fin; fin = fin->prev) {
    if (fin->bc == this) { lastFin = fin; break; }
  }
  if (lastFin) {
    // emit code for all finalizers up to ours
    for (VFinalizer *fin = ec->lastFin; fin != lastFin; fin = fin->prev) {
      fin->emit();
    }
  }
}



// ////////////////////////////////////////////////////////////////////////// //
// VEmitContext
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEmitContext::VEmitContext
//
//==========================================================================
VEmitContext::VEmitContext (VMemberBase *Member)
  : compIndex(0)
  , firstLoopCompIndex(-1)
  , lastFin(nullptr)
  , lastBC(nullptr)
  , CurrentFunc(nullptr)
  , IndArray(nullptr)
  , FuncRetType(TYPE_Unknown)
  , localsofs(0)
  , InDefaultProperties(false)
{
  // find the class
  VMemberBase *CM = Member;
  while (CM && CM->MemberType != MEMBER_Class) CM = CM->Outer;
  SelfClass = (VClass *)CM;

  VMemberBase *PM = Member;
  while (PM != nullptr && PM->MemberType != MEMBER_Package) PM = PM->Outer;
  Package = (VPackage *)PM;

  if (Member != nullptr && Member->MemberType == MEMBER_Method) {
    CurrentFunc = (VMethod *)Member;
    CurrentFunc->Instructions.Clear();
    //k8: nope, don't do this. many (most!) states with actions are creating anonymous functions
    //    with several statements only. this is ALOT of functions. and each get several kb of RAM.
    //    hello, OOM on any moderately sized mod. yay.
    //CurrentFunc->Instructions.Resize(1024);
    FuncRetType = CurrentFunc->ReturnType;
    /*
    if (CurrentFunc->SelfTypeName != NAME_None) {
      SelfClass = VMemberBase::StaticFindClass(CurrentFunc->SelfTypeName);
      if (!SelfClass) {
        ParseError(CurrentFunc->Loc, "No such class `%s`", *CurrentFunc->SelfTypeName);
      } else {
        if (CM) {
          VClass *cc = (VClass *)CM;
          while (cc && cc->Name != CurrentFunc->SelfTypeName) cc = cc->ParentClass;
               if (!cc) ParseError(CurrentFunc->Loc, "Forced self `%s` for class `%s`, which is not super (method `%s`)", *CurrentFunc->SelfTypeName, ((VClass *)CM)->GetName(), CurrentFunc->GetName());
          else if (CM == SelfClass) ParseWarning(CurrentFunc->Loc, "Forced self `%s` for the same class (method `%s`)", *CurrentFunc->SelfTypeName, CurrentFunc->GetName());
          else GLog.Logf("%s: forced class `%s` for class `%s` (method `%s`)", *CurrentFunc->Loc.toStringNoCol(), *CurrentFunc->SelfTypeName, ((VClass *)CM)->GetName(), CurrentFunc->GetName());
        } else {
          ParseError(CurrentFunc->Loc, "Forced self `%s` for nothing (wtf?!) (method `%s`)", *CurrentFunc->SelfTypeName, CurrentFunc->GetName());
        }
      }
    }
    */
  }
}


//==========================================================================
//
//  VEmitContext::EndCode
//
//==========================================================================
void VEmitContext::EndCode () {
  if (lastFin) FatalError("Internal compiler error: unbalanced finalizers");
  if (lastBC) FatalError("Internal compiler error: unbalanced break/cont");

  // fix-up labels.
  for (int i = 0; i < Fixups.Num(); ++i) {
    if (Labels[Fixups[i].LabelIdx] < 0) FatalError("Label was not marked");
    if (Fixups[i].Arg == 1) {
      CurrentFunc->Instructions[Fixups[i].Pos].Arg1 = Labels[Fixups[i].LabelIdx];
    } else {
      CurrentFunc->Instructions[Fixups[i].Pos].Arg2 = Labels[Fixups[i].LabelIdx];
    }
  }

#ifdef OPCODE_STATS
  for (int i = 0; i < CurrentFunc->Instructions.Num(); ++i) {
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
}


//==========================================================================
//
//  VEmitContext::AllocLocal
//
//==========================================================================
// allocates new local, sets offset
VLocalVarDef &VEmitContext::AllocLocal (VName aname, const VFieldType &atype, const TLocation &aloc) {
  int ssz = atype.GetStackSize()/4;

  // try to find reusable local
  int besthit = 0x7fffffff, bestidx = -1;
  if (!atype.IsReusingDisabled()) {
    for (int f = 0; f < LocalDefs.length(); ++f) {
      //break;
      const VLocalVarDef &ll = LocalDefs[f];
      // don't rewrite type info
      if (ll.Reusable && !ll.Visible && !ll.Type.IsReusingDisabled() && ll.Type.IsReplacableWith(atype)) {
        if (ll.stackSize >= ssz) {
          // i found her!
          if (ll.stackSize == ssz) {
            bestidx = f;
            break;
          }
          // if this is better match, use it
          int points = ll.stackSize-ssz;
          if (points < besthit) {
            besthit = points;
            bestidx = f;
          }
        }
      }
    }
  } else if (atype.Type == TYPE_String) {
    // string can be safely replaced with another string, they both require dtor
    for (int f = 0; f < LocalDefs.length(); ++f) {
      const VLocalVarDef &ll = LocalDefs[f];
      if (ll.Reusable && !ll.Visible && ll.Type.Type == TYPE_String) {
        // i found her!
        bestidx = f;
        break;
      }
    }
  }

  if (bestidx >= 0) {
    VLocalVarDef &ll = LocalDefs[bestidx];
    //fprintf(stderr, "method '%s': found reusable local '%s' at index %d (new local is '%s'); type is '%s'; new type is '%s'; oloc:%s:%d; nloc:%s:%d\n", CurrentFunc->GetName(), *ll.Name, bestidx, *aname, *ll.Type.GetName(), *atype.GetName(), *ll.Loc.GetSource(), ll.Loc.GetLine(), *aloc.GetSource(), aloc.GetLine());
    ll.Loc = aloc;
    ll.Reusable = false;
    ll.Visible = true;
    ll.Name = aname;
    ll.Type = atype;
    ll.ParamFlags = 0;
    ll.compIndex = compIndex;
    ll.reused = true;
    return ll;
  } else {
    // introduce new local
    VLocalVarDef &loc = LocalDefs.Alloc();
    loc.Loc = aloc;
    loc.Name = aname;
    loc.Type = atype;
    loc.Offset = localsofs;
    loc.Reusable = false;
    loc.Visible = true;
    loc.ParamFlags = 0;
    loc.ldindex = LocalDefs.length()-1;
    loc.compIndex = compIndex;
    loc.stackSize = ssz;
    loc.reused = false;
    localsofs += ssz;
    if (localsofs > 1024) {
      ParseError(aloc, "Local vars > 1k");
      FatalError("VC: too many locals");
    }
    return loc;
  }
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
//  VEmitContext::EnterCompound
//
//==========================================================================
int VEmitContext::EnterCompound (bool asLoop) {
  ++compIndex;
  if (asLoop && firstLoopCompIndex < 0) firstLoopCompIndex = compIndex;
  return compIndex;
}


//==========================================================================
//
//  VEmitContext::ExitCompound
//
//==========================================================================
void VEmitContext::ExitCompound (int cidx) {
  if (cidx != compIndex) Sys_Error("VC COMPILER INTERNAL ERROR: unbalanced compounds");
  if (cidx < 1) Sys_Error("VC COMPILER INTERNAL ERROR: invalid compound index");
  for (int f = 0; f < LocalDefs.length(); ++f) {
    VLocalVarDef &loc = LocalDefs[f];
    if (loc.compIndex == cidx) {
      //fprintf(stderr, "method '%s': compound #%d; freeing '%s' (%d; %s)\n", CurrentFunc->GetName(), cidx, *loc.Name, f, *loc.Type.GetName());
      loc.Visible = false;
      loc.Reusable = true;
      loc.compIndex = -1;
    }
  }
  if (firstLoopCompIndex == compIndex) firstLoopCompIndex = -1;
  --compIndex;
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
  return VLabel(Labels.Num()-1);
}


//==========================================================================
//
//  VEmitContext::MarkLabel
//
//==========================================================================
void VEmitContext::MarkLabel (VLabel l) {
  if (l.Index < 0 || l.Index >= Labels.Num()) FatalError("Bad label index %d", l.Index);
  if (Labels[l.Index] >= 0) FatalError("Label has already been marked");
  Labels[l.Index] = CurrentFunc->Instructions.Num();
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_None) FatalError("Opcode doesn't take 0 params");
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
    FatalError("Opcode doesn't take 1 param");
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
  if (StatementInfo[statement].Args != OPCARGS_Int) FatalError("Opcode does\'t take float argument");
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
  if (StatementInfo[statement].Args != OPCARGS_Name) FatalError("Opcode does\'t take name argument");
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
    FatalError("Opcode does't take member as argument");
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
    FatalError("Opcode does't take member and byte as argument");
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
    FatalError("Opcode doesn't take type as argument");
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
    FatalError("Opcode doesn't take type as argument");
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
    FatalError("Opcode doesn't take types as argument");
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
  if (StatementInfo[statement].Args != OPCARGS_BranchTarget) FatalError("Opcode does\'t take label as argument");
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = 0;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = CurrentFunc->Instructions.Num()-1;
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
    FatalError("Opcode does't take 2 params");
  }
  FInstruction &I = CurrentFunc->Instructions.Alloc();
  I.Opcode = statement;
  I.Arg1 = parm1;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = CurrentFunc->Instructions.Num()-1;
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
  //if (StatementInfo[statement].Args != OPCARGS_Builtin) FatalError("Opcode does\'t take builtin");
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
  if (lcidx < 0 || lcidx >= LocalDefs.length()) FatalError("VC: internal compiler error (VEmitContext::EmitLocalValue)");
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) FatalError("VC: internal compiler error (VEmitContext::EmitLocalValue)");
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
  if (lcidx < 0 || lcidx >= LocalDefs.length()) FatalError("VC: internal compiler error (VEmitContext::EmitLocalValue)");
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) FatalError("VC: internal compiler error (VEmitContext::EmitLocalPtrValue)");
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
//  VEmitContext::EmitLocalDtors
//
//==========================================================================
void VEmitContext::EmitLocalDtors (int Start, int End, const TLocation &aloc, bool zeroIt) {
  for (int i = Start; i < End; ++i) EmitOneLocalDtor(i, aloc, zeroIt);
}


//==========================================================================
//
//  VEmitContext::EmitLocalDtors
//
//==========================================================================
void VEmitContext::EmitOneLocalDtor (int locidx, const TLocation &aloc, bool zeroIt) {
  // don't touch out/ref parameters
  if (LocalDefs[locidx].ParamFlags&(FPARM_Out|FPARM_Ref)) return;

  if (LocalDefs[locidx].Type.Type == TYPE_String) {
    EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
    AddStatement(OPC_ClearPointedStr, aloc);
    return;
  }

  if (LocalDefs[locidx].Type.Type == TYPE_Dictionary) {
    EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
    AddStatement(OPC_DictDispatch, LocalDefs[locidx].Type.GetDictKeyType(), LocalDefs[locidx].Type.GetDictValueType(), OPC_DictDispatch_ClearPointed, aloc);
    return;
  }

  if (LocalDefs[locidx].Type.Type == TYPE_DynamicArray) {
    EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
    AddStatement(OPC_PushNumber0, aloc);
    //AddStatement(OPC_DynArraySetNum, LocalDefs[locidx].Type.GetArrayInnerType(), aloc);
    AddStatement(OPC_DynArrayDispatch, LocalDefs[locidx].Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNum, aloc);
    return;
  }

  if (LocalDefs[locidx].Type.Type == TYPE_Struct) {
    if (LocalDefs[locidx].Type.Struct->NeedsDestructor()) {
      EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
      AddStatement((!zeroIt ? OPC_ClearPointedStruct : OPC_ZeroPointedStruct), LocalDefs[locidx].Type.Struct, aloc);
    } else if (zeroIt) {
      EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
      AddStatement(OPC_ZeroByPtr, LocalDefs[locidx].Type.GetSize(), aloc);
    }
    return;
  }

  if (LocalDefs[locidx].Type.Type == TYPE_Array) {
    if (LocalDefs[locidx].Type.ArrayInnerType == TYPE_String) {
      for (int j = 0; j < LocalDefs[locidx].Type.GetArrayDim(); ++j) {
        EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
        EmitPushNumber(j, aloc);
        AddStatement(OPC_ArrayElement, LocalDefs[locidx].Type.GetArrayInnerType(), aloc);
        AddStatement(OPC_ClearPointedStr, aloc);
      }
    } else if (LocalDefs[locidx].Type.ArrayInnerType == TYPE_Struct) {
      if (LocalDefs[locidx].Type.Struct->NeedsDestructor()) {
        for (int j = 0; j < LocalDefs[locidx].Type.GetArrayDim(); ++j) {
          EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
          EmitPushNumber(j, aloc);
          AddStatement(OPC_ArrayElement, LocalDefs[locidx].Type.GetArrayInnerType(), aloc);
          AddStatement((!zeroIt ? OPC_ClearPointedStruct : OPC_ZeroPointedStruct), LocalDefs[locidx].Type.Struct, aloc);
        }
      } else {
        // just zero it
        EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
        AddStatement(OPC_ZeroByPtr, LocalDefs[locidx].Type.GetSize(), aloc);
      }
    }
    return;
  }

  if (zeroIt) {
    EmitLocalAddress(LocalDefs[locidx].Offset, aloc);
    AddStatement(OPC_ZeroByPtr, LocalDefs[locidx].Type.GetSize(), aloc);
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


  struct VGotoList {
    VLabel jlbl;
    VName name;
  };
  TArray<VGotoList> GotoLabels;

//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
void VEmitContext::EmitGotoTo (VName lblname, const TLocation &aloc) {
  if (lblname == NAME_None) FatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
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
  if (lblname == NAME_None) FatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
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


//==========================================================================
//
//  VEmitContext::RegisterFinalizer
//
//  use this to register finalizer that will be called in `return`, or
//  when `VAutoFin` object is destroyed
//
//==========================================================================
VEmitContext::VAutoFin VEmitContext::RegisterFinalizer (VStatement *st) {
  if (!st) return VAutoFin(); // this is noop anyway
  VFinalizer *fin = new VFinalizer();
  fin->rc = 0; // will be incremented in `VAutoFin()` constructor
  fin->ec = this;
  fin->prev = lastFin;
  fin->st = st;
  fin->bc = nullptr;
  lastFin = fin;
  return VAutoFin(fin); // this increments `rc`
}


//==========================================================================
//
//  VEmitContext::RegisterLoopFinalizer
//
//==========================================================================
VEmitContext::VAutoFin VEmitContext::RegisterLoopFinalizer (VStatement *st) {
  if (!st) return VAutoFin(); // this is noop anyway
  VFinalizer *fin = new VFinalizer();
  fin->rc = 0; // will be incremented in `VAutoFin()` constructor
  fin->ec = this;
  fin->prev = lastFin;
  fin->st = st;
  fin->bc = lastBC;
  lastFin = fin;
  return VAutoFin(fin); // this increments `rc`
}


//==========================================================================
//
//  VEmitContext::EmitFinalizers
//
//  emit all currently registered finalizers, from last to first
//
//==========================================================================
void VEmitContext::EmitFinalizers () {
  for (VFinalizer *fin = lastFin; fin; fin = fin->prev) fin->emit();
}


//==========================================================================
//
//  VEmitContext::DefineBreakCont
//
//==========================================================================
VEmitContext::VAutoBreakCont VEmitContext::DefineBreakCont (VEmitContext::BCType atype) {
  VBreakCont *bc = new VBreakCont();
  bc->rc = 0; // will be incremented in `VAutoBreakCont()` constructor
  bc->ec = this;
  bc->prev = lastBC;
  bc->lbl = DefineLabel();
  bc->type = atype;
  lastBC = bc;
  return VAutoBreakCont(bc); // this increments `rc`
}


//==========================================================================
//
//  VEmitContext::DefineBreak
//
//==========================================================================
VEmitContext::VAutoBreakCont VEmitContext::DefineBreak () {
  return DefineBreakCont(BCType::Break);
}


//==========================================================================
//
//  VEmitContext::DefineContinue
//
//==========================================================================
VEmitContext::VAutoBreakCont VEmitContext::DefineContinue () {
  return DefineBreakCont(BCType::Continue);
}


//==========================================================================
//
//  VEmitContext::BlockBreakContReturn
//
//==========================================================================
VEmitContext::VAutoBreakCont VEmitContext::BlockBreakContReturn () {
  return DefineBreakCont(BCType::Block);
}


//==========================================================================
//
//  VEmitContext::EmitBreak
//
//  returns success flag
//
//==========================================================================
bool VEmitContext::EmitBreak (const TLocation &loc) {
  for (VBreakCont *bc = lastBC; bc; bc = bc->prev) {
    switch (bc->type) {
      case BCType::Break:
        bc->emitFinalizers();
        AddStatement(OPC_Goto, bc->lbl, loc);
        return true; // done, no need to emit finalizers
      case BCType::Continue:
        break;
      case BCType::Block:
        return false;
    }
  }
  return false; // oops
}


//==========================================================================
//
//  VEmitContext::EmitContinue
//
//  returns success flag
//
//==========================================================================
bool VEmitContext::EmitContinue (const TLocation &loc) {
  for (VBreakCont *bc = lastBC; bc; bc = bc->prev) {
    switch (bc->type) {
      case BCType::Break:
        break;
      case BCType::Continue:
        bc->emitFinalizers();
        AddStatement(OPC_Goto, bc->lbl, loc);
        return true; // done, no need to emit finalizers
      case BCType::Block:
        return false;
    }
  }
  return false; // oops
}


//==========================================================================
//
//  VEmitContext::IsReturnAllowed
//
//==========================================================================
bool VEmitContext::IsReturnAllowed () {
  for (VBreakCont *bc = lastBC; bc; bc = bc->prev) {
    if (bc->type == BCType::Block) return false;
  }
  return true;
}
