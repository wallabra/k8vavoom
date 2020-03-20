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


//==========================================================================
//
//  VPlayerChannel::VPlayerChannel
//
//==========================================================================
VPlayerChannel::VPlayerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Player, AIndex, AOpenedLocally)
  , Plr(nullptr)
  , OldData(nullptr)
  , NewObj(false)
  , FieldCondValues(nullptr)
  , NextUpdateTime(0)
  , LastMOSUid(0)
{
  OpenAcked = true; // this channel is pre-opened
  GameTimeField = VBasePlayer::StaticClass()->FindFieldChecked("GameTime");
}


//==========================================================================
//
//  VPlayerChannel::~VPlayerChannel
//
//==========================================================================
VPlayerChannel::~VPlayerChannel () {
  if (Connection) SetPlayer(nullptr);
}


//==========================================================================
//
//  VPlayerChannel::GetName
//
//==========================================================================
VStr VPlayerChannel::GetName () const noexcept {
  return VStr(va("plrchan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VPlayerChannel::SetClosing
//
//==========================================================================
void VPlayerChannel::SetClosing () {
  VChannel::SetClosing();
  if (Connection) SetPlayer(nullptr);
}


//==========================================================================
//
//  VPlayerChannel::SetPlayer
//
//==========================================================================
void VPlayerChannel::SetPlayer (VBasePlayer *APlr) {
  if (Plr) {
    if (OldData) {
      for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
        VField::DestructField(OldData+F->Ofs, F->Type);
      }
      delete[] OldData;
      OldData = nullptr;
    }
    if (FieldCondValues) {
      delete[] FieldCondValues;
      FieldCondValues = nullptr;
    }
    FieldsToResend.reset();
    NewObj = false;
  }

  Plr = APlr;

  if (Plr) {
    VBasePlayer *Def = (VBasePlayer *)Plr->GetClass()->Defaults;
    OldData = new vuint8[Plr->GetClass()->ClassSize];
    memset(OldData, 0, Plr->GetClass()->ClassSize);
    for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
      VField::CopyFieldValue((vuint8 *)Def+F->Ofs, OldData+F->Ofs, F->Type);
    }
    FieldCondValues = new vuint8[Plr->GetClass()->NumNetFields];
    FieldsToResend.reset();
    NewObj = true;
    NextUpdateTime = 0; // just in case
  }

  LastMOSUid = 0;
}


//==========================================================================
//
//  VPlayerChannel::ResetLevel
//
//  we need to reset all class field values, because new map will
//  get new thinkers, but we won't notice that, and client won't be
//  able to recreate them.
//  FIXME: the client will leak those objects!
//
//==========================================================================
void VPlayerChannel::ResetLevel () {
  if (!OldData || !Plr) return;

  /*
  // actually, clear everything, why not?
  for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
    VField::DestructField(OldData+F->Ofs, F->Type);
  }

  memset(OldData, 0, Plr->GetClass()->ClassSize);
  VBasePlayer *Def = (VBasePlayer *)Plr->GetClass()->Defaults;
  for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
    VField::CopyFieldValue((vuint8 *)Def+F->Ofs, OldData+F->Ofs, F->Type);
  }
  */

  // it is enough to set this flag
  NewObj = true;
  NextUpdateTime = 0; // just in case
  FieldsToResend.reset();
  LastMOSUid = 0;
}


//==========================================================================
//
//  VPlayerChannel::EvalCondValues
//
//==========================================================================
void VPlayerChannel::EvalCondValues (VObject *Obj, VClass *Class, vuint8 *Values) {
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
//  VPlayerChannel::Update
//
//==========================================================================
void VPlayerChannel::Update () {
  if (Closing) return; // just in case

  // if network connection is saturated, do nothing
  if (!CanSendData()) { /*Connection->NeedsUpdate = true;*/ return; }

  const double ctt = Connection->Driver->GetNetTime();
  // for server, limit client updates
  if (!NewObj && Connection->IsServer() && NextUpdateTime > ctt) {
    // skip updating
    return;
  }

  EvalCondValues(Plr, Plr->GetClass(), FieldCondValues);

  // use bitstream and split it to the messages here
  VMessageOut Msg(this);
  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, false); // no expand
  int flushCount = 0;

  vuint8 *Data = (vuint8 *)Plr;
  for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!FieldCondValues[F->NetIndex]) continue;
    if (!NewObj && VField::IdenticalValue(Data+F->Ofs, OldData+F->Ofs, F->Type)) {
      if (!FieldsToResend.has(F)) continue;
      GCon->Logf(NAME_DevNet, "%s: need to resend field '%s' (%s)", *GetDebugName(), F->GetName(), *F->Type.GetName());
    }
    //GCon->Logf(NAME_DevNet, "%s: ...sending player update (%s); field %s", *GetDebugName(), (Connection->IsClient() ? "client" : "server"), *F->GetFullName());
    if (F->Type.Type == TYPE_Array) {
      VFieldType IntType = F->Type;
      IntType.Type = F->Type.ArrayInnerType;
      int InnerSize = IntType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        if (!NewObj && VField::IdenticalValue(Data+F->Ofs+i*InnerSize, OldData+F->Ofs+i*InnerSize, IntType)) {
          //FIXME: store field index too
          if (!FieldsToResend.has(F)) continue;
        }
        //GCon->Logf(NAME_DevNet, "%s: updating array (%d) field '%s' (%s) (queue: depth=%d; bitsize=%d; satura=%d)", *GetDebugName(), i, F->GetName(), *F->Type.GetName(), OutListCount, OutListBits, Connection->SaturaDepth);
        strm.WriteUInt((unsigned)F->NetIndex);
        strm.WriteUInt((unsigned)i);
        if (VField::NetSerialiseValue(strm, Connection->ObjMap, Data+F->Ofs+i*InnerSize, IntType)) {
          VField::CopyFieldValue(Data+F->Ofs+i*InnerSize, OldData+F->Ofs+i*InnerSize, IntType);
          FieldsToResend.remove(F);
        } else {
          if (NewObj || true) FieldsToResend.put(F, true);
        }
        flushCount += PutStream(&Msg, strm);
      }
    } else {
      //GCon->Logf(NAME_DevNet, "%s: updating field '%s' (%s) (queue: depth=%d; bitsize=%d; satura=%d)", *GetDebugName(), F->GetName(), *F->Type.GetName(), OutListCount, OutListBits, Connection->SaturaDepth);
      strm.WriteUInt((unsigned)F->NetIndex);
      if (VField::NetSerialiseValue(strm, Connection->ObjMap, Data+F->Ofs, F->Type)) {
        VField::CopyFieldValue(Data+F->Ofs, OldData+F->Ofs, F->Type);
        FieldsToResend.remove(F);
      } else {
        if (NewObj || true) FieldsToResend.put(F, true);
      }
      flushCount += PutStream(&Msg, strm);
    }
  }
  NewObj = false;

  //GCon->Logf(NAME_DevNet, "%s: sending player update (%s)", *GetDebugName(), (Connection->IsClient() ? "client" : "server"));
  //SendMessage(&Msg);
  flushCount += FlushMsg(&Msg);
  // if update happened, note it
  if (flushCount) NextUpdateTime = ctt+1.0/60.0; // don't do it more often, because why?
  // send something in any case if we are a client
  /* nope, there is no need to do this
  if (!flushCount && Connection->IsClient()) {
    vassert(Msg.GetNumBits() == 0);
    SendMessage(&Msg);
  }
  */

  if (Plr->MO) {
    if (Plr->MO->ServerUId != LastMOSUid) {
      LastMOSUid = Plr->MO->ServerUId;
      if (Connection->IsClient()) Plr->eventInitWeaponSlots();
    }
  } else {
    LastMOSUid = 0;
  }

  //if (Plr) GCon->Logf(NAME_Debug, "%s: sent WorldTimer=%g", *GetDebugName(), Plr->WorldTimer);
}


