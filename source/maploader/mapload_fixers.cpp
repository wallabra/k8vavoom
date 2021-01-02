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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"

#define DEBUG_DEEP_WATERS


static VCvarB ldr_fix_udmf("ldr_fix_udmf", false, "Apply fixers for UDMF maps?", CVAR_Archive);

static VCvarB dbg_deep_water("dbg_deep_water", false, "Show debug messages in Deep Water processor?", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarB dbg_floodfill_fixer("dbg_floodfill_fixer", false, "Show debug messages from floodfill fixer?", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarB deepwater_hacks("deepwater_hacks", true, "Apply self-referenced deepwater hacks?", CVAR_Archive);
static VCvarB deepwater_hacks_floor("deepwater_hacks_floor", true, "Apply deepwater hacks to fix some map errors?", CVAR_Archive);
static VCvarB deepwater_hacks_ceiling("deepwater_hacks_ceiling", true, "Apply deepwater hacks to fix some map errors?", CVAR_Archive);
static VCvarB deepwater_hacks_bridges("deepwater_hacks_bridges", true, "Apply hack for \"handing bridges\"?", CVAR_Archive);

static VCvarB ldr_fix_slope_cracks("ldr_fix_slope_cracks", true, "Try to fix empty cracks near sloped floors?", /*CVAR_Archive|*/CVAR_PreInit);
static VCvarB ldr_fix_transparent_doors("ldr_fix_transparent_doors", false, "Try to fix transparent doors?", CVAR_Archive);


//==========================================================================
//
//  NeedUDMFFix
//
//==========================================================================
static bool NeedUDMFFix (VLevel *level) noexcept {
  if (ldr_fix_udmf) return true;
  if (!(level->LevelFlags&VLevel::LF_TextMap)) return true;
  if (!(level->LevelFlags&VLevel::LF_Extended)) return true;
  return false; // skip it
}


//==========================================================================
//
//  VLevel::DetectHiddenSectors
//
//==========================================================================
void VLevel::DetectHiddenSectors () {
  /* detect hidden sectors
     to do this, we have to check if all sector "not on automap" lines are one-sided.
     this doesn't do a best job possible, but should be good enough for most cases, and it is cheap.
   */
  for (auto &&sec : allSectors()) {
    if (sec.SectorFlags&sector_t::SF_Hidden) continue; // this may come from UDMF
    if (sec.linecount < 1) continue;

    if ((sec.SectorFlags&(sector_t::SF_HasExtrafloors|sector_t::SF_ExtrafloorSource|sector_t::SF_TransferSource|sector_t::SF_UnderWater))) {
      if (!(sec.SectorFlags&sector_t::SF_IgnoreHeightSec)) continue; // this is special sector, skip it
    }

    bool seenBadLine = false;
    int count2s = 0;
    vuint32 seenHidden = 0;
    line_t **lptr = sec.lines;
    for (int cnt = sec.linecount; cnt--; ++lptr) {
      const line_t *ldef = *lptr;
      seenHidden |= ldef->flags;
      if (ldef->flags&ML_TWOSIDED) {
        ++count2s;
        if (count2s > 1) { seenBadLine = true; break; } // if it has more than one 2s visible line, it doesn't look like a secret
        continue; // we're not interested
      }
      // one-sided, and not hidden? this sector is not hidden
      if ((ldef->flags&ML_DONTDRAW) == 0) { seenBadLine = true; break; }
    }

    if (!seenBadLine && (seenHidden&ML_DONTDRAW)) {
      GCon->Logf("sector #%d is detected as hidden", (int)(ptrdiff_t)(&sec-&Sectors[0]));
      sec.SectorFlags |= sector_t::SF_Hidden;
    }
  }


#if 0
  // fix missing top/bottom textures, so there won't be empty cracks
  if (!ldr_fix_slope_cracks) return;

  for (auto &&sec : allSectors()) {
    if ((sec.SectorFlags&sector_t::SF_HasExtrafloors) == 0) continue;
    //GCon->Logf(NAME_Debug, "FIXING SLOPED SECTOR #%d", (int)(ptrdiff_t)(&sec-&Sectors[0]));
    for (sec_region_t *reg = sec.eregions; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_SaneRegion)) continue;
      if (!reg->extraline) continue;
      GCon->Logf(NAME_Debug, "FIXING SLOPED SECTOR #%d: es0=%d; es1=%d", (int)(ptrdiff_t)(&sec-&Sectors[0]),
        (reg->extraline->frontsector ? (int)(ptrdiff_t)(reg->extraline->frontsector-&Sectors[0]) : -1),
        (reg->extraline->backsector ? (int)(ptrdiff_t)(reg->extraline->backsector-&Sectors[0]) : -1));
      sector_t *sc2 = reg->extraline->frontsector;
      //RF_SkipFloorSurf = 1u<<3, // do not create floor surface for this region
      //RF_SkipCeilSurf  = 1u<<4, // do not create ceiling surface for this region
      if (!sc2) continue;
      if (!sc2->floor.isSlope() && !sc2->ceiling.isSlope()) continue;
      GCon->Logf(NAME_Debug, "  FIXING SLOPED SECTOR #%d", (int)(ptrdiff_t)(sc2-&Sectors[0]));
      // 474
      // 475
      //sc2 = &sec;
      line_t **lptr = sc2->lines;
      for (int cnt = sc2->linecount; cnt--; ++lptr) {
        const line_t *ldef = *lptr;
        for (int snum = 0; snum < 2; ++snum) {
          if (ldef->sidenum[snum] >= 0) {
            side_t *sd = &Sides[ldef->sidenum[snum]];
            // floor
            if (/*sec.floor.isSlope() &&*/ sd->BottomTexture <= 0) {
              sd->BottomTexture = sc2->floor.pic;
              GCon->Logf(NAME_Debug, "FIXING SLOPED SECTOR #%d (bot)", (int)(ptrdiff_t)(&sec-&Sectors[0]));
            }
            // ceiling
            if (/*sec.ceiling.isSlope() &&*/ sd->TopTexture <= 0) {
              sd->TopTexture = sc2->floor.pic;
              GCon->Logf(NAME_Debug, "FIXING SLOPED SECTOR #%d (top)", (int)(ptrdiff_t)(&sec-&Sectors[0]));
            }
          }
        }
      }
    }
  }
#endif
}


