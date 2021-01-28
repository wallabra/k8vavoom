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

//#define VV_SETSTATE_DEBUG

#ifdef VV_SETSTATE_DEBUG
# define VSLOGF(...)  GCon->Logf(NAME_Debug, __VA_ARGS__)
#else
# define VSLOGF(...)  (void)0
#endif


IMPLEMENT_CLASS(V, Entity);


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB _decorate_dont_warn_about_invalid_labels("_decorate_dont_warn_about_invalid_labels", false, "Don't do this!", CVAR_Archive|CVAR_PreInit);
static VCvarB dbg_disable_state_advance("dbg_disable_state_advance", false, "Disable states processing (for debug)?", CVAR_PreInit);

static VCvarB dbg_emulate_broken_gozzo_gotos("dbg_emulate_broken_gozzo_gotos", false, "Emulate (partially) broken GZDoom decorate gotos to missing labels?", CVAR_Archive);

static VCvarB vm_optimise_statics("vm_optimise_statics", true, "Try to detect some static things, and don't run physics for them? (DO NOT USE, IT IS GLITCHY!)", CVAR_Archive);

extern VCvarB dbg_vm_show_tick_stats;


// ////////////////////////////////////////////////////////////////////////// //
static VClass *classSectorThinker = nullptr;
static VField *fldNextAffector = nullptr;

static VClass *classScroller = nullptr;
static VField *fldCarryScrollX = nullptr;
static VField *fldCarryScrollY = nullptr;
static VField *fldVDX = nullptr;
static VField *fldVDY = nullptr;
static VField *fldbAccel = nullptr;

static VClass *classEntityEx = nullptr;
static VField *fldbWindThrust = nullptr;
static VField *fldLastScrollOrig = nullptr;

static VClass *classActor = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
struct SavedVObjectPtr {
public:
  VObject **ptr;
  VObject *saved;
public:
  VV_DISABLE_COPY(SavedVObjectPtr)
  inline SavedVObjectPtr (VObject **aptr) noexcept : ptr(aptr), saved(*aptr) {}
  inline ~SavedVObjectPtr () noexcept { *ptr = saved; }
};


struct PCSaver {
public:
  VStateCall **ptr;
  VStateCall *PrevCall;
public:
  VV_DISABLE_COPY(PCSaver)
  inline PCSaver (VStateCall **aptr) noexcept : ptr(aptr), PrevCall(nullptr) { if (aptr) PrevCall = *aptr; }
  inline ~PCSaver () noexcept { if (ptr) *ptr = PrevCall; ptr = nullptr; }
};


// ////////////////////////////////////////////////////////////////////////// //
struct SetStateGuard {
public:
  VEntity *ent;
public:
  VV_DISABLE_COPY(SetStateGuard)
  // constructor increases invocation count
  inline SetStateGuard (VEntity *aent) noexcept : ent(aent) { aent->setStateWatchCat = 0; }
  inline ~SetStateGuard () noexcept { ent->setStateWatchCat = 0; }
};


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VEntity::EntityStaticInit
//
//==========================================================================
void VEntity::EntityStaticInit () {
  classSectorThinker = VClass::FindClassNoCase("SectorThinker");
  if (classSectorThinker) {
    GCon->Log(NAME_Init, "`SectorThinker` class found");
    //SectorThinker NextAffector;
    fldNextAffector = classSectorThinker->FindField("NextAffector");
    if (fldNextAffector && (fldNextAffector->Type.Type != TYPE_Reference || fldNextAffector->Type.Class != classSectorThinker)) fldNextAffector = nullptr;
    if (fldNextAffector) GCon->Logf(NAME_Init, "`SectorThinker.NextAffector` field found (%s)", *fldNextAffector->Type.GetName());
  }

  classScroller = (fldNextAffector ? VClass::FindClassNoCase("Scroller") : nullptr);
  if (classScroller) {
    GCon->Log(NAME_Init, "`Scroller` class found");

    fldCarryScrollX = classScroller->FindField("CarryScrollX");
    if (fldCarryScrollX->Type.Type != TYPE_Float) fldCarryScrollX = nullptr;
    if (fldCarryScrollX) GCon->Logf(NAME_Init, "`Scroller.CarryScrollX` field found (%s)", *fldCarryScrollX->Type.GetName());

    fldCarryScrollY = classScroller->FindField("CarryScrollY");
    if (fldCarryScrollY->Type.Type != TYPE_Float) fldCarryScrollY = nullptr;
    if (fldCarryScrollY) GCon->Logf(NAME_Init, "`Scroller.CarryScrollY` field found (%s)", *fldCarryScrollY->Type.GetName());

    fldVDX = classScroller->FindField("VDX");
    if (fldVDX->Type.Type != TYPE_Float) fldVDX = nullptr;
    if (fldVDX) GCon->Logf(NAME_Init, "`Scroller.VDX` field found (%s)", *fldVDX->Type.GetName());

    fldVDY = classScroller->FindField("VDY");
    if (fldVDY->Type.Type != TYPE_Float) fldVDY = nullptr;
    if (fldVDY) GCon->Logf(NAME_Init, "`Scroller.VDY` field found (%s)", *fldVDY->Type.GetName());

    fldbAccel = classScroller->FindField("bAccel");
    if (fldbAccel->Type.Type != TYPE_Bool) fldbAccel = nullptr;
    if (fldbAccel) GCon->Logf(NAME_Init, "`Scroller.bAccel` field found (%s)", *fldbAccel->Type.GetName());

  }

  classEntityEx = VClass::FindClassNoCase("EntityEx");
  if (classEntityEx) {
    GCon->Log(NAME_Init, "`EntityEx` class found");

    fldbWindThrust = classEntityEx->FindField("bWindThrust");
    if (fldbWindThrust && fldbWindThrust->Type.Type != TYPE_Bool) fldbWindThrust = nullptr; // bad type
    if (fldbWindThrust) GCon->Logf(NAME_Init, "`EntityEx.bWindThrust` field found (%s)", *fldbWindThrust->Type.GetName());

    fldLastScrollOrig = classEntityEx->FindField("lastScrollCheckOrigin");
    if (fldLastScrollOrig && fldLastScrollOrig->Type.Type != TYPE_Vector) fldLastScrollOrig = nullptr; // bad type
    if (fldLastScrollOrig) GCon->Logf(NAME_Init, "`EntityEx.lastScrollCheckOrigin` field found (%s)", *fldLastScrollOrig->Type.GetName());

    if (!fldLastScrollOrig && classScroller) {
      GCon->Log(NAME_Init, "`Scroller` class discarded due to missing EntityEx fields");
      classScroller = nullptr;
    }
  }

  classActor = VClass::FindClassNoCase("Actor");
  if (classActor) GCon->Log(NAME_Init, "`Actor` class found");

  // discard the things we cannot check, to do less work in ticker
  if (fldLastScrollOrig && classScroller) fldCarryScrollX = nullptr;
  if (classScroller && (!fldCarryScrollX || !fldCarryScrollY || !fldVDX || !fldVDY || !fldbAccel)) {
    GCon->Log(NAME_Init, "`Scroller` class discarded due to missing fields");
    GCon->Log(NAME_Init, "`SectorThinker` class discarded due to missing fields");
    classSectorThinker = nullptr;
    fldNextAffector = nullptr;
    classScroller = nullptr;
    fldCarryScrollX = fldCarryScrollY = fldVDX = fldVDY = fldbAccel = nullptr;
  }
}


