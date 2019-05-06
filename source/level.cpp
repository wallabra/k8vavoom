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
#include "gamedefs.h"
#include "sv_local.h"
#ifdef CLIENT
#include "cl_local.h"
#endif
#include "render/r_local.h" // for decals

//#define VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
//#define VAVOOM_DECALS_DEBUG

extern VCvarB r_decals_enabled;

IMPLEMENT_CLASS(V, Level);

VLevel *GLevel;
VLevel *GClLevel;

static VCvarI r_decal_onetype_max("r_decal_onetype_max", "128", "Maximum decals of one decaltype on a wall segment.", CVAR_Archive);
static VCvarI r_decal_gore_onetype_max("r_decal_gore_onetype_max", "8", "Maximum decals of one decaltype on a wall segment for Gore Mod.", CVAR_Archive);

static VCvarB gm_compat_corpses_can_hear("gm_compat_corpses_can_hear", false, "Can corpses hear sound propagation?", CVAR_Archive);
static VCvarB gm_compat_everything_can_hear("gm_compat_everything_can_hear", false, "Can everything hear sound propagation?", CVAR_Archive);
static VCvarF gm_compat_max_hearing_distance("gm_compat_max_hearing_distance", "0", "Maximum hearing distance (0 means unlimited)?", CVAR_Archive);


opening_t *VLevel::openListHead = nullptr;
opening_t *VLevel::openListFree = nullptr;


//==========================================================================
//
//  VLevelScriptThinker::~VLevelScriptThinker
//
//==========================================================================
VLevelScriptThinker::~VLevelScriptThinker () {
  if (!destroyed) Sys_Error("trying to delete unfinalized Acs script");
}


//==========================================================================
//
//  VLevel::IncrementValidCount
//
//==========================================================================
void VLevel::IncrementValidCount () {
  if (++validcount == 0x7fffffff) {
    validcount = 1;
    line_t *ld = &Lines[0];
    for (int count = NumLines; count--; ++ld) ld->validcount = 0;
  }
}


//==========================================================================
//
//  VLevel::AllocOpening
//
//  allocate new opening from list
//
//==========================================================================
opening_t *VLevel::AllocOpening () {
  // get or alloc opening
  opening_t *res = openListFree;
  if (res) {
    openListFree = res->listnext;
  } else {
    res = (opening_t *)Z_Malloc(sizeof(opening_t));
  }
  // clear it
  memset((void *)res, 0, sizeof(opening_t));
  // and include it into allocated list
  if (openListHead) openListHead->listprev = res;
  res->listnext = openListHead;
  openListHead = res;
  return res;
}


//==========================================================================
//
//  VLevel::FreeOpening
//
//  free one opening
//
//==========================================================================
void VLevel::FreeOpening (opening_t *op) {
  if (!op) return;
  // remove from allocated list
  if (op->listprev) op->listprev->listnext = op->listnext; else openListHead = op->listnext;
  if (op->listnext) op->listnext->listprev = op->listprev;
  op->listprev = nullptr;
  op->listnext = openListFree;
  openListFree = op;
}


//==========================================================================
//
//  VLevel::FreeOpeningList
//
//  free opening list
//
//==========================================================================
void VLevel::FreeOpeningList (opening_t *&op) {
  while (op) {
    opening_t *next = op->next;
    FreeOpening(op);
    op = next;
  }
}


//==========================================================================
//
//  VLevel::PointInSubsector
//
//==========================================================================
subsector_t *VLevel::PointInSubsector (const TVec &point) const {
  // single subsector is a special case
  if (!NumNodes) return Subsectors;
  int nodenum = NumNodes-1;
  do {
    const node_t *node = Nodes+nodenum;
    nodenum = node->children[node->PointOnSide(point)];
  } while (!(nodenum&NF_SUBSECTOR));
  return &Subsectors[nodenum&~NF_SUBSECTOR];
}


//==========================================================================
//
//  VLevel::CalcSkyHeight
//
//==========================================================================
float VLevel::CalcSkyHeight () const {
  if (NumSectors == 0) return 0.0f; // just in case
  // calculate sky height
  float skyheight = -99999.0f;
  for (unsigned i = 0; i < (unsigned)NumSectors; ++i) {
    if (Sectors[i].ceiling.pic == skyflatnum &&
        Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Sectors[i].ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  return skyheight;
}


//==========================================================================
//
//  VLevel::CalcSectorBoundingHeight
//
//  some sectors (like doors) has floor and ceiling on the same level, so
//  we have to look at neighbour sector to get height.
//  note that if neighbour sector is closed door too, we can safely use
//  our zero height, as camera cannot see through top/bottom textures.
//
//==========================================================================
void VLevel::CalcSectorBoundingHeight (sector_t *sector, float *minz, float *maxz) {
  float tmp0, tmp1;
  if (!minz) minz = &tmp0;
  if (!maxz) maxz = &tmp1;

  if (sector->ZExtentsCached && sector->LastFloorZ == sector->floor.minz && sector->LastCeilingZ == sector->ceiling.maxz) {
    *minz = sector->LastMinZ;
    *maxz = sector->LastMaxZ;
    return;
  }

  *minz = sector->floor.minz;
  *maxz = sector->ceiling.maxz;

  if (!sector->linecount) {
    // skip sectors containing original polyobjs
    *maxz = *minz;
  }
  if (*maxz < *minz) { const float tmp = *minz; *minz = *maxz; *maxz = tmp; }

  // check if we have two-sided lines in this sector
  line_t *const *lines = sector->lines;
  for (unsigned count = sector->linecount; count--; ++lines) {
    const line_t *line = *lines;
    if (!(line->flags&ML_TWOSIDED)) continue;
    // get neighbour sector
    const sector_t *bsec = (sector == line->frontsector ? line->backsector : line->frontsector);
    if (bsec == sector) {
      //FIXME: this is deepwater, make in infinitely high
      *minz = -32767.0f;
      *maxz = 32767.0f;
      return;
    }
    float zmin = min2(bsec->floor.minz, bsec->floor.maxz);
    zmin = min2(bsec->ceiling.minz, bsec->ceiling.maxz);
    float zmax = max2(bsec->ceiling.minz, bsec->ceiling.maxz);
    zmax = max2(bsec->floor.minz, bsec->floor.maxz);
    if (zmax < zmin) { const float tmp = zmax; zmax = zmin; zmin = tmp; }
    *minz = min2(*minz, zmin);
    *maxz = max2(*maxz, zmax);
  }

  // cache values
  sector->ZExtentsCached |= 1;
  sector->LastFloorZ = sector->floor.minz;
  sector->LastCeilingZ = sector->ceiling.maxz;
  sector->LastMinZ = *minz;
  sector->LastMaxZ = *maxz;
}


//==========================================================================
//
//  VLevel::GetSubsectorBBox
//
//==========================================================================
void VLevel::GetSubsectorBBox (const subsector_t *sub, float bbox[6]) const {
  // min
  bbox[0+0] = sub->bbox[0];
  bbox[0+1] = sub->bbox[1];
  // max
  bbox[3+0] = sub->bbox[2];
  bbox[3+1] = sub->bbox[3];

  // for one-sector degenerate maps
  if (sub->parent) {
    bbox[0+2] = min2(sub->parent->bbox[0][2], sub->parent->bbox[1][2]);
    bbox[3+2] = max2(sub->parent->bbox[0][5], sub->parent->bbox[1][5]);
  } else {
    bbox[0+2] = sub->sector->floor.minz;
    bbox[3+2] = sub->sector->ceiling.maxz;
  }
  FixBBoxZ(bbox);

  /*
  float minz, maxz;
  CalcSectorBoundingHeight(sub->sector, &minz, &maxz);
  bbox[2] = min2(bbox[2], minz);
  bbox[5] = max2(bbox[5], maxz);
  */
}


//==========================================================================
//
//  VLevel::UpdateSubsectorBBox
//
//==========================================================================
void VLevel::UpdateSubsectorBBox (int num, float *bbox, const float skyheight) {
  subsector_t *sub = &Subsectors[num];
  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
  /*
  bbox[2] = sub->sector->floor.minz;
  bbox[5] = (IsSky(&sub->sector->ceiling) ? skyheight : sub->sector->ceiling.maxz);
  */
  CalcSectorBoundingHeight(sub->sector, &bbox[2], &bbox[5]);
  if (IsSky(&sub->sector->ceiling)) bbox[5] = skyheight;
  FixBBoxZ(bbox);
}


//==========================================================================
//
//  VLevel::RecalcWorldNodeBBox
//
//==========================================================================
void VLevel::RecalcWorldNodeBBox (int bspnum, float *bbox, const float skyheight) {
  if (bspnum == -1) {
    UpdateSubsectorBBox(0, bbox, skyheight);
    return;
  }
  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    // nope, this is a normal node
    node_t *bsp = &Nodes[bspnum];
    // decide which side the view point is on
    unsigned side = 0; //bsp->PointOnSide(vieworg);
    RecalcWorldNodeBBox(bsp->children[side], bsp->bbox[side], skyheight);
    bbox[2] = min2(bsp->bbox[0][2], bsp->bbox[1][2]);
    bbox[5] = max2(bsp->bbox[0][5], bsp->bbox[1][5]);
    side ^= 1;
    return RecalcWorldNodeBBox(bsp->children[side], bsp->bbox[side], skyheight); // help gcc to see tail-call
  } else {
    // leaf node (subsector)
    UpdateSubsectorBBox(bspnum&(~NF_SUBSECTOR), bbox, skyheight);
  }
}


//==========================================================================
//
//  VLevel::RecalcWorldBBoxes
//
//==========================================================================
void VLevel::RecalcWorldBBoxes () {
  if (NumSectors == 0) return; // just in case
  const float skyheight = CalcSkyHeight();
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };
  RecalcWorldNodeBBox(NumNodes-1, dummy_bbox, skyheight);
}


//==========================================================================
//
//  DecalIO
//
//==========================================================================
static void DecalIO (VStream &Strm, decal_t *dc) {
  if (!dc) return;
  {
    VNTValueIOEx vio(&Strm);
    //if (!vio.IsLoading()) GCon->Logf("SAVE: texture: id=%d; name=<%s>", dc->texture.id, *GTextureManager.GetTextureName(dc->texture));
    vio.io(VName("texture"), dc->texture);
    vio.io(VName("dectype"), dc->dectype);
    if (vio.IsError()) Host_Error("error reading decals");
    //if (vio.IsLoading()) GCon->Logf("LOAD: texture: id=%d; name=<%s>", dc->texture.id, *GTextureManager.GetTextureName(dc->texture));
    if (dc->texture <= 0) {
      GCon->Logf(NAME_Warning, "LOAD: decal of type '%s' has missing texture", *dc->dectype);
      dc->texture = 0;
    }
    vio.io(VName("flags"), dc->flags);
    vio.io(VName("orgz"), dc->orgz);
    vio.io(VName("curz"), dc->curz);
    /* //debug
    if (!Strm.IsLoading()) {
      VStr s = "fuck0";
      vio.io("FFFuck0", s);
    }
    */
    /* //debug
    if (!Strm.IsLoading()) {
      vio.io(VName("linelen"), dc->linelen);
      vio.io(VName("xdist"), dc->xdist);
    } else {
      vio.io(VName("xdist"), dc->xdist);
      vio.io(VName("linelen"), dc->linelen);
    }
    */
    vio.io(VName("xdist"), dc->xdist);
    vio.io(VName("ofsX"), dc->ofsX);
    vio.io(VName("ofsY"), dc->ofsY);
    vio.io(VName("origScaleX"), dc->origScaleX);
    vio.io(VName("origScaleY"), dc->origScaleY);
    vio.io(VName("scaleX"), dc->scaleX);
    vio.io(VName("scaleY"), dc->scaleY);
    vio.io(VName("origAlpha"), dc->origAlpha);
    vio.io(VName("alpha"), dc->alpha);
    vio.io(VName("addAlpha"), dc->addAlpha);
    /* //debug
    if (!Strm.IsLoading()) {
      VStr s = "fuck1";
      vio.io("FFFuck1", s);
    }
    */
    if (vio.IsError()) Host_Error("error in decal i/o");
  }
  VDecalAnim::Serialise(Strm, dc->animator);
}


//==========================================================================
//
//  writeOrCheckUInt
//
//==========================================================================
static bool writeOrCheckUInt (VStream &Strm, vuint32 value, const char *errmsg=nullptr) {
  if (Strm.IsLoading()) {
    vuint32 v;
    Strm << v;
    if (v != value || Strm.IsError()) {
      if (errmsg) Host_Error("Save loader: invalid value for %s; got 0x%08x, but expected 0x%08x", errmsg, v, value);
      return false;
    }
  } else {
    Strm << value;
  }
  return !Strm.IsError();
}


