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
//**
//**  Switches, buttons. Two-state animation. Exits.
//**
//**************************************************************************
#include "gamedefs.h"


#define BUTTONTIME  (1.0f) /* 1 second */


enum EBWhere {
  SWITCH_Top,
  SWITCH_Middle,
  SWITCH_Bottom
};


//==================================================================
//
//      CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//
//==================================================================
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
  vint32 tbversion; // v1 stores more data
  VTextureID SwitchDefTexture;

  virtual void SerialiseOther (VStream &) override;
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

  for (int idx = Switches.length()-1; idx >= 0; --idx) {
    EBWhere where;
    TSwitch *sw = Switches[idx];

    if (texTop && sw->Tex == texTop) {
      where = SWITCH_Top;
      XLevel->Sides[sidenum].TopTexture = sw->Frames[0].Texture;
    } else if (texMid && sw->Tex == texMid) {
      where = SWITCH_Middle;
      XLevel->Sides[sidenum].MidTexture = sw->Frames[0].Texture;
    } else if (texBot && sw->Tex == texBot) {
      where = SWITCH_Bottom;
      XLevel->Sides[sidenum].BottomTexture = sw->Frames[0].Texture;
    } else {
      continue;
    }

    bool PlaySound;
    if (useAgain || sw->NumFrames > 1) {
      PlaySound = StartButton(sidenum, where, idx, DefaultSound, useAgain);
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
  if (!But) return false;
  if (But->IsA(VThinkButton::StaticClass())) {
    But->Side = sidenum;
    But->Where = w;
    But->SwitchDef = SwitchDef;
    But->Frame = -1;
    But->DefaultSound = DefaultSound;
    But->UseAgain = (UseAgain ? 1 : 0);
    But->tbversion = 1;
    But->SwitchDefTexture = Switches[SwitchDef]->Tex;
    But->AdvanceFrame();
    return true;
  } else {
    return false;
  }
}


//==========================================================================
//
//  VThinkButton::SerialiseOther
//
//==========================================================================
void VThinkButton::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (tbversion == 1) {
    VNTValueIOEx vio(&Strm);
    vio.io(VName("switch.texture"), SwitchDefTexture);
    if (Strm.IsLoading()) {
      bool found = false;
      for (int idx = Switches.length()-1; idx >= 0; --idx) {
        TSwitch *sw = Switches[idx];
        if (sw->Tex == SwitchDefTexture) {
          found = true;
          if (idx != SwitchDef) {
            GCon->Logf(NAME_Warning, "switch index changed from %d to %d (this may break the game!)", SwitchDef, idx);
          } else {
            //GCon->Logf("*** switch index %d found!", SwitchDef);
          }
          SwitchDef = idx;
          break;
        }
      }
      if (!found) {
        GCon->Logf(NAME_Error, "switch index for old index %d not found (this WILL break the game!)", SwitchDef);
        SwitchDef = -1;
      }
    }
  } else {
    GCon->Log(NAME_Warning, "*** old switch data found in save, this may break the game!");
  }
}


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
    if (SwitchDef >= 0 && SwitchDef < Switches.length()) {
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
      if (Side >= 0 && Side < XLevel->NumSides) {
        if (Where == SWITCH_Middle) {
          XLevel->Sides[Side].MidTexture = Def->Frames[Frame].Texture;
        } else if (Where == SWITCH_Bottom) {
          XLevel->Sides[Side].BottomTexture = Def->Frames[Frame].Texture;
        } else {
          // texture_top
          XLevel->Sides[Side].TopTexture = Def->Frames[Frame].Texture;
        }
      }
      if (KillMe) DestroyThinker();
    } else {
      UseAgain = 0;
      DestroyThinker();
    }
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
