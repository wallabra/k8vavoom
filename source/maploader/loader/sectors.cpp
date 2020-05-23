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
#include "../../gamedefs.h"
#include "loader_common.h"


//==========================================================================
//
//  VLevel::LoadSectors
//
//==========================================================================
void VLevel::LoadSectors (int Lump) {
  // allocate memory for sectors
  NumSectors = W_LumpLength(Lump)/26;
  Sectors = new sector_t[NumSectors];
  memset((void *)Sectors, 0, sizeof(sector_t)*NumSectors);

  // load sectors
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  sector_t *ss = Sectors;
  for (int i = 0; i < NumSectors; ++i, ++ss) {
    // read data
    vint16 floorheight, ceilingheight, lightlevel, special, tag;
    char floorpic[9];
    char ceilingpic[9];
    memset(floorpic, 0, sizeof(floorpic));
    memset(ceilingpic, 0, sizeof(ceilingpic));
    Strm << floorheight << ceilingheight;
    Strm.Serialise(floorpic, 8);
    Strm.Serialise(ceilingpic, 8);
    Strm << lightlevel << special << tag;

    // floor
    ss->floor.Set(TVec(0, 0, 1), floorheight);
    ss->floor.TexZ = floorheight;
    ss->floor.pic = TexNumForName(floorpic, TEXTYPE_Flat);
    ss->floor.xoffs = 0;
    ss->floor.yoffs = 0;
    ss->floor.XScale = 1.0f;
    ss->floor.YScale = 1.0f;
    ss->floor.Angle = 0.0f;
    ss->floor.minz = floorheight;
    ss->floor.maxz = floorheight;
    ss->floor.Alpha = 1.0f;
    ss->floor.MirrorAlpha = 1.0f;
    ss->floor.LightSourceSector = -1;

    // ceiling
    ss->ceiling.Set(TVec(0, 0, -1), -ceilingheight);
    ss->ceiling.TexZ = ceilingheight;
    ss->ceiling.pic = TexNumForName(ceilingpic, TEXTYPE_Flat);
    ss->ceiling.xoffs = 0;
    ss->ceiling.yoffs = 0;
    ss->ceiling.XScale = 1.0f;
    ss->ceiling.YScale = 1.0f;
    ss->ceiling.Angle = 0.0f;
    ss->ceiling.minz = ceilingheight;
    ss->ceiling.maxz = ceilingheight;
    ss->ceiling.Alpha = 1.0f;
    ss->ceiling.MirrorAlpha = 1.0f;
    ss->ceiling.LightSourceSector = -1;

    // params
    ss->params.lightlevel = lightlevel;
    ss->params.LightColor = 0x00ffffff;

    ss->special = special;
    ss->sectorTag = tag;

    ss->seqType = -1; // default seqType
    ss->Gravity = 1.0f; // default sector gravity of 1.0
    ss->Zone = -1;

    ss->CreateBaseRegion();
  }
  //HashSectors(); //k8: do it later, 'cause map fixer can change loaded map
}


