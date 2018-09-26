//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"
#ifdef CLIENT
#include "cl_local.h"
#endif
#include "render/r_local.h" // for decals

//#define VAVOOM_DECALS_DEBUG

extern VCvarB r_decals_enabled;

IMPLEMENT_CLASS(V, Level);

VLevel *GLevel;
VLevel *GClLevel;

static VCvarI r_decal_onetype_max("r_decal_onetype_max", "128", "Maximum decals of one decaltype on a wall segment", CVAR_Archive);


//==========================================================================
//
//  VLevel::PointInSubsector
//
//==========================================================================
subsector_t *VLevel::PointInSubsector (const TVec &point) const {
  guard(VLevel::PointInSubsector);
  // single subsector is a special case
  if (!NumNodes) return Subsectors;
  int nodenum = NumNodes-1;
  do {
    const node_t *node = Nodes+nodenum;
    nodenum = node->children[node->PointOnSide(point)];
  } while (!(nodenum&NF_SUBSECTOR));
  return &Subsectors[nodenum&~NF_SUBSECTOR];
  unguard;
}


//==========================================================================
//
//  VLevel::LeafPVS
//
//==========================================================================
const vuint8 *VLevel::LeafPVS (const subsector_t *ss) const {
  guard(VLevel::LeafPVS);
  int sub = ss-Subsectors;
  if (VisData) return VisData+(((NumSubsectors+7)>>3)*sub);
  return NoVis;
  unguard;
}


//==========================================================================
//
//  DecalIO
//
//==========================================================================
static void DecalIO (VStream &Strm, decal_t *dc) {
  if (!dc) return;
  char namebuf[64];
  vuint32 namelen = 0;
  if (Strm.IsLoading()) {
    // load picture name
    Strm << namelen;
    if (namelen == 0 || namelen > 63) Host_Error("Level load: invalid decal name length");
    memset(namebuf, 0, sizeof(namebuf));
    Strm.Serialise(namebuf, namelen);
    dc->picname = VName(namebuf);
    dc->texture = GTextureManager.AddPatch(dc->picname, TEXTYPE_Pic);
    // load decal type
    Strm << namelen;
    if (namelen == 0 || namelen > 63) Host_Error("Level load: invalid decal name length");
    memset(namebuf, 0, sizeof(namebuf));
    Strm.Serialise(namebuf, namelen);
    dc->dectype = VName(namebuf);
  } else {
    // save picture name
    namelen = (vuint32)strlen(*dc->picname);
    if (namelen == 0 || namelen > 63) Sys_Error("Level save: invalid decal name length");
    Strm << namelen;
    memcpy(namebuf, *dc->picname, namelen);
    Strm.Serialise(namebuf, namelen);
    // save decal type
    namelen = (vuint32)strlen(*dc->dectype);
    if (namelen == 0 || namelen > 63) Sys_Error("Level save: invalid decal name length");
    Strm << namelen;
    memcpy(namebuf, *dc->dectype, namelen);
    Strm.Serialise(namebuf, namelen);
  }
  Strm << dc->flags;
  Strm << dc->orgz;
  Strm << dc->curz;
  Strm << dc->xdist;
  Strm << dc->linelen;
  Strm << dc->shade[0];
  Strm << dc->shade[1];
  Strm << dc->shade[2];
  Strm << dc->shade[3];
  Strm << dc->ofsX;
  Strm << dc->ofsY;
  Strm << dc->origScaleX;
  Strm << dc->origScaleY;
  Strm << dc->scaleX;
  Strm << dc->scaleY;
  Strm << dc->origAlpha;
  Strm << dc->alpha;
  Strm << dc->addAlpha;
  VDecalAnim::Serialise(Strm, dc->animator);
}


//==========================================================================
//
//  writeOrCheckName
//
//==========================================================================
static void writeOrCheckName (VStream &Strm, const VName &value, const char *errmsg) {
  if (Strm.IsLoading()) {
    auto slen = strlen(*value);
    vuint32 v;
    Strm << v;
    if (v != slen) Host_Error("Save loader: invalid string length for %s", errmsg);
    if (v > 0) {
      auto buf = new char[v];
      memset(buf, 0, v);
      Strm.Serialise(buf, v);
      int res = memcmp(buf, *value, v);
      delete[] buf;
      if (res != 0) Host_Error("Save loader: invalid string value for %s", errmsg);
    }
  } else {
    vuint32 slen = (vuint32)strlen(*value);
    Strm << slen;
    if (slen) Strm.Serialise((char*)*value, slen);
  }
}


