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
  vassert(vcifc);
  vassert(mtLeastCostEstimate);
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
  vassert(vcifc);
  vassert(mtAdjacentCost);
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
  vobjGetParamSelf();
  if (!Self) { RET_BOOL(false); return; }
  RET_BOOL(!!Self->intr);
}


// this is used in `AdjacentCost()`
// final void PushAdjacentCost (MiAStarNodeBase state, float cost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PushAdjacentCost) {
  VMiAStarNodeBase *state;
  float cost;
  vobjGetParamSelf(state, cost);
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
  VMiAStarNodeBase *start, *end;
  vobjGetParamSelf(start, end);
  (void)end;
  (void)start;
  (void)Self;
  VObject::VMDumpCallStack();
  Sys_Error("unimplemented cost function MiAStarGraphBase::LeastCostEstimate");
}


// void AdjacentCost (MiAStarNodeBase state); // abstract
IMPLEMENT_FUNCTION(VMiAStarGraphBase, AdjacentCost) {
  VMiAStarNodeBase *state;
  vobjGetParamSelf(state);
  (void)state;
  (void)Self;
  VObject::VMDumpCallStack();
  Sys_Error("unimplemented cost function MiAStarGraphBase::AdjacentCost");
}


// final void PathArrayClear ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayClear) {
  vobjGetParamSelf();
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayClear"); }
  if (Self->intr) {
    Self->intr->path.clear();
  }
}

// final int PathArrayLength ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayLength) {
  vobjGetParamSelf();
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayLength"); }
  if (Self->intr) {
    RET_INT((int)Self->intr->path.size());
  } else {
    RET_INT(0);
  }
}

// final MiAStarNodeBase PathArrayNode (int index);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayNode) {
  int index;
  vobjGetParamSelf(index);
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

// final void PathArrayPushNode (MiAStarNodeBase node);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, PathArrayPushNode) {
  VMiAStarNodeBase *node;
  vobjGetParamSelf(node);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayNode"); }
  Self->EnsureInterfaces();
  vassert(Self->intr);
  vassert(Self->pather);
  Self->intr->path.push_back((void *)node);
}


// final void NearArrayClear ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayClear) {
  vobjGetParamSelf();
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayClear"); }
  if (Self->intr) {
    Self->intr->mNear.clear();
  }
}

// final int NearArrayLength ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayLength) {
  vobjGetParamSelf();
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayLength"); }
  if (Self->intr) {
    RET_INT((int)Self->intr->mNear.size());
  } else {
    RET_INT(0);
  }
}

// final MiAStarNodeBase NearArrayNode (int index);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayNode) {
  int index;
  vobjGetParamSelf(index);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayNode"); }
  if (Self->intr) {
    if (index < 0 || index >= (int)Self->intr->mNear.size()) { VObject::VMDumpCallStack(); Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNode, max is %d", index, (int)Self->intr->mNear.size()); }
    RET_PTR(Self->intr->mNear[(unsigned)index].state);
  } else {
    VObject::VMDumpCallStack();
    Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNode, length is 0", index);
    RET_PTR(nullptr);
  }
}

// final MiAStarNodeBase NearArrayNodeAndCost (int index, out float cost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayNodeAndCost) {
  int index;
  float *pcost;
  vobjGetParamSelf(index, pcost);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::NearArrayNodeAndCost"); }
  if (Self->intr) {
    if (index < 0 || index >= (int)Self->intr->mNear.size()) { VObject::VMDumpCallStack(); Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNodeAndCost, max is %d", index, (int)Self->intr->mNear.size()); }
    if (pcost) *pcost = Self->intr->mNear[(unsigned)index].cost;
    RET_PTR(Self->intr->mNear[(unsigned)index].state);
  } else {
    VObject::VMDumpCallStack();
    Sys_Error("invalid index %d in MiAStarGraphBase::NearArrayNodeAndCost, length is 0", index);
    RET_PTR(nullptr);
  }
}

// final void NearArrayPushNode (MiAStarNodeBase node, float cost);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, NearArrayPushNode) {
  VMiAStarNodeBase *node;
  float cost;
  vobjGetParamSelf(node, cost);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::PathArrayNode"); }
  Self->EnsureInterfaces();
  vassert(Self->intr);
  vassert(Self->pather);
  micropather::StateCost cst;
  cst.state = (void *)node;
  cst.cost = cost;
  Self->intr->mNear.push_back(cst);
}


// final int Solve (MiAStarNodeBase startState, MiAStarNodeBase endState);
IMPLEMENT_FUNCTION(VMiAStarGraphBase, Solve) {
  VMiAStarNodeBase *startState, *endState;
  vobjGetParamSelf(startState, endState);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::Solve"); }
  if (!startState || !endState) {
    if (Self->intr) Self->intr->path.clear();
    RET_BOOL(false);
    return;
  }
  Self->EnsureInterfaces();
  vassert(Self->intr);
  vassert(Self->pather);
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
  VMiAStarNodeBase *startState;
  float maxCost;
  vobjGetParamSelf(startState, maxCost);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::SolveForNearStates"); }
  if (!startState) {
    if (Self->intr) Self->intr->mNear.clear();
    RET_BOOL(false);
    return;
  }
  Self->EnsureInterfaces();
  vassert(Self->intr);
  vassert(Self->pather);
  Self->intr->mNear.clear();
  int res = Self->pather->SolveForNearStates(startState, &Self->intr->mNear, maxCost);
  if (res == micropather::MicroPather::SOLVED) {
    RET_BOOL(true);
    return;
  }
  if (res == micropather::MicroPather::NO_SOLUTION) {
    // just in case
    Self->intr->mNear.clear();
    RET_BOOL(false);
    return;
  }
  VObject::VMDumpCallStack();
  Sys_Error("Micropather returned something very strange!");
}

// final void Reset ();
IMPLEMENT_FUNCTION(VMiAStarGraphBase, Reset) {
  vobjGetParamSelf();
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in MiAStarGraphBase::Reset"); }
  if (Self->intr) {
    vassert(Self->pather);
    Self->pather->Reset();
  }
}
