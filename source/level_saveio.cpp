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


//==========================================================================
//
//  DecalIO
//
//==========================================================================
static void DecalIO (VStream &Strm, decal_t *dc, VLevel *level) {
  if (!dc) return;
  {
    VNTValueIOEx vio(&Strm);
    //if (!vio.IsLoading()) GCon->Logf("SAVE: texture: id=%d; name=<%s>", dc->texture.id, *GTextureManager.GetTextureName(dc->texture));
    vio.io(VName("texture"), dc->texture);
    vio.io(VName("dectype"), dc->dectype);
    vio.io(VName("translation"), dc->translation);
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
    vint32 slsec = (Strm.IsLoading() ? -666 : (dc->slidesec ? (int)(ptrdiff_t)(dc->slidesec-&level->Sectors[0]) : -1));
    vio.iodef(VName("slidesec"), slsec, -666);
    if (Strm.IsLoading()) {
      if (slsec == -666) {
        // fix backsector
        if ((dc->flags&(decal_t::SlideFloor|decal_t::SlideCeil)) && !dc->slidesec) {
          line_t *lin = dc->seg->linedef;
          if (!lin) Sys_Error("Save loader: invalid seg linedef (0)!");
          int bsidenum = (dc->flags&decal_t::SideDefOne ? 1 : 0);
          dc->slidesec = (bsidenum ? dc->seg->backsector : dc->seg->frontsector);
          GCon->Logf("Save loader: fixed backsector for decal");
        }
      } else {
        dc->slidesec = (slsec < 0 || slsec >= level->NumSectors ? nullptr : &level->Sectors[slsec]);
      }
    }
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


#define EXTSAVE_NUMSEC_MAGIC  (-0x7fefefea)


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

  // do not save stale map marks
  #ifdef CLIENT
  AM_ClearMarksIfMapChanged(this);
  #endif

  if (Strm.IsLoading()) {
    for (unsigned f = 0; f < (unsigned)NumSubsectors; ++f) {
      // reset subsector update frame
      Subsectors[f].updateWorldFrame = 0;
      // clear seen subsectors
      Subsectors[f].miscFlags &= ~subsector_t::SSMF_Rendered;
    }
    // clear seen segs on loading
    for (i = 0; i < NumSegs; ++i) Segs[i].flags &= ~SF_MAPPED;
  }

  // write/check various numbers, so we won't load invalid save accidentally
  // this is not the best or most reliable way to check it, but it is better
  // than nothing...

  writeOrCheckUInt(Strm, LSSHash, "geometry hash");
  bool segsHashOK = writeOrCheckUInt(Strm, SegHash);

  // decals
  if (Strm.IsLoading()) decanimlist = nullptr;

  // decals
  if (segsHashOK) {
    vuint32 dctotal = 0;
    if (Strm.IsLoading()) {
      // load decals
      vint32 dcSize = 0;
      Strm << dcSize;
      auto stpos = Strm.Tell();
      // load decals
      for (int f = 0; f < (int)NumSegs; ++f) {
        vuint32 dcount = 0;
        // remove old decals
        while (Segs[f].decalhead) {
          decal_t *c = Segs[f].decalhead;
          Segs[f].removeDecal(c);
          delete c->animator;
          delete c;
        }
        Segs[f].decalhead = Segs[f].decaltail = nullptr;
        // load decal count for this seg
        Strm << dcount;
        // hack to not break old saves
        if (dcount == 0xffffffffu) {
          Strm << dcount;
          //decal_t *decal = nullptr; // previous
          while (dcount-- > 0) {
            decal_t *dc = new decal_t;
            memset((void *)dc, 0, sizeof(decal_t));
            dc->seg = &Segs[f];
            DecalIO(Strm, dc, this);
            if (dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0 || dc->texture <= 0) {
              delete dc->animator;
              delete dc;
            } else {
              // add to decal list
              dc->seg = nullptr;
              Segs[f].appendDecal(dc);
              //if (decal) decal->next = dc; else Segs[f].decals = dc;
              if (dc->animator) {
                if (decanimlist) decanimlist->prevanimated = dc;
                dc->nextanimated = decanimlist;
                decanimlist = dc;
              }
              //decal = dc;
            }
            ++dctotal;
          }
        } else {
          // oops, non-zero count, old decal data, skip it
          GCon->Logf("skipping old decal data, cannot load it (it is harmless)");
          Strm.Seek(stpos+dcSize);
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
        for (decal_t *decal = Segs[f].decalhead; decal; decal = decal->next) ++dcount;
        vuint32 newmark = 0xffffffffu;
        Strm << newmark;
        Strm << dcount;
        for (decal_t *decal = Segs[f].decalhead; decal; decal = decal->next) {
          DecalIO(Strm, decal, this);
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

  // hack, to keep compatibility
  bool extSaveVer = !Strm.IsLoading();

  // write "extended save" flag
  if (!Strm.IsLoading()) {
    vint32 scflag = EXTSAVE_NUMSEC_MAGIC;
    Strm << STRM_INDEX(scflag);
  }

  // sectors
  {
    vint32 cnt = NumSectors;
    // check "extended save" magic
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt == EXTSAVE_NUMSEC_MAGIC) {
        extSaveVer = true;
        Strm << STRM_INDEX(cnt);
      }
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
      vio.io(VName("tag"), sec->sectorTag);
      vio.io(VName("seqType"), sec->seqType);
      vio.io(VName("SectorFlags"), sec->SectorFlags);
      vio.io(VName("SoundTarget"), sec->SoundTarget);
      vio.io(VName("FloorData"), sec->FloorData);
      vio.io(VName("CeilingData"), sec->CeilingData);
      vio.io(VName("LightingData"), sec->LightingData);
      vio.io(VName("AffectorData"), sec->AffectorData);
      vio.io(VName("ActionList"), sec->ActionList);
      vio.io(VName("Damage"), sec->Damage);
      vio.io(VName("DamageType"), sec->DamageType);
      vio.io(VName("DamageInterval"), sec->DamageInterval);
      vio.io(VName("DamageLeaky"), sec->DamageLeaky);
      vio.io(VName("Friction"), sec->Friction);
      vio.io(VName("MoveFactor"), sec->MoveFactor);
      vio.io(VName("Gravity"), sec->Gravity);
      vio.io(VName("Sky"), sec->Sky);
      if (Strm.IsLoading()) {
        // load additional tags
        sec->moreTags.clear();
        int moreTagCount = 0;
        vio.iodef(VName("moreTagCount"), moreTagCount, 0);
        if (moreTagCount < 0 || moreTagCount > 32767) Host_Error("invalid lindef");
        char tmpbuf[64];
        for (int mtf = 0; mtf < moreTagCount; ++mtf) {
          snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
          int mtag = 0;
          vio.io(VName(tmpbuf), mtag);
          if (!mtag || mtag == -1) continue;
          bool found = false;
          for (int cc = 0; cc < sec->moreTags.length(); ++cc) if (sec->moreTags[cc] == mtag) { found = true; break; }
          if (found) continue;
          sec->moreTags.append(mtag);
        }
        // setup sector bounds
        CalcSecMinMaxs(sec);
        sec->ThingList = nullptr;
      } else {
        // save more tags
        int moreTagCount = sec->moreTags.length();
        vio.io(VName("moreTagCount"), moreTagCount);
        char tmpbuf[64];
        for (int mtf = 0; mtf < moreTagCount; ++mtf) {
          snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
          int mtag = sec->moreTags[mtf];
          vio.io(VName(tmpbuf), mtag);
        }
      }
    }
    if (Strm.IsLoading()) HashSectors();
  }

  // extended info section
  vint32 hasSegVisibility = 1;
  vint32 hasAutomapMarks =
    #ifdef CLIENT
      1;
    #else
      0;
    #endif
  if (extSaveVer) {
    VNTValueIOEx vio(&Strm);
    vio.io(VName("extflags.hassegvis"), hasSegVisibility);
    vio.io(VName("extflags.hasmapmarks"), hasAutomapMarks);
  } else {
    hasSegVisibility = 0;
    hasAutomapMarks = 0;
  }

  // seg visibility
  bool segvisLoaded = false;
  if (hasSegVisibility) {
    //if (Strm.IsLoading()) GCon->Log("loading seg mapping...");
    vint32 dcSize = 0;
    int dcStartPos = Strm.Tell();
    if (segsHashOK) {
      Strm << dcSize; // will be fixed later for writer
      vint32 segCount = NumSegs;
      Strm << segCount;
      if (segCount == NumSegs && segCount > 0) {
        seg_t *seg = &Segs[0];
        for (i = NumSegs; i--; ++seg) {
          VNTValueIOEx vio(&Strm);
          vio.io(VName("seg.flags"), seg->flags);
          // recheck linedef if we have some mapped segs on it
          if (seg->linedef && (seg->flags&SF_MAPPED)) seg->linedef->exFlags |= (ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
        }
        segvisLoaded = !Strm.IsError();
        // fix size, if necessary
        if (!Strm.IsLoading()) {
          auto currPos = Strm.Tell();
          Strm.Seek(dcStartPos);
          dcSize = currPos-(dcStartPos+4);
          Strm << dcSize;
          Strm.Seek(currPos);
        }
      } else {
        vassert(Strm.IsLoading());
        if (dcSize < 0) Host_Error("invalid segmap size");
        GCon->Logf("segcount doesn't match for seg mapping (this is harmless)");
        Strm.Seek(dcStartPos+4+dcSize);
      }
    } else {
      vassert(Strm.IsLoading());
      Strm << dcSize; // will be fixed later for writer
      if (dcSize < 0) Host_Error("invalid segmap size");
      GCon->Logf("seg hash doesn't match for seg mapping (this is harmless)");
      Strm.Seek(dcStartPos+4+dcSize);
    }
  }

  // automap marks
  if (hasAutomapMarks) {
    int number =
    #ifdef CLIENT
      AM_GetMaxMarks();
    #else
      0;
    #endif
    Strm << STRM_INDEX(number);
    if (number < 0 || number > 1024) Host_Error("invalid automap marks data");
    //GCon->Logf(NAME_Debug, "marks: %d", number);
    if (Strm.IsLoading()) {
      // load automap marks
      #ifdef CLIENT
      AM_ClearMarks();
      #endif
      for (int markidx = 0; markidx < number; ++markidx) {
        VNTValueIOEx vio(&Strm);
        float x = 0, y = 0;
        vint32 active = 0;
        vio.io(VName("mark.active"), active);
        vio.io(VName("mark.x"), x);
        vio.io(VName("mark.y"), y);
        // do not replace user marks
        #ifdef CLIENT
        //GCon->Logf(NAME_Debug, "  idx=%d; pos=(%g,%g); active=%d", markidx, x, y, active);
        if (active && !AM_IsMarkActive(markidx) && isFiniteF(x)) {
          //GCon->Logf(NAME_Debug, "   set!");
          AM_SetMarkXY(markidx, x, y);
        }
        #endif
      }
    } else {
      // save automap marks
      #ifdef CLIENT
      for (int markidx = 0; markidx < number; ++markidx) {
        VNTValueIOEx vio(&Strm);
        float x = AM_GetMarkX(markidx), y = AM_GetMarkY(markidx);
        vint32 active = (AM_IsMarkActive(markidx) ? 1 : 0);
        vio.io(VName("mark.active"), active);
        vio.io(VName("mark.x"), x);
        vio.io(VName("mark.y"), y);
      }
      #else
      vassert(number == 0);
      #endif
    }
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
        vio.io(VName("LineTag"), li->lineTag);
        vio.io(VName("alpha"), li->alpha);
        vio.iodef(VName("locknumber"), li->locknumber, 0);
        if (Strm.IsLoading()) {
          // load additional tags
          li->moreTags.clear();
          int moreTagCount = 0;
          vio.iodef(VName("moreTagCount"), moreTagCount, 0);
          if (moreTagCount < 0 || moreTagCount > 32767) Host_Error("invalid lindef");
          char tmpbuf[64];
          for (int mtf = 0; mtf < moreTagCount; ++mtf) {
            snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
            int mtag = 0;
            vio.io(VName(tmpbuf), mtag);
            if (!mtag || mtag == -1) continue;
            bool found = false;
            for (int cc = 0; cc < li->moreTags.length(); ++cc) if (li->moreTags[cc] == mtag) { found = true; break; }
            if (found) continue;
            li->moreTags.append(mtag);
          }
          // mark partially mapped lines as fully mapped if segvis map is not loaded
          if (!segvisLoaded) {
            if (li->exFlags&(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED)) {
              li->flags |= ML_MAPPED;
            }
            li->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
          }
        } else {
          // save more tags
          int moreTagCount = li->moreTags.length();
          vio.io(VName("moreTagCount"), moreTagCount);
          char tmpbuf[64];
          for (int mtf = 0; mtf < moreTagCount; ++mtf) {
            snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
            int mtag = li->moreTags[mtf];
            vio.io(VName(tmpbuf), mtag);
          }
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

  // restore subsector "rendered" flag
  if (Strm.IsLoading()) {
    if (segvisLoaded) {
      // segment visibility info present
      for (i = 0; i < NumSegs; ++i) {
        if (Segs[i].frontsub && (Segs[i].flags&SF_MAPPED)) {
          Segs[i].frontsub->miscFlags |= subsector_t::SSMF_Rendered;
        }
      }
    } else {
      // no segment visibility info, do nothing
    }
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
      float angle = PolyObjs[i]->angle;
      float polyX = PolyObjs[i]->startSpot.x;
      float polyY = PolyObjs[i]->startSpot.y;
      vio.io(VName("angle"), angle);
      vio.io(VName("startSpot.x"), polyX);
      vio.io(VName("startSpot.y"), polyY);
      if (Strm.IsLoading()) {
        RotatePolyobj(PolyObjs[i]->tag, angle);
        //GCon->Logf("poly #%d: oldpos=(%f,%f)", i, PolyObjs[i]->startSpot.x, PolyObjs[i]->startSpot.y);
        MovePolyobj(PolyObjs[i]->tag, polyX-PolyObjs[i]->startSpot.x, polyY-PolyObjs[i]->startSpot.y);
        //GCon->Logf("poly #%d: newpos=(%f,%f) (%f,%f)", i, PolyObjs[i].startSpot.x, PolyObjs[i].startSpot.y, polyX, polyY);
      }
      vio.io(VName("SpecialData"), PolyObjs[i]->SpecialData);
    }
  }

  // static lights
  {
    TMapNC<vuint32, VEntity *> suidmap;

    Strm << STRM_INDEX(NumStaticLights);
    if (Strm.IsLoading()) {
      if (StaticLights) {
        delete[] StaticLights;
        StaticLights = nullptr;
      }
      if (NumStaticLights) StaticLights = new rep_light_t[NumStaticLights];
    } else {
      // build uid map
      for (TThinkerIterator<VEntity> ent(this); ent; ++ent) {
        if (!ent->ServerUId) GCon->Logf(NAME_Error, "entity '%s:%u' has no suid!", ent->GetClass()->GetName(), ent->GetUniqueId());
        suidmap.put(ent->ServerUId, *ent);
      }
    }

    for (i = 0; i < NumStaticLights; ++i) {
      VNTValueIOEx vio(&Strm);
      //TODO: save static light entity
      vio.io(VName("Origin"), StaticLights[i].Origin);
      vio.io(VName("Radius"), StaticLights[i].Radius);
      vio.io(VName("Color"), StaticLights[i].Color);
      if (Strm.IsLoading()) {
        VEntity *owner = nullptr;
        vio.io(VName("Owner"), owner);
        StaticLights[i].OwnerUId = (owner ? owner->ServerUId : 0);
      } else {
        auto opp = suidmap.find(StaticLights[i].OwnerUId);
        VEntity *owner = (opp ? *opp : nullptr);
        vio.io(VName("Owner"), owner);
      }
      //vio.io(VName("OwnerUId"), StaticLights[i].OwnerUId);
      vio.io(VName("ConeDir"), StaticLights[i].ConeDir);
      vio.io(VName("ConeAngle"), StaticLights[i].ConeAngle);
      vuint32 flags = 0;
      vio.io(VName("Flags"), flags);
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
