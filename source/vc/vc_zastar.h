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

class VMiAStarGraphBase;
class VMiAStarNodeBase;


// ////////////////////////////////////////////////////////////////////////// //
class VMiAStarGraphIntr : micropather::Graph {
  friend class VMiAStarGraphBase;

private:
  VMiAStarGraphBase *vcifc;
  MP_VECTOR<void *> path;
  MP_VECTOR<micropather::StateCost> mNear;

  VMethod *mtLeastCostEstimate;
  VMethod *mtAdjacentCost;

public:
  VMiAStarGraphIntr (VMiAStarGraphBase *aifc);
  //virtual ~VMiAStarGraphIntr () override;

  /**
    Return the least possible cost between 2 states. For example, if your pathfinding
    is based on distance, this is simply the straight distance between 2 points on the
    map. If you pathfinding is based on minimum time, it is the minimal travel time
    between 2 points given the best possible terrain.
  */
  virtual float LeastCostEstimate (void *stateStart, void *stateEnd) override;

  /**
    Return the exact cost from the given state to all its neighboring states. This
    may be called multiple times, or cached by the solver. It *must* return the same
    exact values for every call to MicroPather::Solve(). It should generally be a simple,
    fast function with no callbacks into the pather.
  */
  virtual void AdjacentCost (void *state, MP_VECTOR<micropather::StateCost> *adjacent) override;

  /**
    This function is only used in MP_DEBUG mode - it dumps output to stdout. Since void*
    aren't really human readable, normally you print out some concise info (like "(1,2)")
    without an ending newline.
  */
  virtual void PrintStateInfo (void *state) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMiAStarGraphBase : public VObject {
  DECLARE_CLASS(VMiAStarGraphBase, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VMiAStarGraphBase)

  friend class VMiAStarGraphIntr;

private:
  VMiAStarGraphIntr *intr;
  micropather::MicroPather *pather;
  MP_VECTOR<micropather::StateCost> *adjarray;
  float pathCost; // valid after successfull solution

private:
  // default values for micropather
  vuint32 initParam_PoolSize;
  vuint32 initParam_TypicalAdjacent;
  vuint32 initParam_CachePathes; // bit0

private:
  void EnsureInterfaces ();

public:
  virtual void Destroy () override;

  // final bool IsInitialized ();
  DECLARE_FUNCTION(IsInitialized)

  // this is used in `AdjacentCost()`
  // final void PushAdjacentCost (MiAStarNodeBase state, float cost);
  DECLARE_FUNCTION(PushAdjacentCost)

  // float LeastCostEstimate (MiAStarNodeBase start, MiAStarNodeBase end); // abstract
  DECLARE_FUNCTION(LeastCostEstimate)

  // void AdjacentCost (MiAStarNodeBase state); // abstract
  DECLARE_FUNCTION(AdjacentCost)

  // final void PathArrayClear ();
  DECLARE_FUNCTION(PathArrayClear)
  // used to retrieve solved path after calling `Solve()`
  // final int PathArrayLength ();
  DECLARE_FUNCTION(PathArrayLength)
  // used to retrieve solved path after calling `Solve()`
  // final MiAStarNodeBase PathArrayNode (int index);
  DECLARE_FUNCTION(PathArrayNode)
  // force-push node to path array
  // final void PathArrayPushNode (MiAStarNodeBase node);
  DECLARE_FUNCTION(PathArrayPushNode)

  // final void NearArrayClear ();
  DECLARE_FUNCTION(NearArrayClear)
  // used to retrieve solved path after calling `SolveForNearStates()`
  // final int NearArrayLength ();
  DECLARE_FUNCTION(NearArrayLength)
  // used to retrieve solved path after calling `SolveForNearStates()`
  // final MiAStarNodeBase NearArrayNode (int index);
  DECLARE_FUNCTION(NearArrayNode)
  // used to retrieve solved path after calling `SolveForNearStates()`
  // final MiAStarNodeBase NearArrayNodeAndCost (int index, out float cost);
  DECLARE_FUNCTION(NearArrayNodeAndCost)
  // force-push node to near array
  // final void NearArrayPushNode (MiAStarNodeBase node, float cost);
  DECLARE_FUNCTION(NearArrayPushNode)

  /**
    Solve for the path from start to end.

    @param startState Input, the starting state for the path.
    @param endState   Input, the ending state for the path.
    @param path     Output, a vector of states that define the path. Empty if not found.
    @param totalCost  Output, the cost of the path, if found.
    @return       Success or failure, expressed as SOLVED, NO_SOLUTION, or START_END_SAME.

    if start and end states are the same, creates path from one state, with zero cost
  */
  // final bool Solve (MiAStarNodeBase startState, MiAStarNodeBase endState);
  DECLARE_FUNCTION(Solve)

  /**
    Find all the states within a given cost from startState.

    @param startState Input, the starting state for the path.
    @param near     All the states within 'maxCost' of 'startState', and cost to that state.
    @param maxCost    Input, the maximum cost that will be returned. (Higher values return
              larger 'near' sets and take more time to compute.)
    @return       Success or failure, expressed as SOLVED or NO_SOLUTION.
  */
  // final bool SolveForNearStates (MiAStarNodeBase startState, float maxCost);
  DECLARE_FUNCTION(SolveForNearStates)

  /** Should be called whenever the cost between states or the connection between states changes.
    Also frees overhead memory used by MicroPather, and calling will free excess memory.
  */
  // final void Reset ();
  DECLARE_FUNCTION(Reset)
};


class VMiAStarNodeBase : public VObject {
  DECLARE_CLASS(VMiAStarNodeBase, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VMiAStarNodeBase)
};