//==========================================================================
//
//  VLevel::SerialiseOther
//
//==========================================================================
void VLevel::SerialiseOther (VStream &Strm) {
  int i;
  sector_t *sec;
  line_t *li;
  side_t *si;

  // reset subsector update frame
  if (Strm.IsLoading()) {
    for (unsigned f = 0; f < (unsigned)NumSubsectors; ++f) Subsectors[f].updateWorldFrame = 0;
  }

  // write/check various numbers, so we won't load invalid save accidentally
  // this is not the best or most reliable way to check it, but it is better
  // than nothing...

  writeOrCheckUInt(Strm, LSSHash, "geometry hash");
  bool segsHashOK = writeOrCheckUInt(Strm, SegHash);

  // decals
  if (Strm.IsLoading()) decanimlist = nullptr;

  if (segsHashOK) {
    vuint32 dctotal = 0;
    if (Strm.IsLoading()) {
      vint32 dcSize = 0;
      Strm << dcSize;
      // load decals
      for (int f = 0; f < (int)NumSegs; ++f) {
        vuint32 dcount = 0;
        // remove old decals
        decal_t *odcl = Segs[f].decals;
        while (odcl) {
          decal_t *c = odcl;
          odcl = c->next;
          delete c->animator;
          delete c;
        }
        Segs[f].decals = nullptr;
        // load decal count for this seg
        Strm << dcount;
        decal_t *decal = nullptr; // previous
        while (dcount-- > 0) {
          decal_t *dc = new decal_t;
          memset((void *)dc, 0, sizeof(decal_t));
          dc->seg = &Segs[f];
          DecalIO(Strm, dc);
          if (dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0 || dc->texture <= 0) {
            delete dc->animator;
            delete dc;
          } else {
            // fix backsector
            if (dc->flags&(decal_t::SlideFloor|decal_t::SlideCeil)) {
              line_t *lin = Segs[f].linedef;
              if (!lin) Sys_Error("Save loader: invalid seg linedef (0)!");
              int bsidenum = (dc->flags&decal_t::SideDefOne ? 1 : 0);
              dc->slidesec = (bsidenum ? dc->seg->backsector : dc->seg->frontsector);
            }
            // add to decal list
            if (decal) decal->next = dc; else Segs[f].decals = dc;
            if (dc->animator) {
              if (decanimlist) decanimlist->prevanimated = dc;
              dc->nextanimated = decanimlist;
              decanimlist = dc;
            }
            decal = dc;
          }
          ++dctotal;
        }
      }
      GCon->Logf("%u decals loaded", dctotal);
    } else {
      // save decals
      vint32 dcSize = 0;
      int dcStartPos = Strm.Tell();
      Strm << dcSize; // will be fixed later
      for (int f = 0; f < (int)NumSegs; ++f) {
        // count decals
        vuint32 dcount = 0;
        for (decal_t *decal = Segs[f].decals; decal; decal = decal->next) ++dcount;
        Strm << dcount;
        for (decal_t *decal = Segs[f].decals; decal; decal = decal->next) {
          DecalIO(Strm, decal);
          ++dctotal;
        }
      }
      auto currPos = Strm.Tell();
      Strm.Seek(dcStartPos);
      dcSize = currPos-(dcStartPos+4);
      Strm << dcSize;
      Strm.Seek(currPos);
      GCon->Logf("%u decals saved", dctotal);
    }
  } else {
    // skip decals
    vint32 dcSize = 0;
    Strm << dcSize;
    if (Strm.IsLoading()) {
      if (dcSize < 0) Host_Error("decals section is broken");
      Strm.Seek(Strm.Tell()+dcSize);
    }
    if (dcSize) {
      GCon->Logf("seg hash doesn't match (this is harmless, but you lost decals)");
    } else {
      GCon->Logf("seg hash doesn't match (this is harmless)");
    }
  }

  // sectors
  {
    vint32 cnt = NumSectors;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumSectors) Host_Error("invalid number of sectors");
    }

    for (i = 0, sec = Sectors; i < NumSectors; ++i, ++sec) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("floor.dist"), sec->floor.dist);
      vio.io(VName("floor.TexZ"), sec->floor.TexZ);
      vio.io(VName("floor.pic"), sec->floor.pic);
      vio.io(VName("floor.xoffs"), sec->floor.xoffs);
      vio.io(VName("floor.yoffs"), sec->floor.yoffs);
      vio.io(VName("floor.XScale"), sec->floor.XScale);
      vio.io(VName("floor.YScale"), sec->floor.YScale);
      vio.io(VName("floor.Angle"), sec->floor.Angle);
      vio.io(VName("floor.BaseAngle"), sec->floor.BaseAngle);
      vio.io(VName("floor.BaseYOffs"), sec->floor.BaseYOffs);
      vio.io(VName("floor.flags"), sec->floor.flags);
      vio.io(VName("floor.Alpha"), sec->floor.Alpha);
      vio.io(VName("floor.MirrorAlpha"), sec->floor.MirrorAlpha);
      vio.io(VName("floor.LightSourceSector"), sec->floor.LightSourceSector);
      vio.io(VName("floor.SkyBox"), sec->floor.SkyBox);
      vio.io(VName("ceiling.dist"), sec->ceiling.dist);
      vio.io(VName("ceiling.TexZ"), sec->ceiling.TexZ);
      vio.io(VName("ceiling.pic"), sec->ceiling.pic);
      vio.io(VName("ceiling.xoffs"), sec->ceiling.xoffs);
      vio.io(VName("ceiling.yoffs"), sec->ceiling.yoffs);
      vio.io(VName("ceiling.XScale"), sec->ceiling.XScale);
      vio.io(VName("ceiling.YScale"), sec->ceiling.YScale);
      vio.io(VName("ceiling.Angle"), sec->ceiling.Angle);
      vio.io(VName("ceiling.BaseAngle"), sec->ceiling.BaseAngle);
      vio.io(VName("ceiling.BaseYOffs"), sec->ceiling.BaseYOffs);
      vio.io(VName("ceiling.flags"), sec->ceiling.flags);
      vio.io(VName("ceiling.Alpha"), sec->ceiling.Alpha);
      vio.io(VName("ceiling.MirrorAlpha"), sec->ceiling.MirrorAlpha);
      vio.io(VName("ceiling.LightSourceSector"), sec->ceiling.LightSourceSector);
      vio.io(VName("ceiling.SkyBox"), sec->ceiling.SkyBox);
      vio.io(VName("params.lightlevel"), sec->params.lightlevel);
      vio.io(VName("params.LightColor"), sec->params.LightColor);
      vio.io(VName("params.Fade"), sec->params.Fade);
      vio.io(VName("params.contents"), sec->params.contents);
      vio.io(VName("special"), sec->special);
      vio.io(VName("tag"), sec->tag);
      vio.io(VName("seqType"), sec->seqType);
      vio.io(VName("SectorFlags"), sec->SectorFlags);
      vio.io(VName("SoundTarget"), sec->SoundTarget);
      vio.io(VName("FloorData"), sec->FloorData);
      vio.io(VName("CeilingData"), sec->CeilingData);
      vio.io(VName("LightingData"), sec->LightingData);
      vio.io(VName("AffectorData"), sec->AffectorData);
      vio.io(VName("ActionList"), sec->ActionList);
      vio.io(VName("Damage"), sec->Damage);
      vio.io(VName("Friction"), sec->Friction);
      vio.io(VName("MoveFactor"), sec->MoveFactor);
      vio.io(VName("Gravity"), sec->Gravity);
      vio.io(VName("Sky"), sec->Sky);
      if (Strm.IsLoading()) {
        CalcSecMinMaxs(sec);
        sec->ThingList = nullptr;
      }
    }
    if (Strm.IsLoading()) HashSectors();
  }

  // lines
  {
    vint32 cnt = NumLines;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumLines) Host_Error("invalid number of linedefs");
    }

    for (i = 0, li = Lines; i < NumLines; ++i, ++li) {
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("flags"), li->flags);
        vio.io(VName("SpacFlags"), li->SpacFlags);
        //vio.iodef(VName("exFlags"), li->exFlags, 0);
        vio.io(VName("exFlags"), li->exFlags);
        vio.io(VName("special"), li->special);
        vio.io(VName("arg1"), li->arg1);
        vio.io(VName("arg2"), li->arg2);
        vio.io(VName("arg3"), li->arg3);
        vio.io(VName("arg4"), li->arg4);
        vio.io(VName("arg5"), li->arg5);
        vio.io(VName("LineTag"), li->LineTag);
        vio.io(VName("alpha"), li->alpha);
        if (Strm.IsLoading()) {
          // for now, mark partially mapped lines as fully mapped
          if (li->exFlags&(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED)) {
            li->flags |= ML_MAPPED;
          }
          li->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
        }
      }

      for (int j = 0; j < 2; ++j) {
        VNTValueIOEx vio(&Strm);
        if (li->sidenum[j] == -1) {
          // do nothing
        } else {
          si = &Sides[li->sidenum[j]];
          vint32 lnum = si->LineNum;
          vio.io(VName("LineNum"), lnum);
          if (lnum != si->LineNum) Host_Error("invalid sidedef");
          vio.io(VName("TopTexture"), si->TopTexture);
          vio.io(VName("BottomTexture"), si->BottomTexture);
          vio.io(VName("MidTexture"), si->MidTexture);
          vio.io(VName("TopTextureOffset"), si->Top.TextureOffset);
          vio.io(VName("BotTextureOffset"), si->Bot.TextureOffset);
          vio.io(VName("MidTextureOffset"), si->Mid.TextureOffset);
          vio.io(VName("TopRowOffset"), si->Top.RowOffset);
          vio.io(VName("BotRowOffset"), si->Bot.RowOffset);
          vio.io(VName("MidRowOffset"), si->Mid.RowOffset);
          vio.io(VName("Flags"), si->Flags);
          vio.io(VName("Light"), si->Light);
          /*k8: no need to save scaling, as it cannot be changed by ACS/decorate.
                note that VC code can change it.
                do this with flags to not break old saves. */
          vint32 scales = 0;
          if (!Strm.IsLoading()) {
            if (si->Top.ScaleX != 1.0f) scales |= 0x01;
            if (si->Top.ScaleY != 1.0f) scales |= 0x02;
            if (si->Bot.ScaleX != 1.0f) scales |= 0x04;
            if (si->Bot.ScaleY != 1.0f) scales |= 0x08;
            if (si->Mid.ScaleX != 1.0f) scales |= 0x10;
            if (si->Mid.ScaleY != 1.0f) scales |= 0x20;
          }
          vio.iodef(VName("Scales"), scales, 0);
          if (scales&0x01) vio.io(VName("TopScaleX"), si->Top.ScaleX);
          if (scales&0x02) vio.io(VName("TopScaleY"), si->Top.ScaleY);
          if (scales&0x04) vio.io(VName("BotScaleX"), si->Bot.ScaleX);
          if (scales&0x08) vio.io(VName("BotScaleY"), si->Bot.ScaleY);
          if (scales&0x10) vio.io(VName("MidScaleX"), si->Mid.ScaleX);
          if (scales&0x20) vio.io(VName("MidScaleY"), si->Mid.ScaleY);
        }
      }
    }
    if (Strm.IsLoading()) HashLines();
  }

  // polyobjs
  {
    vint32 cnt = NumPolyObjs;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumPolyObjs) Host_Error("invalid number of polyobjects");
    }

    for (i = 0; i < NumPolyObjs; ++i) {
      VNTValueIOEx vio(&Strm);
      float angle = PolyObjs[i].angle;
      float polyX = PolyObjs[i].startSpot.x;
      float polyY = PolyObjs[i].startSpot.y;
      vio.io(VName("angle"), angle);
      vio.io(VName("startSpot.x"), polyX);
      vio.io(VName("startSpot.y"), polyY);
      if (Strm.IsLoading()) {
        RotatePolyobj(PolyObjs[i].tag, angle);
        //GCon->Logf("poly #%d: oldpos=(%f,%f)", i, PolyObjs[i].startSpot.x, PolyObjs[i].startSpot.y);
        MovePolyobj(PolyObjs[i].tag, polyX-PolyObjs[i].startSpot.x, polyY-PolyObjs[i].startSpot.y);
        //GCon->Logf("poly #%d: newpos=(%f,%f) (%f,%f)", i, PolyObjs[i].startSpot.x, PolyObjs[i].startSpot.y, polyX, polyY);
      }
      vio.io(VName("SpecialData"), PolyObjs[i].SpecialData);
    }
  }

  // clear seen segs on loading
  if (Strm.IsLoading()) {
    for (i = 0; i < NumSegs; ++i) Segs[i].flags &= ~SF_MAPPED;
  }

  // static lights
  {
    Strm << STRM_INDEX(NumStaticLights);
    if (Strm.IsLoading()) {
      if (StaticLights) {
        delete[] StaticLights;
        StaticLights = nullptr;
      }
      if (NumStaticLights) StaticLights = new rep_light_t[NumStaticLights];
    }
    for (i = 0; i < NumStaticLights; ++i) {
      VNTValueIOEx vio(&Strm);
      //TODO: save static light entity
      vio.io(VName("Origin"), StaticLights[i].Origin);
      vio.io(VName("Radius"), StaticLights[i].Radius);
      vio.io(VName("Color"), StaticLights[i].Color);
      vio.io(VName("Owner"), StaticLights[i].Owner);
    }
  }

  // ACS: script thinkers must be serialized first
  // script thinkers
  {
    vuint8 xver = 1;
    Strm << xver;
    if (xver != 1) Host_Error("Save is broken (invalid scripts version %u)", (unsigned)xver);
    vint32 sthcount = scriptThinkers.length();
    Strm << STRM_INDEX(sthcount);
    if (sthcount < 0) Host_Error("Save is broken (invalid number of scripts)");
    if (Strm.IsLoading()) scriptThinkers.setLength(sthcount);
    //GCon->Logf("VLSR(%p): %d scripts", (void *)this, sthcount);
    for (int f = 0; f < sthcount; ++f) {
      VSerialisable *obj = scriptThinkers[f];
      Strm << obj;
      if (obj && obj->GetClassName() != "VAcs") Host_Error("Save is broken (loaded `%s` instead of `VAcs`)", *obj->GetClassName());
      //GCon->Logf("VLSR: script #%d: %p", f, (void *)obj);
      scriptThinkers[f] = (VLevelScriptThinker *)obj;
    }
  }

  // script manager
  {
    vuint8 xver = 0;
    Strm << xver;
    if (xver != 0) Host_Error("Save is broken (invalid acs manager version %u)", (unsigned)xver);
    Acs->Serialise(Strm);
  }

  // camera textures
  {
    int NumCamTex = CameraTextures.Num();
    Strm << STRM_INDEX(NumCamTex);
    if (Strm.IsLoading()) CameraTextures.SetNum(NumCamTex);
    for (i = 0; i < NumCamTex; ++i) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("Camera"), CameraTextures[i].Camera);
      vio.io(VName("TexNum"), CameraTextures[i].TexNum);
      vio.io(VName("FOV"), CameraTextures[i].FOV);
    }
  }

  // translation tables
  {
    int NumTrans = Translations.Num();
    Strm << STRM_INDEX(NumTrans);
    if (Strm.IsLoading()) Translations.SetNum(NumTrans);
    for (i = 0; i < NumTrans; ++i) {
      vuint8 Present = !!Translations[i];
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("Present"), Present);
      }
      if (Strm.IsLoading()) {
        if (Present) {
          Translations[i] = new VTextureTranslation;
        } else {
          Translations[i] = nullptr;
        }
      }
      if (Present) Translations[i]->Serialise(Strm);
    }
  }

  // body queue translation tables
  {
    int NumTrans = BodyQueueTrans.Num();
    Strm << STRM_INDEX(NumTrans);
    if (Strm.IsLoading()) BodyQueueTrans.SetNum(NumTrans);
    for (i = 0; i < NumTrans; ++i) {
      vuint8 Present = !!BodyQueueTrans[i];
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("Present"), Present);
      }
      if (Strm.IsLoading()) {
        if (Present) {
          BodyQueueTrans[i] = new VTextureTranslation;
        } else {
          BodyQueueTrans[i] = nullptr;
        }
      }
      if (Present) BodyQueueTrans[i]->Serialise(Strm);
    }
  }

  // zones
  {
    vint32 cnt = NumZones;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumZones) Host_Error("invalid number of zones");
    }

    for (i = 0; i < NumZones; ++i) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("zoneid"), Zones[i]);
    }
  }
}


