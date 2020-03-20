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
#include "net_message.h"

//#define VV_NET_DEBUG_DUMP_ORGVEL


static VCvarI net_dbg_dump_thinker_channels("net_dbg_dump_thinker_channels", "0", "Dump thinker channels creation/closing (bit 0)?");
static VCvarB net_dbg_allow_simulated_proxies("net_dbg_allow_simulated_proxies", true, "Allow simulated proxies?");
VCvarB net_dbg_dump_thinker_detach("net_dbg_dump_thinker_detach", false, "Dump thinker detaches?");


//==========================================================================
//
//  VThinkerChannel::VThinkerChannel
//
//==========================================================================
VThinkerChannel::VThinkerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Thinker, AIndex, AOpenedLocally)
  , Thinker(nullptr)
  , OldData(nullptr)
  , NewObj(false)
  , FieldCondValues(nullptr)
  , GotOrigin(false)
  , LastUpdateFrame(0)
{
}


//==========================================================================
//
//  VThinkerChannel::~VThinkerChannel
//
//==========================================================================
VThinkerChannel::~VThinkerChannel () {
  if (Connection) {
    // mark channel as closing to prevent sending a message
    Closing = true;
    // if this is a client version of entity, destroy it
    RemoveThinkerFromGame();
  }
}


//==========================================================================
//
//  VThinkerChannel::GetName
//
//==========================================================================
VStr VThinkerChannel::GetName () const noexcept {
  if (Thinker) {
    return VStr(va("thchan #%d <%s:%u>", Index, Thinker->GetClass()->GetName(), Thinker->GetUniqueId()));
  } else {
    return VStr(va("thchan #%d <none>", Index));
  }
}