//==========================================================================
//
//  VLevel::FixTransparentDoors
//
//  this tries to mark all sectors with "transparent door hack"
//  it also creates fake floors and ceilings for them
//
//==========================================================================
void VLevel::FixTransparentDoors () {
  if (!ldr_fix_transparent_doors) return;
  if (!NeedUDMFFix(this)) { GCon->Logf(NAME_Debug, "FixTransparentDoors: skipped due to UDMF map"); return; }

  // mark all sectors with transparent door hacks
  for (auto &&sec : allSectors()) {
    sec.SectorFlags &= ~sector_t::SF_IsTransDoor;
    if (sec.linecount < 3) continue;

    if ((sec.SectorFlags&(sector_t::SF_HasExtrafloors|sector_t::SF_ExtrafloorSource|sector_t::SF_TransferSource|sector_t::SF_UnderWater))) {
      if (!(sec.SectorFlags&sector_t::SF_IgnoreHeightSec)) continue; // this is special sector, skip it
    }

    // never do this for slopes
    if (sec.floor.isSlope() || sec.ceiling.isSlope()) continue;

    bool doorFlag = false;
    for (int f = 0; f < sec.linecount; ++f) {
      const line_t *ldef = sec.lines[f];

      // mark only back sectors (the usual door setup)
      if (ldef->backsector != &sec) continue;
      if (ldef->sidenum[0] < 0) continue; // sector-less control line (usually)

      if ((ldef->flags&ML_ADDITIVE) != 0 || ldef->alpha < 1.0f) continue; // skip translucent walls
      if (!(ldef->flags&ML_TWOSIDED)) continue; // one-sided wall always blocks everything
      if (ldef->flags&ML_3DMIDTEX) continue; // 3dmidtex never blocks anything

      const sector_t *fsec = ldef->frontsector;
      const sector_t *bsec = ldef->backsector;

      if (fsec == bsec) continue; // self-referenced sector
      if (!fsec || !bsec) continue; // one-sided

      const TVec vv1 = *ldef->v1;
      const TVec vv2 = *ldef->v2;

      const float secfrontcz1 = fsec->ceiling.GetPointZ(vv1);
      const float secfrontcz2 = fsec->ceiling.GetPointZ(vv2);
      const float secfrontfz1 = fsec->floor.GetPointZ(vv1);
      const float secfrontfz2 = fsec->floor.GetPointZ(vv2);

      const float secbackcz1 = bsec->ceiling.GetPointZ(vv1);
      const float secbackcz2 = bsec->ceiling.GetPointZ(vv2);
      const float secbackfz1 = bsec->floor.GetPointZ(vv1);
      const float secbackfz2 = bsec->floor.GetPointZ(vv2);

      bool flag = false;
      // if front sector is not closed, check it
      if (secfrontfz1 < secfrontcz1 || secfrontfz2 < secfrontcz2) {
        // front sector is not closed, check if it can see top/bottom textures
        // to see a bottom texture, front sector floor must be lower than back sector floor
        if (secfrontfz1 < secbackfz1 || secfrontfz2 < secbackfz2) {
          // it can see a bottom texture, check if it is solid
          if (GTextureManager.GetTextureType(Sides[ldef->sidenum[0]].BottomTexture) != VTextureManager::TCT_SOLID) {
            flag = true;
          }
        }
        // to see a top texture, front sector ceiling must be higher than back sector ceiling
        if (secfrontcz1 > secbackcz1 || secfrontcz2 > secbackcz2) {
          // it can see a top texture, check if it is solid
          doorFlag = true;
          if (GTextureManager.GetTextureType(Sides[ldef->sidenum[0]].TopTexture) != VTextureManager::TCT_SOLID) {
            flag = true;
          }
        }
      }
      if (!flag) continue;

      sec.SectorFlags |= sector_t::SF_IsTransDoor;
      GCon->Logf(NAME_Debug, "sector #%d is transdoor", (int)(ptrdiff_t)(&sec-&Sectors[0]));
      break;
    }

    // create fake ceilings for transparent doors
    if (sec.fakefloors || (sec.SectorFlags&sector_t::SF_IsTransDoor) == 0) continue;

    // check if this is a door
    //if (!doorFlag) continue;

    // ignore sectors with skies (for now)
    if (sec.floor.pic == skyflatnum || sec.ceiling.pic == skyflatnum) continue;

    // ignore sector with invalid ceiling texture
    //if (GTextureManager.GetTextureType(sec.ceiling.pic) != VTextureManager::TCT_SOLID) continue;

    // find lowest surrounding sector
    const sector_t *lowsec = nullptr; // lowest ceiling
    const sector_t *highsec = nullptr; // highest floor
    for (int f = 0; f < sec.linecount; ++f) {
      const line_t *ldef = sec.lines[f];
      if (!(ldef->flags&ML_TWOSIDED)) continue; // one-sided wall always blocks everything
      const sector_t *ss = (ldef->frontsector == &sec ? ldef->backsector : ldef->frontsector);
      if (ss == &sec) continue;
      // ignore sloped sectors
      //if (ss->floor.isSlope() || ss->ceiling.isSlope()) continue;
      if (doorFlag) {
        if (ss->ceiling.minz > sec.ceiling.minz && !ss->ceiling.isSlope()) {
          if (!lowsec || lowsec->ceiling.minz > ss->ceiling.minz) lowsec = ss;
        }
      } else {
        if (ss->floor.minz < sec.floor.minz && !ss->floor.isSlope()) {
          if (!highsec || highsec->floor.minz < ss->floor.minz) highsec = ss;
        }
      }
    }

    if (doorFlag) {
      if (!lowsec) continue;
    } else {
      if (!highsec) continue;
    }

    // allocate fakefloor data (engine require it to complete setup)
    GCon->Logf(NAME_Debug, "sector #%d got transdoor flat fix", (int)(ptrdiff_t)(&sec-&Sectors[0]));
    vassert(!sec.fakefloors);
    sec.fakefloors = new fakefloor_t;
    fakefloor_t *ff = sec.fakefloors;
    memset((void *)ff, 0, sizeof(fakefloor_t));
    ff->flags |= fakefloor_t::FLAG_CreatedByLoader;
    ff->floorplane = sec.floor;
    ff->ceilplane = sec.ceiling;
    if (doorFlag) {
      ff->floorplane = sec.floor;
      ff->ceilplane = lowsec->ceiling;
      ff->params = lowsec->params;
    } else {
      ff->floorplane = highsec->floor;
      ff->ceilplane = sec.ceiling;
      ff->params = highsec->params;
    }
  }
}


