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
//  VFieldBase
//
//==========================================================================
VFieldBase::VFieldBase (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , FieldName(AFieldName)
{
}

VFieldBase::~VFieldBase () {
  if (op) { delete op; op = nullptr; }
}

void VFieldBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VFieldBase *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->FieldName = FieldName;
}


//==========================================================================
//
//  VPointerField::VPointerField
//
//==========================================================================
VPointerField::VPointerField (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VFieldBase(AOp, AFieldName, ALoc)
{
}


//==========================================================================
//
//  VPointerField::SyntaxCopy
//
//==========================================================================
VExpression *VPointerField::SyntaxCopy () {
  auto res = new VPointerField();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPointerField::DoResolve
//
//==========================================================================
VExpression *VPointerField::DoResolve (VEmitContext &ec) {
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Pointer type required on left side of ->");
    delete this;
    return nullptr;
  }

  VFieldType type = op->Type.GetPointerInnerType();
  if (!type.Struct) {
    ParseError(Loc, "Not a structure type");
    delete this;
    return nullptr;
  }

  VField *field = type.Struct->FindField(type.Struct->ResolveAlias(FieldName));
  if (!field) {
    ParseError(Loc, "No such field %s", *FieldName);
    delete this;
    return nullptr;
  }

  VExpression *e = new VFieldAccess(op, field, Loc, 0);
  op = nullptr;
  delete this;

  return e->Resolve(ec);
}


//==========================================================================
//
//  VPointerField::Emit
//
//==========================================================================
void VPointerField::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VPointerField)");
}


