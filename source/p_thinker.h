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

enum {
  ROLE_None,
  ROLE_DumbProxy,
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
    TF_AlwaysRelevant = 0x00000001,
    TF_NetInitial     = 0x00000002,
    TF_NetOwner       = 0x00000004,
  };
  vuint32 ThinkerFlags;

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

  void BroadcastPrint (const char *);
  void BroadcastPrintf (const char *, ...) __attribute__((format(printf,2,3)));
  void BroadcastCentrePrint (const char *);
  void BroadcastCentrePrintf (const char *, ...) __attribute__((format(printf,2,3)));

  DECLARE_FUNCTION(Spawn)
  DECLARE_FUNCTION(Destroy)

  // print functions
  DECLARE_FUNCTION(bprint)

  DECLARE_FUNCTION(AllocDlight)
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
};


// ////////////////////////////////////////////////////////////////////////// //
template <class T> class TThinkerIterator {
private:
  VThinker *Th;

  void GetNext () {
    while (Th && (!Th->IsA(T::StaticClass()) || (Th->GetFlags()&_OF_DelayedDestroy))) Th = Th->Next;
  }

public:
  TThinkerIterator (const VLevel *Level) {
    Th = Level->ThinkerHead;
    GetNext();
  }

  inline operator bool () const { return Th != nullptr; }
  inline void operator ++ () {
    if (Th) {
      Th = Th->Next;
      GetNext();
    }
  }
  inline T *operator -> () { return (T*)Th; }
  inline T *operator * () { return (T*)Th; }
};
