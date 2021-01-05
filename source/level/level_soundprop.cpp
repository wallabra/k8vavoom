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
#include "../server/sv_local.h"


static VCvarB gm_compat_corpses_can_hear("gm_compat_corpses_can_hear", false, "Can corpses hear sound propagation?", CVAR_Archive);
static VCvarB gm_compat_everything_can_hear("gm_compat_everything_can_hear", false, "Can everything hear sound propagation?", CVAR_Archive);
static VCvarF gm_compat_max_hearing_distance("gm_compat_max_hearing_distance", "0", "Maximum hearing distance (0 means unlimited)?", CVAR_Archive);
static VCvarB dbg_disable_sound_alert("dbg_disable_sound_alert", false, "Disable sound alerting?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
// inter-sector sound propagation code
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
//  VLevel::processSoundSector
//
//==========================================================================
void VLevel::processSoundSector (int validcount, TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin) {
  if (!sec) return;

  // `validcount` and other things were already checked in caller
  // also, caller already set `soundtraversed` and `SoundTarget`
  if (maxdist <= 0.0f) maxdist = 0.0f; else maxdist *= maxdist; // squared
  const bool distUnlim = (maxdist == 0.0f);

  unsigned hmask = 0/*, exmask = VEntity::EFEX_NoInteraction*/;
  if (!gm_compat_everything_can_hear) {
    hmask = VEntity::EF_NoSector|VEntity::EF_NoBlockmap;
    if (!gm_compat_corpses_can_hear) hmask |= VEntity::EF_Corpse;
  }

  for (VEntity *Ent = sec->ThingList; Ent; Ent = Ent->SNext) {
    //FIXME: skip some entities that cannot (possibly) react
    //       this can break some code, but... meh
    //       maybe don't omit corpses?
    if ((Ent->EntityFlags&hmask)|(Ent->FlagsEx&VEntity::EFEX_NoInteraction)) continue;
    if (Ent == soundtarget) continue; // skip target
    // check max distance
    if (!distUnlim && length2DSquared(sndorigin-Ent->Origin) > maxdist) continue;
    if (!recSoundSectorSeenEnts.put(Ent, true)) {
      // register for processing
      elist.append(Ent);
    }
  }

  line_t **slinesptr = sec->lines;
  for (int i = sec->linecount; i--; ++slinesptr) {
    const line_t *check = *slinesptr;

    // ignore one-sided lines
    if (check->sidenum[1] == -1 || !(check->flags&ML_TWOSIDED)) continue;

    // early out for intra-sector lines
    if (check->frontsector == check->backsector) continue;

    /*
    if (check->validcount == validcount) continue;
    check->validcount = validcount;
    */

    /* check for closed door
    if (!SV_LineOpenings(check, *check->v1, 0xffffffff)) {
      if (!SV_LineOpenings(check, *check->v2, 0xffffffff)) {
        // closed door
        continue;
      }
    }
    */

    sector_t *other = (check->frontsector == sec ? check->backsector : check->frontsector);
    if (!other) continue; // just in case

    bool addIt = false;
    int sblock = 0;

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
      // moved door check here, because why not?
      opening_t *op = SV_LineOpenings(check, *check->v1, 0xffffffff);
      while (op && op->range <= 0.0f) op = op->next;
      if (!op) {
        op = SV_LineOpenings(check, *check->v2, 0xffffffff);
        while (op && op->range <= 0.0f) op = op->next;
        if (!op) {
          // closed door
          continue;
        }
      }
      #if 0
      GCon->Logf(NAME_Debug, "  sound to sector: scount=%d; from sector=%d; sector=%d from line %d", sblock, (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(other-&Sectors[0]), (int)(ptrdiff_t)(check-&Lines[0]));
      /*
      op = SV_LineOpenings(check, *check->v1, 0xffffffff);
      if (op) {
        GCon->Log(NAME_Debug, "    v1 openinigs:");
        for (; op; op = op->next) GCon->Logf(NAME_Debug, "      range=%g; top=%g; bottom=%g; lowfloor=%g; highceiling=%g", op->range, op->top, op->bottom, op->lowfloor, op->highceiling);
      }
      op = SV_LineOpenings(check, *check->v2, 0xffffffff);
      if (op) {
        GCon->Log(NAME_Debug, "    v2 openinigs:");
        for (; op; op = op->next) GCon->Logf(NAME_Debug, "      range=%g; top=%g; bottom=%g; lowfloor=%g; highceiling=%g", op->range, op->top, op->bottom, op->lowfloor, op->highceiling);
      }
      */
      #endif
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
//  VLevel::doRecursiveSound
//
//  Called by NoiseAlert. Recursively traverse adjacent sectors, sound
//  blocking lines cut off traversal.
//
//==========================================================================
void VLevel::doRecursiveSound (TArray<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin) {
  IncrementValidCount();

  // wake up all monsters in this sector
  if (!sec || (sec->validcount == validcount && sec->soundtraversed <= soundblocks+1)) return; // already flooded

  //GCon->Log(NAME_Debug, "=== sound propagation ===");

  if (maxdist < 0.0f) maxdist = 0.0f;
  if (gm_compat_max_hearing_distance.asFloat() > 0.0f && (maxdist == 0.0f || maxdist > gm_compat_max_hearing_distance.asFloat())) {
    maxdist = gm_compat_max_hearing_distance.asFloat();
  }

  sec->validcount = validcount;
  sec->soundtraversed = soundblocks+1;
  sec->SoundTarget = soundtarget;

  recSoundSectorList.resetNoDtor();
  recSoundSectorSeenEnts.reset();
  processSoundSector(validcount, elist, sec, soundblocks, soundtarget, maxdist, sndorigin);

  // don't use `foreach` here!
  int rspos = 0;
  while (rspos < recSoundSectorList.length()) {
    const SoundSectorListItem *sli = recSoundSectorList.ptr()+rspos;
    processSoundSector(validcount, elist, sli->sec, sli->sblock, soundtarget, maxdist, sndorigin);
    ++rspos;
  }

  //if (recSoundSectorList.length > 1) print("RECSOUND: len=%d", recSoundSectorList.length);
  //recSoundSectorList.resetNoDtor();
  //recSoundSectorSeenEnts.reset();
}


//native final void doRecursiveSound (ref array!Entity elist, sector_t *sec, int soundblocks, Entity soundtarget, float maxdist, const TVec sndorigin);
IMPLEMENT_FUNCTION(VLevel, doRecursiveSound) {
  int soundblocks;
  TArray<VEntity *> *elist;
  sector_t *sec;
  VEntity *soundtarget;
  float maxdist;
  TVec sndorigin;
  vobjGetParamSelf(elist, sec, soundblocks, soundtarget, maxdist, sndorigin);
  elist->resetNoDtor();
  if (!dbg_disable_sound_alert) {
    Self->doRecursiveSound(*elist, sec, soundblocks, soundtarget, maxdist, sndorigin);
  }
}