//==========================================================================
//
//  VLevel::PostLoadSubsectors
//
//==========================================================================
void VLevel::PostLoadSubsectors () {
  // this can be called with already linked sector<->subsectors, so don't assume anything
  for (auto &&sec : allSectors()) sec.subsectors = nullptr;
  for (auto &&sub : allSubsectors()) sub.seclink = nullptr;

  for (auto &&it : allSubsectorsIdx()) {
    const int idx = it.index();
    subsector_t *ss = it.value();
    if (ss->firstline < 0 || ss->firstline >= NumSegs) Host_Error("Bad seg index %d", ss->firstline);
    if (ss->numlines <= 0 || ss->firstline+ss->numlines > NumSegs) Host_Error("Bad segs range %d %d", ss->firstline, ss->numlines);

    // look up sector number for each subsector
    // (and link this subsector to its parent sector)
    ss->sector = nullptr;
    for (auto &&seg : allSubSegs(ss)) {
      if (seg.linedef) {
        vassert(seg.sidedef);
        vassert(seg.sidedef->Sector);
        ss->sector = seg.sidedef->Sector;
        ss->seclink = ss->sector->subsectors;
        ss->sector->subsectors = ss;
        break;
      }
    }
    if (!ss->sector) Host_Error("Subsector %d contains only minisegs; this is nodes builder bug!", idx);

    // calculate bounding box
    // also, setup frontsector and backsector for segs
    ss->bbox2d[BOX2D_LEFT] = ss->bbox2d[BOX2D_BOTTOM] = 999999.0f;
    ss->bbox2d[BOX2D_RIGHT] = ss->bbox2d[BOX2D_TOP] = -999999.0f;
    for (auto &&seg : allSubSegs(ss)) {
      seg.frontsub = ss;
      // for minisegs, set front and back sectors to subsector owning sector
      if (!seg.linedef) seg.frontsector = seg.backsector = ss->sector;
      // min
      ss->bbox2d[BOX2D_LEFT] = min2(ss->bbox2d[BOX2D_LEFT], min2(seg.v1->x, seg.v2->x));
      ss->bbox2d[BOX2D_BOTTOM] = min2(ss->bbox2d[BOX2D_BOTTOM], min2(seg.v1->y, seg.v2->y));
      // max
      ss->bbox2d[BOX2D_RIGHT] = max2(ss->bbox2d[BOX2D_RIGHT], max2(seg.v1->x, seg.v2->x));
      ss->bbox2d[BOX2D_TOP] = max2(ss->bbox2d[BOX2D_TOP], max2(seg.v1->y, seg.v2->y));
    }

    ss->firstseg = &Segs[ss->firstline];
  }

  for (auto &&it : allSegsIdx()) {
    if (it.value()->frontsub == nullptr) Host_Error("Seg %d: frontsub is not set!", it.index());
    if (it.value()->frontsector == nullptr) {
      if (it.value()->backsector == nullptr) {
        Host_Error("Seg %d: front sector is not set!", it.index());
      } else {
        GCon->Logf(NAME_Warning, "Seg %d: only back sector is set (this may be a bug, but i am not sure)", it.index());
      }
    }
  }
}


//==========================================================================
//
//  VLevel::LoadSubsectors
//
//==========================================================================
void VLevel::LoadSubsectors (int Lump) {
  // determine format of the subsectors data
  int Format;
  if (LevelFlags&LF_GLNodesV5) {
    Format = 5;
    NumSubsectors = W_LumpLength(Lump)/8;
  } else {
    char Header[4];
    W_ReadFromLump(Lump, Header, 0, 4);
    if (memcmp(Header, GL_V3_MAGIC, 4) == 0) {
      Format = 3;
      NumSubsectors = (W_LumpLength(Lump)-4)/8;
    } else {
      Format = 1;
      NumSubsectors = W_LumpLength(Lump)/4;
    }
  }

  // allocate memory for subsectors
  Subsectors = new subsector_t[NumSubsectors];
  memset((void *)Subsectors, 0, sizeof(subsector_t)*NumSubsectors);

  // read data
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  if (Format == 3) Strm.Seek(4);
  subsector_t *ss = Subsectors;
  for (int i = 0; i < NumSubsectors; ++i, ++ss) {
    if (Format < 3) {
      vuint16 numsegs, firstseg;
      Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    } else {
      vint32 numsegs, firstseg;
      Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    }
  }
}


