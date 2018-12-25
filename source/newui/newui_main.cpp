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
#include "../gamedefs.h"

#ifdef CLIENT
#include "../cl_local.h"
#include "../drawer.h"
#include "newui.h"


// ////////////////////////////////////////////////////////////////////////// //
struct NewUIICB {
  NewUIICB () {
    VDrawer::RegisterICB(&drawerICB);
  }

  static VClass *nuiMainCls;
  static VObject *nuiMainObj;
  static VMethod *nuiMDispatch;
  static VMethod *nuiMIsPaused;

  static void drawerICB (int phase) {
    //GCon->Logf("NEWUI: phase=%d", phase);
    if (phase == VDrawer::VCB_InitVideo && !nuiMainCls) {
      nuiMainCls = VClass::FindClass("NUI_Main");
      if (nuiMainCls) {
        nuiMainObj = (VObject *)VObject::StaticSpawnObject(nuiMainCls, false); // don't skip replacement
        if (nuiMainObj) {
          nuiMDispatch = nuiMainCls->FindMethod("Dispatch");
          if (nuiMDispatch) {
            if (nuiMDispatch->NumParams != 1 ||
                nuiMDispatch->ParamTypes[0].Type != TYPE_Struct ||
                !nuiMDispatch->ParamTypes[0].Struct ||
                nuiMDispatch->ParamTypes[0].Struct->Name != "event_t" ||
                nuiMDispatch->ParamFlags[0] != FPARM_Ref ||
                (nuiMDispatch->ReturnType.Type != TYPE_Int && nuiMDispatch->ReturnType.Type != TYPE_Bool) ||
                (nuiMDispatch->Flags&(FUNC_VarArgs|FUNC_Net|FUNC_Spawner|FUNC_NetReliable|FUNC_Iterator|FUNC_Private|FUNC_Protected)) != 0)
            {
              nuiMDispatch = nullptr;
            }
          }
          nuiMIsPaused = nuiMainCls->FindMethod("IsPaused");
          if (nuiMIsPaused) {
            if (nuiMIsPaused->NumParams != 0 ||
                (nuiMIsPaused->ReturnType.Type != TYPE_Int && nuiMIsPaused->ReturnType.Type != TYPE_Bool) ||
                (nuiMDispatch->Flags&(FUNC_VarArgs|FUNC_Net|FUNC_Spawner|FUNC_NetReliable|FUNC_Iterator|FUNC_Private|FUNC_Protected)) != 0)
            {
              nuiMIsPaused = nullptr;
            }
          }
        }
      }
    }
  }
};

static NewUIICB newuiicb;

VClass *NewUIICB::nuiMainCls = nullptr;
VObject *NewUIICB::nuiMainObj = nullptr;
VMethod *NewUIICB::nuiMDispatch = nullptr;
VMethod *NewUIICB::nuiMIsPaused = nullptr;


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  NUI_IsPaused
//
//==========================================================================
bool NUI_IsPaused () {
  if (NewUIICB::nuiMIsPaused) {
    if ((NewUIICB::nuiMIsPaused->Flags&FUNC_Static) == 0) P_PASS_REF(NewUIICB::nuiMainObj);
    return VObject::ExecuteFunction(NewUIICB::nuiMIsPaused).getBool();
  }
  return false;
}


//==========================================================================
//
//  NUI_Responder
//
//==========================================================================
bool NUI_Responder (event_t *ev) {
  if (!ev) return false;
  if (NewUIICB::nuiMDispatch) {
    if ((NewUIICB::nuiMDispatch->Flags&FUNC_Static) == 0) P_PASS_REF(NewUIICB::nuiMainObj);
    P_PASS_PTR(ev);
    return VObject::ExecuteFunction(NewUIICB::nuiMDispatch).getBool();
  }
  return false;
}


#endif