//==========================================================================
//
//  VEntity::PostCtor
//
//==========================================================================
void VEntity::PostCtor () {
  if (classEntityEx && GetClass()->IsChildOf(classEntityEx)) FlagsEx |= EFEX_IsEntityEx; else FlagsEx &= ~EFEX_IsEntityEx;
  if (classActor && GetClass()->IsChildOf(classActor)) FlagsEx |= EFEX_IsActor; else FlagsEx &= ~EFEX_IsActor;
  Super::PostCtor();
}


//==========================================================================
//
//  VEntity::SerialiseOther
//
//==========================================================================
void VEntity::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (Strm.IsLoading()) {
    if (classEntityEx && GetClass()->IsChildOf(classEntityEx)) FlagsEx |= EFEX_IsEntityEx; else FlagsEx &= ~EFEX_IsEntityEx;
    if (classActor && GetClass()->IsChildOf(classActor)) FlagsEx |= EFEX_IsActor; else FlagsEx &= ~EFEX_IsActor;
    //CHECKME: k8: is this right? voodoo dolls?
    if (EntityFlags&EF_IsPlayer) Player->MO = this;
    SubSector = nullptr; // must mark as not linked
    LinkToWorld(true); // proper floor check
  }
}


//==========================================================================
//
//  VEntity::DestroyThinker
//
//==========================================================================
void VEntity::DestroyThinker () {
  if (!IsGoingToDie()) {
    if (Role == ROLE_Authority) {
      eventDestroyed();
      if (TID) RemoveFromTIDList(); // remove from TID list
      // stop any playing sound
      StopSound(0);
    }

    // unlink from sector and block lists
    // but we may be inside a state action (or VC code), so keep `Sector` and `SubSector` alive
    subsector_t *oldSubSector = SubSector;
    sector_t *oldSector = Sector;
    UnlinkFromWorld();
    if (XLevel) XLevel->DelSectorList();
    // those could be still used, restore
    // note that it is still safe to call `UnlinkFromWorld()` on such "half-linked" object
    // but to play safe, set some guard flags
    EntityFlags |= EF_NoSector|EF_NoBlockmap;
    SubSector = oldSubSector;
    Sector = oldSector;

    Super::DestroyThinker();
  }
}


//==========================================================================
//
//  VEntity::AddedToLevel
//
//==========================================================================
void VEntity::AddedToLevel () {
  VThinker::AddedToLevel();
  if (!XLevel->NextSoundOriginID) XLevel->NextSoundOriginID = 1;
  SoundOriginID = XLevel->NextSoundOriginID+(SNDORG_Entity<<24);
  XLevel->NextSoundOriginID = (XLevel->NextSoundOriginID+1)&0x00ffffff;
}