//==========================================================================
//
//  VLevel::GroupLines
//
//  builds sector line lists and subsector sector numbers
//  finds block bounding boxes for sectors
//
//==========================================================================
void VLevel::GroupLines () {
  LinkNode(NumNodes-1, nullptr);

  if (NumSectors > 0) {
    if (Sectors[0].lines) delete[] Sectors[0].lines;
    if (Sectors[0].nbsecs) delete[] Sectors[0].nbsecs;
  }

  // clear counts, just in case
  for (auto &&sector : allSectors()) {
    sector.lines = nullptr;
    sector.linecount = 0;
    sector.nbsecs = nullptr;
    sector.nbseccount = 0;
  }

  // count number of lines, and number of lines in each sector
  int total = 0;
  for (auto &&line : allLines()) {
    ++total;
    ++line.frontsector->linecount;
    if (line.backsector && line.backsector != line.frontsector) {
      ++total;
      ++line.backsector->linecount;
    }
  }

  // build line tables for each sector
  line_t **linebuffer = new line_t*[total+1]; // `+1` just in case
  for (auto &&sec : allSectors()) {
    sec.lines = linebuffer;
    linebuffer += sec.linecount;
  }

  // assign lines for each sector
  TArray<int> SecLineCount;
  SecLineCount.setLength(NumSectors);
  memset(SecLineCount.ptr(), 0, sizeof(int)*SecLineCount.length());

  for (auto &&line : allLines()) {
    if (line.frontsector) {
      line.frontsector->lines[SecLineCount[(int)(ptrdiff_t)(line.frontsector-Sectors)]++] = &line;
    }
    if (line.backsector && line.backsector != line.frontsector) {
      line.backsector->lines[SecLineCount[(int)(ptrdiff_t)(line.backsector-Sectors)]++] = &line;
    }
  }

  for (auto &&it : allSectorsIdx()) {
    sector_t *sector = it.value();
    if (SecLineCount[it.index()] != sector->linecount) Sys_Error("GroupLines: miscounted");

    float bbox2d[4];
    //int block;

    ClearBox2D(bbox2d);
    for (int j = 0; j < sector->linecount; ++j) {
      line_t *li = sector->lines[j];
      AddToBox2D(bbox2d, li->v1->x, li->v1->y);
      AddToBox2D(bbox2d, li->v2->x, li->v2->y);
    }

    // set the soundorg to the middle of the bounding box
    sector->soundorg = TVec((bbox2d[BOX2D_RIGHT]+bbox2d[BOX2D_LEFT])/2.0f, (bbox2d[BOX2D_TOP]+bbox2d[BOX2D_BOTTOM])/2.0f, 0);

    // adjust bounding box to map blocks
    /*
    block = clampval(MapBlock(bbox2d[BOX2D_TOP]-BlockMapOrgY+MAXRADIUS), 0, BlockMapHeight-1);
    sector->blockbox[BOX2D_TOP] = block;

    block = clampval(MapBlock(bbox2d[BOX2D_BOTTOM]-BlockMapOrgY-MAXRADIUS), 0, BlockMapHeight-1);
    sector->blockbox[BOX2D_BOTTOM] = block;

    block = clampval(MapBlock(bbox2d[BOX2D_RIGHT]-BlockMapOrgX+MAXRADIUS), 0, BlockMapWidth-1);
    sector->blockbox[BOX2D_RIGHT] = block;

    block = clampval(MapBlock(bbox2d[BOX2D_LEFT]-BlockMapOrgX-MAXRADIUS), 0, BlockMapWidth-1);
    sector->blockbox[BOX2D_LEFT] = block;
    */
  }

  // collect neighbouring sectors
  if (NumSectors < 1) return; // just in case
  TArray<vuint8> ssmark;
  ssmark.setLength(NumSectors);
  int nbstotal = 0;

  for (auto &&sector : allSectors()) {
    if (!sector.linecount) continue;
    memset(ssmark.ptr(), 0, sizeof(vuint8)*NumSectors);
    for (int j = 0; j < sector.linecount; ++j) {
      const line_t *line = sector.lines[j];
      if (!(line->flags&ML_TWOSIDED)) continue;
      // get neighbour sector
      const sector_t *bsec = (&sector == line->frontsector ? line->backsector : line->frontsector);
      if (!bsec) continue; // just in case
      int snum = (int)(ptrdiff_t)(bsec-Sectors);
      vassert(snum >= 0 && snum < NumSectors);
      if (ssmark[snum]) continue;
      ssmark[snum] = 1;
      ++sector.nbseccount;
      ++nbstotal;
    }
  }

  sector_t **nbsbuffer = new sector_t*[nbstotal+1]; // `+1` just in case
  for (auto &&sector : allSectors()) {
    sector.nbsecs = nbsbuffer;
    //nbsbuffer += sector.nbseccount;
    if (sector.nbseccount) {
      memset(ssmark.ptr(), 0, sizeof(vuint8)*NumSectors);
      for (int j = 0; j < sector.linecount; ++j) {
        const line_t *line = sector.lines[j];
        if (!(line->flags&ML_TWOSIDED)) continue;
        // get neighbour sector
        sector_t *bsec = (&sector == line->frontsector ? line->backsector : line->frontsector);
        if (!bsec) continue; // just in case
        int snum = (int)(ptrdiff_t)(bsec-Sectors);
        vassert(snum >= 0 && snum < NumSectors);
        if (ssmark[snum]) continue;
        ssmark[snum] = 1;
        nbsbuffer[0] = bsec;
        ++nbsbuffer;
      }
    }
  }
  vassert(nbsbuffer == Sectors[0].nbsecs+nbstotal);
  //GCon->Logf("nbs buffer size: %d", nbstotal);
}


