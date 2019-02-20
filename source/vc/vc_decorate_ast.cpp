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
//  VDecorateAJump
//
//  as `A_Jump()` can have insane amounts of arguments, we'll generate
//  VM code directly instead
//
//==========================================================================
class VDecorateAJump : public VExpression {
private:
  VExpression *xstc; // bool(`XLevel.StateCall`) access expression
  VExpression *xass; // XLevel.StateCall->Result = false
  VExpression *crnd0; // first call to P_Random()
  VExpression *crnd1; // second call to P_Random()

public:
  VExpression *prob; // probability
  TArray<VExpression *> labels; // jump labels
  VState *CallerState;

  VDecorateAJump (const TLocation &aloc);
  virtual ~VDecorateAJump () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual VStr toString () const override;

protected:
  VDecorateAJump () {}
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



//==========================================================================
//
//  VDecorateAJump::VDecorateAJump
//
//==========================================================================
VDecorateAJump::VDecorateAJump (const TLocation &aloc)
  : VExpression(aloc)
  , xstc(nullptr)
  , xass(nullptr)
  , crnd0(nullptr)
  , crnd1(nullptr)
  , prob(nullptr)
  , labels()
  , CallerState(nullptr)
{
}


//==========================================================================
//
//  VDecorateAJump::~VDecorateAJump
//
//==========================================================================
VDecorateAJump::~VDecorateAJump () {
  delete xstc; xstc = nullptr;
  delete xass; xass = nullptr;
  delete crnd0; crnd0 = nullptr;
  delete crnd1; crnd1 = nullptr;
  delete prob; prob = nullptr;
  for (int f = labels.length()-1; f >= 0; --f) delete labels[f];
  labels.clear();
  CallerState = nullptr;
}


//==========================================================================
//
//  VDecorateAJump::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateAJump::SyntaxCopy () {
  auto res = new VDecorateAJump();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateAJump::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateAJump::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateAJump *)e;
  res->prob = (prob ? prob->SyntaxCopy() : nullptr);
  res->labels.setLength(labels.length());
  for (int f = 0; f < labels.length(); ++f) {
    res->labels[f] = (labels[f] ? labels[f]->SyntaxCopy() : nullptr);
  }
  res->CallerState = CallerState;
}


//==========================================================================
//
//  VDecorateAJump::DoResolve
//
//==========================================================================
VExpression *VDecorateAJump::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) Sys_Error("VDecorateAJump::DoResolve: internal compiler error");

  //AutoCopy probcopy(op);

  if (labels.length() > 255) {
    ParseError(Loc, "%d labels in `A_Jump` -- are you nuts?!", labels.length());
    delete this;
    return nullptr;
  }

  if (!prob) { delete this; return nullptr; }

  prob = prob->Resolve(ec);
  if (!prob) { delete this; return nullptr; }

  if (prob->Type.Type != TYPE_Int && prob->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`A_Jump` argument #1 should be integer, not `%s`", *prob->Type.GetName());
    delete this;
    return nullptr;
  }

  for (int lbidx = 0; lbidx < labels.length(); ++lbidx) {
    VExpression *lbl = labels[lbidx];

    if (!lbl) {
      ParseError(Loc, "`A_Jump` cannot have default arguments");
      delete this;
      return nullptr;
    }

    lbl = lbl->MassageDecorateArg(ec, CallerState, "A_Jump", lbidx+2, VFieldType(TYPE_State));
    if (!lbl) { delete this; return nullptr; } // some error

    lbl = new VCastOrInvocation("DoJump", Loc, 1, &lbl);
    lbl = new VDropResult(lbl);

    labels[lbidx] = lbl->Resolve(ec);
    if (!labels[lbidx]) { delete this; return nullptr; }
  }

  /* generate this code:
    if (XLevel.StateCall) XLevel.StateCall->Result = false;
     if (prob > 0) {
       if (prob > 255 || P_Random() < prob) {
         switch (P_Random()%label.length()) {
           case n: do_jump_to_label_n;
         }
       }
     }

     do it by allocate local array for labels, populate it, and generate code
     for checks and sets
   */

  // create `XLevel.StateCall` access expression
  {
    VExpression *xlvl = new VSingleName("XLevel", Loc);
    xstc = new VDotField(xlvl, "StateCall", Loc);
  }
  // XLevel.StateCall->Result
  VExpression *xres = new VPointerField(xstc->SyntaxCopy(), "Result", Loc);
  // XLevel.StateCall->Result = false
  xass = new VAssignment(VAssignment::Assign, xres, new VIntLiteral(0, Loc), Loc);
  // call to `P_Random()`
  crnd0 = new VCastOrInvocation("P_Random", Loc, 0, nullptr);
  crnd1 = crnd0->SyntaxCopy();

  // now resolve all generated expressions
  xstc = xstc->ResolveBoolean(ec);
  if (!xstc) { delete this; return nullptr; }

  xass = xass->Resolve(ec);
  if (!xass) { delete this; return nullptr; }

  crnd0 = crnd0->Resolve(ec);
  if (!crnd0) { delete this; return nullptr; }

  crnd1 = crnd1->Resolve(ec);
  if (!crnd1) { delete this; return nullptr; }

  if (crnd0->Type.Type != TYPE_Int && crnd0->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`P_Random()` should return integer, not `%s`", *crnd0->Type.GetName());
    delete this;
    return nullptr;
  }

  if (labels.length() == 0) {
    ParseWarning(Loc, "this `A_Jump` is never taken");
  } else if (labels.length() == 1 && prob->IsIntConst() && prob->GetIntConst() > 255) {
    ParseWarning(Loc, "this `A_Jump` is uncoditional; this is probably a bug (replace it with `Goto` if it isn't)");
  }

  Type.Type = TYPE_Void;

  return this;
}


