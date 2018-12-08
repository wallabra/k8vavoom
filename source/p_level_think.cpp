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
#include "render/r_local.h"

extern VCvarB r_decals_enabled;


//==========================================================================
//
//  VLevel::AddThinker
//
//==========================================================================
void VLevel::AddThinker (VThinker *Th) {
  guard(VLevel::AddThinker);
  Th->XLevel = this;
  Th->Level = LevelInfo;
  Th->Prev = ThinkerTail;
  Th->Next = nullptr;
  if (ThinkerTail) ThinkerTail->Next = Th; else ThinkerHead = Th;
  ThinkerTail = Th;
  // notify thinker that is was just added to a level
  Th->AddedToLevel();
  unguard;
}


//==========================================================================
//
//  VLevel::RemoveThinker
//
//==========================================================================
void VLevel::RemoveThinker (VThinker *Th) {
  guard(VLevel::RemoveThinker);
  // notify that thinker is being removed from level
  Th->RemovedFromLevel();
  if (Th == ThinkerHead) ThinkerHead = Th->Next; else Th->Prev->Next = Th->Next;
  if (Th == ThinkerTail) ThinkerTail = Th->Prev; else Th->Next->Prev = Th->Prev;
  unguard;
}


//==========================================================================
//
//  VLevel::DestroyAllThinkers
//
//==========================================================================
void VLevel::DestroyAllThinkers () {
  guard(VLevel::DestroyAllThinkers);
  VThinker *Th = ThinkerHead;
  while (Th) {
    VThinker *c = Th;
    Th = c->Next;
    if (!(c->GetFlags()&_OF_DelayedDestroy)) c->DestroyThinker();
  }
  Th = ThinkerHead;
  while (Th) {
    VThinker *Next = Th->Next;
    Th->ConditionalDestroy();
    Th = Next;
  }
  ThinkerHead = nullptr;
  ThinkerTail = nullptr;
  unguard;
}


//==========================================================================
//
//  VLevel::TickWorld
//
//==========================================================================
void VLevel::TickWorld (float DeltaTime) {
  guard(VLevel::TickWorld);

  eventBeforeWorldTick(DeltaTime);

  // run decal thinkers
  if (DeltaTime > 0 && r_decals_enabled) {
    decal_t *dc = decanimlist;
    while (dc) {
      bool removeIt = true;
      if (dc->animator) removeIt = !dc->animator->animate(dc, DeltaTime);
      decal_t *c = dc;
      dc = dc->nextanimated;
      if (removeIt) RemoveAnimatedDecal(c);
    }
  }

  // run thinkers
  VThinker *Th = ThinkerHead;
  while (Th) {
    VThinker *c = Th;
    Th = c->Next;
    if (!(c->GetFlags()&_OF_DelayedDestroy)) {
      c->Tick(DeltaTime);
    }
    if (c->GetFlags()&_OF_DelayedDestroy) {
      RemoveThinker(c);
      // if it is just destroyed, call level notifier
      if (!(c->GetFlags()&_OF_Destroyed) && c->GetClass()->IsChildOf(VEntity::StaticClass())) eventEntityDying((VEntity *)c);
      c->ConditionalDestroy();
    }
  }

  // don't update specials if the level is frozen
  if (!(LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) LevelInfo->eventUpdateSpecials();

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (LevelInfo->Game->Players[i] && (LevelInfo->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned) != 0) {
      LevelInfo->Game->Players[i]->eventSetViewPos();
    }
  }

  //GCon->Logf("VLevel::TickWorld: time=%f; tictime=%f; dt=%f : %f", (double)Time, (double)TicTime, DeltaTime, DeltaTime*1000.0);
  Time += DeltaTime;
  //++TicTime;
  TicTime = (int)(Time*35.0);

  eventAfterWorldTick(DeltaTime);

  /*
  {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("AfterWorldTick"));
    P_PASS_SELF;
    P_PASS_FLOAT(DeltaTime);
    //VStr s = ExecuteFunction(GetVFunctionIdx(mtindex)).getStr();
    //GCon->Logf("STR: %s", *s.quote(true));
    //VName n = ExecuteFunction(GetVFunctionIdx(mtindex)).getName();
    //GCon->Logf("NAME: <%s>", *n);
    TVec v = ExecuteFunction(GetVFunctionIdx(mtindex)).getVector();
    GCon->Logf("VECTOR: (%f,%f,%f)", v.x, v.y, v.z);
  }
  */

  unguard;
}


//==========================================================================
//
//  VLevel::SpawnThinker
//
//==========================================================================
VThinker *VLevel::SpawnThinker (VClass *AClass, const TVec &AOrigin,
                                const TAVec &AAngles, mthing_t *mthing, bool AllowReplace)
{
  guard(VLevel::SpawnThinker);
  check(AClass);
  VClass *Class = (AllowReplace ? AClass->GetReplacement() : AClass);
  VThinker *Ret = (VThinker *)StaticSpawnObject(Class);
  AddThinker(Ret);

  if (IsForServer() && Class->IsChildOf(VEntity::StaticClass())) {
    VEntity *e = (VEntity *)Ret;
    e->Origin = AOrigin;
    e->Angles = AAngles;
    e->eventOnMapSpawn(mthing);
    // call it anyway, some script code may rely on this
    /*if (!(e->GetFlags()&(_OF_Destroyed|_OF_Destroyed)))*/ {
      if (LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_BegunPlay) e->eventBeginPlay();
    }
  }

  if (IsForServer() && Class->IsChildOf(VEntity::StaticClass())) {
    VEntity *e = (VEntity *)Ret;
    if (!(e->GetFlags()&(_OF_Destroyed|_OF_Destroyed))) eventEntitySpawned(e);
  }

  return Ret;
  unguard;
}
