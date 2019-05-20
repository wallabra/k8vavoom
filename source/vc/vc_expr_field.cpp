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

// builtin codes
#define BUILTIN_OPCODE_INFO
#include "../progdefs.h"

#define DICTDISPATCH_OPCODE_INFO
#include "../progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "../progdefs.h"


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

//==========================================================================
//
//  VFieldBase::~VFieldBase
//
//==========================================================================
VFieldBase::~VFieldBase () {
  if (op) { delete op; op = nullptr; }
}

//==========================================================================
//
//  VFieldBase::DoSyntaxCopyTo
//
//==========================================================================
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
//  VPointerField::TryUFCS
//
//==========================================================================
VExpression *VPointerField::TryUFCS (VEmitContext &ec, AutoCopy &opcopy, const char *errdatatype, VMemberBase *mb) {
  // try UFCS
  VExpression *pp = opcopy.extract();
  if (ec.SelfClass) {
    pp = new VPushPointed(pp, pp->Loc);
    //HACK! for classes
    if (mb->MemberType == MEMBER_Class) {
      // class
      VClass *cls = (VClass *)mb;
      VMethod *mt = cls->FindAccessibleMethod(FieldName, ec.SelfClass);
      if (mt) {
        VExpression *dotinv = new VDotInvocation(pp, FieldName, Loc, 0, nullptr);
        delete this;
        return dotinv->Resolve(ec);
      }
    }
    // normal UFCS
    VExpression *ufcsArgs[2/*VMethod::MAX_PARAMS+1*/];
    ufcsArgs[0] = pp;
    if (VInvocation::FindMethodWithSignature(ec, FieldName, 1, ufcsArgs)) {
      VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
      delete this;
      return call->Resolve(ec);
    }
  }
  delete pp;
  ParseError(Loc, "No such field `%s` in %s `%s`", *FieldName, errdatatype, *mb->GetFullName());
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VPointerField::DoResolve
//
//==========================================================================
VExpression *VPointerField::DoResolve (VEmitContext &ec) {
  AutoCopy opcopy(op);

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

  if (type.Type == TYPE_Struct) {
    if (!type.Struct) {
      ParseError(Loc, "Not a structure/reference type");
      delete this;
      return nullptr;
    }
  } else if (type.Type == TYPE_Vector) {
    // vectors are such structs
    if (!type.Struct) {
#if defined(IN_VCC) //&& !defined(VCC_STANDALONE_EXECUTOR)
      VClass *ocls = (VClass *)VMemberBase::StaticFindMember("Object", ANY_PACKAGE, MEMBER_Class);
      check(ocls);
      VStruct *tvs = (VStruct *)VMemberBase::StaticFindMember("TVec", ocls, MEMBER_Struct);
#else
      VStruct *tvs = (VStruct *)VMemberBase::StaticFindMember("TVec", /*ANY_PACKAGE*/VObject::StaticClass(), MEMBER_Struct);
#endif
      if (!tvs) {
        ParseError(Loc, "Cannot find `TVec` struct for vector type");
        delete this;
        return nullptr;
      }
      type.Struct = tvs;
    }
  } else if (type.Type == TYPE_Reference) {
    // this can came from dictionary
    if (!type.Class) {
      ParseError(Loc, "Not a structure/reference type");
      delete this;
      return nullptr;
    }
  } else {
    ParseError(Loc, "Expected structure/reference type, but got `%s`", *op->Type.GetName());
    delete this;
    return nullptr;
  }

  VField *field;
  if (type.Type == TYPE_Struct || type.Type == TYPE_Vector) {
    check(type.Struct);
    field = type.Struct->FindField(FieldName);
    if (!field) return TryUFCS(ec, opcopy, "struct", type.Struct);
  } else {
    check(type.Type == TYPE_Reference);
    check(type.Class);
    field = type.Class->FindField(FieldName);
    if (!field) return TryUFCS(ec, opcopy, "class", type.Class);
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
//  VPointerField::toString
//
//==========================================================================
VStr VPointerField::toString () const {
  return e2s(op)+"->"+VStr(FieldName);
}



//==========================================================================
//
//  VDotField::VDotField
//
//==========================================================================
VDotField::VDotField (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VFieldBase(AOp, AFieldName, ALoc)
  , builtin(-1)
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
VExpression *VDotField::DoPropertyResolve (VEmitContext &ec, VProperty *Prop, AssType assType) {
  if (assType == AssType::AssTarget) {
    if (Prop->SetFunc) {
      VExpression *e;
      if ((Prop->SetFunc->Flags&FUNC_Static) != 0) {
        if (op->Type.Type != TYPE_Class || !op->Type.Class) {
          ParseError(Loc, "Class name expected at the left side of `.`");
          delete this;
          return nullptr;
        }
        // statics has no self
        e = new VPropertyAssign(nullptr, Prop->SetFunc, false, Loc);
      } else {
        if (op->Type.Type == TYPE_Class) {
          ParseError(Loc, "Trying to use non-static property as static");
          delete this;
          return nullptr;
        }
        e = new VPropertyAssign(op, Prop->SetFunc, true, Loc);
        op = nullptr;
      }
      delete this;
      // assignment will call resolve
      return e;
    } else if (Prop->WriteField) {
      if (op->Type.Type == TYPE_Class) {
        ParseError(Loc, "Static fields are not supported yet");
        delete this;
        return nullptr;
      }
      VExpression *e = new VFieldAccess(op, Prop->WriteField, Loc, 0);
      op = nullptr;
      delete this;
      return e->ResolveAssignmentTarget(ec);
    } else {
      ParseError(Loc, "Property `%s` cannot be set", *FieldName);
      delete this;
      return nullptr;
    }
  } else {
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
      if (Prop->GetFunc) {
        VExpression *e;
        if ((Prop->GetFunc->Flags&FUNC_Static) != 0) {
          //e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          if (op->Type.Type != TYPE_Class || !op->Type.Class) {
            ParseError(Loc, "Class name expected at the left side of `.`");
            delete this;
            return nullptr;
          }
          // statics has no self
          e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
        } else {
          if (op->Type.Type == TYPE_Class) {
            ParseError(Loc, "Trying to use non-static property as static");
            delete this;
            return nullptr;
          }
          e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          op = nullptr;
        }
        delete this;
        return e->Resolve(ec);
      } else if (Prop->ReadField) {
        VExpression *e = new VFieldAccess(op, Prop->ReadField, Loc, 0);
        op = nullptr;
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Property `%s` cannot be read", *FieldName);
        delete this;
        return nullptr;
      }
    }
  }
}


//==========================================================================
//
//  VDotField::InternalResolve
//
//==========================================================================
VExpression *VDotField::InternalResolve (VEmitContext &ec, VDotField::AssType assType) {
  // try enum constant
  if (op && op->IsSingleName()) {
    VName ename = ((VSingleName *)op)->Name;
    // in this class
    if (ec.SelfClass && ec.SelfClass->IsKnownEnum(ename)) {
      VConstant *Const = ec.SelfClass->FindConstant(FieldName, ename);
      if (Const) {
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s`", *FieldName, *ename);
        delete this;
        return nullptr;
      }
    }
    // in this package
    //fprintf(stderr, "ENUM TRY: <%s %s>\n", *ename, *FieldName);
    if (ec.Package->IsKnownEnum(ename)) {
      //fprintf(stderr, "  <%s %s>\n", *ename, *FieldName);
      VConstant *Const = ec.Package->FindConstant(FieldName, ename);
      if (Const) {
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s`", *FieldName, *ename);
        delete this;
        return nullptr;
      }
    }
    // in any package (this does package imports)
    VConstant *Const = (VConstant *)VMemberBase::StaticFindMember(FieldName, ANY_PACKAGE, MEMBER_Const, ename);
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  // try enum constant in other class: `C::E.x`
  if (op && op->IsDoubleName()) {
    VDoubleName *dn = (VDoubleName *)op;
    VClass *Class = VMemberBase::StaticFindClass(dn->Name1);
    //fprintf(stderr, "Class=%s; n0=%s; n1=%s (fld=%s)\n", Class->GetName(), *dn->Name1, *dn->Name2, *FieldName);
    //!if (Class && Class != ec.SelfClass && !Class->Defined) Class->Define();
    if (Class && Class->IsKnownEnum(dn->Name2)) {
      VConstant *Const = Class->FindConstant(FieldName, dn->Name2);
      if (Const) {
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s::%s`", *FieldName, *dn->Name1, *dn->Name2);
        delete this;
        return nullptr;
      }
    }
  }

  // we need a copy in case this is a pointer thingy
  AutoCopy opcopy(op);

  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  // allow dotted access for dynamic arrays
  if (op->Type.Type == TYPE_Pointer) {
    auto oldflags = op->Flags;
    if (op->Type.InnerType == TYPE_DynamicArray) {
      auto loc = op->Loc;
      delete op;
      op = nullptr;
      op = (new VPushPointed(opcopy.SyntaxCopy(), loc))->Resolve(ec);
      op->Flags |= oldflags&FIELD_ReadOnly;
      if (!op) { delete this; return nullptr; }
    } else {
      delete op;
      op = nullptr;
      VPointerField *e = new VPointerField(opcopy.extract(), FieldName, Loc);
      e->Flags |= oldflags&FIELD_ReadOnly;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (op->Type.Type == TYPE_Reference) {
    if (op->Type.Class) {
      //!if (op->Type.Class && op->Type.Class != ec.SelfClass && !op->Type.Class->Defined) op->Type.Class->Define();
      VMethod *M = op->Type.Class->FindAccessibleMethod(FieldName, ec.SelfClass, &Loc);
      if (M) {
        if (M->Flags&FUNC_Iterator) {
          ParseError(Loc, "Iterator methods can only be used in foreach statements");
          delete this;
          return nullptr;
        }
        VExpression *e;
        // rewrite as invoke
        if ((M->Flags&FUNC_Static) != 0) {
          e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
        } else {
          e = new VDotInvocation(opcopy.extract(), FieldName, Loc, 0, nullptr);
        }
        delete this;
        return e->Resolve(ec);
      }

      // we never ever need opcopy here
      //opcopy.release();

      VField *field = op->Type.Class->FindField(FieldName, Loc, ec.SelfClass);
      if (field) {
        VExpression *e;
        // "normal" access: call delegate (if it is operand-less)
        /*if (assType == AssType::Normal && field->Type.Type == TYPE_Delegate && field->Func && field->Func->NumParams == 0) {
          fprintf(stderr, "*** FLD! %s\n", *field->Name);
          e = new VInvocation(nullptr, field->Func, field, false, false, Loc, 0, nullptr);
        } else*/ {
          // generate field access
          e = new VFieldAccess(op, field, Loc, op->IsDefaultObject() ? FIELD_ReadOnly : 0);
          //!!!e->Flags |= op->Flags&FIELD_ReadOnly;
          op = nullptr;
        }
        delete this;
        return e->Resolve(ec);
      }

      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
    }

    // convert to method, 'cause why not?
    if (assType != AssType::AssTarget && ec.SelfClass && ec.SelfClass->FindMethod(FieldName)) {
      VExpression *ufcsArgs[1];
      ufcsArgs[0] = opcopy.extract();
      VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
      delete this;
      return call->Resolve(ec);
    }

    ParseError(Loc, "No such field `%s`", *FieldName);
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Struct || op->Type.Type == TYPE_Vector) {
    VFieldType type = op->Type;
    // `auto v = vector(a, b, c)` is vector without struct, for example, hence the check
    if (!type.Struct && (FieldName == "x" || FieldName == "y" || FieldName == "z")) {
      // this is something that creates a vector without a struct, and wants a field
      int index;
      switch ((*FieldName)[0]) {
        case 'x': index = 0; break;
        case 'y': index = 1; break;
        case 'z': index = 2; break;
        default: abort();
      }
      VExpression *e = new VVectorFieldAccess(op, index, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    }
    VField *field = (type.Struct ? type.Struct->FindField(FieldName) : nullptr);
    if (!field) {
      // convert to method, 'cause why not?
      if (assType != AssType::AssTarget) {
        VExpression *ufcsArgs[1];
        ufcsArgs[0] = opcopy.extract();
        VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      } else {
        ParseError(Loc, "No such field `%s`", *FieldName);
        delete this;
        return nullptr;
      }
    }
    //delete opcopy; // we never ever need opcopy here
    //opcopy = nullptr; // just in case
    int opflags = op->Flags;
    //!!!if (assType != AssType::AssTarget) op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
    VExpression *e = new VFieldAccess(op, field, Loc, Flags&FIELD_ReadOnly);
    e->Flags |= (opflags|field->Flags)&FIELD_ReadOnly;
    op = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  // dynarray properties
  if (op->Type.Type == TYPE_DynamicArray &&
      (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length ||
       FieldName == "length1" || FieldName == "length2"))
  {
    //delete opcopy; // we never ever need opcopy here
    //opcopy = nullptr; // just in case
    //VFieldType type = op->Type;
    if (assType == AssType::AssTarget) {
      if (FieldName == "length1" || FieldName == "length2") {
        ParseError(Loc, "Use `setLength` method to create 2d dynamic array");
        delete this;
        return nullptr;
      }
      if (op->Flags&FIELD_ReadOnly) {
        ParseError(Loc, "Cannot change length of read-only array");
        delete this;
        return nullptr;
      }
    }
    op->RequestAddressOf();
    if (assType == AssType::AssTarget) {
      // op1 will be patched in assign resolver, so it is ok
      VExpression *e = new VDynArraySetNum(op, nullptr, nullptr, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      VExpression *e = new VDynArrayGetNum(op, (FieldName == "length1" ? 1 : FieldName == "length2" ? 2 : 0), Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    }
  }

  // array properties
  if (op->Type.Type == TYPE_Array &&
      (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length ||
       FieldName == "length1" || FieldName == "length2"))
  {
    //delete opcopy; // we never ever need opcopy here
    //opcopy = nullptr; // just in case
    if (assType == AssType::AssTarget) {
      ParseError(Loc, "Cannot change length of static array");
      delete this;
      return nullptr;
    }
    VExpression *e;
         if (FieldName == "length1") e = new VIntLiteral(op->Type.GetFirstDim(), Loc);
    else if (FieldName == "length2") e = new VIntLiteral(op->Type.GetSecondDim(), Loc);
    else e = new VIntLiteral(op->Type.GetArrayDim(), Loc);
    delete this;
    return e->Resolve(ec);
  }

  // string properties
  if (op->Type.Type == TYPE_String) {
    if (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length) {
      //delete opcopy; // we never ever need opcopy here
      //opcopy = nullptr; // just in case
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change string length via assign yet");
        delete this;
        return nullptr;
      }
      if (!op->IsStrConst()) op->RequestAddressOf();
      VExpression *e = new VStringGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    }
    static const char *knownTrans[] = {
      "toLowerCase", "strlwr",
      "toUpperCase", "strupr",
      nullptr,
    };
    for (const char **tr = knownTrans; *tr; tr += 2) {
      if (FieldName == *tr) {
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign to read-only property `%s`", *FieldName);
          delete this;
          return nullptr;
        }
        // let UFCS do the work
        FieldName = VName(tr[1]);
        break;
      }
    }
  }

  // float properties
  if (op->Type.Type == TYPE_Float) {
         if (FieldName == "isnan" || FieldName == "isNan" || FieldName == "isNaN" || FieldName == "isNAN") builtin = OPC_Builtin_FloatIsNaN;
    else if (FieldName == "isinf" || FieldName == "isInf") builtin = OPC_Builtin_FloatIsInf;
    else if (FieldName == "isfinite" || FieldName == "isFinite") builtin = OPC_Builtin_FloatIsFinite;
    if (builtin >= 0) {
      //delete opcopy; // we never ever need opcopy here
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot assign to read-only property");
        delete this;
        return nullptr;
      }
      // optimization for float literals
      if (op->IsFloatConst()) {
        VExpression *e = nullptr;
        switch (builtin) {
          case OPC_Builtin_FloatIsNaN: e = new VIntLiteral((isNaNF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          case OPC_Builtin_FloatIsInf: e = new VIntLiteral((isInfF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          case OPC_Builtin_FloatIsFinite: e = new VIntLiteral((isFiniteF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          default: FatalError("VC: internal compiler error (float property `%s`)", *FieldName);
        }
        delete this;
        return e->Resolve(ec);
      }
      //FIXME: use int instead of bool here, it generates faster opcode, and doesn't matter for now
      //Type = VFieldType(TYPE_Bool); Type.BitMask = 1;
      Type = VFieldType(TYPE_Int);
      return this;
    }
  }

  // slice properties
  if (op->Type.Type == TYPE_SliceArray) {
    if (FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change slice length");
        delete this;
        return nullptr;
      }
      op->RequestAddressOf();
      VExpression *e = new VSliceGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else if (FieldName == "ptr") {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change slice ptr");
        delete this;
        return nullptr;
      }
      op->RequestAddressOf();
      VExpression *e = new VSliceGetPtr(op, Loc);
      e->Flags |= (Flags|op->Flags)&FIELD_ReadOnly;
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      ParseError(Loc, "No field `%s` in slice", *FieldName);
      delete this;
      return nullptr;
    }
  }

  // dictionary properties
  if (op->Type.Type == TYPE_Dictionary) {
    if (FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change dictionary length");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDictGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else if (FieldName == "capacity") {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change dictionary capacity");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDictGetCapacity(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      ParseError(Loc, "No field `%s` in dictionary", *FieldName);
      delete this;
      return nullptr;
    }
  }

  // convert to method, 'cause why not?
  if (assType != AssType::AssTarget) {
    // Class.Method -- for static methods
    if (op->Type.Type == TYPE_Class) {
      //delete opcopy; // we never ever need opcopy here
      //opcopy = nullptr; // just in case
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.`");
        delete this;
        return nullptr;
      }
      // read property
      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
      // method
      VMethod *M = op->Type.Class->FindAccessibleMethod(FieldName, ec.SelfClass, &Loc);
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
      ufcsArgs[0] = opcopy.extract();
      if (VInvocation::FindMethodWithSignature(ec, FieldName, 1, ufcsArgs)) {
        VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      }
    }
  } else if (assType == AssType::AssTarget) {
    if (op->Type.Type == TYPE_Class) {
      //delete opcopy; // we never ever need opcopy here
      //opcopy = nullptr; // just in case
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.`");
        delete this;
        return nullptr;
      }
      // write property
      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
    }
  }

  //delete opcopy; // we never ever need opcopy here
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
  if (Flags&FIELD_ReadOnly) {
    ParseError(Loc, "Cannot assign to read-only destination");
    delete this;
    return nullptr;
  }
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
void VDotField::Emit (VEmitContext &ec) {
  if (builtin < 0) {
    ParseError(Loc, "Should not happen (VDotField)");
  } else {
    if (op) op->Emit(ec);
    ec.AddBuiltin(builtin, Loc);
  }
}


//==========================================================================
//
//  VDotField::toString
//
//==========================================================================
VStr VDotField::toString () const {
  return e2s(op)+"."+VStr(FieldName);
}



//==========================================================================
//
//  VVectorFieldAccess::VVectorFieldAccess
//
//==========================================================================
VVectorFieldAccess::VVectorFieldAccess (VExpression *AOp, int AIndex, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , index(AIndex)
{
}


//==========================================================================
//
//  VVectorFieldAccess::~VVectorFieldAccess
//
//==========================================================================
VVectorFieldAccess::~VVectorFieldAccess () {
  delete op; op = nullptr;
}


//==========================================================================
//
//  VVectorFieldAccess::SyntaxCopy
//
//==========================================================================
VExpression *VVectorFieldAccess::SyntaxCopy () {
  auto res = new VVectorFieldAccess();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorFieldAccess::DoSyntaxCopyTo
//
//==========================================================================
void VVectorFieldAccess::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VVectorFieldAccess *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->index = index;
}


//==========================================================================
//
//  VVectorFieldAccess::DoResolve
//
//==========================================================================
VExpression *VVectorFieldAccess::DoResolve (VEmitContext &ec) {
  if (op) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  Type = VFieldType(TYPE_Float);
  return this;
}


//==========================================================================
//
//  VVectorFieldAccess::Emit
//
//==========================================================================
void VVectorFieldAccess::Emit (VEmitContext &ec) {
  if (!op) return;
  op->Emit(ec);
  ec.AddStatement(OPC_VectorDirect, index, Loc);
}


//==========================================================================
//
//  VVectorFieldAccess::toString
//
//==========================================================================
VStr VVectorFieldAccess::toString () const {
  return e2s(op)+"."+(index == 0 ? VStr("x") : index == 1 ? VStr("y") : index == 2 ? VStr("z") : VStr(index));
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
  Flags = /*field->Flags|*/ExtraFlags;
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
VExpression *VFieldAccess::DoResolve (VEmitContext &) {
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
    ec.AddStatement(OPC_Offset, field, Loc);
  } else {
    switch (field->Type.Type) {
      case TYPE_Int:
      case TYPE_Float:
      case TYPE_Name:
        ec.AddStatement(OPC_FieldValue, field, Loc);
        break;
      case TYPE_Byte:
        ec.AddStatement(OPC_ByteFieldValue, field, Loc);
        break;
      case TYPE_Bool:
             if (field->Type.BitMask&0x000000ff) ec.AddStatement(OPC_Bool0FieldValue, field, (int)(field->Type.BitMask), Loc);
        else if (field->Type.BitMask&0x0000ff00) ec.AddStatement(OPC_Bool1FieldValue, field, (int)(field->Type.BitMask>>8), Loc);
        else if (field->Type.BitMask&0x00ff0000) ec.AddStatement(OPC_Bool2FieldValue, field, (int)(field->Type.BitMask>>16), Loc);
        else ec.AddStatement(OPC_Bool3FieldValue, field, (int)(field->Type.BitMask>>24), Loc);
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        ec.AddStatement(OPC_PtrFieldValue, field, Loc);
        break;
      case TYPE_Vector:
        ec.AddStatement(OPC_VFieldValue, field, Loc);
        break;
      case TYPE_String:
        ec.AddStatement(OPC_StrFieldValue, field, Loc);
        break;
      case TYPE_Delegate:
        ec.AddStatement(OPC_Offset, field, Loc);
        ec.AddStatement(OPC_PushPointedDelegate, Loc);
        break;
      case TYPE_SliceArray:
        ec.AddStatement(OPC_SliceFieldValue, field, Loc);
        break;
      default:
        ParseError(Loc, "Invalid operation on field of this type");
    }
  }
}


//==========================================================================
//
//  VFieldAccess::toString
//
//==========================================================================
VStr VFieldAccess::toString () const {
  return e2s(op)+"->"+(field ? VStr(field->Name) : e2s(nullptr));
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
VExpression *VDelegateVal::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) { ParseError(Loc, "delegate should be in class"); delete this; return nullptr; }
  bool wasError = false;
  if ((M->Flags&FUNC_Static) != 0) { wasError = true; ParseError(Loc, "delegate should not be static"); }
  if ((M->Flags&FUNC_VarArgs) != 0) { wasError = true; ParseError(Loc, "delegate should not be vararg"); }
  if ((M->Flags&FUNC_Iterator) != 0) { wasError = true; ParseError(Loc, "delegate should not be iterator"); }
  // sadly, `FUNC_NonVirtual` is not set yet, so use slower check
  //if (ec.SelfClass->isNonVirtualMethod(M->Name)) { wasError = true; ParseError(Loc, "delegate should not be final"); }
  if (wasError) { delete this; return nullptr; }
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
  // call class postload, so `FUNC_NonVirtual` will be set
  //ec.SelfClass->PostLoad();
  if (!M->Outer) { ParseError(Loc, "VDelegateVal::Emit: Method has no outer!"); return; }
  if (M->Outer->MemberType != MEMBER_Class) { ParseError(Loc, "VDelegateVal::Emit: Method outer is not a class!"); return; }
  M->Outer->PostLoad();
  //fprintf(stderr, "MT: %s (rf=%d)\n", *M->GetFullName(), (M->Flags&FUNC_NonVirtual ? 1 : 0));
  if (M->Flags&FUNC_NonVirtual) {
    ec.AddStatement(OPC_PushFunc, M, Loc);
  } else {
    ec.AddStatement(OPC_PushVFunc, M, Loc);
  }
}


//==========================================================================
//
//  VDelegateVal::toString
//
//==========================================================================
VStr VDelegateVal::toString () const {
  return VStr("&(")+e2s(op)+"."+(M ? VStr(M->Name) : e2s(nullptr))+")";
}