//==========================================================================
//
//  VEntity::NeedPhysics
//
//==========================================================================
bool VEntity::NeedPhysics () {
  if (IsPlayer()) return true;
  if (Owner) return true; // inventory
  //if (IsPlayerOrMissileOrMonster()) return true;
  //if (WaterLevel != 0) return true; // i don't think that we need to check this
  //if (!Velocity.isZero2D()) return true;
  // roughly smaller than lowest fixed point 16.16 (it is more like 0.0000152587890625)
  if (fabsf(Velocity.x) > 0.000016f*4.0f || fabsf(Velocity.y) > 0.000016f*4.0f) return true;

  // check sticks
  if (FlagsEx&(EFEX_StickToFloor|EFEX_StickToCeiling)) {
    if (FlagsEx&EFEX_StickToFloor) {
      return (Origin.z != CeilingZ);
    } else {
      return (Origin.z != FloorZ);
    }
  }

  if (!(EntityFlags&EF_NoGravity)) {
    // going up, or not stanting on a floor?
    if (Velocity.z > 0.0f || Origin.z != FloorZ) return true;
  } else {
    // no gravity, check for vertical velocity
    if (fabsf(Velocity.z) > 0.000016f*4.0f) return true;
  }

  // interpolation
  // actually, interpolation is done by the renderer, so why bother?
  /*
  bool removeJustMoved = false;
  if (MoveFlags&MVF_JustMoved) {
    TVec odiff = LastMoveOrigin-Origin;
    if (fabsf(odiff.x) > 0.2f || fabsf(odiff.y) > 0.2f || fabsf(odiff.z) > 0.2f) return true;
    removeJustMoved = true;
  }
  */

  if (FlagsEx&EFEX_IsEntityEx) {
    // check for scrollers
    if (classScroller) {
      if (classScroller->InstanceCountWithSub && (EntityFlags&(EF_NoSector|EF_ColideWithWorld)) == EF_ColideWithWorld &&
          Sector && fldLastScrollOrig->GetVec(this) != Origin)
      {
        // do as much as we can here (it is *MUCH* faster than VM code)
        // fallback to VM checks only if they have a chance to succeed
        for (msecnode_t *mnode = TouchingSectorList; mnode; mnode = mnode->TNext) {
          sector_t *sec = mnode->Sector;
          VThinker *th = sec->AffectorData;
          if (!th) continue;
          if (th->GetClass()->IsChildOf(classSectorThinker)) {
            // check if we have any transporters
            //TODO: add another velocity-affecting classes here if they will ever emerge
            bool needCheck = false;
            while (th) {
              if (th->GetClass()->IsChildOf(classScroller)) {
                if (fldCarryScrollX->GetFloat(th) != 0.0f || fldCarryScrollY->GetFloat(th) != 0.0f ||
                    (fldbAccel->GetBool(th) && (fldVDX->GetFloat(th) != 0.0f || fldVDX->GetFloat(th) != 0.0f)))
                {
                  needCheck = true;
                  break;
                }
              }
              th = (VThinker *)fldNextAffector->GetObjectValue(th);
            }
            if (needCheck) {
              if (eventPhysicsCheckScroller()) return true;
              break;
            }
          } else {
            // affector is not a sector thinker: this should not happen, but let's play safe
            if (eventPhysicsCheckScroller()) return true;
            break;
          }
        }
      }
    } else {
      // no scroller class, something wicked that way comes...
      if (eventPhysicsCheckScroller()) return true;
    }

    // check for windthrust
    if (fldbWindThrust && fldbWindThrust->GetBool(this)) return true;
  } else {
    // just in case (everything should be EntityEx here, but let's play safe)
    if (eventPhysicsCheckScroller()) return true;
  }

  /*
  if (removeJustMoved) {
    MoveFlags &= ~MVF_JustMoved;
    LastMoveOrigin = Origin;
  }
  */

  // reset horizontal velocity (just in case, it is either already zero, or very close to zero, but...)
  Velocity.x = Velocity.y = 0.0f;

  return false;
}


//==========================================================================
//
//  VEntity::Tick
//
//==========================================================================
void VEntity::Tick (float deltaTime) {
  ++dbgEntityTickTotal;
  // advance it here, why not
  // may be moved down later if some VC code will start using it
  DataGameTime = XLevel->Time+deltaTime;
  // skip ticker?
  const unsigned eflags = FlagsEx;
  if (eflags&EFEX_NoTickGrav) {
    ++dbgEntityTickNoTick;
    #ifdef CLIENT
    //GCon->Logf(NAME_Debug, "*** %s ***", GetClass()->GetName());
    #endif
    // stick to floor or ceiling?
    if (SubSector) {
      if (eflags&(EFEX_StickToFloor|EFEX_StickToCeiling)) {
        if (eflags&EFEX_StickToFloor) {
          Origin.z = SV_GetLowestSolidPointZ(SubSector->sector, Origin, false); // don't ignore 3d floors
        } else {
          #ifdef CLIENT
          //const float oldz = Origin.z;
          #endif
          Origin.z = SV_GetHighestSolidPointZ(SubSector->sector, Origin, false)-Height; // don't ignore 3d floors
          #ifdef CLIENT
          //GCon->Logf(NAME_Debug, "*** %s ***: stick to ceiling; oldz=%g; newz=%g", GetClass()->GetName(), oldz, Origin.z);
          #endif
        }
      } else if (!(EntityFlags&EF_NoGravity)) {
        // it is always at floor level
        Origin.z = SV_GetLowestSolidPointZ(SubSector->sector, Origin, false); // don't ignore 3d floors
        #ifdef CLIENT
        //GCon->Logf(NAME_Debug, "  : %s down to earth", GetClass()->GetName());
        #endif
      }
    }
    if (eflags&EFEX_NoTickGravLT) {
      #ifdef CLIENT
      //GCon->Logf(NAME_Debug, "  : %s lifetime (lmt=%g)", GetClass()->GetName(), LastMoveTime-deltaTime);
      #endif
      //GCon->Logf(NAME_Debug, "*** %s ***", GetClass()->GetName());
      // perform lifetime logic
      // LastMoveTime is time before the next step
      // PlaneAlpha is fadeout after the time expires:
      // if PlaneAlpha is:
      //   <=0: die immediately
      //    >0: fadeout step time
      // it fades out by 0.016 per step
      LastMoveTime -= deltaTime;
      while (LastMoveTime <= 0) {
        // die now
        if (PlaneAlpha <= 0) {
          //GCon->Logf(NAME_Debug, "%s: DIED!", GetClass()->GetName());
          DestroyThinker();
          return;
        }
        LastMoveTime += PlaneAlpha;
        Alpha -= 0.016;
        // did it faded out completely?
        if (Alpha <= 0.002f) {
          //GCon->Logf(NAME_Debug, "%s: FADED!", GetClass()->GetName());
          DestroyThinker();
          return;
        }
        if (RenderStyle == STYLE_Normal) RenderStyle = STYLE_Translucent;
      }
    }
    return;
  }

  bool doSimplifiedTick = false;
  // allow optimiser in netplay servers too, because why not?
  if (GGameInfo->NetMode != NM_Client && !(FlagsEx&EFEX_AlwaysTick) &&
      vm_optimise_statics.asBool() &&
      (StateTime < 0.0f || StateTime-deltaTime > 0.0f) && !NeedPhysics())
  {
    if (StateTime > 0.0f) StateTime -= deltaTime;
    doSimplifiedTick = true;
  }

  // `Mass` is clamped in `OnMapSpawn()`, and we should take care of it in VC code
  // clamp velocity (just in case)
  if (!doSimplifiedTick) {
    if (dbg_vm_show_tick_stats.asBool()) {
      GCon->Logf(NAME_Debug, "%s: cannot simplify tick; vel=(%g,%g,%g); z=%g; floorz=%g; statetime=%g (%g); Owner=%p; justmoved=%d; vcheck=%d; floorcheck=%d; stcheck=%d; interdiff=(%g,%g,%g)",
      GetClass()->GetName(), Velocity.x, Velocity.y, Velocity.z, Origin.z, FloorZ, StateTime, StateTime-deltaTime, Owner,
      (int)((MoveFlags&MVF_JustMoved) != 0), (int)(!(fabsf(Velocity.x) > 0.000016f*4.0f || fabsf(Velocity.x) > 0.000016f*4.0f)),
      (int)(!(Velocity.z > 0.0f || Origin.z != FloorZ)),
      (int)(StateTime < 0.0f || StateTime-deltaTime > 0.0f),
      (LastMoveOrigin-Origin).x, (LastMoveOrigin-Origin).y, (LastMoveOrigin-Origin).z);
    }
    Velocity.clampScaleInPlace(PHYS_MAXMOVE);
    // call normal ticker
    VThinker::Tick(deltaTime);
  } else {
    ++dbgEntityTickSimple;
    eventSimplifiedTick(deltaTime);
  }
}


