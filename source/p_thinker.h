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

enum {
  // no role assigned yet
  ROLE_None,
  // this is dumb proxy: it does no physics, no animation, and updates only
  // when the server sends an update.
  // this situation is only seen in network clients, never for network servers or SP games
  ROLE_DumbProxy,
  // this is simulated proxy: the client should simulate physics and perform animation, but
  // the server is still the authority.
  // this situation is only seen in network clients, never for network servers or SP games
  ROLE_SimulatedProxy,
  // this is local player (not implemented yet)
  // used to perform client-side predictions
  // this situation is only seen in network clients, never for network servers or SP games
  ROLE_AutonomousProxy,
  // this machine has absolute, authoritative control over the actor.
  // this is the case for all actors on a network server or in SP game.
  // on a client, this is the case for actors that were locally spawned by the client,
  // such as gratuitous special effects which are done client-side in order to reduce bandwidth usage.
  ROLE_Authority,
};

// type of the sound origin, used for origin IDs when playing sounds
enum {
  SNDORG_Entity,
  SNDORG_Sector,
  SNDORG_PolyObj,
};


// ////////////////////////////////////////////////////////////////////////// //
// doubly linked list of actors and other special elements of a level
class VThinker : public VGameObject {
  DECLARE_CLASS(VThinker, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VThinker)

  VLevel *XLevel; // level object
  VLevelInfo *Level; // level info object

  VThinker *Prev;
  VThinker *Next;

  float SpawnTime; // `Spawn()` function sets this to game time

  vuint8 Role;
  vuint8 RemoteRole;

  enum {
    TF_AlwaysRelevant   = 1u<<0,
    TF_NetInitial       = 1u<<1,
    TF_NetOwner         = 1u<<2,
    TF_ServerSideOnly   = 1u<<3, // never sent to client (but `AlwaysRelevant` has priority)
    // set this flag to detach the thinker from the server
    // i.e. the server will destroy it, and the client should perform ticking for it
    // the logic is like this: the server sets this flag on initial update, and
    // waits for `Role` to change.
    // when `Role` changes to `ROLE_Authority`, the client should take full control
    // over the thinker.
    //TF_DetachFromServer = 1u<<4,
  };
  vuint32 ThinkerFlags;

private:
  // must be called from `Spawn*` implementations
  static VThinker *SpawnCommon (bool allowNoneClass, bool checkKillEntityEx, bool hasDesiredClass);

public:
  //VThinker ();

  // VObject interface
  virtual void Destroy () override;
  virtual void SerialiseOther (VStream &) override;

  // VThinker interface
  virtual void Tick (float);
  virtual void DestroyThinker ();
  virtual void AddedToLevel ();
  virtual void RemovedFromLevel ();

  void StartSound (const TVec &, vint32, vint32, vint32, float, float, bool, bool=false);
  void StopSound (vint32, vint32);
  void StartSoundSequence (const TVec &, vint32, VName, vint32);
  void AddSoundSequenceChoice (vint32, VName);
  void StopSoundSequence (vint32);

  void BroadcastChatPrint (VStr nick, VStr text);
  void BroadcastPrint (const char *);
  void BroadcastCenterPrint (const char *);

  DECLARE_FUNCTION(SpawnThinker)
  DECLARE_FUNCTION(SpawnNoTypeCheck)
  DECLARE_FUNCTION(SpawnEntityChecked)
  DECLARE_FUNCTION(Spawn)
  DECLARE_FUNCTION(Destroy)

  // print functions
  DECLARE_FUNCTION(bprint)

  DECLARE_FUNCTION(AllocDlight)
  DECLARE_FUNCTION(ShiftDlightHeight)
  DECLARE_FUNCTION(NewParticle)
  DECLARE_FUNCTION(GetAmbientSound)

  // iterators
  DECLARE_FUNCTION(AllThinkers)
  DECLARE_FUNCTION(AllActivePlayers)
  DECLARE_FUNCTION(PathTraverse)
  DECLARE_FUNCTION(RadiusThings)

  void eventClientTick (float DeltaTime) {
    if (DeltaTime <= 0.0f) return;
    static VMethodProxy method("ClientTick");
    vobjPutParamSelf(DeltaTime);
    VMT_RET_VOID(method);
  }

  VThinker *eventGetNextAffector () { static VMethodProxy method("GetNextAffector"); vobjPutParamSelf(); VMT_RET_REF(VThinker, method); }
};


// ////////////////////////////////////////////////////////////////////////// //
template <class T> class TThinkerIterator {
private:
  VThinker *Th;

  inline void GetNext () noexcept {
    while (Th && (!Th->IsA(T::StaticClass()) || Th->IsGoingToDie())) Th = Th->Next;
  }

public:
  inline TThinkerIterator (const VLevel *Level) noexcept {
    Th = Level->ThinkerHead;
    GetNext();
  }

  inline operator bool () const noexcept { return Th != nullptr; }
  inline void operator ++ () noexcept {
    if (Th) {
      Th = Th->Next;
      GetNext();
    }
  }
  inline T *operator -> () noexcept { return (T *)Th; }
  inline T *operator * () noexcept { return (T *)Th; }
};
