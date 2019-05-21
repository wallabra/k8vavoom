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
#ifndef IN_VCC
//# define VCC_DEBUG_COMPILER_LEAKS
//# define VCC_DEBUG_COMPILER_LEAKS_CHECKS
#endif

#include "vc_local.h"


#ifdef VCC_DEBUG_COMPILER_LEAKS
struct MemInfo {
  void *ptr;
  size_t size;
  MemInfo *prev, *next;
};

static MemInfo *allocInfoHead = nullptr;
static MemInfo *allocInfoTail = nullptr;


static void *AllocMem (size_t sz) {
  MemInfo *mi = (MemInfo *)malloc(sz+sizeof(MemInfo));
  mi->ptr = (vuint8 *)(mi+1)+sizeof(size_t);
  mi->size = sz-sizeof(size_t);
  mi->prev = allocInfoTail;
  mi->next = nullptr;
  if (allocInfoTail) allocInfoTail->next = mi; else allocInfoHead = mi;
  allocInfoTail = mi;
  return mi+1;
}


static void FreeMem (void *p) {
  if (!p) return;
  MemInfo *mi = ((MemInfo *)p)-1;
  if (mi->prev) mi->prev->next = mi->next; else allocInfoHead = mi->next;
  if (mi->next) mi->next->prev = mi->prev; else allocInfoTail = mi->prev;
  free(mi);
}


#if defined(VCC_DEBUG_COMPILER_LEAKS_CHECKS)
static size_t CalcMem () {
  size_t res = 0;
  for (MemInfo *mi = allocInfoHead; mi; mi = mi->next) res += mi->size;
  return res;
}


static void DumpAllocs () {
  fprintf(stderr, "=== ALLOCS ===\n");
  for (MemInfo *mi = allocInfoHead; mi; mi = mi->next) {
    fprintf(stderr, "address: %p; size: %u\n", mi->ptr, (unsigned)mi->size);
  }
  fprintf(stderr, "---\n");
}
#endif


void VExpression::ReportLeaks () {
  GLog.WriteLine("================================= LEAKS =================================");
  for (MemInfo *mi = allocInfoHead; mi; mi = mi->next) {
    GLog.WriteLine("address: %p", mi->ptr);
    auto e = (VExpression *)mi->ptr;
    GLog.WriteLine("  type: %s (loc: %s:%d)", *shitppTypeNameObj(*e), *e->Loc.GetSource(), e->Loc.GetLine());
  }
  GLog.WriteLine("---");
}


#else
# define AllocMem  malloc
# define FreeMem   free
void VExpression::ReportLeaks () {
}
#endif


//==========================================================================
//
//  VExpression::VExpression
//
//==========================================================================
VExpression::VExpression (const TLocation &ALoc)
  : Type(TYPE_Void)
  , RealType(TYPE_Void)
  , Flags(0)
  , Loc(ALoc)
{
}


//==========================================================================
//
//  VExpression::~VExpression
//
//==========================================================================
VExpression::~VExpression () {
}


//==========================================================================
//
//  VExpression::DoRestSyntaxCopyTo
//
//==========================================================================
//#include <typeinfo>
void VExpression::DoSyntaxCopyTo (VExpression *e) {
  //fprintf(stderr, "  ***VExpression::DoSyntaxCopyTo for `%s`\n", typeid(*e).name());
  e->Type = Type;
  e->RealType = RealType;
  e->Flags = Flags;
  e->Loc = Loc;
}


//==========================================================================
//
//  VExpression::Resolve
//
//==========================================================================
VExpression *VExpression::Resolve (VEmitContext &ec) {
  VExpression *e = DoResolve(ec);
  return e;
}


//==========================================================================
//
//  VExpression::ResolveBoolean
//
//==========================================================================
VExpression *VExpression::ResolveBoolean (VEmitContext &ec) {
  VExpression *e = Resolve(ec);
  if (!e) return nullptr;
  return e->CoerceToBool(ec);
}