//==========================================================================
//
//  VEntity::SetTID
//
//==========================================================================
void VEntity::SetTID (int tid) {
  RemoveFromTIDList();
  if (tid) InsertIntoTIDList(tid);
}


//==========================================================================
//
//  VEntity::InsertIntoTIDList
//
//==========================================================================
void VEntity::InsertIntoTIDList (int tid) {
  vassert(TID == 0);
  TID = tid;
  int HashIndex = tid&(VLevelInfo::TID_HASH_SIZE-1);
  TIDHashPrev = nullptr;
  TIDHashNext = Level->TIDHash[HashIndex];
  if (TIDHashNext) TIDHashNext->TIDHashPrev = this;
  Level->TIDHash[HashIndex] = this;
}


//==========================================================================
//
//  VEntity::RemoveFromTIDList
//
//==========================================================================
void VEntity::RemoveFromTIDList () {
  if (!TID) return; // no TID, which means it's not in the cache
  if (TIDHashNext) TIDHashNext->TIDHashPrev = TIDHashPrev;
  if (TIDHashPrev) {
    TIDHashPrev->TIDHashNext = TIDHashNext;
  } else {
    int HashIndex = TID&(VLevelInfo::TID_HASH_SIZE-1);
    vassert(Level->TIDHash[HashIndex] == this);
    Level->TIDHash[HashIndex] = TIDHashNext;
  }
  TID = 0;
}


