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
#include "render/r_local.h"

extern VCvarB r_decals_enabled;

VCvarB dbg_world_think_vm_time("dbg_world_think_vm_time", false, "Show time taken by VM thinkers (for debug)?", CVAR_Archive);
VCvarB dbg_world_think_decal_time("dbg_world_think_decal_time", false, "Show time taken by decal thinkers (for debug)?", CVAR_Archive);
VCvarB dbg_vm_disable_thinkers("dbg_vm_disable_thinkers", false, "Disable VM thinkers (for debug)?", CVAR_PreInit);
VCvarB dbg_vm_disable_specials("dbg_vm_disable_specials", false, "Disable updating specials (for debug)?", CVAR_PreInit);

double worldThinkTimeVM = -1;
double worldThinkTimeDecal = -1;


//==========================================================================
//
//  VLevel::AddScriptThinker
//
//  won't call `Destroy()`, won't call `delete`
//
//==========================================================================
void VLevel::RemoveScriptThinker (VLevelScriptThinker *sth) {
  if (!sth) return;
  const int sclenOrig = scriptThinkers.length();
  for (int scidx = sclenOrig-1; scidx >= 0; --scidx) {
    if (scriptThinkers[scidx] == sth) {
      // remove it
      for (int c = scidx+1; c < sclenOrig; ++c) scriptThinkers[c-1] = scriptThinkers[c];
      scriptThinkers.setLength(scidx, false); // don't resize
      return;
    }
  }
}


//==========================================================================
//
//  VLevel::AddScriptThinker
//
//==========================================================================
void VLevel::AddScriptThinker (VLevelScriptThinker *sth, bool ImmediateRun) {
  if (!sth) return;
  check(!sth->XLevel);
  check(!sth->Level);
  sth->XLevel = this;
  sth->Level = LevelInfo;
  if (ImmediateRun) return;
#if 0
  {
    // cleanup script thinkers
    int firstEmpty = -1;
    const int sclenOrig = scriptThinkers.length();
    for (int scidx = 0; scidx < sclenOrig; ++scidx) {
      VLevelScriptThinker *scthr = scriptThinkers[scidx];
      if (!scthr) {
        if (firstEmpty < 0) firstEmpty = scidx;
        continue;
      }
      if (scthr->destroyed) {
        GCon->Logf("(0)DEAD ACS at slot #%d", scidx);
        if (firstEmpty < 0) firstEmpty = scidx;
        delete scthr;
        scriptThinkers[scidx] = nullptr;
        continue;
      }
    }
    // remove dead thinkers
    if (firstEmpty >= 0) {
      const int sclen = scriptThinkers.length();
      int currIdx = firstEmpty+1;
      while (currIdx < sclen) {
        VLevelScriptThinker *scthr = scriptThinkers[currIdx];
        if (scthr) {
          // alive
          check(firstEmpty < currIdx);
          scriptThinkers[firstEmpty] = scthr;
          scriptThinkers[currIdx] = nullptr;
          // find next empty slot
          ++firstEmpty;
          while (firstEmpty < sclen && scriptThinkers[firstEmpty]) ++firstEmpty;
        } else {
          // dead, do nothing
        }
        ++currIdx;
      }
      GCon->Logf("  SHRINKING ACS from %d to %d", sclen, firstEmpty);
      scriptThinkers.setLength(firstEmpty, false); // don't resize
    }
  }
#endif
  scriptThinkers.append(sth);
  //GCon->Logf("*** ADDED ACS: %s (%d)", *sth->DebugDumpToString(), scriptThinkers.length());
  if (scriptThinkers.length() > 16384) Host_Error("too many ACS thinkers spawned");
}


//==========================================================================
//
//  VLevel::SuspendNamedScriptThinkers
//
//==========================================================================
void VLevel::SuspendNamedScriptThinkers (const VStr &name, int map) {
  if (name.length() == 0) return;
  const int sclenOrig = scriptThinkers.length();
  VLevelScriptThinker **sth = scriptThinkers.ptr();
  for (int count = sclenOrig; count--; ++sth) {
    if (!sth[0] || sth[0]->destroyed) continue;
    if (name.ICmp(*sth[0]->GetName()) == 0) {
      //Acs->Suspend(sth[0]->GetNumber(), map);
      AcsSuspendScript(Acs, sth[0]->GetNumber(), map);
    }
  }
}


