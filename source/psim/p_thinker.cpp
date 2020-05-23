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
//**
//**  THINKERS
//**
//**  All thinkers should be allocated by Z_Malloc so they can be operated
//**  on uniformly. The actual structures will vary in size, but the first
//**  element must be VThinker.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../net/network.h"
#include "../client/cl_local.h" // for dlight_t


IMPLEMENT_CLASS(V, Thinker)


//==========================================================================
//
//  VThinker::Destroy
//
//==========================================================================
void VThinker::Destroy () {
  // close any thinker channels
  if (XLevel && XLevel->NetContext) XLevel->NetContext->ThinkerDestroyed(this);
  Super::Destroy();
}


//==========================================================================
//
//  VThinker::SerialiseOther
//
//==========================================================================
void VThinker::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (Strm.IsLoading()) XLevel->AddThinker(this);
}


//==========================================================================
//
//  VThinker::Tick
//
//==========================================================================
void VThinker::Tick (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  static VMethodProxy method("Tick");
  vobjPutParamSelf(DeltaTime);
  VMT_RET_VOID(method);
}


//==========================================================================
//
//  VThinker::DestroyThinker
//
//==========================================================================
void VThinker::DestroyThinker () {
  SetDelayedDestroy();
}


//==========================================================================
//
//  VThinker::AddedToLevel
//
//==========================================================================
void VThinker::AddedToLevel () {
  if (XLevel) {
    if (XLevel->Renderer) XLevel->Renderer->ThinkerAdded(this);
  }
}


//==========================================================================
//
//  VThinker::RemovedFromLevel
//
//==========================================================================
void VThinker::RemovedFromLevel () {
  if (XLevel) {
    if (XLevel->Renderer) XLevel->Renderer->ThinkerDestroyed(this);
    if (XLevel->NetContext) XLevel->NetContext->ThinkerDestroyed(this);
  }
}


//==========================================================================
//
//  VThinker::StartSound
//
//==========================================================================
void VThinker::StartSound (const TVec &origin, vint32 origin_id,
                           vint32 sound_id, vint32 channel, float volume, float Attenuation,
                           bool Loop, bool Local)
{
  if (!Level || !Level->Game) return; //FIXME! for client-side entities (this should be fixed, client-side entities can emit sounds)
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStartSound(sound_id, origin, (Local ? -666 : origin_id), channel, volume, Attenuation, Loop);
  }
}


//==========================================================================
//
//  VThinker::StopSound
//
//==========================================================================
void VThinker::StopSound (vint32 origin_id, vint32 channel) {
  if (!Level || !Level->Game) return; //FIXME! for client-side entities (this should be fixed, client-side entities can emit sounds)
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStopSound(origin_id, channel);
  }
}


//==========================================================================
//
//  VThinker::StartSoundSequence
//
//==========================================================================
void VThinker::StartSoundSequence (const TVec &Origin, vint32 OriginId, VName Name, vint32 ModeNum) {
  if (!Level || !Level->Game) return; //FIXME! for client-side entities (this should be fixed, client-side entities can emit sounds)

  // remove any existing sequences of this origin
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == OriginId) {
      XLevel->ActiveSequences.RemoveIndex(i);
      --i;
    }
  }

  VSndSeqInfo &Seq = XLevel->ActiveSequences.Alloc();
  Seq.Name = Name;
  Seq.OriginId = OriginId;
  Seq.Origin = Origin;
  Seq.ModeNum = ModeNum;

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStartSequence(Origin, OriginId, Name, ModeNum);
  }
}


//==========================================================================
//
//  VThinker::AddSoundSequenceChoice
//
//==========================================================================
void VThinker::AddSoundSequenceChoice (int origin_id, VName Choice) {
  if (!Level || !Level->Game) return; //FIXME! for client-side entities (this should be fixed, client-side entities can emit sounds)

  // remove it from server's sequences list
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == origin_id) {
      XLevel->ActiveSequences[i].Choices.Append(Choice);
    }
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientAddSequenceChoice(origin_id, Choice);
  }
}


//==========================================================================
//
//  VThinker::StopSoundSequence
//
//==========================================================================
void VThinker::StopSoundSequence (int origin_id) {
  if (!Level || !Level->Game) return; //FIXME! for client-side entities (this should be fixed, client-side entities can emit sounds)

  // remove it from server's sequences list
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == origin_id) {
      XLevel->ActiveSequences.RemoveIndex(i);
      --i;
    }
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStopSequence(origin_id);
  }
}


