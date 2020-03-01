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
#include "gamedefs.h"
#include "network.h"

static VCvarI net_dbg_dump_thinker_channels("net_dbg_dump_thinker_channels", "0", "Dump thinker channels creation/closing (bit 0)?");
static VCvarB net_dbg_dump_thinker_detach("net_dbg_dump_thinker_detach", false, "Dump thinker detaches?");


//==========================================================================
//
//  VThinkerChannel::VThinkerChannel
//
//==========================================================================
VThinkerChannel::VThinkerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Thinker, AIndex, AOpenedLocally)
  , Thinker(nullptr)
  , ThinkerClass(nullptr)
  , OldData(nullptr)
  , NewObj(false)
  , UpdatedThisFrame(false)
  , FieldCondValues(nullptr)
{
  // it is ok to close it... or it isn't?
  //bAllowPrematureClose = true;
}


//==========================================================================
//
//  VThinkerChannel::~VThinkerChannel
//
//==========================================================================
VThinkerChannel::~VThinkerChannel () {
  // mark channel as closing to prevent sending a message
  Closing = true;
  // if this is a client version of entity, destroy it
  RemoveThinkerFromGame();
}


//==========================================================================
//
//  VThinkerChannel::RemoveThinkerFromGame
//
//==========================================================================
void VThinkerChannel::RemoveThinkerFromGame () {
  // if this is a client version of entity, destroy it
  if (!Thinker) return;
  bool doRemove = !OpenedLocally;
  if (doRemove) {
    // for clients: don't remove "authority" thinkers, they became autonomous
    if (Connection->Context->IsClient()) {
      if (Thinker->RemoteRole == ROLE_DumbProxy) {
        doRemove = false;
        if (net_dbg_dump_thinker_detach) GCon->Logf(NAME_Debug, "VThinkerChannel::RemoveThinkerFromGame: skipping autonomous thinker '%s':%u", Thinker->GetClass()->GetName(), Thinker->GetUniqueId());
      }
    }
  }
  if (doRemove) {
    if (net_dbg_dump_thinker_channels.asInt()&1) GCon->Logf(NAME_Debug, "VThinkerChannel #%d: removing thinker '%s':%u from game...", Index, Thinker->GetClass()->GetName(), Thinker->GetUniqueId());
    // avoid loops
    VThinker *th = Thinker;
    Thinker = nullptr;
    th->DestroyThinker();
    //k8: oooh; hacks upon hacks, and more hacks... one of those methods can call `SetThinker(nullptr)` for us
    if (th && th->XLevel) {
      th->XLevel->RemoveThinker(th);
    }
    if (th) {
      th->ConditionalDestroy();
    }
    // restore it
    Thinker = th;
  }
  SetThinker(nullptr);
}


//==========================================================================
//
//  VThinkerChannel::ReceivedClosingAck
//
//  some channels may want to set some flags here
//
//  WARNING! don't close/suicide here!
//
//==========================================================================
void VThinkerChannel::ReceivedClosingAck () {
  RemoveThinkerFromGame();
}


//==========================================================================
//
//  VThinkerChannel::Suicide
//
//==========================================================================
void VThinkerChannel::Suicide () {
  Closing = true;
  // if this is a client version of entity, destroy it
  RemoveThinkerFromGame();
  VChannel::Suicide();
}


//==========================================================================
//
//  VThinkerChannel::Close
//
//==========================================================================
void VThinkerChannel::Close () {
  if (!Closing) {
    // don't suicide here, we still need to wait for ack
    VChannel::Close();
    // if this is a client version of entity, destroy it
    RemoveThinkerFromGame();
  }
}


