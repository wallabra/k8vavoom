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
// this directly included from "vc_decorate.cpp"


//==========================================================================
//
//  VDecorateInvocation
//
//==========================================================================
class VDecorateInvocation : public VExpression {
public:
  VName Name;
  int NumArgs;
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VDecorateInvocation (VName, const TLocation &, int, VExpression **);
  virtual ~VDecorateInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VDecorateInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateUserVar
//
//  this generates call to _get/_set uservar methods
//
//==========================================================================
class VDecorateUserVar : public VExpression {
public:
  VName fldname;
  VExpression *index; // array index, if not nullptr

  VDecorateUserVar (VName afldname, const TLocation &aloc);
  VDecorateUserVar (VName afldname, VExpression *aindex, const TLocation &aloc);
  virtual ~VDecorateUserVar () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  //virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual bool IsDecorateUserVar () const override;
  virtual VStr toString () const override;

protected:
  VDecorateUserVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateInvocation::VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::VDecorateInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VExpression(ALoc)
  , Name(AName)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args));
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VDecorateInvocation::~VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::~VDecorateInvocation () {
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      delete Args[i];
      Args[i] = nullptr;
    }
  }
}


//==========================================================================
//
//  VDecorateInvocation::toString
//
//==========================================================================
VStr VDecorateInvocation::toString () const {
  VStr res = *Name;
  res += "(";
  for (int f = 0; f < NumArgs; ++f) {
    if (f != 0) res += ", ";
    if (Args[f]) res += Args[f]->toString(); else res += "default";
  }
  res += ")";
  return res;
}


//==========================================================================
//
//  VDecorateInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateInvocation::SyntaxCopy () {
  auto res = new VDecorateInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateInvocation::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateInvocation *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->Name = Name;
  res->NumArgs = NumArgs;
  for (int f = 0; f < NumArgs; ++f) res->Args[f] = (Args[f] ? Args[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateInvocation::DoResolve
//
//==========================================================================
VExpression *VDecorateInvocation::DoResolve (VEmitContext &ec) {
  guard(VDecorateInvocation::DoResolve);
  //if (VStr::ICmp(*Name, "CallACS") == 0) Name = VName("ACS_NamedExecuteWithResult"); // decorate hack
  if (ec.SelfClass) {
    //FIXME: sanitize this!
    // first try with decorate_ prefix, then without
    VMethod *M = ec.SelfClass->FindMethod/*NoCase*/(va("decorate_%s", *Name));
    if (!M) M = ec.SelfClass->FindMethod/*NoCase*/(Name);
    if (!M) {
      VStr loname = VStr(*Name).toLowerCase();
      M = ec.SelfClass->FindMethod/*NoCase*/(va("decorate_%s", *loname));
      if (!M) M = ec.SelfClass->FindMethod(VName(*loname));
      if (!M && ec.SelfClass) {
        // just in case
        VDecorateStateAction *Act = ec.SelfClass->FindDecorateStateAction(*loname);
        if (Act) M = Act->Method;
      }
      if (M) Name = VName(*loname);
    }
    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements (method '%s', class '%s')", *Name, *ec.SelfClass->GetFullName());
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (ec.SelfClass) {
    ParseError(Loc, "Unknown decorate action `%s` in class `%s`", *Name, *ec.SelfClass->GetFullName());
  } else {
    ParseError(Loc, "Unknown decorate action `%s`", *Name);
  }
  delete this;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VDecorateInvocation::Emit
//
//==========================================================================
void VDecorateInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VName afldname, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(nullptr)
{
}


//==========================================================================
//
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VName afldname, VExpression *aindex, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(aindex)
{
}


//==========================================================================
//
//  VDecorateUserVar::~VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::~VDecorateUserVar () {
  delete index; index = nullptr;
  fldname = NAME_None; // just4fun
}


//==========================================================================
//
//  VDecorateUserVar::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateUserVar::SyntaxCopy () {
  auto res = new VDecorateUserVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateUserVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateUserVar *)e;
  res->fldname = fldname;
  res->index = (index ? index->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateUserVar::DoResolve
//
//==========================================================================
VExpression *VDecorateUserVar::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  VName fldnamelo = (fldname == NAME_None ? fldname : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  // first try to find the corresponding non-array method
  if (!index) {
    VMethod *mt = ec.SelfClass->FindMethod(fldn);
    if (mt) {
      // use found method
      VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }
  // no method, use checked field access
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_get_user_var_int" : "_get_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VExpression *args[2];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 2, args);
  index = nullptr;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::ResolveCompleteAssign
//
//  this will be called before actual assign resolving
//  return `nullptr` to indicate error, or consume `val` and set `resolved`
//  to `true` if resolved
//  if `nullptr` is returned, both `this` and `val` should be destroyed
//
//==========================================================================
VExpression *VDecorateUserVar::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  if (!val) { delete this; return nullptr; }
  resolved = true; // anyway
  VName fldnamelo = (fldname == NAME_None ? fldname : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_set_user_var_int" : "_set_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VExpression *args[3];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = val;
  args[2] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 3, args);
  index = nullptr;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::Emit
//
//==========================================================================
void VDecorateUserVar::Emit (VEmitContext &ec) {
  Sys_Error("VDecorateUserVar::Emit: the thing that should not be!");
}


//==========================================================================
//
//  VDecorateUserVar::toString
//
//==========================================================================
VStr VDecorateUserVar::toString () const {
  VStr res = VStr(*fldname);
  if (index) { res += "["; res += index->toString(); res += "]"; }
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::IsDecorateUserVar
//
//==========================================================================
bool VDecorateUserVar::IsDecorateUserVar () const {
  return true;
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName (const VStr &AName, const TLocation &ALoc)
  : VExpression(ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName () {
}


//==========================================================================
//
//  VDecorateSingleName::toString
//
//==========================================================================
VStr VDecorateSingleName::toString () const {
  return VStr(*Name);
}


//==========================================================================
//
//  VDecorateSingleName::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateSingleName::SyntaxCopy () {
  auto res = new VDecorateSingleName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateSingleName::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateSingleName::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateSingleName *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VDecorateSingleName::DoResolve
//
//==========================================================================
VExpression *VDecorateSingleName::DoResolve (VEmitContext &ec) {
  guard(VDecorateSingleName::DoResolve);
  VName CheckName = va("decorate_%s", *Name.ToLower());
  if (ec.SelfClass) {
    // prefixed constant
    VConstant *Const = ec.SelfClass->FindConstant(CheckName);
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }

    // prefixed property
    VProperty *Prop = ec.SelfClass->FindProperty(CheckName);
    if (Prop) {
      if (!Prop->GetFunc) {
        ParseError(Loc, "Property `%s` cannot be read", *Name);
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }

  CheckName = *Name.ToLower();

  if (ec.SelfClass) {
    // non-prefixed checked field access
    VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(CheckName);
    if (fldn != NAME_None) {
      // checked field access
      VExpression *e = new VDecorateUserVar(VName(*Name), Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  // non-prefixed constant
  // look only for constants defined in DECORATE scripts
  VConstant *Const = ec.Package->FindConstant(CheckName);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "Illegal expression identifier `%s`", *Name);
  delete this;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VDecorateSingleName::Emit
//
//==========================================================================
void VDecorateSingleName::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  VDecorateSingleName::IsDecorateSingleName
//
//==========================================================================
bool VDecorateSingleName::IsDecorateSingleName () const {
  return true;
}