//==========================================================================
//
//  VLevel::ClearReferences
//
//==========================================================================
void VLevel::ClearReferences () {
  Super::ClearReferences();
  // clear script refs
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) {
    VLevelScriptThinker *sth = scriptThinkers[scidx];
    if (sth && !sth->destroyed) sth->ClearReferences();
  }
  // clear other refs
  sector_t *sec = Sectors;
  for (int i = NumSectors-1; i >= 0; --i, ++sec) {
    if (sec->SoundTarget && (sec->SoundTarget->GetFlags()&_OF_CleanupRef)) sec->SoundTarget = nullptr;
    if (sec->FloorData && (sec->FloorData->GetFlags()&_OF_CleanupRef)) sec->FloorData = nullptr;
    if (sec->CeilingData && (sec->CeilingData->GetFlags()&_OF_CleanupRef)) sec->CeilingData = nullptr;
    if (sec->LightingData && (sec->LightingData->GetFlags()&_OF_CleanupRef)) sec->LightingData = nullptr;
    if (sec->AffectorData && (sec->AffectorData->GetFlags()&_OF_CleanupRef)) sec->AffectorData = nullptr;
    if (sec->ActionList && (sec->ActionList->GetFlags()&_OF_CleanupRef)) sec->ActionList = nullptr;
  }
  // polyobjects
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].SpecialData && (PolyObjs[i].SpecialData->GetFlags()&_OF_CleanupRef)) {
      PolyObjs[i].SpecialData = nullptr;
    }
  }
  // cameras
  for (int i = 0; i < CameraTextures.Num(); ++i) {
    if (CameraTextures[i].Camera && (CameraTextures[i].Camera->GetFlags()&_OF_CleanupRef)) {
      CameraTextures[i].Camera = nullptr;
    }
  }
  // static lights
  // TODO: collect all static lights with owners into separate list for speed
  for (int f = 0; f < NumStaticLights; ++f) {
    rep_light_t &sl = StaticLights[f];
    if (sl.Owner && (sl.Owner->GetFlags()&_OF_CleanupRef)) sl.Owner = nullptr;
  }
  // renderer
  if (RenderData) RenderData->ClearReferences();
}


//==========================================================================
//
//  VLevel::Destroy
//
//==========================================================================
void VLevel::Destroy () {
  decanimlist = nullptr; // why not?

  if (csTouched) Z_Free(csTouched);
  csTouchCount = 0;
  csTouched = nullptr;

  // destroy all thinkers (including scripts)
  DestroyAllThinkers();

  while (HeadSecNode) {
    msecnode_t *Node = HeadSecNode;
    HeadSecNode = Node->SNext;
    delete Node;
    Node = nullptr;
  }

  // free render data
  if (RenderData) {
    delete RenderData;
    RenderData = nullptr;
  }

  for (int i = 0; i < NumPolyObjs; ++i) {
    delete[] PolyObjs[i].segs;
    PolyObjs[i].segs = nullptr;
    delete[] PolyObjs[i].originalPts;
    PolyObjs[i].originalPts = nullptr;
    if (PolyObjs[i].prevPts) {
      delete[] PolyObjs[i].prevPts;
      PolyObjs[i].prevPts = nullptr;
    }
  }
  if (PolyBlockMap) {
    for (int i = 0; i < BlockMapWidth*BlockMapHeight; ++i) {
      for (polyblock_t *pb = PolyBlockMap[i]; pb; ) {
        polyblock_t *Next = pb->next;
        delete pb;
        pb = Next;
      }
    }
    delete[] PolyBlockMap;
    PolyBlockMap = nullptr;
  }
  if (PolyObjs) {
    delete[] PolyObjs;
    PolyObjs = nullptr;
  }
  if (PolyAnchorPoints) {
    delete[] PolyAnchorPoints;
    PolyAnchorPoints = nullptr;
  }

  if (Sectors) {
    // delete regions
    for (int i = 0; i < NumSectors; ++i) Sectors[i].DeleteAllRegions();
    // line buffer is shared, so this correctly deletes it
    delete[] Sectors[0].lines;
    Sectors[0].lines = nullptr;
  }

  if (Segs) {
    for (int f = 0; f < NumSegs; ++f) {
      decal_t *decal = Segs[f].decals;
      while (decal) {
        decal_t *c = decal;
        decal = c->next;
        delete c->animator;
        delete c;
      }
    }
  }

  for (int f = 0; f < NumLines; ++f) {
    line_t *ld = Lines+f;
    delete[] ld->v1lines;
    delete[] ld->v2lines;
  }

  delete[] Vertexes;
  Vertexes = nullptr;
  NumVertexes = 0;
  delete[] Sectors;
  Sectors = nullptr;
  NumSectors = 0;
  delete[] Sides;
  Sides = nullptr;
  NumSides = 0;
  delete[] Lines;
  Lines = nullptr;
  NumLines = 0;
  delete[] Segs;
  Segs = nullptr;
  NumSegs = 0;
  delete[] Subsectors;
  Subsectors = nullptr;
  NumSubsectors = 0;
  delete[] Nodes;
  Nodes = nullptr;
  NumNodes = 0;
  if (VisData) delete[] VisData; else delete[] NoVis;
  VisData = nullptr;
  NoVis = nullptr;
  delete[] BlockMapLump;
  BlockMapLump = nullptr;
  BlockMapLumpSize = 0;
  delete[] BlockLinks;
  BlockLinks = nullptr;
  delete[] RejectMatrix;
  RejectMatrix = nullptr;
  RejectMatrixSize = 0;
  delete[] Things;
  Things = nullptr;
  NumThings = 0;
  delete[] Zones;
  Zones = nullptr;
  NumZones = 0;

  delete[] BaseLines;
  BaseLines = nullptr;
  delete[] BaseSides;
  BaseSides = nullptr;
  delete[] BaseSectors;
  BaseSectors = nullptr;
  delete[] BasePolyObjs;
  BasePolyObjs = nullptr;

  if (Acs) {
    delete Acs;
    Acs = nullptr;
  }
  if (GenericSpeeches) {
    delete[] GenericSpeeches;
    GenericSpeeches = nullptr;
  }
  if (LevelSpeeches) {
    delete[] LevelSpeeches;
    LevelSpeeches = nullptr;
  }
  if (StaticLights) {
    delete[] StaticLights;
    StaticLights = nullptr;
  }

  ActiveSequences.Clear();

  for (int i = 0; i < Translations.Num(); ++i) {
    if (Translations[i]) {
      delete Translations[i];
      Translations[i] = nullptr;
    }
  }
  Translations.Clear();

  for (int i = 0; i < BodyQueueTrans.Num(); ++i) {
    if (BodyQueueTrans[i]) {
      delete BodyQueueTrans[i];
      BodyQueueTrans[i] = nullptr;
    }
  }
  BodyQueueTrans.Clear();

  // openings
  while (openListHead) {
    opening_t *op = openListHead;
    openListHead = op->listnext;
    Z_Free(op);
  }

  while (openListFree) {
    opening_t *op = openListFree;
    openListFree = op->listnext;
    Z_Free(op);
  }

  GTextureManager.ResetMapTextures();

  // call parent class' `Destroy()` method
  Super::Destroy();
}


//==========================================================================
//
//  VLevel::AddStaticLightRGB
//
//==========================================================================
void VLevel::AddStaticLightRGB (VEntity *Ent, const TVec &Origin, float Radius, vuint32 Color) {
  //FIXME: use proper data structure instead of reallocating it again and again
  rep_light_t *OldLights = StaticLights;
  ++NumStaticLights;
  StaticLights = new rep_light_t[NumStaticLights];
  if (OldLights) {
    memcpy(StaticLights, OldLights, (NumStaticLights-1)*sizeof(rep_light_t));
    delete[] OldLights;
  }
  rep_light_t &L = StaticLights[NumStaticLights-1];
  L.Owner = Ent;
  L.Origin = Origin;
  L.Radius = Radius;
  L.Color = Color;
}


//==========================================================================
//
//  VLevel::AddStaticLightRGB
//
//==========================================================================
void VLevel::MoveStaticLightByOwner (VEntity *Ent, const TVec &Origin) {
  //FIXME: use proper data structure instead of reallocating it again and again
  //TODO: write this with hashmap, and replicate properly
  /*
  rep_light_t *stl = StaticLights;
  for (int count = NumStaticLights; count--; ++stl) {
    if (stl->Owner == Ent) {
      stl->Origin = Origin;
    }
  }
  */
  if (RenderData) RenderData->MoveStaticLightByOwner(Ent, Origin);
}


//==========================================================================
//
//  VLevel::SetCameraToTexture
//
//==========================================================================
void VLevel::SetCameraToTexture (VEntity *Ent, VName TexName, int FOV) {
  if (!Ent) return;

  // get texture index
  int TexNum = GTextureManager.CheckNumForName(TexName, TEXTYPE_Wall, true);
  if (TexNum < 0) {
    GCon->Logf("SetCameraToTexture: %s is not a valid texture", *TexName);
    return;
  }

  // make camera to be always relevant
  Ent->ThinkerFlags |= VEntity::TF_AlwaysRelevant;

  for (int i = 0; i < CameraTextures.Num(); ++i) {
    if (CameraTextures[i].TexNum == TexNum) {
      CameraTextures[i].Camera = Ent;
      CameraTextures[i].FOV = FOV;
      return;
    }
  }

  VCameraTextureInfo &C = CameraTextures.Alloc();
  C.Camera = Ent;
  C.TexNum = TexNum;
  C.FOV = FOV;
}


//=============================================================================
//
//  VLevel::AddSecnode
//
//  phares 3/16/98
//
//  Searches the current list to see if this sector is already there.
//  If not, it adds a sector node at the head of the list of sectors this
//  object appears in. This is called when creating a list of nodes that
//  will get linked in later. Returns a pointer to the new node.
//
//=============================================================================
msecnode_t *VLevel::AddSecnode (sector_t *Sec, VEntity *Thing, msecnode_t *NextNode) {
  msecnode_t *Node;

  if (!Sec) Sys_Error("AddSecnode of 0 for %s\n", Thing->GetClass()->GetName());

  Node = NextNode;
  while (Node) {
    // already have a node for this sector?
    if (Node->Sector == Sec) {
      // yes: setting m_thing says 'keep it'
      Node->Thing = Thing;
      return NextNode;
    }
    Node = Node->TNext;
  }

  // couldn't find an existing node for this sector: add one at the head of the list

  // retrieve a node from the freelist
  if (HeadSecNode) {
    Node = HeadSecNode;
    HeadSecNode = HeadSecNode->SNext;
  } else {
    Node = new msecnode_t;
  }

  // killough 4/4/98, 4/7/98: mark new nodes unvisited
  Node->Visited = 0;

  Node->Sector = Sec; // sector
  Node->Thing = Thing; // mobj
  Node->TPrev = nullptr; // prev node on Thing thread
  Node->TNext = NextNode; // next node on Thing thread
  if (NextNode) NextNode->TPrev = Node; // set back link on Thing

  // add new node at head of sector thread starting at Sec->TouchingThingList
  Node->SPrev = nullptr; // prev node on sector thread
  Node->SNext = Sec->TouchingThingList; // next node on sector thread
  if (Sec->TouchingThingList) Node->SNext->SPrev = Node;
  Sec->TouchingThingList = Node;
  return Node;
}


//=============================================================================
//
//  VLevel::DelSecnode
//
//  Deletes a sector node from the list of sectors this object appears in.
//  Returns a pointer to the next node on the linked list, or nullptr.
//
//=============================================================================
msecnode_t *VLevel::DelSecnode (msecnode_t *Node) {
  msecnode_t *tp; // prev node on thing thread
  msecnode_t *tn; // next node on thing thread
  msecnode_t *sp; // prev node on sector thread
  msecnode_t *sn; // next node on sector thread

  if (Node) {
    // unlink from the Thing thread. The Thing thread begins at
    // sector_list and not from VEntity->TouchingSectorList
    tp = Node->TPrev;
    tn = Node->TNext;
    if (tp) tp->TNext = tn;
    if (tn) tn->TPrev = tp;

    // unlink from the sector thread. This thread begins at
    // sector_t->TouchingThingList
    sp = Node->SPrev;
    sn = Node->SNext;
    if (sp) sp->SNext = sn; else Node->Sector->TouchingThingList = sn;
    if (sn) sn->SPrev = sp;

    // return this node to the freelist
    Node->SNext = HeadSecNode;
    HeadSecNode = Node;
    return tn;
  }
  return nullptr;
}
// phares 3/13/98


//=============================================================================
//
//  VLevel::DelSectorList
//
//  Deletes the sector_list and NULLs it.
//
//=============================================================================
void VLevel::DelSectorList () {
  if (SectorList) {
    msecnode_t *Node = SectorList;
    while (Node) Node = DelSecnode(Node);
    SectorList = nullptr;
  }
}


