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

IMPLEMENT_CLASS(V, Entity);

int VEntity::FIndex_OnMapSpawn;
int VEntity::FIndex_BeginPlay;
int VEntity::FIndex_Destroyed;
int VEntity::FIndex_Touch;
int VEntity::FIndex_BlastedHitLine;
int VEntity::FIndex_CheckForPushSpecial;
int VEntity::FIndex_ApplyFriction;
int VEntity::FIndex_HandleFloorclip;
int VEntity::FIndex_CrossSpecialLine;
int VEntity::FIndex_SectorChanged;
int VEntity::FIndex_RoughCheckThing;
int VEntity::FIndex_GiveInventory;
int VEntity::FIndex_TakeInventory;
int VEntity::FIndex_CheckInventory;
int VEntity::FIndex_GetSigilPieces;
int VEntity::FIndex_MoveThing;
int VEntity::FIndex_GetStateTime;


struct SavedVObjectPtr {
  VObject **ptr;
  VObject *saved;
  SavedVObjectPtr (VObject **aptr) : ptr(aptr), saved(*aptr) {}
  ~SavedVObjectPtr() { *ptr = saved; }
};


static VCvarB _decorate_dont_warn_about_invalid_labels("_decorate_dont_warn_about_invalid_labels", false, "Don't do this!", CVAR_Archive);


//==========================================================================
//
//  VEntity::InitFuncIndexes
//
//==========================================================================
void VEntity::InitFuncIndexes () {
  guard(VEntity::InitFuncIndexes);
  FIndex_OnMapSpawn = StaticClass()->GetMethodIndex(NAME_OnMapSpawn);
  FIndex_BeginPlay = StaticClass()->GetMethodIndex(NAME_BeginPlay);
  FIndex_Destroyed = StaticClass()->GetMethodIndex(NAME_Destroyed);
  FIndex_Touch = StaticClass()->GetMethodIndex(NAME_Touch);
  FIndex_BlastedHitLine = StaticClass()->GetMethodIndex(NAME_BlastedHitLine);
  FIndex_CheckForPushSpecial = StaticClass()->GetMethodIndex(NAME_CheckForPushSpecial);
  FIndex_ApplyFriction = StaticClass()->GetMethodIndex(NAME_ApplyFriction);
  FIndex_HandleFloorclip = StaticClass()->GetMethodIndex(NAME_HandleFloorclip);
  FIndex_CrossSpecialLine = StaticClass()->GetMethodIndex(NAME_CrossSpecialLine);
  FIndex_SectorChanged = StaticClass()->GetMethodIndex(NAME_SectorChanged);
  FIndex_RoughCheckThing = StaticClass()->GetMethodIndex(NAME_RoughCheckThing);
  FIndex_GiveInventory = StaticClass()->GetMethodIndex(NAME_GiveInventory);
  FIndex_TakeInventory = StaticClass()->GetMethodIndex(NAME_TakeInventory);
  FIndex_CheckInventory = StaticClass()->GetMethodIndex(NAME_CheckInventory);
  FIndex_GetSigilPieces = StaticClass()->GetMethodIndex(NAME_GetSigilPieces);
  FIndex_MoveThing = StaticClass()->GetMethodIndex(NAME_MoveThing);
  FIndex_GetStateTime = StaticClass()->GetMethodIndex(NAME_GetStateTime);
  unguard;
}


//==========================================================================
//
//  VEntity::Serialise
//
//==========================================================================
void VEntity::Serialise (VStream &Strm) {
  guard(VEntity::Serialise);
  Super::Serialise(Strm);
  vuint8 xver = 0;
  Strm << xver;
  if (Strm.IsLoading()) {
    if (EntityFlags&EF_IsPlayer) Player->MO = this;
    SubSector = nullptr; // must mark as not linked
    LinkToWorld();
  }
  unguard;
}


//==========================================================================
//
//  VEntity::DestroyThinker
//
//==========================================================================
void VEntity::DestroyThinker () {
  guard(VEntity::DestroyThinker)

  if (Role == ROLE_Authority) {
    eventDestroyed();
    if (TID) RemoveFromTIDList(); // remove from TID list
    // stop any playing sound
    StopSound(0);
  }

  // unlink from sector and block lists
  UnlinkFromWorld();
  if (XLevel) XLevel->DelSectorList();

  Super::DestroyThinker();
  unguard;
}


