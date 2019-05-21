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

class VTypeExpr;


// ////////////////////////////////////////////////////////////////////////// //
class VExpression {
public:
  struct AutoCopy {
  protected:
    VExpression *e;

  public:
    AutoCopy () : e(nullptr) {}
    AutoCopy (VExpression *ae) : e(ae ? ae->SyntaxCopy() : nullptr) {}
    ~AutoCopy () { delete e; e = nullptr; }
    inline bool isEmpty () const { return !!e; }
    // if you're used peeked autocopy, you can reset this one
    inline void reset () { e = nullptr; }
    // use `clear()` to delete `e`
    inline void clear () { delete e; e = nullptr; }
    // will not empty autocopy; USE WITH CARE!
    inline VExpression *peek () { return e; }
    // use `extract()` to extract `e` (and leave autocopy empty)
    inline VExpression *extract () { VExpression *res = e; e = nullptr; return res; }
    VExpression *SyntaxCopy () { return (e ? e->SyntaxCopy() : nullptr); }
    // delete current `e`, and remember `ae->SyntaxCopy()`
    inline void replaceSyntaxCopy (VExpression *ae) {
      if (!ae) { delete e; e = nullptr; return; }
      if (ae != e) {
        delete e;
        e = (ae ? ae->SyntaxCopy() : nullptr);
      } else {
        Sys_Error("VC: internal compiler error (AutoCopy::assignSyntaxCopy)");
      }
    }
    // delete current `e`, and remember `ae` (without syntax-copying)
    inline void replaceNoCopy (VExpression *ae) {
      if (!ae) { delete e; e = nullptr; return; }
      if (ae != e) {
        delete e;
        e = ae;
      } else {
        Sys_Error("VC: internal compiler error (AutoCopy::assignNoCopy)");
      }
    }

  private:
    AutoCopy (const AutoCopy &ac) = delete;
    AutoCopy &operator = (const AutoCopy &ac) = delete;
  };

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
  // this coerces ints to floats, and fixes `none`\`nullptr` type
  static void CoerceTypes (VExpression *&op1, VExpression *&op2, bool coerceNoneDelegate); // expression *MUST* be already resolved
  virtual void RequestAddressOf ();
  virtual void Emit (VEmitContext &ec) = 0;
  virtual void EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue);
  void EmitPushPointedCode (VFieldType type, VEmitContext &ec); // yeah, non-virtual
  // return non-nullptr to drop `VDropResult` node, and use the result directly
  // this is called on resolved node
  virtual VExpression *AddDropResult ();
  virtual bool IsValidTypeExpression () const;
  virtual bool IsIntConst () const;
  virtual bool IsFloatConst () const;
  virtual bool IsStrConst () const;
  virtual bool IsNameConst () const;
  virtual vint32 GetIntConst () const;
  virtual float GetFloatConst () const;
  virtual const VStr &GetStrConst (VPackage *) const;
  virtual VName GetNameConst () const;
  virtual bool IsNoneLiteral () const;
  virtual bool IsNoneDelegateLiteral () const;
  virtual bool IsNullLiteral () const;
  virtual bool IsDefaultObject () const;
  virtual bool IsPropertyAssign () const;
  virtual bool IsDynArraySetNum () const;
  virtual bool IsDecorateSingleName () const;
  virtual bool IsDecorateUserVar () const;
  virtual bool IsLocalVarDecl () const;
  virtual bool IsLocalVarExpr () const;
  virtual bool IsAssignExpr () const;
  virtual bool IsParens () const;
  virtual bool IsUnaryMath () const;
  virtual bool IsUnaryMutator () const;
  virtual bool IsBinaryMath () const;
  virtual bool IsBinaryLogical () const;
  virtual bool IsSingleName () const;
  virtual bool IsDoubleName () const;
  virtual bool IsDotField () const;
  virtual bool IsMarshallArg () const;
  virtual bool IsRefArg () const;
  virtual bool IsOutArg () const;
  virtual bool IsOptMarshallArg () const;
  virtual bool IsDefaultArg () const;
  virtual bool IsNamedArg () const;
  virtual VName GetArgName () const;
  virtual bool IsAnyInvocation () const;
  virtual bool IsLLInvocation () const; // is this `VInvocation`?
  virtual bool IsTypeExpr () const;
  virtual bool IsAutoTypeExpr () const;
  virtual bool IsSimpleType () const;
  virtual bool IsReferenceType () const;
  virtual bool IsClassType () const;
  virtual bool IsPointerType () const;
  virtual bool IsAnyArrayType () const;
  virtual bool IsDictType () const;
  virtual bool IsStaticArrayType () const;
  virtual bool IsDynamicArrayType () const;
  virtual bool IsSliceType () const;
  virtual bool IsDelegateType () const;
  virtual bool IsVectorCtor () const;
  virtual bool IsConstVectorCtor () const;

  virtual VStr toString () const;

  static inline VStr e2s (const VExpression *e) { return (e ? e->toString() : "<{null}>"); }

  // this will try to coerce some decorate argument to something sensible
  VExpression *MassageDecorateArg (VEmitContext &ec, VState *CallerState, const char *funcName,
                                   int argnum, const VFieldType &destType, const TLocation *aloc=nullptr);

  // checks if expression consists of only numeric literals
  // returns `ExprXXX`
  enum { ExprNotNum, ExprInt, ExprFloat };
  static int IsNumericLiteralExpr (VExpression *e);

  // this simplifies expression consists of only numeric literals
  // if it cannot simplify the expression, it returns the original one
  static VExpression *CalculateNumericLiteralExpr (VExpression *e);

  // this resolves one-char strings and names to int literals too
  VExpression *ResolveToIntLiteralEx (VEmitContext &ec, bool allowFloatTrunc=false);

  static void *operator new (size_t size);
  static void *operator new[] (size_t size);
  static void operator delete (void *p);
  static void operator delete[] (void *p);

  static void ReportLeaks ();

protected:
  // `e` should be of correct type
  virtual void DoSyntaxCopyTo (VExpression *e);
};