//==========================================================================
//
//  VDecorateAJump::Emit
//
//==========================================================================
void VDecorateAJump::Emit (VEmitContext &ec) {
  /* generate this code:
     if (XLevel.StateCall) XLevel.StateCall->Result = false;
     if (prob <= 0) goto end;
     if (prob > 255) goto doit;
     if (P_Random() >= prob) goto end;
     doit:
       switch (P_Random()%label.length()) {
         case n: do_jump_to_label_n;
       }
     end:

     do it by allocate local array for labels, populate it, and generate code
     for checks and sets
   */

  //if (XLevel.StateCall) XLevel.StateCall->Result = false;
  VLabel falseTarget = ec.DefineLabel();
  // expression
  xstc->EmitBranchable(ec, falseTarget, false);
  // true statement
  xass->Emit(ec);
  ec.MarkLabel(falseTarget);

  VLabel endTarget = ec.DefineLabel();
  VLabel doitTarget = ec.DefineLabel();

  bool doDrop; // do we need to drop `prob` at the end?

  if (prob->IsIntConst() && prob->GetIntConst() < 0) {
    // jump is never taken
    doDrop = false;
    ec.AddStatement(OPC_Goto, endTarget, Loc);
  } else if (prob->IsIntConst() && prob->GetIntConst() > 255) {
    doDrop = false;
    // ...and check nothing, jump is always taken
  } else {
    doDrop = true;
    prob->Emit(ec); // prob

    if (!prob->IsIntConst()) {
      //if (prob <= 0) goto end;
      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
      ec.AddStatement(OPC_PushNumber, 0, Loc); // prob, prob, 0
      ec.AddStatement(OPC_LessEquals, Loc); // prob, flag
      ec.AddStatement(OPC_IfGoto, endTarget, Loc); // prob

      //if (prob > 255) goto doit;
      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
      ec.AddStatement(OPC_PushNumber, 255, Loc); // prob, prob, 255
      ec.AddStatement(OPC_GreaterEquals, Loc); // prob, flag
      ec.AddStatement(OPC_IfGoto, doitTarget, Loc); // prob

      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
    } else {
      // if we know prob value, we can omit most checks, and
      // we don't need to keep `prob` on the stack
      doDrop = false;
    }

    //if (P_Random() >= prob) goto end;
    //equals to: if (prob < P_Random()) goto end;
    crnd0->Emit(ec); // prob, prob, prand
    ec.AddStatement(OPC_Less, Loc); // prob, flag
    ec.AddStatement(OPC_IfGoto, endTarget, Loc); // prob
  }

  ec.MarkLabel(doitTarget); // prob

  if (labels.length() == 1) {
    labels[0]->Emit(ec); // prob
  } else if (labels.length() > 0) {
    // P_Random()%label.length()
    crnd1->Emit(ec); // prob, rnd
    ec.AddStatement(OPC_PushNumber, labels.length(), Loc); // prob, rnd, labels.length()
    ec.AddStatement(OPC_Modulus, Loc); // prob, lblidx
    // switch:
    TArray<VLabel> addrs;
    addrs.setLength(labels.length());
    for (int lidx = 0; lidx < labels.length(); ++lidx) addrs[lidx] = ec.DefineLabel();
    for (int lidx = 0; lidx < labels.length(); ++lidx) {
      if (lidx >= 0 && lidx < 256) {
        ec.AddStatement(OPC_CaseGotoB, lidx, addrs[lidx], Loc);
      } else if (lidx >= MIN_VINT16 && lidx < MAX_VINT16) {
        ec.AddStatement(OPC_CaseGotoS, lidx, addrs[lidx], Loc);
      } else {
        ec.AddStatement(OPC_CaseGoto, lidx, addrs[lidx], Loc);
      }
    }
    // just in case (and for optimiser)
    ec.AddStatement(OPC_DropPOD, Loc); // prob (lidx dropped)
    ec.AddStatement(OPC_Goto, endTarget, Loc); // prob

    // now generate label jump code
    for (int lidx = 0; lidx < labels.length(); ++lidx) {
      ec.MarkLabel(addrs[lidx]); // prob
      labels[lidx]->Emit(ec); // prob
      if (lidx != labels.length()-1) ec.AddStatement(OPC_Goto, endTarget, Loc); // prob
    }
  }

  ec.MarkLabel(endTarget); // prob
  if (doDrop) ec.AddStatement(OPC_DropPOD, Loc);
}


//==========================================================================
//
//  VDecorateAJump::toString
//
//==========================================================================
VStr VDecorateAJump::toString () const {
  VStr res = "A_Jump("+e2s(prob);
  for (int f = 0; f < labels.length(); ++f) {
    res += ", ";
    res += e2s(labels[f]);
  }
  res += ")";
  return res;
}