//==========================================================================
//
//  VEntity::AddedToLevel
//
//==========================================================================
void VEntity::AddedToLevel () {
  guard(VEntity::AddedToLevel);
  if (!XLevel->NextSoundOriginID) XLevel->NextSoundOriginID = 1;
  SoundOriginID = XLevel->NextSoundOriginID+(SNDORG_Entity<<24);
  XLevel->NextSoundOriginID = (XLevel->NextSoundOriginID+1)&0x00ffffff;
  unguard;
}


//==========================================================================
//
//  VEntity::SetTID
//
//==========================================================================
void VEntity::SetTID (int tid) {
  guard(VEntity::SetTID);
  RemoveFromTIDList();
  if (tid) InsertIntoTIDList(tid);
  unguard;
}


//==========================================================================
//
//  VEntity::InsertIntoTIDList
//
//==========================================================================
void VEntity::InsertIntoTIDList (int tid) {
  guard(VEntity::InsertIntoTIDList);
  check(TID == 0);
  TID = tid;
  int HashIndex = tid&(VLevelInfo::TID_HASH_SIZE-1);
  TIDHashPrev = nullptr;
  TIDHashNext = Level->TIDHash[HashIndex];
  if (TIDHashNext) TIDHashNext->TIDHashPrev = this;
  Level->TIDHash[HashIndex] = this;
  unguard;
}


//==========================================================================
//
//  VEntity::RemoveFromTIDList
//
//==========================================================================
void VEntity::RemoveFromTIDList () {
  guard(VEntity::RemoveFromTIDList);
  if (!TID) return; // no TID, which means it's not in the cache
  if (TIDHashNext) TIDHashNext->TIDHashPrev = TIDHashPrev;
  if (TIDHashPrev) {
    TIDHashPrev->TIDHashNext = TIDHashNext;
  } else {
    int HashIndex = TID&(VLevelInfo::TID_HASH_SIZE-1);
    check(Level->TIDHash[HashIndex] == this);
    Level->TIDHash[HashIndex] = TIDHashNext;
  }
  TID = 0;
  unguard;
}


//==========================================================================
//
//  VEntity::SetState
//
//  Returns true if the actor is still present.
//
//==========================================================================
bool VEntity::SetState (VState *InState) {
  guard(VEntity::SetState);
  VState *st = InState;
  int watchcatCount = 1024;
  //if (VStr::ICmp(GetClass()->GetName(), "Doomer") == 0) GCon->Logf("***(000): Doomer %p: state=%s (%s)", this, (st ? *st->GetFullName() : "<none>"), (st ? *st->Loc.toStringNoCol() : ""));
  if (GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) {
    if (developer) GCon->Logf(NAME_Dev, "   (00):%s: dead (0x%04x) before state actions, state is %s", *GetClass()->GetFullName(), GetFlags(), (st ? *st->Loc.toStringNoCol() : "<none>"));
    State = nullptr;
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    return false;
  }
  do {
    if (--watchcatCount <= 0) {
      //k8: FIXME!
      GCon->Logf("ERROR: WatchCat interrupted `VEntity::SetState`!");
      break;
    }

    if (!st) {
      // remove mobj
      DispSpriteFrame = 0;
      DispSpriteName = NAME_None;
      State = nullptr;
      StateTime = -1;
      DestroyThinker();
      return false;
    }

    // remember current sprite and frame
    UpdateDispFrameFrom(st);

    State = st;
    StateTime = eventGetStateTime(st, st->Time);
    EntityFlags &= ~EF_FullBright;

    // modified handling
    // call action functions when the state is set
    if (st->Function) {
      //if (VStr::ICmp(GetClass()->GetName(), "Doomer") == 0) GCon->Logf("   (011):%s: Doomer %p STATE ACTION: %p '%s'", *st->Loc.toStringNoCol(), this, st, st->Function->GetName());
      XLevel->CallingState = State;
      {
        SavedVObjectPtr svp(&_stateRouteSelf);
        _stateRouteSelf = nullptr;
        P_PASS_SELF;
        ExecuteFunctionNoArgs(st->Function);
        if (GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) {
          /*
          GCon->Logf(NAME_Dev, "   (01):%s: dead (0x%04x) after state action, state is %s (next is %s; State is %s)", *GetClass()->GetFullName(), GetFlags(), *st->Loc.toStringNoCol(),
            (st && st->Next ? *st->Next->Loc.toStringNoCol() : "<none>"), (State ? *State->Loc.toStringNoCol() : "<none>"));
          */
          State = nullptr; // just in case
        }
      }
    }

    if (!State) {
      //if (VStr::ICmp(GetClass()->GetName(), "Doomer") == 0) GCon->Logf("***(660): Doomer %p IS DEAD", this);
      DispSpriteFrame = 0;
      DispSpriteName = NAME_None;
      return false;
    }
    st = State->NextState;
  } while (!StateTime);
  return true;
  unguard;
}