//==========================================================================
//
//  VLevel::SetBodyQueueTrans
//
//==========================================================================
int VLevel::SetBodyQueueTrans (int Slot, int Trans) {
  int Type = Trans>>TRANSL_TYPE_SHIFT;
  int Index = Trans&((1<<TRANSL_TYPE_SHIFT)-1);
  if (Type != TRANSL_Player) return Trans;
  if (Slot < 0 || Slot > MAX_BODY_QUEUE_TRANSLATIONS || Index < 0 ||
      Index >= MAXPLAYERS || !LevelInfo->Game->Players[Index])
  {
    return Trans;
  }

  // add it
  while (BodyQueueTrans.Num() <= Slot) BodyQueueTrans.Append(nullptr);
  VTextureTranslation *Tr = BodyQueueTrans[Slot];
  if (!Tr) {
    Tr = new VTextureTranslation;
    BodyQueueTrans[Slot] = Tr;
  }
  Tr->Clear();
  VBasePlayer *P = LevelInfo->Game->Players[Index];
  Tr->BuildPlayerTrans(P->TranslStart, P->TranslEnd, P->Color);
  return (TRANSL_BodyQueue<<TRANSL_TYPE_SHIFT)+Slot;
}


//==========================================================================
//
//  VLevel::FindSectorFromTag
//
//==========================================================================
int VLevel::FindSectorFromTag (int tag, int start) {
  if (tag == 0 || NumSectors < 1) return -1; //k8: just in case
  for (int i = start < 0 ? Sectors[(vuint32)tag%(vuint32)NumSectors].HashFirst : Sectors[start].HashNext;
       i >= 0;
       i = Sectors[i].HashNext)
  {
    if (Sectors[i].tag == tag) return i;
  }
  return -1;
}


//==========================================================================
//
//  VLevel::FindLine
//
//==========================================================================
line_t *VLevel::FindLine (int lineTag, int *searchPosition) {
  if (NumLines > 0) {
    for (int i = *searchPosition < 0 ? Lines[(vuint32)lineTag%(vuint32)NumLines].HashFirst : Lines[*searchPosition].HashNext;
         i >= 0;
         i = Lines[i].HashNext)
    {
      if (Lines[i].LineTag == lineTag) {
        *searchPosition = i;
        return &Lines[i];
      }
    }
  }
  *searchPosition = -1;
  return nullptr;
}


