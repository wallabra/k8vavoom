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
//**  Copyright (C) 2018 Ketmar Dark
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
#if 0
// replicator
static void evalCondValues (VObject *obj, VClass *Class, vuint8 *values) {
  if (Class->GetSuperClass()) evalCondValues(obj, Class->GetSuperClass(), values);
  int len = Class->RepInfos.length();
  for (int i = 0; i < len; ++i) {
    P_PASS_REF(obj);
    vuint8 val = (VObject::ExecuteFunction(Class->RepInfos[i].Cond).i ? 1 : 0);
    int rflen = Class->RepInfos[i].RepFields.length();
    for (int j = 0; j < rflen; ++j) {
      if (Class->RepInfos[i].RepFields[j].Member->MemberType != MEMBER_Field) continue;
      values[((VField *)Class->RepInfos[i].RepFields[j].Member)->NetIndex] = val;
    }
  }
}


static vuint8 *createEvalConds (VObject *obj) {
  return new vuint8[obj->NumNetFields];
}


static vuint8 *createOldData (VClass *Class) {
  vuint8 *oldData = new vuint8[Class->ClassSize];
  memset(oldData, 0, Class->ClassSize);
  vuint8 *def = (vuint8 *)Class->Defaults;
  for (VField *F = Class->NetFields; F; F = F->NextNetField) {
    VField::CopyFieldValue(def+F->Ofs, oldData+F->Ofs, F->Type);
  }
}


static void deleteOldData (VClass *Class, vuint8 *oldData) {
  if (oldData) {
    for (VField *F = Class->NetFields; F; F = F->NextNetField) {
      VField::DestructField(oldData+F->Ofs, F->Type);
    }
    delete[] oldData;
}


static void replicateObj (VObject *obj, vuint8 *oldData) {
  // set up thinker flags that can be used by field condition
  //if (NewObj) Thinker->ThinkerFlags |= VThinker::TF_NetInitial;
  //if (Ent != nullptr && Ent->GetTopOwner() == Connection->Owner->MO) Thinker->ThinkerFlags |= VThinker::TF_NetOwner;

  auto condv = createEvalConds(obj);
  memset(condv, 0, obj->NumNetFields);
  evalCondValues(obj, obj->GetClass(), condv);

  vuint8 *data = (vuint8 *)obj;
  VObject *nullObj = nullptr;

  /*
  if (NewObj) {
    Msg.bOpen = true;
    VClass *TmpClass = Thinker->GetClass();
    Connection->ObjMap->SerialiseClass(Msg, TmpClass);
    NewObj = false;
  }
  */

  /*
  TAVec SavedAngles;
  if (Ent) {
    SavedAngles = Ent->Angles;
    if (Ent->EntityFlags & VEntity::EF_IsPlayer) {
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
  */

  for (VField *F = obj->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!condv[F->NetIndex]) continue;

    // set up pointer to the value and do swapping for the role fields
    vuint8 *fieldData = data+F->Ofs;
    /*
         if (F == Connection->Context->RoleField) fieldData = data+Connection->Context->RemoteRoleField->Ofs;
    else if (F == Connection->Context->RemoteRoleField) fieldData = data+Connection->Context->RoleField->Ofs;
    */

    if (VField::IdenticalValue(fieldData, oldData+F->Ofs, F->Type)) continue;

    if (F->Type.Type == TYPE_Array) {
      VFieldType intrType = F->Type;
      intrType.Type = F->Type.ArrayInnerType;
      int innerSize = intrType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        vuint8 *val = fieldData+i*innerSize;
        vuint8 *oldval = oldData+F->Ofs+i*innerSize;
        if (VField::IdenticalValue(val, oldval, intrType)) continue;
        // if it's an object reference that cannot be serialised, send it as nullptr reference
        if (intrType.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)val)) {
          if (!*(VObject **)oldval) continue; // already sent as nullptr
          val = (vuint8 *)&nullObj;
        }

        Msg.WriteInt(F->NetIndex, obj->GetClass()->NumNetFields);
        Msg.WriteInt(i, F->Type.GetArrayDim());
        if (VField::NetSerialiseValue(Msg, Connection->ObjMap, val, intrType)) {
          VField::CopyFieldValue(val, oldval, intrType);
        }
      }
    } else {
      // if it's an object reference that cannot be serialised, send it as nullptr reference
      if (F->Type.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject**)fieldData)) {
        if (!*(VObject **)(oldData+F->Ofs)) continue; // already sent as nullptr
        fieldData = (vuint8 *)&nullObj;
      }

      Msg.WriteInt(F->NetIndex, obj->GetClass()->NumNetFields);
      if (VField::NetSerialiseValue(Msg, Connection->ObjMap, fieldData, F->Type)) {
        VField::CopyFieldValue(fieldData, oldData + F->Ofs, F->Type);
      }
    }
  }

  if (Ent && (Ent->EntityFlags & VEntity::EF_IsPlayer)) Ent->Angles = SavedAngles;
  UpdatedThisFrame = true;

  if (Msg.GetNumBits()) SendMessage(&Msg);

  // clear temporary networking flags
  obj->ThinkerFlags &= ~VThinker::TF_NetInitial;
  obj->ThinkerFlags &= ~VThinker::TF_NetOwner;

  unguard;
}
#endif



// ////////////////////////////////////////////////////////////////////////// //
static bool onExecuteNetMethod (VObject *aself, VMethod *func) {
  /*
  if (GDemoRecordingContext) {
    // find initial version of the method
    VMethod *Base = func;
    while (Base->SuperMethod) Base = Base->SuperMethod;
    // execute it's replication condition method
    check(Base->ReplCond);
    P_PASS_REF(this);
    vuint32 SavedFlags = PlayerFlags;
    PlayerFlags &= ~VBasePlayer::PF_IsClient;
    bool ShouldSend = false;
    if (VObject::ExecuteFunction(Base->ReplCond).i) ShouldSend = true;
    PlayerFlags = SavedFlags;
    if (ShouldSend) {
      // replication condition is true, the method must be replicated
      GDemoRecordingContext->ClientConnections[0]->Channels[CHANIDX_Player]->SendRpc(func, this);
    }
  }
  */

  /*
#ifdef CLIENT
  if (GGameInfo->NetMode == NM_TitleMap ||
    GGameInfo->NetMode == NM_Standalone ||
    (GGameInfo->NetMode == NM_ListenServer && this == cl))
  {
    return false;
  }
#endif
  */

  // find initial version of the method
  VMethod *Base = func;
  while (Base->SuperMethod) Base = Base->SuperMethod;
  // execute it's replication condition method
  check(Base->ReplCond);
  P_PASS_REF(aself);
  if (!VObject::ExecuteFunction(Base->ReplCond).getBool()) {
    //fprintf(stderr, "rpc call to `%s` (%s) is not done\n", aself->GetClass()->GetName(), *func->GetFullName());
    return false;
  }

  /*
  if (Net) {
    // replication condition is true, the method must be replicated
    Net->Channels[CHANIDX_Player]->SendRpc(func, this);
  }
  */

  // clean up parameters
  func->CleanupParams();

  fprintf(stderr, "rpc call to `%s` (%s) is DONE!\n", aself->GetClass()->GetName(), *func->GetFullName());

  // it's been handled here
  return true;
}
