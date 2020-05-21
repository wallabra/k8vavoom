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

//#define VV_DEBUG_ALLOC_RELEASE


//==========================================================================
//
//  VLocalDecl::VLocalDecl
//
//==========================================================================
VLocalDecl::VLocalDecl (const TLocation &ALoc) : VExpression(ALoc) {
}


//==========================================================================
//
//  VLocalDecl::~VLocalDecl
//
//==========================================================================
VLocalDecl::~VLocalDecl () {
  for (int i = 0; i < Vars.length(); ++i) {
    if (Vars[i].TypeExpr) { delete Vars[i].TypeExpr; Vars[i].TypeExpr = nullptr; }
    if (Vars[i].Value) { delete Vars[i].Value; Vars[i].Value = nullptr; }
    if (Vars[i].TypeOfExpr) { delete Vars[i].TypeOfExpr; Vars[i].TypeOfExpr = nullptr; }
  }
}


//==========================================================================
//
//  VLocalDecl::SyntaxCopy
//
//==========================================================================
VExpression *VLocalDecl::SyntaxCopy () {
  auto res = new VLocalDecl();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalDecl::DoSyntaxCopyTo
//
//==========================================================================
void VLocalDecl::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalDecl *)e;
  res->Vars.SetNum(Vars.Num());
  for (int f = 0; f < Vars.Num(); ++f) {
    res->Vars[f] = Vars[f];
    if (Vars[f].TypeExpr) res->Vars[f].TypeExpr = Vars[f].TypeExpr->SyntaxCopy();
    if (Vars[f].Value) res->Vars[f].Value = Vars[f].Value->SyntaxCopy();
    if (Vars[f].TypeOfExpr) res->Vars[f].TypeOfExpr = Vars[f].TypeOfExpr->SyntaxCopy();
  }
}


//==========================================================================
//
//  VLocalDecl::DoResolve
//
//==========================================================================
VExpression *VLocalDecl::DoResolve (VEmitContext &ec) {
  VCFatalError("internal compiler error: VLocalDecl::DoResolve should not be called directly");
  Declare(ec);
  return this;
}


//==========================================================================
//
//  VLocalDecl::Emit
//
//==========================================================================
void VLocalDecl::Emit (VEmitContext &ec) {
  VCFatalError("internal compiler error: VLocalDecl::Emit should not be called directly");
  EmitInitialisations(ec);
}


//==========================================================================
//
//  VLocalDecl::EmitInitialisations
//
//  this either inits, or zeroes (unless `dozero` is `false`)
//
//==========================================================================
void VLocalDecl::EmitInitialisations (VEmitContext &ec) {
  for (auto &&loc : Vars) {
    // do we need to zero variable memory?
    // the variable was properly destructed beforehand (this is invariant)
    bool dozero = loc.emitClear;
    if (!dozero) {
      const VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
      // unconditionally zero complex things like dicts and structs
      dozero = (ldef.reused && !loc.Value);
      // still zero some complex data types
      if (!dozero && ldef.Type.NeedZeroingOnSlotReuse()) dozero = true;
    }
    // zero it if necessary
    if (dozero) {
      if (loc.locIdx < 0) VCFatalError("VC: internal compiler error (VLocalDecl::EmitInitialisations)");
      ec.EmitLocalZero(loc.locIdx, Loc);
    }
    if (loc.Value) loc.Value->Emit(ec);
  }
}


//==========================================================================
//
//  VLocalDecl::EmitDtors
//
//==========================================================================
void VLocalDecl::EmitDtors (VEmitContext &ec) {
  for (auto &&loc : Vars) ec.EmitLocalDtor(loc.locIdx, Loc);
}