//==========================================================================
//
// VLevel::FixSelfRefDeepWater
//
// This code was taken from Hyper3dge
//
//==========================================================================
void VLevel::FixSelfRefDeepWater () {
  // in UDMF maps, there is no sense to do "classic deepwater hack", so don't bother
  // but don't exit, because of converted doom->udmf maps, for example
  if (!NeedUDMFFix(this)) { GCon->Logf(NAME_Debug, "FixSelfRefDeepWater: skipped due to UDMF map"); return; }

  TArray<vuint8> self_subs;
  self_subs.setLength(NumSubsectors);
  memset(self_subs.ptr(), 0, NumSubsectors);

  for (int i = 0; i < NumSegs; ++i) {
    const seg_t *seg = &Segs[i];

    if (!seg->linedef) continue; // miniseg?
    if (!seg->frontsub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: FRONT SUBSECTOR IS NOT SET!"); return; }
    if (seg->frontsub->sector->linecount == 0) continue; // original polyobj sector

    const int fsnum = (int)(ptrdiff_t)(seg->frontsub-Subsectors);

    sector_t *fs = seg->linedef->frontsector;
    sector_t *bs = seg->linedef->backsector;

    // slopes aren't interesting
    if (bs && fs == bs &&
        (fs->SectorFlags&sector_t::SF_ExtrafloorSource) == 0 &&
        fs->floor.normal.z == 1.0f /*&& fs->ceiling.normal.z == -1.0f*/ &&
        bs->floor.normal.z == 1.0f /*&& bs->ceiling.normal.z == -1.0f*/)
    {
      self_subs[fsnum] |= 1;
    } else {
      self_subs[fsnum] |= 2;
    }
  }

  for (int pass = 0; pass < 100; ++pass) {
    int count = 0;

    for (int j = 0; j < NumSubsectors; ++j) {
      subsector_t *sub = &Subsectors[j];
      if (sub->numlines < 3) continue; // incomplete subsector

      seg_t *seg;

      if (self_subs[j] != 1) continue;
#ifdef DEBUG_DEEP_WATERS
      if (dbg_deep_water) GCon->Logf("Subsector [%d] sec %d --> %d", j, (int)(sub->sector-Sectors), self_subs[j]);
#endif
      seg_t *Xseg = 0;

      for (int ssi = 0; ssi < sub->numlines; ++ssi) {
        // this is how back_sub set
        seg = &Segs[sub->firstline+ssi];
        subsector_t *back_sub = (seg->partner ? seg->partner->frontsub : nullptr);
        if (!back_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: BACK SUBSECTOR IS NOT SET!"); return; }

        int k = (int)(back_sub-Subsectors);
#ifdef DEBUG_DEEP_WATERS
        if (dbg_deep_water) GCon->Logf("  Seg [%d] back_sub %d (back_sect %d)", (int)(seg-Segs), k, (int)(back_sub->sector-Sectors));
#endif
        if (self_subs[k]&2) {
          if (!Xseg) Xseg = seg;
        }
      }

      if (Xseg) {
        //sub->deep_ref = Xseg->back_sub->deep_ref ? Xseg->back_sub->deep_ref : Xseg->back_sub->sector;
        subsector_t *Xback_sub = (Xseg->partner ? Xseg->partner->frontsub : nullptr);
        if (!Xback_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: BACK SUBSECTOR IS NOT SET!"); return; }
        sub->deepref = (Xback_sub->deepref ? Xback_sub->deepref : Xback_sub->sector);
#ifdef DEBUG_DEEP_WATERS
        if (dbg_deep_water) GCon->Logf("  Updating (from seg %d) --> SEC %d", (int)(Xseg-Segs), (int)(sub->deepref-Sectors));
#endif
        self_subs[j] = 3;

        ++count;
      }
    }

    if (count == 0) break;
  }

  for (int i = 0; i < NumSubsectors; ++i) {
    subsector_t *sub = &Subsectors[i];
    if (sub->numlines < 3) continue; // incomplete subsector
    sector_t *hs = sub->deepref;
    if (!hs) continue;
    sector_t *ss = sub->sector;
    // if deepref is the same as the source sector, this is pointless
    if (hs == ss) { sub->deepref = nullptr; continue; }
    if (!ss) { if (dbg_deep_water) GCon->Logf("WTF(0)?!"); continue; }
    if (ss->deepref) {
      if (ss->deepref != hs) { if (dbg_deep_water) GCon->Logf("WTF(1) %d : %d?!", (int)(hs-Sectors), (int)(ss->deepref-Sectors)); continue; }
    } else {
      ss->deepref = hs;
    }
    // also, add deepref PVS info to the current one
    // this is unoptimal, but required for zdbsp, until
    // i'll fix PVS calculations for it.
    // the easiest way to see why it is required is to
    // comment this out, and do:
    //   -tnt +map map02 +notarget +noclip +warpto 866 1660
    // you will immediately see rendering glitches.
    // this also affects sight calculations, 'cause PVS is
    // used for fast rejects there.
    if (VisData && GetNodesBuilder() == BSP_ZD) {
      vuint8 *vis = VisData+(((NumSubsectors+7)>>3)*i);
      for (subsector_t *s2 = hs->subsectors; s2; s2 = s2->seclink) {
        if (s2 == sub) continue; // just in case
        // or vis data
        vuint8 *v2 = VisData+(((NumSubsectors+7)>>3)*((int)(ptrdiff_t)(s2-Subsectors)));
        vuint8 *dest = vis;
        for (int cc = (NumSubsectors+7)/8; cc > 0; --cc, ++dest, ++v2) *dest |= *v2;
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  FFBugFloor   = 0x01u<<0,
  FFBugCeiling = 0x01u<<1,
};


//static TMapNC<vuint32, line_t *> checkSkipLines;


//==========================================================================
//
//  isTriangleLine
//
//==========================================================================
/*
static bool isTriangleLine (const int lidx, const line_t *line) {
  //GCon->Logf(NAME_Debug, "isTriangleLine: linedef #%d; vc=(%d,%d)", lidx, line->vxCount(0), line->vxCount(1));
  if (line->vxCount(0) != 1 || line->vxCount(1) != 1) return false;

  const line_t *l1 = line->vxLine(0, 0);
  if (l1->vxCount(0) != 1 || l1->vxCount(1) != 1) return false;

  const line_t *l2 = line->vxLine(1, 0);
  if (l2->vxCount(0) != 1 || l2->vxCount(1) != 1) return false;

  int vidx2 = (l2->vxLine(0, 0) == l1 ? 1 : 0);
  if (l2->vxLine(vidx2, 0) != line) return false;

  if (line->flags&ML_TWOSIDED) return (((l1->flags|l2->flags)&ML_TWOSIDED) == 0);
  if (l1->flags&ML_TWOSIDED) return (((line->flags|l2->flags)&ML_TWOSIDED) == 0);
  if (l2->flags&ML_TWOSIDED) return (((l1->flags|line->flags)&ML_TWOSIDED) == 0);
  return false;
}
*/


//==========================================================================
//
//  isBadTriangle
//
//==========================================================================
static bool isBadTriangle (const sector_t *tsec, const sector_t *mysec) {
  if (!tsec || !mysec || tsec == mysec) return false;
  if (tsec->linecount > 3) return false;
  for (int f = 0; f < tsec->linecount; ++f) {
    const line_t *line = tsec->lines[f];
    if ((line->flags&ML_TWOSIDED) == 0) continue;
    if (!line->frontsector || !line->backsector) continue;
    if (line->frontsector == tsec) {
      if (line->backsector != mysec) return false;
    } else if (line->backsector == tsec) {
      if (line->frontsector != mysec) return false;
    } else {
      return false;
    }
  }
  return true;
}


static TArray<int> lineMarks;
static int lineMarkId = 0;

static TArray<int> lineSectorPart; // line indicies


//==========================================================================
//
//  RecurseMarkLines
//
//==========================================================================
static void RecurseMarkLines (VLevel *level, const sector_t *sec, const line_t *line) {
  if (!line) return;
  if (line->vxCount(0) == 0 || line->vxCount(1) == 0) return;
  if (line->frontsector != sec && line->backsector != sec) return;
  const int lidx = (int)(ptrdiff_t)(line-&level->Lines[0]);
  if (lineMarks[lidx] == lineMarkId) return;
  lineMarks[lidx] = lineMarkId;
  lineSectorPart.append(lidx);
  for (int c = 0; c < 2; ++c) {
    for (int f = 0; f < line->vxCount(c); ++f) {
      RecurseMarkLines(level, sec, line->vxLine(c, f));
    }
  }
}


//==========================================================================
//
//  VLevel::IsFloodBugSectorPart
//
//==========================================================================
vuint32 VLevel::IsFloodBugSectorPart (sector_t *sec) {
  if (lineSectorPart.length() < 3) return 0;

  vuint32 res = (deepwater_hacks_floor ? FFBugFloor : 0)|(deepwater_hacks_ceiling ? FFBugCeiling : 0); // not yet
  // don't fix alphas
  if (sec->floor.Alpha != 1.0f) res &= ~FFBugFloor;
  if (sec->ceiling.Alpha != 1.0f) res &= ~FFBugCeiling;
  if (!res) return 0;

  int myside = -1;
  bool hasMissingBottomTexture = false;
  bool hasMissingTopTexture = false;

  // if we have only one of 4+ walls with bottex, still consider it as a floodfill bug
  int floorBotTexCount = 0;
  int ceilTopTexCount = 0;
  //for (int f = 0; f < sec->linecount; ++f)
  for (int ff = 0; ff < lineSectorPart.length(); ++ff)
  {
    //line_t *line = sec->lines[f];
    line_t *line = &Lines[lineSectorPart[ff]];
    if (line->vxCount(0) == 0 || line->vxCount(1) == 0) continue;

    //const int lidx = (int)(ptrdiff_t)(line-&Lines[0]);
    //if (lineMarks[lidx] != lineMarkId) continue;

    if (!res) {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  skipped sector #%d due to res=0", (int)(ptrdiff_t)(sec-&Sectors[0]));
      return 0;
    }
    //vint32 lineidx = (vint32)(ptrdiff_t)(line-&Lines[0]);
    //if (checkSkipLines.find(lineidx)) continue; // skip lines we are not interested into
    if (!line->frontsector || !line->backsector) {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: skipped line #%d due to missing side sector", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
      continue;
    }
    sector_t *bs;
    if (line->frontsector == sec) {
      // back
      bs = line->backsector;
      if (myside == 1) {
        if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: skipped line #%d due to side conflict (1)", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
        if (!hasMissingBottomTexture && bs->floor.minz > sec->floor.minz && (line->sidenum[0] < 0 || Sides[line->sidenum[0]].BottomTexture <= 0)) hasMissingBottomTexture = true;
        if (!hasMissingTopTexture && bs->ceiling.minz < sec->ceiling.minz && (line->sidenum[0] < 0 || Sides[line->sidenum[0]].TopTexture <= 0)) hasMissingTopTexture = true;
        continue;
      }
      myside = 0;
    } else if (line->backsector == sec) {
      // front
      bs = line->frontsector;
      if (myside == 0) {
        if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: skipped line #%d due to side conflict (0)", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
        if (!hasMissingBottomTexture && bs->floor.minz > sec->floor.minz && (line->sidenum[1] < 0 || Sides[line->sidenum[1]].BottomTexture <= 0)) hasMissingBottomTexture = true;
        if (!hasMissingTopTexture && bs->ceiling.minz < sec->ceiling.minz && (line->sidenum[1] < 0 || Sides[line->sidenum[1]].TopTexture <= 0)) hasMissingTopTexture = true;
        continue;
      }
      myside = 1;
    } else {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  something's strange in the neighbourhood, sector #%d", (int)(ptrdiff_t)(sec-&Sectors[0]));
      return 0; // something's strange in the neighbourhood
    }
    // this is self-referenced sector, nothing to see here, come along
    if (bs == sec) {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  skipping self-referenced sector #%d", (int)(ptrdiff_t)(sec-&Sectors[0]));
      return 0;
    }
    if (bs->floor.minz >= bs->ceiling.minz) {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  door-like sector #%d", (int)(ptrdiff_t)(sec-&Sectors[0]));
      // this looks like a door, don't "fix" anything
      return 0;
    }
    if (isBadTriangle(bs, sec)) {
      if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  ignore triangle sector sector #%d", (int)(ptrdiff_t)(bs-&Sectors[0]));
      continue;
    }
    // check for possible floor floodbug
    do {
      if (res&FFBugFloor) {
        // ignore lines with the same height
        if (bs->floor.minz == sec->floor.minz) {
          break;
        }
        // line has no bottom texture?
        if (line->sidenum[myside] >= 0 && Sides[line->sidenum[myside]].BottomTexture > 0) {
          ++floorBotTexCount;
          if (floorBotTexCount > 1 || sec->linecount == 3) {
            if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: reset floorbug flag due to line #%d -- has bottom texture", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
            res &= ~FFBugFloor;
            break;
          }
        }
        // slope?
        if (bs->floor.normal.z != 1.0f) {
          res &= ~FFBugFloor;
          if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: reset floorbug flag due to line #%d -- floor is a slope", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
          break;
        }
        // height?
        if (bs->floor.minz < sec->floor.minz && !bs->othersecFloor) {
          if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: reset floorbug flag due to line #%d -- floor height: bs=%g : sec=%g", (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]), bs->floor.minz, sec->floor.minz);
          res &= ~FFBugFloor;
          break;
        }
        if (line->sidenum[myside] < 0 || Sides[line->sidenum[myside]].BottomTexture <= 0) hasMissingBottomTexture = true;
        if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "  sector #%d (back #%d): floor fix ok; fs:floor=(%g,%g); bs:floor=(%g,%g) (linedef #%d)", (int)(ptrdiff_t)(sec-Sectors), (int)(ptrdiff_t)(bs-Sectors), sec->floor.minz, sec->floor.maxz, bs->floor.minz, bs->floor.maxz, (int)(ptrdiff_t)(line-&Lines[0]));
        //if (/*line->special != 0 &&*/ bs->floor.minz == sec->floor.minz) { res &= ~FFBugFloor; continue; }
      }
    } while (0);
    // check for possible ceiling floodbug
    do {
      //TODO: here we should ignore lifts
      if (res&FFBugCeiling) {
        /*
        int ssnum = (int)(ptrdiff_t)(sec-Sectors);
        if (ssnum == 314) {
          int fsnum = (int)(ptrdiff_t)(line->frontsector-Sectors);
          int bsnum = (int)(ptrdiff_t)(line->backsector-Sectors);
          if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "fs: %d; bs: %d", fsnum, bsnum);
        } else {
          //if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "ss: %d", ssnum);
        }
        */
        // ignore lines with the same height
        if (bs->ceiling.minz == sec->ceiling.minz) {
          break;
        }
        // line has no top texture?
        if (line->sidenum[myside] >= 0 && Sides[line->sidenum[myside]].TopTexture > 0) {
          ++ceilTopTexCount;
          if (ceilTopTexCount > 1 || sec->linecount == 3) {
            res &= ~FFBugCeiling;
            break;
          }
        }
        // slope?
        if (bs->ceiling.normal.z != -1.0f) { res &= ~FFBugCeiling; break; }
        // height?
        if (bs->ceiling.minz > sec->ceiling.minz) { res &= ~FFBugCeiling; break; }
        //if (line->special != 0 && bs->ceiling.minz == sec->ceiling.minz) { res &= ~FFBugCeiling; continue; }
        if (line->sidenum[myside] < 0 && Sides[line->sidenum[myside]].TopTexture <= 0) hasMissingTopTexture = true;
      }
    } while (0);
  }

  if ((res&FFBugFloor) != 0 && !hasMissingBottomTexture) {
    if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: final reset due to no missing bottom textures", (int)(ptrdiff_t)(sec-&Sectors[0]));
    res &= ~FFBugFloor;
  }

  if ((res&FFBugCeiling) != 0 && !hasMissingTopTexture) {
    //if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d: final reset due to no missing bottom textures", (int)(ptrdiff_t)(sec-&Sectors[0]));
    res &= ~FFBugCeiling;
  }

  if (res && dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector:  sector #%d, result=%d", (int)(ptrdiff_t)(sec-&Sectors[0]), res);
  return res;
}


//==========================================================================
//
//  VLevel::IsFloodBugSector
//
//==========================================================================
vuint32 VLevel::IsFloodBugSector (sector_t *sec) {
  if (!sec) return 0;
  if (sec->linecount < 3 || sec->deepref) return 0;
  if (sec->floor.minz >= sec->ceiling.minz) return 0;
  if (sec->floor.normal.z != 1.0f || sec->ceiling.normal.z != -1.0f) return 0;
  vuint32 rres = (deepwater_hacks_floor ? FFBugFloor : 0)|(deepwater_hacks_ceiling ? FFBugCeiling : 0); // not yet
  // don't fix alphas
  if (sec->floor.Alpha != 1.0f) rres &= ~FFBugFloor;
  if (sec->ceiling.Alpha != 1.0f) rres &= ~FFBugCeiling;
  if (!rres) return 0;
  if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "IsFloodBugSector: checking sector #%d (%d lines)", (int)(ptrdiff_t)(sec-&Sectors[0]), sec->linecount);

  if (lineMarks.length() != NumLines) {
    lineMarkId = 1;
    lineMarks.setLength(NumLines);
    for (auto &&mk : lineMarks) mk = 0;
  } else if (lineMarkId++ == MAX_VINT32) {
    lineMarkId = 1;
    for (auto &&mk : lineMarks) mk = 0;
  }

  if (sec->linecount > 3) {
    vuint32 res = 0;
    bool firstRes = true;
    for (int f = 0; f < sec->linecount; ++f) {
      line_t *line = sec->lines[f];
      if (line->vxCount(0) == 0 || line->vxCount(1) == 0) continue;
      const int lidx = (int)(ptrdiff_t)(sec->lines[f]-&Lines[0]);
      if (lineMarks[lidx] == lineMarkId) continue;
      lineSectorPart.resetNoDtor();
      RecurseMarkLines(this, sec, line);
      if (lineSectorPart.length() < 3) continue;
      vuint32 r = IsFloodBugSectorPart(sec);
      if (!r) return 0;
      if (firstRes) {
        firstRes = false;
        res = r;
      } else {
        res &= r;
        if (!res) return 0;
      }
    }
    return res;
  } else {
    for (int f = 0; f < sec->linecount; ++f) {
      line_t *line = sec->lines[f];
      if (line->vxCount(0) == 0 || line->vxCount(1) == 0) continue;
      const int lidx = (int)(ptrdiff_t)(sec->lines[f]-&Lines[0]);
      lineSectorPart.append(lidx);
    }
    if (lineSectorPart.length() != 3) return 0;
    return IsFloodBugSectorPart(sec);
  }
}


//==========================================================================
//
//  VLevel::FindGoodFloodSector
//
//  try to find a sector to borrow a fake surface
//  we'll try each neighbour sector until we'll find a sector without
//  flood bug
//
//==========================================================================
sector_t *VLevel::FindGoodFloodSector (sector_t *sec, bool wantFloor) {
  if (!sec) return nullptr;
  if (wantFloor && dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "FindGoodFloodSector: looking for source sector for sector #%d", (int)(ptrdiff_t)(sec-&Sectors[0]));
  TArray<sector_t *> good;
  TArray<sector_t *> seen;
  TArray<sector_t *> sameBug;
  seen.append(sec);
  vuint32 bugMask = (wantFloor ? FFBugFloor : FFBugCeiling);
  //int ssnum = (int)(ptrdiff_t)(sec-Sectors);
  for (;;) {
    for (int f = 0; f < sec->linecount; ++f) {
      line_t *line = sec->lines[f];
      if (!(!!line->frontsector && !!line->backsector)) continue;
      sector_t *fs;
      sector_t *bs;
      if (line->frontsector == sec) {
        // back
        fs = line->frontsector;
        bs = line->backsector;
      } else if (line->backsector == sec) {
        // front
        fs = line->backsector;
        bs = line->frontsector;
      } else {
        return nullptr; // something's strange in the neighbourhood
      }
      // bs is possible sector to move to
      bool wasSeen = false;
      for (int c = seen.length(); c > 0; --c) if (seen[c-1] == bs) { wasSeen = true; break; }
      if (wasSeen) continue; // we already rejected this sector
      seen.append(bs);
      if (fs == bs) continue;
      //vuint32 xxbug = IsFloodBugSector(fs);
      vuint32 ffbug = IsFloodBugSector(bs);
      //ffbug |= xxbug;
      //if (ssnum == 981) GCon->Logf("xxx: %d (%d); xxbug=%d; ffbug=%d", ssnum, (int)(ptrdiff_t)(bs-Sectors), xxbug, ffbug);
      //if (ssnum == 981) GCon->Logf("xxx: %d (%d); ffbug=%d", ssnum, (int)(ptrdiff_t)(bs-Sectors), ffbug);
      if (ffbug) {
        if ((ffbug&bugMask) != 0) {
          sameBug.append(bs);
          continue;
        }
      }
      // we found a sector without floodbug, check if it is a good one
      // sloped?
      if (wantFloor && bs->floor.normal.z != 1.0f) continue;
      if (!wantFloor && bs->ceiling.normal.z != -1.0f) continue;
      // check height
      if (wantFloor && bs->floor.minz < sec->floor.minz) continue;
      if (!wantFloor && bs->ceiling.minz > sec->ceiling.minz) continue;
      // possible good sector, remember it
      if (wantFloor && dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "FindGoodFloodSector:  found possible good sector #%d for source sector for sector #%d", (int)(ptrdiff_t)(bs-&Sectors[0]), (int)(ptrdiff_t)(sec-&Sectors[0]));
      good.append(bs);
    }
    // if we have no good sectors, try neighbour sector with the same bug
    if (good.length() != 0) break;
    //if (good.length() == 0) return nullptr; // oops
    /*
    sec = good[0];
    good.removeAt(0);
    */
    if (sameBug.length() == 0) return nullptr;
    sec = sameBug[0];
    sameBug.removeAt(0);
  }
  // here we should have some good sectors
  if (good.length() == 0) return nullptr; // sanity check
  if (good.length() == 1) return good[0];
  // we have several good sectors; check if they have the same height, and the same flat texture
  //if (ssnum == 981) GCon->Logf("xxx: %d; len=%d", ssnum, good.length());
  sector_t *res = good[0];
  //sector_t *best = (IsFloodBugSector(res) ? nullptr : res);
  for (int f = 1; f < good.length(); ++f) {
    sec = good[f];
    //if (ssnum == 981) GCon->Logf("yyy(%d): %d; res=%d; ff=%d", f, ssnum, (int)(ptrdiff_t)(sec-Sectors), IsFloodBugSector(sec));
    //!if (sec->params.lightlevel != res->params.lightlevel) return nullptr; //k8: ignore this?
    //!if (sec->params.LightColor != res->params.LightColor) return nullptr; //k8: ignore this?
    if (wantFloor) {
      // floor
      if (sec->floor.minz != res->floor.minz) return nullptr;
      if (sec->floor.pic != res->floor.pic) return nullptr;
    } else {
      // ceiling
      if (sec->ceiling.minz != (/*best ? best :*/ res)->ceiling.minz) {
        //GCon->Logf("000: %d (%d)", ssnum, (int)(ptrdiff_t)(sec-Sectors));
        //if (IsFloodBugSector(sec)) continue;
        //GCon->Logf("000: %d; %d (%d)  %f : %f", ssnum, (int)(ptrdiff_t)(res-Sectors), (int)(ptrdiff_t)(sec-Sectors), sec->ceiling.minz, (best ? best : res)->ceiling.minz);
        return nullptr;
      }
      if (sec->ceiling.pic != res->ceiling.pic) {
        //if (IsFloodBugSector(sec)) continue;
        //GCon->Logf("001: %d (%d)", ssnum, (int)(ptrdiff_t)(sec-Sectors));
        return nullptr;
      }
    }
    //if (!best && !IsFloodBugSector(sec)) best = sec;
  }
  //if (!best) return nullptr;
  //if (best) res = best;
  //if (ssnum == 981) GCon->Logf("zzz: %d; res=%d", ssnum, (int)(ptrdiff_t)(res-Sectors));
  return res;
}


//==========================================================================
//
// VLevel::FixDeepWaters
//
//==========================================================================
void VLevel::FixDeepWaters () {
  if (NumSectors == 0) return;
  //vassert(checkSkipLines.length() == 0);

  if (!NeedUDMFFix(this)) { GCon->Logf(NAME_Debug, "FixDeepWaters: skipped due to UDMF map"); return; }

  for (auto &&sec : allSectors()) {
    sec.deepref = nullptr;
    sec.othersecFloor = nullptr;
    sec.othersecCeiling = nullptr;
  }

  // process deepwater
  if (deepwater_hacks && !(LevelFlags&LF_ForceNoDeepwaterFix)) FixSelfRefDeepWater();

  bool oldFixFloor = deepwater_hacks_floor;
  bool oldFixCeiling = deepwater_hacks_ceiling;

  if (LevelFlags&LF_ForceNoFloorFloodfillFix) deepwater_hacks_floor = false;
  if (LevelFlags&LF_ForceNoCeilingFloodfillFix) deepwater_hacks_ceiling = false;

  if (deepwater_hacks_floor || deepwater_hacks_ceiling) {
    // fix "floor holes"
    for (int sidx = 0; sidx < NumSectors; ++sidx) {
      sector_t *sec = &Sectors[sidx];
      if (sec->linecount < 3 || sec->deepref) continue;
      if (sec->SectorFlags&sector_t::SF_UnderWater) continue; // this is special sector, skip it
      if ((sec->SectorFlags&(sector_t::SF_HasExtrafloors|sector_t::SF_ExtrafloorSource|sector_t::SF_TransferSource|sector_t::SF_UnderWater))) {
        if (!(sec->SectorFlags&sector_t::SF_IgnoreHeightSec)) continue; // this is special sector, skip it
      }
      // slopes aren't interesting
      if (sec->floor.normal.z != 1.0f || sec->ceiling.normal.z != -1.0f) continue;
      if (sec->floor.minz >= sec->ceiling.minz) continue; // closed something
      vuint32 bugFlags = IsFloodBugSector(sec);
      if (bugFlags == 0) continue;
      sector_t *fsecFloor = nullptr, *fsecCeiling = nullptr;
      if (bugFlags&FFBugFloor) fsecFloor = FindGoodFloodSector(sec, true);
      if (bugFlags&FFBugCeiling) fsecCeiling = FindGoodFloodSector(sec, false);
      if (fsecFloor == sec) fsecFloor = nullptr;
      if (fsecCeiling == sec) fsecCeiling = nullptr;
      if (!fsecFloor) {
        if (dbg_floodfill_fixer) GCon->Logf(NAME_Debug, "skipping illusiopit fix for sector #%d (no floor)", sidx);
      }
      if (!fsecFloor && !fsecCeiling) continue;
      GCon->Logf("FLATFIX: found illusiopit at sector #%d (floor:%d; ceiling:%d)", sidx, (fsecFloor ? (int)(ptrdiff_t)(fsecFloor-Sectors) : -1), (fsecCeiling ? (int)(ptrdiff_t)(fsecCeiling-Sectors) : -1));
      sec->othersecFloor = fsecFloor;
      sec->othersecCeiling = fsecCeiling;
      // allocate fakefloor data (engine require it to complete setup)
      if (!sec->fakefloors) sec->fakefloors = new fakefloor_t;
      fakefloor_t *ff = sec->fakefloors;
      memset((void *)ff, 0, sizeof(fakefloor_t));
      ff->flags |= fakefloor_t::FLAG_CreatedByLoader;
      ff->floorplane = (fsecFloor ? fsecFloor : sec)->floor;
      ff->ceilplane = (fsecCeiling ? fsecCeiling : sec)->ceiling;
      ff->params = sec->params;
      //sec->SectorFlags = (fsecFloor ? SF_FakeFloorOnly : 0)|(fsecCeiling ? SF_FakeCeilingOnly : 0);
    }
  }

  if (deepwater_hacks_bridges) {
    // eh, do another loop, why not?
    // this should fix "hanging bridges"
    // such bridges are made with sectors higher than the surrounding sectors,
    // and lines with missing low texture. the surrounding sectors
    // should have the same height.
    for (int sidx = 0; sidx < NumSectors; ++sidx) {
      sector_t *sec = &Sectors[sidx];
      // skip special sectors
      if (sec->linecount < 3 || sec->deepref) continue;
      if (sec->othersecFloor || sec->othersecCeiling) continue;
      if (sec->heightsec) continue;
      if (sec->fakefloors) continue;
      if (sec->SectorFlags&(sector_t::SF_HasExtrafloors|sector_t::SF_ExtrafloorSource|sector_t::SF_TransferSource|sector_t::SF_UnderWater)) continue;
      // shoild not be sloped
      if (sec->floor.normal.z != 1.0f) continue;
      // check if the surrounding sectors are of the same height
      bool valid = true;
      bool foundSomething = false;
      float surheight = 0;
      //GCon->Logf(NAME_Debug, "::: checking sector #%d for a bridge", sidx);
      sector_t *sursec = nullptr;
      for (int lcnt = 0; lcnt < sec->linecount; ++lcnt) {
        line_t *line = sec->lines[lcnt];
        if (!line->frontsector || !line->backsector) continue;
        if (line->frontsector == line->backsector) continue; // wtf?!
        int sidenum = (int)(line->frontsector == sec);
        if (line->sidenum[sidenum] < 0) continue;
        if (line->sidenum[sidenum^1] < 0) continue;
        side_t *fside = &Sides[line->sidenum[sidenum^1]];
        side_t *bside = &Sides[line->sidenum[sidenum]];
        sector_t *bsec = bside->Sector;
        if (bsec == sec) continue;
        // both sides should not have lowtex
        if (fside->BottomTexture > 0 || bside->BottomTexture > 0) {
          //GCon->Logf(NAME_Debug, "  sector #%d: bottex fail", sidx);
          valid = false;
          break;
        }
        // back sector should be lower
        const float fz1 = bsec->floor.GetPointZ(*line->v1);
        const float fz2 = bsec->floor.GetPointZ(*line->v2);
        // different heights cannot be processed
        if (fz1 != fz2) {
          //GCon->Logf(NAME_Debug, "  sector #%d: slope fail (%g : %g); ldef=%d; fsec=%d; bsec=%d", sidx, fz1, fz2, (int)(ptrdiff_t)(line-&Lines[0]), (int)(ptrdiff_t)(fside->Sector-&Sectors[0]), (int)(ptrdiff_t)(bside->Sector-&Sectors[0]));
          valid = false;
          break;
        }
        // must be lower
        if (fz1 > sec->floor.minz) {
          //GCon->Logf(NAME_Debug, "  sector #%d: height fail (fz1=%g; fmz=%g); ldef=%d; fsec=%d; bsec=%d", sidx, fz1, sec->floor.minz, (int)(ptrdiff_t)(line-&Lines[0]), (int)(ptrdiff_t)(fside->Sector-&Sectors[0]), (int)(ptrdiff_t)(bside->Sector-&Sectors[0]));
          valid = false;
          break;
        }
        // skip same-height sectors
        if (fz1 == sec->floor.minz) {
          continue;
        }
        if (foundSomething) {
          // height should be the same
          if (surheight != fz1) {
            //GCon->Logf(NAME_Debug, "  sector #%d: surround height fail (fz1=%g; surheight=%g); ldef=%d; fsec=%d; bsec=%d", sidx, fz1, surheight, (int)(ptrdiff_t)(line-&Lines[0]), (int)(ptrdiff_t)(fside->Sector-&Sectors[0]), (int)(ptrdiff_t)(bside->Sector-&Sectors[0]));
            valid = false;
            break;
          }
        } else {
          foundSomething = true;
          surheight = fz1;
          sursec = bsec; // use first found sector
        }
      }
      if (!valid || !foundSomething) continue;
      vassert(sursec);
      // ok, it looks like a bridge; create fake floor and ceiling
      GCon->Logf("BRIDGEFIX: found bridge at sector #%d", sidx);
      /*
      for (int lcnt = 0; lcnt < sec->linecount; ++lcnt) {
        line_t *line = sec->lines[lcnt];
        int sidenum = (int)(line->frontsector == sec);
        if (line->sidenum[sidenum] < 0) continue;
        if (line->sidenum[sidenum^1] < 0) continue;
        side_t *fside = &Sides[line->sidenum[sidenum^1]];
        side_t *bside = &Sides[line->sidenum[sidenum]];
        GCon->Logf(NAME_Debug, "  sector #%d: ldef=%d; fsec=%d; bsec=%d; fsz=%g; bsz=%g; blc=%d", sidx, (int)(ptrdiff_t)(line-&Lines[0]), (int)(ptrdiff_t)(fside->Sector-&Sectors[0]), (int)(ptrdiff_t)(bside->Sector-&Sectors[0]), fside->Sector->floor.minz, bside->Sector->floor.minz, bside->Sector->linecount);
      }
      */
      sec->othersecFloor = sursec;
      // allocate fakefloor data (engine require it to complete setup)
      vassert(!sec->fakefloors);
      sec->fakefloors = new fakefloor_t;
      fakefloor_t *ff = sec->fakefloors;
      memset((void *)ff, 0, sizeof(fakefloor_t));
      ff->flags |= fakefloor_t::FLAG_CreatedByLoader;
      ff->floorplane = sursec->floor;
      // ceiling must be current sector's floor, flipped
      ff->ceilplane = sec->floor;
      ff->ceilplane.flipInPlace();
      ff->params = sursec->params;
      sec->SectorFlags |= sector_t::SF_HangingBridge/*|sector_t::SF_ClipFakePlanes*/;
    }
  }

  deepwater_hacks_floor = oldFixFloor;
  deepwater_hacks_ceiling = oldFixCeiling;
}