//==========================================================================
//
//  VLevel::SectorSetLink
//
//==========================================================================
void VLevel::SectorSetLink (int controltag, int tag, int surface, int movetype) {
  if (controltag <= 0) return;
  if (tag <= 0) return;
  if (tag == controltag) return;
  //FIXME: just enough to let annie working
  if (surface != 0 || movetype != 1) {
    GCon->Logf(NAME_Warning, "UNIMPLEMENTED: setting sector link: controltag=%d; tag=%d; surface=%d; movetype=%d", controltag, tag, surface, movetype);
    return;
  }
  for (int csi = FindSectorFromTag(controltag); csi >= 0; csi = FindSectorFromTag(controltag, csi)) {
    for (int lsi = FindSectorFromTag(tag); lsi >= 0; lsi = FindSectorFromTag(tag, lsi)) {
      if (lsi == csi) continue;
      if (csi < sectorlinkStart.length()) {
        int f = sectorlinkStart[csi];
        while (f >= 0) {
          if (sectorlinks[f].index == lsi) break;
          f = sectorlinks[f].next;
        }
        if (f >= 0) continue;
      }
      // add it
      //GCon->Logf("linking sector #%d (tag=%d) to sector #%d (controltag=%d)", lsi, tag, csi, controltag);
      while (csi >= sectorlinkStart.length()) sectorlinkStart.append(-1);
      //GCon->Logf("  csi=%d; len=%d", csi, sectorlinkStart.length());
      //GCon->Logf("  csi=%d; sl=%d", csi, sectorlinkStart[csi]);
      // allocate sectorlink
      int slidx = sectorlinks.length();
      SectorLink &sl = sectorlinks.alloc();
      sl.index = lsi;
      sl.mts = (movetype&0x0f)|(surface ? 1<<30 : 0);
      sl.next = sectorlinkStart[csi];
      sectorlinkStart[csi] = slidx;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// intersector sound propagation code
// moved here 'cause levels like Vela Pax with ~10000 interconnected sectors
// causes a huge slowdown on shooting
// will be moved back to VM when i'll implement JIT compiler

//private transient array!Entity recSoundSectorEntities; // will be collected in native code

struct SoundSectorListItem {
  sector_t *sec;
  int sblock;
};


static TArray<SoundSectorListItem> recSoundSectorList;
static TMapNC<VEntity *, bool> recSoundSectorSeenEnts;


//==========================================================================
//
//  VLevel::processRecursiveSoundSectorList
//
//==========================================================================
void VLevel::processSoundSector (int validcount, TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin) {
  if (!sec) return;

  // `validcount` and other things were already checked in caller
  // also, caller already set `soundtraversed` and `SoundTarget`

  int hmask = 0;
  if (!gm_compat_everything_can_hear) {
    hmask = VEntity::EF_NoSector|VEntity::EF_NoBlockmap;
    if (!gm_compat_corpses_can_hear) hmask |= VEntity::EF_Corpse;
  }

  for (VEntity *Ent = sec->ThingList; Ent; Ent = Ent->SNext) {
    if (recSoundSectorSeenEnts.has(Ent)) continue;
    recSoundSectorSeenEnts.put(Ent, true);
    if (Ent == soundtarget) continue; // skip target
    //FIXME: skip some entities that cannot (possibly) react
    //       this can break some code, but... meh
    //       maybe don't omit corpses?
    if (Ent->EntityFlags&hmask) continue;
    // check max distance
    if (maxdist > 0 && length2D(sndorigin-Ent->Origin) > maxdist) continue;
    // register for processing
    elist.append(Ent);
  }

  for (int i = 0; i < sec->linecount; ++i) {
    line_t *check = sec->lines[i];
    if (check->sidenum[1] == -1 || !(check->flags&ML_TWOSIDED)) continue;

    // early out for intra-sector lines
    if (check->frontsector == check->backsector) continue;

    if (!SV_LineOpenings(check, *check->v1, 0xffffffff)) {
      if (!SV_LineOpenings(check, *check->v2, 0xffffffff)) {
        // closed door
        continue;
      }
    }

    sector_t *other = (check->frontsector == sec ? check->backsector : check->frontsector);
    if (!other) continue; // just in case

    bool addIt = false;
    int sblock;

    if (check->flags&ML_SOUNDBLOCK) {
      if (!soundblocks) {
        //RecursiveSound(other, 1, soundtarget, Splash, maxdist!optional, emmiter!optional);
        addIt = true;
        sblock = 1;
      }
    } else {
      //RecursiveSound(other, soundblocks, soundtarget, Splash, maxdist!optional, emmiter!optional);
      addIt = true;
      sblock = soundblocks;
    }

    if (addIt) {
      // don't add one sector several times
      if (other->validcount == validcount && other->soundtraversed <= sblock+1) continue; // already flooded
      // set flags
      other->validcount = validcount;
      other->soundtraversed = sblock+1;
      other->SoundTarget = soundtarget;
      // add to processing list
      SoundSectorListItem &sl = recSoundSectorList.alloc();
      sl.sec = other;
      sl.sblock = sblock;
    }
  }
}


//==========================================================================
//
//  RecursiveSound
//
//  Called by NoiseAlert. Recursively traverse adjacent sectors, sound
//  blocking lines cut off traversal.
//
//==========================================================================
void VLevel::doRecursiveSound (int validcount, TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin) {
  // wake up all monsters in this sector
  if (!sec || (sec->validcount == validcount && sec->soundtraversed <= soundblocks+1)) return; // already flooded

  sec->validcount = validcount;
  sec->soundtraversed = soundblocks+1;
  sec->SoundTarget = soundtarget;

  recSoundSectorList.clear();
  recSoundSectorSeenEnts.reset();
  processSoundSector(validcount, elist, sec, soundblocks, soundtarget, maxdist, sndorigin);

  if (maxdist < 0) maxdist = 0;
  if (gm_compat_max_hearing_distance > 0 && (maxdist == 0 || maxdist > gm_compat_max_hearing_distance)) maxdist = gm_compat_max_hearing_distance;

  // don't use `foreach` here!
  int rspos = 0;
  while (rspos < recSoundSectorList.length()) {
    processSoundSector(validcount, elist, recSoundSectorList[rspos].sec, recSoundSectorList[rspos].sblock, soundtarget, maxdist, sndorigin);
    ++rspos;
  }

  //if (recSoundSectorList.length > 1) print("RECSOUND: len=%d", recSoundSectorList.length);
  recSoundSectorList.clear();
  recSoundSectorSeenEnts.reset();
}


//==========================================================================
//
//  VLevel::AddAnimatedDecal
//
//==========================================================================
void VLevel::AddAnimatedDecal (decal_t *dc) {
  if (!dc || dc->prevanimated || dc->nextanimated || decanimlist == dc || !dc->animator) return;
  if (decanimlist) decanimlist->prevanimated = dc;
  dc->nextanimated = decanimlist;
  decanimlist = dc;
}


//==========================================================================
//
//  VLevel::RemoveAnimatedDecal
//
//  this will also kill animator
//
//==========================================================================
void VLevel::RemoveAnimatedDecal (decal_t *dc) {
  if (!dc || (!dc->prevanimated && !dc->nextanimated && decanimlist != dc)) return;
  if (dc->prevanimated) dc->prevanimated->nextanimated = dc->nextanimated; else decanimlist = dc->nextanimated;
  if (dc->nextanimated) dc->nextanimated->prevanimated = dc->prevanimated;
  delete dc->animator;
  dc->animator = nullptr;
  dc->prevanimated = dc->nextanimated = nullptr;
}


//==========================================================================
//
//  lif2str
//
//==========================================================================
static __attribute__((unused)) const char *lif2str (int flags) {
  static char buf[128];
  char *pp = buf;
  *pp++ = '<';
  if (flags&ML_TWOSIDED) *pp++ = '2';
  if (flags&ML_DONTPEGTOP) *pp++ = 'T';
  if (flags&ML_DONTPEGBOTTOM) *pp++ = 'B';
  *pp++ = '>';
  *pp = 0;
  return buf;
}


//==========================================================================
//
//  isDecalsOverlap
//
//==========================================================================
static bool isDecalsOverlap (VDecalDef *dec, float dcx0, float dcy0, decal_t *cur, VTexture *DTex) {
  const float twdt = DTex->GetScaledWidth()*dec->scaleX;
  const float thgt = DTex->GetScaledHeight()*dec->scaleY;

  const float txofs = DTex->GetScaledSOffset()*dec->scaleX;
  const float tyofs = DTex->GetScaledTOffset()*dec->scaleY;

  const float myx0 = dcx0;
  const float myx1 = myx0+twdt;
  const float myy0 = dcy0;
  const float myy1 = myy0+thgt;

  const float itx0 = cur->xdist-txofs;
  const float itx1 = itx0+twdt;
  const float ity1 = cur->orgz+cur->scaleY+tyofs;
  const float ity0 = ity1-thgt;

  /*
  GCon->Logf("  my=(%g,%g)-(%g,%g)", myx0, myy0, myx1, myy1);
  GCon->Logf("  it=(%g,%g)-(%g,%g)", itx0, ity0, itx1, ity1);
  */

  return !(itx1 <= myx0 || ity1 <= myy0 || itx0 >= myx1 || ity0 >= myy1);
}


//==========================================================================
//
//  VLevel::PutDecalAtLine
//
//==========================================================================
void VLevel::PutDecalAtLine (int tex, float orgz, float lineofs, VDecalDef *dec, int side, line_t *li, vuint32 flips) {
  // don't process linedef twice
  if (li->decalMark == decanimuid) return;
  li->decalMark = decanimuid;

  VTexture *DTex = GTextureManager[tex];
  if (!DTex || DTex->Type == TEXTYPE_Null) return;

  //GCon->Logf("decal '%s' at linedef %d", *GTextureManager[tex]->Name, (int)(ptrdiff_t)(li-Lines));

  float twdt = DTex->GetScaledWidth()*dec->scaleX;
  float thgt = DTex->GetScaledHeight()*dec->scaleY;

  if (twdt < 1 || thgt < 1) return;

  float txofs = DTex->GetScaledSOffset()*dec->scaleX;
  float tyofs = DTex->GetScaledTOffset()*dec->scaleY;

  const float dcx0 = lineofs-txofs;
  const float dcx1 = dcx0+twdt;
  const float dcy1 = orgz+dec->scaleY+tyofs;
  const float dcy0 = dcy1-thgt;

  const TVec &v1 = *li->v1;
  const TVec &v2 = *li->v2;
  const float linelen = (v2-v1).length2D();

  // check if decal is in line bounds
  if (dcx1 <= 0 || dcx0 >= linelen) return; // out of bounds

  int dcmaxcount = r_decal_onetype_max;
       if (twdt >= 128 || thgt >= 128) dcmaxcount = 8;
  else if (twdt >= 64 || thgt >= 64) dcmaxcount = 16;
  else if (twdt >= 32 || thgt >= 32) dcmaxcount = 32;
  //HACK!
  if (VStr::startsWithCI(*dec->name, "K8Gore")) dcmaxcount = r_decal_gore_onetype_max;

  sector_t *fsec, *bsec;
  if (side == 0) {
    fsec = li->frontsector;
    bsec = li->backsector;
  } else {
    fsec = li->backsector;
    bsec = li->frontsector;
  }

  if (!fsec) {
    side = 1-side;
    fsec = bsec;
    if (!bsec) Sys_Error("oops; something went wrong in decal code!");
  }

#ifdef VAVOOM_DECALS_DEBUG
  GCon->Logf("Decal '%s' at line #%d (side %d; fs=%d; bs=%d): linelen=%g; o0=%g; o1=%g (ofsorig=%g; txofs=%g; tyofs=%g; tw=%g; th=%g)", *dec->name, (int)(ptrdiff_t)(li-Lines), side, (int)(ptrdiff_t)(fsec-Sectors), (bsec ? (int)(ptrdiff_t)(bsec-Sectors) : -1), linelen, dcx0, dcx1, lineofs, txofs, tyofs, twdt, thgt);
#endif

  TVec linepos = v1+li->ndir*lineofs;

  const float ffloorZ = fsec->floor.GetPointZ(linepos);
  const float fceilingZ = fsec->ceiling.GetPointZ(linepos);

  const float bfloorZ = (bsec ? bsec->floor.GetPointZ(linepos) : ffloorZ);
  const float bceilingZ = (bsec ? bsec->ceiling.GetPointZ(linepos) : fceilingZ);

  if ((li->flags&ML_NODECAL) == 0) {
    // find segs for this decal (there may be several segs)
    // for two-sided lines, put decal on segs for both sectors
    for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
      if (!seg->linedef) continue; // ignore minisegs (just in case)
      check(seg->linedef == li);

#ifdef VAVOOM_DECALS_DEBUG
      GCon->Logf("  checking seg #%d; offset=%g; length=%g", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length);
#endif

      // check if decal is in seg bounds
      if (dcx1 <= seg->offset || dcx0 >= seg->offset+seg->length) continue; // out of bounds

      side_t *sb = seg->sidedef;

      // check if decal is allowed on this side
      if (sb->MidTexture == skyflatnum) continue; // never on the sky

      bool slideWithFloor = false;
      bool slideWithCeiling = false;
      sector_t *slidesec = nullptr;
      bool hasMidTex = true;

      if (sb->MidTexture <= 0 || GTextureManager(sb->MidTexture)->Type == TEXTYPE_Null) {
        hasMidTex = false;
      }

      // check if we have top/bottom textures
      bool allowTopTex = (sb->TopTexture > 0 && sb->TopTexture != skyflatnum);
      bool allowBotTex = (sb->BottomTexture > 0 && sb->BottomTexture != skyflatnum);
      if (allowTopTex) {
        VTexture *xtx = GTextureManager(sb->TopTexture);
        allowTopTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
      }
      if (allowBotTex) {
        VTexture *xtx = GTextureManager(sb->BottomTexture);
        allowBotTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
      }
      // can we hit toptex?
      if (allowTopTex) {
        if (fsec && bsec) {
          // if there is no ceiling height difference, toptex cannot be visible
          if (fsec->ceiling.minz == bsec->ceiling.minz &&
              fsec->ceiling.maxz == bsec->ceiling.maxz)
          {
            allowTopTex = false;
          } else if (fsec->ceiling.minz <= bsec->ceiling.minz) {
            // if front ceiling is lower than back ceiling, toptex cannot be visible
            allowTopTex = false;
          } else if (dcy1 <= min2(fceilingZ, bceilingZ)) {
            // if decal top is lower than lowest ceiling, consider toptex invisible
            // (i assume that we won't have animators sliding up)
            allowTopTex = false;
          }
        } else {
          // one-sided: see the last coment above
          if (dcy1 <= fceilingZ) allowTopTex = false;
        }
      }
      // can we hit bottex?
      if (allowBotTex) {
        if (fsec && bsec) {
          // if there is no floor height difference, bottex cannot be visible
          if (fsec->floor.minz == bsec->floor.minz &&
              fsec->floor.maxz == bsec->floor.maxz)
          {
            allowBotTex = false;
          } else if (fsec->floor.maxz >= bsec->floor.maxz) {
            // if front floor is higher than back floor, bottex cannot be visible
            allowBotTex = false;
          } else if (!dec->animator && dcy0 >= max2(ffloorZ, bfloorZ)) {
            // if decal bottom is higher than highest floor, consider toptex invisible
            // (but don't do this for animated decals -- this may be sliding blood)
            allowBotTex = false;
          }
        } else {
          // one-sided: see the last coment above
          if (!dec->animator && dcy0 >= ffloorZ) allowBotTex = false;
        }
      }

      // if no textures were hit, don't bother
      if (!hasMidTex && !allowTopTex && !allowBotTex) continue;

      if (fsec && bsec) {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Logf("  2s: orgz=%g; front=(%g,%g); back=(%g,%g)", orgz, ffloorZ, fceilingZ, bfloorZ, bceilingZ);
#endif
        if (hasMidTex && orgz > max2(ffloorZ, bfloorZ) && orgz < min2(fceilingZ, bceilingZ)) {
          // midtexture
               if (li->flags&ML_DONTPEGBOTTOM) slideWithFloor = true;
          else if (li->flags&ML_DONTPEGTOP) slideWithCeiling = true;
          else slideWithCeiling = true;
        } else {
          if (allowTopTex && allowBotTex) {
            // both top and bottom
            if (orgz < max2(ffloorZ, bfloorZ)) {
              // bottom texture
              if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
            } else if (orgz > min2(fceilingZ, bceilingZ)) {
              // top texture
              if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            }
          } else if (allowBotTex) {
            // only bottom texture
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
          } else if (allowTopTex) {
            // only top texture
            if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
          }
#ifdef VAVOOM_DECALS_DEBUG
          GCon->Logf("  2s: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
#endif
        }

        // door hack
        /*
        if (!slideWithFloor && !slideWithCeiling) {
          if (ffloorZ == fceilingZ || bfloorZ == bceilingZ) {
            slideWithCeiling = (bfloorZ == ffloorZ);
            slideWithFloor = !slideWithCeiling;
            slidesec = (ffloorZ == fceilingZ ? fsec : bsec);
            //GCon->Logf("DOOR HACK: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
          }
        }
        */
      } else {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Logf("  1s: orgz=%g; front=(%g,%g)", orgz, ffloorZ, fceilingZ);
#endif
        // one-sided
        if (hasMidTex && orgz > ffloorZ && orgz < fceilingZ) {
          // midtexture
               if (li->flags&ML_DONTPEGBOTTOM) slideWithFloor = true;
          else if (li->flags&ML_DONTPEGTOP) slideWithCeiling = true;
          else slideWithCeiling = true;
          //GCon->Logf("one-sided midtex: pegbot=%d; pegtop=%d; fslide=%d; cslide=%d", (int)(!!(li->flags&ML_DONTPEGBOTTOM)), (int)(!!(li->flags&ML_DONTPEGTOP)), (int)slideWithFloor, (int)slideWithCeiling);
        } else {
          if (allowTopTex && allowBotTex) {
            // both top and bottom
            if (orgz < ffloorZ) {
              // bottom texture
              if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
            } else if (orgz > fceilingZ) {
              // top texture
              if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            }
          } else if (allowBotTex) {
            // only bottom texture
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
          } else if (allowTopTex) {
            // only top texture
            if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
          }
        }
        if (slideWithFloor || slideWithCeiling) slidesec = fsec;
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Logf("  1s: front=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
#endif
      }

      // remove old same-typed decals, if necessary
      if (dcmaxcount > 0 && dcmaxcount < 10000) {
        int count = 0;
        decal_t *prev = nullptr;
        decal_t *first = nullptr;
        decal_t *cur = seg->decals;
        while (cur) {
          // also, check if this decal is touching our one
          if (cur->dectype == dec->name) {
            //GCon->Logf("seg #%d: decal '%s'", (int)(ptrdiff_t)(seg-Segs), *cur->dectype);
            if (isDecalsOverlap(dec, dcx0, dcy0, cur, DTex)) {
              //GCon->Log("  overlap!");
              if (!first) first = cur;
              ++count;
            }
          }
          if (!first) prev = cur;
          cur = cur->next;
        }
        if (count >= dcmaxcount) {
          //GCon->Logf("removing %d extra '%s' decals (of %d)", count-dcmaxcount+1, *dec->name, dcmaxcount);
          // do removal
          decal_t *currd = first;
          if (prev) {
            if (prev->next != currd) Sys_Error("decal oops(0)");
          } else {
            if (seg->decals != currd) Sys_Error("decal oops(1)");
          }
          while (currd) {
            decal_t *n = currd->next;
            if (currd->dectype == dec->name && isDecalsOverlap(dec, dcx0, dcy0, currd, DTex)) {
              if (prev) prev->next = n; else seg->decals = n;
              RemoveAnimatedDecal(currd);
              delete currd;
              if (--count < dcmaxcount) break;
            }
            currd = n;
          }
        }
      }

#ifdef VAVOOM_DECALS_DEBUG
      GCon->Logf("  decaling seg #%d; offset=%g; length=%g", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length);
#endif

      // create decal
      decal_t *decal = new decal_t;
      memset((void *)decal, 0, sizeof(decal_t));
      decal_t *cdec = seg->decals;
      if (cdec) {
        while (cdec->next) cdec = cdec->next;
        cdec->next = decal;
      } else {
        seg->decals = decal;
      }
      decal->seg = seg;
      decal->dectype = dec->name;
      decal->texture = tex;
      decal->orgz = decal->curz = orgz;
      decal->xdist = lineofs;
      decal->ofsX = decal->ofsY = 0;
      decal->scaleX = decal->origScaleX = dec->scaleX;
      decal->scaleY = decal->origScaleY = dec->scaleY;
      decal->alpha = decal->origAlpha = dec->alpha;
      decal->addAlpha = dec->addAlpha;
      decal->animator = (dec->animator ? dec->animator->clone() : nullptr);
      if (decal->animator) AddAnimatedDecal(decal);

      // setup misc flags
      decal->flags = flips|(dec->fullbright ? decal_t::Fullbright : 0)|(dec->fuzzy ? decal_t::Fuzzy : 0);

      // setup curz and pegs
      if (slideWithFloor) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideFloor;
          decal->curz -= decal->slidesec->floor.TexZ;
#ifdef VAVOOM_DECALS_DEBUG
          GCon->Logf("  floor slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
#endif
        }
      } else if (slideWithCeiling) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideCeil;
          decal->curz -= decal->slidesec->ceiling.TexZ;
#ifdef VAVOOM_DECALS_DEBUG
          GCon->Logf("  ceil slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
#endif
        }
      }

      if (side != seg->side) decal->flags ^= decal_t::FlipX;
    }
  }

  // if our decal is not completely at linedef, spread it to adjacent linedefs
  if (dcx0 < 0) {
    // to the left
#ifdef VAVOOM_DECALS_DEBUG
    GCon->Logf("Decal '%s' at line #%d: going to the left; ofs=%g", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx0);
#endif
    line_t **ngb = li->v1lines;
    for (int ngbCount = li->v1linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (li->v1 == nline->v2) {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Log("  v1 at nv2");
#endif
        PutDecalAtLine(tex, orgz, ((*nline->v2)-(*nline->v1)).length2D()+dcx0+txofs, dec, (nline->frontsector == fsec ? 0 : 1), nline, flips);
      } else if (li->v1 == nline->v1) {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Log("  v1 at nv1");
#endif
        //PutDecalAtLine(tex, orgz, -(twdt+dcx0)+txofs, dec, (nline->frontsector == fsec ? 0 : 1), nline, flips);
      }
    }
  }

  if (dcx1 > linelen) {
    // to the right
#ifdef VAVOOM_DECALS_DEBUG
    GCon->Logf("Decal '%s' at line #%d: going to the right; ofs=%g", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx1);
#endif
    line_t **ngb = li->v2lines;
    for (int ngbCount = li->v2linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (li->v2 == nline->v1) {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Log("  v2 at nv1");
#endif
        PutDecalAtLine(tex, orgz, dcx0-linelen+txofs, dec, (nline->frontsector == fsec ? 0 : 1), nline, flips);
      } else if (li->v2 == nline->v2) {
#ifdef VAVOOM_DECALS_DEBUG
        GCon->Log("  v2 at nv2");
#endif
        //PutDecalAtLine(tex, orgz, ((*nline->v2)-(*nline->v1)).length2D()+twdt-(dcx0-linelen)+txofs, dec, (nline->frontsector == fsec ? 0 : 1), nline, flips);
      }
    }
  }
}


//==========================================================================
//
// VLevel::AddOneDecal
//
//==========================================================================
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, int side, line_t *li) {
  if (!dec || !li) return;

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  if (dec->lowername != NAME_None) {
    //GCon->Logf("adding lower decal '%s' for decal '%s' (level %d)", *dec->lowername, *dec->name, level);
    AddDecal(org, dec->lowername, side, li, level+1);
  }

  if (dec->scaleX <= 0 || dec->scaleY <= 0) {
    GCon->Logf("Decal '%s' has zero scale", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (dec->alpha <= 0.004f) {
    GCon->Logf("Decal '%s' has zero alpha", *dec->name);
    return;
  }

  int tex = dec->texid;
  VTexture *DTex = GTextureManager[tex];
  if (!DTex || DTex->Type == TEXTYPE_Null) {
    // no decal gfx, nothing to do
    GCon->Logf("Decal '%s' has no pic", *dec->name);
    return;
  }

  //GCon->Logf("Decal '%s', texture '%s'", *dec->name, *DTex->Name);

  if (++decanimuid == 0x7fffffff) {
    decanimuid = 1;
    for (int f = 0; f < NumLines; ++f) {
      line_t *ld = Lines+f;
      if (ld->decalMark != -1) ld->decalMark = 0;
    }
  }

  // setup flips
  vuint32 flips = 0;
  if (dec->flipX == VDecalDef::FlipRandom) {
    if (Random() < 0.5f) flips |= decal_t::FlipX;
  } else if (dec->flipX == VDecalDef::FlipAlways) {
    flips |= decal_t::FlipX;
  }
  if (dec->flipY == VDecalDef::FlipRandom) {
    if (Random() < 0.5f) flips |= decal_t::FlipY;
  } else if (dec->flipY == VDecalDef::FlipAlways) {
    flips |= decal_t::FlipY;
  }

  // calculate offset from line start
  const TVec &v1 = *li->v1;
  const TVec &v2 = *li->v2;

  float dx = v2.x-v1.x;
  float dy = v2.y-v1.y;
  float dist = 0; // distance from wall start
       if (fabsf(dx) > fabsf(dy)) dist = (org.x-v1.x)/dx;
  else if (dy != 0) dist = (org.y-v1.y)/dy;
  else dist = 0;

  const float lineofs = dist*(v2-v1).length2D();
#ifdef VAVOOM_DECALS_DEBUG
  GCon->Logf("linelen=%g; dist=%g; lineofs=%g", (v2-v1).length2D(), dist, lineofs);
#endif

  PutDecalAtLine(tex, org.z, lineofs, dec, side, li, flips);
}


//==========================================================================
//
// VLevel::AddDecal
//
//==========================================================================
void VLevel::AddDecal (TVec org, const VName &dectype, int side, line_t *li, int level) {
  if (!r_decals_enabled) return;
  if (!li || dectype == NAME_None) return; // just in case

  //GCon->Logf("%s: oorg:(%g,%g,%g); org:(%g,%g,%g)", *dectype, org.x, org.y, org.z, li->landAlongNormal(org).x, li->landAlongNormal(org).y, li->landAlongNormal(org).z);

  org = li->landAlongNormal(org);

  static TStrSet baddecals;

#ifdef VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
  VDecalDef *dec = VDecalDef::getDecal(VName("k8TestDecal"));
#else
  VDecalDef *dec = VDecalDef::getDecal(dectype);
#endif
  if (dec) {
    //GCon->Logf("DECAL '%s'; name is '%s', texid is %d; org=(%g,%g,%g)", *dectype, *dec->name, dec->texid, org.x, org.y, org.z);
    AddOneDecal(level, org, dec, side, li);
  } else {
    if (!baddecals.put(*dectype)) GCon->Logf("NO DECAL: '%s'", *dectype);
  }
}


//==========================================================================
//
// VLevel::AddDecalById
//
//==========================================================================
void VLevel::AddDecalById (TVec org, int id, int side, line_t *li, int level) {
  if (!r_decals_enabled) return;
  if (!li || id < 0) return; // just in case

  org = li->landAlongNormal(org);

  VDecalDef *dec = VDecalDef::getDecalById(id);
  if (dec) AddOneDecal(level, org, dec, side, li);
}


//==========================================================================
//
//  checkPlaneHit
//
//  WARNING: `currhit` should not be the same as `lineend`!
//
//==========================================================================
bool VLevel::CheckPlaneHit (const TSecPlaneRef &plane, const TVec &linestart, const TVec &lineend, TVec &currhit, bool &isSky, const float threshold) {
  const float orgDist = plane.DotPointDist(linestart);
  if (orgDist < threshold) return true; // don't shoot back side

  const float hitDist = plane.DotPointDist(lineend);
  if (hitDist >= threshold) return true; // didn't hit plane

  currhit = lineend;
  // sky?
  if (plane.splane->pic == skyflatnum) {
    // don't shoot the sky!
    isSky = true;
  } else {
    isSky = false;
    currhit -= (lineend-linestart)*hitDist/(hitDist-orgDist);
  }

  // don't go any farther
  return false;
}


#define UPDATE_PLANE_HIT(plane_)  do { \
  if (!CheckPlaneHit((plane_), linestart, lineend, currhit, isSky, threshold)) { \
    wasHit = true; \
    float dist = (currhit-linestart).lengthSquared(); \
    if (dist < besthdist) { \
      besthit = currhit; \
      bestIsSky = isSky; \
      besthdist = dist; \
      bestNormal = (plane_).GetNormal(); \
    } \
  } \
} while (0)


//==========================================================================
//
//  VLevel::CheckHitPlanes
//
//  checks all sector regions, returns `false` if any region plane was hit
//  sets `outXXX` arguments on hit (and only on hit!)
//  if `checkSectorBounds` is false, skip checking sector bounds
//  (and the first sector region)
//
//  any `outXXX` can be `nullptr`
//
//==========================================================================
bool VLevel::CheckHitPlanes (sector_t *sector, bool checkSectorBounds, TVec linestart, TVec lineend, unsigned flagmask,
                             TVec *outHitPoint, TVec *outHitNormal, bool *outIsSky, const float threshold)
{
  if (!sector) return true;

  TVec besthit = lineend;
  TVec bestNormal(0.0f, 0.0f, 0.0f);
  bool bestIsSky = false;
  TVec currhit(0.0f, 0.0f, 0.0f);
  bool wasHit = false;
  float besthdist = 9999999.0f;
  bool isSky = false;

  if (checkSectorBounds) {
    // make fake floors and ceilings block view
    TSecPlaneRef bfloor, bceil;
    sector_t *hs = sector->heightsec;
    if (!hs) hs = sector;
    bfloor.set(&hs->floor, false);
    bceil.set(&hs->ceiling, false);
    // check sector floor
    UPDATE_PLANE_HIT(bfloor);
    // check sector ceiling
    UPDATE_PLANE_HIT(bceil);
  }

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&sec_region_t::RF_OnlyVisual) continue;
    if ((reg->efloor.splane->flags&flagmask) == 0) {
      UPDATE_PLANE_HIT(reg->efloor);
    }
    if ((reg->eceiling.splane->flags&(unsigned)flagmask) == 0) {
      UPDATE_PLANE_HIT(reg->eceiling);
    }
  }

  if (wasHit) {
    // hit floor or ceiling
    if (outHitPoint) *outHitPoint = besthit;
    if (outHitNormal) *outHitNormal = bestNormal;
    if (outIsSky) *outIsSky = bestIsSky;
    return false;
  } else {
    return true;
  }
}