//==========================================================================
//
//  writeOrCheckUInt
//
//==========================================================================
static void writeOrCheckUInt (VStream &Strm, vuint32 value, const char *errmsg) {
  if (Strm.IsLoading()) {
    vuint32 v;
    Strm << v;
    if (v != value) Host_Error("Save loader: invalid value for %s; got %d, but expected %d", errmsg, v, value);
  } else {
    Strm << value;
  }
}


//==========================================================================
//
//  writeOrCheckInt
//
//==========================================================================
static void writeOrCheckInt (VStream &Strm, int value, const char *errmsg, bool dofail=true) {
  if (Strm.IsLoading()) {
    int v;
    Strm << v;
    if (v != value) {
      if (dofail) Host_Error("Save loader: invalid value for %s; got %d, but expected %d", errmsg, v, value);
      GCon->Logf("Save loader: invalid value for %s; got %d, but expected %d (should be harmless)", errmsg, v, value);
    }
  } else {
    Strm << value;
  }
}


//==========================================================================
//
//  writeOrCheckFloat
//
//==========================================================================
static void writeOrCheckFloat (VStream &Strm, float value, const char *errmsg, bool dofail=true) {
  if (Strm.IsLoading()) {
    float v;
    Strm << v;
    if (v != value) {
      if (dofail) Host_Error("Save loader: invalid value for %s; got %f, but expected %f", errmsg, v, value);
      GCon->Logf("Save loader: invalid value for %s; got %f, but expected %f (should be harmless)", errmsg, v, value);
    }
  } else {
    Strm << value;
  }
}