//==========================================================================
//
//  VEntity::SetState
//
//  returns true if the actor is still present
//
//==========================================================================
bool VEntity::SetState (VState *InState) {
  if (!InState || IsGoingToDie()) {
    if (developer) GCon->Logf(NAME_Dev, "   (00):%s: dead (0x%04x) before state actions, state is %s", *GetClass()->GetFullName(), GetFlags(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
    State = nullptr;
    StateTime = -1;
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    //GCon->Logf(NAME_Debug, "*** 000: STATE DYING THINKER %u: %s", GetUniqueId(), GetClass()->GetName());
    if (!IsGoingToDie()) DestroyThinker();
    return false;
  }

  // the only way we can arrive here is via decorate call
  if (setStateWatchCat) {
    vassert(InState);
    VSLOGF("%s: recursive(%d) SetState, %s to %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"), *InState->Loc.toStringNoCol());
    setStateNewState = InState;
    return true;
  }

  {
    SetStateGuard guard(this);
    ++validcountState;

    VState *st = InState;
    do {
      if (!st) { State = nullptr; break; }

      if (++setStateWatchCat > 512 /*|| st->validcount == validcountState*/) {
        //k8: FIXME! what to do here?
        GCon->Logf(NAME_Error, "WatchCat interrupted `%s::SetState()` at '%s' (%s)!", *GetClass()->GetFullName(), *st->Loc.toStringNoCol(), (st->validcount == validcountState ? "loop" : "timeout"));
        //StateTime = 13.0f;
        break;
      }
      st->validcount = validcountState;

      VSLOGF("%s: loop SetState(%d), %s to %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"), *st->Loc.toStringNoCol());

      // remember current sprite and frame
      UpdateDispFrameFrom(st);

      State = st;
      StateTime = eventGetStateTime(st, st->Time);
      EntityFlags &= ~EF_FullBright;
      //GCon->Logf("%s: loop SetState(%d), time %g, %s", GetClass()->GetName(), setStateWatchCat, StateTime, *State->Loc.toStringNoCol());

      // modified handling
      // call action functions when the state is set
      if (st->Function && Role == ROLE_Authority) {
        XLevel->CallingState = State;
        {
          SavedVObjectPtr svp(&_stateRouteSelf);
          _stateRouteSelf = nullptr;
          setStateNewState = nullptr;
          ExecuteFunctionNoArgs(this, st->Function); //k8: allow VMT lookups (k8:why?)
          if (IsGoingToDie()) {
            State = nullptr;
            break;
          }
          if (setStateNewState) {
            // recursive invocation set a new state
            st = setStateNewState;
            StateTime = 0.0f;
            continue;
          }
        }
      }

      st = State->NextState;
    } while (!StateTime);
    VSLOGF("%s: SetState(%d), done with %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"));
  }
  vassert(setStateWatchCat == 0);

  if (!State || IsGoingToDie()) {
    //GCon->Logf(NAME_Debug, "*** 001: STATE DYING THINKER %u: %s from %s", GetUniqueId(), GetClass()->GetName(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1;
    if (!IsGoingToDie()) DestroyThinker();
    return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::SetInitialState
//
//  Returns true if the actor is still present.
//
//==========================================================================
void VEntity::SetInitialState (VState *InState) {
  State = InState;
  if (InState) {
    UpdateDispFrameFrom(InState);
    StateTime = eventGetStateTime(InState, InState->Time);
    if (StateTime > 0.0f) StateTime += 0.0002f; // delay it slightly, so spawner may do its business
    // first state can be a goto; follow it
    if (DispSpriteName == NAME_None && InState->NextState && StateTime <= 0.0f) {
      UpdateDispFrameFrom(InState->NextState);
      //GCon->Logf(NAME_Debug, "SetInitialState: jumpfix for `%s`", GetClass()->GetName());
    }
  } else {
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1.0f;
  }
  if (NoTickGravOnIdleType && (!State || StateTime < 0.0f)) PerformOnIdle();
}


//==========================================================================
//
//  VEntity::PerformOnIdle
//
//==========================================================================
void VEntity::PerformOnIdle () {
  //GCon->Logf(NAME_Debug, "*** %s: idle; ntit=%d", GetClass()->GetName(), NoTickGravOnIdleType);
  if (NoTickGravOnIdleType) {
    // remove from blockmap
    if (!(EntityFlags&EF_NoBlockmap)) {
      UnlinkFromWorld();
      EntityFlags |= EF_NoBlockmap;
      LinkToWorld(true); // ...and link back again
    }
    switch (NoTickGravOnIdleType) {
      case 1: // switches object to "k8vavoomInternalNoTickGrav" when it enters idle state (the one with negative duration)
        FlagsEx |= EFEX_NoTickGrav;
        //GCon->Logf(NAME_Debug, "*** %s becomes notick", GetClass()->GetName());
        break;
      case 2: // switches object to "NoInteraction" when it enters idle state (the one with negative duration)
        FlagsEx |= EFEX_NoInteraction;
        //GCon->Logf(NAME_Debug, "*** %s becomes nointeraction", GetClass()->GetName());
        break;
      //default: NoTickGravOnIdleType = 0; break; // just in case
    }
  }
}


//==========================================================================
//
//  VEntity::AdvanceState
//
//==========================================================================
bool VEntity::AdvanceState (float deltaTime) {
  if (dbg_disable_state_advance) return true;
  if (deltaTime < 0.0f) return true; // allow zero delta time to process zero-duration states
  if (!State) return true;

  //if (VStr::strEquCI(GetClass()->GetName(), "DeadDoomImp")) GCon->Logf(NAME_Debug, "%s: ADVANCE(000): state=%s", GetClass()->GetName(), (State ? *State->Loc.toStringNoCol() : "<none>"));

  if (StateTime >= 0.0f) {
    //const bool dbg = isDebugDumpEnt(this);
    //if (dbg) GCon->Logf(NAME_Debug, "%u:%s:%s: StateTime=%g (%g) (nst=%g); delta=%g (%g)", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), StateTime, StateTime*35.0f, StateTime-deltaTime, deltaTime, deltaTime*35.0f);
    // we can came here with zero-duration states; if we'll subtract delta time in such case, we'll overshoot for the whole frame
    if (StateTime > 0.0f) {
      // normal states
      StateTime -= deltaTime;
      // loop here, just in case current state duration is less than our delta time
      while (StateTime <= 0.0f) {
        const float tleft = StateTime; // "overjump time"
        if (!SetState(State->NextState)) return false; // freed itself
        if (!State) break; // just in case
        if (StateTime < 0.0f) { StateTime = -1.0f; break; } // force `-1` here just in case
        if (StateTime <= 0.0f) break; // zero should not end up here, but WatchCat can cause this
        // this somewhat compensates freestep instability (at least revenant missiles are more stable on a short term)
        //if (dbg) GCon->Logf(NAME_Debug, "%u:%s:%s:     tleft=%g; StateTime=%g (%g); rest=%g", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), tleft, StateTime, StateTime*35.0f, StateTime+tleft);
        StateTime += tleft;
      }
    } else {
      // zero-duration state; advance, and delay next non-zero state a little
      if (!SetState(State->NextState)) return false; // freed itself
      if (State) {
        //vassert(StateTime != 0.0f); // WatchCat can cause zero duration
        if (StateTime > 0.0f) StateTime += 0.0002f; // delay it slightly, so spawner may do its business
      }
    }
    if (NoTickGravOnIdleType && (!State || StateTime < 0.0f)) PerformOnIdle();
    //if (dbg && State) GCon->Logf(NAME_Debug, "%u:%s:%s:     END; StateTime=%g (%g); delta=%g (%g)", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), StateTime, StateTime*35.0f, deltaTime, deltaTime*35.0f);
  } else if (NoTickGravOnIdleType) {
    vassert(StateTime < 0.0f);
    PerformOnIdle();
  }
  /*
  if (State) {
    if (VStr::strEquCI(GetClass()->GetName(), "DeadDoomImp")) GCon->Logf(NAME_Debug, "%s: state=%s", GetClass()->GetName(), *State->Loc.toStringNoCol());
    UpdateDispFrameFrom(State);
  }
  */
  return true;
}


//==========================================================================
//
//  VEntity::FindState
//
//==========================================================================
VState *VEntity::FindState (VName StateName, VName SubLabel, bool Exact) {
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName, SubLabel, Exact);
  //k8: there's no need to manually resolve compound labels (like "A.B"), `VClass::FindStateLabel()` will do it for us
  //if (Lbl) GCon->Logf("VEntity::FindState(%s): found '%s' (%s : %s)", GetClass()->GetName(), *StateName, *Lbl->Name, *Lbl->State->Loc.toStringNoCol());
  return (Lbl ? Lbl->State : nullptr);
}