//==========================================================================
//
//  VThinker::BroadcastPrint
//
//==========================================================================
void VThinker::BroadcastPrint (const char *s) {
  if (!Level || !Level->Game) return; // for client-side entities
  for (int i = 0; i < svs.max_clients; ++i) {
    if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientPrint(s);
  }
}


//==========================================================================
//
//  VThinker::BroadcastChatPrint
//
//==========================================================================
void VThinker::BroadcastChatPrint (VStr nick, VStr str) {
  if (!Level || !Level->Game) return; // for client-side entities
  for (int i = 0; i < svs.max_clients; ++i) {
    if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientChatPrint(nick, str);
  }
}


//==========================================================================
//
//  VThinker::BroadcastCenterPrint
//
//==========================================================================
void VThinker::BroadcastCenterPrint (const char *s) {
  if (!Level) return; // for client-side entities
  for (int i = 0; i < svs.max_clients; ++i) {
    if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientCenterPrint(s);
  }
}


//==========================================================================
//
//  Script iterators
//
//==========================================================================
class VScriptThinkerIterator : public VScriptIterator {
private:
  VThinker *Self;
  VClass *Class;
  VThinker **Out;
  VThinker *Current;

public:
  VScriptThinkerIterator (VThinker *ASelf, VClass *AClass, VThinker **AOut)
    : Self(ASelf)
    , Class(AClass)
    , Out(AOut)
    , Current(nullptr)
  {}

  virtual bool GetNext () override {
    if (!Current) {
      Current = Self->XLevel->ThinkerHead;
    } else {
      Current = Current->Next;
    }
    *Out = nullptr;
    while (Current) {
      if (Current->IsA(Class) && !Current->IsGoingToDie()) {
        *Out = Current;
        break;
      }
      Current = Current->Next;
    }
    return !!*Out;
  }
};


class VActivePlayersIterator : public VScriptIterator {
private:
  VThinker *Self;
  VBasePlayer **Out;
  int Index;

public:
  VActivePlayersIterator (VThinker *ASelf, VBasePlayer **AOut)
    : Self(ASelf)
    , Out(AOut)
    , Index(0)
  {}

  virtual bool GetNext () override {
    while (Index < MAXPLAYERS) {
      VBasePlayer *P = Self->Level->Game->Players[Index];
      ++Index;
      if (P && (P->PlayerFlags&VBasePlayer::PF_Spawned)) {
        *Out = P;
        return true;
      }
    }
    return false;
  }
};


//==========================================================================
//
//  Script natives
//
//==========================================================================
VThinker *VThinker::SpawnCommon (bool allowNoneClass, bool checkKillEntityEx, bool hasDesiredClass) {
  static VClass *eexCls = nullptr;
  if (checkKillEntityEx && !eexCls) {
    eexCls = VClass::FindClass("EntityEx");
    if (!eexCls) Sys_Error("cannot find class `EntityEx`");
  }

  VClass *desiredClass = nullptr;
  VClass *Class;
  VOptParamVec AOrigin(TVec(0, 0, 0));
  VOptParamAVec AAngles(TAVec(0, 0, 0));
  VOptParamPtr<mthing_t> mthing;
  VOptParamBool AllowReplace(true);
  vobjDeclareSelf;

  if (hasDesiredClass) {
    vobjGetParam(Self, desiredClass, Class, AOrigin, AAngles, mthing, AllowReplace);
  } else {
    vobjGetParam(Self, Class, AOrigin, AAngles, mthing, AllowReplace);
  }

  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty self in `Thinker::SpawnXXX()`"); }
  VEntity *SelfEnt = Cast<VEntity>(Self);
  // if spawner is entity, default to it's origin and angles
  if (SelfEnt) {
    if (!AOrigin.specified) AOrigin = SelfEnt->Origin;
    if (!AAngles.specified) AAngles = SelfEnt->Angles;
  }
  if (!Class) {
    if (!allowNoneClass) { VObject::VMDumpCallStack(); Sys_Error("Trying to spawn `None` class"); }
    return nullptr;
  }
  if (!Self->XLevel) { VObject::VMDumpCallStack(); Sys_Error("empty XLevel self in `Thinker::SpawnXXX()`"); }

  //if (checkKillEntityEx && !Class->IsChildOf(eexCls)) { VObject::VMDumpCallStack(); Sys_Error("trying to spawn non-EntityEx class `%s`", Class->GetName()); }

  VThinker *th = Self->XLevel->SpawnThinker(Class, AOrigin, AAngles, mthing, AllowReplace);

  // check it
  if (th && checkKillEntityEx && !th->IsA(eexCls)) {
    vassert(eexCls);
    GCon->Logf(NAME_Warning, "%s: tried to spawn class `%s`, got class `%s`, which is not `EntityEx` (this is mostly harmless)", Self->GetClass()->GetName(), Class->GetName(), th->GetClass()->GetName());
    th->DestroyThinker();
    th = nullptr;
  }

  if (th && desiredClass && !th->IsA(desiredClass)) {
    GCon->Logf(NAME_Warning, "%s: tried to spawn class `%s`, got class `%s`, which is not `%s`", Self->GetClass()->GetName(), Class->GetName(), th->GetClass()->GetName(), desiredClass->GetName());
    th->DestroyThinker();
    th = nullptr;
  }

  return th;
}


