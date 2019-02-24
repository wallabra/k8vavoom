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
//**
//**  Switches, buttons. Two-state animation. Exits.
//**
//**************************************************************************

//==================================================================
//
//      CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//
//==================================================================

#include "gamedefs.h"
#include "sv_local.h"


#define BUTTONTIME  (1.0f) // 1 second


enum EBWhere {
  SWITCH_Top,
  SWITCH_Middle,
  SWITCH_Bottom
};


class VThinkButton : public VThinker {
  DECLARE_CLASS(VThinkButton, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VThinkButton)

  vint32 Side;
  vuint8 Where;
  vint32 SwitchDef;
  vint32 Frame;
  float Timer;
  VName DefaultSound;
  vuint8 UseAgain; // boolean

  //virtual void SerialiseOther (VStream&) override;
  virtual void Tick (float) override;
  bool AdvanceFrame ();
};


IMPLEMENT_CLASS(V, ThinkButton)


//==========================================================================
//
//  VLevelInfo::ChangeSwitchTexture
//
//  Function that changes wall texture.
//  Tell it if switch is ok to use again (1=yes, it's a button).
//
//==========================================================================
bool VLevelInfo::ChangeSwitchTexture (int sidenum, bool useAgain, VName DefaultSound, bool &Quest) {
  int texTop = XLevel->Sides[sidenum].TopTexture;
  int texMid = XLevel->Sides[sidenum].MidTexture;
  int texBot = XLevel->Sides[sidenum].BottomTexture;

  for (int  i = 0; i < Switches.Num(); ++i) {
    EBWhere where;
    TSwitch *sw = Switches[i];

    if (sw->Tex == texTop) {
      where = SWITCH_Top;
      XLevel->Sides[sidenum].TopTexture = sw->Frames[0].Texture;
    } else if (sw->Tex == texMid) {
      where = SWITCH_Middle;
      XLevel->Sides[sidenum].MidTexture = sw->Frames[0].Texture;
    } else if (sw->Tex == texBot) {
      where = SWITCH_Bottom;
      XLevel->Sides[sidenum].BottomTexture = sw->Frames[0].Texture;
    } else {
      continue;
    }

    bool PlaySound;
    if (useAgain || sw->NumFrames > 1) {
      PlaySound = StartButton(sidenum, where, i, DefaultSound, useAgain);
    } else {
      PlaySound = true;
    }

    if (PlaySound) {
      SectorStartSound(XLevel->Sides[sidenum].Sector, (sw->Sound ? sw->Sound : GSoundManager->GetSoundID(DefaultSound)), 0, 1, 1);
    }
    Quest = sw->Quest;
    return true;
  }
  Quest = false;
  return false;
}


//==========================================================================
//
//  VLevelInfo::StartButton
//
//  start a button counting down till it turns off
//  FIXME: make this faster!
//
//==========================================================================
bool VLevelInfo::StartButton (int sidenum, vuint8 w, int SwitchDef, VName DefaultSound, bool UseAgain) {
  // see if button is already pressed
  for (TThinkerIterator<VThinkButton> Btn(XLevel); Btn; ++Btn) {
    if (Btn->Side == sidenum) {
      // force advancing to the next frame
      Btn->Timer = 0.001f;
      return false;
    }
  }

  VThinkButton *But = (VThinkButton *)XLevel->SpawnThinker(VThinkButton::StaticClass());
  But->Side = sidenum;
  But->Where = w;
  But->SwitchDef = SwitchDef;
  But->Frame = -1;
  But->DefaultSound = DefaultSound;
  But->UseAgain = (UseAgain ? 1 : 0);
  But->AdvanceFrame();
  return true;
}


//==========================================================================
//
//  VThinkButton::SerialiseOther
//
//==========================================================================
/*
void VThinkButton::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  vuint8 xver = 0;
  Strm << xver;
  Strm << STRM_INDEX(Side)
    << Where
    << STRM_INDEX(SwitchDef)
    << STRM_INDEX(Frame)
    << Timer;
}
*/

//==========================================================================
//
//  VThinkButton::Tick
//
//==========================================================================
void VThinkButton::Tick (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  // do buttons
  Timer -= DeltaTime;
  if (Timer <= 0.0f) {
    TSwitch *Def = Switches[SwitchDef];
    if (Frame == Def->NumFrames-1) {
      SwitchDef = Def->PairIndex;
      Def = Switches[Def->PairIndex];
      Frame = -1;
      Level->SectorStartSound(XLevel->Sides[Side].Sector,
        Def->Sound ? Def->Sound :
        GSoundManager->GetSoundID(DefaultSound), 0, 1, 1);
      UseAgain = 0;
    }

    bool KillMe = AdvanceFrame();
    if (Where == SWITCH_Middle) {
      XLevel->Sides[Side].MidTexture = Def->Frames[Frame].Texture;
    } else if (Where == SWITCH_Bottom) {
      XLevel->Sides[Side].BottomTexture = Def->Frames[Frame].Texture;
    } else {
      // texture_top
      XLevel->Sides[Side].TopTexture = Def->Frames[Frame].Texture;
    }
    if (KillMe) DestroyThinker();
  }
}


//==========================================================================
//
//  VThinkButton::AdvanceFrame
//
//==========================================================================
bool VThinkButton::AdvanceFrame () {
  ++Frame;
  bool Ret = false;
  TSwitch *Def = Switches[SwitchDef];
  if (Frame == Def->NumFrames-1) {
    if (UseAgain) {
      Timer = BUTTONTIME;
    } else {
      Ret = true;
    }
  } else {
    if (Def->Frames[Frame].RandomRange) {
      Timer = (Def->Frames[Frame].BaseTime+Random()*Def->Frames[Frame].RandomRange)/35.0f;
    } else {
      Timer = Def->Frames[Frame].BaseTime/35.0f;
    }
  }
  return Ret;
}
