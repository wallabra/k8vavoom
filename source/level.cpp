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

//#define VAVOOM_DECALS_DEBUG
//#define VAVOOM_DECALS_DEBUG_X2

extern VCvarB r_decals_enabled;

IMPLEMENT_CLASS(V, Level);

VLevel *GLevel;
VLevel *GClLevel;

static VCvarI r_decal_onetype_max("r_decal_onetype_max", "128", "Maximum decals of one decaltype on a wall segment.", CVAR_Archive);

static VCvarB gm_compat_corpses_can_hear("gm_compat_corpses_can_hear", false, "Can corpses hear sound propagation?", CVAR_Archive);
static VCvarB gm_compat_everything_can_hear("gm_compat_everything_can_hear", false, "Can everything hear sound propagation?", CVAR_Archive);
static VCvarF gm_compat_max_hearing_distance("gm_compat_max_hearing_distance", "0", "Maximum hearing distance (0 means unlimited)?", CVAR_Archive);


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
void VLevel::CalcSectorBoundingHeight (const sector_t *sector, float *minz, float *maxz) const {
  float tmp0, tmp1;
  if (!minz) minz = &tmp0;
  if (!maxz) maxz = &tmp1;
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
    const float zmin = MIN(bsec->floor.minz, bsec->ceiling.maxz);
    const float zmax = MAX(bsec->floor.minz, bsec->ceiling.maxz);
    *minz = MIN(*minz, zmin);
    *maxz = MAX(*maxz, zmax);
  }
}