//==========================================================================
//
//  CalcLine
//
//==========================================================================
void CalcLine (line_t *line) {
  // calc line's slopetype
  line->dir = (*line->v2)-(*line->v1);
  if (!line->dir.x) {
    line->slopetype = ST_VERTICAL;
  } else if (!line->dir.y) {
    line->slopetype = ST_HORIZONTAL;
  } else {
    if (line->dir.y/line->dir.x > 0) {
      line->slopetype = ST_POSITIVE;
    } else {
      line->slopetype = ST_NEGATIVE;
    }
  }

  line->SetPointDirXY(*line->v1, line->dir);
  line->ndir = line->dir.normalised2D();

  // calc line's bounding box
  if (line->v1->x < line->v2->x) {
    line->bbox[BOXLEFT] = line->v1->x;
    line->bbox[BOXRIGHT] = line->v2->x;
  } else {
    line->bbox[BOXLEFT] = line->v2->x;
    line->bbox[BOXRIGHT] = line->v1->x;
  }

  if (line->v1->y < line->v2->y) {
    line->bbox[BOXBOTTOM] = line->v1->y;
    line->bbox[BOXTOP] = line->v2->y;
  } else {
    line->bbox[BOXBOTTOM] = line->v2->y;
    line->bbox[BOXTOP] = line->v1->y;
  }
}


//==========================================================================
//
//  CalcSeg
//
//==========================================================================
void CalcSeg (seg_t *seg) {
  seg->Set2Points(*seg->v1, *seg->v2);
  if (seg->length <= 0.0f) {
    seg->dir = TVec(1, 0, 0); // arbitrary
  } else {
    seg->dir = ((*seg->v2)-(*seg->v1)).normalised2D();
    if (!seg->dir.isValid() || seg->dir.isZero()) seg->dir = TVec(1, 0, 0); // arbitrary
  }
}


#ifdef SERVER
//==========================================================================
//
//  SV_LoadLevel
//
//==========================================================================
void SV_LoadLevel (VName MapName) {
#ifdef CLIENT
  GClLevel = nullptr;
#endif
  if (GLevel) {
    delete GLevel;
    GLevel = nullptr;
  }

  GLevel = SpawnWithReplace<VLevel>();
  GLevel->LevelFlags |= VLevel::LF_ForServer;

  GLevel->LoadMap(MapName);
  Host_ResetSkipFrames();
}
#endif


#ifdef CLIENT
//==========================================================================
//
//  CL_LoadLevel
//
//==========================================================================
void CL_LoadLevel (VName MapName) {
  if (GClLevel) {
    delete GClLevel;
    GClLevel = nullptr;
  }

  GClLevel = SpawnWithReplace<VLevel>();
  GClGame->GLevel = GClLevel;

  GClLevel->LoadMap(MapName);
  Host_ResetSkipFrames();
}
#endif


//==========================================================================
//
//  VLevel::dumpSectorRegions
//
//==========================================================================
void VLevel::dumpRegion (const sec_region_t *reg) {
  if (!reg) return;
  char xflags[128];
  xflags[0] = 0;
  if (reg->regflags&sec_region_t::RF_SaneRegion) strcat(xflags, " [sane]");
  if (reg->regflags&sec_region_t::RF_NonSolid) strcat(xflags, " [non-solid]");
  if (reg->regflags&sec_region_t::RF_OnlyVisual) strcat(xflags, " [visual]");
  if (reg->regflags&sec_region_t::RF_SkipFloorSurf) strcat(xflags, " [skip-floor]");
  if (reg->regflags&sec_region_t::RF_SkipCeilSurf) strcat(xflags, " [skip-ceil]");
  GCon->Logf("  %p: floor=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x%s",
    reg,
    reg->efloor.GetNormal().x, reg->efloor.GetNormal().y, reg->efloor.GetNormal().z, reg->efloor.GetDist(),
    reg->efloor.splane->minz, reg->efloor.splane->maxz,
    reg->efloor.splane->flags,
    reg->eceiling.GetNormal().x, reg->eceiling.GetNormal().y, reg->eceiling.GetNormal().z, reg->eceiling.GetDist(),
    reg->eceiling.splane->minz, reg->eceiling.splane->maxz,
    reg->eceiling.splane->flags,
    (reg->extraline ? 1 : 0),
    reg->regflags, xflags);
}


//==========================================================================
//
//  VLevel::dumpSectorRegions
//
//==========================================================================
void VLevel::dumpSectorRegions (const sector_t *dst) {
  GCon->Logf(" === bot -> top (sector: %p) ===", dst);
  for (const sec_region_t *reg = dst->eregions; reg; reg = reg->next) dumpRegion(reg);
  GCon->Log("--------");
}


//==========================================================================
//
//  getTexName
//
//==========================================================================
static __attribute__((unused)) const char *getTexName (int txid) {
  if (txid == 0) return "<->";
  VTexture *tex = GTextureManager[txid];
  return (tex ? *tex->Name : "<none>");
}


//==========================================================================
//
//  VLevel::AppendControlLink
//
//==========================================================================
void VLevel::AppendControlLink (const sector_t *src, const sector_t *dest) {
  if (!src || !dest || src == dest) return; // just in case

  if (ControlLinks.length() == 0) {
    // first time, create empty array
    ControlLinks.setLength(NumSectors);
    Ctl2DestLink *link = ControlLinks.ptr();
    for (int f = NumSectors; f--; ++link) {
      link->src = -1;
      link->dest = -1;
      link->next = -1;
    }
  }

  const int srcidx = (int)(ptrdiff_t)(src-Sectors);
  const int destidx = (int)(ptrdiff_t)(dest-Sectors);
  Ctl2DestLink *lnk = &ControlLinks[srcidx];
  if (lnk->dest < 0) {
    // first slot
    check(lnk->src == -1);
    check(lnk->next == -1);
    lnk->src = srcidx;
    lnk->dest = destidx;
    lnk->next = -1;
  } else {
    // find list tail
    int lastidx = srcidx;
    for (;;) {
      int nli = ControlLinks[lastidx].next;
      if (nli < 0) break;
      lastidx = nli;
    }
    // append to list
    int newidx = ControlLinks.length();
    Ctl2DestLink *newlnk = &ControlLinks.alloc();
    lnk = &ControlLinks[lastidx]; // refresh lnk, because array might be reallocated
    check(lnk->next == -1);
    lnk->next = newidx;
    newlnk->src = srcidx;
    newlnk->dest = destidx;
    newlnk->next = -1;
  }

  /*
  GCon->Logf("=== AppendControlLink (src=%d; dst=%d) ===", srcidx, destidx);
  for (auto it = IterControlLinks(src); !it.isEmpty(); it.next()) {
    GCon->Logf("   dest=%d", it.getDestSectorIndex());
  }
  */
}


//==========================================================================
//
//  VLevel::AddExtraFloorSane
//
//  vavoom
//
//==========================================================================
void VLevel::AddExtraFloorSane (line_t *line, sector_t *dst) {
  sector_t *src = line->frontsector;

  const float floorz = src->floor.GetPointZ(dst->soundorg);
  const float ceilz = src->ceiling.GetPointZ(dst->soundorg);
  bool flipped = false;

  if (floorz < ceilz) {
    flipped = true;
    GCon->Logf("Swapped planes for Vavoom 3d floor, tag: %d, floorz: %g, ceilz: %g", line->arg1, ceilz, floorz);
  }

  // append link
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;
  AppendControlLink(src, dst);

  // insert into region array
  // control must have negative height, so
  // region floor is ceiling, and region ceiling is floor
  sec_region_t *reg = dst->AllocRegion();
  if (flipped) {
    // flipped
    reg->efloor.set(&src->floor, true);
    reg->eceiling.set(&src->ceiling, true);
  } else {
    // normal
    reg->efloor.set(&src->ceiling, false);
    reg->eceiling.set(&src->floor, false);
  }
  reg->params = &src->params;
  reg->extraline = line;
}