//==========================================================================
//
//  VEntity::SetInitialState
//
//  Returns true if the actor is still present.
//
//==========================================================================
void VEntity::SetInitialState (VState *InState) {
  guard(VEntity::SetInitialState);
  State = InState;
  if (InState) {
    UpdateDispFrameFrom(InState);
    StateTime = eventGetStateTime(InState, InState->Time);
  } else {
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1.0f;
  }
  unguard;
}


//==========================================================================
//
//  VEntity::AdvanceState
//
//==========================================================================
bool VEntity::AdvanceState (float deltaTime) {
  guard(VEntity::AdvanceState);
  if (deltaTime <= 0.0f) return true;
  if (State && StateTime != -1.0f) {
    StateTime -= deltaTime;
    // you can cycle through multiple states in a tic
    if (StateTime <= 0.0f) {
      //if (!State->NextState && VStr::ICmp(GetClass()->GetName(), "Doomer") == 0) GCon->Logf("***(669): Doomer %p: EMPTY NEXT on state=%s (%s)", this, (State ? *State->GetFullName() : "<none>"), (State ? *State->Loc.toStringNoCol() : ""));
      if (!SetState(State->NextState)) return false; // freed itself
    }
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VEntity::FindState
//
//==========================================================================
VState *VEntity::FindState (VName StateName, VName SubLabel, bool Exact) {
  guard(VEntity::FindState);
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName, SubLabel, Exact);
  if (!Lbl && !Exact && SubLabel == NAME_None && StateName != NAME_None && strchr(*StateName, '.')) {
    // try to split, if not exact
    TArray<VName> Names;
    VMemberBase::StaticSplitStateLabel(*StateName, Names);
    //GCon->Logf("VEntity::FindState(%s): splitted '%s' to %d parts", GetClass()->GetName(), *StateName, Names.length());
    if (Names.length() > 1) Lbl = GetClass()->FindStateLabel(Names, true);
  }
  //if (Lbl) GCon->Logf("VEntity::FindState(%s): found '%s' (%s : %s)", GetClass()->GetName(), *StateName, *Lbl->Name, *Lbl->State->Loc.toStringNoCol());
  return (Lbl ? Lbl->State : nullptr);
  unguard;
}


//==========================================================================
//
//  VEntity::FindStateEx
//
//==========================================================================
VState *VEntity::FindStateEx (const VStr &StateName, bool Exact) {
  guard(VEntity::FindStateEx);
  TArray<VName> Names;
  VMemberBase::StaticSplitStateLabel(StateName, Names);
  VStateLabel *Lbl = GetClass()->FindStateLabel(Names, Exact);
  return (Lbl ? Lbl->State : nullptr);
  unguard;
}


//==========================================================================
//
//  VEntity::HasSpecialStates
//
//==========================================================================
bool VEntity::HasSpecialStates (VName StateName) {
  guard(VEntity::HasSpecialStates);
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName);
  return (Lbl != nullptr && Lbl->SubLabels.Num() > 0);
  unguard;
}


//==========================================================================
//
//  VEntity::GetStateEffects
//
//==========================================================================
void VEntity::GetStateEffects (TArray<VLightEffectDef *> &Lights, TArray<VParticleEffectDef *> &Part) const {
  guard(VEntity::GetStateEffects);
  // clear arrays
  Lights.reset();
  // check for valid state
  if (!State) return;
  // find all matching effects
  const int len = GetClass()->SpriteEffects.length();
  for (int i = 0; i < len; ++i) {
    VSpriteEffect &SprFx = GetClass()->SpriteEffects[i];
    if (SprFx.SpriteIndex != State->SpriteIndex) continue;
    if (SprFx.Frame != -1 && SprFx.Frame != (State->Frame&VState::FF_FRAMEMASK)) continue;
    if (SprFx.LightDef) Lights.Append(SprFx.LightDef);
    if (SprFx.PartDef) Part.Append(SprFx.PartDef);
  }
  if (!State->LightInited) {
    State->LightInited = true;
    State->LightDef = nullptr;
    if (State->LightName.length()) State->LightDef = R_FindLightEffect(State->LightName);
  }
  if (State->LightDef) Lights.Append(State->LightDef);
  unguard;
}