//==========================================================================
//
//  VLevel::GetSubsectorBBox
//
//==========================================================================
void VLevel::GetSubsectorBBox (const subsector_t *sub, float bbox[6]) const {
  bbox[0] = sub->bbox[0];
  bbox[1] = sub->bbox[1];
  bbox[2] = MIN(sub->parent->bbox[0][2], sub->parent->bbox[1][2]);
  // max
  bbox[3] = sub->bbox[2];
  bbox[4] = sub->bbox[3];
  bbox[5] = MAX(sub->parent->bbox[0][5], sub->parent->bbox[1][5]);
  FixBBoxZ(bbox);

  /*
  float minz, maxz;
  CalcSectorBoundingHeight(sub->sector, &minz, &maxz);
  bbox[2] = MIN(bbox[2], minz);
  bbox[5] = MAX(bbox[5], maxz);
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
    bbox[2] = MIN(bsp->bbox[0][2], bsp->bbox[1][2]);
    bbox[5] = MAX(bsp->bbox[0][5], bsp->bbox[1][5]);
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
    vio.io(VName("linelen"), dc->linelen);
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
              if (lin->sidenum[bsidenum] < 0) {
                bsidenum = 1-bsidenum;
                if (lin->sidenum[bsidenum] < 0) Sys_Error("Save loader: invalid seg linedef (1)!");
              }
              side_t *sb = &Sides[lin->sidenum[bsidenum]];
              dc->bsec = sb->Sector;
              if (!dc->bsec) Sys_Error("Save loader: invalid seg linedef (2)!");
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
      vio.io(VName("params.LightColour"), sec->params.LightColour);
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
          vio.io(VName("TopTextureOffset"), si->TopTextureOffset);
          vio.io(VName("BotTextureOffset"), si->BotTextureOffset);
          vio.io(VName("MidTextureOffset"), si->MidTextureOffset);
          vio.io(VName("TopRowOffset"), si->TopRowOffset);
          vio.io(VName("BotRowOffset"), si->BotRowOffset);
          vio.io(VName("MidRowOffset"), si->MidRowOffset);
          vio.io(VName("Flags"), si->Flags);
          vio.io(VName("Light"), si->Light);
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
      vio.io(VName("Origin"), StaticLights[i].Origin);
      vio.io(VName("Radius"), StaticLights[i].Radius);
      vio.io(VName("Colour"), StaticLights[i].Colour);
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
    if (sec->SoundTarget && sec->SoundTarget->GetFlags()&_OF_CleanupRef) sec->SoundTarget = nullptr;
    if (sec->FloorData && sec->FloorData->GetFlags()&_OF_CleanupRef) sec->FloorData = nullptr;
    if (sec->CeilingData && sec->CeilingData->GetFlags()&_OF_CleanupRef) sec->CeilingData = nullptr;
    if (sec->LightingData && sec->LightingData->GetFlags()&_OF_CleanupRef) sec->LightingData = nullptr;
    if (sec->AffectorData && sec->AffectorData->GetFlags()&_OF_CleanupRef) sec->AffectorData = nullptr;
    if (sec->ActionList && sec->ActionList->GetFlags()&_OF_CleanupRef) sec->ActionList = nullptr;
  }
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].SpecialData && (PolyObjs[i].SpecialData->GetFlags()&_OF_CleanupRef)) {
      PolyObjs[i].SpecialData = nullptr;
    }
  }
  for (int i = 0; i < CameraTextures.Num(); ++i) {
    if (CameraTextures[i].Camera && (CameraTextures[i].Camera->GetFlags()&_OF_CleanupRef)) {
      CameraTextures[i].Camera = nullptr;
    }
  }
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
    for (int i = 0; i < NumSectors; ++i) {
      sec_region_t *r = Sectors[i].botregion;
      while (r) {
        sec_region_t *Next = r->next;
        delete r;
        r = Next;
      }
    }
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

  GTextureManager.ResetMapTextures();

  // call parent class' `Destroy()` method
  Super::Destroy();
}


//==========================================================================
//
//  VLevel::SetCameraToTexture
//
//==========================================================================
void VLevel::SetCameraToTexture (VEntity *Ent, VName TexName, int FOV) {
  if (!Ent) return;

  // get texture index
  int TexNum = GTextureManager.CheckNumForName(TexName, TEXTYPE_Wall, true, false);
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
  Tr->BuildPlayerTrans(P->TranslStart, P->TranslEnd, P->Colour);
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
static bool isDecalsOverlap (VDecalDef *dec, float segdist, float orgz, decal_t *cur, const picinfo_t &tinf) {
  float two = tinf.xoffset*dec->scaleX;
  float segd0 = segdist-two;
  float segd1 = segd0+tinf.width*dec->scaleX;
  float cco = cur->xdist-tinf.xoffset*cur->scaleX+cur->ofsX;
  float ccz = cur->curz-cur->ofsY+tinf.yoffset*cur->scaleY;
  orgz += tinf.yoffset*dec->scaleY;
  return
    segd1 > cco && segd0 < cco+tinf.width*cur->scaleX &&
    orgz > ccz-tinf.height*cur->scaleY && orgz-tinf.height*dec->scaleY < ccz;
}


//==========================================================================
//
//  VLevel::PutDecalAtLine
//
//  prevdir<0: `segdist` is negative, left offset
//  prevdir=0: `segdist` is normal dist
//  prevdir>0: `segdist` is positive, right offset
//
//==========================================================================
void VLevel::PutDecalAtLine (int tex, float orgz, float segdist, VDecalDef *dec, sector_t *sec, line_t *li, int prevdir, vuint32 flips) {
  // don't process linedef twice
  if (li->decalMark == decanimuid) return;
  li->decalMark = decanimuid;

  if (!GTextureManager[tex]) return;

  //GCon->Logf("decal '%s' at linedef %d", *GTextureManager[tex]->Name, (int)(ptrdiff_t)(li-Lines));

  picinfo_t tinf;
  GTextureManager.GetTextureInfo(tex, &tinf);
  float tw = tinf.width*dec->scaleX;
  float tw2 = tw*0.5f;
  //float th2 = tinf.height*dec->scaleY*0.5f;

  float txofs = tinf.xoffset*dec->scaleX;
  segdist += txofs;

  int sidenum = 0;
  vertex_t *v1;
  vertex_t *v2;

  if (li->frontsector == sec) {
    v1 = li->v1;
    v2 = li->v2;
  } else {
    sidenum = 1;
    v1 = li->v2;
    v2 = li->v1;
  }

  TVec lv1 = *v1, lv2 = *v2;
  lv1.z = 0;
  lv2.z = 0;
  float linelen = Length(lv2-lv1);

#if defined(VAVOOM_DECALS_DEBUG) || defined(VAVOOM_DECALS_DEBUG_X2)
  GCon->Logf("***PutDecalAtLine***: lidx=%d; segdist=%f; prevdir=%d; sidenum=%d; linelen=%f", (int)(ptrdiff_t)(li-Lines), segdist, prevdir, sidenum, linelen);
#endif

  float segd0, segd1;
  if (prevdir < 0) {
    if (segdist >= 0) return; // just in case
    segd0 = segdist+linelen;
    segd1 = segd0+tw;
    segdist = (segd0+segd1)*0.5f;
#if defined(VAVOOM_DECALS_DEBUG) || defined(VAVOOM_DECALS_DEBUG_X2)
    GCon->Logf("** left spread; segdist=%f; segd0=%f; segd1=%f", segdist, segd0, segd1);
#endif
  } else if (prevdir > 0) {
    if (segdist <= 0) return; // just in case
    segd1 = segdist;
    segd0 = segd1-tw;
    segdist = (segd0+segd1)*0.5f;
#if defined(VAVOOM_DECALS_DEBUG) || defined(VAVOOM_DECALS_DEBUG_X2)
    GCon->Logf("** right spread; segdist=%f; segd0=%f; segd1=%f", segdist, segd0, segd1);
#endif
  } else {
    segd0 = segdist-tw2;
    segd1 = segd0+tw;
  }

  // find segs for this decal (there may be several segs)
  for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
    if (!seg->linedef) continue; // ignore minisegs

    //if (seg->linedef) fprintf(stderr, "ldef(%p): special=%d\n", seg->linedef, seg->linedef->special);
    if ((seg->linedef->flags&ML_NODECAL) != 0) continue;

    if (/*seg->linedef == li &&*/ seg->frontsector == sec) {
      if (segd0 >= seg->offset+seg->length || segd1 < seg->offset) {
        //fprintf(stderr, "* SKIP seg: (segd=%f:%f; seg=%f:%f)\n", segd0, segd1, seg->offset, seg->offset+seg->length);
        continue;
      }
      bool slideWithFloor = false;
      bool slideWithCeiling = false;
#if defined(VAVOOM_DECALS_DEBUG) || defined(VAVOOM_DECALS_DEBUG_X2)
      GCon->Logf("  ** found seg: (segd=%f:%f; seg=%f:%f)", segd0, segd1, seg->offset, seg->offset+seg->length);
#endif
      int dcmaxcount = r_decal_onetype_max;
           if (tinf.width >= 128 || tinf.height >= 128) dcmaxcount = 8;
      else if (tinf.width >= 64 || tinf.height >= 64) dcmaxcount = 16;
      else if (tinf.width >= 32 || tinf.height >= 32) dcmaxcount = 32;

      {
        float hiz = orgz+tinf.yoffset*dec->scaleY;
        float loz = hiz-tinf.height*dec->scaleY;
        {
          side_t *sb = &Sides[li->sidenum[sidenum]];

          // check if decal is allowed on this side
          if (sb->MidTexture == skyflatnum) continue; // never on the sky
          /*
          bool allowTopTex = (sb->TopTexture > 0 && sb->TopTexture != skyflatnum);
          bool allowMidTex = (sb->MidTexture > 0 && sb->MidTexture != skyflatnum);
          bool allowBotTex = (sb->BottomTexture > 0 && sb->BottomTexture != skyflatnum);
          if (allowTopTex) {
            VTexture *xtx = GTextureManager(sb->TopTexture);
            allowTopTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
          }
          if (allowMidTex) {
            VTexture *xtx = GTextureManager(sb->MidTexture);
            allowMidTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
          }
          if (allowBotTex) {
            VTexture *xtx = GTextureManager(sb->BottomTexture);
            allowBotTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
          }
          if (!allowTopTex && !allowMidTex && !allowBotTex) continue;
          */

          /* nope
          if (!allowMidTex && loz > sec->floor.TexZ && hiz < sec->ceiling.TexZ) {
            // fully inside midtex (probably)
            GCon->Logf("NOMT; loz=%f; hiz=%f; ftz=%f; ctz=%f", loz, hiz, sec->floor.TexZ, sec->ceiling.TexZ);
            GCon->Logf("      floor=(%f,%f); ceiling=(%f,%f)", sec->floor.minz, sec->floor.maxz, sec->ceiling.minz, sec->ceiling.maxz);
            continue;
          }
          */
          /*
          if (!allowTopTex && loz >= sec->ceiling.TexZ) continue;
          if (!allowBotTex && hiz <= sec->floor.TexZ) continue;
          */

          //if (sb->TopTexture <= 0 && sb->BottomTexture <= 0 && sb->MidTexture <= 0) { /*!GCon->Logf("  *** no textures at all (sidenum=%d)", sidenum);*/ continue; }
          if (sb->MidTexture <= 0 || GTextureManager(sb->MidTexture)->Type == TEXTYPE_Null) {
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
            if (!allowTopTex && /*!allowMidTex &&*/ !allowBotTex) continue;

            if ((li->flags&ML_TWOSIDED) != 0 && li->sidenum[1-sidenum] >= 0) {
              // has other side
              sector_t *bsec = Sides[li->sidenum[1-sidenum]].Sector;
              bool botHit = false, topHit = false;
              //!GCon->Logf("sidenum=%d; orgz=%f; loz=%f; hiz=%f; sec.floorZ=%f; sec.ceilZ=%f; bsec.floorZ=%f; bsec.ceilZ=%f; toptex=%d; midtex=%d; bottex=%d; liflg=%s", sidenum, orgz, loz, hiz, sec->floor.TexZ, sec->ceiling.TexZ, bsec->floor.TexZ, bsec->ceiling.TexZ, sb->TopTexture, sb->MidTexture, sb->BottomTexture, lif2str(li->flags));

              if (allowBotTex && bsec->floor.TexZ > sec->floor.TexZ) {
                botHit = !(hiz <= sec->floor.TexZ || loz >= bsec->floor.TexZ);
                if (botHit) {
                  if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
                  /*!GCon->Logf("  BOTTOM HIT! slide=%d", (slideWithFloor ? 1 : 0));*/
                }
              }

              if (allowTopTex && bsec->ceiling.TexZ < sec->ceiling.TexZ) {
                topHit = !(hiz <= bsec->ceiling.TexZ || loz >= sec->ceiling.TexZ);
                if (topHit) {
                  if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
                  /*!GCon->Logf("  TOP HIT! slide=%d", (slideWithCeiling ? 1 : 0));*/
                }
              }

              if (!botHit && !topHit) {
                //!GCon->Logf("  *** in air");
                /*if (!allowMidTex)*/ {
                  /*
                  line_t *xl = &Lines[289];
                  GCon->Log("************");
                  GCon->Logf("v1lines: %d", xl->v1linesCount);
                  for (int f = 0; f < xl->v1linesCount; ++f) GCon->Logf("  %d: %d", f, (int)(ptrdiff_t)(xl->v1lines[f]-Lines));
                  GCon->Logf("v2lines: %d", xl->v2linesCount);
                  for (int f = 0; f < xl->v2linesCount; ++f) GCon->Logf("  %d: %d", f, (int)(ptrdiff_t)(xl->v2lines[f]-Lines));
                  */
                  continue;
                }
#if defined(VAVOOM_DECALS_DEBUG) || defined(VAVOOM_DECALS_DEBUG_X2)
                GCon->Logf("  *** 2sided %d", (int)(ptrdiff_t)(li-Lines));
#endif
              }
            } else {
              // no other side
              //!GCon->Logf("::: sidenum=%d; orgz=%f; loz=%f; hiz=%f; sec.floorZ=%f; sec.ceilZ=%f; toptex=%d; midtex=%d; bottex=%d; liflg=%s", sidenum, orgz, loz, hiz, sec->floor.TexZ, sec->ceiling.TexZ, sb->TopTexture, sb->MidTexture, sb->BottomTexture, lif2str(li->flags));
              /*
              if (!allowMidTex && loz > sec->floor.TexZ && hiz < sec->ceiling.TexZ) continue;
              if (!allowTopTex && loz >= sec->ceiling.TexZ) continue;
              if (!allowBotTex && hiz <= sec->floor.TexZ) continue;
              */
              /*
              if (loz > sec->floor.TexZ && hiz < sec->ceiling.TexZ) {
                / *!GCon->Logf("  *** in air, and no middle texture");* /
                if (sb->MidTexture <= 0) continue;
                VTexture *xtx = GTextureManager(sb->MidTexture);
                if (!xtx || xtx->Type == TEXTYPE_Null || xtx->noDecals) continue;
              }
              if (hiz >= sec->ceiling.TexZ) {
                / *!GCon->Logf("  *** higher than ceiling, and no top texture");* /
                VTexture *xtx = GTextureManager(sb->TopTexture);
                if (!xtx || xtx->Type == TEXTYPE_Null || xtx->noDecals) continue;
              }
              if (loz <= sec->floor.TexZ) {
                / *!GCon->Logf("  *** lower than floor, and no bottom texture");* /
                VTexture *xtx = GTextureManager(sb->BottomTexture);
                if (!xtx || xtx->Type == TEXTYPE_Null || xtx->noDecals) continue;
              }
              */
            }
          } else {
                 if ((li->flags&ML_DONTPEGBOTTOM) != 0) slideWithFloor = true;
            else if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            /*
            if (!allowMidTex && loz > sec->floor.TexZ && hiz < sec->ceiling.TexZ) continue;
            if (!allowTopTex && loz >= sec->ceiling.TexZ) continue;
            if (!allowBotTex && hiz <= sec->floor.TexZ) continue;
            */
          }
        }
      }

      // remove old same-typed decals, if necessary
      if (dcmaxcount > 0 && dcmaxcount < 10000) {
        int count = 0;
        decal_t *prev = nullptr;
        decal_t *first = nullptr;
        decal_t *cur = seg->decals;
        while (cur) {
          // also, check if this decal is touching our one
          if (cur->dectype == dec->name && isDecalsOverlap(dec, segdist, orgz, cur, tinf)) {
            if (!first) first = cur;
            ++count;
          }
          if (!first) prev = cur;
          cur = cur->next;
        }
        if (count >= dcmaxcount) {
          //GCon->Logf("removing %d extra '%s' decals", count-dcmaxcount+1, *dec->name);
          // do removal
          decal_t *currd = first;
          if (prev) {
            if (prev->next != currd) Sys_Error("decal oops(0)");
          } else {
            if (seg->decals != currd) Sys_Error("decal oops(1)");
          }
          while (currd) {
            decal_t *n = currd->next;
            if (currd->dectype == dec->name && isDecalsOverlap(dec, segdist, orgz, currd, tinf)) {
              /*
              GCon->Logf("removing extra '%s' decal; org=(%f:%f,%f:%f); neworg=(%f:%f,%f:%f)", *dec->name,
                currd->xdist-tw2, currd->xdist+tw2, currd->orgz-th2, currd->orgz+th2,
                segd0, segd1, orgz-th2, orgz+th2
              );
              */
              if (prev) prev->next = n; else seg->decals = n;
              RemoveAnimatedDecal(currd);
              delete currd;
              if (--count < dcmaxcount) break;
            }
            currd = n;
          }
        }
      }

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
      //printf("seg: ofs=%f; end=%f; d0=%f; d1=%f\n", (float)seg->offset, (float)(seg->offset+seg->length), segd0, segd1);
      decal->seg = seg;
      decal->dectype = dec->name;
      //decal->picname = dec->pic;
      decal->texture = tex;
      decal->orgz = decal->curz = orgz;
      decal->xdist = /*segd0+tw2*/segdist/*-txofs*/; //tinf.width*0.5f;
      decal->linelen = linelen;
      decal->ofsX = decal->ofsY = 0;
      decal->scaleX = decal->origScaleX = dec->scaleX;
      decal->scaleY = decal->origScaleY = dec->scaleY;
      decal->alpha = decal->origAlpha = dec->alpha;
      decal->addAlpha = dec->addAlpha;
      decal->animator = (dec->animator ? dec->animator->clone() : nullptr);
      if (decal->animator) AddAnimatedDecal(decal);

      // setup misc flags
      decal->flags = flips|(dec->fullbright ? decal_t::Fullbright : 0)|(dec->fuzzy ? decal_t::Fuzzy : 0);

      sector_t *slidesec = (li->sidenum[1-sidenum] >= 0 ? Sides[li->sidenum[1-sidenum]].Sector : Sides[li->sidenum[sidenum]].Sector);
      // setup curz and pegs
      if (slideWithFloor) {
        //sector_t *slidesec = Sides[li->sidenum[1-sidenum]].Sector;
        decal->flags |= decal_t::SlideFloor|(sidenum == 1 ? decal_t::SideDefOne : 0);
        decal->curz -= slidesec->floor.TexZ;
        decal->bsec = slidesec;
      } else if (slideWithCeiling) {
        //sector_t *slidesec = Sides[li->sidenum[1-sidenum]].Sector;
        decal->flags |= decal_t::SlideCeil|(sidenum == 1 ? decal_t::SideDefOne : 0);
        decal->curz -= slidesec->ceiling.TexZ;
        decal->bsec = slidesec;
      }
    }
  }

  // if our decal is not completely at linedef, spread it to adjacent linedefs
  // FIXME: this is not right, 'cause we want not a linedef at the same sector,
  //        but linedef we can use for spreading. the difference is that it can
  //        belong to any other sector, it just has to have the texture at the
  //        given z point. i.e. linedef without midtexture (for example) can't
  //        be used, but another linedef from another sector with such texture
  //        is ok.

  /*
  printf("0: segd0=%f; segd1=%f; linelen=%f; prevdir=%d\n", segd0, segd1, linelen, prevdir);
  segd0 += txofs;
  segd1 += txofs;
  printf("1: segd0=%f; segd1=%f; linelen=%f; prevdir=%d; pos=%f\n", segd0, segd1, linelen, prevdir, segdist);
  */
  segd0 = segdist-txofs;
  segd1 = segd0+tw;