//==========================================================================
//
//  VThinkerChannel::SetThinker
//
//==========================================================================
void VThinkerChannel::SetThinker (VThinker *AThinker) {
  if (Thinker && !AThinker && net_dbg_dump_thinker_channels.asInt()&1) GCon->Logf(NAME_Debug, "VThinkerChannel #%d: clearing thinker '%s':%u", Index, Thinker->GetClass()->GetName(), Thinker->GetUniqueId());
  if (!Thinker && AThinker && net_dbg_dump_thinker_channels.asInt()&1) GCon->Logf(NAME_Debug, "VThinkerChannel #%d: setting thinker '%s':%u", Index, AThinker->GetClass()->GetName(), AThinker->GetUniqueId());
  if (Thinker && AThinker && net_dbg_dump_thinker_channels.asInt()&1) GCon->Logf(NAME_Debug, "VThinkerChannel #%d: replacing thinker '%s':%u with '%s':%u", Index, Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), AThinker->GetClass()->GetName(), AThinker->GetUniqueId());

  if (Thinker) {
    Connection->ThinkerChannels.Remove(Thinker);
    if (OldData) {
      for (VField *F = ThinkerClass->NetFields; F; F = F->NextNetField) {
        VField::DestructField(OldData+F->Ofs, F->Type);
      }
      delete[] OldData;
      OldData = nullptr;
    }
    if (FieldCondValues) {
      delete[] FieldCondValues;
      FieldCondValues = nullptr;
    }
  }

  Thinker = AThinker;

  if (Thinker) {
    ThinkerClass = Thinker->GetClass();
    if (OpenedLocally) {
      VThinker *Def = (VThinker *)ThinkerClass->Defaults;
      OldData = new vuint8[ThinkerClass->ClassSize];
      memset(OldData, 0, ThinkerClass->ClassSize);
      for (VField *F = ThinkerClass->NetFields; F; F = F->NextNetField) {
        VField::CopyFieldValue((vuint8 *)Def+F->Ofs, OldData+F->Ofs, F->Type);
      }
      FieldCondValues = new vuint8[ThinkerClass->NumNetFields];
    }
    NewObj = true;
    Connection->ThinkerChannels.Set(Thinker, this);
  }
}


//==========================================================================
//
//  VThinkerChannel::EvalCondValues
//
//==========================================================================
void VThinkerChannel::EvalCondValues (VObject *Obj, VClass *Class, vuint8 *Values) {
  if (Class->GetSuperClass()) EvalCondValues(Obj, Class->GetSuperClass(), Values);
  for (int i = 0; i < Class->RepInfos.Num(); ++i) {
    bool Val = VObject::ExecuteFunctionNoArgs(Obj, Class->RepInfos[i].Cond, true).getBool(); // allow VMT lookups
    for (int j = 0; j < Class->RepInfos[i].RepFields.Num(); ++j) {
      if (Class->RepInfos[i].RepFields[j].Member->MemberType != MEMBER_Field) continue;
      Values[((VField *)Class->RepInfos[i].RepFields[j].Member)->NetIndex] = Val;
    }
  }
}