//==========================================================================
//
//  VEntity::CallStateChain
//
//==========================================================================
bool VEntity::CallStateChain (VEntity *Actor, VState *AState) {
  guard(VEntity::CallStateChain);
  // set up state call structure
  VStateCall *PrevCall = XLevel->StateCall;
  VStateCall Call;
  Call.Item = this;
  bool Ret = false;
  XLevel->StateCall = &Call;

  int RunAway = 0;
  VState *S = AState;
  while (S) {
    Call.State = S;
    // call action function
    if (S->Function) {
      // assume success by default
      XLevel->CallingState = S;
      Call.Result = true;
      P_PASS_REF(Actor);
      ExecuteFunctionNoArgs(S->Function);
      // at least one success means overal success
      if (Call.Result) Ret = true;
    }

    // check for infinite loops
    ++RunAway;
    if (RunAway > 1000) break;

    if (Call.State == S) {
      // abort immediately if next state loops to itself
      // in this case the overal result is always false
      if (S->NextState == S) {
        Ret = false;
        break;
      }
      // advance to the next state
      S = S->NextState;
    } else {
      // there was a state jump
      S = Call.State;
    }
  }

  XLevel->StateCall = PrevCall;
  return Ret;
  unguard;
}


//==========================================================================
//
//  VEntity::StartSound
//
//==========================================================================
void VEntity::StartSound (VName Sound, vint32 Channel, float Volume, float Attenuation, bool Loop, bool Local) {
  guard(VEntity::StartSound);
  if (!Sector) return;
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::StartSound(Origin, SoundOriginID,
    GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
    Channel, Volume, Attenuation, Loop, Local);
  unguard;
}


//==========================================================================
//
//  VEntity::StartLocalSound
//
//==========================================================================
void VEntity::StartLocalSound (VName Sound, vint32 Channel, float Volume, float Attenuation) {
  guard(VEntity::StartLocalSound);
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  if (Player) {
    Player->eventClientStartSound(
      GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
      TVec(0, 0, 0), /*0*/-666, Channel, Volume, Attenuation, false);
  }
  unguard;
}


//==========================================================================
//
//  VEntity::StopSound
//
//==========================================================================
void VEntity::StopSound (vint32 channel) {
  guard(VEntity::StopSound);
  Super::StopSound(SoundOriginID, channel);
  unguard;
}


//==========================================================================
//
//  VEntity::StartSoundSequence
//
//==========================================================================
void VEntity::StartSoundSequence (VName Name, vint32 ModeNum) {
  guard(VEntity::StartSoundSequence);
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::StartSoundSequence(Origin, SoundOriginID, Name, ModeNum);
  unguard;
}


//==========================================================================
//
//  VEntity::AddSoundSequenceChoice
//
//==========================================================================
void VEntity::AddSoundSequenceChoice (VName Choice) {
  guard(VEntity::AddSoundSequenceChoice);
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::AddSoundSequenceChoice(SoundOriginID, Choice);
  unguard;
}


//==========================================================================
//
//  VEntity::StopSoundSequence
//
//==========================================================================
void VEntity::StopSoundSequence () {
  guard(VEntity::StopSoundSequence);
  Super::StopSoundSequence(SoundOriginID);
  unguard;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
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

IMPLEMENT_FUNCTION(VEntity, CallStateChain) {
  P_GET_PTR(VState, State);
  P_GET_REF(VEntity, Actor);
  P_GET_SELF;
  RET_BOOL(Self->CallStateChain(Actor, State));
}

IMPLEMENT_FUNCTION(VEntity, PlaySound) {
  P_GET_BOOL_OPT(Local, false);
  P_GET_BOOL_OPT(Loop, false);
  P_GET_FLOAT_OPT(Attenuation, 1.0f);
  P_GET_FLOAT_OPT(Volume, 1.0f);
  P_GET_INT(Channel);
  P_GET_NAME(SoundName);
  P_GET_SELF;
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