//==========================================================================
//
//  VPlayerChannel::ParseMessage
//
//==========================================================================
void VPlayerChannel::ParseMessage (VMessageIn &Msg) {
  if (!Plr) return; // just in case

  //GCon->Logf(NAME_DevNet, "%s: received player update (%s)", *GetDebugName(), (Connection->IsClient() ? "client" : "server"));
  while (!Msg.AtEnd()) {
    int FldIdx = (int)Msg.ReadUInt();
    VField *F = nullptr;
    for (VField *CF = Plr->GetClass()->NetFields; CF; CF = CF->NextNetField) {
      if (CF->NetIndex == FldIdx) {
        F = CF;
        break;
      }
    }
    if (F) {
      //GCon->Logf(NAME_DevNet, "%s: ...received player update (%s); field %s", *GetDebugName(), (Connection->IsClient() ? "client" : "server"), *F->GetFullName());
      if (F->Type.Type == TYPE_Array) {
        int Idx = (int)Msg.ReadUInt();
        VFieldType IntType = F->Type;
        IntType.Type = F->Type.ArrayInnerType;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Plr+F->Ofs+Idx*IntType.GetSize(), IntType);
      } else {
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Plr+F->Ofs, F->Type);
        // update client times
        if (F == GameTimeField && Connection->IsClient()) {
          Plr->ClLastGameTime = Plr->GameTime;
        }
      }
      continue;
    }

    if (ReadRpc(Msg, FldIdx, Plr)) continue;

    Sys_Error("Bad net field %d", FldIdx);
  }

  //GCon->Logf(NAME_Debug, "%s: got WorldTimer=%g", *GetDebugName(), Plr->WorldTimer);
}