//==========================================================================
//
//  VDotField::VDotField
//
//==========================================================================
VDotField::VDotField (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VFieldBase(AOp, AFieldName, ALoc)
{
}


//==========================================================================
//
//  VDotField::IsDotField
//
//==========================================================================
bool VDotField::IsDotField () const { return true; }


//==========================================================================
//
//  VDotField::SyntaxCopy
//
//==========================================================================
VExpression *VDotField::SyntaxCopy () {
  auto res = new VDotField();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDotField::InternalResolve
//
//==========================================================================
VExpression *VDotField::InternalResolve (VEmitContext &ec, VDotField::AssType assType) {
  // we need a copy in case this is a pointer thingy
  auto opcopy = (op ? op->SyntaxCopy() : nullptr);

  if (op) op = op->Resolve(ec);
  if (!op) {
    delete opcopy;
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Pointer) {
    // allow dotted access for dynamic arrays
    if (op->Type.InnerType == TYPE_DynamicArray) {
      auto cp2 = opcopy->SyntaxCopy(); // for vector property conversions
      delete op;
      op = nullptr;
      op = (new VPushPointed(opcopy))->Resolve(ec);
      if (!op) { delete cp2; delete this; return nullptr; }
      opcopy = cp2;
    } else {
      delete op;
      op = nullptr;
      VPointerField *e = new VPointerField(opcopy, FieldName, Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  if (op->Type.Type == TYPE_Reference) {
    VMethod *M = op->Type.Class->FindAccessibleMethod(op->Type.Class->ResolveAlias(FieldName), ec.SelfClass);
    if (M) {
      //fprintf(stderr, "DOTFIELD: <%s> {%s} %u\n", *FieldName, *op->Type.GetName(), op->Type.Type);
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete opcopy;
        delete this;
        return nullptr;
      }
      VExpression *e;
      // `dg = dgname`?
      /*if (assType == AssType::AssValue) {
        // yes
        ParseWarning(Loc, "prepend delegate with `&`, please");
        e = new VDelegateVal(op, M, Loc);
        op = nullptr;
        delete opcopy;
      } else*/ {
        // no; rewrite as invoke
        if ((M->Flags&FUNC_Static) != 0) {
          delete opcopy;
          e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
        } else {
          e = new VDotInvocation(opcopy, op->Type.Class->ResolveAlias(FieldName), Loc, 0, nullptr);
        }
      }
      delete this;
      return e->Resolve(ec);
    }

    // we never ever need opcopy here
    delete opcopy;

    VField *field = op->Type.Class->FindField(op->Type.Class->ResolveAlias(FieldName), Loc, ec.SelfClass);
    if (field) {
      VExpression *e;
      // "normal" access: call delegate (if it is operand-less)
      if (assType == AssType::Normal && field->Type.Type == TYPE_Delegate && field->Func && field->Func->NumParams == 0) {
        //fprintf(stderr, "*** FLD! %s\n", *field->Name);
        e = new VInvocation(nullptr, field->Func, field, false, false, Loc, 0, nullptr);
      } else {
        // generate field access
        e = new VFieldAccess(op, field, Loc, op->IsDefaultObject() ? FIELD_ReadOnly : 0);
        op = nullptr;
      }
      delete this;
      return e->Resolve(ec);
    }

    VProperty *Prop = op->Type.Class->FindProperty(op->Type.Class->ResolveAlias(FieldName));
    if (Prop) {
      if (assType == AssType::AssTarget) {
        if (!Prop->SetFunc) {
          ParseError(Loc, "Property %s cannot be set", *FieldName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VPropertyAssign(op, Prop->SetFunc, true, Loc);
        op = nullptr;
        delete this;
        // assignment will call resolve
        return e;
      } else {
        if (op->IsDefaultObject()) {
          if (!Prop->DefaultField) {
            ParseError(Loc, "Property %s has no default field set", *FieldName);
            delete this;
            return nullptr;
          }
          VExpression *e = new VFieldAccess(op, Prop->DefaultField, Loc, FIELD_ReadOnly);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        } else {
          if (!Prop->GetFunc) {
            ParseError(Loc, "Property %s cannot be read", *FieldName);
            delete this;
            return nullptr;
          }
          VExpression *e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        }
      }
    }

    ParseError(Loc, "No such field %s", *FieldName);
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Struct || op->Type.Type == TYPE_Vector) {
    VFieldType type = op->Type;
    if (!type.Struct) {
      // convert to method, 'cause why not?
      if (assType == AssType::Normal) {
        VExpression *ufcsArgs[1];
        ufcsArgs[0] = opcopy;
        VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      } else {
        delete opcopy;
        ParseError(Loc, "INTERNAL COMPILER ERROR: No such field `%s`, and no struct also!", *FieldName);
        delete this;
        return nullptr;
      }
    }
    delete opcopy; // we never ever need opcopy here
    int Flags = op->Flags;
    op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
    VField *field = type.Struct->FindField(type.Struct->ResolveAlias(FieldName));
    if (!field) {
      ParseError(Loc, "No such field %s", *FieldName);
      delete this;
      return nullptr;
    }
    VExpression *e = new VFieldAccess(op, field, Loc, Flags & FIELD_ReadOnly);
    op = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  if (op->Type.Type == TYPE_DynamicArray) {
    delete opcopy; // we never ever need opcopy here
    //VFieldType type = op->Type;
    op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
    if (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        VExpression *e = new VDynArraySetNum(op, nullptr, Loc);
        op = nullptr;
        delete this;
        return e->Resolve(ec);
      } else {
        VExpression *e = new VDynArrayGetNum(op, Loc);
        op = nullptr;
        delete this;
        return e->Resolve(ec);
      }
    } else {
      ParseError(Loc, "No such field %s", *FieldName);
      delete this;
      return nullptr;
    }
  }

  if (op->Type.Type == TYPE_String) {
    delete opcopy; // we never ever need opcopy here
    if (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change string length via assign yet.");
        delete this;
        return nullptr;
      }
      if (!op->IsStrConst()) {
        op->Flags &= ~FIELD_ReadOnly;
        op->RequestAddressOf();
      }
      VExpression *e = new VStringGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      ParseError(Loc, "No field '%s' for string", *FieldName);
      delete this;
      return nullptr;
    }
  }

  // convert to method, 'cause why not?
  if (assType != AssType::AssTarget) {
    // Class.Method -- for static methods
    if (op->Type.Type == TYPE_Class) {
      delete opcopy; // we never ever need opcopy here
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.`");
        delete this;
        return nullptr;
      }
      VName origName = op->Type.Class->ResolveAlias(FieldName);
      // read property
      VProperty *Prop = op->Type.Class->FindProperty(origName);
      if (Prop) {
        if (op->IsDefaultObject()) {
          if (!Prop->DefaultField) {
            ParseError(Loc, "Property `%s` has no default field set", *FieldName);
            delete this;
            return nullptr;
          }
          VExpression *e = new VFieldAccess(op, Prop->DefaultField, Loc, FIELD_ReadOnly);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        } else {
          if (!Prop->GetFunc) {
            ParseError(Loc, "Property `%s` cannot be read", *FieldName);
            delete this;
            return nullptr;
          }
          VExpression *e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        }
      }
      // method
      VMethod *M = op->Type.Class->FindAccessibleMethod(origName, ec.SelfClass);
      if (!M) {
        ParseError(Loc, "Method `%s` not found in class `%s`", *FieldName, op->Type.Class->GetName());
        delete this;
        return nullptr;
      }
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      if ((M->Flags&FUNC_Static) == 0) {
        ParseError(Loc, "Only static methods can be called with this syntax");
        delete this;
        return nullptr;
      }
      // statics has no self
      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
    // convert to `func(op)`
    if (ec.SelfClass) {
      VExpression *ufcsArgs[1];
      ufcsArgs[0] = opcopy;
      if (VInvocation::FindMethodWithSignature(ec, ec.SelfClass->ResolveAlias(FieldName), 1, ufcsArgs)) {
        VCastOrInvocation *call = new VCastOrInvocation(ec.SelfClass->ResolveAlias(FieldName), Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      }
    }
  } else if (assType == AssType::AssTarget) {
    if (op->Type.Type == TYPE_Class) {
      delete opcopy; // we never ever need opcopy here
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.`");
        delete this;
        return nullptr;
      }
      VName origName = op->Type.Class->ResolveAlias(FieldName);
      // property
      VProperty *Prop = op->Type.Class->FindProperty(origName);
      if (Prop) {
        if (!Prop->SetFunc) {
          ParseError(Loc, "Property `%s` cannot be set", *FieldName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VPropertyAssign(op, Prop->SetFunc, true, Loc);
        op = nullptr;
        delete this;
        // assignment will call resolve
        return e;
      }
    }
  }

  delete opcopy; // we never ever need opcopy here
  ParseError(Loc, "Reference, struct or vector expected on left side of `.` (got `%s`)", *op->Type.GetName());
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDotField::DoResolve
//
//==========================================================================
VExpression *VDotField::DoResolve (VEmitContext &ec) {
  return InternalResolve(ec, AssType::Normal);
}


//==========================================================================
//
//  VDotField::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VDotField::ResolveAssignmentTarget (VEmitContext &ec) {
  return InternalResolve(ec, AssType::AssTarget);
}


//==========================================================================
//
//  VDotField::ResolveAssignmentValue
//
//==========================================================================
VExpression *VDotField::ResolveAssignmentValue (VEmitContext &ec) {
  return InternalResolve(ec, AssType::AssValue);
}


//==========================================================================
//
//  VDotField::Emit
//
//==========================================================================
void VDotField::Emit (VEmitContext&) {
  ParseError(Loc, "Should not happen (VDotField)");
}


//==========================================================================
//
//  VFieldAccess::VFieldAccess
//
//==========================================================================
VFieldAccess::VFieldAccess (VExpression *AOp, VField *AField, const TLocation &ALoc, int ExtraFlags)
  : VExpression(ALoc)
  , op(AOp)
  , field(AField)
  , AddressRequested(false)
{
  Flags = field->Flags | ExtraFlags;
}


//==========================================================================
//
//  VFieldAccess::~VFieldAccess
//
//==========================================================================
VFieldAccess::~VFieldAccess () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VFieldAccess::SyntaxCopy
//
//==========================================================================
VExpression *VFieldAccess::SyntaxCopy () {
  auto res = new VFieldAccess();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFieldAccess::DoSyntaxCopyTo
//
//==========================================================================
void VFieldAccess::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VFieldAccess *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->field = field;
  res->AddressRequested = AddressRequested;
}


//==========================================================================
//
//  VFieldAccess::DoResolve
//
//==========================================================================
VExpression *VFieldAccess::DoResolve (VEmitContext&) {
  Type = field->Type;
  RealType = field->Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VFieldAccess::RequestAddressOf
//
//==========================================================================
void VFieldAccess::RequestAddressOf () {
  if (Flags&FIELD_ReadOnly) ParseError(op->Loc, "Tried to assign to a read-only field");
  if (AddressRequested) ParseError(Loc, "Multiple address of");
  AddressRequested = true;
}


//==========================================================================
//
//  VFieldAccess::Emit
//
//==========================================================================
void VFieldAccess::Emit (VEmitContext &ec) {
  if (!op) return; //k8: don't segfault
  op->Emit(ec);
  if (AddressRequested) {
    ec.AddStatement(OPC_Offset, field);
  } else {
    switch (field->Type.Type) {
      case TYPE_Int:
      case TYPE_Float:
      case TYPE_Name:
        ec.AddStatement(OPC_FieldValue, field);
        break;
      case TYPE_Byte:
        ec.AddStatement(OPC_ByteFieldValue, field);
        break;
      case TYPE_Bool:
             if (field->Type.BitMask&0x000000ff) ec.AddStatement(OPC_Bool0FieldValue, field, (int)(field->Type.BitMask));
        else if (field->Type.BitMask&0x0000ff00) ec.AddStatement(OPC_Bool1FieldValue, field, (int)(field->Type.BitMask>>8));
        else if (field->Type.BitMask&0x00ff0000) ec.AddStatement(OPC_Bool2FieldValue, field, (int)(field->Type.BitMask>>16));
        else ec.AddStatement(OPC_Bool3FieldValue, field, (int)(field->Type.BitMask>>24));
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        ec.AddStatement(OPC_PtrFieldValue, field);
        break;
      case TYPE_Vector:
        ec.AddStatement(OPC_VFieldValue, field);
        break;
      case TYPE_String:
        ec.AddStatement(OPC_StrFieldValue, field);
        break;
      case TYPE_Delegate:
        ec.AddStatement(OPC_Offset, field);
        ec.AddStatement(OPC_PushPointedDelegate);
        break;
      default:
        ParseError(Loc, "Invalid operation on field of this type");
    }
  }
}


//==========================================================================
//
//  VDelegateVal::VDelegateVal
//
//==========================================================================
VDelegateVal::VDelegateVal (VExpression *AOp, VMethod *AM, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , M(AM)
{
}


//==========================================================================
//
//  VDelegateVal::~VDelegateVal
//
//==========================================================================
VDelegateVal::~VDelegateVal () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VDelegateVal::SyntaxCopy
//
//==========================================================================
VExpression *VDelegateVal::SyntaxCopy () {
  auto res = new VDelegateVal();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDelegateVal::DoSyntaxCopyTo
//
//==========================================================================
void VDelegateVal::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDelegateVal *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->M = M;
}


//==========================================================================
//
//  VDelegateVal::DoResolve
//
//==========================================================================
VExpression *VDelegateVal::DoResolve (VEmitContext &) {
  Type = TYPE_Delegate;
  Type.Function = M;
  return this;
}


//==========================================================================
//
//  VDelegateVal::Emit
//
//==========================================================================
void VDelegateVal::Emit (VEmitContext &ec) {
  if (!op) return;
  op->Emit(ec);
  ec.AddStatement(OPC_PushVFunc, M);
}
