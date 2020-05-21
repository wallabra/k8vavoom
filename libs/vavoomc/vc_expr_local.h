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

//==========================================================================
//
//  VLocalEntry
//
//==========================================================================
class VLocalEntry {
public:
  VExpression *TypeExpr;
  VName Name;
  TLocation Loc;
  VExpression *Value;
  VExpression *TypeOfExpr; // for automatic vars w/o initializer, resolve this, and use its type
  bool isRef; // for range foreach
  bool isConst; // for range foreach
  int toeIterArgN; // >=0: `TypeOfExpr` is iterator call, take nth arg
  //bool emitClear;
  int locIdx;
  bool ctorInit; // `true` if `Value` is constructor call

  VLocalEntry () : TypeExpr(nullptr), Name(NAME_None), Value(nullptr), TypeOfExpr(nullptr), isRef(false), isConst(false), toeIterArgN(-1), /*emitClear(false),*/ locIdx(-1), ctorInit(false) {}
};


//==========================================================================
//
//  VLocalDecl
//
//==========================================================================
class VLocalDecl : public VExpression {
public:
  TArray<VLocalEntry> Vars;

  VLocalDecl (const TLocation &);
  virtual ~VLocalDecl () override;

  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;

  // this emits initialisations (always zeroes)
  virtual void Emit (VEmitContext &ec) override;

  // this registers new local, but doesn't allocate stack space yet
  // used in code resolver
  bool Declare (VEmitContext &ec);

  // hide all declared locals
  void Hide (VEmitContext &ec);

  // doesn't zero, used in code emiter
  void Allocate (VEmitContext &ec);
  // doesn't zero, doesn't call dtors, used in code emiter
  void Release (VEmitContext &ec);

  // this emits all init expressions
  // it also zeroes vars without initialisers if `inloop` is `true`
  // if we are in some loop, we need to zero loop vars on each iteration
  void EmitInitialisations (VEmitContext &ec, bool inloop);
  void EmitDtors (VEmitContext &ec);

  virtual bool IsLocalVarDecl () const override;

  virtual VStr toString () const override;

protected:
  VLocalDecl () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VLocalVar
//
//==========================================================================
class VLocalVar : public VExpression {
public:
  int num;
  bool AddressRequested;
  bool PushOutParam;
  vuint32 locSavedFlags; // local reusing can replace 'em

  VLocalVar (int ANum, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsLocalVarExpr () const override;

  virtual VStr toString () const override;

protected:
  VLocalVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  //void genLocalValue (VEmitContext &ec, const VLocalVarDef &loc, int xofs=0);
};