//==========================================================================
//
//  VThinkerChannel::IsQueueFull
//
//  limit thinkers by the number of outgoing packets instead
//
//==========================================================================
int VThinkerChannel::IsQueueFull () const noexcept {
  return
    OutListCount >= MAX_RELIABLE_BUFFER+8 ? -1 : // oversaturated
    OutListCount >= MAX_RELIABLE_BUFFER ? 1 : // full
    0; // ok
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
      if (Thinker->Role == ROLE_Authority) {
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
//  VThinkerChannel::SetClosing
//
//==========================================================================
void VThinkerChannel::SetClosing () {
  // we have to do this first!
  VChannel::SetClosing();
  RemoveThinkerFromGame();
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
      for (VField *F = Thinker->GetClass()->NetFields; F; F = F->NextNetField) {
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
    VClass *ThinkerClass = Thinker->GetClass();
    if (OpenedLocally) {
      VThinker *Def = (VThinker *)ThinkerClass->Defaults;
      OldData = new vuint8[ThinkerClass->ClassSize];
      memset(OldData, 0, ThinkerClass->ClassSize);
      for (VField *F = ThinkerClass->NetFields; F; F = F->NextNetField) {
        VField::CopyFieldValue((vuint8 *)Def+F->Ofs, OldData+F->Ofs, F->Type);
      }
      FieldCondValues = new vuint8[ThinkerClass->NumNetFields];
      GotOrigin = true;
    } else {
      GotOrigin = false;
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

  // saturation check already done
  // also, if this is client, and the thinker is authority, it is detached, don't send anything
  if (Connection->IsClient() && /*Thinker->Role == ROLE_Authority*/Thinker->Role != ROLE_DumbProxy && Thinker->Role != ROLE_AutonomousProxy) return;

  //if (Thinker->ThinkerFlags&VThinker::TF_DetachSimulated) GCon->Logf(NAME_DevNet, "%s:%u: updating", Thinker->GetClass()->GetName(), Thinker->GetUniqueId());

  VEntity *Ent = Cast<VEntity>(Thinker);

  // set up thinker flags that can be used by field conditions
  // this is required both for the server and for the client
  // the flags will be sent to the client, so it would be able to replicate some one-time fields too
  if (NewObj) Thinker->ThinkerFlags |= VThinker::TF_NetInitial;
  if (Ent != nullptr && Ent->GetTopOwner() == Connection->Owner->MO) Thinker->ThinkerFlags |= VThinker::TF_NetOwner;

  // we need to set `EFEX_NetDetach` flag for replication condition checking if we're going to detach it
  const bool isServer = Connection->Context->IsServer();
  const bool detachEntity =
    isServer && !Connection->AutoAck && Ent &&
    (Ent->FlagsEx&(VEntity::EFEX_NoTickGrav|VEntity::EFEX_DetachFromServer));
  bool detachSimulated = // this is "detached for the first time" (can be reset if it is not the first time)
    net_dbg_allow_simulated_proxies &&
    isServer && !Connection->AutoAck &&
    (Thinker->ThinkerFlags&VThinker::TF_DetachSimulated);
    //Thinker->RemoteRole == ROLE_DumbProxy; // don't detach twice
  bool isSimulated = detachSimulated;

  //if (Thinker->ThinkerFlags&VThinker::TF_DetachSimulated) GCon->Logf(NAME_Debug, "%s: role=%u; rrole=%u; dts=%d; dt=%d", *GetDebugName(), Thinker->Role, Thinker->RemoteRole, (int)detachSimulated, (int)detachEntity);

  //if (VStr::strEqu(Thinker->GetClass()->GetName(), "DoomImpBall")) GCon->Logf(NAME_DevNet, "%s:000: role=%u; remote=%u; desim=%d; issim=%d; dsflag=%d; inlist=%d", *GetDebugName(), Thinker->Role, Thinker->RemoteRole, (int)detachSimulated, (int)isSimulated, (int)(!!(Thinker->ThinkerFlags&VThinker::TF_DetachSimulated)), Connection->SimulatedThinkers.has(Thinker));

  // temporarily set "detach complete"
  const vuint32 oldThFlags = Thinker->ThinkerFlags;
  // check if we're detaching it for the first time
  if (Connection->SimulatedThinkers.has(Thinker)) {
    // in detached list, check if the server wants it back
    if (!detachSimulated) {
      // yeah, it becomes dumb proxy again
      Connection->SimulatedThinkers.del(Thinker);
      Thinker->ThinkerFlags &= ~(VThinker::TF_DetachSimulated|VThinker::TF_DetachComplete); // just in case
      GCon->Logf(NAME_DevNet, "%s: becomes dumb proxy again", *GetDebugName());
    } else {
      // it was already detached
      isSimulated = true;
      // do not send unnecessary fields
      Thinker->ThinkerFlags |= VThinker::TF_DetachComplete;
    }
    detachSimulated = false; // reset "detach simulated" flag, so we won't send detach info
  } else {
    // not in detached list, prolly doing it for the first time
    if (detachSimulated) {
      GCon->Logf(NAME_DevNet, "%s: becomes simulated proxy", *GetDebugName());
      isSimulated = true;
      Connection->SimulatedThinkers.put(Thinker, true);
    }
  }

  //if (VStr::strEqu(Thinker->GetClass()->GetName(), "DoomImpBall")) GCon->Logf(NAME_DevNet, "%s:001: role=%u; remote=%u; desim=%d; issim=%d; dsflag=%d; inlist=%d", *GetDebugName(), Thinker->Role, Thinker->RemoteRole, (int)detachSimulated, (int)isSimulated, (int)(!!(Thinker->ThinkerFlags&VThinker::TF_DetachSimulated)), Connection->SimulatedThinkers.has(Thinker));

  // temporarily set `bNetDetach`
  if (Ent && (detachEntity || detachSimulated)) Ent->FlagsEx |= VEntity::EFEX_NetDetach;

  // it is important to call this *BEFORE* changing the roles!
  EvalCondValues(Thinker, Thinker->GetClass(), FieldCondValues);

  // we can reset "detach complete" now
  Thinker->ThinkerFlags = oldThFlags;

  // fix the roles if we're going to detach this entity
  vuint8 oldRole = 0, oldRemoteRole = 0;
  bool restoreRoles = false;
  if (detachEntity || isSimulated) {
    restoreRoles = true;
    // this is Role on the client
    oldRole = Thinker->Role;
    oldRemoteRole = Thinker->RemoteRole;
    // set role on the client (completely detached)
    Thinker->RemoteRole = (isSimulated ? ROLE_SimulatedProxy : ROLE_Authority);
    // set role on the server (simulated proxy is still the authority)
    if (!isSimulated) Thinker->Role = ROLE_DumbProxy;
  }

  vuint8 *Data = (vuint8 *)Thinker;
  VObject *NullObj = nullptr;

  // use bitstream and split it to the messages
  VMessageOut Msg(this);
  Msg.bOpen = NewObj;
  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, false); // no expand
  int flushCount = 0;

  if (NewObj) {
    VClass *TmpClass = Thinker->GetClass();
    if (!Connection->ObjMap->SerialiseClass(strm, TmpClass)) {
      Sys_Error("%s: cannot serialise thinker class '%s'", *GetDebugName(), Thinker->GetClass()->GetName());
    }
    // send unique id
    vuint32 suid = Thinker->GetUniqueId();
    //suid = 0;
    strm << STRM_INDEX_U(suid);
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

  //Thinker->GetClass()->NumNetFields
  for (VField *F = Thinker->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!FieldCondValues[F->NetIndex]) continue;

    // set up pointer to the value and do swapping for the role fields
    bool forceSend = false;
    vuint8 *FieldValue = Data+F->Ofs;
         if (F == Connection->Context->RoleField) { forceSend = (detachEntity || detachSimulated); FieldValue = Data+Connection->Context->RemoteRoleField->Ofs; }
    else if (F == Connection->Context->RemoteRoleField) { forceSend = (detachEntity || detachSimulated); FieldValue = Data+Connection->Context->RoleField->Ofs; }
    else if (NewObj && F == Connection->OriginField) forceSend = true;

    if (!forceSend && VField::IdenticalValue(FieldValue, OldData+F->Ofs, F->Type)) {
      //GCon->Logf(NAME_DevNet, "%s:%u: skipped field #%d (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, F->GetName(), *F->Type.GetName());
      continue;
    }

    if (F->Type.Type == TYPE_Array) {
      VFieldType IntType = F->Type;
      IntType.Type = F->Type.ArrayInnerType;
      int InnerSize = IntType.GetSize();
      //F->Type.GetArrayDim()
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        vuint8 *Val = FieldValue+i*InnerSize;
        vuint8 *OldVal = OldData+F->Ofs+i*InnerSize;
        if (VField::IdenticalValue(Val, OldVal, IntType)) {
          //GCon->Logf(NAME_DevNet, "%s:%u: skipped array field #%d [%d] (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, i, F->GetName(), *IntType.GetName());
          continue;
        }

        bool allowValueCopy = true;
        // if it's an object reference that cannot be serialised, send it as nullptr reference
        if (IntType.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)Val)) {
          if (!*(VObject **)OldVal) continue; // already sent as nullptr
          Val = (vuint8 *)&NullObj;
          // resend
          allowValueCopy = false;
        }

        strm.WriteUInt((unsigned)F->NetIndex);
        strm.WriteUInt((unsigned)i);
        if (VField::NetSerialiseValue(strm, Connection->ObjMap, Val, IntType)) {
          //GCon->Logf(NAME_DevNet, "%s:%u: sent array field #%d [%d] (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, i, F->GetName(), *IntType.GetName());
          if (allowValueCopy) VField::CopyFieldValue(Val, OldVal, IntType);
        }
        flushCount += PutStream(&Msg, strm);
      }
    } else {
      bool allowValueCopy = true;
      // if it's an object reference that cannot be serialised, send it as nullptr reference
      if (F->Type.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)FieldValue)) {
        if (!*(VObject **)(OldData+F->Ofs)) {
          // already sent as nullptr
          //GCon->Logf(NAME_DevNet, "%s: `%s` is already sent as `nullptr`", *GetDebugName(), F->GetName());
          continue;
        }
        FieldValue = (vuint8 *)&NullObj;
        // resend
        allowValueCopy = false;
      }

      strm.WriteUInt((unsigned)F->NetIndex);
      if (VField::NetSerialiseValue(strm, Connection->ObjMap, FieldValue, F->Type)) {
        //if (Thinker->RemoteRole == ROLE_SimulatedProxy) GCon->Logf(NAME_DevNet, "%s:%u: sent field #%d (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, F->GetName(), *F->Type.GetName());
        //GCon->Logf(NAME_DevNet, "%s:%u: sent field #%d (%s : %s)", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), F->NetIndex, F->GetName(), *F->Type.GetName());
        if (allowValueCopy) VField::CopyFieldValue(FieldValue, OldData+F->Ofs, F->Type);
      }

      //HACK: send suids
      if (F == Connection->Context->OwnerField ||
          F == Connection->Context->TargetField ||
          F == Connection->Context->TracerField ||
          F == Connection->Context->MasterField)
      {
        vassert(Ent);
        vuint32 ownersuid;
             if (F == Connection->Context->OwnerField) ownersuid = (Ent->Owner ? Ent->Owner->GetUniqueId() : 0u);
        else if (F == Connection->Context->TargetField) ownersuid = (Ent->Target ? Ent->Target->GetUniqueId() : 0u);
        else if (F == Connection->Context->TracerField) ownersuid = (Ent->Tracer ? Ent->Tracer->GetUniqueId() : 0u);
        else if (F == Connection->Context->MasterField) ownersuid = (Ent->Master ? Ent->Master->GetUniqueId() : 0u);
        else abort();
        strm << STRM_INDEX_U(ownersuid);
        //GCon->Logf(NAME_DevNet, "%s: sending owner suid (%u)", *GetDebugName(), ownersuid);
      }

      flushCount += PutStream(&Msg, strm);
    }
  }

  // restore player angles
  if (Ent && (Ent->EntityFlags&VEntity::EF_IsPlayer)) Ent->Angles = SavedAngles;

  // restore roles
  if (restoreRoles) {
    Thinker->Role = oldRole;
    Thinker->RemoteRole = oldRemoteRole;
  }

  // remember last update frame (this is used as "got updated" flag)
  LastUpdateFrame = Connection->UpdateFrameCounter;

  // clear temporary networking flags
  Thinker->ThinkerFlags &= ~(VThinker::TF_NetInitial|VThinker::TF_NetOwner);

  // reset detach flag
  if (Ent && (detachEntity || detachSimulated)) Ent->FlagsEx &= ~VEntity::EFEX_NetDetach;

  flushCount += FlushMsg(&Msg);

  // if this is initial send, we have to flush the message, even if it is empty
  if (Msg.bOpen || Msg.bClose || Msg.GetNumBits() || NewObj || detachEntity || detachSimulated) {
    SendMessage(&Msg);
  }
  // not detached, and no interesting data: no reason to send anything

  NewObj = false;

  // if this object becomes "dumb proxy", mark it as detached, and close the channel
  if (detachEntity) {
    if (net_dbg_dump_thinker_detach) GCon->Logf(NAME_DevNet, "%s:%u: became notick, closing channel%s", Thinker->GetClass()->GetName(), Thinker->GetUniqueId(), (OpenedLocally ? " (opened locally)" : ""));
    // remember that we already sent this thinker
    Connection->DetachedThinkers.put(Thinker, true);
    Close();
  }
}


