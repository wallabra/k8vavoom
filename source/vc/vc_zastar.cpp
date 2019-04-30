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
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run.h"
# endif
#endif


// ////////////////////////////////////////////////////////////////////////// //
//VMiAStarGraphIntr::~VMiAStarGraphIntr () {}


VMiAStarGraphIntr::VMiAStarGraphIntr (VMiAStarGraphBase *aifc)
  : vcifc(aifc)
  , mtLeastCostEstimate(nullptr)
  , mtAdjacentCost(nullptr)
{
  mtLeastCostEstimate = aifc->GetClass()->FindMethod("LeastCostEstimate");
  mtAdjacentCost = aifc->GetClass()->FindMethod("AdjacentCost");
}


//==========================================================================
//
//  VMiAStarGraphIntr::LeastCostEstimate
//
//==========================================================================
float VMiAStarGraphIntr::LeastCostEstimate (void *stateStart, void *stateEnd) {
  check(vcifc);
  check(mtLeastCostEstimate);
  P_PASS_REF(vcifc);
  P_PASS_REF(stateStart);
  P_PASS_REF(stateEnd);
  return VObject::ExecuteFunction(mtLeastCostEstimate).getFloat();
}


//==========================================================================
//
//  VMiAStarGraphIntr::AdjacentCost
//
//==========================================================================
void VMiAStarGraphIntr::AdjacentCost (void *state, MP_VECTOR<micropather::StateCost> *adjacent) {
  check(vcifc);
  check(mtAdjacentCost);
  vcifc->adjarray = adjacent;
  try {
    P_PASS_REF(vcifc);
    P_PASS_REF(state);
    (void)VObject::ExecuteFunction(mtAdjacentCost);
  } catch (...) {
    vcifc->adjarray = nullptr;
    throw;
  }
  vcifc->adjarray = nullptr;
}


//==========================================================================
//
//  VMiAStarGraphIntr::PrintStateInfo
//
//==========================================================================
void VMiAStarGraphIntr::PrintStateInfo (void *state) {
}


// ////////////////////////////////////////////////////////////////////////// //
//  MiAStarGraphBase
// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, MiAStarGraphBase)


//==========================================================================
//
//  VMiAStarGraphBase::Destroy
//
//==========================================================================
void VMiAStarGraphBase::Destroy () {
  //GLog.Log("killing GraphBase");
  if (intr) { delete intr; intr = nullptr; }
  if (pather) { delete pather; pather = nullptr; }
  Super::Destroy();
}


//==========================================================================
//
//  VMiAStarGraphBase::EnsureInterfaces
//
//==========================================================================
void VMiAStarGraphBase::EnsureInterfaces () {
  if (!intr) intr = new VMiAStarGraphIntr(this);
  if (!pather) pather = new micropather::MicroPather(intr, (unsigned)initParam_PoolSize, (unsigned)initParam_TypicalAdjacent, !!(initParam_CachePathes&1));
}


// final bool IsInitialized ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, IsInitialized) {
  P_GET_SELF;
  if (!Self) { RET_BOOL(false); return; }
  RET_BOOL(!!Self->intr);
}


// this is used in `AdjacentCost()`
// final void PushAdjacentCost (MiAStarNodeBase state, float cost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PushAdjacentCost) {
  P_GET_FLOAT(cost);
  P_GET_REF(VMiAStarNodeBase, state);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PushAdjacentCost"); }
  if (!Self->adjarray) { VObject::VMDumpCallStack(); Sys_Error("trying to push cost outside of cost calculator in MiAStarGraphBase::PushAdjacentCost"); }
  if (!isFiniteF(cost)) { VObject::VMDumpCallStack(); Sys_Error("invalid cost value in MiAStarGraphBase::PushAdjacentCost"); }
  micropather::StateCost cst;
  cst.state = (void *)state;
  cst.cost = cost;
  Self->adjarray->push_back(cst);
}


// float LeastCostEstimate (MiAStarNodeBase start, MiAStarNodeBase end); // abstract
IMPLEMENT_FUNCTION(VMiAStarGraphBase, LeastCostEstimate) {
  P_GET_REF(VMiAStarNodeBase, end);
  P_GET_REF(VMiAStarNodeBase, start);
  P_GET_SELF;
  (void)end;
  (void)start;
  (void)Self;
  VObject::VMDumpCallStack();
  Sys_Error("unimplemented cost function MiAStarGraphBase::LeastCostEstimate");
}


// void AdjacentCost (MiAStarNodeBase state); // abstract
IMPLEMENT_FUNCTION(VMiAStarGraphBase, AdjacentCost) {
  P_GET_REF(VMiAStarNodeBase, state);
  P_GET_SELF;
  (void)state;
  (void)Self;
  VObject::VMDumpCallStack();
  Sys_Error("unimplemented cost function MiAStarGraphBase::AdjacentCost");
}


