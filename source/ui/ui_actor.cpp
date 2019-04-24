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
#include "cl_local.h"
#include "ui.h"


class VActorDisplayWindow : public VWidget {
  DECLARE_CLASS(VActorDisplayWindow, VWidget, 0)
  NO_DEFAULT_CONSTRUCTOR(VActorDisplayWindow)

  VState *CastState;
  float CastTime;
  float StateTime;
  VState *NextState;

public:
  void SetState(VState*);
  virtual void OnDraw() override;

  DECLARE_FUNCTION(SetState)
};


bool R_DrawStateModelFrame (VState *State, VState *NextState, float Inter, const TVec &Origin, float Angle);


IMPLEMENT_CLASS(V, ActorDisplayWindow);


//==========================================================================
//
//  VActorDisplayWindow::SetState
//
//==========================================================================
void VActorDisplayWindow::SetState (VState *AState) {
  CastState = AState;
  StateTime = CastState->Time;
  NextState = CastState->NextState;
}


//==========================================================================
//
//  VActorDisplayWindow::OnDraw
//
//==========================================================================
void VActorDisplayWindow::OnDraw () {
  // draw the current frame in the middle of the screen
  float TimeFrac = 0.0;
  if (StateTime > 0.0) {
    TimeFrac = 1.0 - CastTime / StateTime;
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }
  if (!R_DrawStateModelFrame(CastState, CastState->NextState ? CastState->NextState : CastState,
     TimeFrac, TVec(-128.0, 0.0, -48.0), 0.0))
  {
    auto ow = VirtualWidth, oh = VirtualHeight;
    SCR_SetVirtualScreen(320, 200);
    R_DrawSpritePatch(160, 170, CastState->SpriteIndex, CastState->Frame, 0);
    //SCR_SetVirtualScreen(640, 480);
    SCR_SetVirtualScreen(ow, oh);
  }
}


IMPLEMENT_FUNCTION(VActorDisplayWindow, SetState) {
  P_GET_PTR(VState, State);
  P_GET_SELF;
  Self->SetState(State);
}