//==========================================================================
//
//  VExpression::CoerceToBool
//
//==========================================================================
VExpression *VExpression::CoerceToBool (VEmitContext &ec) {
  switch (Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      break;
    case TYPE_Float:
      return (new VFloatToBool(this, true))->Resolve(ec);
    case TYPE_Name:
      return (new VNameToBool(this, true))->Resolve(ec);
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      return (new VPointerToBool(this, true))->Resolve(ec);
    case TYPE_String:
      return (new VStringToBool(this, true))->Resolve(ec);
      break;
    case TYPE_Delegate:
      return (new VDelegateToBool(this, true))->Resolve(ec);
      break;
    case TYPE_Vector:
      return (new VVectorToBool(this, true))->Resolve(ec);
    default:
      ParseError(Loc, "Expression type mismatch, boolean expression expected, got `%s`", *Type.GetName());
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VExpression::ResolveFloat
//
//==========================================================================
VExpression *VExpression::ResolveFloat (VEmitContext &ec) {
  VExpression *e = Resolve(ec);
  if (!e) return nullptr;
  switch (e->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    //case TYPE_Bool:
      e = (new VScalarToFloat(e, true))->Resolve(ec);
      break;
    case TYPE_Float:
      break;
    default:
      ParseError(e->Loc, "Expression type mismatch, float expression expected");
      delete e;
      return nullptr;
  }
  return e;
}


//==========================================================================
//
//  VExpression::CoerceToFloat
//
//  Expression MUST be already resolved here.
//
//==========================================================================
VExpression *VExpression::CoerceToFloat (VEmitContext &ec) {
  if (Type.Type == TYPE_Float) return this; // nothing to do
  if (Type.Type == TYPE_Int || Type.Type == TYPE_Byte) {
    if (IsIntConst()) {
      VExpression *e = new VFloatLiteral((float)GetIntConst(), Loc);
      delete this;
      return e->Resolve(ec);
    }
    return (new VScalarToFloat(this, true))->Resolve(ec);
  }
  ParseError(Loc, "Expression type mismatch, float expression expected");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  workerCoerceOp1None
//
//==========================================================================
static bool workerCoerceOp1None (VEmitContext &ec, VExpression *&op1, VExpression *&op2, bool coerceNoneDelegate) {
  if (!op1 || !op2) return false;
  if (op1->IsNoneLiteral() && !op2->IsNoneLiteral() && !op2->IsNullLiteral()) {
    switch (op2->Type.Type) {
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        op1->Type = op2->Type;
        return true;
      //k8: delegate coercing requires turning `none` to two different types:
      //    `none class/object` and `none delegate`
      case TYPE_Delegate:
        if (coerceNoneDelegate) {
          VNoneDelegateLiteral *nl = new VNoneDelegateLiteral(op1->Loc);
          delete op1;
          op1 = nl->Resolve(ec); //???
          if (!op1) return false;
        }
        return true;
    }
    return true;
  }
  return false;
}


//==========================================================================
//
//  workerCoerceOp1Null
//
//==========================================================================
static bool workerCoerceOp1Null (VEmitContext &ec, VExpression *&op1, VExpression *&op2) {
  if (!op1 || !op2) return false;
  if (op1->IsNullLiteral() && !op2->IsNoneLiteral() && !op2->IsNullLiteral()) {
    switch (op2->Type.Type) {
      case TYPE_Pointer:
        op1->Type = op2->Type;
        return true;
    }
    return true;
  }
  return false;
}


//==========================================================================
//
//  VExpression::CoerceTypes
//
//  this coerces ints to floats, and fixes `none`/`nullptr` type
//
//==========================================================================
void VExpression::CoerceTypes (VEmitContext &ec, VExpression *&op1, VExpression *&op2, bool coerceNoneDelegate) {
  if (!op1 || !op2) return; // oops
  // if one operand is vector, and other operand is integer, coerce integer to float
  // this is required for float vs scalar operators
  if (op1->Type.Type == TYPE_Vector && (op2->Type.Type == TYPE_Int || op2->Type.Type == TYPE_Byte)) {
    op2 = op2->CoerceToFloat(ec);
    return;
  }
  if (op2->Type.Type == TYPE_Vector && (op1->Type.Type == TYPE_Int || op1->Type.Type == TYPE_Byte)) {
    op1 = op1->CoerceToFloat(ec);
    return;
  }
  // coerce to float
  if ((op1->Type.Type == TYPE_Float || op2->Type.Type == TYPE_Float) &&
      (op1->Type.Type == TYPE_Int || op2->Type.Type == TYPE_Int ||
       op1->Type.Type == TYPE_Byte || op2->Type.Type == TYPE_Byte))
  {
    op1 = op1->CoerceToFloat(ec);
    op2 = op2->CoerceToFloat(ec);
    return;
  }
  // coerce `none`
  if (workerCoerceOp1None(ec, op1, op2, coerceNoneDelegate)) return;
  if (workerCoerceOp1None(ec, op2, op1, coerceNoneDelegate)) return;
  // coerce `nullptr`
  if (workerCoerceOp1Null(ec, op1, op2)) return;
  if (workerCoerceOp1Null(ec, op2, op1)) return;
}


//==========================================================================
//
//  VExpression::ResolveAsType
//
//==========================================================================
VTypeExpr *VExpression::ResolveAsType (VEmitContext &) {
  ParseError(Loc, "Invalid type expression");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VExpression::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VExpression::ResolveAssignmentTarget (VEmitContext &ec) {
  return Resolve(ec);
}


//==========================================================================
//
//  VExpression::ResolveAssignmentValue
//
//==========================================================================
VExpression *VExpression::ResolveAssignmentValue (VEmitContext &ec) {
  return Resolve(ec);
}


//==========================================================================
//
//  VExpression::ResolveIterator
//
//==========================================================================
VExpression *VExpression::ResolveIterator (VEmitContext &) {
  ParseError(Loc, "Iterator method expected");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VExpression::ResolveCompleteAssign
//
//==========================================================================
VExpression *VExpression::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  // do nothing
  return this;
}


//==========================================================================
//
//  VExpression::RequestAddressOf
//
//==========================================================================
void VExpression::RequestAddressOf () {
  ParseError(Loc, "Bad address operation");
}


//==========================================================================
//
//  VExpression::EmitBranchable
//
//==========================================================================
void VExpression::EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue) {
  Emit(ec);
  if (OnTrue) {
    ec.AddStatement(OPC_IfGoto, Lbl, Loc);
  } else {
    ec.AddStatement(OPC_IfNotGoto, Lbl, Loc);
  }
}


//==========================================================================
//
//  VExpression::EmitPushPointedCode
//
//==========================================================================
void VExpression::EmitPushPointedCode (VFieldType type, VEmitContext &ec) {
  ec.EmitPushPointedCode(type, Loc);
}


//==========================================================================
//
//  VExpression::ResolveToIntLiteralEx
//
//  this resolves one-char strings and names to int literals too
//
//==========================================================================
VExpression *VExpression::ResolveToIntLiteralEx (VEmitContext &ec, bool allowFloatTrunc) {
  VExpression *res = Resolve(ec);
  if (!res) return nullptr; // we are dead anyway

  // easy case
  if (res->IsIntConst()) return res;

  // truncate floats?
  if (allowFloatTrunc && res->IsFloatConst()) {
    VExpression *e = new VIntLiteral((int)res->GetFloatConst(), res->Loc);
    delete res;
    return e->Resolve(ec);
  }

  // one-char string?
  if (res->IsStrConst()) {
    const VStr &s = res->GetStrConst(ec.Package);
    if (s.length() == 1) {
      VExpression *e = new VIntLiteral((vuint8)s[0], res->Loc);
      delete res;
      return e->Resolve(ec);
    }
  }

  // one-char name?
  if (res->IsNameConst()) {
    const char *s = *res->GetNameConst();
    if (s && s[0] && !s[1]) {
      VExpression *e = new VIntLiteral((vuint8)s[0], res->Loc);
      delete res;
      return e->Resolve(ec);
    }
  }

  ParseError(res->Loc, "Integer constant expected");
  delete res;
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
// IsXXX
VExpression *VExpression::AddDropResult () { return nullptr; } // not processed
bool VExpression::IsValidTypeExpression () const { return false; }
bool VExpression::IsIntConst () const { return false; }
bool VExpression::IsFloatConst () const { return false; }
bool VExpression::IsStrConst () const { return false; }
bool VExpression::IsNameConst () const { return false; }
vint32 VExpression::GetIntConst () const { ParseError(Loc, "Integer constant expected"); return 0; }
float VExpression::GetFloatConst () const { ParseError(Loc, "Float constant expected"); return 0.0f; }
const VStr &VExpression::GetStrConst (VPackage *) const { ParseError(Loc, "String constant expected"); return VStr::EmptyString; }
VName VExpression::GetNameConst () const { ParseError(Loc, "Name constant expected"); return NAME_None; }
bool VExpression::IsNoneLiteral () const { return false; }
bool VExpression::IsNoneDelegateLiteral () const { return false; }
bool VExpression::IsNullLiteral () const { return false; }
bool VExpression::IsDefaultObject () const { return false; }
bool VExpression::IsPropertyAssign () const { return false; }
bool VExpression::IsDynArraySetNum () const { return false; }
bool VExpression::IsDecorateSingleName () const { return false; }
bool VExpression::IsDecorateUserVar () const { return false; }
bool VExpression::IsLocalVarDecl () const { return false; }
bool VExpression::IsLocalVarExpr () const { return false; }
bool VExpression::IsAssignExpr () const { return false; }
bool VExpression::IsParens () const { return false; }
bool VExpression::IsUnaryMath () const { return false; }
bool VExpression::IsUnaryMutator () const { return false; }
bool VExpression::IsBinaryMath () const { return false; }
bool VExpression::IsBinaryLogical () const { return false; }
bool VExpression::IsSingleName () const { return false; }
bool VExpression::IsDoubleName () const { return false; }
bool VExpression::IsDotField () const { return false; }
bool VExpression::IsMarshallArg () const { return false; }
bool VExpression::IsRefArg () const { return false; }
bool VExpression::IsOutArg () const { return false; }
bool VExpression::IsOptMarshallArg () const { return false; }
bool VExpression::IsDefaultArg () const { return false; }
bool VExpression::IsNamedArg () const { return false; }
VName VExpression::GetArgName () const { return NAME_None; }
bool VExpression::IsAnyInvocation () const { return false; }
bool VExpression::IsLLInvocation () const { return false; }
bool VExpression::IsTypeExpr () const { return false; }
bool VExpression::IsAutoTypeExpr () const { return false; }
bool VExpression::IsSimpleType () const { return false; }
bool VExpression::IsReferenceType () const { return false; }
bool VExpression::IsClassType () const { return false; }
bool VExpression::IsPointerType () const { return false; }
bool VExpression::IsAnyArrayType () const { return false; }
bool VExpression::IsDictType () const { return false; }
bool VExpression::IsStaticArrayType () const { return false; }
bool VExpression::IsDynamicArrayType () const { return false; }
bool VExpression::IsDelegateType () const { return false; }
bool VExpression::IsSliceType () const { return false; }
bool VExpression::IsVectorCtor () const { return false; }
bool VExpression::IsConstVectorCtor () const { return false; }

VStr VExpression::toString () const { return VStr("<VExpression::")+shitppTypeNameObj(*this)+":no-toString>"; }


// ////////////////////////////////////////////////////////////////////////// //
// memory allocation
vuint32 VExpression::TotalMemoryUsed = 0;
vuint32 VExpression::CurrMemoryUsed = 0;
vuint32 VExpression::PeakMemoryUsed = 0;
vuint32 VExpression::TotalMemoryFreed = 0;
bool VExpression::InCompilerCleanup = false;


void *VExpression::operator new (size_t size) {
  size_t *res = (size_t *)AllocMem(size+sizeof(size_t));
  if (!res) Sys_Error("OUT OF MEMORY!");
  *res = size;
  ++res;
  if (size) memset(res, 0, size);
  TotalMemoryUsed += (vuint32)size;
  CurrMemoryUsed += (vuint32)size;
  if (PeakMemoryUsed < CurrMemoryUsed) PeakMemoryUsed = CurrMemoryUsed;
  //fprintf(stderr, "* new: %u (%p)\n", (unsigned)size, res);
#if defined(VCC_DEBUG_COMPILER_LEAKS) && defined(VCC_DEBUG_COMPILER_LEAKS_CHECKS)
  if (CalcMem() != CurrMemoryUsed) {
    fprintf(stderr, "NEW CALC: %u\nNEW CURR: %u\n", (unsigned)CalcMem(), (unsigned)CurrMemoryUsed);
    DumpAllocs();
    abort();
  }
#endif
  return res;
}


void *VExpression::operator new[] (size_t size) {
  size_t *res = (size_t *)AllocMem(size+sizeof(size_t));
  if (!res) Sys_Error("OUT OF MEMORY!");
  *res = size;
  ++res;
  if (size) memset(res, 0, size);
  TotalMemoryUsed += (vuint32)size;
  CurrMemoryUsed += (vuint32)size;
  if (PeakMemoryUsed < CurrMemoryUsed) PeakMemoryUsed = CurrMemoryUsed;
  //fprintf(stderr, "* new[]: %u (%p)\n", (unsigned)size, res);
#if defined(VCC_DEBUG_COMPILER_LEAKS) && defined(VCC_DEBUG_COMPILER_LEAKS_CHECKS)
  if (CalcMem() != CurrMemoryUsed) {
    fprintf(stderr, "NEW[] CALC: %u\nNEW[] CURR: %u\n", (unsigned)CalcMem(), (unsigned)CurrMemoryUsed);
    DumpAllocs();
    abort();
  }
#endif
  return res;
}


void VExpression::operator delete (void *p) {
  if (p) {
    //fprintf(stderr, "* del: %u (%p)\n", (vuint32)*((size_t *)p-1), p);
    if (InCompilerCleanup) TotalMemoryFreed += (vuint32)*((size_t *)p-1);
    CurrMemoryUsed -= (vuint32)*((size_t *)p-1);
    FreeMem(((size_t *)p-1));
#if defined(VCC_DEBUG_COMPILER_LEAKS) && defined(VCC_DEBUG_COMPILER_LEAKS_CHECKS)
    if (CalcMem() != CurrMemoryUsed) {
      fprintf(stderr, "DEL CALC: %u\nDEL CURR: %u\n", (unsigned)CalcMem(), (unsigned)CurrMemoryUsed);
      DumpAllocs();
      abort();
    }
#endif
  }
}


void VExpression::operator delete[] (void *p) {
  if (p) {
    //fprintf(stderr, "* del[]: %u (%p)\n", (vuint32)*((size_t *)p-1), p);
    if (InCompilerCleanup) TotalMemoryFreed += (vuint32)*((size_t *)p-1);
    CurrMemoryUsed -= (vuint32)*((size_t *)p-1);
    FreeMem(((size_t *)p-1));
#if defined(VCC_DEBUG_COMPILER_LEAKS) && defined(VCC_DEBUG_COMPILER_LEAKS_CHECKS)
    if (CalcMem() != CurrMemoryUsed) {
      fprintf(stderr, "DEL[] CALC: %u\nDEL[] CURR: %u\n", (unsigned)CalcMem(), (unsigned)CurrMemoryUsed);
      DumpAllocs();
      abort();
    }
#endif
  }
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VInvocation::MassageDecorateArg
//
//  this will try to coerce some decorate argument to something sensible
//
//==========================================================================
VExpression *VExpression::MassageDecorateArg (VEmitContext &ec, VState *CallerState, const char *funcName,
                                              int argnum, const VFieldType &destType, const TLocation *aloc) {
  //FIXME: move this to separate method
  // simplify a little:
  //   replace `+number` with `number`
  if (IsUnaryMath()) {
    VUnary *un = (VUnary *)this;
    if (un->op) {
      if (un->Oper == VUnary::Plus && (un->op->IsIntConst() || un->op->IsFloatConst())) {
        VExpression *enew = un->op;
        //fprintf(stderr, "SIMPLIFIED! <%s> -> <%s>\n", *un->toString(), *etmp->toString());
        un->op = nullptr;
        delete this;
        return enew->MassageDecorateArg(ec, CallerState, funcName, argnum, destType, aloc);
      }
    }
  }

  switch (destType.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Float:
    case TYPE_Bool:
      if (IsStrConst()) {
        const VStr &str = GetStrConst(ec.Package);
        if (str.length() == 0 || str.ICmp("none") == 0 || str.ICmp("null") == 0 || str.ICmp("nil") == 0 || str.ICmp("false") == 0) {
          ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be number (replaced with 1); PLEASE, FIX THE CODE!", funcName, argnum);
          VExpression *enew = new VIntLiteral(0, Loc);
          delete this;
          return enew;
        }
        if (str.ICmp("true") == 0) {
          ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be number (replaced with 0); PLEASE, FIX THE CODE!", funcName, argnum);
          VExpression *enew = new VIntLiteral(1, Loc);
          delete this;
          return enew;
        }
      }
      break;

    case TYPE_Name:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VNameLiteral(*e->Name, Loc);
        delete this;
        return enew;
      }
      // string?
      if (IsStrConst()) {
        const VStr &val = GetStrConst(ec.Package);
        VExpression *enew = new VNameLiteral(*val, Loc);
        delete this;
        return enew;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be string (replaced `0` with empty string); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VNameLiteral(NAME_None, Loc);
        delete this;
        return enew;
      }
      break;

    case TYPE_String:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VStringLiteral(VStr(e->Name), ec.Package->FindString(*e->Name), Loc);
        delete this;
        return enew;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be string (replaced `0` with empty string); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VStringLiteral(VStr(), ec.Package->FindString(""), Loc);
        delete this;
        return enew;
      }
      break;

    case TYPE_Class:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VStringLiteral(VStr(e->Name), ec.Package->FindString(*e->Name), Loc);
        delete this;
        return enew->MassageDecorateArg(ec, CallerState, funcName, argnum, destType, aloc);
      }
      // string?
      if (IsStrConst()) {
        const VStr &CName = GetStrConst(ec.Package);
        //TLocation ALoc = Loc;
        if (CName.length() == 0 || CName.ICmp("None") == 0 || CName.ICmp("nil") == 0 || CName.ICmp("null") == 0) {
          //ParseWarning(ALoc, "NONE CLASS `%s`", CName);
          VExpression *enew = new VNoneLiteral(Loc);
          delete this;
          return enew;
        } else {
          VClass *Cls = VClass::FindClassNoCase(*CName);
          if (!Cls) {
            ParseWarning((aloc ? *aloc : Loc), "No such class `%s` for argument #%d of `%s`", *CName, argnum, funcName);
            VExpression *enew = new VNoneLiteral(Loc);
            delete this;
            return enew;
          }
          if (destType.Class && !Cls->IsChildOf(destType.Class)) {
            ParseWarning((aloc ? *aloc : Loc), "Class `%s` is not a descendant of `%s` for argument #%d of `%s`", *CName, destType.Class->GetName(), argnum, funcName);
            VExpression *enew = new VNoneLiteral(Loc);
            delete this;
            return enew;
          }
          VExpression *enew = new VClassConstant(Cls, Loc);
          delete this;
          return enew;
        }
        break;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be class (replaced with `none`); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VNoneLiteral(Loc);
        delete this;
        return enew;
      }
      break;

    case TYPE_State:
      // some very bright persons does this: `A_JumpIfTargetInLOS("1")` -- brilliant!
      // string?
      if (IsStrConst()) {
        const VStr &str = GetStrConst(ec.Package);
        int lbl = -1;
        if (str.convertInt(&lbl)) {
          //TLocation ALoc = Args[i]->Loc;
          if (lbl < 0) {
            ParseError((aloc ? *aloc : Loc), "`%s` argument #%d is something fucked: '%s'", funcName, argnum, *str);
            delete this;
            return nullptr;
          }
          ParseWarning((aloc ? *aloc : Loc), "`%s` argument #%d should be number %d; PLEASE, FIX THE CODE!", funcName, argnum, lbl);
          VExpression *enew = new VIntLiteral(lbl, Loc);
          delete this;
          return enew->MassageDecorateArg(ec, CallerState, funcName, argnum, destType, aloc);
        }
      }
      // integer?
      if (IsIntConst()) {
        int Offs = GetIntConst();
        //TLocation ALoc = Args[i]->Loc;
        if (Offs < 0) {
          ParseError((aloc ? *aloc : Loc), "Negative state jumps are not allowed");
          delete this;
          return nullptr;
        }
        if (Offs == 0) {
          // 0 means no state
          VExpression *enew = new VNoneLiteral(Loc);
          delete this;
          return enew;
        }
        // positive jump
        check(CallerState);
        VState *S = CallerState->GetPlus(Offs, true);
        if (!S) {
          ParseError((aloc ? *aloc : Loc), "Bad state jump offset");
          delete this;
          return nullptr;
        }
        VExpression *enew = new VStateConstant(S, Loc);
        delete this;
        return enew;
      }
      // string?
      if (IsStrConst()) {
        VStr Lbl = GetStrConst(ec.Package);
        //TLocation ALoc = Args[i]->Loc;
        int DCol = Lbl.IndexOf("::");
        if (DCol >= 0) {
          // jump to a specific parent class state, resolve it and pass value directly
          VStr ClassName(Lbl, 0, DCol);
          VClass *CheckClass;
          if (ClassName.ICmp("Super") == 0) {
            CheckClass = ec.SelfClass->ParentClass;
            if (!CheckClass) {
              ParseError((aloc ? *aloc : Loc), "`%s` argument #%d wants `Super` without superclass!", funcName, argnum);
              delete this;
              return nullptr;
            }
          } else {
            CheckClass = VClass::FindClassNoCase(*ClassName);
            if (!CheckClass) {
              ParseError((aloc ? *aloc : Loc), "No such class `%s`", *ClassName);
              delete this;
              return nullptr;
            }
            if (!ec.SelfClass->IsChildOf(CheckClass)) {
              ParseError((aloc ? *aloc : Loc), "`%s` is not a subclass of `%s`", ec.SelfClass->GetName(), CheckClass->GetName());
              delete this;
              return nullptr;
            }
          }
          check(CheckClass);
          VStr LblName(Lbl, DCol+2, Lbl.Length()-DCol-2);
          TArray<VName> Names;
          VMemberBase::StaticSplitStateLabel(LblName, Names);
          VStateLabel *StLbl = CheckClass->FindStateLabel(Names, true);
          if (!StLbl) {
            ParseError((aloc ? *aloc : Loc), "No such state '%s' in class '%s'", *Lbl, CheckClass->GetName());
            delete this;
            return nullptr;
          }
          VExpression *enew = new VStateConstant(StLbl->State, Loc);
          delete this;
          return enew;
        }
        // it's a virtual state jump
        //ParseWarning(Args[i]->Loc, "***VSJMP `%s`: <%s>", Func->GetName(), *Lbl);
        VExpression *TmpArgs[1];
        TmpArgs[0] = this;
        return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpState"), nullptr, false, false, Loc, 1, TmpArgs);
      }
      break;
  }
  return this;
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VExpression::IsNumericLiteralExpr
//
//  checks if expression consists of only numeric literals
//
//==========================================================================
int VExpression::IsNumericLiteralExpr (VExpression *e) {
  if (!e) return ExprNotNum;
  if (e->IsIntConst()) return ExprInt;
  if (e->IsFloatConst()) return ExprFloat;
  // parentheses
  if (e->IsParens()) {
    VExprParens *ep = (VExprParens *)e;
    return IsNumericLiteralExpr(ep->op);
  }
  // unary math
  if (e->IsUnaryMath()) {
    VUnary *eu = (VUnary *)e;
    switch (eu->Oper) {
      case VUnary::Plus:
      case VUnary::Minus:
        return IsNumericLiteralExpr(eu->op);
      case VUnary::Not:
        return ExprInt;
      case VUnary::BitInvert:
        if (IsNumericLiteralExpr(eu->op) != ExprInt) return ExprNotNum; // invalid type
        return ExprInt;
      case VUnary::TakeAddress:
        return ExprNotNum;
      default: break;
    }
    Sys_Error("ketmar forgot to process some unary operator in `VExpression::IsNumericLiteralExpr()`");
  }
  // binary math
  if (e->IsBinaryMath()) {
    int t0, t1;
    VBinary *eb = (VBinary *)e;
    switch (eb->Oper) {
      case VBinary::Add:
      case VBinary::Subtract:
      case VBinary::Multiply:
      case VBinary::Divide:
        t0 = IsNumericLiteralExpr(eb->op1);
        if (t0 == ExprNotNum) return ExprNotNum;
        t1 = IsNumericLiteralExpr(eb->op2);
        if (t1 == ExprNotNum) return ExprNotNum;
        if (t0 == ExprFloat || t1 == ExprFloat) return ExprFloat;
        check(t0 == ExprInt);
        check(t1 == ExprInt);
        return ExprInt;
      case VBinary::Equals:
      case VBinary::NotEquals:
      case VBinary::Less:
      case VBinary::LessEquals:
      case VBinary::Greater:
      case VBinary::GreaterEquals:
        if (IsNumericLiteralExpr(eb->op1) == ExprNotNum) return ExprNotNum;
        if (IsNumericLiteralExpr(eb->op2) == ExprNotNum) return ExprNotNum;
        return ExprInt;
      case VBinary::Modulus:
      case VBinary::LShift:
      case VBinary::RShift:
      case VBinary::URShift:
      case VBinary::And:
      case VBinary::XOr:
      case VBinary::Or:
        if (IsNumericLiteralExpr(eb->op1) != ExprInt) return ExprNotNum;
        if (IsNumericLiteralExpr(eb->op2) != ExprInt) return ExprNotNum;
        return ExprInt;
      case VBinary::StrCat:
      case VBinary::IsA:
      case VBinary::NotIsA:
        return ExprNotNum;
      default: break;
    }
    Sys_Error("ketmar forgot to process some binary operator in `VExpression::IsNumericLiteralExpr()`");
  }
  return ExprNotNum;
}


struct LEVal {
  vint32 iv;
  float fv;
  bool isFloat;

  LEVal () : iv(0), fv(0), isFloat(false) {}
  LEVal (vint32 v) : iv(v), fv(0), isFloat(false) {}
  LEVal (float v) : iv(0), fv(v), isFloat(true) {}

  static inline LEVal Invalid () { LEVal res; res.fv = NAN; res.isFloat = true; return res; }

  inline bool isValid () { return !(isFloat && isFiniteF(fv)); }

  inline LEVal toFloat () const { if (isFloat) return LEVal(fv); return LEVal((float)iv); }
};


//==========================================================================
//
//  calcLE
//
//  sanity checks are already done
//
//==========================================================================
static LEVal calcLE (VExpression *e) {
  check(e);
  if (e->IsIntConst()) return LEVal(e->GetIntConst());
  if (e->IsFloatConst()) return LEVal(e->GetFloatConst());
  // parentheses
  if (e->IsParens()) {
    VExprParens *ep = (VExprParens *)e;
    return calcLE(ep->op);
  }
  // unary math
  if (e->IsUnaryMath()) {
    VUnary *eu = (VUnary *)e;
    LEVal val = calcLE(eu->op);
    if (!val.isValid()) return val;
    switch (eu->Oper) {
      case VUnary::Plus:
        return val;
      case VUnary::Minus:
        return LEVal(-val.fv);
      case VUnary::Not:
        return LEVal(val.fv == 0 ? (vint32)1 : (vint32)0);
      case VUnary::BitInvert:
        if (val.isFloat) return LEVal::Invalid();
        return LEVal((vint32)(~val.iv));
      case VUnary::TakeAddress:
        return LEVal::Invalid();
      default: break;
    }
    Sys_Error("ketmar forgot to process some unary operator in `VExpression::calcLE()`");
  }
  // binary math
  if (e->IsBinaryMath()) {
    VBinary *eb = (VBinary *)e;
    LEVal val1 = calcLE(eb->op1);
    if (!val1.isValid()) return val1;
    LEVal val2 = calcLE(eb->op2);
    if (!val2.isValid()) return val2;
    if (val1.isFloat) {
      if (!val2.isFloat) val2 = val2.toFloat();
    } else if (val2.isFloat) {
      val1 = val1.toFloat();
    }
    check(val1.isFloat == val2.isFloat);
    switch (eb->Oper) {
      case VBinary::Add:
        return (val1.isFloat ? LEVal(val1.fv+val2.fv) : LEVal((vint32)(val1.iv+val2.iv)));
      case VBinary::Subtract:
        return (val1.isFloat ? LEVal(val1.fv-val2.fv) : LEVal((vint32)(val1.iv-val2.iv)));
      case VBinary::Multiply:
        return (val1.isFloat ? LEVal(val1.fv*val2.fv) : LEVal((vint32)(val1.iv*val2.iv)));
      case VBinary::Divide:
        if (val1.isFloat) return LEVal(val1.fv/val2.fv);
        if (val2.iv == 0) return LEVal::Invalid();
        return LEVal((vint32)(val1.iv/val2.iv));
      case VBinary::Modulus:
        if (val1.isFloat) return LEVal::Invalid();
        if (val2.iv == 0) return LEVal::Invalid();
        return LEVal((vint32)(val1.iv%val2.iv));
      case VBinary::Equals:
        if (val1.isFloat) return LEVal(val1.fv == val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv == val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::NotEquals:
        if (val1.isFloat) return LEVal(val1.fv != val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv != val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::Less:
        if (val1.isFloat) return LEVal(val1.fv < val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv < val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::LessEquals:
        if (val1.isFloat) return LEVal(val1.fv <= val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv <= val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::Greater:
        if (val1.isFloat) return LEVal(val1.fv > val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv > val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::GreaterEquals:
        if (val1.isFloat) return LEVal(val1.fv >= val2.fv ? (vint32)1 : (vint32)0);
        return LEVal(val1.iv >= val2.iv ? (vint32)1 : (vint32)0);
      case VBinary::LShift:
        if (val1.isFloat) return LEVal::Invalid();
        if (val2.iv > 31) return LEVal((vint32)0);
        if (val2.iv < 0 || val2.iv > 31) return LEVal::Invalid();
        return LEVal(val1.iv<<val2.iv);
      case VBinary::RShift:
        if (val1.isFloat) return LEVal::Invalid();
        if (val2.iv > 31) return LEVal((vint32)(val1.iv>>31));
        if (val2.iv < 0 || val2.iv > 31) return LEVal::Invalid();
        return LEVal(val1.iv>>val2.iv);
      case VBinary::URShift:
        if (val1.isFloat) return LEVal::Invalid();
        if (val2.iv > 31) return LEVal((vint32)0);
        if (val2.iv < 0 || val2.iv > 31) return LEVal::Invalid();
        return LEVal((vint32)((vuint32)val1.iv>>val2.iv));
      case VBinary::And:
        if (val1.isFloat) return LEVal::Invalid();
        return LEVal((vint32)((vuint32)val1.iv&(vuint32)val2.iv));
      case VBinary::XOr:
        if (val1.isFloat) return LEVal::Invalid();
        return LEVal((vint32)((vuint32)val1.iv^(vuint32)val2.iv));
      case VBinary::Or:
        if (val1.isFloat) return LEVal::Invalid();
        return LEVal((vint32)((vuint32)val1.iv|(vuint32)val2.iv));
      case VBinary::StrCat:
      case VBinary::IsA:
      case VBinary::NotIsA:
        return LEVal::Invalid();
      default: break;
    }
    Sys_Error("ketmar forgot to process some binary operator in `VExpression::calcLE()`");
  }
  return LEVal::Invalid();
}


//==========================================================================
//
//  VExpression::CalculateNumericLiteralExpr
//
//  this simplifies expression consists of only numeric literals
//  if it cannot simplify the expression, it returns the original one
//
//==========================================================================
VExpression *VExpression::CalculateNumericLiteralExpr (VExpression *e) {
  auto rtp = IsNumericLiteralExpr(e);
  if (rtp == ExprNotNum) return e;
  auto val = calcLE(e);
  if (!val.isValid()) return e;
  if (val.isFloat) {
    VExpression *res = new VFloatLiteral(val.fv, e->Loc);
    delete e;
    return res;
  } else {
    VExpression *res = new VIntLiteral(val.iv, e->Loc);
    delete e;
    return res;
  }
}