//==========================================================================
//
//  VLevel::HashSectors
//
//==========================================================================
void VLevel::HashSectors () {
  /*
  // clear hash; count number of sectors with fake something
  for (int i = 0; i < NumSectors; ++i) Sectors[i].HashFirst = Sectors[i].HashNext = -1;
  // create hash: process sectors in backward order so that they get processed in original order
  for (int i = NumSectors-1; i >= 0; --i) {
    if (Sectors[i].tag) {
      vuint32 HashIndex = (vuint32)Sectors[i].tag%(vuint32)NumSectors;
      Sectors[i].HashNext = Sectors[HashIndex].HashFirst;
      Sectors[HashIndex].HashFirst = i;
    }
  }
  */
  //GCon->Log("hashing sectors");
  tagHashClear(sectorTags);
  for (int i = 0; i < NumSectors; ++i) {
    //GCon->Logf("sector #%d: tag=%d; moretags=%d", i, Sectors[i].sectorTag, Sectors[i].moreTags.length());
    tagHashPut(sectorTags, Sectors[i].sectorTag, &Sectors[i]);
    for (int cc = 0; cc < Sectors[i].moreTags.length(); ++cc) tagHashPut(sectorTags, Sectors[i].moreTags[cc], &Sectors[i]);
  }
}


//==========================================================================
//
//  VLevel::BuildSectorLists
//
//==========================================================================
void VLevel::BuildSectorLists () {
  // count number of fake and tagged sectors
  int fcount = 0;
  int tcount = 0;
  const int scount = NumSectors;

  TArray<int> interesting;
  int intrcount = 0;
  interesting.setLength(scount);

  const sector_t *sec = Sectors;
  for (int i = 0; i < scount; ++i, ++sec) {
    bool intr = false;
    // tagged?
    if (sec->sectorTag) { ++tcount; /*intr = true;*/ }
    else for (int cc = 0; cc < sec->moreTags.length(); ++cc) if (sec->moreTags[cc]) { ++tcount; /*intr = true;*/ break; }
    // with fakes?
         if (sec->deepref) { ++fcount; intr = true; }
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) { ++fcount; intr = true; }
    else if (sec->othersecFloor || sec->othersecCeiling) { ++fcount; intr = true; }
    // register "interesting" sector
    if (intr) interesting[intrcount++] = i;
  }

  GCon->Logf("%d tagged sectors, %d sectors with fakes, %d total sectors", tcount, fcount, scount);

  FakeFCSectors.setLength(fcount);
  //!TaggedSectors.setLength(tcount);
  fcount = tcount = 0;

  for (int i = 0; i < intrcount; ++i) {
    const int idx = interesting[i];
    sec = &Sectors[idx];
    // tagged?
    //!if (sec->tag) TaggedSectors[tcount++] = idx;
    // with fakes?
         if (sec->deepref) FakeFCSectors[fcount++] = idx;
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) FakeFCSectors[fcount++] = idx;
    else if (sec->othersecFloor || sec->othersecCeiling) FakeFCSectors[fcount++] = idx;
  }

  // set "has 3d midtex" flag for sectors
  // do it here for now...
  {
    line_t *ldef = Lines;
    for (int lleft = NumLines; lleft--; ++ldef) {
      if (ldef->flags&ML_3DMIDTEX) {
        //GCon->Logf("linedef #%d has 3dmidtex", (int)(ptrdiff_t)(ldef-&Lines[0]));
        if (ldef->frontsector) {
          //GCon->Logf("sector #%d has 3dmidtex", (int)(ptrdiff_t)(ldef->frontsector-&Sectors[0]));
          ldef->frontsector->SectorFlags |= sector_t::SF_Has3DMidTex;
        }
        if (ldef->backsector) {
          //GCon->Logf("sector #%d has 3dmidtex", (int)(ptrdiff_t)(ldef->backsector-&Sectors[0]));
          ldef->backsector->SectorFlags |= sector_t::SF_Has3DMidTex;
        }
      }
    }
  }
}