//==========================================================================
//
//  VEntity::FindStateEx
//
//==========================================================================
VState *VEntity::FindStateEx (VStr StateName, bool Exact) {
  TArray<VName> Names;
  VMemberBase::StaticSplitStateLabel(StateName, Names);
  VStateLabel *Lbl = GetClass()->FindStateLabel(Names, Exact);
  return (Lbl ? Lbl->State : nullptr);
}


//==========================================================================
//
//  VEntity::HasSpecialStates
//
//==========================================================================
bool VEntity::HasSpecialStates (VName StateName) {
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName);
  return (Lbl != nullptr && Lbl->SubLabels.Num() > 0);
}


//==========================================================================
//
//  VEntity::GetStateEffects
//
//==========================================================================
void VEntity::GetStateEffects (TArray<VLightEffectDef *> &Lights, TArray<VParticleEffectDef *> &Part) const {
  // clear arrays
  Lights.resetNoDtor();
  Part.resetNoDtor();

  // check for valid state
  if (!State) return;

  // init state light effect
  if (!State->LightInited) {
    State->LightInited = true;
    State->LightDef = nullptr;
    if (State->LightName.length()) State->LightDef = R_FindLightEffect(State->LightName);
  }
  if (State->LightDef) Lights.Append(State->LightDef);

  // add class sprite effects
  for (auto &&it : GetClass()->SpriteEffects) {
    if (it.SpriteIndex != State->SpriteIndex) continue;
    if (it.Frame != -1 && it.Frame != (State->Frame&VState::FF_FRAMEMASK)) continue;
    if (it.LightDef) Lights.Append(it.LightDef);
    if (it.PartDef) Part.Append(it.PartDef);
  }
}


//==========================================================================
//
//  VEntity::HasAnyLightEffects
//
//==========================================================================
bool VEntity::HasAnyLightEffects () const {
  // check for valid state
  if (!State) return false;

  // init state light effect
  if (!State->LightInited) {
    State->LightInited = true;
    State->LightDef = nullptr;
    if (State->LightName.length()) State->LightDef = R_FindLightEffect(State->LightName);
  }
  if (State->LightDef) return true;

  // add class sprite effects
  for (auto &&it : GetClass()->SpriteEffects) {
    if (it.SpriteIndex != State->SpriteIndex) continue;
    if (it.Frame != -1 && it.Frame != (State->Frame&VState::FF_FRAMEMASK)) continue;
    if (it.LightDef) return true;
  }

  return false;
}


//==========================================================================
//
//  VEntity::CallStateChain
//
//==========================================================================
bool VEntity::CallStateChain (VEntity *Actor, VState *AState) {
  if (!Actor) return false;

  // set up state call structure
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: CHAIN (Actor=%s); actorstate=%s; itstate=%s", *GetClass()->GetFullName(), *Actor->GetClass()->GetFullName(), (Actor->State ? *Actor->State->Loc.toStringNoCol() : "<none>"), (AState ? *AState->Loc.toStringNoCol() : "<none>"));
  PCSaver saver(&XLevel->StateCall);
  VStateCall Call;
  Call.Item = this;
  Call.State = AState;
  Call.Result = 1;
  XLevel->StateCall = &Call;

  int RunAway = 0;
  VState *S = AState;
  vuint8 res = 0;
  //const bool dbg = (isDebugDumpEnt(this) || isDebugDumpEnt(Actor));
  //if (dbg && S) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s: CallStateChain ENTER", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), *S->Loc.toStringShort());
  while (S) {
    // check for infinite loops
    if (++RunAway > 512) {
      GCon->Logf(NAME_Warning, "entity '%s' state chain interrupted by WatchCat!", *Actor->GetClass()->GetFullName());
      GCon->Logf(NAME_Warning, "... state: '%s'", *S->Loc.toStringNoCol());
      res = false; // watchcat break, oops
      break;
    }

    //if (dbg) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s:   calling state", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), *S->Loc.toStringShort());
    Call.State = S;
    // call action function
    if (S->Function) {
      XLevel->CallingState = S;
      // assume success by default
      Call.Result = 1;
      ExecuteFunctionNoArgs(Actor, S->Function); //k8: allow VMT lookups (k8:why?)
      // at least one success means overal success (do it later)
      //res |= Call.Result;
    } else {
      Call.Result = 0; // don't modify
    }

    if (Call.State == S) {
      // abort immediately if next state loops to itself
      // in this case the overal result is always false
      if (S->NextState == S) { res = 0; break; }
      // advance to the next state
      S = S->NextState;
      // at least one success means overal success
      res |= Call.Result;
    } else {
      // there was a state jump, result should not be modified
      S = Call.State;
    }
  }

  //if (dbg) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s:  CallStateChain EXIT", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), (S ? *S->Loc.toStringShort() : "<none>"));
  return !!res;
}