//==========================================================================
//
//  VLevel::AddExtraFloorShitty
//
//  gozzo
//
//==========================================================================
void VLevel::AddExtraFloorShitty (line_t *line, sector_t *dst) {
  enum {
    Invalid,
    Solid,
    Swimmable,
    NonSolid,
  };

  static int doDump = -1;
  if (doDump < 0) doDump = (GArgs.CheckParm("-Wall") || GArgs.CheckParm("-Wgozzo-3d") ? 1 : 0);

  //int eftype = (line->arg2&3);
  const bool isSolid = ((line->arg2&3) == Solid);

  sector_t *src = line->frontsector;

  if (doDump) { GCon->Logf("src sector #%d: floor=%s; ceiling=%s; (%g,%g); type=0x%02x (solid=%d)", (int)(ptrdiff_t)(src-Sectors), getTexName(src->floor.pic), getTexName(src->ceiling.pic), min2(src->floor.minz, src->floor.maxz), max2(src->ceiling.minz, src->ceiling.maxz), line->arg2, (int)isSolid); }
  if (doDump) { GCon->Logf("dst sector #%d: soundorg=(%g,%g,%g); fc=(%g,%g)", (int)(ptrdiff_t)(dst-Sectors), dst->soundorg.x, dst->soundorg.y, dst->soundorg.z, min2(dst->floor.minz, dst->floor.maxz), max2(dst->ceiling.minz, dst->ceiling.maxz)); }

  const float floorz = src->floor.GetPointZ(dst->soundorg);
  const float ceilz = src->ceiling.GetPointZ(dst->soundorg);
  bool flipped = false;

  if (floorz > ceilz) {
    flipped = true;
    GCon->Logf("Swapped planes for tag: %d, floorz: %g, ceilz: %g", line->arg1, ceilz, floorz);
  }

  if (doDump) { GCon->Logf("3d floor for tag %d (dst #%d, src #%d) (floorz=%g; ceilz=%g)", line->arg1, (int)(ptrdiff_t)(dst-Sectors), (int)(ptrdiff_t)(src-Sectors), floorz, ceilz); }
  if (doDump) { GCon->Logf("::: BEFORE"); dumpSectorRegions(dst); }

  // append link
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;
  AppendControlLink(src, dst);

  // insert into region array
  sec_region_t *reg = dst->AllocRegion();
  if (isSolid) {
    // solid region: floor points down, ceiling points up
    if (flipped) {
      // flipped
      reg->efloor.set(&src->ceiling, false);
      reg->eceiling.set(&src->floor, false);
    } else {
      // normal
      reg->efloor.set(&src->floor, true);
      reg->eceiling.set(&src->ceiling, true);
    }
  } else {
    // non-solid region: floor points up, ceiling points down
    if (flipped) {
      // flipped
      reg->efloor.set(&src->ceiling, true);
      reg->eceiling.set(&src->floor, true);
    } else {
      // normal
      reg->efloor.set(&src->floor, false);
      reg->eceiling.set(&src->ceiling, false);
    }
  }
  reg->params = &src->params;
  reg->extraline = line;
  if (!isSolid) {
    //reg.extraline = nullptr; //FIXME!
    reg->regflags |= sec_region_t::RF_NonSolid;
  }

  if (!isSolid) {
    // non-solid regions has visible floor and ceiling only when camera is inside
    // add the same region, but with flipped floor and ceiling (and mark it as visual only)
    sec_region_t *reg2 = dst->AllocRegion();
    reg2->efloor = reg->efloor;
    reg2->efloor.Flip();
    reg2->eceiling = reg->eceiling;
    reg2->eceiling.Flip();
    reg2->params = reg->params;
    reg2->extraline = nullptr;
    reg2->regflags = sec_region_t::RF_OnlyVisual;
  }
}


//==========================================================================
//
//  Level::AddExtraFloor
//
//  can return `nullptr`
//
//==========================================================================
void VLevel::AddExtraFloor (line_t *line, sector_t *dst) {
  return (line->arg2 == 0 ? AddExtraFloorSane(line, dst) : AddExtraFloorShitty(line, dst));
}


//==========================================================================
//
//  SwapPlanes
//
//==========================================================================
void SwapPlanes (sector_t *s) {
  float tempHeight = s->floor.TexZ;
  int tempTexture = s->floor.pic;

  s->floor.TexZ = s->ceiling.TexZ;
  s->floor.dist = s->floor.TexZ;
  s->floor.minz = s->floor.TexZ;
  s->floor.maxz = s->floor.TexZ;

  s->ceiling.TexZ = tempHeight;
  s->ceiling.dist = -s->ceiling.TexZ;
  s->ceiling.minz = s->ceiling.TexZ;
  s->ceiling.maxz = s->ceiling.TexZ;

  s->floor.pic = s->ceiling.pic;
  s->ceiling.pic = tempTexture;
}


//==========================================================================
//
//  CalcSecMinMaxs
//
//==========================================================================
void CalcSecMinMaxs (sector_t *sector) {
  float minz;
  float maxz;

  if (sector->floor.normal.z == 1.0f) {
    // horisontal floor
    sector->floor.minz = sector->floor.dist;
    sector->floor.maxz = sector->floor.dist;
  } else {
    // sloped floor
    minz = 99999.0f;
    maxz = -99999.0f;
    for (int i = 0; i < sector->linecount; ++i) {
      float z = sector->floor.GetPointZ(*sector->lines[i]->v1);
      if (minz > z) minz = z;
      if (maxz < z) maxz = z;
      z = sector->floor.GetPointZ(*sector->lines[i]->v2);
      if (minz > z) minz = z;
      if (maxz < z) maxz = z;
    }
    sector->floor.minz = minz;
    sector->floor.maxz = maxz;
  }

  if (sector->ceiling.normal.z == -1.0f) {
    // horisontal ceiling
    sector->ceiling.minz = -sector->ceiling.dist;
    sector->ceiling.maxz = -sector->ceiling.dist;
  } else {
    // sloped ceiling
    minz = 99999.0f;
    maxz = -99999.0f;
    for (int i = 0; i < sector->linecount; ++i) {
      float z = sector->ceiling.GetPointZ(*sector->lines[i]->v1);
      if (minz > z) minz = z;
      if (maxz < z) maxz = z;
      z = sector->ceiling.GetPointZ(*sector->lines[i]->v2);
      if (minz > z) minz = z;
      if (maxz < z) maxz = z;
    }
    sector->ceiling.minz = minz;
    sector->ceiling.maxz = maxz;
  }
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, GetLineIndex) {
  P_GET_PTR(line_t, line);
  P_GET_SELF;
  int idx = -1;
  if (line) idx = (int)(ptrdiff_t)(line-Self->Lines);
  RET_INT(idx);
}

IMPLEMENT_FUNCTION(VLevel, PointInSector) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point)->sector);
}

IMPLEMENT_FUNCTION(VLevel, PointInSubsector) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point));
}

IMPLEMENT_FUNCTION(VLevel, ChangeSector) {
  P_GET_INT(crunch);
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  RET_BOOL(Self->ChangeSector(sec, crunch));
}

IMPLEMENT_FUNCTION(VLevel, ChangeOneSectorInternal) {
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  Self->ChangeOneSectorInternal(sec);
}

IMPLEMENT_FUNCTION(VLevel, AddExtraFloor) {
  P_GET_PTR(sector_t, dst);
  P_GET_PTR(line_t, line);
  P_GET_SELF;
  Self->AddExtraFloor(line, dst);
}

IMPLEMENT_FUNCTION(VLevel, SwapPlanes) {
  P_GET_PTR(sector_t, s);
  P_GET_SELF;
  (void)Self;
  SwapPlanes(s);
}

IMPLEMENT_FUNCTION(VLevel, SetFloorLightSector) {
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->floor.LightSourceSector = SrcSector-Self->Sectors;
}

IMPLEMENT_FUNCTION(VLevel, SetCeilingLightSector) {
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->ceiling.LightSourceSector = SrcSector-Self->Sectors;
}

IMPLEMENT_FUNCTION(VLevel, SetHeightSector) {
  P_GET_INT(Flags);
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  (void)Flags;
  (void)SrcSector;
  if (Self->RenderData) Self->RenderData->SetupFakeFloors(Sector);
}

IMPLEMENT_FUNCTION(VLevel, FindSectorFromTag) {
  P_GET_INT_OPT(start, -1);
  P_GET_INT(tag);
  P_GET_SELF;
  RET_INT(Self->FindSectorFromTag(tag, start));
}

IMPLEMENT_FUNCTION(VLevel, FindLine) {
  P_GET_PTR(int, searchPosition);
  P_GET_INT(lineTag);
  P_GET_SELF;
  RET_PTR(Self->FindLine(lineTag, searchPosition));
}

//native final void SectorSetLink (int controltag, int tag, int surface, int movetype);
IMPLEMENT_FUNCTION(VLevel, SectorSetLink) {
  P_GET_INT(movetype);
  P_GET_INT(surface);
  P_GET_INT(tag);
  P_GET_INT(controltag);
  P_GET_SELF;
  Self->SectorSetLink(controltag, tag, surface, movetype);
}

IMPLEMENT_FUNCTION(VLevel, SetBodyQueueTrans) {
  P_GET_INT(Trans);
  P_GET_INT(Slot);
  P_GET_SELF;
  RET_INT(Self->SetBodyQueueTrans(Slot, Trans));
}

//native final void AddDecal (TVec org, name dectype, int side, line_t *li);
IMPLEMENT_FUNCTION(VLevel, AddDecal) {
  P_GET_PTR(line_t, li);
  P_GET_INT(side);
  P_GET_NAME(dectype);
  P_GET_VEC(org);
  P_GET_SELF;
  Self->AddDecal(org, dectype, side, li, 0);
}

//native final void AddDecalById (TVec org, int id, int side, line_t *li);
IMPLEMENT_FUNCTION(VLevel, AddDecalById) {
  P_GET_PTR(line_t, li);
  P_GET_INT(side);
  P_GET_INT(id);
  P_GET_VEC(org);
  P_GET_SELF;
  Self->AddDecalById(org, id, side, li, 0);
}


//native final void doRecursiveSound (int validcount, ref array!Entity elist, sector_t *sec, int soundblocks, Entity soundtarget, float maxdist, const TVec sndorigin);
IMPLEMENT_FUNCTION(VLevel, doRecursiveSound) {
  P_GET_VEC(sndorigin);
  P_GET_FLOAT(maxdist);
  P_GET_PTR(VEntity, soundtarget);
  P_GET_INT(soundblocks);
  P_GET_PTR(sector_t, sec);
  P_GET_PTR(TArray<VEntity *>, elist);
  P_GET_INT(validcount);
  P_GET_SELF;
  Self->doRecursiveSound(validcount, *elist, sec, soundblocks, soundtarget, maxdist, sndorigin);
}


struct StreamSection {
public:
  VStream *strm;
  int tspos;

public:
  StreamSection (VStream *astrm, const char *sign) {
    strm = astrm;
    if (!sign) sign = "";
    size_t slen = strlen(sign);
    if (slen > 255) Sys_Error("invalid signature");
    vuint8 blen = (vuint8)slen;
    *astrm << blen;
    if (slen) astrm->Serialise(sign, (int)slen);
    vint32 totalSize = 0;
    *astrm << totalSize;
    tspos = astrm->Tell();
  }

  ~StreamSection () {
    int cpos = strm->Tell();
    vint32 totalSize = cpos-tspos;
    strm->Seek(tspos-4);
    *strm << totalSize;
    strm->Seek(cpos);
    strm = nullptr;
  }

  StreamSection () = delete;
  StreamSection (const StreamSection &) = delete;
  StreamSection &operator = (const StreamSection &) = delete;
};


//==========================================================================
//
//  WriteTexture
//
//==========================================================================
static void WriteTexture (VStream &strm, VTextureID v) {
  if (v.id < 0) {
    vuint8 len = 1;
    strm << len;
    char cc = '-';
    strm.Serialise(&cc, 1);
    vuint8 ttype = TEXTYPE_Null;
    strm << ttype;
    return;
  }
  if (v.id == 0) {
    vuint8 len = 0;
    strm << len;
    return;
  }
  if (!GTextureManager.getIgnoreAnim(v.id)) {
    GCon->Logf(NAME_Warning, "SAVE: trying to save inexisting texture with id #%d", v.id);
    vuint8 len = 0;
    strm << len;
    return;
  }
  const char *txname = *GTextureManager.GetTextureName(v.id);
  vuint8 ttype = (vuint8)GTextureManager.getIgnoreAnim(v.id)->Type;
  if (ttype == TEXTYPE_Null) {
    vuint8 len = 0;
    strm << len;
    return;
  }
  size_t slen = strlen(txname);
  if (slen > 255) Sys_Error("invalid texture name (too long)");
  vuint8 blen = (vuint8)slen;
  strm << blen;
  if (slen) strm.Serialise(txname, (int)slen);
  strm << ttype;
}


//==========================================================================
//
//  WritePlane
//
//==========================================================================
static void WritePlane (VStream &strm, const TPlane &plane) {
  strm << plane.normal.x << plane.normal.y << plane.normal.z << plane.dist;
}


//==========================================================================
//
//  WriteSectorPlane
//
//==========================================================================
static void WriteSectorPlane (VStream &strm, const sec_plane_t &plane) {
  WritePlane(strm, plane);
  strm << plane.minz << plane.maxz;
  strm << plane.TexZ;
  WriteTexture(strm, plane.pic);
  strm << plane.xoffs << plane.yoffs;
  strm << plane.XScale << plane.YScale;
  strm << plane.Angle;
  strm << plane.BaseAngle;
  strm << plane.BaseYOffs;
  strm << plane.flags;
  strm << plane.Alpha;
  strm << plane.MirrorAlpha;
  strm << plane.LightSourceSector;
}


//==========================================================================
//
//  WriteTexInfo
//
//==========================================================================
static void WriteTexInfo (VStream &strm, texinfo_t &ti) {
  strm << ti.saxis.x << ti.saxis.y << ti.saxis.z << ti.soffs;
  strm << ti.taxis.x << ti.taxis.y << ti.taxis.z << ti.toffs;
  strm << ti.noDecals;
  strm << ti.Alpha;
  strm << ti.Additive;
  strm << ti.ColorMap;
  // texture
  if (!ti.Tex || ti.Tex->Type == TEXTYPE_Null) {
    vuint8 len = 0;
    strm << len;
  } else {
    const char *txname = *(ti.Tex->Name);
    vuint8 ttype = (vuint8)(ti.Tex->Type);
    size_t slen = strlen(txname);
    if (slen > 255) Sys_Error("invalid texture name (too long)");
    vuint8 blen = (vuint8)slen;
    strm << blen;
    if (slen) strm.Serialise(txname, (int)slen);
    strm << ttype;
  }
}


//==========================================================================
//
//  WriteSurface
//
//==========================================================================
static void WriteSurface (VStream &strm, surface_t *surf, VLevel *level) {
  for (;;) {
    vuint8 present = (surf ? 1 : 0);
    strm << present;
    if (!surf) return;
    WritePlane(strm, surf->plane);
    WriteTexInfo(strm, *surf->texinfo);
    present = (surf->HorizonPlane ? 1 : 0);
    strm << present;
    if (surf->HorizonPlane) WriteSectorPlane(strm, *surf->HorizonPlane);
    strm << surf->Light;
    strm << surf->Fade;
    // subsector
    vint32 ssnum = (surf->subsector ? (vint32)(ptrdiff_t)(surf->subsector-level->Subsectors) : -1);
    strm << ssnum;
    // mins, extents
    vint32 t0, t1;
    t0 = surf->texturemins[0];
    t1 = surf->texturemins[1];
    strm << t0 << t1;
    t0 = surf->extents[0];
    t1 = surf->extents[1];
    strm << t0 << t1;
    // vertices
    vint32 count = surf->count;
    strm << count;
    for (int f = 0; f < count; ++f) strm << surf->verts[f].x << surf->verts[f].y << surf->verts[f].z;
    surf = surf->next;
  }
}