#ifdef VAVOOM_DECALS_DEBUG
  GCon->Logf("1: segd0=%f; segd1=%f; linelen=%f; prevdir=%d; pos=%f; tw=%f; tw2=%f", segd0, segd1, linelen, prevdir, segdist, tw, tw2);
#endif
  if ((segd0 < 0 && prevdir <= 0) || (segd1 > linelen && prevdir >= 0)) {
    for (int vn = 0; vn < 2; ++vn) {
      line_t **ngb = (vn == 0 ? li->v1lines : li->v2lines);
      int ngbCount = (vn == 0 ? li->v1linesCount : li->v2linesCount);
      for (int f = 0; f < ngbCount; ++f) {
        line_t *spline = ngb[f];
        //if (spline->frontsector && spline->backsector) continue;
        TVec sv1 = *spline->v1, sv2 = *spline->v2;
        sv1.z = sv2.z = 0;
        if (segd0 < 0 && prevdir <= 0) {
          //FIXME! should this (both parts) take `sidenum` into account, and use segd1?
          if (spline->frontsector && sv2 == lv1) {
#ifdef VAVOOM_DECALS_DEBUG
            GCon->Logf("  pdn: found v2->v1 (f) segd0=%f; segd1=%f", segd0, segd1);
#endif
            PutDecalAtLine(tex, orgz, segd0-txofs, dec, spline->frontsector, spline, -1, flips);
          }
          if (spline->backsector && sv1 == lv1) {
#ifdef VAVOOM_DECALS_DEBUG
            GCon->Logf("  pdn: found v2->v1 (b) segd0=%f; segd1=%f", segd0, segd1);
#endif
            PutDecalAtLine(tex, orgz, segd0-txofs, dec, spline->backsector, spline, -1, flips);
          }
        }
        if (segd1 > linelen && prevdir >= 0) {
          if (spline->frontsector && sv1 == lv2) {
            //FIXME! should this take `sidenum` into account, and use segd1?
#ifdef VAVOOM_DECALS_DEBUG
            GCon->Logf("  pdp(0): found v1->v2 (f) segd0=%f; segd1=%f; newsegdist=%f", segd0, segd1, segd1-linelen-txofs);
#endif
            PutDecalAtLine(tex, orgz, segd1-linelen-txofs, dec, spline->frontsector, spline, 1, flips);
          }
          if (spline->backsector && sv2 == lv2) {
            if (sidenum == 0) {
#ifdef VAVOOM_DECALS_DEBUG
              GCon->Logf("  pdp(0): found v1->v2 (b) segd0=%f; segd1=%f; newsegdist=%f", segd0, segd1, linelen-(segdist-txofs));
#endif
              PutDecalAtLine(tex, orgz, /*segd1-linelen*/linelen-(segdist-txofs), dec, spline->backsector, spline, 1, flips);
            } else {
#ifdef VAVOOM_DECALS_DEBUG
              GCon->Logf("  pdp(1): found v1->v2 (b) segd0=%f; segd1=%f; newsegdist=%f", segd0, segd1, segd1-linelen-txofs);
#endif
              PutDecalAtLine(tex, orgz, segd1-linelen-txofs, dec, spline->backsector, spline, 1, flips);
            }
          }
        }
      }
    }
  }
}


