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

struct VAliasModelFrameInfo {
  VName sprite;
  int frame; // sprite frame
  int index; // monotonicaly increasing index
  int spriteIndex;

  VAliasModelFrameInfo () : sprite(NAME_None), frame(0), index(0), spriteIndex(0) {}
};


struct VLightEffectDef;

class VState : public VMemberBase {
public:
  // frame flags:
  // handles maximum brightness (torches, muzzle flare, light sources)
  enum {
    FF_FRAMEMASK = 0x7f,

    FF_FULLBRIGHT = 0x00080, // flag in Frame
    FF_CANRAISE   = 0x00100, // flag in Frame
    FF_DONTCHANGE = 0x00200, // keep old frame
    FF_SKIPOFFS   = 0x00400, // skip this state in offset calculation
    FF_SKIPMODEL  = 0x00800, // skip this state in model frame numbering
    FF_FAST       = 0x01000, // flag in Frame
    FF_SLOW       = 0x02000, // flag in Frame
    FF_KEEPSPRITE = 0x04000, // sprite number can be replaced, so we need to store this flag
  };

  enum { VaVoom, D2DF };

  enum TicKind {
    TCK_Normal,
    TCK_Random, // random(Arg1, Arg2)
  };

  // persistent fields
  vint32 Type;
  TicKind TicType;
  // state info
  VName SpriteName; // NAME_None: don't change
  vint32 Frame;
  float Time; // for d2df states: number of frames we should wait (ignore fractional part)
  vint32 Misc1;
  vint32 Misc2;
  vint32 Arg1;
  vint32 Arg2;
  VState *NextState;
  VMethod *Function;
  // linked list of states
  VState *Next;

  // compile time fields
  VName GotoLabel;
  vint32 GotoOffset;
  VName FunctionName;
  // <0: use texture size
  vint32 frameWidth;
  vint32 frameHeight;
  vint32 frameOfsX;
  vint32 frameOfsY;
  vint32 frameAction; // for d2df states: >0 means: do action on each nth frame

  // run-time fields
  vint32 SpriteIndex; // 1: don't change
  vint32 InClassIndex; // used by model rendering code (only)
  vint32 NetId;
  VState *NetNext;

  // inline light effect for state
  bool LightInited;
  VLightEffectDef *LightDef;
  VStr LightName;

  VState (VName AName, VMemberBase *AOuter, TLocation ALoc);
  virtual ~VState () override;

  virtual void Serialise (VStream &) override;
  virtual void PostLoad () override;

  bool Define ();
  void Emit ();
  bool IsInRange (VState *Start, VState *End, int MaxDepth);
  bool IsInSequence (VState *Start);
  VState *GetPlus (int Offset, bool IgnoreJump);

  inline VAliasModelFrameInfo getMFI () const {
    VAliasModelFrameInfo res;
    res.sprite = SpriteName;
    res.frame = Frame&FF_FRAMEMASK;
    res.index = InClassIndex;
    res.spriteIndex = 1; // unknown
    return res;
  }

  friend inline VStream &operator << (VStream &Strm, VState *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
