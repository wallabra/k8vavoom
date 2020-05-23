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
#include <stdlib.h>
#include <string.h>
// we have to include it first, so map implementation will see our `GetTypeHash()`
#include "../../libs/core/hash/hashfunc.h"

struct __attribute__((packed)) VectorInfo {
  float xy[2];
  //unsigned aidx;
  unsigned lidx; // linedef index
  VectorInfo *next;

  inline bool operator == (const VectorInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) == 0); }
  inline bool operator != (const VectorInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) != 0); }
};
static_assert(sizeof(VectorInfo) == sizeof(float)*2+sizeof(unsigned)*1+sizeof(void *), "oops");

static inline vuint32 GetTypeHash (const VectorInfo &vi) { return joaatHashBuf(vi.xy, sizeof(vi.xy)); }


// ////////////////////////////////////////////////////////////////////////// //
#include "../gamedefs.h"


//==========================================================================
//
//  VLevel::BuildDecalsVVList
//
//  build v1 and v2 lists (for decals)
//
//==========================================================================
void VLevel::BuildDecalsVVList () {
  if (NumLines < 1) return; // just in case

  // build hashes and lists
  TMapNC<VectorInfo, unsigned> vmap; // value: index in in tarray
  TArray<VectorInfo> va;
  va.SetLength(NumLines*2);
  line_t *ld = Lines;
  for (unsigned i = 0; i < (unsigned)NumLines; ++i, ++ld) {
    ld->decalMark = 0;
    ld->v1linesCount = ld->v2linesCount = 0;
    ld->v1lines = ld->v2lines = nullptr;
    for (unsigned vn = 0; vn < 2; ++vn) {
      const unsigned aidx = i*2+vn;
      VectorInfo *vi = &va[aidx];
      const TVec *vertex = (vn == 0 ? ld->v1 : ld->v2);
      vi->xy[0] = vertex->x;
      vi->xy[1] = vertex->y;
      //vi->aidx = aidx;
      vi->lidx = i;
      vi->next = nullptr;
      auto vaidxp = vmap.find(*vi);
      if (vaidxp) {
        //vassert(*vaidxp != vi->aidx);
        VectorInfo *cv = &va[*vaidxp];
        while (cv->next) {
          //vassert(cv->aidx < aidx);
          if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(0)!");
          cv = cv->next;
        }
        if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(1)!");
        cv->next = vi;
      } else {
        vmap.put(*vi, /*vi->*/aidx);
      }
    }
  }

  TArray<line_t *> wklist;
  wklist.setLength(NumLines*2);

  TArray<vuint8> wkhit;
  wkhit.setLength(NumLines);

  // fill linedef lists
  ld = Lines;
  for (unsigned i = 0; i < (unsigned)NumLines; ++i, ++ld) {
    for (unsigned vn = 0; vn < 2; ++vn) {
      memset(wkhit.ptr(), 0, wkhit.length()*sizeof(wkhit.ptr()[0]));
      wkhit[i] = 1;

      unsigned count = 0;
      VectorInfo *vi = &va[i*2+vn];
      auto vaidxp = vmap.find(*vi);
      if (!vaidxp) Sys_Error("VLevel::BuildDecalsVVList: internal error (0)");
      VectorInfo *cv = &va[*vaidxp];
      while (cv) {
        if (!wkhit[cv->lidx]) {
          if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(2)!");
          wkhit[cv->lidx] = 1;
          wklist[count++] = &Lines[cv->lidx];
        }
        cv = cv->next;
      }

      if (count > 0) {
        line_t **list = new line_t *[count];
        memcpy(list, wklist.ptr(), count*sizeof(line_t *));
        if (vn == 0) {
          ld->v1linesCount = count;
          ld->v1lines = list;
        } else {
          ld->v2linesCount = count;
          ld->v2lines = list;
        }
      }
    }
  }
}


//==========================================================================
//
//  VLevel::PostProcessForDecals
//
//==========================================================================
void VLevel::PostProcessForDecals () {
  GCon->Logf(NAME_Dev, "postprocessing level for faster decals");

  for (auto &&line : allLines()) line.firstseg = nullptr;

  GCon->Logf(NAME_Dev, "postprocessing level for faster decals: assigning segs");
  // collect segments, so we won't go thru the all segs in decal spawner
  for (auto &&seg : allSegs()) {
    line_t *li = seg.linedef;
    if (!li) continue;
    //seg.lsnext = li->firstseg;
    //li->firstseg = &seg;
    seg_t *cs = li->firstseg;
    if (cs) {
      while (cs->lsnext) cs = cs->lsnext;
      cs->lsnext = &seg;
    } else {
      li->firstseg = &seg;
    }
  }
}