//==========================================================================
//
//  VThinkerChannel::Update
//
//==========================================================================
void VThinkerChannel::Update () {
  if (Closing || !Thinker) return;

  VEntity *Ent = Cast<VEntity>(Thinker);

  // set up thinker flags that can be used by field condition
  if (NewObj) Thinker->ThinkerFlags |= VThinker::TF_NetInitial;
  if (Ent != nullptr && Ent->GetTopOwner() == Connection->Owner->MO) Thinker->ThinkerFlags |= VThinker::TF_NetOwner;

  EvalCondValues(Thinker, Thinker->GetClass(), FieldCondValues);
  vuint8 *Data = (vuint8 *)Thinker;
  VObject *NullObj = nullptr;

  VMessageOut Msg(this, true/*reliable*/);

  if (NewObj) {
    Msg.bOpen = true;
    VClass *TmpClass = Thinker->GetClass();
    Connection->ObjMap->SerialiseClass(Msg, TmpClass);
    NewObj = false;
  }

  const bool isServer = Connection->Context->IsServer();
  vuint8 oldRole = 0, oldRemoteRole = 0;

  if (isServer && !Connection->AutoAck) {
    if (Ent && Ent->FlagsEx&VEntity::EFEX_NoTickGrav) {
      // this is Role on the client
      oldRole = Thinker->Role;
      oldRemoteRole = Thinker->RemoteRole;
      Thinker->RemoteRole = ROLE_Authority;
      // this is RemoteRole on the client (and Role on the server)
      Thinker->Role = ROLE_DumbProxy;
      if (net_dbg_dump_thinker_detach) GCon->Logf(NAME_DevNet, "%s:%u: became notick, closing channel%s", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), (OpenedLocally ? " (opened locally)" : ""));
    }
  }

  TAVec SavedAngles;
  if (Ent) {
    SavedAngles = Ent->Angles;
    if (Ent->EntityFlags&VEntity::EF_IsPlayer) {
      // clear look angles, because they must not affect model orientation
      Ent->Angles.pitch = 0;
      Ent->Angles.roll = 0;
    }
  } else {
    // shut up compiler warnings
    SavedAngles.yaw = 0;
    SavedAngles.pitch = 0;
    SavedAngles.roll = 0;
  }

  for (VField *F = Thinker->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!FieldCondValues[F->NetIndex]) continue;

    // set up pointer to the value and do swapping for the role fields
    vuint8 *FieldValue = Data+F->Ofs;
         if (F == Connection->Context->RoleField) FieldValue = Data+Connection->Context->RemoteRoleField->Ofs;
    else if (F == Connection->Context->RemoteRoleField) FieldValue = Data+Connection->Context->RoleField->Ofs;

    if (VField::IdenticalValue(FieldValue, OldData+F->Ofs, F->Type)) {
      //GCon->Logf(NAME_DevNet, "%s:%u: skipped field #%d (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, F->GetName(), *F->Type.GetName());
      continue;
    }
    if (F->Type.Type == TYPE_Array) {
      VFieldType IntType = F->Type;
      IntType.Type = F->Type.ArrayInnerType;
      int InnerSize = IntType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        vuint8 *Val = FieldValue+i*InnerSize;
        vuint8 *OldVal = OldData+F->Ofs+i*InnerSize;
        if (VField::IdenticalValue(Val, OldVal, IntType)) {
          //GCon->Logf(NAME_DevNet, "%s:%u: skipped array field #%d [%d] (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, i, F->GetName(), *IntType.GetName());
          continue;
        }

        // if it's an object reference that cannot be serialised, send it as nullptr reference
        if (IntType.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)Val)) {
          if (!*(VObject **)OldVal) continue; // already sent as nullptr
          Val = (vuint8 *)&NullObj;
        }

        Msg.WriteInt(F->NetIndex/*, Thinker->GetClass()->NumNetFields*/);
        Msg.WriteInt(i/*, F->Type.GetArrayDim()*/);
        if (VField::NetSerialiseValue(Msg, Connection->ObjMap, Val, IntType)) {
          //GCon->Logf(NAME_DevNet, "%s:%u: sent array field #%d [%d] (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, i, F->GetName(), *IntType.GetName());
          VField::CopyFieldValue(Val, OldVal, IntType);
        }
      }
    } else {
      // if it's an object reference that cannot be serialised, send it as nullptr reference
      if (F->Type.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)FieldValue)) {
        if (!*(VObject **)(OldData+F->Ofs)) continue; // already sent as nullptr
        FieldValue = (vuint8 *)&NullObj;
      }

      Msg.WriteInt(F->NetIndex/*, Thinker->GetClass()->NumNetFields*/);
      if (VField::NetSerialiseValue(Msg, Connection->ObjMap, FieldValue, F->Type)) {
        //GCon->Logf(NAME_DevNet, "%s:%u: sent field #%d (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, F->GetName(), *F->Type.GetName());
        VField::CopyFieldValue(FieldValue, OldData+F->Ofs, F->Type);
      }
    }
  }

  if (Ent && (Ent->EntityFlags&VEntity::EF_IsPlayer)) Ent->Angles = SavedAngles;
  UpdatedThisFrame = true;

  // if this object becomes "dumb proxy", transmit some additional data
  if (isServer && Thinker->Role == ROLE_DumbProxy && Ent) {
    Msg.WriteInt(-1); // "special data" flag
    vuint8 exFlags = 0;
    if (Ent->FlagsEx&VEntity::EFEX_NoInteraction) exFlags |= 1u<<0;
    if (Ent->FlagsEx&VEntity::EFEX_NoTickGrav) exFlags |= 1u<<1;
    if (Ent->FlagsEx&VEntity::EFEX_NoTickGravLT) exFlags |= 1u<<2;
    if (Ent->EntityFlags&VEntity::EF_NoGravity) exFlags |= 1u<<3;
    Msg.SerialiseBits(&exFlags, 3);
    // lifetime fields
    if (exFlags&(1u<<2)) {
      // special fields for "notick"
      Msg << Ent->LastMoveTime;
      Msg << Ent->PlaneAlpha;
    }
  }

  if (Msg.GetNumBits()) SendMessage(&Msg);

  // clear temporary networking flags
  Thinker->ThinkerFlags &= ~VThinker::TF_NetInitial;
  Thinker->ThinkerFlags &= ~VThinker::TF_NetOwner;

  // if this object becomes "dumb proxy", mark it as detached, and close the channel
  if (isServer && Thinker->Role == ROLE_DumbProxy && Ent) {
    // remember that we already did this thinker
    Connection->DetachedThinkers.put(Thinker, true);
    // restore roles
    Thinker->Role = oldRole;
    Thinker->RemoteRole = oldRemoteRole;
    Close();
  }
}


