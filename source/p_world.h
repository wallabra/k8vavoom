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
//
//  BLOCK MAP ITERATORS
//
//  For each line/thing in the given mapblock, call the passed PIT_*
//  function. If the function returns false, exit with false without checking
//  anything else.
//
//**************************************************************************


//==========================================================================
//
//  VBlockLinesIterator
//
//  The validcount flags are used to avoid checking lines that are marked in
// multiple mapblocks, so increment validcount before the first call to
// SV_BlockLinesIterator, then make one or more calls to it.
//
//==========================================================================
class VBlockLinesIterator {
private:
  VLevel *Level;
  line_t **LinePtr;
  polyblock_t *PolyLink;
  vint32 PolySegIdx;
  vint32 *List;

public:
  VBlockLinesIterator (VLevel *Level, int x, int y, line_t **);
  bool GetNext ();
};


//==========================================================================
//
//  VBlockThingsIterator
//
//==========================================================================
class VBlockThingsIterator {
private:
  VEntity *Ent;

public:
  VBlockThingsIterator (VLevel *Level, int x, int y) {
    if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) {
      Ent = nullptr;
    } else {
      Ent = Level->BlockLinks[y*Level->BlockMapWidth+x];
    }
  }

  inline operator bool () const { return !!Ent; }
  inline void operator ++ () { Ent = Ent->BlockMapNext; }
  inline VEntity *operator * () const { return Ent; }
  inline VEntity *operator -> () const { return Ent; }
};


//==========================================================================
//
//  VRadiusThingsIterator
//
//==========================================================================
class VRadiusThingsIterator : public VScriptIterator {
private:
  VThinker *Self;
  VEntity **EntPtr;
  VEntity *Ent;
  int x;
  int y;
  int xl;
  int xh;
  int yl;
  int yh;

public:
  VRadiusThingsIterator (VThinker *, VEntity **, TVec, float);
  virtual bool GetNext () override;
};


//==========================================================================
//
//  VPathTraverse
//
//  Traces a line from x1,y1 to x2,y2, calling the traverser function for
//  each. Returns true if the traverser function returns true for all lines.
//
//==========================================================================
class VPathTraverse : public VScriptIterator {
private:
  TArray<intercept_t> Intercepts;

  TPlane trace_plane;
  TVec trace_org;
  TVec trace_dest;
  TVec trace_delta;
  TVec trace_dir;
  float trace_len;

  int Count;
  intercept_t *In;
  intercept_t **InPtr;

public:
  VPathTraverse (VThinker *, intercept_t **, float, float, float, float, int);
  virtual bool GetNext () override;

private:
  void Init (VThinker *, float, float, float, float, int);
  bool AddLineIntercepts (VThinker *, int, int, bool);
  void AddThingIntercepts (VThinker *, int, int);
  intercept_t &NewIntercept (const float frac);
  void RemoveInterceptsAfter (const float frac); // >=
};
