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
#include "../server/sv_local.h"


static VCvarB gm_compat_corpses_can_hear("gm_compat_corpses_can_hear", false, "Can corpses hear sound propagation?", CVAR_Archive);
static VCvarB gm_compat_everything_can_hear("gm_compat_everything_can_hear", false, "Can everything hear sound propagation?", CVAR_Archive);
static VCvarF gm_compat_max_hearing_distance("gm_compat_max_hearing_distance", "0", "Maximum hearing distance (0 means unlimited)?", CVAR_Archive);
static VCvarB dbg_disable_sound_alert("dbg_disable_sound_alert", false, "Disable sound alerting?", CVAR_PreInit);


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
  if (maxdist <= 0) maxdist = 0; else maxdist *= maxdist; // squared

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
    if (maxdist > 0 && length2DSquared(sndorigin-Ent->Origin) > maxdist) continue;
    if (!recSoundSectorSeenEnts.put(Ent, true)) {
      // register for processing
      elist.append(Ent);
    }
  }

  line_t **slinesptr = sec->lines;
  for (int i = sec->linecount; i--; ++slinesptr) {
    const line_t *check = *slinesptr;
    if (check->sidenum[1] == -1 || !(check->flags&ML_TWOSIDED)) continue;

    // early out for intra-sector lines
    if (check->frontsector == check->backsector) continue;

    /*
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
      // moved door check here, because why not?
      if (!SV_LineOpenings(check, *check->v1, 0xffffffff)) {
        if (!SV_LineOpenings(check, *check->v2, 0xffffffff)) {
          // closed door
          continue;
        }
      }
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

  recSoundSectorList.reset();
  recSoundSectorSeenEnts.reset();
  processSoundSector(validcount, elist, sec, soundblocks, soundtarget, maxdist, sndorigin);

  if (maxdist < 0) maxdist = 0;
  if (gm_compat_max_hearing_distance > 0 && (maxdist == 0 || maxdist > gm_compat_max_hearing_distance)) maxdist = gm_compat_max_hearing_distance;

  // don't use `foreach` here!
  int rspos = 0;
  while (rspos < recSoundSectorList.length()) {
    const SoundSectorListItem *sli = recSoundSectorList.ptr()+rspos;
    processSoundSector(validcount, elist, sli->sec, sli->sblock, soundtarget, maxdist, sndorigin);
    ++rspos;
  }

  //if (recSoundSectorList.length > 1) print("RECSOUND: len=%d", recSoundSectorList.length);
  recSoundSectorList.reset();
  recSoundSectorSeenEnts.reset();
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
  if (!dbg_disable_sound_alert) {
    Self->doRecursiveSound(validcount, *elist, sec, soundblocks, soundtarget, maxdist, sndorigin);
  }
}
