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
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "VThinkerChannel::~VThinkerChannel:%p (#%d) -- done", this, Index);
  #endif
  #endif
}


//==========================================================================
//
//  VThinkerChannel::RemoveThinkerFromGame
//
//==========================================================================
void VThinkerChannel::RemoveThinkerFromGame () {
  // if this is a client version of entity, destroy it
  if (!Thinker) return;
  if (!OpenedLocally) {
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::~RemoveThinkerFromGame:%p (#%d) -- before `DestroyThinker()`", this, Index);
    #endif
    #endif
    // avoid loops
    VThinker *th = Thinker;
    Thinker = nullptr;
    th->DestroyThinker();
    //k8: oooh; hacks upon hacks, and more hacks... one of those methods can call `SetThinker(nullptr)` for us
    if (th && th->XLevel) {
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      #ifdef CLIENT
      GCon->Logf(NAME_Debug, "VThinkerChannel::~RemoveThinkerFromGame:%p (#%d) -- before `RemoveThinker()`", this, Index);
      #endif
      #endif
      th->XLevel->RemoveThinker(th);
    }
    if (th) {
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      #ifdef CLIENT
      GCon->Logf(NAME_Debug, "VThinkerChannel::~RemoveThinkerFromGame:%p (#%d) -- before `ConditionalDestroy()`", this, Index);
      #endif
      #endif
      th->ConditionalDestroy();
    }
    // restore it
    Thinker = th;
  }
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "VThinkerChannel::~RemoveThinkerFromGame:%p (#%d) -- before `SetThinker()`", this, Index);
  #endif
  #endif
  SetThinker(nullptr);
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "VThinkerChannel::~RemoveThinkerFromGame:%p (#%d) -- done", this, Index);
  #endif
  #endif
}


//==========================================================================
//
//  VThinkerChannel::Suicide
//
//==========================================================================
void VThinkerChannel::Suicide () {
  Closing = true;
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "VThinkerChannel::Suicide:%p (#%d) -- resetting thinker", this, Index);
  #endif
  #endif
  // if this is a client version of entity, destroy it
  RemoveThinkerFromGame();
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "VThinkerChannel::Suicide:%p (#%d) -- calling super::Suicide()", this, Index);
  #endif
  #endif
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
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::Close:%p (#%d) -- calling super::Close()", this, Index);
    #endif
    #endif
    VChannel::Close();
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::Close:%p (#%d) -- removing thinker", this, Index);
    #endif
    #endif
    // if this is a client version of entity, destroy it
    RemoveThinkerFromGame();
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::Close:%p (#%d) -- done", this, Index);
    #endif
    #endif
  }
}


//==========================================================================
//
//  VThinkerChannel::SetThinker
//
//==========================================================================
void VThinkerChannel::SetThinker (VThinker *AThinker) {
  if (Thinker) {
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::SetThinker:%p (#%d): removing thinker '%s':%u", this, Index, Thinker->GetClass()->GetName(), Thinker->GetUniqueId());
    #endif
    #endif
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
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifdef CLIENT
    GCon->Logf(NAME_Debug, "VThinkerChannel::SetThinker:%p (#%d): setting thinker '%s':%u", this, Index, AThinker->GetClass()->GetName(), AThinker->GetUniqueId());
    #endif
    #endif
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
  if (Closing) return;

  VEntity *Ent = Cast<VEntity>(Thinker);

  // set up thinker flags that can be used by field condition
  if (NewObj) Thinker->ThinkerFlags |= VThinker::TF_NetInitial;
  if (Ent != nullptr && Ent->GetTopOwner() == Connection->Owner->MO) Thinker->ThinkerFlags |= VThinker::TF_NetOwner;

  EvalCondValues(Thinker, Thinker->GetClass(), FieldCondValues);
  vuint8 *Data = (vuint8 *)Thinker;
  VObject *NullObj = nullptr;

  VMessageOut Msg(this);
  Msg.bReliable = true;

  if (NewObj) {
    Msg.bOpen = true;
    VClass *TmpClass = Thinker->GetClass();
    Connection->ObjMap->SerialiseClass(Msg, TmpClass);
    NewObj = false;
    // if this is gore object, set role to authority, and remoterole to dumbproxy
    // dumbproxy channels will be closed before exiting this method
    // note that roles are swapped here
    /*
    if (VStr::startsWith(TmpClass->GetName(), "K8Gore")) {
      // this is Role on the client
      Thinker->SetFieldByte("RemoteRole", ROLE_SimulatedProxy);
      // this is RemoteRole on the client (and Role on the server)
      Thinker->SetFieldByte("Role", ROLE_SimulatedProxy);
    }
    */
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

    if (VField::IdenticalValue(FieldValue, OldData+F->Ofs, F->Type)) continue;
    if (F->Type.Type == TYPE_Array) {
      VFieldType IntType = F->Type;
      IntType.Type = F->Type.ArrayInnerType;
      int InnerSize = IntType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        vuint8 *Val = FieldValue+i*InnerSize;
        vuint8 *OldVal = OldData+F->Ofs+i*InnerSize;
        if (VField::IdenticalValue(Val, OldVal, IntType)) continue;

        // if it's an object reference that cannot be serialised, send it as nullptr reference
        if (IntType.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)Val)) {
          if (!*(VObject **)OldVal) continue; // already sent as nullptr
          Val = (vuint8 *)&NullObj;
        }

        Msg.WriteInt(F->NetIndex/*, Thinker->GetClass()->NumNetFields*/);
        Msg.WriteInt(i/*, F->Type.GetArrayDim()*/);
        if (VField::NetSerialiseValue(Msg, Connection->ObjMap, Val, IntType)) {
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
        VField::CopyFieldValue(FieldValue, OldData+F->Ofs, F->Type);
      }
    }
  }

  if (Ent && (Ent->EntityFlags&VEntity::EF_IsPlayer)) Ent->Angles = SavedAngles;
  UpdatedThisFrame = true;

  if (Msg.GetNumBits()) SendMessage(&Msg);

  // clear temporary networking flags
  Thinker->ThinkerFlags &= ~VThinker::TF_NetInitial;
  Thinker->ThinkerFlags &= ~VThinker::TF_NetOwner;
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
    VField *F = nullptr;
    for (VField *CF = ThinkerClass->NetFields; CF; CF = CF->NextNetField) {
      if (CF->NetIndex == FldIdx) {
        F = CF;
        break;
      }
    }
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