// final void PathArrayClear ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayClear) {
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayClear"); }
  if (Self->intr) {
    Self->intr->path.clear();
  }
}

// final int PathArrayLength ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayLength) {
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayLength"); }
  if (Self->intr) {
    RET_INT((int)Self->intr->path.size());
  } else {
    RET_INT(0);
  }
}

// final MiAStarNodeBase PathArrayNode (int index);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayNode) {
  P_GET_INT(index);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayNode"); }
  if (Self->intr) {
    if (index < 0 || index >= (int)Self->intr->path.size()) { VObject::VMDumpCallStack(); Sys_Error("invalid index %d in MiAStarGraphBase::PathArrayNode, length is %d", index, (int)Self->intr->path.size()); }
    RET_PTR(Self->intr->path[(unsigned)index]);
  } else {
    VObject::VMDumpCallStack();
    Sys_Error("invalid index %d in MiAStarGraphBase::PathArrayNode, length is 0", index);
    RET_PTR(nullptr);
  }
}

// final void NearArrayClear ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayClear) {
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayClear"); }
  if (Self->intr) {
    Self->intr->near.clear();
  }
}

// final int NearArrayLength ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayLength) {
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayLength"); }
  if (Self->intr) {
    RET_INT((int)Self->intr->near.size());
  } else {
    RET_INT(0);
  }
}

// final MiAStarNodeBase NearArrayNode (int index);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayNode) {
  P_GET_INT(index);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayNode"); }
  if (Self->intr) {
    if (index < 0 || index >= (int)Self->intr->near.size()) { VObject::VMDumpCallStack(); Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNode, max is %d", index, (int)Self->intr->near.size()); }
    RET_PTR(Self->intr->near[(unsigned)index].state);
  } else {
    VObject::VMDumpCallStack();
    Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNode, length is 0", index);
    RET_PTR(nullptr);
  }
}

// final MiAStarNodeBase NearArrayNodeAndCost (int index, out float cost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayNodeAndCost) {
  P_GET_REF(float, pcost);
  P_GET_INT(index);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayNodeAndCost"); }
  if (Self->intr) {
    if (index < 0 || index >= (int)Self->intr->near.size()) { VObject::VMDumpCallStack(); Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNodeAndCost, max is %d", index, (int)Self->intr->near.size()); }
    if (pcost) *pcost = Self->intr->near[(unsigned)index].cost;
    RET_PTR(Self->intr->near[(unsigned)index].state);
  } else {
    VObject::VMDumpCallStack();
    Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNodeAndCost, length is 0", index);
    RET_PTR(nullptr);
  }
}

// final int Solve (MiAStarNodeBase startState, MiAStarNodeBase endState);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, Solve) {
  P_GET_REF(VMiAStarNodeBase, endState);
  P_GET_REF(VMiAStarNodeBase, startState);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::Solve"); }
  if (!startState || !endState) {
    if (Self->intr) Self->intr->path.clear();
    RET_BOOL(false);
    return;
  }
  Self->EnsureInterfaces();
  check(Self->intr);
  check(Self->pather);
  Self->intr->path.clear();
  int res = Self->pather->Solve((void *)startState, (void *)endState, &Self->intr->path, &Self->pathCost);
  if (res == micropather::MicroPather::SOLVED) {
    RET_BOOL(true);
    return;
  }
  if (res == micropather::MicroPather::NO_SOLUTION) {
    // just in case
    Self->intr->path.clear();
    Self->pathCost = 0;
    RET_BOOL(false);
    return;
  }
  if (res == micropather::MicroPather::START_END_SAME) {
    // just in case
    Self->intr->path.clear();
    Self->pathCost = 0;
    Self->intr->path.push_back((void *)startState);
    RET_BOOL(true);
    return;
  }
  VObject::VMDumpCallStack();
  Sys_Error("Micropather returned something very strange!");
}

// int SolveForNearStates (MiAStarNodeBase startState, float maxCost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, SolveForNearStates) {
  P_GET_FLOAT(maxCost);
  P_GET_REF(VMiAStarNodeBase, startState);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::SolveForNearStates"); }
  if (!startState) {
    if (Self->intr) Self->intr->near.clear();
    RET_BOOL(false);
    return;
  }
  Self->EnsureInterfaces();
  check(Self->intr);
  check(Self->pather);
  Self->intr->near.clear();
  int res = Self->pather->SolveForNearStates(startState, &Self->intr->near, maxCost);
  if (res == micropather::MicroPather::SOLVED) {
    RET_BOOL(true);
    return;
  }
  if (res == micropather::MicroPather::NO_SOLUTION) {
    // just in case
    Self->intr->near.clear();
    RET_BOOL(false);
    return;
  }
  VObject::VMDumpCallStack();
  Sys_Error("Micropather returned something very strange!");
}

// final void Reset ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, Reset) {
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::Reset"); }
  if (Self->intr) {
    check(Self->pather);
    Self->pather->Reset();
  }
}