//==========================================================================
//
//  VEntity::StartSound
//
//==========================================================================
void VEntity::StartSound (VName Sound, vint32 Channel, float Volume, float Attenuation, bool Loop, bool Local) {
  if (!Sector) return;
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "player sound '%s' (sound class '%s', gender '%s')", *Sound, *SoundClass, *SoundGender);
  Super::StartSound(Origin, SoundOriginID,
    GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
    Channel, Volume, Attenuation, Loop, Local);
}


//==========================================================================
//
//  VEntity::StartLocalSound
//
//==========================================================================
void VEntity::StartLocalSound (VName Sound, vint32 Channel, float Volume, float Attenuation) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  if (Player) {
    Player->eventClientStartSound(
      GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
      TVec(0, 0, 0), /*0*/-666, Channel, Volume, Attenuation, false);
  }
}


//==========================================================================
//
//  VEntity::StopSound
//
//==========================================================================
void VEntity::StopSound (vint32 channel) {
  Super::StopSound(SoundOriginID, channel);
}


//==========================================================================
//
//  VEntity::StartSoundSequence
//
//==========================================================================
void VEntity::StartSoundSequence (VName Name, vint32 ModeNum) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::StartSoundSequence(Origin, SoundOriginID, Name, ModeNum);
}


//==========================================================================
//
//  VEntity::AddSoundSequenceChoice
//
//==========================================================================
void VEntity::AddSoundSequenceChoice (VName Choice) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::AddSoundSequenceChoice(SoundOriginID, Choice);
}


//==========================================================================
//
//  VEntity::StopSoundSequence
//
//==========================================================================
void VEntity::StopSoundSequence () {
  Super::StopSoundSequence(SoundOriginID);
}


//==========================================================================
//
//  VEntity::GetTouchedFloorSectorEx
//
//  used for 3d floors
//  can return `nullptr`
//  `orgsector` can be used to avoid BSP search (nullptr allowed)
//
//==========================================================================
sector_t *VEntity::GetTouchedFloorSectorEx (sector_t **swimmable) {
  if (swimmable) *swimmable = nullptr;
  if (!Sector) return nullptr;
  const float orgz = Origin.z;
  //if (Origin.z != FloorZ) return nullptr;
  if (!Sector->Has3DFloors()) {
    if (Sector->floor.GetPointZClamped(Origin) != orgz) return nullptr;
    return Sector;
  }
  sector_t *bestNonSolid = nullptr;
  sector_t *bestSolid = nullptr;
  float bestNSDist = 99999.0f;
  // check 3d floors
  for (sec_region_t *reg = Sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    if (!reg->extraline) continue;
    if (!reg->extraline->frontsector) continue;
    const float rtopz = reg->eceiling.GetPointZClamped(Origin);
    const float rbotz = reg->efloor.GetPointZClamped(Origin);
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      // swimmable sector
      if (orgz > rbotz && orgz < rtopz) {
        // inside, check for best distance
        const float bdist = orgz-rbotz;
        const float tdist = rtopz-orgz;
        if (bdist < bestNSDist) {
          bestNSDist = bdist;
          bestNonSolid = reg->extraline->frontsector;
        } else if (tdist < bestNSDist) {
          bestNSDist = tdist;
          bestNonSolid = reg->extraline->frontsector;
        }
      }
    } else {
      // solid sector, check floor
      if (rtopz == orgz) {
        //return reg->extraline->frontsector;
        if (!bestSolid) bestSolid = reg->extraline->frontsector;
      }
    }
  }
  if (swimmable) {
    *swimmable = bestNonSolid;
  } else {
    // prefer swimmable
    if (bestNonSolid) return bestNonSolid;
  }
  return bestSolid;
}


//==========================================================================
//
//  VEntity::GetActorTerrain
//
//  cannot return `nullptr`
//
//==========================================================================
VTerrainInfo *VEntity::GetActorTerrain () {
  return SV_TerrainType(Sector ? EFloor.splane->pic.id : -1);
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, GetTouchedFloorSector) {
  vobjGetParamSelf();
  RET_PTR((Self ? Self->GetTouchedFloorSectorEx(nullptr) : nullptr));
}

IMPLEMENT_FUNCTION(VEntity, GetTouchedFloorSectorEx) {
  sector_t **swimmable;
  vobjGetParamSelf(swimmable);
  RET_PTR((Self ? Self->GetTouchedFloorSectorEx(swimmable) : nullptr));
}

IMPLEMENT_FUNCTION(VEntity, SetTID) {
  P_GET_INT(tid);
  P_GET_SELF;
  Self->SetTID(tid);
}

IMPLEMENT_FUNCTION(VEntity, SetState) {
  P_GET_PTR(VState, state);
  P_GET_SELF;
  RET_BOOL(Self->SetState(state));
}

IMPLEMENT_FUNCTION(VEntity, SetInitialState) {
  P_GET_PTR(VState, state);
  P_GET_SELF;
  Self->SetInitialState(state);
}

IMPLEMENT_FUNCTION(VEntity, AdvanceState) {
  P_GET_FLOAT(deltaTime);
  P_GET_SELF;
  RET_BOOL(Self->AdvanceState(deltaTime));
}

IMPLEMENT_FUNCTION(VEntity, FindState) {
  P_GET_BOOL_OPT(Exact, false);
  P_GET_NAME_OPT(SubLabel, NAME_None);
  P_GET_NAME(StateName);
  P_GET_SELF;
  RET_PTR(Self->FindState(StateName, SubLabel, Exact));
}

