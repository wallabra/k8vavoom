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
#include "gamedefs.h"
#include "sv_local.h"
#ifdef CLIENT
# include "cl_local.h"
#endif


IMPLEMENT_CLASS(V, Level);

VLevel *GLevel;
VLevel *GClLevel;


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
//  VLevelScriptThinker::~VLevelScriptThinker
//
//==========================================================================
VLevelScriptThinker::~VLevelScriptThinker () {
  if (!destroyed) Sys_Error("trying to delete unfinalized Acs script");
}


//==========================================================================
//
//  VLevel::PostCtor
//
//==========================================================================
void VLevel::PostCtor () {
  lineTags = tagHashAlloc();
  sectorTags = tagHashAlloc();
}


//==========================================================================
//
//  VLevel::ResetValidCount
//
//==========================================================================
void VLevel::ResetValidCount () {
  validcount = 1;
  for (auto &&it : allLines()) it.validcount = 0;
  for (auto &&it : allSectors()) it.validcount = 0;
  for (auto &&it : allPolyObjs()) it->validcount = 0;
}


//==========================================================================
//
//  VLevel::IncrementValidCount
//
//==========================================================================
void VLevel::IncrementValidCount () {
  if (++validcount == 0x7fffffff) ResetValidCount();
}


//==========================================================================
//
//  VLevel::ResetSZValidCount
//
//==========================================================================
void VLevel::ResetSZValidCount () {
  validcountSZCache = 1;
  for (auto &&it : allSectors()) it.ZExtentsCacheId = 0;
}


//==========================================================================
//
//  VLevel::IncrementSZValidCount
//
//==========================================================================
void VLevel::IncrementSZValidCount () {
  if (++validcountSZCache == 0x7fffffff) ResetSZValidCount();
}


/* reference code from GZDoom
static int R_PointOnSideSlow(double xx, double yy, node_t *node)
{
  // [RH] This might have been faster than two multiplies and an
  // add on a 386/486, but it certainly isn't on anything newer than that.
  auto x = FloatToFixed(xx);
  auto y = FloatToFixed(yy);
  double  left;
  double  right;

  if (!node->dx)
  {
    if (x <= node->x)
      return node->dy > 0;

    return node->dy < 0;
  }
  if (!node->dy)
  {
    if (y <= node->y)
      return node->dx < 0;

    return node->dx > 0;
  }

  auto dx = (x - node->x);
  auto dy = (y - node->y);

  // Try to quickly decide by looking at sign bits.
  if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000)
  {
    if ((node->dy ^ dx) & 0x80000000)
    {
      // (left is negative)
      return 1;
    }
    return 0;
  }

  // we must use doubles here because the fixed point code will produce errors due to loss of precision for extremely short linedefs.
  // Note that this function is used for all map spawned actors and not just a compatibility fallback!
  left = (double)node->dy * (double)dx;
  right = (double)dy * (double)node->dx;

  if (right < left)
  {
    // front side
    return 0;
  }
  // back side
  return 1;
}
*/


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
    const float dist = node->PointDistance(point);
    //k8: hack for back subsector
    if (dist == 0.0f) {
      #if 0
      if (false && node->splitldef && (node->splitldef->flags&ML_TWOSIDED) &&
          node->splitldef->frontsector != node->splitldef->backsector)
      {
        // if we are exactly on a two-sided linedef, choose node that leads to back sector
        // this is what vanilla does, and some map authors rely on that fact
        /*
        GCon->Logf("ldef=(%g,%g,%g:%g); node=(%g,%g,%g:%g)",
          node->splitldef->normal.x, node->splitldef->normal.y, node->splitldef->normal.z, node->splitldef->dist,
          node->normal.x, node->normal.y, node->normal.z, node->dist);
        */
        const line_t *ldef = node->splitldef;
        // compare plane distance signs to find out the right node
        bool sameSign;
        if (node->dist == ldef->dist) {
          // special case: zero distance
          if (ldef->dist == 0.0f) {
            // don't bother with z, it is always zero
            sameSign = ((node->normal.x < 0.0f) == (ldef->normal.x < 0.0f) && (node->normal.y < 0.0f) == (ldef->normal.y < 0.0f));
          } else {
            sameSign = true;
          }
        } else {
          sameSign = ((node->dist < 0.0f) == (ldef->dist < 0.0f));
        }
        // if the sign is same, back sector is child #1, otherwise it is child #0
        nodenum = node->children[(sameSign ? 1 : 0)];
      } else
      #endif
      // try to emulate original Doom buggy code
      {
        //nodenum = node->children[0];
        //normal = TVec(dir.y, -dir.x, 0.0f);
        // dy:  node.normal.x
        // dx: -node.normal.y
        const float fdx = -node->normal.y;
        const float fdy = +node->normal.x;
             if (fdx == 0) nodenum = node->children[(unsigned)(fdy > 0)];
        else if (fdy == 0) nodenum = node->children[(unsigned)(fdx < 0)];
        else {
          //nodenum = node->children[1/*(unsigned)(dist <= 0.0f)*/]; // is this right?
          vint32 dx = (vint32)(point.x*65536.0)-node->sx;
          vint32 dy = (vint32)(point.y*65536.0)-node->sy;
          // try to quickly decide by looking at sign bits
          if ((node->dy^node->dx^dx^dy)&0x80000000) {
            if ((node->dy^dx)&0x80000000) {
              // (left is negative)
              nodenum = node->children[1];
            } else {
              nodenum = node->children[0];
            }
          } else {
            const double left = (double)node->dy*(double)dx;
            const double right = (double)dy*(double)node->dx;
            nodenum = node->children[(unsigned)(right >= left)];
          }
        }
      }
    } else {
      nodenum = node->children[/*node->PointOnSide(point)*/(unsigned)(dist <= 0.0f)];
    }
  } while ((nodenum&NF_SUBSECTOR) == 0);
  return &Subsectors[nodenum&~NF_SUBSECTOR];
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
    if (PolyObjs[i]->SpecialData && (PolyObjs[i]->SpecialData->GetFlags()&_OF_CleanupRef)) {
      PolyObjs[i]->SpecialData = nullptr;
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
  if (Renderer) Renderer->ClearReferences();
}


