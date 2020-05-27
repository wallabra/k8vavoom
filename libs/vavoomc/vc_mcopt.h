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
// OPTIMIZER
//**************************************************************************
#ifndef VCC_MC_OPTIMISER_HEADER
#define VCC_MC_OPTIMISER_HEADER

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
struct Instr;


// ////////////////////////////////////////////////////////////////////////// //
// main optimizer class (k8: well, `MC` stands for `machine code`, lol)
class VMCOptimizer {
friend struct Instr;

public:
  VMethod *func;
  TArray<FInstruction> *origInstrList;
  // instructions list
  Instr *ilistHead, *ilistTail;
  // all known jump instructions
  Instr *jplistHead, *jplistTail;
  int instrCount;
  // support list to ease indexed access
  TArray<Instr *> instrList;

private:
  Instr *getInstrAtSlow (int idx) const;

  Instr *getInstrAt (int idx) const;

  void disasmAll () const;

  void recalcJumpTargetCacheFor (Instr *it);

  void fixJumpTargetCache ();

  void appendToList (Instr *i);
  void appendToJPList (Instr *i);

  void removeInstr (Instr *it);
  void killInstr (Instr *it);

  //WARNING: copies contents of `src` to `dest`, but does no list reordering!
  void replaceInstr (Instr *dest, Instr *src);

  // range is inclusive
  bool canRemoveRange (int idx0, int idx1, Instr *ignoreThis=nullptr, Instr *ignoreThis1=nullptr);

  // range is inclusive
  void killRange (int idx0, int idx1);

  void traceReachable (int pc=0);

public:
  VMCOptimizer (VMethod *afunc, TArray<FInstruction> &aorig);
  ~VMCOptimizer ();

  void clear ();

  void setupFrom (VMethod *afunc, TArray<FInstruction> *aorig);

  // this will copy result back to `aorig`, and will clear everything
  void finish ();

  // this does flood-fill search to see if all execution pathes are finished with `return`
  void checkReturns ();

  void optimizeAll ();
  void shortenInstructions ();

  inline int countInstrs () const { return instrCount; }

protected:
  // returns `true` if this path (and all its possible branches) reached `return` instruction
  // basically, it just marks all reachable instructions, and fails if it reached end-of-function
  // note that we don't try to catch endless loops, but simple endless loops are considered ok
  // (due to an accident)
  bool isPathEndsWithReturn (int iidx);

  void optimizeLoads ();
  void optimizeJumps ();

  bool removeDeadBranches ();

  bool removeRedunantJumps ();
  bool simplifyIfJumpJump ();
  bool simplifyIfJumps ();

  void calcStackDepth ();
};


#endif