//==========================================================================
//
//  VLevel::TerminateNamedScriptThinkers
//
//==========================================================================
void VLevel::TerminateNamedScriptThinkers (const VStr &name, int map) {
  if (name.length() == 0) return;
  const int sclenOrig = scriptThinkers.length();
  VLevelScriptThinker **sth = scriptThinkers.ptr();
  for (int count = sclenOrig; count--; ++sth) {
    if (!sth[0] || sth[0]->destroyed) continue;
    if (name.ICmp(*sth[0]->GetName()) == 0) {
      //Acs->Terminate(sth[0]->GetNumber(), map);
      AcsTerminateScript(Acs, sth[0]->GetNumber(), map);
    }
  }
}


//==========================================================================
//
//  VLevel::AddThinker
//
//==========================================================================
void VLevel::AddThinker (VThinker *Th) {
  Th->XLevel = this;
  Th->Level = LevelInfo;
  Th->Prev = ThinkerTail;
  Th->Next = nullptr;
  if (ThinkerTail) ThinkerTail->Next = Th; else ThinkerHead = Th;
  ThinkerTail = Th;
  // notify thinker that is was just added to a level
  Th->AddedToLevel();
}


//==========================================================================
//
//  VLevel::RemoveThinker
//
//==========================================================================
void VLevel::RemoveThinker (VThinker *Th) {
  if (Th) {
    // notify that thinker is being removed from level
    Th->RemovedFromLevel();
    if (Th == ThinkerHead) ThinkerHead = Th->Next; else Th->Prev->Next = Th->Next;
    if (Th == ThinkerTail) ThinkerTail = Th->Prev; else Th->Next->Prev = Th->Prev;
  }
}


//==========================================================================
//
//  VLevel::DestroyAllThinkers
//
//==========================================================================
void VLevel::DestroyAllThinkers () {
  // destroy scripts
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) if (scriptThinkers[scidx]) scriptThinkers[scidx]->Destroy();
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) delete scriptThinkers[scidx];
  scriptThinkers.clear();

  // destroy VC thinkers
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
}


//==========================================================================
//
//  VLevel::RunScriptThinkers
//
//==========================================================================
void VLevel::RunScriptThinkers (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  // run script thinkers
  // do not run newly spawned scripts on this frame, though
  //const int sclenOrig = scriptThinkers.length();
  int firstEmpty = -1;
  // don't cache number of thinkers, as scripts may add new thinkers
  for (int scidx = 0; scidx < scriptThinkers.length()/*sclenOrig*/; ++scidx) {
    VLevelScriptThinker *sth = scriptThinkers[scidx];
    if (!sth) {
      if (firstEmpty < 0) firstEmpty = scidx;
      continue;
    }
    if (sth->destroyed) {
      //GCon->Logf("(0)DEAD ACS at slot #%d", scidx);
      if (firstEmpty < 0) firstEmpty = scidx;
      delete sth;
      scriptThinkers[scidx] = nullptr;
      continue;
    }
    //GCon->Logf("ACS #%d RUNNING: %s", scidx, *sth->DebugDumpToString());
    sth->Tick(DeltaTime);
    if (sth->destroyed) {
      //GCon->Logf("(1)DEAD ACS at slot #%d", scidx);
      if (firstEmpty < 0) firstEmpty = scidx;
      delete sth;
      scriptThinkers[scidx] = nullptr;
      continue;
    }
  }
  // remove dead thinkers
  if (firstEmpty >= 0) {
    const int sclen = scriptThinkers.length();
    int currIdx = firstEmpty+1;
    while (currIdx < sclen) {
      VLevelScriptThinker *sth = scriptThinkers[currIdx];
      if (sth) {
        // alive
        check(firstEmpty < currIdx);
        scriptThinkers[firstEmpty] = sth;
        scriptThinkers[currIdx] = nullptr;
        // find next empty slot
        ++firstEmpty;
        while (firstEmpty < sclen && scriptThinkers[firstEmpty]) ++firstEmpty;
      } else {
        // dead, do nothing
      }
      ++currIdx;
    }
    //GCon->Logf("  SHRINKING ACS from %d to %d", sclen, firstEmpty);
    scriptThinkers.setLength(firstEmpty, false); // don't resize
  }
}