//==========================================================================
//
//  WriteSegPart
//
//==========================================================================
static void WriteSegPart (VStream &strm, segpart_t *sp, VLevel *level) {
  for (;;) {
    vuint8 present = (sp ? 1 : 0);
    strm << present;
    if (!sp) return;
    strm << sp->frontTopDist;
    strm << sp->frontBotDist;
    strm << sp->backTopDist;
    strm << sp->backBotDist;
    strm << sp->TextureOffset;
    strm << sp->RowOffset;
    WriteTexInfo(strm, sp->texinfo);
    WriteSurface(strm, sp->surfs, level);
    sp = sp->next;
  }
}


//==========================================================================
//
//  VLevel::DebugSaveLevel
//
//  this saves everything except thinkers, so i can load it for
//  further experiments
//
//==========================================================================
void VLevel::DebugSaveLevel (VStream &strm) {
  {
    StreamSection section(&strm, "VERTEX");
    strm << NumVertexes;
    for (int f = 0; f < NumVertexes; ++f) {
      strm << Vertexes[f].x << Vertexes[f].y << Vertexes[f].z;
    }
  }

  {
    StreamSection section(&strm, "SECTOR");
    strm << NumSectors;
    for (int f = 0; f < NumSectors; ++f) {
      sector_t *sec = &Sectors[f];
      WriteSectorPlane(strm, sec->floor);
      WriteSectorPlane(strm, sec->ceiling);
      // params
      strm << sec->params.lightlevel;
      strm << sec->params.LightColor;
      strm << sec->params.Fade;
      strm << sec->params.contents;
      // other sector fields
      strm << sec->special;
      strm << sec->tag;
      strm << sec->skyheight;
      strm << sec->seqType;
      strm << sec->blockbox[0] << sec->blockbox[1] << sec->blockbox[2] << sec->blockbox[3];
      strm << sec->soundorg.x << sec->soundorg.y << sec->soundorg.z; // sound origin
      // write sector line numbers
      strm << sec->linecount;
      for (int lnum = 0; lnum < sec->linecount; ++lnum) {
        vint32 lidx = (sec->lines[lnum] ? (vint32)(ptrdiff_t)(sec->lines[lnum]-Lines) : -1);
        strm << lidx;
      }
      // height sector
      vint32 snum = (sec->heightsec ? (vint32)(ptrdiff_t)(sec->heightsec-Sectors) : -1);
      strm << snum;
      // flags
      strm << sec->SectorFlags;
      strm << sec->Damage;
      strm << sec->Friction;
      strm << sec->MoveFactor;
      strm << sec->Gravity;
      strm << sec->Sky;
      strm << sec->Zone;
      // "other" sectors
      snum = (sec->deepref ? (vint32)(ptrdiff_t)(sec->deepref-Sectors) : -1);
      strm << snum;
      snum = (sec->othersecFloor ? (vint32)(ptrdiff_t)(sec->othersecFloor-Sectors) : -1);
      strm << snum;
      snum = (sec->othersecCeiling ? (vint32)(ptrdiff_t)(sec->othersecCeiling-Sectors) : -1);
      strm << snum;
      // write subsector indicies
      vint32 sscount = 0;
      for (subsector_t *ss = sec->subsectors; ss; ss = ss->seclink) ++sscount;
      strm << sscount;
      for (subsector_t *ss = sec->subsectors; ss; ss = ss->seclink) {
        vint32 ssnum = (vint32)(ptrdiff_t)(ss-Subsectors);
        strm << ssnum;
      }
      // regions, from bottom to top
      /*
      vint32 regcount = 0;
      for (sec_region_t *reg = sec->botregion; reg; reg = reg->next) ++regcount;
      strm << regcount;
      for (sec_region_t *reg = sec->botregion; reg; reg = reg->next) {
        check(reg->efloor.isValid());
        check(reg->eceiling.isValid());
        check(reg->params);
        //WriteSectorPlane(strm, *reg->efloor);
        //WriteSectorPlane(strm, *reg->eceiling);
        // params
        strm << reg->params->lightlevel;
        strm << reg->params->LightColor;
        strm << reg->params->Fade;
        strm << reg->params->contents;
        vint32 elidx = (reg->extraline ? (vint32)(ptrdiff_t)(reg->extraline-Lines) : -1);
        strm << elidx;
      }
      */
    }
  }

  {
    StreamSection section(&strm, "SIDE");
    strm << NumSides;
    for (int f = 0; f < NumSides; ++f) {
      side_t *side = &Sides[f];
      strm << side->Top.TextureOffset;
      strm << side->Bot.TextureOffset;
      strm << side->Mid.TextureOffset;
      strm << side->Top.RowOffset;
      strm << side->Bot.RowOffset;
      strm << side->Mid.RowOffset;
      //TODO: scale
      WriteTexture(strm, side->TopTexture);
      WriteTexture(strm, side->BottomTexture);
      WriteTexture(strm, side->MidTexture);
      // facing sector
      vint32 snum = (side->Sector ? (vint32)(ptrdiff_t)(side->Sector-Sectors) : -1);
      strm << snum;
      strm << side->LineNum;
      strm << side->Flags;
      strm << side->Light;
    }
  }

  {
    StreamSection section(&strm, "LINE");
    strm << NumLines;
    for (int f = 0; f < NumLines; ++f) {
      // line plane
      line_t *line = &Lines[f];
      WritePlane(strm, *line);
      // line vertices
      vint32 vnum = (vint32)(ptrdiff_t)(line->v1-Vertexes);
      strm << vnum;
      vnum = (vint32)(ptrdiff_t)(line->v2-Vertexes);
      strm << vnum;
      // dir
      strm << line->dir.x << line->dir.y << line->dir.z;
      // flags
      strm << line->flags;
      strm << line->SpacFlags;
      // sides
      strm << line->sidenum[0] << line->sidenum[1];
      // bbox
      strm << line->bbox[0] << line->bbox[1] << line->bbox[2] << line->bbox[3];
      // other
      strm << line->slopetype;
      strm << line->alpha;
      strm << line->special;
      strm << line->arg1 << line->arg2 << line->arg3 << line->arg4 << line->arg5;
      strm << line->LineTag;
      // lines connected to v1
      strm << line->v1linesCount;
      for (int ln = 0; ln < line->v1linesCount; ++ln) {
        vint32 lidx = (vint32)(ptrdiff_t)(line->v1lines[ln]-Lines);
        strm << lidx;
      }
      // lines connected to v2
      strm << line->v2linesCount;
      for (int ln = 0; ln < line->v2linesCount; ++ln) {
        vint32 lidx = (vint32)(ptrdiff_t)(line->v2lines[ln]-Lines);
        strm << lidx;
      }
      // front and back sectors
      vint32 snum = (line->frontsector ? (vint32)(ptrdiff_t)(line->frontsector-Sectors) : -1);
      strm << snum;
      snum = (line->backsector ? (vint32)(ptrdiff_t)(line->backsector-Sectors) : -1);
      strm << snum;
      // first segment
      vint32 sgnum = (line->firstseg ? (vint32)(ptrdiff_t)(line->firstseg-Segs) : -1);
      strm << sgnum;
    }
  }

  {
    StreamSection section(&strm, "SEG");
    strm << NumSegs;
    for (int f = 0; f < NumSegs; ++f) {
      seg_t *seg = &Segs[f];
      // plane
      WritePlane(strm, *seg);
      // vertices
      vint32 vnum = (vint32)(ptrdiff_t)(seg->v1-Vertexes);
      strm << vnum;
      vnum = (vint32)(ptrdiff_t)(seg->v2-Vertexes);
      strm << vnum;
      strm << seg->offset;
      strm << seg->length;
      strm << seg->side;
      // line
      vint32 lidx = (seg->linedef ? (vint32)(ptrdiff_t)(seg->linedef-Lines) : -1);
      strm << lidx;
      // next seg in linedef (lsnext)
      vint32 lsnidx = (seg->lsnext ? (vint32)(ptrdiff_t)(seg->lsnext-Segs) : -1);
      strm << lsnidx;
      // parnter seg
      vint32 psidx = (seg->partner ? (vint32)(ptrdiff_t)(seg->partner-Segs) : -1);
      strm << psidx;
      // side index
      vint32 sdidx = (seg->sidedef ? (vint32)(ptrdiff_t)(seg->sidedef-Sides) : -1);
      strm << sdidx;
      // front and back sectors
      vint32 snum = (seg->frontsector ? (vint32)(ptrdiff_t)(seg->frontsector-Sectors) : -1);
      strm << snum;
      snum = (seg->backsector ? (vint32)(ptrdiff_t)(seg->backsector-Sectors) : -1);
      strm << snum;
      // front subsector
      vint32 fss = (seg->front_sub ? (vint32)(ptrdiff_t)(seg->front_sub-Subsectors) : -1);
      strm << fss;
      // drawsegs
      vint32 dscount = 0;
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) ++dscount;
      strm << dscount;
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
        check(ds->seg == seg);
        WriteSegPart(strm, ds->top, this);
        WriteSegPart(strm, ds->mid, this);
        WriteSegPart(strm, ds->bot, this);
        WriteSegPart(strm, ds->topsky, this);
        WriteSegPart(strm, ds->extra, this);
        WriteSurface(strm, ds->HorizonTop, this);
        WriteSurface(strm, ds->HorizonBot, this);
      }
    }
  }

  {
    StreamSection section(&strm, "SUBSEC");
    strm << NumSubsectors;
    for (int f = 0; f < NumSubsectors; ++f) {
      subsector_t *sub = &Subsectors[f];
      // sector
      vint32 snum = (sub->sector ? (vint32)(ptrdiff_t)(sub->sector-Sectors) : -1);
      strm << snum;
      // seclink
      vint32 slnum = (sub->seclink ? (vint32)(ptrdiff_t)(sub->seclink-Subsectors) : -1);
      strm << slnum;
      // segs
      strm << sub->firstline;
      strm << sub->numlines;
      // parent node
      vint32 nnum = (sub->parent ? (vint32)(ptrdiff_t)(sub->parent-Nodes) : -1);
      strm << nnum;
      //strm << sub->VisFrame;
      // regions
      vint32 regcount = 0;
      for (subregion_t *sreg = sub->regions; sreg; sreg = sreg->next) ++regcount;
      strm << regcount;
      for (subregion_t *sreg = sub->regions; sreg; sreg = sreg->next) {
        sec_region_t *reg = sreg->secregion;
        check(reg->efloor.isValid());
        check(reg->eceiling.isValid());
        check(reg->params);
        //WriteSectorPlane(strm, *reg->efloor);
        //WriteSectorPlane(strm, *reg->eceiling);
        // params
        strm << reg->params->lightlevel;
        strm << reg->params->LightColor;
        strm << reg->params->Fade;
        strm << reg->params->contents;
        vint32 elidx = (reg->extraline ? (vint32)(ptrdiff_t)(reg->extraline-Lines) : -1);
        strm << elidx;
        // subregion things
        WriteSectorPlane(strm, *sreg->floorplane.splane);
        WriteSectorPlane(strm, *sreg->ceilplane.splane);
        // surfaces; meh
        //sec_surface_t *floor;
        //sec_surface_t *ceil;
        strm << sreg->count;
        for (int dsi = 0; dsi < sreg->count; ++dsi) {
          drawseg_t *ds = &sreg->lines[dsi];
          vint32 sgnum = (ds->seg ? (vint32)(ptrdiff_t)(ds->seg-Segs) : -1);
          strm << sgnum;
          WriteSegPart(strm, ds->top, this);
          WriteSegPart(strm, ds->mid, this);
          WriteSegPart(strm, ds->bot, this);
          WriteSegPart(strm, ds->topsky, this);
          WriteSegPart(strm, ds->extra, this);
          WriteSurface(strm, ds->HorizonTop, this);
          WriteSurface(strm, ds->HorizonBot, this);
        }
      }
    }
  }

  {
    StreamSection section(&strm, "NODE");
    strm << NumNodes;
    for (int f = 0; f < NumNodes; ++f) {
      node_t *node = &Nodes[f];
      WritePlane(strm, *node);
      for (int c0 = 0; c0 < 2; ++c0) {
        for (int c1 = 0; c1 < 6; ++c1) {
          strm << node->bbox[c0][c1];
        }
      }
      strm << node->children[0];
      strm << node->children[1];
      vint32 nnum = (node->parent ? (vint32)(ptrdiff_t)(node->parent-Nodes) : -1);
      strm << nnum;
      //strm << node->VisFrame;
    }
  }

  {
    //BlockMap = BlockMapLump+4;
    StreamSection section(&strm, "BLOCKMAP");
    strm << BlockMapWidth;
    strm << BlockMapHeight;
    strm << BlockMapOrgX;
    strm << BlockMapOrgY;
    strm << BlockMapLumpSize;
    strm.Serialise(BlockMapLump, BlockMapLumpSize);
  }

/*
  // !!! Used only during level loading
  mthing_t *Things;
  vint32 NumThings;
*/
}


COMMAND(DebugExportLevel) {
  if (Args.length() != 2) {
    GCon->Log("(only) file name expected!");
    return;
  }

  if (!GLevel) {
    GCon->Log("no level loaded");
    return;
  }

  // find a file name to save it to
  VStr fname = va("%s.lvl", *Args[1]);
  auto strm = FL_OpenFileWrite(fname, true); // as full name
  if (!strm) {
    GCon->Logf(NAME_Error, "cannot create file '%s'", *fname);
    return;
  }

  GLevel->DebugSaveLevel(*strm);
  delete strm;

  GCon->Logf("Level exported to '%s'", *fname);
}