IMPLEMENT_FUNCTION(VThinker, SpawnThinker) {
  VThinker *th = SpawnCommon(/*allowNoneClass*/false, /*checkKillEntityEx*/false, /*hasDesiredClass*/false);
  RET_REF(th);
}

IMPLEMENT_FUNCTION(VThinker, SpawnNoTypeCheck) {
  VThinker *th = SpawnCommon(/*allowNoneClass*/true, /*checkKillEntityEx*/false, /*hasDesiredClass*/false);
  RET_REF(th);
}

IMPLEMENT_FUNCTION(VThinker, Spawn) {
  VThinker *th = SpawnCommon(/*allowNoneClass*/true, /*checkKillEntityEx*/true, /*hasDesiredClass*/false);
  RET_REF(th);
}

IMPLEMENT_FUNCTION(VThinker, SpawnEntityChecked) {
  VThinker *th = SpawnCommon(/*allowNoneClass*/true, /*checkKillEntityEx*/true, /*hasDesiredClass*/true);
  RET_REF(th);
}

IMPLEMENT_FUNCTION(VThinker, Destroy) {
  vobjGetParamSelf();
  Self->DestroyThinker();
}

IMPLEMENT_FUNCTION(VThinker, bprint) {
  VStr Msg = PF_FormatString();
  vobjGetParamSelf();
  Self->BroadcastPrint(*Msg);
}

// native final dlight_t *AllocDlight(Thinker Owner, TVec origin, /*optional*/ float radius, optional int lightid);
IMPLEMENT_FUNCTION(VThinker, AllocDlight) {
  VThinker *Owner;
  TVec lorg;
  float radius;
  VOptParamInt lightid(-1);
  vobjGetParamSelf(Owner, lorg, radius, lightid);
  if (radius < 0) radius = 0;
  if (!Self->XLevel || !Self->XLevel->Renderer) { RET_PTR(nullptr); return; } // for dedicated server
  RET_PTR(Self->XLevel->Renderer->AllocDlight(Owner, lorg, radius, lightid));
}

//native final bool ShiftDlightHeight (int lightid, float zdelta);
IMPLEMENT_FUNCTION(VThinker, ShiftDlightHeight) {
  int lightid;
  float zdelta;
  vobjGetParamSelf(lightid, zdelta);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VThinker::ShiftDlightOrigin"); }
  if (!Self->XLevel || !Self->XLevel->Renderer) { RET_BOOL(false); return; }
  dlight_t *dl = Self->XLevel->Renderer->FindDlightById(lightid);
  if (dl) {
    //GCon->Logf("fixing dlight with id %d, delta=%g", lightid, zdelta);
    dl->origin.z += zdelta;
    RET_BOOL(true);
  } else {
    RET_BOOL(false);
  }
}

IMPLEMENT_FUNCTION(VThinker, NewParticle) {
  TVec porg;
  vobjGetParamSelf(porg);
  if (GGameInfo->IsPaused() || !Self->XLevel || !Self->XLevel->Renderer) {
    RET_PTR(nullptr);
  } else {
    RET_PTR(Self->XLevel->Renderer->NewParticle(porg));
  }
}

IMPLEMENT_FUNCTION(VThinker, GetAmbientSound) {
  int Idx;
  vobjGetParam(Idx);
  RET_PTR(GSoundManager->GetAmbientSound(Idx));
}