//==========================================================================
//
//  VLevel::Destroy
//
//==========================================================================
void VLevel::Destroy () {
  decanimlist = nullptr; // why not?

  tagHashFree(lineTags);
  tagHashFree(sectorTags);

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
  if (Renderer) {
    delete Renderer;
    Renderer = nullptr;
  }

  for (int i = 0; i < NumPolyObjs; ++i) {
    delete[] PolyObjs[i]->segs;
    PolyObjs[i]->segs = nullptr;
    delete[] PolyObjs[i]->originalPts;
    PolyObjs[i]->originalPts = nullptr;
    if (PolyObjs[i]->prevPts) {
      delete[] PolyObjs[i]->prevPts;
      PolyObjs[i]->prevPts = nullptr;
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
    for (int i = 0; i < NumPolyObjs; ++i) delete PolyObjs[i];
    delete[] PolyObjs;
    PolyObjs = nullptr;
  }

  if (PolyAnchorPoints) {
    delete[] PolyAnchorPoints;
    PolyAnchorPoints = nullptr;
  }

  ClearAllMapData();

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
//  VLevel::MoveStaticLightByOwner
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
  if (Renderer) Renderer->MoveStaticLightByOwner(Ent, Origin);
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
int VLevel::FindSectorFromTag (sector_t *&sector, int tag, int start) {
   //k8: just in case
  if (tag == -1 || NumSubsectors < 1) {
    sector = nullptr;
    return -1;
  }

  if (tag == 0) {
    // this has to be special
    if (start < -1) start = -1; // first
    // find next untagged sector
    for (++start; start < NumSectors; ++start) {
      sector_t *sec = &Sectors[start];
      if (sec->sectorTag || sec->moreTags.length()) continue;
      sector = sec;
      return start;
    }
    sector = nullptr;
    return -1;
  }

  if (start < 0) {
    // first
    start = tagHashFirst(sectorTags, tag);
  } else {
    // next
    start = tagHashNext(sectorTags, start, tag);
  }
  sector = (sector_t *)tagHashPtr(sectorTags, start);
  return start;

  /*
  if (tag == 0 || NumSectors < 1) return -1; //k8: just in case
  for (int i = start < 0 ? Sectors[(vuint32)tag%(vuint32)NumSectors].HashFirst : Sectors[start].HashNext;
       i >= 0;
       i = Sectors[i].HashNext)
  {
    if (Sectors[i].tag == tag) return i;
  }
  return -1;
  */
}


//==========================================================================
//
//  VLevel::FindLine
//
//==========================================================================
line_t *VLevel::FindLine (int lineTag, int *searchPosition) {
  if (!searchPosition) return nullptr;
  //k8: should zero tag be allowed here?
  if (lineTag == -1 || NumLines < 1) { *searchPosition = -1; return nullptr; }

  if (lineTag == 0) {
    int start = (searchPosition ? *searchPosition : -1);
    if (start < -1) start = -1;
    // find next untagged line
    for (++start; start < NumLines; ++start) {
      line_t *ldef = &Lines[start];
      if (ldef->lineTag || ldef->moreTags.length()) continue;
      *searchPosition = start;
      return ldef;
    }
    *searchPosition = -1;
    return nullptr;
  }

  if (*searchPosition < 0) {
    // first
    *searchPosition = tagHashFirst(lineTags, lineTag);
  } else {
    // next
    *searchPosition = tagHashNext(lineTags, *searchPosition, lineTag);
  }
  return (line_t *)tagHashPtr(lineTags, *searchPosition);

  /*
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
  */
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
  sector_t *ccsector;
  for (int cshidx = FindSectorFromTag(ccsector, controltag); cshidx >= 0; cshidx = FindSectorFromTag(ccsector, controltag, cshidx)) {
    const int csi = (int)(ptrdiff_t)(ccsector-&Sectors[0]);
    sector_t *sssector;
    for (int lshidx = FindSectorFromTag(sssector, tag); lshidx >= 0; lshidx = FindSectorFromTag(sssector, tag, lshidx)) {
      const int lsi = (int)(ptrdiff_t)(sssector-&Sectors[0]);
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
      VSectorLink &sl = sectorlinks.alloc();
      sl.index = lsi;
      sl.mts = (movetype&0x0f)|(surface ? 1<<30 : 0);
      sl.next = sectorlinkStart[csi];
      sectorlinkStart[csi] = slidx;
    }
  }
}


//==========================================================================
//
//  VLevel::AppendControlLink
//
//  returns `false` for duplicate/invalid link
//
//==========================================================================
bool VLevel::AppendControlLink (const sector_t *src, const sector_t *dest) {
  if (!src || !dest || src == dest) return false; // just in case

  if (ControlLinks.length() == 0) {
    // first time, create empty array
    ControlLinks.setLength(NumSectors);
    for (auto &&link : ControlLinks) {
      link.src = -1;
      link.dest = -1;
      link.next = -1;
    }
  }

  const int srcidx = (int)(ptrdiff_t)(src-Sectors);
  const int destidx = (int)(ptrdiff_t)(dest-Sectors);
  VCtl2DestLink *lnk = &ControlLinks[srcidx];
  if (lnk->dest < 0) {
    // first slot
    vassert(lnk->src == -1);
    vassert(lnk->next == -1);
    lnk->src = srcidx;
    lnk->dest = destidx;
    lnk->next = -1;
  } else {
    // find list tail
    int lastidx = srcidx;
    for (;;) {
      vassert(ControlLinks[lastidx].src == srcidx);
      if (ControlLinks[lastidx].dest == destidx) return false;
      int nli = ControlLinks[lastidx].next;
      if (nli < 0) break;
      lastidx = nli;
    }
    // append to list
    int newidx = ControlLinks.length();
    VCtl2DestLink *newlnk = &ControlLinks.alloc();
    lnk = &ControlLinks[lastidx]; // refresh lnk, because array might be reallocated
    vassert(lnk->next == -1);
    lnk->next = newidx;
    newlnk->src = srcidx;
    newlnk->dest = destidx;
    newlnk->next = -1;
  }

  #if 0
  GCon->Logf(NAME_Debug, "=== AppendControlLink (src=%d; dst=%d) ===", srcidx, destidx);
  for (auto it = IterControlLinks(src); !it.isEmpty(); it.next()) {
    GCon->Logf(NAME_Debug, "   dest=%d", it.getDestSectorIndex());
  }
  #endif

  return true;
}


//==========================================================================
//
//  VLevel::dumpSectorRegions
//
//==========================================================================
void VLevel::dumpRegion (const sec_region_t *reg) {
  if (!reg) return;
  char xflags[128];
  xflags[0] = 0;
  if (reg->regflags&sec_region_t::RF_BaseRegion) strcat(xflags, " [base]");
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

// native final void SetHeightSector (sector_t *Sector, sector_t *SrcSector, int Flags);
// see https://zdoom.org/wiki/Transfer_Heights for flags
IMPLEMENT_FUNCTION(VLevel, SetHeightSector) {
  sector_t *Sector, *SrcSector;
  int Flags;
  vobjGetParamSelf(Sector, SrcSector, Flags);
  if (!Sector || !SrcSector) return;
  if (Sector->heightsec == SrcSector) return; // nothing to do
  //???if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) FakeFCSectors[fcount++] = idx;
  const int destidx = (int)(ptrdiff_t)(Sector-&Self->Sectors[0]);
  const int srcidx = (int)(ptrdiff_t)(SrcSector-&Self->Sectors[0]);
  if (Sector->heightsec) GCon->Logf(NAME_Warning, "tried to set height sector for already set height sector: dest=%d; src=%d", destidx, srcidx);
  if (Sector->fakefloors) Sector->eregions->params = &Sector->params; // this may be called for already inited sector, so restore params
  Sector->heightsec = SrcSector;
  // add to list
  bool found = false;
  for (int xidx = 0; xidx < Self->FakeFCSectors.length(); ++xidx) if (Self->FakeFCSectors[xidx] == srcidx) { found = true; break; }
  if (!found) {
    vint32 &it = Self->FakeFCSectors.alloc();
    it = srcidx;
  }
  if (Self->Renderer) Self->Renderer->SetupFakeFloors(Sector);
}

// native final int FindSectorFromTag (out sector_t *Sector, int tag, optional int start);
IMPLEMENT_FUNCTION(VLevel, FindSectorFromTag) {
  P_GET_INT_OPT(start, -1);
  P_GET_INT(tag);
  P_GET_PTR(sector_t *, osectorp);
  P_GET_SELF;
  sector_t *sector;
  start = Self->FindSectorFromTag(sector, tag, start);
  if (osectorp) *osectorp = sector;
  RET_INT(start);
  //RET_INT(Self->FindSectorFromTag(tag, start));
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