//==========================================================================
//
//  VLevel::Serialise
//
//==========================================================================
void VLevel::Serialise (VStream &Strm) {
  guard(VLevel::Serialise);
  int i;
  sector_t *sec;
  line_t *li;
  side_t *si;

  // write/check various numbers, so we won't load invalid save accidentally
  // this is not the best or most reliable way to check it, but it is better
  // than nothing...

  writeOrCheckName(Strm, MapName, "map name");
  writeOrCheckUInt(Strm, LevelFlags, "level flags");

  writeOrCheckInt(Strm, NumVertexes, "vertex count", false);
  writeOrCheckInt(Strm, NumSectors, "sector count");
  writeOrCheckInt(Strm, NumSides, "side count");
  writeOrCheckInt(Strm, NumLines, "line count");
  writeOrCheckInt(Strm, NumSegs, "seg count", false);
  writeOrCheckInt(Strm, NumSubsectors, "subsector count", false);
  writeOrCheckInt(Strm, NumNodes, "node count", false);
  writeOrCheckInt(Strm, NumPolyObjs, "polyobj count");
  writeOrCheckInt(Strm, NumZones, "zone count");

  writeOrCheckFloat(Strm, BlockMapOrgX, "blockmap x origin", false);
  writeOrCheckFloat(Strm, BlockMapOrgY, "blockmap y origin", false);

  // decals
  if (Strm.IsLoading()) decanimlist = nullptr;

  vuint32 sgc = (vuint32)NumSegs;
  Strm << sgc; // just to be sure

  if (sgc) {
    vuint32 dctotal = 0;
    if (Strm.IsLoading()) {
      // load decals
      bool loadDecals = true;
      if (sgc != (vuint32)NumSegs) {
        GCon->Logf("Level load: invalid number of segs, skipping decals");
        loadDecals = false;
      }
      for (int f = 0; f < (int)sgc; ++f) {
        vuint32 dcount = 0;
        // remove old decals
        if (loadDecals) {
          decal_t *odcl = Segs[f].decals;
          while (odcl) {
            decal_t *c = odcl;
            odcl = c->next;
            delete c->animator;
            delete c;
          }
          Segs[f].decals = nullptr;
        }
        // load decal count for this seg
        Strm << dcount;
        decal_t *decal = nullptr; // previous
        while (dcount-- > 0) {
          decal_t *dc = new decal_t;
          memset((void *)dc, 0, sizeof(decal_t));
          if (loadDecals) dc->seg = &Segs[f]; else dc->seg = &Segs[0];
          DecalIO(Strm, dc);
          if (loadDecals) {
            if (dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0 || dc->texture < 0) {
              delete dc->animator;
              delete dc;
            } else {
              // fix backsector
              if (dc->flags&(decal_t::SlideFloor|decal_t::SlideCeil)) {
                line_t *lin = Segs[f].linedef;
                if (!lin) Sys_Error("Save loader: invalid seg linedef (0)!");
                int bsidenum = (dc->flags&decal_t::SideDefOne ? 1 : 0);
                if (lin->sidenum[bsidenum] < 0) Sys_Error("Save loader: invalid seg linedef (1)!");
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
          } else {
            delete dc->animator;
            delete dc;
          }
          ++dctotal;
        }
      }
      GCon->Logf("%u decals %s", dctotal, (loadDecals ? "loaded" : "skipped"));
    } else {
      // save decals
      for (int f = 0; f < (int)sgc; ++f) {
        // count decals
        vuint32 dcount = 0;
        for (decal_t *decal = Segs[f].decals; decal; decal = decal->next) ++dcount;
        Strm << dcount;
        for (decal_t *decal = Segs[f].decals; decal; decal = decal->next) {
          DecalIO(Strm, decal);
          ++dctotal;
        }
      }
      GCon->Logf("%u decals saved", dctotal);
    }
  }

  // sectors
  guard(VLevel::Serialise::Sectors);
  for (i = 0, sec = Sectors; i < NumSectors; ++i, ++sec) {
    Strm << sec->floor.dist
      << sec->floor.TexZ
      << sec->floor.pic
      << sec->floor.xoffs
      << sec->floor.yoffs
      << sec->floor.XScale
      << sec->floor.YScale
      << sec->floor.Angle
      << sec->floor.BaseAngle
      << sec->floor.BaseYOffs
      << sec->floor.flags
      << sec->floor.Alpha
      << sec->floor.MirrorAlpha
      << sec->floor.LightSourceSector
      << sec->floor.SkyBox
      << sec->ceiling.dist
      << sec->ceiling.TexZ
      << sec->ceiling.pic
      << sec->ceiling.xoffs
      << sec->ceiling.yoffs
      << sec->ceiling.XScale
      << sec->ceiling.YScale
      << sec->ceiling.Angle
      << sec->ceiling.BaseAngle
      << sec->ceiling.BaseYOffs
      << sec->ceiling.flags
      << sec->ceiling.Alpha
      << sec->ceiling.MirrorAlpha
      << sec->ceiling.LightSourceSector
      << sec->ceiling.SkyBox
      << sec->params.lightlevel
      << sec->params.LightColour
      << sec->params.Fade
      << sec->params.contents
      << sec->special
      << sec->tag
      << sec->seqType
      << sec->SectorFlags
      << sec->SoundTarget
      << sec->FloorData
      << sec->CeilingData
      << sec->LightingData
      << sec->AffectorData
      << sec->ActionList
      << sec->Damage
      << sec->Friction
      << sec->MoveFactor
      << sec->Gravity
      << sec->Sky;
    if (Strm.IsLoading()) {
      CalcSecMinMaxs(sec);
      sec->ThingList = nullptr;
    }
  }
  if (Strm.IsLoading()) HashSectors();
  unguard;

  // lines
  guard(VLevel::Serialise::Lines);
  for (i = 0, li = Lines; i < NumLines; ++i, ++li) {
    Strm << li->flags
      << li->SpacFlags
      << li->special
      << li->arg1
      << li->arg2
      << li->arg3
      << li->arg4
      << li->arg5
      << li->LineTag
      << li->alpha;
    for (int j = 0; j < 2; ++j) {
      if (li->sidenum[j] == -1) continue;
      si = &Sides[li->sidenum[j]];
      Strm << si->TopTextureOffset
        << si->BotTextureOffset
        << si->MidTextureOffset
        << si->TopRowOffset
        << si->BotRowOffset
        << si->MidRowOffset
        << si->TopTexture
        << si->BottomTexture
        << si->MidTexture
        << si->Flags
        << si->Light;
    }
  }
  if (Strm.IsLoading()) HashLines();
  unguard;

  // polyobjs
  guard(VLevel::Serialise::Polyobjs);
  for (i = 0; i < NumPolyObjs; ++i) {
    if (Strm.IsLoading()) {
      float angle, polyX, polyY;
      Strm << angle << polyX << polyY;
      RotatePolyobj(PolyObjs[i].tag, angle);
      MovePolyobj(PolyObjs[i].tag, polyX-PolyObjs[i].startSpot.x, polyY-PolyObjs[i].startSpot.y);
    } else {
      Strm << PolyObjs[i].angle
        << PolyObjs[i].startSpot.x
        << PolyObjs[i].startSpot.y;
    }
    Strm << PolyObjs[i].SpecialData;
  }
  unguard;

  // static lights
  guard(VLevel::Serialise::StaticLights);
  Strm << STRM_INDEX(NumStaticLights);
  if (Strm.IsLoading()) {
    if (StaticLights) {
      delete[] StaticLights;
      StaticLights = nullptr;
    }
    if (NumStaticLights) StaticLights = new rep_light_t[NumStaticLights];
  }
  for (i = 0; i < NumStaticLights; ++i) {
    Strm << StaticLights[i].Origin
      << StaticLights[i].Radius
      << StaticLights[i].Colour;
  }
  unguard;

  // ACS
  guard(VLevel::Serialise::ACS);
  Acs->Serialise(Strm);
  unguard;

  // camera textures
  guard(VLevel::Serialise::CameraTextures);
  int NumCamTex = CameraTextures.Num();
  Strm << STRM_INDEX(NumCamTex);
  if (Strm.IsLoading()) CameraTextures.SetNum(NumCamTex);
  for (i = 0; i < NumCamTex; ++i) {
    Strm << CameraTextures[i].Camera
      << CameraTextures[i].TexNum
      << CameraTextures[i].FOV;
  }
  unguard;

  // translation tables
  guard(VLevel::Serialise::Translations);
  int NumTrans = Translations.Num();
  Strm << STRM_INDEX(NumTrans);
  if (Strm.IsLoading()) Translations.SetNum(NumTrans);
  for (i = 0; i < NumTrans; ++i) {
    vuint8 Present = !!Translations[i];
    Strm << Present;
    if (Strm.IsLoading()) {
      if (Present) {
        Translations[i] = new VTextureTranslation;
      } else {
        Translations[i] = nullptr;
      }
    }
    if (Present) Translations[i]->Serialise(Strm);
  }
  unguard;

  // body queue translation tables
  guard(VLevel::Serialise::BodyQueueTranslations);
  int NumTrans = BodyQueueTrans.Num();
  Strm << STRM_INDEX(NumTrans);
  if (Strm.IsLoading()) BodyQueueTrans.SetNum(NumTrans);
  for (i = 0; i < NumTrans; ++i) {
    vuint8 Present = !!BodyQueueTrans[i];
    Strm << Present;
    if (Strm.IsLoading()) {
      if (Present) {
        BodyQueueTrans[i] = new VTextureTranslation;
      } else {
        BodyQueueTrans[i] = nullptr;
      }
    }
    if (Present) BodyQueueTrans[i]->Serialise(Strm);
  }
  unguard;

  // zones
  guard(VLevel::Serialise::Zones);
  for (i = 0; i < NumZones; ++i) {
    Strm << STRM_INDEX(Zones[i]);
  }
  unguard;

  unguard;
}


//==========================================================================
//
//  VLevel::ClearReferences
//
//==========================================================================
void VLevel::ClearReferences () {
  guard(VLevel::ClearReferences);
  Super::ClearReferences();
  for (int i = 0; i < NumSectors; ++i) {
    sector_t *sec = Sectors+i;
    if (sec->SoundTarget && sec->SoundTarget->GetFlags()&_OF_CleanupRef) sec->SoundTarget = nullptr;
    if (sec->FloorData && sec->FloorData->GetFlags()&_OF_CleanupRef) sec->FloorData = nullptr;
    if (sec->CeilingData && sec->CeilingData->GetFlags()&_OF_CleanupRef) sec->CeilingData = nullptr;
    if (sec->LightingData && sec->LightingData->GetFlags()&_OF_CleanupRef) sec->LightingData = nullptr;
    if (sec->AffectorData && sec->AffectorData->GetFlags()&_OF_CleanupRef) sec->AffectorData = nullptr;
    if (sec->ActionList && sec->ActionList->GetFlags()&_OF_CleanupRef) sec->ActionList = nullptr;
  }
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].SpecialData && PolyObjs[i].SpecialData->GetFlags()&_OF_CleanupRef) {
      PolyObjs[i].SpecialData = nullptr;
    }
  }
  for (int i = 0; i < CameraTextures.Num(); ++i) {
    if (CameraTextures[i].Camera && CameraTextures[i].Camera->GetFlags()&_OF_CleanupRef) {
      CameraTextures[i].Camera = nullptr;
    }
  }
  unguard;
}


