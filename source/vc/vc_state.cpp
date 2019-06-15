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
#include "vc_local.h"


//==========================================================================
//
//  VState::VState
//
//==========================================================================
VState::VState (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_State, AName, AOuter, ALoc)
  , Type(Vavoom)
  , TicType(TicKind::TCK_Normal)
  , SpriteName(NAME_None)
  , Frame(0)
  , Time(0)
  , Misc1(0)
  , Misc2(0)
  , Arg1(0)
  , Arg2(0)
  , NextState(nullptr)
  , Function(nullptr)
  , Next(nullptr)
  , GotoLabel(NAME_None)
  , GotoOffset(0)
  , FunctionName(NAME_None)
  , frameWidth(-1)
  , frameHeight(-1)
  , frameOfsX(0)
  , frameOfsY(0)
  , frameAction(0)
  , SpriteIndex(0)
  , InClassIndex(-1)
  , NetId(-1)
  , NetNext(nullptr)
  , LightInited(false)
  , LightDef(nullptr)
  , LightName(VStr())
{
}


//==========================================================================
//
//  VState::~VState
//
//==========================================================================
VState::~VState () {
}


//==========================================================================
//
//  VState::Serialise
//
//==========================================================================
void VState::Serialise (VStream &Strm) {
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  //if (!Strm.IsLoading()) ver = 0; // just in case
  Strm
    << STRM_INDEX(Type)
    << STRM_INDEX(TicType)
    << SpriteName
    << STRM_INDEX(Frame)
    << Time
    << STRM_INDEX(Misc1)
    << STRM_INDEX(Misc2)
    << STRM_INDEX(Arg1)
    << STRM_INDEX(Arg2)
    << STRM_INDEX(frameWidth)
    << STRM_INDEX(frameHeight)
    << STRM_INDEX(frameOfsX)
    << STRM_INDEX(frameOfsY)
    << STRM_INDEX(frameAction)
    << LightName
    << NextState
    << Function
    << Next;
  if (Strm.IsLoading()) {
    LightInited = false;
    LightDef = nullptr;
  }
}


//==========================================================================
//
//  VState::PostLoad
//
//==========================================================================
void VState::PostLoad () {
  SpriteIndex = (SpriteName != NAME_None ? VClass::FindSprite(SpriteName) : 1);
  NetNext = Next;
}


//==========================================================================
//
//  VState::Define
//
//==========================================================================
bool VState::Define () {
  bool Ret = true;
  if (Function && !Function->Define()) Ret = false;
  return Ret;
}


//==========================================================================
//
//  VState::Emit
//
//==========================================================================
void VState::Emit () {
  VEmitContext ec(this);
  if (GotoLabel != NAME_None) {
    //fprintf(stderr, "state `%s` label resolve: %s\n", *GetFullName(), *GotoLabel);
    NextState = ((VClass *)Outer)->ResolveStateLabel(Loc, GotoLabel, GotoOffset);
  }

  if (Function) {
    Function->Emit();
  } else if (FunctionName != NAME_None) {
    Function = ((VClass *)Outer)->FindMethod(FunctionName);
    if (!Function) {
      ParseError(Loc, "No such method `%s`", *FunctionName);
    } else {
      if (Function->ReturnType.Type != TYPE_Void) ParseError(Loc, "State method must not return a value");
      if (Function->NumParams) ParseError(Loc, "State method must not take any arguments");
      if (Function->Flags&FUNC_Static) ParseError(Loc, "State method must not be static");
      if (Function->Flags&FUNC_VarArgs) ParseError(Loc, "State method must not have varargs");
      if (Type == Vavoom) {
        if (!(Function->Flags&FUNC_Final)) ParseError(Loc, "State method must be final"); //k8: why?
      }
    }
  }
}


//==========================================================================
//
//  VState::IsInRange
//
//==========================================================================
bool VState::IsInRange (VState *Start, VState *End, int MaxDepth) {
  int Depth = 0;
  VState *check = Start;
  do {
    if (check == this) return true;
    if (check) check = check->Next;
    ++Depth;
  } while (Depth < MaxDepth && check != End);
  return false;
}


//==========================================================================
//
//  VState::IsInSequence
//
//==========================================================================
bool VState::IsInSequence (VState *Start) {
  for (VState *check = Start; check; check = (check->Next == check->NextState ? check->Next : nullptr)) {
    if (check == this) return true;
  }
  return false;
}


//==========================================================================
//
//  VState::GetPlus
//
//==========================================================================
VState *VState::GetPlus (int Offset, bool IgnoreJump) {
  check(Offset >= 0);
  VState *S = this;
  int Count = Offset;
  while (S && Count--) {
    if (!IgnoreJump && S->Next != S->NextState) return nullptr;
    S = S->Next;
  }
  return S;
}
