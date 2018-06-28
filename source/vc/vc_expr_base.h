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

class VTypeExpr;


// ////////////////////////////////////////////////////////////////////////// //
class VExpression {
public:
  VFieldType Type;
  VFieldType RealType;
  int Flags;
  TLocation Loc;
  static vuint32 TotalMemoryUsed;
  static vuint32 CurrMemoryUsed;
  static vuint32 PeakMemoryUsed;
  static vuint32 TotalMemoryFreed;
  static bool InCompilerCleanup;

protected:
  VExpression () {} // used in SyntaxCopy

public:
  VExpression (const TLocation &ALoc);
  virtual ~VExpression ();
  virtual VExpression *SyntaxCopy () = 0; // this should be called on *UNRESOLVED* expression
  virtual VExpression *DoResolve (VEmitContext &ec) = 0; // tho shon't call this twice, neither thrice!
  VExpression *Resolve (VEmitContext &ec); // this will usually just call `DoResolve()`
  VExpression *ResolveBoolean (VEmitContext &ec); // actually, *TO* boolean
  VExpression *ResolveFloat (VEmitContext &ec); // actually, *TO* float
  VExpression *CoerceToFloat (); // expression *MUST* be already resolved
  virtual VTypeExpr *ResolveAsType (VEmitContext &ec);
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec);
  virtual VExpression *ResolveAssignmentValue (VEmitContext &ec);
  virtual VExpression *ResolveIterator (VEmitContext &ec);
  // this will be called before actual assign resolving
  // return `nullptr` to indicate error, or consume `val` and set `resolved` to `true` if resolved
  // if `nullptr` is returned, both `this` and `val` should be destroyed
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved);
  virtual void RequestAddressOf ();
  virtual void Emit (VEmitContext &ec) = 0;
  virtual void EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue);
  void EmitPushPointedCode (VFieldType type, VEmitContext &ec); // yeah, non-virtual
  virtual bool IsValidTypeExpression () const;
  virtual bool IsIntConst () const;
  virtual bool IsFloatConst () const;
  virtual bool IsStrConst () const;
  virtual bool IsNameConst () const;
  virtual vint32 GetIntConst () const;
  virtual float GetFloatConst () const;
  virtual VStr GetStrConst (VPackage *) const;
  virtual VName GetNameConst () const;
  virtual bool IsNoneLiteral () const;
  virtual bool IsNullLiteral () const;
  virtual bool IsDefaultObject () const;
  virtual bool IsPropertyAssign () const;
  virtual bool IsDynArraySetNum () const;
  virtual bool AddDropResult ();
  virtual bool IsDecorateSingleName () const;
  virtual bool IsLocalVarDecl () const;
  virtual bool IsLocalVarExpr () const;
  virtual bool IsAssignExpr () const;
  virtual bool IsBinaryMath () const;
  virtual bool IsSingleName () const;
  virtual bool IsDoubleName () const;
  virtual bool IsDotField () const;
  virtual bool IsMarshallArg () const;
  virtual bool IsRefArg () const;
  virtual bool IsOutArg () const;
  virtual bool IsOptMarshallArg () const;
  virtual bool IsAnyInvocation () const;
  virtual bool IsLLInvocation () const; // is this `VInvocation`?
  virtual bool IsTypeExpr () const;
  virtual bool IsAutoTypeExpr () const;
  virtual bool IsSimpleType () const;
  virtual bool IsReferenceType () const;
  virtual bool IsClassType () const;
  virtual bool IsPointerType () const;
  virtual bool IsAnyArrayType () const;
  virtual bool IsStaticArrayType () const;
  virtual bool IsDynamicArrayType () const;
  virtual bool IsSliceType () const;
  virtual bool IsDelegateType () const;

  // this resolves one-char strings and names to int literals too
  VExpression *ResolveToIntLiteralEx (VEmitContext &ec, bool allowFloatTrunc=false);

  static void *operator new (size_t size);
  static void *operator new[] (size_t size);
  static void operator delete (void *p);
  static void operator delete[] (void *p);

protected:
  // `e` should be of correct type
  virtual void DoSyntaxCopyTo (VExpression *e);
};