//==========================================================================
//
//  VLevel::TickWorld
//
//==========================================================================
void VLevel::TickWorld (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;

  double stimed = 0, stimet = 0;

  eventBeforeWorldTick(DeltaTime);

  if (dbg_world_think_decal_time) stimed = -Sys_Time();
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
  worldThinkTimeDecal = (dbg_world_think_decal_time ? Sys_Time()+stimed : -1);

  if (dbg_world_think_vm_time) stimet = -Sys_Time();

  // run thinkers
#ifdef CLIENT
  // first run player mobile thinker, so we'll get camera position
  VThinker *plrmo = (cl ? cl->MO : nullptr);
  if (plrmo) {
    if (plrmo->GetFlags()&_OF_DelayedDestroy) {
      plrmo = nullptr;
    } else {
      plrmo->Tick(DeltaTime);
    }
  }
#else
  // server: run player thinkers when vm thinkers are disabled
  if (dbg_vm_disable_thinkers) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;
      if (!(Player->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
      VThinker *plrmo = Player->MO;
      if (plrmo && !(plrmo->GetFlags()&_OF_DelayedDestroy)) {
        plrmo->Tick(DeltaTime);
      }
    }
  }
#endif

  VThinker *Th = ThinkerHead;
  if (!dbg_vm_disable_thinkers) {
    while (Th) {
      VThinker *c = Th;
      Th = c->Next;
#ifdef CLIENT
      if (c != plrmo)
#endif
      {
        if (!(c->GetFlags()&_OF_DelayedDestroy)) c->Tick(DeltaTime);
      }
      if (c->GetFlags()&_OF_DelayedDestroy) {
        RemoveThinker(c);
        // if it is just destroyed, call level notifier
        if (!(c->GetFlags()&_OF_Destroyed) && c->GetClass()->IsChildOf(VEntity::StaticClass())) eventEntityDying((VEntity *)c);
        c->ConditionalDestroy();
      }
    }
  }

  worldThinkTimeVM = (dbg_world_think_vm_time ? Sys_Time()+stimet : -1);

  RunScriptThinkers(DeltaTime);


  if (!dbg_vm_disable_specials) {
    // don't update specials if the level is frozen
    if (!(LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) LevelInfo->eventUpdateSpecials();
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (LevelInfo->Game->Players[i] && (LevelInfo->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned) != 0) {
      LevelInfo->Game->Players[i]->eventSetViewPos();
    }
  }

  //GCon->Logf("VLevel::TickWorld: time=%f; tictime=%f; dt=%f : %f", (double)Time, (double)TicTime, DeltaTime, DeltaTime*1000.0);
  Time += DeltaTime;
  //++TicTime;
  TicTime = (int)(Time*35.0f);

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
}


//==========================================================================
//
//  VLevel::SpawnThinker
//
//==========================================================================
VThinker *VLevel::SpawnThinker (VClass *AClass, const TVec &AOrigin,
                                const TAVec &AAngles, mthing_t *mthing, bool AllowReplace)
{
  check(AClass);
  VClass *Class = (AllowReplace ? AClass->GetReplacement() : AClass);
  if (!Class) Class = AClass;
  VThinker *Ret = (VThinker *)StaticSpawnNoReplace(Class);
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
}


//==========================================================================
//
//  Script iterators
//
//==========================================================================
class VScriptThinkerLevelIterator : public VScriptIterator {
private:
  VLevel *Self;
  VClass *Class;
  VThinker **Out;
  VThinker *Current;

public:
  VScriptThinkerLevelIterator (VLevel *ASelf, VClass *AClass, VThinker **AOut)
    : Self(ASelf)
    , Class(AClass)
    , Out(AOut)
    , Current(nullptr)
  {}

  virtual bool GetNext () override {
    if (!Current) {
      Current = Self->ThinkerHead;
    } else {
      Current = Current->Next;
    }
    *Out = nullptr;
    while (Current) {
      if (Current->IsA(Class) && !(Current->GetFlags()&_OF_DelayedDestroy)) {
        *Out = Current;
        break;
      }
      Current = Current->Next;
    }
    return !!*Out;
  }
};


class VActivePlayersLevelIterator : public VScriptIterator {
private:
  VLevel *Self;
  VBasePlayer **Out;
  int Index;

public:
  VActivePlayersLevelIterator (VBasePlayer **AOut) : Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < MAXPLAYERS && GGameInfo) {
      VBasePlayer *P = GGameInfo->Players[Index];
      ++Index;
      if (P && (P->PlayerFlags&VBasePlayer::PF_Spawned)) {
        *Out = P;
        return true;
      }
    }
    return false;
  }
};


IMPLEMENT_FUNCTION(VLevel, AllThinkers) {
  P_GET_PTR(VThinker *, Thinker);
  P_GET_PTR(VClass, Class);
  P_GET_SELF;
  RET_PTR(new VScriptThinkerLevelIterator(Self, Class, Thinker));
}


IMPLEMENT_FUNCTION(VLevel, AllActivePlayers) {
  P_GET_PTR(VBasePlayer *, Out);
  RET_PTR(new VActivePlayersLevelIterator(Out));
}