//==========================================================================
//
//  VThinkerChannel::ParseMessage
//
//==========================================================================
void VThinkerChannel::ParseMessage (VMessageIn &Msg) {
  if (Closing) return;

  if (Connection->IsServer()) {
    // if this thinker is detached, don't process any messages
    if (Thinker && Connection->DetachedThinkers.has(Thinker)) return;
  }

  if (Msg.bOpen) {
    VClass *C = nullptr;
    Connection->ObjMap->SerialiseClass(Msg, C);

    if (!C) {
      //Sys_Error("%s: cannot spawn `none` thinker", *GetDebugName());
      GCon->Logf(NAME_Debug, "%s: tried to spawn thinker without name (or with invalid name)", *GetDebugName());
      Connection->Close();
      return;
    }

    // in no case client can spawn anything on the server
    if (Connection->IsServer()) {
      GCon->Logf(NAME_Debug, "%s: client tried to spawn thinker `%s` on the server, dropping client", *GetDebugName(), C->GetName());
      Connection->Close();
      return;
    }

    vuint32 suid = 0;
    // get server unique id
    Msg << STRM_INDEX_U(suid);
    VThinker *Th = Connection->Context->GetLevel()->SpawnThinker(C, TVec(0, 0, 0), TAVec(0, 0, 0), nullptr, false, suid); // no replacements
    #ifdef CLIENT
    //GCon->Logf(NAME_DevNet, "%s spawned thinker with class `%s`(%u)", *GetDebugName(), Th->GetClass()->GetName(), Th->GetUniqueId());
    if (Th->IsA(VLevelInfo::StaticClass())) {
      //GCon->Logf(NAME_DevNet, "*** %s: got LevelInfo, sending 'client_spawn' command", *GetDebugName());
      vassert(Connection->Context->GetLevel() == GClLevel);
      VLevelInfo *LInfo = (VLevelInfo *)Th;
      LInfo->Level = LInfo;
      GClLevel->LevelInfo = LInfo;
      GClLevel->UpdateThinkersLevelInfo();
      cl->Level = LInfo;
      //k8: let's hope we got enough info here
      cl->Net->SendCommand("Client_Spawn\n");
      cls.signon = 1;
    }
    #endif
    SetThinker(Th);
  }

  if (!Thinker) {
    if (Connection->IsServer()) {
      GCon->Logf(NAME_Error, "SERVER: %s: for some reason it doesn't have thinker!", *GetDebugName());
      //Close();
      return;
    } else {
      GCon->Logf(NAME_Error, "CLIENT: %s: for some reason it doesn't have thinker!", *GetDebugName());
      return;
    }
  }

  vassert(Thinker);
  VClass *ThinkerClass = Thinker->GetClass();
  const vuint8 prevRole = Thinker->Role;
  const vuint8 prevRemoteRole = Thinker->RemoteRole;

  VEntity *Ent = Cast<VEntity>(Thinker);
  TVec oldOrg(0.0f, 0.0f, 0.0f);
  TAVec oldAngles(0.0f, 0.0f, 0.0f);
  float oldDT = 0;
  bool gotDataGameTime = false;
  const bool wasGotOrigin = GotOrigin;
  #ifdef VV_NET_DEBUG_DUMP_ORGVEL
  TVec oldVel(0.0f, 0.0f, 0.0f);
  #endif
  if (Ent) {
    //Ent->UnlinkFromWorld();
    //TODO: use this to interpolate movements
    //      actually, we need to quantize movements by frame tics (1/35), and
    //      setup interpolation variables
    oldOrg = Ent->Origin;
    oldAngles = Ent->Angles;
    oldDT = Ent->DataGameTime;
    #ifdef VV_NET_DEBUG_DUMP_ORGVEL
    oldVel = Ent->Velocity;
    #endif
  }

  while (!Msg.AtEnd()) {
    int FldIdx = (int)Msg.ReadUInt();

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
        int Idx = (int)Msg.ReadUInt();
        VFieldType IntType = F->Type;
        IntType.Type = F->Type.ArrayInnerType;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Thinker+F->Ofs+Idx*IntType.GetSize(), IntType);
      } else {
        if (Ent && F == Connection->OriginField) Ent->UnlinkFromWorld();

        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Thinker+F->Ofs, F->Type);

        //HACK: read owner suid
        if (F == Connection->Context->OwnerField ||
            F == Connection->Context->TargetField ||
            F == Connection->Context->TracerField ||
            F == Connection->Context->MasterField)
        {
          vassert(Ent);
          vuint32 ownersuid = 0;
          Msg << STRM_INDEX_U(ownersuid);
               if (F == Connection->Context->OwnerField) Ent->OwnerSUId = ownersuid;
          else if (F == Connection->Context->TargetField) Ent->TargetSUId = ownersuid;
          else if (F == Connection->Context->TracerField) Ent->TracerSUId = ownersuid;
          else if (F == Connection->Context->MasterField) Ent->MasterSUId = ownersuid;
          else abort();
        }

        // set "got origin" flag
        if (F == Connection->OriginField) {
          if (Ent) Ent->LinkToWorld(false); // no need to do proper floor check here
          GotOrigin = true;
          // update player flags
          if (Connection->IsClient()) {
            auto pc = Connection->GetPlayerChannel();
            if (pc && pc->Plr && pc->Plr->MO == Thinker) pc->GotMOOrigin = true;
          }
        } else if (F == Connection->DataGameTimeField) {
          gotDataGameTime = true;
        }
      }
      continue;
    }

    // not a field: this must be RPC
    if (ReadRpc(Msg, FldIdx, Thinker)) continue;

    Host_Error(va("%s: Bad net field %d", *GetDebugName(), FldIdx));
  }

  if (Ent) {
    Ent->LinkToWorld(true);
    auto pc = Connection->GetPlayerChannel();
    // do not interpolate player mobj
    if (!wasGotOrigin || (pc && pc->Plr && pc->Plr->MO == Thinker)) {
      Ent->MoveFlags &= ~VEntity::MVF_JustMoved;
    } else {
      TVec newOrg = Ent->Origin;
      // try to compensate for teleports
      if (fabs(newOrg.x-oldOrg.x) > 32 || fabs(newOrg.y-oldOrg.y) > 32 || fabs(newOrg.z-oldOrg.z) > 32) {
        Ent->MoveFlags &= ~VEntity::MVF_JustMoved;
      } else if (gotDataGameTime && Ent->DataGameTime > oldDT) {
        Ent->LastMoveOrigin = oldOrg;
        Ent->LastMoveAngles = oldAngles;
        Ent->LastMoveTime = Ent->XLevel->Time; //-(Ent->DataGameTime-oldDT);
        Ent->LastMoveDuration = (Ent->DataGameTime-oldDT);
        Ent->MoveFlags |= VEntity::MVF_JustMoved;
        //GCon->Logf(NAME_DevNet, "%s: INTERPOLATOR!", *GetDebugName());
        #ifdef VV_NET_DEBUG_DUMP_ORGVEL
        GCon->Logf(NAME_DevNet, "%s: oldtime=%g; newtime=%g; oldorg=(%g,%g,%g); neworg=(%g,%g,%g); oldvel=(%g,%g,%g); newvel=(%g,%g,%g)",
          *GetDebugName(), oldDT, Ent->DataGameTime,
          oldOrg.x, oldOrg.y, oldOrg.z,
          Ent->Origin.x, Ent->Origin.y, Ent->Origin.z,
          oldVel.x, oldVel.y, oldVel.z,
          Ent->Velocity.x, Ent->Velocity.y, Ent->Velocity.z);
        #endif
      }
    }
  }

  if (!Thinker) return; // just in case

  if (Connection->IsClient()) {
    // client
    //if (Thinker->Role == ROLE_SimulatedProxy) GCon->Logf(NAME_Debug, "%s: prevrole=%u; role=%u", Thinker->GetClass()->GetName(), prevRole, Thinker->Role);
    if ((Msg.bClose && Thinker->Role == ROLE_Authority) ||
        (prevRole != ROLE_SimulatedProxy && Thinker->Role == ROLE_SimulatedProxy))
    {
      //GCon->Logf(NAME_Debug, "completed detaching for '%s'", Thinker->GetClass()->GetName());
      Thinker->ThinkerFlags |= VThinker::TF_DetachComplete;
      Thinker->eventOnDetachedFromServer();
      if (Ent) Ent->MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  } else {
    // server
    if (prevRemoteRole != Thinker->RemoteRole || prevRole != Thinker->Role) GCon->Logf(NAME_DevNet, "%s: prev: rem=%u; role=%u; curr: rem=%u; role=%u", *GetDebugName(), prevRemoteRole, prevRole, Thinker->RemoteRole, Thinker->Role);
    if (prevRemoteRole == ROLE_SimulatedProxy && Thinker->RemoteRole == ROLE_DumbProxy) {
      // yeah, it becomes dumb proxy again
      Connection->SimulatedThinkers.del(Thinker);
      GCon->Logf(NAME_DevNet, "%s: becomes dumb proxy again (client request)", *GetDebugName());
      Thinker->ThinkerFlags &= ~(VThinker::TF_DetachSimulated|VThinker::TF_DetachComplete);
    }
  }
}