//==========================================================================
//
//  VLevel::Destroy
//
//==========================================================================
void VLevel::Destroy () {
  guard(VLevel::Destroy);

  decanimlist = nullptr; // why not?

  // destroy all thinkers
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

  // call parent class' `Destroy()` method
  Super::Destroy();
  unguard;
}


//==========================================================================
//
//  VLevel::SetCameraToTexture
//
//==========================================================================
void VLevel::SetCameraToTexture (VEntity *Ent, VName TexName, int FOV) {
  guard(VLevel::SetCameraToTexture);
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
  unguard;
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
  guard(VLevel::AddSecnode);
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
  Node->Visited = false;

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
  unguard;
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
  guard(VLevel::DelSecnode);
  msecnode_t *tp; // prev node on thing thread
  msecnode_t *tn; // next node on thing thread
  msecnode_t *sp; // prev node on sector thread
  msecnode_t *sn; // next node on sector thread

  if (Node) {
    // unlink from the Thing thread. The Thing thread begins at
    // sector_list and not from VEntiy->TouchingSectorList
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
  unguard;
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
  guard(VLevel::DelSectorList);
  if (SectorList) {
    msecnode_t *Node = SectorList;
    while (Node) Node = DelSecnode(Node);
    SectorList = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VLevel::SetBodyQueueTrans
//
//==========================================================================
int VLevel::SetBodyQueueTrans (int Slot, int Trans) {
  guard(VLevel::SetBodyQueueTrans);
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
  unguard;
}


//==========================================================================
//
//  VLevel::FindSectorFromTag
//
//==========================================================================
int VLevel::FindSectorFromTag (int tag, int start) {
  guard(VLevel::FindSectorFromTag);
  for (int i = start < 0 ? Sectors[(vuint32)tag%(vuint32)NumSectors].HashFirst : Sectors[start].HashNext;
       i >= 0;
       i = Sectors[i].HashNext)
  {
    if (Sectors[i].tag == tag) return i;
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  VLevel::FindLine
//
//==========================================================================
line_t *VLevel::FindLine (int lineTag, int *searchPosition) {
  guard(VLevel::FindLine);
  for (int i = *searchPosition < 0 ? Lines[(vuint32)lineTag%(vuint32)NumLines].HashFirst : Lines[*searchPosition].HashNext;
       i >= 0;
       i = Lines[i].HashNext)
  {
    if (Lines[i].LineTag == lineTag) {
      *searchPosition = i;
      return &Lines[i];
    }
  }
  *searchPosition = -1;
  return nullptr;
  unguard;
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
  guard(VLevel::PutDecalAtLine);

  if (tex < 0 || tex >= GTextureManager.GetNumTextures()) return;

  // don't process linedef twice
  if (li->decalMark == decanimuid) return;
  li->decalMark = decanimuid;

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

#ifdef VAVOOM_DECALS_DEBUG
  fprintf(stderr, "***PutDecalAtLine***: segdist=%f; prevdir=%d; sidenum=%d; linelen=%f\n", segdist, prevdir, sidenum, linelen);
#endif

  float segd0, segd1;
  if (prevdir < 0) {
    if (segdist >= 0) return; // just in case
    segd0 = segdist+linelen;
    segd1 = segd0+tw;
    segdist = (segd0+segd1)*0.5f;
#ifdef VAVOOM_DECALS_DEBUG
    fprintf(stderr, "** left spread; segdist=%f; segd0=%f; segd1=%f\n", segdist, segd0, segd1);
#endif
  } else if (prevdir > 0) {
    if (segdist <= 0) return; // just in case
    segd1 = segdist;
    segd0 = segd1-tw;
    segdist = (segd0+segd1)*0.5f;
#ifdef VAVOOM_DECALS_DEBUG
    fprintf(stderr, "** right spread; segdist=%f; segd0=%f; segd1=%f\n", segdist, segd0, segd1);
#endif
  } else {
    segd0 = segdist-tw2;
    segd1 = segd0+tw;
  }

  // find segs for this decal (there may be several segs)
  for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
    if (/*seg->linedef == li &&*/ seg->frontsector == sec) {
      if (segd0 >= seg->offset+seg->length || segd1 < seg->offset) {
        //fprintf(stderr, "* SKIP seg: (segd=%f:%f; seg=%f:%f)\n", segd0, segd1, seg->offset, seg->offset+seg->length);
        continue;
      }
      bool slideWithFloor = false;
      bool slideWithCeiling = false;
#ifdef VAVOOM_DECALS_DEBUG
      fprintf(stderr, "  ** found seg: (segd=%f:%f; seg=%f:%f)\n", segd0, segd1, seg->offset, seg->offset+seg->length);
#endif
      int dcmaxcount = r_decal_onetype_max;
           if (tinf.width >= 128 || tinf.height >= 128) dcmaxcount = 8;
      else if (tinf.width >= 64 || tinf.height >= 64) dcmaxcount = 16;
      else if (tinf.width >= 32 || tinf.height >= 32) dcmaxcount = 32;
      // remove old same-typed decals, if necessary
      if (dcmaxcount > 0 && dcmaxcount < 10000) {
        float hiz = orgz+tinf.yoffset*dec->scaleY;
        float loz = hiz-tinf.height*dec->scaleY;
        {
          side_t *sb = &Sides[li->sidenum[sidenum]];
          if (sb->TopTexture <= 0 && sb->BottomTexture <= 0 && sb->MidTexture <= 0) { /*!GCon->Logf("  *** no textures at all (sidenum=%d)", sidenum);*/ continue; }
          if (sb->MidTexture <= 0) {
            if ((li->flags&ML_TWOSIDED) != 0 && li->sidenum[1-sidenum] >= 0) {
              // has other side
              sector_t *bsec = Sides[li->sidenum[1-sidenum]].Sector;
              bool botHit = false, topHit = false;
              //!GCon->Logf("sidenum=%d; orgz=%f; loz=%f; hiz=%f; sec.floorZ=%f; sec.ceilZ=%f; bsec.floorZ=%f; bsec.ceilZ=%f; toptex=%d; midtex=%d; bottex=%d; liflg=%s", sidenum, orgz, loz, hiz, sec->floor.TexZ, sec->ceiling.TexZ, bsec->floor.TexZ, bsec->ceiling.TexZ, sb->TopTexture, sb->MidTexture, sb->BottomTexture, lif2str(li->flags));

              if (sb->BottomTexture > 0 && bsec->floor.TexZ > sec->floor.TexZ) {
                // raises from a floor
                botHit = !(hiz <= sec->floor.TexZ || loz >= bsec->floor.TexZ);
                if (botHit) { if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true; /*!GCon->Logf("  BOTTOM HIT! slide=%d", (slideWithFloor ? 1 : 0));*/ }
              } else {
              }

              if (sb->TopTexture > 0 && bsec->ceiling.TexZ < sec->ceiling.TexZ) {
                // raises from a floor
                topHit = !(hiz <= bsec->ceiling.TexZ || loz >= sec->ceiling.TexZ);
                if (topHit) { if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true; /*!GCon->Logf("  TOP HIT! slide=%d", (slideWithCeiling ? 1 : 0));*/ }
              } else {
              }

              if (!botHit && !topHit) { /*!GCon->Logf("  *** in air");*/ continue; }
            } else {
              // no other side
              //!GCon->Logf("::: sidenum=%d; orgz=%f; loz=%f; hiz=%f; sec.floorZ=%f; sec.ceilZ=%f; toptex=%d; midtex=%d; bottex=%d; liflg=%s", sidenum, orgz, loz, hiz, sec->floor.TexZ, sec->ceiling.TexZ, sb->TopTexture, sb->MidTexture, sb->BottomTexture, lif2str(li->flags));
              if (loz >= sec->floor.TexZ && hiz <= sec->ceiling.TexZ) { /*!GCon->Logf("  *** in air, and no middle texture");*/ continue; }
              if (sb->TopTexture <= 0 && hiz >= sec->ceiling.TexZ) { /*!GCon->Logf("  *** higher than ceiling, and no top texture");*/ continue; }
              if (sb->BottomTexture <= 0 && loz <= sec->floor.TexZ) { /*!GCon->Logf("  *** lower than floor, and no bottom texture");*/ continue; }
            }
          }
        }
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
      decal->picname = dec->pic;
      decal->texture = tex;
      decal->orgz = decal->curz = orgz;
      decal->xdist = /*segd0+tw2*/segdist/*-txofs*/; //tinf.width*0.5f;
      decal->linelen = linelen;
      decal->shade[0] = dec->shade[0];
      decal->shade[1] = dec->shade[1];
      decal->shade[2] = dec->shade[2];
      decal->shade[3] = dec->shade[3];
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
        sector_t *bsec = Sides[li->sidenum[1-sidenum]].Sector;
        decal->flags |= decal_t::SlideFloor|(sidenum == 0 ? decal_t::SideDefOne : 0);
        decal->curz -= bsec->floor.TexZ;
        decal->bsec = bsec;
      } else if (slideWithCeiling) {
        sector_t *bsec = Sides[li->sidenum[1-sidenum]].Sector;
        decal->flags |= decal_t::SlideCeil|(sidenum == 0 ? decal_t::SideDefOne : 0);
        decal->curz -= bsec->ceiling.TexZ;
        decal->bsec = bsec;
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
  fprintf(stderr, "1: segd0=%f; segd1=%f; linelen=%f; prevdir=%d; pos=%f; tw=%f; tw2=%f\n", segd0, segd1, linelen, prevdir, segdist, tw, tw2);
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
            fprintf(stderr, "  pdn: found v2->v1 (f) segd0=%f; segd1=%f\n", segd0, segd1);
#endif
            PutDecalAtLine(tex, orgz, segd0-txofs, dec, spline->frontsector, spline, -1, flips);
          }
          if (spline->backsector && sv1 == lv1) {
#ifdef VAVOOM_DECALS_DEBUG
            fprintf(stderr, "  pdn: found v2->v1 (b) segd0=%f; segd1=%f\n", segd0, segd1);
#endif
            PutDecalAtLine(tex, orgz, segd0-txofs, dec, spline->backsector, spline, -1, flips);
          }
        }
        if (segd1 > linelen && prevdir >= 0) {
          if (spline->frontsector && sv1 == lv2) {
            //FIXME! should this take `sidenum` into account, and use segd1?
#ifdef VAVOOM_DECALS_DEBUG
            fprintf(stderr, "  pdp(0): found v1->v2 (f) segd0=%f; segd1=%f; newsegdist=%f\n", segd0, segd1, segd1-linelen-txofs);
#endif
            PutDecalAtLine(tex, orgz, segd1-linelen-txofs, dec, spline->frontsector, spline, 1, flips);
          }
          if (spline->backsector && sv2 == lv2) {
            if (sidenum == 0) {
#ifdef VAVOOM_DECALS_DEBUG
              fprintf(stderr, "  pdp(0): found v1->v2 (b) segd0=%f; segd1=%f; newsegdist=%f\n", segd0, segd1, linelen-(segdist-txofs));
#endif
              PutDecalAtLine(tex, orgz, /*segd1-linelen*/linelen-(segdist-txofs), dec, spline->backsector, spline, 1, flips);
            } else {
#ifdef VAVOOM_DECALS_DEBUG
              fprintf(stderr, "  pdp(1): found v1->v2 (b) segd0=%f; segd1=%f; newsegdist=%f\n", segd0, segd1, segd1-linelen-txofs);
#endif
              PutDecalAtLine(tex, orgz, segd1-linelen-txofs, dec, spline->backsector, spline, 1, flips);
            }
          }
        }
      }
    }
  }

  unguard;
}


//==========================================================================
//
// VLevel::AddOneDecal
//
//==========================================================================
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, sector_t *sec, line_t *li) {
  if (!dec || !sec || !li) return;

  if (level > 16) {
    GCon->Logf("WARNING: too many lower decals '%s'", *dec->name);
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
      ld->decalMark = 0;
    }
  }

  if (dec->scaleX <= 0 || dec->scaleY <= 0) {
    GCon->Logf("Decal '%s' has zero scale", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (dec->alpha <= 0.1) {
    GCon->Logf("Decal '%s' has zero alpha", *dec->name);
    return;
  }

  int tex = GTextureManager.AddPatch(dec->pic, TEXTYPE_Pic);
  //if (dec->pic == VName("scorch1")) tex = GTextureManager.AddPatch(VName("bulde1"), TEXTYPE_Pic);
  if (tex < 0 || tex >= GTextureManager.GetNumTextures()) {
    // no decal gfx, nothing to do
    GCon->Logf("Decal '%s' has no pic (%s)", *dec->name, *dec->pic);
    return;
  }

  // get picture size, so we can spread it over segs and linedefs
  picinfo_t tinf;
  GTextureManager.GetTextureInfo(tex, &tinf);
  if (tinf.width < 1 || tinf.height < 1) {
    // invisible picture, nothing to do
    GCon->Logf("Decal '%s' has pic without pixels (%s)", *dec->name, *dec->pic);
    return;
  }

  //GCon->Logf("Picture '%s' size: %d, %d  offsets: %d, %d", *dec->pic, tinf.width, tinf.height, tinf.xoffset, tinf.yoffset);

  // setup flips
  vuint32 flips = 0;
  if (dec->flipX == VDecalDef::FlipRandom) {
    if (Random() < 0.5) flips |= decal_t::FlipX;
  } else if (dec->flipX == VDecalDef::FlipAlways) {
    flips |= decal_t::FlipX;
  }
  if (dec->flipY == VDecalDef::FlipRandom) {
    if (Random() < 0.5) flips |= decal_t::FlipY;
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
       if (fabs(dx) > fabs(dy)) dist = (org.x-v1->x)/dx;
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
  guard(VLevel::AddDecal);

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
    AddOneDecal(level, org, dec, sec, li);
  } else {
    if (!baddecals.put(*dectype)) GCon->Logf("NO DECAL: '%s'", *dectype);
  }

  unguard;
}


//==========================================================================
//
//  CalcLine
//
//==========================================================================
void CalcLine (line_t *line) {
  guard(CalcLine);
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
  unguard;
}


//==========================================================================
//
//  CalcSeg
//
//==========================================================================
void CalcSeg (seg_t *seg) {
  guardSlow(CalcSeg);
  seg->Set2Points(*seg->v1, *seg->v2);
  unguardSlow;
}


#ifdef SERVER
//==========================================================================
//
//  SV_LoadLevel
//
//==========================================================================
void SV_LoadLevel (VName MapName) {
  guard(SV_LoadLevel);
#ifdef CLIENT
  GClLevel = nullptr;
#endif
  if (GLevel) {
    delete GLevel;
    GLevel = nullptr;
  }

  GLevel = Spawn<VLevel>();
  GLevel->LevelFlags |= VLevel::LF_ForServer;

  GLevel->LoadMap(MapName);
  unguard;
}
#endif


#ifdef CLIENT
//==========================================================================
//
//  CL_LoadLevel
//
//==========================================================================
void CL_LoadLevel (VName MapName) {
  guard(CL_LoadLevel);
  if (GClLevel) {
    delete GClLevel;
    GClLevel = nullptr;
  }

  GClLevel = Spawn<VLevel>();
  GClGame->GLevel = GClLevel;

  GClLevel->LoadMap(MapName);
  unguard;
}
#endif


//==========================================================================
//
//  AddExtraFloor
//
//==========================================================================
sec_region_t *AddExtraFloor (line_t *line, sector_t *dst) {
  guard(AddExtraFloor);
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
    if (inregion->floor->normal.z != 1.0) {
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
    else if (inregion->ceiling->normal.z != -1.0) {
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
  unguard;
}


//==========================================================================
//
//  SwapPlanes
//
//==========================================================================
void SwapPlanes (sector_t *s) {
  guard(SwapPlanes);
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
  unguard;
}


//==========================================================================
//
//  CalcSecMinMaxs
//
//==========================================================================
void CalcSecMinMaxs (sector_t *sector) {
  guard(CalcSecMinMaxs);
  float minz;
  float maxz;

  if (sector->floor.normal.z == 1.0) {
    // horisontal floor
    sector->floor.minz = sector->floor.dist;
    sector->floor.maxz = sector->floor.dist;
  } else {
    // sloped floor
    minz = 99999.0;
    maxz = -99999.0;
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

  if (sector->ceiling.normal.z == -1.0) {
    // horisontal ceiling
    sector->ceiling.minz = -sector->ceiling.dist;
    sector->ceiling.maxz = -sector->ceiling.dist;
  } else {
    // sloped ceiling
    minz = 99999.0;
    maxz = -99999.0;
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
  unguard;
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
  if (line) idx = (int)(line-Self->Lines);
  RET_INT(idx);
}

IMPLEMENT_FUNCTION(VLevel, PointInSector) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point)->sector);
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
  P_GET_INT(start);
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