IMPLEMENT_FUNCTION(VEntity, FindStateEx) {
  P_GET_BOOL_OPT(Exact, false);
  P_GET_STR(StateName);
  P_GET_SELF;
  RET_PTR(Self->FindStateEx(StateName, Exact));
}

IMPLEMENT_FUNCTION(VEntity, HasSpecialStates) {
  P_GET_NAME(StateName);
  P_GET_SELF;
  RET_BOOL(Self->HasSpecialStates(StateName));
}

IMPLEMENT_FUNCTION(VEntity, GetStateEffects) {
  P_GET_PTR(TArray<VParticleEffectDef *>, Part);
  P_GET_PTR(TArray<VLightEffectDef *>, Lights);
  P_GET_SELF;
  Self->GetStateEffects(*Lights, *Part);
}

IMPLEMENT_FUNCTION(VEntity, HasAnyLightEffects) {
  P_GET_SELF;
  RET_BOOL(Self->HasAnyLightEffects());
}

IMPLEMENT_FUNCTION(VEntity, CallStateChain) {
  P_GET_PTR(VState, State);
  P_GET_REF(VEntity, Actor);
  P_GET_SELF;
  RET_BOOL(Self->CallStateChain(Actor, State));
}

//native final void PlaySound (name SoundName, int Channel, optional float Volume,
//                             optional float Atenuation, optional bool Loop, optional bool Local);
IMPLEMENT_FUNCTION(VEntity, PlaySound) {
  VName SoundName;
  int Channel;
  VOptParamFloat Volume(1.0f);
  VOptParamFloat Attenuation(1.0f);
  VOptParamBool Loop(false);
  VOptParamBool Local(false);
  vobjGetParamSelf(SoundName, Channel, Volume, Attenuation, Loop, Local);
  if (Channel&256) Loop = true; // sorry for this magic number
  Channel &= 7; // other bits are flags
  Self->StartSound(SoundName, Channel, Volume, Attenuation, Loop, Local);
}

IMPLEMENT_FUNCTION(VEntity, StopSound) {
  P_GET_INT(Channel);
  P_GET_SELF;
  Self->StopSound(Channel);
}

IMPLEMENT_FUNCTION(VEntity, AreSoundsEquivalent) {
  P_GET_NAME(Sound2);
  P_GET_NAME(Sound1);
  P_GET_SELF;
  RET_BOOL(GSoundManager->ResolveEntitySound(Self->SoundClass,
    Self->SoundGender, Sound1) == GSoundManager->ResolveEntitySound(
    Self->SoundClass, Self->SoundGender, Sound2));
}

IMPLEMENT_FUNCTION(VEntity, IsSoundPresent) {
  P_GET_NAME(Sound);
  P_GET_SELF;
  RET_BOOL(GSoundManager->IsSoundPresent(Self->SoundClass, Self->SoundGender, Sound));
}

IMPLEMENT_FUNCTION(VEntity, StartSoundSequence) {
  P_GET_INT(ModeNum);
  P_GET_NAME(Name);
  P_GET_SELF;
  Self->StartSoundSequence(Name, ModeNum);
}

IMPLEMENT_FUNCTION(VEntity, AddSoundSequenceChoice) {
  P_GET_NAME(Choice);
  P_GET_SELF;
  Self->AddSoundSequenceChoice(Choice);
}

IMPLEMENT_FUNCTION(VEntity, StopSoundSequence) {
  P_GET_SELF;
  Self->StopSoundSequence();
}

IMPLEMENT_FUNCTION(VEntity, SetDecorateFlag) {
  P_GET_BOOL(Value);
  P_GET_STR(Name);
  P_GET_SELF;
  Self->SetDecorateFlag(Name, Value);
}


IMPLEMENT_FUNCTION(VEntity, GetDecorateFlag) {
  P_GET_STR(Name);
  P_GET_SELF;
  RET_BOOL(Self->GetDecorateFlag(Name));
}



// native final void QS_PutInt (name fieldname, int value);
IMPLEMENT_FUNCTION(VEntity, QS_PutInt) {
  P_GET_INT(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateInt(Self, name, value));
}

// native final void QS_PutName (name fieldname, name value);
IMPLEMENT_FUNCTION(VEntity, QS_PutName) {
  P_GET_NAME(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateName(Self, name, value));
}

// native final void QS_PutStr (name fieldname, string value);
IMPLEMENT_FUNCTION(VEntity, QS_PutStr) {
  P_GET_STR(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateStr(Self, name, value));
}

// native final void QS_PutFloat (name fieldname, float value);
IMPLEMENT_FUNCTION(VEntity, QS_PutFloat) {
  P_GET_FLOAT(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateFloat(Self, name, value));
}


// native final int QS_GetInt (name fieldname, optional int defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetInt) {
  P_GET_INT_OPT(value, 0);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Int) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.ival = value;
  }
  RET_INT(ret.ival);
}

// native final name QS_GetName (name fieldname, optional name defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetName) {
  P_GET_NAME_OPT(value, NAME_None);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Name) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.nval = value;
  }
  RET_NAME(ret.nval);
}

// native final string QS_GetStr (name fieldname, optional string defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetStr) {
  P_GET_STR_OPT(value, VStr::EmptyString);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Str) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.sval = value;
  }
  RET_STR(ret.sval);
}

// native final float QS_GetFloat (name fieldname, optional float defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetFloat) {
  P_GET_FLOAT_OPT(value, 0.0f);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Float) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.fval = value;
  }
  RET_FLOAT(ret.fval);
}
