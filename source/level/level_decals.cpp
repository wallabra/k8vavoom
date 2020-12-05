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
#include "../gamedefs.h"

//#define VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
//#define VAVOOM_DECALS_DEBUG

#ifdef VAVOOM_DECALS_DEBUG
# define VDC_DLOG  GCon->Logf
#else
# define VDC_DLOG(...)  do {} while (0)
#endif


extern VCvarB r_decals;

static VCvarI r_decal_onetype_max("r_decal_onetype_max", "128", "Maximum decals of one decaltype on a wall segment.", CVAR_Archive);
static VCvarI r_decal_gore_onetype_max("r_decal_gore_onetype_max", "8", "Maximum decals of one decaltype on a wall segment for Gore Mod.", CVAR_Archive);

// make renderer life somewhat easier by not allowing alot of decals
extern VCvarI gl_bigdecal_limit;
extern VCvarI gl_smalldecal_limit;

TArray<VLevel::DecalLineInfo> VLevel::connectedLines;


//==========================================================================
//
//  lif2str
//
//==========================================================================
static VVA_OKUNUSED const char *lif2str (int flags) {
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
/*
static bool isDecalsOverlap (VDecalDef *dec, float dcx0, float dcy0, decal_t *cur, VTexture *DTex) {
  const float twdt = DTex->GetScaledWidth()*dec->scaleX.value;
  const float thgt = DTex->GetScaledHeight()*dec->scaleY.value;

  const float txofs = DTex->GetScaledSOffset()*dec->scaleX.value;
  const float tyofs = DTex->GetScaledTOffset()*dec->scaleY.value;

  const float myx0 = dcx0;
  const float myx1 = myx0+twdt;
  const float myy0 = dcy0;
  const float myy1 = myy0+thgt;

  const float itx0 = cur->xdist-txofs;
  const float itx1 = itx0+twdt;
  const float ity1 = cur->orgz+cur->scaleY+tyofs;
  const float ity0 = ity1-thgt;

  //GCon->Logf(NAME_Debug, "  my=(%g,%g)-(%g,%g)", myx0, myy0, myx1, myy1);
  //GCon->Logf(NAME_Debug, "  it=(%g,%g)-(%g,%g)", itx0, ity0, itx1, ity1);

  return !(itx1 <= myx0 || ity1 <= myy0 || itx0 >= myx1 || ity0 >= myy1);
}
*/


//==========================================================================
//
//  calcDecalSide
//
//==========================================================================
static int calcDecalSide (const line_t *li, const sector_t *fsec, const sector_t *bsec, const line_t *nline, int origside, int lvnum) {
  // find out correct side
  if (nline->frontsector == fsec || nline->backsector == bsec) return 0;
  if (nline->backsector == fsec || nline->frontsector == bsec) return 1;
  // try to find out the side by using decal spawn point, and current side direction
  // move back a little
  TVec xdir = li->normal;
  if (origside) xdir = -xdir;
  //TVec norg = ((*li->v1)+(*li->v2))*0.5f; // line center
  TVec norg = (lvnum == 0 ? *li->v1 : *li->v2);
  norg += xdir*1024;
  int nside = nline->PointOnSide(norg);
  VDC_DLOG(NAME_Debug, "  (0)nline=%d, detected side %d", (int)(ptrdiff_t)(nline-GLevel->Lines), nside);
  if (li->sidenum[nside] < 0) {
    nside ^= 1;
    if (li->sidenum[nside] < 0) return -1; // wuta?
  }
  VDC_DLOG(NAME_Debug, "  (0)nline=%d, choosen side %d", (int)(ptrdiff_t)(nline-GLevel->Lines), nside);
  /*
  VDC_DLOG(NAME_Debug, "  nline=%d, cannot detect side", (int)(ptrdiff_t)(nline-GLevel->Lines));
  //nside = origside;
  continue;
  */
  return nside;
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
//  VLevel::RemoveDecalAnimator
//
//  this will also delete animator
//  safe to call on decals without an animator//
//
//==========================================================================
void VLevel::RemoveDecalAnimator (decal_t *dc) {
  if (!dc->animator) return;
  if (!dc || (!dc->prevanimated && !dc->nextanimated && decanimlist != dc)) return;
  if (dc->prevanimated) dc->prevanimated->nextanimated = dc->nextanimated; else decanimlist = dc->nextanimated;
  if (dc->nextanimated) dc->nextanimated->prevanimated = dc->prevanimated;
  delete dc->animator;
  dc->animator = nullptr;
  dc->prevanimated = dc->nextanimated = nullptr;
}


//==========================================================================
//
//  VLevel::AllocSegDecal
//
//==========================================================================
decal_t *VLevel::AllocSegDecal (seg_t *seg, VDecalDef *dec) {
  vassert(seg);
  vassert(dec);
  decal_t *decal = new decal_t;
  memset((void *)decal, 0, sizeof(decal_t));
  seg->appendDecal(decal);
  decal->dectype = dec->name;
  //decal->texture = tex;
  //decal->translation = translation;
  //decal->orgz = decal->curz = orgz;
  //decal->xdist = lineofs;
  decal->ofsX = decal->ofsY = 0;
  decal->scaleX = decal->origScaleX = dec->scaleX.value;
  decal->scaleY = decal->origScaleY = dec->scaleY.value;
  decal->alpha = decal->origAlpha = dec->alpha.value;
  decal->addAlpha = dec->addAlpha.value;
  decal->animator = (dec->animator ? dec->animator->clone() : nullptr);
  if (decal->animator) AddAnimatedDecal(decal);
  return decal;
}


#define KILL_CDC_DECAL()  do { \
  RemoveDecalAnimator(cdc); \
  seg->removeDecal(cdc); \
  delete cdc; \
} while (0)


//==========================================================================
//
//  VLevel::CleanupDecals
//
//==========================================================================
void VLevel::CleanupDecals (seg_t *seg) {
  if (!seg) return;
  const int bigLimit = gl_bigdecal_limit.asInt();
  const int smallLimit = gl_smalldecal_limit.asInt();
  if (bigLimit < 1 && smallLimit < 1) return; // nothing to do

  // count decals
  int bigDecalCount = 0, smallDecalCount = 0;
  decal_t *dc = seg->decalhead;
  while (dc) {
    decal_t *cdc = dc;
    dc = dc->next;

    int dcTexId = cdc->texture;
    auto dtex = GTextureManager[dcTexId];
    if (!dtex || dtex->Width < 1 || dtex->Height < 1) {
      // remove this decal (just in case)
      KILL_CDC_DECAL();
      continue;
    }

    const int twdt = (int)(dtex->GetScaledWidth()*cdc->scaleX);
    const int thgt = (int)(dtex->GetScaledHeight()*cdc->scaleY);

    if (!cdc->animator && (twdt < 1 || thgt < 1)) {
      // remove this decal (just in case)
      KILL_CDC_DECAL();
      continue;
    }

    if (twdt >= BigDecalWidth || thgt >= BigDecalHeight) ++bigDecalCount; else ++smallDecalCount;
  }

  int toKillBig = (bigLimit > 0 ? bigDecalCount-bigLimit : 0);
  int toKillSmall = (smallLimit > 0 ? smallDecalCount-smallLimit : 0);
  if (toKillBig <= 0 && toKillSmall <= 0) return;

  if (toKillBig < 0) toKillBig = 0;
  if (toKillSmall < 0) toKillSmall = 0;

  // hack: if we have to remove big decals, limit small decals too
  if (toKillBig > 0 && smallDecalCount > bigLimit) {
    int biglim = bigLimit+bigLimit/4;
    //if (biglim < 1) biglim = 1;
    int tks = smallDecalCount-biglim;
    if (tks > toKillSmall) {
      //GCon->Logf(NAME_Debug, "force-kill %d small decals instead of %d", tks, toKillSmall);
      toKillSmall = tks;
    }
  }

  //if (toKillBig) GCon->Logf(NAME_Debug, "have to kill %d big decals...", toKillBig);
  //if (toKillSmall) GCon->Logf(NAME_Debug, "have to kill %d small decals...", toKillSmall);

  dc = seg->decalhead;
  while (dc && (toKillBig|toKillSmall)) {
    decal_t *cdc = dc;
    dc = dc->next;
    int dcTexId = cdc->texture;
    auto dtex = GTextureManager[dcTexId];

    if (!dtex || dtex->Width < 1 || dtex->Height < 1) {
      // remove this decal (just in case)
      KILL_CDC_DECAL();
      continue;
    }

    const int twdt = (int)(dtex->GetScaledWidth()*cdc->scaleX);
    const int thgt = (int)(dtex->GetScaledHeight()*cdc->scaleY);

    if (!cdc->animator && (twdt < 1 || thgt < 1)) {
      // remove this decal (just in case)
      KILL_CDC_DECAL();
      continue;
    }

    //GCon->Logf(NAME_Debug, "twdt=%g; thgt=%g", twdt, thgt);
    bool doKill = false;
    if (twdt >= BigDecalWidth || thgt >= BigDecalHeight) {
      if (toKillBig) { --toKillBig; doKill = true; }
    } else {
      if (toKillSmall) { --toKillSmall; doKill = true; }
    }

    if (doKill) {
      KILL_CDC_DECAL();
      continue;
    }
  }
}


//==========================================================================
//
//  VLevel::PutDecalAtLine
//
//==========================================================================
void VLevel::PutDecalAtLine (int tex, float orgz, float lineofs, VDecalDef *dec, int side, line_t *li, vuint32 flips, int translation, bool skipMarkCheck) {
  // don't process linedef twice
  if (!skipMarkCheck) {
    if (li->decalMark == decanimuid) return;
    li->decalMark = decanimuid;
  }

  VTexture *DTex = GTextureManager[tex];
  if (!DTex || DTex->Type == TEXTYPE_Null) return;

  //GCon->Logf(NAME_Debug, "decal '%s' at linedef %d", *GTextureManager[tex]->Name, (int)(ptrdiff_t)(li-Lines));

  float twdt = DTex->GetScaledWidth()*dec->scaleX.value;
  float thgt = DTex->GetScaledHeight()*dec->scaleY.value;

  if (twdt < 1 || thgt < 1) return;

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

  const side_t *sidedef = (li->sidenum[side] >= 0 ? &Sides[li->sidenum[side]] : nullptr);
  const TVec &v1 = *li->v1;
  const TVec &v2 = *li->v2;
  const float linelen = (v2-v1).length2D();

  float txofs = DTex->GetScaledSOffset()*dec->scaleX.value;
  // this is not quite right, but i need it this way
  if (flips&decal_t::FlipX) txofs = twdt-txofs;

  const float dcx0 = lineofs-txofs;
  const float dcx1 = dcx0+twdt;

  // check if decal is in line bounds
  if (dcx1 <= 0 || dcx0 >= linelen) {
    // out of bounds
    VDC_DLOG(NAME_Debug, "*** OOB: Decal '%s' at line #%d (side %d; fs=%d; bs=%d): linelen=%g; dcx0=%g; dcx1=%g", *dec->name, (int)(ptrdiff_t)(li-Lines), side, (int)(ptrdiff_t)(fsec-Sectors), (bsec ? (int)(ptrdiff_t)(bsec-Sectors) : -1), linelen, dcx0, dcx1);
  }

  const float tyofs = DTex->GetScaledTOffset()*dec->scaleY.value;
  const float dcy1 = orgz+dec->scaleY.value+tyofs;
  const float dcy0 = dcy1-thgt;

  /*
  int dcmaxcount = r_decal_onetype_max;
       if (twdt >= 128 || thgt >= 128) dcmaxcount = 8;
  else if (twdt >= 64 || thgt >= 64) dcmaxcount = 16;
  else if (twdt >= 32 || thgt >= 32) dcmaxcount = 32;
  //HACK!
  if (VStr::startsWithCI(*dec->name, "K8Gore")) dcmaxcount = r_decal_gore_onetype_max;
  */

  VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d (side %d; fs=%d; bs=%d): linelen=%g; o0=%g; o1=%g (ofsorig=%g; txofs=%g; tyofs=%g; tw=%g; th=%g)", *dec->name, (int)(ptrdiff_t)(li-Lines), side, (int)(ptrdiff_t)(fsec-Sectors), (bsec ? (int)(ptrdiff_t)(bsec-Sectors) : -1), linelen, dcx0, dcx1, lineofs, txofs, tyofs, twdt, thgt);

  TVec linepos = v1+li->ndir*lineofs;

  const float ffloorZ = fsec->floor.GetPointZClamped(linepos);
  const float fceilingZ = fsec->ceiling.GetPointZClamped(linepos);

  const float bfloorZ = (bsec ? bsec->floor.GetPointZClamped(linepos) : ffloorZ);
  const float bceilingZ = (bsec ? bsec->ceiling.GetPointZClamped(linepos) : fceilingZ);

  if (sidedef && (li->flags&(ML_NODECAL|ML_ADDITIVE)) == 0 && (sidedef->Flags&SDF_NODECAL) == 0) {
    // find segs for this decal (there may be several segs)
    // for two-sided lines, put decal on segs for both sectors
    for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
      if (!seg->linedef) continue; // ignore minisegs (just in case)
      //if (seg->frontsub->sector->linecount == 0) continue; // ignore original polyobj sectors (just in case)
      if (seg->flags&SF_ZEROLEN) continue; // invalid seg
      vassert(seg->linedef == li);

      VDC_DLOG(NAME_Debug, "  checking seg #%d; offset=%g; length=%g (ldef=%d; segside=%d)", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length, (int)(ptrdiff_t)(seg->linedef-&Lines[0]), seg->side);

      sector_t *sec3d = seg->backsector;
      const bool has3dregs = (sec3d && sec3d->eregions->next);

      // check for 3d floors
      // for some reason, "oob" segs should still be checked if they has 3d floors, so do this before bounds checking
      //FIXME: reverse and document this!
      if (has3dregs) {
        //if (s3dsnum == 1 && seg->backsector == seg->frontsector) continue;
        VDC_DLOG(NAME_Debug, "*** this seg has 3d floor regions (secnum=%d)!", (int)(ptrdiff_t)(sec3d-&Sectors[0]));
        // find solid region to put decal onto
        for (sec_region_t *reg = sec3d->eregions->next; reg; reg = reg->next) {
          if (!reg->extraline) continue; // no need to create extra side
          // hack: 3d floor with sky texture seems to be transparent in renderer
          if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual)) != 0) continue;
          const side_t *extraside = &Sides[reg->extraline->sidenum[0]];
          if (extraside->MidTexture == skyflatnum) continue;
          const float fz = reg->efloor.GetPointZClamped(linepos);
          const float cz = reg->eceiling.GetPointZClamped(linepos);
          if (dcy0 < cz && dcy1 > fz) {
            VDC_DLOG(NAME_Debug, " HIT solid region: fz=%g; cz=%g; orgz=%g; dcy0=%g; dcy1=%g", fz, cz, orgz, dcy0, dcy1);
            // create decal
            decal_t *decal = AllocSegDecal(seg, dec);
            decal->texture = tex;
            decal->translation = translation;
            decal->orgz = decal->curz = orgz;
            decal->xdist = lineofs;
            // setup misc flags
            decal->flags = flips|(dec->fullbright ? decal_t::Fullbright : 0)|(dec->fuzzy ? decal_t::Fuzzy : 0);
            decal->flags |= /*disabledTextures*/0;
            // slide with 3d floor floor
            decal->slidesec = sec3d;
            decal->flags |= decal_t::SlideFloor;
            decal->curz -= decal->slidesec->floor.TexZ;
            //if (side != seg->side) decal->flags ^= decal_t::FlipX;
            CleanupDecals(seg);
          } else {
            VDC_DLOG(NAME_Debug, " SKIP solid region: fz=%g; cz=%g; orgz=%g; dcy0=%g; dcy1=%g", fz, cz, orgz, dcy0, dcy1);
          }
        }
      }

      // check if decal is in seg bounds
      if (seg->side == 0) {
        // seg running forward
        if (dcx1 <= seg->offset || dcx0 >= seg->offset+seg->length) {
          // out of bounds
          VDC_DLOG(NAME_Debug, "   OOB(side0)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, seg->offset, seg->offset+seg->length);
          continue;
        }
        VDC_DLOG(NAME_Debug, "   HIT(side0)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, seg->offset, seg->offset+seg->length);
      } else {
        // seg running backward
        const float sofs = linelen-seg->offset-seg->length;
        if (dcx1 <= sofs || dcx0 >= sofs+seg->length) {
          // out of bounds
          VDC_DLOG(NAME_Debug, "   OOB(side1)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, sofs, sofs+seg->length);
          continue;
        }
        VDC_DLOG(NAME_Debug, "   HIT(side1)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, sofs, sofs+seg->length);
      }

      const side_t *sb = seg->sidedef;

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

      vuint32 disabledTextures = 0;
      //FIXME: animators are often used to slide blood decals down
      //       until i'll implement proper bounding for animated decals,
      //       just allow bottom textures here
      //  later: don't do it yet, cropping sliding blood is ugly, but acceptable
      /*if (!dec->animator)*/ {
        if (!allowBotTex || min2(dcy0, dcy1) >= max2(ffloorZ, bfloorZ)) disabledTextures |= decal_t::NoBotTex;
      }
      if (!allowTopTex || max2(dcy0, dcy1) <= min2(fceilingZ, bceilingZ)) disabledTextures |= decal_t::NoTopTex;
      if (!hasMidTex) {
        disabledTextures |= decal_t::NoMidTex;
      } else {
        if (min2(dcy0, dcy1) >= max2(ffloorZ, bfloorZ) || max2(dcy0, dcy1) <= min2(fceilingZ, bceilingZ)) {
          // touching midtex
        } else {
          disabledTextures |= decal_t::NoMidTex;
        }
      }

      if (fsec && bsec) {
        VDC_DLOG(NAME_Debug, "  2s: orgz=%g; front=(%g,%g); back=(%g,%g)", orgz, ffloorZ, fceilingZ, bfloorZ, bceilingZ);
        if (hasMidTex && orgz >= max2(ffloorZ, bfloorZ) && orgz <= min2(fceilingZ, bceilingZ)) {
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
          VDC_DLOG(NAME_Debug, "  2s: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
        }

        // door hack
        /*
        if (!slideWithFloor && !slideWithCeiling) {
          if (ffloorZ == fceilingZ || bfloorZ == bceilingZ) {
            slideWithCeiling = (bfloorZ == ffloorZ);
            slideWithFloor = !slideWithCeiling;
            slidesec = (ffloorZ == fceilingZ ? fsec : bsec);
            //GCon->Logf(NAME_Debug, "DOOR HACK: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
          }
        }
        */
      } else {
        VDC_DLOG(NAME_Debug, "  1s: orgz=%g; front=(%g,%g)", orgz, ffloorZ, fceilingZ);
        // one-sided
        if (hasMidTex && orgz >= ffloorZ && orgz <= fceilingZ) {
          // midtexture
               if (li->flags&ML_DONTPEGBOTTOM) slideWithFloor = true;
          else if (li->flags&ML_DONTPEGTOP) slideWithCeiling = true;
          else slideWithCeiling = true;
          //GCon->Logf(NAME_Debug, "one-sided midtex: pegbot=%d; pegtop=%d; fslide=%d; cslide=%d", (int)(!!(li->flags&ML_DONTPEGBOTTOM)), (int)(!!(li->flags&ML_DONTPEGTOP)), (int)slideWithFloor, (int)slideWithCeiling);
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
        VDC_DLOG(NAME_Debug, "  1s: front=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
      }

      VDC_DLOG(NAME_Debug, "  decaling seg #%d; offset=%g; length=%g", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length);

      // create decal
      decal_t *decal = AllocSegDecal(seg, dec);
      decal->texture = tex;
      decal->translation = translation;
      decal->orgz = decal->curz = orgz;
      decal->xdist = lineofs;
      // setup misc flags
      decal->flags = flips|(dec->fullbright ? decal_t::Fullbright : 0)|(dec->fuzzy ? decal_t::Fuzzy : 0);
      decal->flags |= disabledTextures;

      // setup curz and pegs
      if (slideWithFloor) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideFloor;
          decal->curz -= decal->slidesec->floor.TexZ;
          VDC_DLOG(NAME_Debug, "  floor slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
        }
      } else if (slideWithCeiling) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideCeil;
          decal->curz -= decal->slidesec->ceiling.TexZ;
          VDC_DLOG(NAME_Debug, "  ceil slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
        }
      }

      if (side != seg->side) decal->flags ^= decal_t::FlipX;

      CleanupDecals(seg);
    }
  }

  const float dstxofs = dcx0+txofs;

  // if our decal is not completely at linedef, spread it to adjacent linedefs
  const int clnum = connectedLines.length();

  if (dcx0 < 0) {
    // to the left
    VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d: going to the left; ofs=%g; side=%d", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx0, side);
    line_t **ngb = li->v1lines;
    for (int ngbCount = li->v1linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (nline->decalMark == decanimuid) continue;
      nline->decalMark = decanimuid;
      // find out correct side
      int nside = calcDecalSide(li, fsec, bsec, nline, side, 0);
      if (nside < 0) continue;
      if (li->v1 == nline->v2) {
        VDC_DLOG(NAME_Debug, "  v1 at nv2 (%d) (ok)", (int)(ptrdiff_t)(nline-Lines));
        //!later:PutDecalAtLine(tex, orgz, ((*nline->v2)-(*nline->v1)).length2D()+dstxofs, dec, nside, nline, flips, translation);
        DecalLineInfo &dlc = connectedLines.alloc();
        dlc.nline = nline;
        dlc.nside = nside;
        dlc.isv1 = true;
        dlc.isbackside = false;
      } else if (li->v1 == nline->v1) {
        VDC_DLOG(NAME_Debug, "  v1 at nv1 (%d) (opp)", (int)(ptrdiff_t)(nline-Lines));
        //PutDecalAtLine(tex, orgz, dstxofs, dec, (nline->frontsector == fsec ? 0 : 1), nline, flips, translation);
        DecalLineInfo &dlc = connectedLines.alloc();
        dlc.nline = nline;
        dlc.nside = nside;
        dlc.isv1 = true;
        dlc.isbackside = true;
      }
    }
  }

  if (dcx1 > linelen) {
    // to the right
    VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d: going to the right; left=%g; side=%d", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx1-linelen, side);
    line_t **ngb = li->v2lines;
    for (int ngbCount = li->v2linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (nline->decalMark == decanimuid) continue;
      nline->decalMark = decanimuid;
      // find out correct side
      int nside = calcDecalSide(li, fsec, bsec, nline, side, 1);
      if (nside < 0) continue;
      if (li->v2 == nline->v1) {
        VDC_DLOG(NAME_Debug, "  v2 at nv1 (%d) (ok)", (int)(ptrdiff_t)(nline-Lines));
        //!later:PutDecalAtLine(tex, orgz, dstxofs-linelen, dec, nside, nline, flips, translation);
        DecalLineInfo &dlc = connectedLines.alloc();
        dlc.nline = nline;
        dlc.nside = nside;
        dlc.isv1 = false;
        dlc.isbackside = false;
      } else if (li->v2 == nline->v2) {
        VDC_DLOG(NAME_Debug, "  v2 at nv2 (%d) (opp)", (int)(ptrdiff_t)(nline-Lines));
        //PutDecalAtLine(tex, orgz, ((*nline->v2)-(*nline->v1)).length2D()+(dstxofs-linelen), dec, (nline->frontsector == fsec ? 0 : 1), nline, flips, translation);
        DecalLineInfo &dlc = connectedLines.alloc();
        dlc.nline = nline;
        dlc.nside = nside;
        dlc.isv1 = false;
        dlc.isbackside = true;
      }
    }
  }

  // put decals
  const int clend = connectedLines.length();
  for (int cc = clnum; cc < clend; ++cc) {
    DecalLineInfo *dlc = connectedLines.ptr()+cc;
    if (dlc->isv1) {
      if (!dlc->isbackside) {
        PutDecalAtLine(tex, orgz, ((*dlc->nline->v2)-(*dlc->nline->v1)).length2D()+dstxofs, dec, dlc->nside, dlc->nline, flips, translation, true); // skip mark check
      } else {
        VDC_DLOG(NAME_Debug, ":::v1b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt);
        PutDecalAtLine(tex, orgz, -dcx0-twdt+txofs, dec, dlc->nside, dlc->nline, flips^decal_t::FlipX, translation, true); // skip mark check
      }
    } else {
      if (!dlc->isbackside) {
        PutDecalAtLine(tex, orgz, dstxofs-linelen, dec, dlc->nside, dlc->nline, flips, translation, true); // skip mark check
      } else {
        VDC_DLOG(NAME_Debug, ":::v2b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt);
        PutDecalAtLine(tex, orgz, ((*dlc->nline->v2)-(*dlc->nline->v1)).length2D()-(dcx1-linelen)+txofs, dec, dlc->nside, dlc->nline, flips^decal_t::FlipX, translation, true); // skip mark check
      }
    }
  }

  connectedLines.setLength(clnum, false); // don't realloc
}


//==========================================================================
//
// VLevel::AddOneDecal
//
//==========================================================================
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, int side, line_t *li, int translation) {
  if (!dec || !li) return;

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  VDC_DLOG(NAME_Debug, "DECAL: line #%d; side #%d", (int)(ptrdiff_t)(li-&Lines[0]), side);

  if (dec->lowername != NAME_None) {
    //GCon->Logf(NAME_Debug, "adding lower decal '%s' for decal '%s' (level %d)", *dec->lowername, *dec->name, level);
    AddDecal(org, dec->lowername, side, li, level+1, translation);
  }

  //HACK!
  dec->genValues();
  //GCon->Logf(NAME_Debug, "decal '%s': scale=(%g:%g)", *dec->name, dec->scaleX.value, dec->scaleY.value);

  if (dec->scaleX.value <= 0 || dec->scaleY.value <= 0) {
    GCon->Logf(NAME_Warning, "Decal '%s' has zero scale", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (dec->alpha.value <= 0.004f) {
    GCon->Logf(NAME_Warning, "Decal '%s' has zero alpha", *dec->name);
    return;
  }

  int tex = dec->texid;
  VTexture *DTex = GTextureManager[tex];
  if (!DTex || DTex->Type == TEXTYPE_Null) {
    // no decal gfx, nothing to do
    GCon->Logf(NAME_Warning, "Decal '%s' has no pic", *dec->name);
    return;
  }

  //GCon->Logf(NAME_Debug, "Decal '%s', texture '%s'", *dec->name, *DTex->Name);

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
  VDC_DLOG(NAME_Debug, "linelen=%g; dist=%g; lineofs=%g", (v2-v1).length2D(), dist, lineofs);

  connectedLines.reset();
  PutDecalAtLine(tex, org.z, lineofs, dec, side, li, flips, translation, false); // don't skip mark check
}


//==========================================================================
//
// VLevel::AddDecal
//
//==========================================================================
void VLevel::AddDecal (TVec org, VName dectype, int side, line_t *li, int level, int translation) {
  if (!r_decals) return;
  if (!li || dectype == NAME_None) return; // just in case

  //GCon->Logf(NAME_Debug, "%s: oorg:(%g,%g,%g); org:(%g,%g,%g)", *dectype, org.x, org.y, org.z, li->landAlongNormal(org).x, li->landAlongNormal(org).y, li->landAlongNormal(org).z);

  org = li->landAlongNormal(org);

  static TMapNC<VName, bool> baddecals;

#ifdef VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
  //dectype = VName("k8TestDecal");
  dectype = VName("k8TestDecal001");
#endif
  VDecalDef *dec = VDecalDef::getDecal(dectype);
  if (dec) {
    //GCon->Logf(NAME_Debug, "DECAL '%s'; name is '%s', texid is %d; org=(%g,%g,%g)", *dectype, *dec->name, dec->texid, org.x, org.y, org.z);
    AddOneDecal(level, org, dec, side, li, translation);
  } else {
    if (!baddecals.put(dectype, true)) GCon->Logf(NAME_Warning, "NO DECAL: '%s'", *dectype);
  }
}


//==========================================================================
//
// VLevel::AddDecalById
//
//==========================================================================
void VLevel::AddDecalById (TVec org, int id, int side, line_t *li, int level, int translation) {
  if (!r_decals) return;
  if (!li || id < 0) return; // just in case

  org = li->landAlongNormal(org);

  VDecalDef *dec = VDecalDef::getDecalById(id);
  if (dec) AddOneDecal(level, org, dec, side, li, translation);
}


//native final void AddDecal (TVec org, name dectype, int side, line_t *li, optional int translation);
IMPLEMENT_FUNCTION(VLevel, AddDecal) {
  TVec org;
  VName dectype;
  int side;
  line_t *li;
  VOptParamInt translation(0);
  vobjGetParamSelf(org, dectype, side, li, translation);
  Self->AddDecal(org, dectype, side, li, 0, translation);
}

//native final void AddDecalById (TVec org, int id, int side, line_t *li, optional int translation);
IMPLEMENT_FUNCTION(VLevel, AddDecalById) {
  TVec org;
  int id;
  int side;
  line_t *li;
  VOptParamInt translation(0);
  vobjGetParamSelf(org, id, side, li, translation);
  Self->AddDecalById(org, id, side, li, 0, translation);
}