//==========================================================================
//
//  VThinkerChannel::ParsePacket
//
//==========================================================================
void VThinkerChannel::ParsePacket (VMessageIn &Msg) {
  if (Msg.bOpen) {
    VClass *C;
    Connection->ObjMap->SerialiseClass(Msg, C);

    VThinker *Th = Connection->Context->GetLevel()->SpawnThinker(C, TVec(0, 0, 0), TAVec(0, 0, 0), nullptr, false);
    #ifdef CLIENT
    //GCon->Logf("NET:%p: spawned thinker with class `%s`", Th, Th->GetClass()->GetName());
    if (Th && Th->IsA(VLevelInfo::StaticClass())) {
      //GCon->Logf(NAME_DevNet, "*** NET:%p: spawned thinker with class `%s`", Th, Th->GetClass()->GetName());
      VLevelInfo *LInfo = (VLevelInfo *)Th;
      LInfo->Level = LInfo;
      GClLevel->LevelInfo = LInfo;
      cl->Level = LInfo;
      cl->Net->SendCommand("Client_Spawn\n");
      cls.signon = 1;
    }
    #endif
    SetThinker(Th);
  }

  VEntity *Ent = Cast<VEntity>(Thinker);
  TVec oldOrg(0.0f, 0.0f, 0.0f);
  TAVec oldAngles(0.0f, 0.0f, 0.0f);
  if (Ent) {
    Ent->UnlinkFromWorld();
    //TODO: use this to interpolate movements
    //      actually, we need to quantize movements by frame tics (1/35), and
    //      setup interpolation variables
    oldOrg = Ent->Origin;
    oldAngles = Ent->Angles;
  }

  while (!Msg.AtEnd()) {
    int FldIdx = Msg.ReadInt(/*Thinker->GetClass()->NumNetFields*/);
    // special data?
    if (FldIdx == -1) {
      // receive some additional fields for entities
      vuint8 exFlags = 0;
      Msg.SerialiseBits(&exFlags, 3);
      if (Ent) {
        if (exFlags&(1u<<0)) Ent->FlagsEx |= VEntity::EFEX_NoInteraction;
        if (exFlags&(1u<<1)) Ent->FlagsEx |= VEntity::EFEX_NoTickGrav;
        if (exFlags&(1u<<2)) Ent->FlagsEx |= VEntity::EFEX_NoTickGravLT;
        if (exFlags&(1u<<3)) Ent->EntityFlags |= VEntity::EF_NoGravity;
        if (net_dbg_dump_thinker_detach) GCon->Logf(NAME_Debug, "%s:%u: got special flags 0x%02x", Ent->GetClass()->GetName(), Ent->GetUniqueId(), exFlags);
      }
      // lifetime fields
      if (exFlags&(1u<<2)) {
        if (Ent) {
          // special fields for "notick"
          Msg << Ent->LastMoveTime;
          Msg << Ent->PlaneAlpha;
          if (net_dbg_dump_thinker_detach) GCon->Logf(NAME_Debug, "%s:%u: got special notick fields: %g, %g", Ent->GetClass()->GetName(), Ent->GetUniqueId(), Ent->LastMoveTime, Ent->PlaneAlpha);
        } else {
          float a, b;
          Msg << a;
          Msg << b;
        }
      }
      continue;
    }

    // find field
    VField *F = nullptr;
    for (VField *CF = ThinkerClass->NetFields; CF; CF = CF->NextNetField) {
      if (CF->NetIndex == FldIdx) {
        F = CF;
        break;
      }
    }

    // got field?
    if (F) {
      if (F->Type.Type == TYPE_Array) {
        int Idx = Msg.ReadInt(/*F->Type.GetArrayDim()*/);
        VFieldType IntType = F->Type;
        IntType.Type = F->Type.ArrayInnerType;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Thinker+F->Ofs+Idx*IntType.GetSize(), IntType);
      } else {
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Thinker+F->Ofs, F->Type);
      }
      continue;
    }

    // not a field: this must be RPC
    if (ReadRpc(Msg, FldIdx, Thinker)) continue;

    Sys_Error("Bad net field %d", FldIdx);
  }

  if (Ent) {
    Ent->LinkToWorld(true);
    //TODO: do not interpolate players?
    TVec newOrg = Ent->Origin;
    if (newOrg != oldOrg) {
      //GCon->Logf(NAME_Debug, "ENTITY '%s':%u moved! statetime=%g; state=%s", Ent->GetClass()->GetName(), Ent->GetUniqueId(), Ent->StateTime, (Ent->State ? *Ent->State->Loc.toStringNoCol() : "<none>"));
      if (Ent->StateTime < 0) {
        Ent->MoveFlags &= ~VEntity::MVF_JustMoved;
      } else if (Ent->StateTime > 0 && (newOrg-oldOrg).length2DSquared() < 64*64) {
        Ent->LastMoveOrigin = oldOrg;
        //Ent->LastMoveAngles = oldAngles;
        Ent->LastMoveAngles = Ent->Angles;
        Ent->LastMoveTime = Ent->XLevel->Time;
        Ent->LastMoveDuration = Ent->StateTime;
        Ent->MoveFlags |= VEntity::MVF_JustMoved;
      } else {
        Ent->MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    }
  }
}