//==========================================================================
//
//  VLocalDecl::Allocate
//
//==========================================================================
void VLocalDecl::Allocate (VEmitContext &ec) {
  for (auto &&loc : Vars) {
    ec.AllocateLocalSlot(loc.locIdx);
    #ifdef VV_DEBUG_ALLOC_RELEASE
    VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
    GLog.Logf(NAME_Debug, "VLocalDecl::Allocate: name=`%s`; idx=%d; ofs=%d; reused=%d; %s", *ldef.Name, loc.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    #endif
  }
}


//==========================================================================
//
//  VLocalDecl::Release
//
//==========================================================================
void VLocalDecl::Release (VEmitContext &ec) {
  for (auto &&loc : Vars) {
    ec.ReleaseLocalSlot(loc.locIdx);
    #ifdef VV_DEBUG_ALLOC_RELEASE
    VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
    GLog.Logf(NAME_Debug, "VLocalDecl::Release: name=`%s`; idx=%d; ofs=%d; reused=%d; %s", *ldef.Name, loc.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    #endif
  }
}


//==========================================================================
//
//  VLocalDecl::Declare
//
//  hide all declared locals
//
//==========================================================================
void VLocalDecl::Hide (VEmitContext &ec) {
  for (auto &&e : Vars) {
    if (e.locIdx >= 0) {
      VLocalVarDef &loc = ec.GetLocalByIndex(e.locIdx);
      loc.Visible = false;
    }
  }
}


//==========================================================================
//
//  VLocalDecl::Declare
//
//==========================================================================
bool VLocalDecl::Declare (VEmitContext &ec) {
  bool retres = true;
  for (int i = 0; i < Vars.length(); ++i) {
    VLocalEntry &e = Vars[i];

    if (ec.CheckForLocalVar(e.Name) != -1) {
      retres = false;
      ParseError(e.Loc, "Redefined identifier `%s`", *e.Name);
    }

    // resolve automatic type
    if (e.TypeExpr->Type.Type == TYPE_Automatic) {
      VExpression *te = (e.Value ? e.Value : e.TypeOfExpr);
      if (!te) Sys_Error("VC INTERNAL COMPILER ERROR: automatic type without initializer!");
      if (e.ctorInit) {
        retres = false;
        ParseError(e.Loc, "cannot determine type from ctor for local `%s`", *e.Name);
      }
      // resolve type
      if (e.toeIterArgN >= 0) {
        // special resolving for iterator
        if (te->IsAnyInvocation()) {
          VGagErrors gag;
          VMethod *mnext = ((VInvocationBase *)te)->GetVMethod(ec);
          if (mnext && e.toeIterArgN < mnext->NumParams) {
            //fprintf(stderr, "*** <%s>\n", *mnext->ParamTypes[e.toeIterArgN].GetName()); abort();
            delete e.TypeExpr; // delete old `automatic` type
            e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(mnext->ParamTypes[e.toeIterArgN], te->Loc);
          }
        }
        if (e.TypeExpr->Type.Type == TYPE_Automatic) {
          retres = false;
          ParseError(e.TypeExpr->Loc, "Cannot infer type for variable `%s`", *e.Name);
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(VFieldType(TYPE_Int), te->Loc);
        }
      } else {
        auto res = te->SyntaxCopy()->Resolve(ec);
        if (!res) {
          retres = false;
          ParseError(e.Loc, "Cannot resolve type for identifier `%s`", *e.Name);
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = new VTypeExprSimple(TYPE_Void, te->Loc);
        } else {
          //fprintf(stderr, "*** automatic type resolved to `%s`\n", *(res->Type.GetName()));
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(res->Type, te->Loc);
          delete res;
        }
      }
    }

    //GLog.Logf(NAME_Debug, "LOC:000: <%s>; type: <%s>\n", *e.Name, *e.TypeExpr->toString());
    e.TypeExpr = e.TypeExpr->ResolveAsType(ec);
    if (!e.TypeExpr) {
      retres = false;
      // create dummy local
      VLocalVarDef &L = ec.NewLocal(e.Name, VFieldType(TYPE_Void), e.Loc);
      L.ParamFlags = (e.isRef ? FPARM_Ref : 0)|(e.isConst ? FPARM_Const : 0);
      e.locIdx = L.ldindex;
      continue;
    }
    //GLog.Logf(NAME_Debug, "LOC:001: <%s>; type: <%s>\n", *e.Name, *e.TypeExpr->Type.GetName());

    VFieldType Type = e.TypeExpr->Type;
    if (Type.Type == TYPE_Void || Type.Type == TYPE_Automatic) {
      retres = false;
      ParseError(e.TypeExpr->Loc, "Bad variable type for variable `%s`", *e.Name);
    }

    //VLocalVarDef &L = ec.AllocLocal(e.Name, Type, e.Loc);
    VLocalVarDef &L = ec.NewLocal(e.Name, Type, e.Loc);
    L.ParamFlags = (e.isRef ? FPARM_Ref : 0)|(e.isConst ? FPARM_Const : 0);
    //if (e.isRef) fprintf(stderr, "*** <%s:%d> is REF\n", *e.Name, L.ldindex);
    e.locIdx = L.ldindex;

    // always clear reused/loop locals
    // this flag will be adjusted later
    e.emitClear = true; //!L.reused || ec.IsInLoop();

    // resolve initialisation
    if (e.Value) {
      // invocation means "constructor call"
      if (e.ctorInit) {
        if (Type.Type != TYPE_Struct) {
          ParseError(e.Value->Loc, "cannot construct something that is not a struct");
        } else {
          e.Value = e.Value->Resolve(ec);
          if (!e.Value) retres = false;
        }
      } else {
        L.Visible = false; // hide from initializer expression
        VExpression *op1 = new VLocalVar(L.ldindex, e.Loc);
        e.Value = new VAssignment(VAssignment::Assign, op1, e.Value, e.Loc);
        e.Value = e.Value->Resolve(ec);
        if (!e.Value) retres = false;
        L.Visible = true; // and make it visible again
        // if we are assigning default value, drop assign
        if (e.Value && e.Value->IsAssignExpr() &&
            ((VAssignment *)e.Value)->Oper == VAssignment::Assign &&
            ((VAssignment *)e.Value)->op2)
        {
          VExpression *val = ((VAssignment *)e.Value)->op2;
          bool defaultInit = false;
          switch (val->Type.Type) {
            case TYPE_Int:
            case TYPE_Byte:
            case TYPE_Bool:
              defaultInit = (val->IsIntConst() && val->GetIntConst() == 0);
              break;
            case TYPE_Float:
              defaultInit = (val->IsFloatConst() && val->GetFloatConst() == 0);
              break;
            case TYPE_Name:
              defaultInit = (val->IsNameConst() && val->GetNameConst() == NAME_None);
              break;
            case TYPE_String:
              defaultInit = (val->IsStrConst() && val->GetStrConst(ec.Package).length() == 0);
              break;
            case TYPE_Pointer:
              defaultInit = val->IsNullLiteral();
              break;
            case TYPE_Reference:
            case TYPE_Class:
            case TYPE_State:
              defaultInit = val->IsNoneLiteral();
              break;
            case TYPE_Delegate:
              defaultInit = (val->IsNoneDelegateLiteral() || val->IsNoneLiteral() || val->IsNullLiteral());
              break;
            case TYPE_Vector:
              if (val->IsConstVectorCtor()) {
                VVectorExpr *vc = (VVectorExpr *)val;
                TVec vec = vc->GetConstValue();
                defaultInit = (vec.x == 0 && vec.y == 0 && vec.z == 0);
              }
              break;
          }
          if (defaultInit) e.emitClear = false;
        }
      }
    }
    #ifdef VV_DEBUG_ALLOC_RELEASE
    {
      VLocalVarDef &ldef = ec.GetLocalByIndex(e.locIdx);
      GLog.Logf(NAME_Debug, "VLocalDecl::Declare(%d): name=`%s`; idx=%d; ofs=%d; reused=%d; %s", i, *ldef.Name, e.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    }
    #endif
  }
  return retres;
}


//==========================================================================
//
//  VLocalDecl::IsLocalVarDecl
//
//==========================================================================
bool VLocalDecl::IsLocalVarDecl () const {
  return true;
}


//==========================================================================
//
//  VLocalDecl::toString
//
//==========================================================================
VStr VLocalDecl::toString () const {
  VStr res;
  for (int f = 0; f < Vars.length(); ++f) {
    if (res.length()) res += " ";
    res += e2s(Vars[f].TypeExpr);
    res += " ";
    res += *Vars[f].Name;
    res += "("+VStr(Vars[f].locIdx)+")";
    if (Vars[f].Value) res += Vars[f].Value->toString();
    res += ";";
  }
  return res;
}



//==========================================================================
//
//  VLocalVar::VLocalVar
//
//==========================================================================
VLocalVar::VLocalVar (int ANum, const TLocation &ALoc)
  : VExpression(ALoc)
  , num(ANum)
  , AddressRequested(false)
  , PushOutParam(false)
  , locSavedFlags(0)
{
}


//==========================================================================
//
//  VLocalVar::SyntaxCopy
//
//==========================================================================
VExpression *VLocalVar::SyntaxCopy () {
  auto res = new VLocalVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalVar::DoSyntaxCopyTo
//
//==========================================================================
void VLocalVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalVar *)e;
  res->num = num;
  res->AddressRequested = AddressRequested;
  res->PushOutParam = PushOutParam;
}


//==========================================================================
//
//  VLocalVar::DoResolve
//
//==========================================================================
VExpression *VLocalVar::DoResolve (VEmitContext &ec) {
  const VLocalVarDef &loc = ec.GetLocalByIndex(num);
  locSavedFlags = loc.ParamFlags;
  Type = loc.Type;
  RealType = loc.Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  PushOutParam = !!(locSavedFlags&(FPARM_Out|FPARM_Ref));
  if (locSavedFlags&FPARM_Const) SetReadOnly();
  return this;
}


//==========================================================================
//
//  VLocalVar::RequestAddressOf
//
//==========================================================================
void VLocalVar::RequestAddressOf () {
  if (PushOutParam) {
    PushOutParam = false;
    return;
  }
  if (AddressRequested) {
    ParseError(Loc, "Multiple address of local (%s)", *toString());
    //VCFatalError("oops");
  }
  AddressRequested = true;
}


//==========================================================================
//
//  VLocalVar::Emit
//
//==========================================================================
void VLocalVar::Emit (VEmitContext &ec) {
  if (AddressRequested) {
    const VLocalVarDef &loc = ec.GetLocalByIndex(num);
    ec.EmitLocalAddress(loc.Offset, Loc);
  } else if (locSavedFlags&(FPARM_Out|FPARM_Ref)) {
    ec.EmitLocalPtrValue(num, Loc);
    if (PushOutParam) {
      const VLocalVarDef &loc = ec.GetLocalByIndex(num);
      EmitPushPointedCode(loc.Type, ec);
    }
  } else {
    ec.EmitLocalValue(num, Loc);
  }
}


//==========================================================================
//
//  VLocalVar::IsLocalVarExpr
//
//==========================================================================
bool VLocalVar::IsLocalVarExpr () const {
  return true;
}


//==========================================================================
//
//  VLocalDecl::toString
//
//==========================================================================
VStr VLocalVar::toString () const {
  VStr res("local(");
  res += VStr(num);
  res += ")";
  return res;
}