IMPLEMENT_FUNCTION(VThinker, AllThinkers) {
  VClass *Class;
  VThinker **Thinker;
  vobjGetParamSelf(Class, Thinker);
  RET_PTR(new VScriptThinkerIterator(Self, Class, Thinker));
}

IMPLEMENT_FUNCTION(VThinker, AllActivePlayers) {
  VBasePlayer **Out;
  vobjGetParamSelf(Out);
  RET_PTR(new VActivePlayersIterator(Self, Out));
}

// native final iterator PathTraverse (out intercept_t *In, float x1, float y1, float x2, float y2, int flags);
IMPLEMENT_FUNCTION(VThinker, PathTraverse) {
  intercept_t **In;
  float x1, y1, x2, y2;
  int flags;
  vobjGetParamSelf(In, x1, y1, x2, y2, flags);
  RET_PTR(new VPathTraverse(Self, In, x1, y1, x2, y2, flags));
}

// native final iterator RadiusThings (out Entity Ent, TVec Org, float Radius);
IMPLEMENT_FUNCTION(VThinker, RadiusThings) {
  VEntity **EntPtr;
  TVec Org;
  float Radius;
  vobjGetParamSelf(EntPtr, Org, Radius);
  RET_PTR(new VRadiusThingsIterator(Self, EntPtr, Org, Radius));
}


//==========================================================================
//
//  Info_ThinkerCount
//
//==========================================================================
COMMAND(Info_ThinkerCount) {
  VBasePlayer *plr = GGameInfo->Players[0];
  if (!plr || !plr->Level || !plr->Level->XLevel) return;
  int count = 0;
  for (VThinker *th = plr->Level->XLevel->ThinkerHead; th; th = th->Next) {
    ++count;
  }
  GCon->Logf("%d thinkers on level", count);
}


//==========================================================================
//
//  classNameCompare
//
//==========================================================================
extern "C" {
  static int classNameCompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    VClass *a = *(VClass **)aa;
    VClass *b = *(VClass **)bb;
    return VStr::ICmp(a->GetName(), b->GetName());
  }
}


struct ThinkerListEntry {
  VClass *cls;
  int count;
};


//==========================================================================
//
//  classTLECompare
//
//==========================================================================
extern "C" {
  static int classTLECompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const ThinkerListEntry *a = (const ThinkerListEntry *)aa;
    const ThinkerListEntry *b = (const ThinkerListEntry *)bb;
    return (a->count-b->count);
  }
}


//==========================================================================
//
//  Info_ThinkerCountDetail
//
//==========================================================================
COMMAND(Info_ThinkerCountDetail) {
  VBasePlayer *plr = GGameInfo->Players[0];
  if (!plr || !plr->Level || !plr->Level->XLevel) return;
  // collect
  TMapNC<VClass *, int> thmap;
  int count = 0;
  int maxlen = 1;
  for (VThinker *th = plr->Level->XLevel->ThinkerHead; th; th = th->Next) {
    int nlen = VStr::length(th->GetClass()->GetName());
    if (maxlen < nlen) maxlen = nlen;
    VClass *tc = th->GetClass();
    auto tcp = thmap.find(tc);
    if (tcp) {
      ++(*tcp);
    } else {
      thmap.put(tc, 1);
    }
    ++count;
  }
  GCon->Logf("\034K=== %d thinkers on level ===", count);
  // sort
  if (Args.length() > 1 && Args[1].length() && Args[1][0] == 't') {
    TArray<ThinkerListEntry> list;
    for (auto it = thmap.first(); it; ++it) {
      ThinkerListEntry &e = list.alloc();
      e.cls = it.getKey();
      e.count = it.getValue();
    }
    timsort_r(list.ptr(), list.length(), sizeof(ThinkerListEntry), &classTLECompare, nullptr);
    // dump
    for (int f = 0; f < list.length(); ++f) {
      GCon->Logf("\034K%*s\034-: \034D%d", maxlen, list[f].cls->GetName(), list[f].count);
    }
  } else {
    TArray<VClass *> list;
    for (auto it = thmap.first(); it; ++it) list.append(it.getKey());
    timsort_r(list.ptr(), list.length(), sizeof(VClass *), &classNameCompare, nullptr);
    // dump
    for (int f = 0; f < list.length(); ++f) {
      auto tcp = thmap.find(list[f]);
      GCon->Logf("\034K%*s\034-: \034D%d", maxlen, list[f]->GetName(), *tcp);
    }
  }
}