//==========================================================================
//
// VLevel::AddOneDecal
//
//==========================================================================
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, sector_t *sec, line_t *li) {
  if (!dec || !sec || !li) return;

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  if (dec->lowername != NAME_None) {
    //GCon->Logf("adding lower decal '%s' for decal '%s' (level %d)", *dec->lowername, *dec->name, level);
    AddDecal(org, dec->lowername, (sec == li->backsector ? 1 : 0), li, level+1);
  }

  if (++decanimuid == 0x7fffffff) {
    decanimuid = 1;
    for (int f = 0; f < NumLines; ++f) {
      line_t *ld = Lines+f;
      if (ld->decalMark != -1) ld->decalMark = 0;
    }
  }

  if (dec->scaleX <= 0 || dec->scaleY <= 0) {
    GCon->Logf("Decal '%s' has zero scale", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (dec->alpha <= 0.002f) {
    GCon->Logf("Decal '%s' has zero alpha", *dec->name);
    return;
  }

  /*int tex = GTextureManager.AddPatch(dec->pic, TEXTYPE_Pic);*/
  int tex = dec->texid;
  //if (dec->pic == VName("scorch1")) tex = GTextureManager.AddPatch(VName("bulde1"), TEXTYPE_Pic);
  if (tex <= 0 || tex >= GTextureManager.GetNumTextures()) {
    // no decal gfx, nothing to do
    GCon->Logf("Decal '%s' has no pic", *dec->name);
    return;
  }

  // get picture size, so we can spread it over segs and linedefs
  picinfo_t tinf;
  GTextureManager.GetTextureInfo(tex, &tinf);
  if (tinf.width < 1 || tinf.height < 1) {
    // invisible picture, nothing to do
    GCon->Logf("Decal '%s' has pic without pixels", *dec->name);
    return;
  }

  //GCon->Logf("Picture '%s' size: %d, %d  offsets: %d, %d", *dec->pic, tinf.width, tinf.height, tinf.xoffset, tinf.yoffset);

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

  // calculate `dist` -- distance from wall start
  //int sidenum = 0;
  vertex_t *v1;
  vertex_t *v2;

  if (li->frontsector == sec) {
    v1 = li->v1;
    v2 = li->v2;
  } else {
    //sidenum = 1;
    v1 = li->v2;
    v2 = li->v1;
  }

  float dx = v2->x-v1->x;
  float dy = v2->y-v1->y;
  float dist = 0; // distance from wall start
       if (fabsf(dx) > fabsf(dy)) dist = (org.x-v1->x)/dx;
  else if (dy != 0) dist = (org.y-v1->y)/dy;
  else dist = 0;

  TVec lv1 = *v1, lv2 = *v2;
  lv1.z = 0;
  lv2.z = 0;
  float linelen = Length(lv2-lv1);
  float segdist = dist*linelen;

  //GCon->Logf("Want to spawn decal '%s' (%s); pic=<%s>; dist=%f (linelen=%f)", *dec->name, (li->frontsector == sec ? "front" : "back"), *dec->pic, dist, linelen);

  segdist -= tinf.xoffset*dec->scaleX;
  float tw = tinf.width*dec->scaleX;

  float segd0 = segdist-tw/2.0f;
  float segd1 = segd0+tw;

  if (segd1 <= 0 || segd0 >= linelen) return; // out of linedef

  PutDecalAtLine(tex, org.z, segdist, dec, sec, li, 0, flips);
}


//==========================================================================
//
// VLevel::AddDecal
//
//==========================================================================
void VLevel::AddDecal (TVec org, const VName &dectype, int side, line_t *li, int level) {
  if (!r_decals_enabled) return;
  if (!li || dectype == NAME_None) return; // just in case

  sector_t *sec = (side ? li->backsector : li->frontsector);
  if (!sec) return; // just in case

  // ignore slopes
  if (sec->floor.minz != sec->floor.maxz || sec->ceiling.minz != sec->ceiling.maxz) return;

  //TVec oorg = org;
  org = P_SectorClosestPoint(sec, org);
  //org.z = oorg.z;
  //GCon->Logf("oorg:(%f,%f,%f); org:(%f,%f,%f)", oorg.x, oorg.y, oorg.z, org.x, org.y, org.z);

  int sidenum = (int)(li->backsector == sec);
  if (li->sidenum[sidenum] < 0) Sys_Error("decal engine: invalid linedef (0)!");

  static TStrSet baddecals;

  //VDecalDef *dec = VDecalDef::getDecal(dectype);
  //VDecalDef *dec = VDecalDef::getDecal(VName("K8GoreBloodSplat01"));
  //VDecalDef *dec = VDecalDef::getDecal(VName("PlasmaScorchLower1"));
  //VDecalDef *dec = VDecalDef::getDecal(VName("BigScorch"));
#ifdef VAVOOM_DECALS_DEBUG
  VDecalDef *dec = VDecalDef::getDecal(VName("Scorch"));
#else
  VDecalDef *dec = VDecalDef::getDecal(dectype);
#endif
  if (dec) {
    //GCon->Logf("DECAL '%s'; name is '%s', texid is %d", *dectype, *dec->name, dec->texid);
    AddOneDecal(level, org, dec, sec, li);
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

  sector_t *sec = (side ? li->backsector : li->frontsector);
  if (!sec) return; // just in case

  // ignore slopes
  if (sec->floor.minz != sec->floor.maxz || sec->ceiling.minz != sec->ceiling.maxz) return;

  //TVec oorg = org;
  org = P_SectorClosestPoint(sec, org);
  //org.z = oorg.z;
  //GCon->Logf("DECAL %d: oorg:(%f,%f,%f); org:(%f,%f,%f)", id, oorg.x, oorg.y, oorg.z, org.x, org.y, org.z);

  int sidenum = (int)(li->backsector == sec);
  if (li->sidenum[sidenum] < 0) Sys_Error("decal engine: invalid linedef (0)!");

  //VDecalDef *dec = VDecalDef::getDecal(dectype);
  //VDecalDef *dec = VDecalDef::getDecal(VName("K8GoreBloodSplat01"));
  //VDecalDef *dec = VDecalDef::getDecal(VName("PlasmaScorchLower1"));
  //VDecalDef *dec = VDecalDef::getDecal(VName("BigScorch"));
  VDecalDef *dec = VDecalDef::getDecalById(id);
  if (dec) {
    //GCon->Logf("DECAL %d: oorg:(%f,%f,%f); org:(%f,%f,%f)", id, oorg.x, oorg.y, oorg.z, org.x, org.y, org.z);
    //GCon->Logf("DECAL %d:<%s>: texture=<%s>; org:(%f,%f,%f)", id, *dec->name, *GTextureManager.GetTextureName(dec->texid), org.x, org.y, org.z);
    /*
    picinfo_t nfo;
    GTextureManager.GetTextureInfo(dec->texid, &nfo);
    GCon->Logf("DECAL %d:<%s>: texture=<%s>,(%dx%d),ofs(%d,%d); org:(%f,%f,%f)", id, *dec->name, *GTextureManager.GetTextureName(dec->texid), nfo.width, nfo.height, nfo.xoffset, nfo.yoffset, org.x, org.y, org.z);
    */
    AddOneDecal(level, org, dec, sec, li);
  } else {
    //if (!baddecals.put(*dectype)) GCon->Logf("NO DECAL: '%s'", *dectype);
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
//  AddExtraFloor
//
//==========================================================================
sec_region_t *AddExtraFloor (line_t *line, sector_t *dst) {
  sec_region_t *region;
  sec_region_t *inregion;
  sector_t *src;

  src = line->frontsector;
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;

  float floorz = src->floor.GetPointZ(dst->soundorg);
  float ceilz = src->ceiling.GetPointZ(dst->soundorg);

  // swap planes for 3d floors like those of GZDoom
  if (floorz < ceilz) {
    SwapPlanes(src);
    floorz = src->floor.GetPointZ(dst->soundorg);
    ceilz = src->ceiling.GetPointZ(dst->soundorg);
    GCon->Logf("Swapped planes for tag: %d, ceilz: %f, floorz: %f", line->arg1, ceilz, floorz);
  }

  for (inregion = dst->botregion; inregion; inregion = inregion->next) {
    float infloorz = inregion->floor->GetPointZ(dst->soundorg);
    float inceilz = inregion->ceiling->GetPointZ(dst->soundorg);

    if (infloorz <= floorz && inceilz >= ceilz) {
      region = new sec_region_t;
      memset((void *)region, 0, sizeof(*region));
      region->floor = inregion->floor;
      region->ceiling = &src->ceiling;
      region->params = &src->params;
      region->extraline = line;
      inregion->floor = &src->floor;

      if (inregion->prev) {
        inregion->prev->next = region;
      } else {
        dst->botregion = region;
      }
      region->prev = inregion->prev;
      region->next = inregion;
      inregion->prev = region;

      return region;
    }

    // check for sloped floor
    if (inregion->floor->normal.z != 1.0f) {
      if (inregion->floor->maxz <= src->ceiling.minz && inregion->ceiling->maxz >= src->floor.minz) {
        region = new sec_region_t;
        memset((void *)region, 0, sizeof(*region));
        region->floor = inregion->floor;
        region->ceiling = &src->ceiling;
        region->params = &src->params;
        region->extraline = line;
        inregion->floor = &src->floor;

        if (inregion->prev) {
          inregion->prev->next = region;
        } else {
          dst->botregion = region;
        }
        region->prev = inregion->prev;
        region->next = inregion;
        inregion->prev = region;

        return region;
      }

      //GCon->Logf("tag: %d, floor->maxz: %f, ceiling.minz: %f, ceiling->maxz: %f, floor.minz: %f", line->arg1, inregion->floor->maxz, src->ceiling.minz, inregion->ceiling->maxz, src->floor.minz);
    }

    // check for sloped ceiling
    if (inregion->ceiling->normal.z != -1.0f) {
      if (inregion->floor->minz <= src->ceiling.maxz && inregion->ceiling->minz >= src->floor.maxz) {
        region = new sec_region_t;
        memset((void *)region, 0, sizeof(*region));
        region->floor = inregion->floor;
        region->ceiling = &src->ceiling;
        region->params = &src->params;
        region->extraline = line;
        inregion->floor = &src->floor;

        if (inregion->prev) {
          inregion->prev->next = region;
        } else {
          dst->botregion = region;
        }
        region->prev = inregion->prev;
        region->next = inregion;
        inregion->prev = region;

        return region;
      }

      //GCon->Logf("tag: %d, floor->minz: %f, ceiling.maxz: %f, ceiling->minz: %f, floor.maxz: %f", line->arg1, inregion->floor->minz, src->ceiling.maxz, inregion->ceiling->minz, src->floor.maxz);
    }

    //GCon->Logf("tag: %d, infloorz: %f, ceilz: %f, inceilz: %f, floorz: %f", line->arg1, infloorz, ceilz, inceilz, floorz);
  }
  GCon->Logf("Invalid extra floor, tag %d", dst->tag);

  return nullptr;
}


//==========================================================================
//
//  SwapPlanes
//
//==========================================================================
void SwapPlanes (sector_t *s) {
  float tempHeight;
  int tempTexture;

  tempHeight = s->floor.TexZ;
  tempTexture = s->floor.pic;

  //  Floor
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

IMPLEMENT_FUNCTION(VLevel, AddExtraFloor) {
  P_GET_PTR(sector_t, dst);
  P_GET_PTR(line_t, line);
  P_GET_SELF;
  (void)Self;
  RET_PTR(AddExtraFloor(line, dst));
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
  strm << ti.ColourMap;
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
    WritePlane(strm, *surf->plane);
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
      strm << sec->params.LightColour;
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
      vint32 regcount = 0;
      for (sec_region_t *reg = sec->botregion; reg; reg = reg->next) ++regcount;
      strm << regcount;
      for (sec_region_t *reg = sec->botregion; reg; reg = reg->next) {
        check(reg->floor);
        check(reg->ceiling);
        check(reg->params);
        WriteSectorPlane(strm, *reg->floor);
        WriteSectorPlane(strm, *reg->ceiling);
        // params
        strm << reg->params->lightlevel;
        strm << reg->params->LightColour;
        strm << reg->params->Fade;
        strm << reg->params->contents;
        vint32 elidx = (reg->extraline ? (vint32)(ptrdiff_t)(reg->extraline-Lines) : -1);
        strm << elidx;
      }
    }
  }

  {
    StreamSection section(&strm, "SIDE");
    strm << NumSides;
    for (int f = 0; f < NumSides; ++f) {
      side_t *side = &Sides[f];
      strm << side->TopTextureOffset;
      strm << side->BotTextureOffset;
      strm << side->MidTextureOffset;
      strm << side->TopRowOffset;
      strm << side->BotRowOffset;
      strm << side->MidRowOffset;
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
        check(reg->floor);
        check(reg->ceiling);
        check(reg->params);
        WriteSectorPlane(strm, *reg->floor);
        WriteSectorPlane(strm, *reg->ceiling);
        // params
        strm << reg->params->lightlevel;
        strm << reg->params->LightColour;
        strm << reg->params->Fade;
        strm << reg->params->contents;
        vint32 elidx = (reg->extraline ? (vint32)(ptrdiff_t)(reg->extraline-Lines) : -1);
        strm << elidx;
        // subregion things
        WriteSectorPlane(strm, *sreg->floorplane);
        WriteSectorPlane(strm, *sreg->ceilplane);
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
