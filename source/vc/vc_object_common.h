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
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(IsA)
  DECLARE_FUNCTION(IsDestroyed)
  DECLARE_FUNCTION(GC_CollectGarbage)
  DECLARE_FUNCTION(get_GC_AliveObjects)
  DECLARE_FUNCTION(get_GC_LastCollectedObjects)
  DECLARE_FUNCTION(get_GC_LastCollectDuration)
  DECLARE_FUNCTION(get_GC_LastCollectTime)
  DECLARE_FUNCTION(get_GC_MessagesAllowed)
  DECLARE_FUNCTION(set_GC_MessagesAllowed)

#include "vc_object_rtti.h"

  // error functions
  DECLARE_FUNCTION(Error)
  DECLARE_FUNCTION(FatalError)
  DECLARE_FUNCTION(AssertError)

  // math functions
  DECLARE_FUNCTION(AngleMod360)
  DECLARE_FUNCTION(AngleMod180)
  DECLARE_FUNCTION(AngleVectors)
  DECLARE_FUNCTION(AngleVector)
  DECLARE_FUNCTION(VectorAngles)
  DECLARE_FUNCTION(VectorAngleYaw)
  DECLARE_FUNCTION(VectorAnglePitch)
  DECLARE_FUNCTION(AngleYawVector)
  DECLARE_FUNCTION(GetPlanePointZ)
  DECLARE_FUNCTION(GetPlanePointZRev)
  DECLARE_FUNCTION(PointOnPlaneSide)
  DECLARE_FUNCTION(PointOnPlaneSide2)
  DECLARE_FUNCTION(BoxOnLineSide2DV)
  DECLARE_FUNCTION(RotateDirectionVector)
  DECLARE_FUNCTION(VectorRotateAroundZ)
  DECLARE_FUNCTION(RotateVectorAroundVector)
  DECLARE_FUNCTION(IsPlainFloor)
  DECLARE_FUNCTION(IsPlainCeiling)
  DECLARE_FUNCTION(IsSlopedFlat)
  DECLARE_FUNCTION(IsVerticalPlane)

  DECLARE_FUNCTION(PlaneProjectPoint)

  DECLARE_FUNCTION(PlaneForPointDir)
  DECLARE_FUNCTION(PlaneForPointNormal)
  DECLARE_FUNCTION(PlaneForLine)

  // string functions
  DECLARE_FUNCTION(strlenutf8)
  DECLARE_FUNCTION(strcmp)
  DECLARE_FUNCTION(stricmp)
  DECLARE_FUNCTION(nameicmp)
  DECLARE_FUNCTION(namestricmp)
  //DECLARE_FUNCTION(strcat)
  DECLARE_FUNCTION(strlwr)
  DECLARE_FUNCTION(strupr)
  DECLARE_FUNCTION(substrutf8)
  DECLARE_FUNCTION(strmid)
  DECLARE_FUNCTION(strleft)
  DECLARE_FUNCTION(strright)
  DECLARE_FUNCTION(strrepeat)
  DECLARE_FUNCTION(strFromChar)
  DECLARE_FUNCTION(strFromCharUtf8)
  DECLARE_FUNCTION(strFromInt)
  DECLARE_FUNCTION(strFromFloat)
  DECLARE_FUNCTION(va)
  DECLARE_FUNCTION(atoi)
  DECLARE_FUNCTION(atof)
  DECLARE_FUNCTION(StrStartsWith)
  DECLARE_FUNCTION(StrEndsWith)
  DECLARE_FUNCTION(StrReplace)
  DECLARE_FUNCTION(globmatch)
  DECLARE_FUNCTION(strIndexOf)
  DECLARE_FUNCTION(strLastIndexOf)

  // random numbers
  DECLARE_FUNCTION(Random)
  DECLARE_FUNCTION(FRandomFull)
  DECLARE_FUNCTION(FRandomBetween)
  DECLARE_FUNCTION(P_Random)

  DECLARE_FUNCTION(GenRandomSeedU32)

  DECLARE_FUNCTION(bjprngSeed)
  DECLARE_FUNCTION(bjprngNext)
  DECLARE_FUNCTION(bjprngNextFloat)

  // printing in console
  DECLARE_FUNCTION(print)
  DECLARE_FUNCTION(dprint)
  DECLARE_FUNCTION(printwarn)

  // class methods
  DECLARE_FUNCTION(FindClass)
  DECLARE_FUNCTION(FindClassNoCase)
  DECLARE_FUNCTION(FindClassLowerCase)
  DECLARE_FUNCTION(ClassIsChildOf)
  DECLARE_FUNCTION(GetClassName)
  DECLARE_FUNCTION(GetClassParent)
  DECLARE_FUNCTION(GetClassReplacement)
  DECLARE_FUNCTION(GetCompatibleClassReplacement)
  DECLARE_FUNCTION(GetClassReplacee)
  DECLARE_FUNCTION(FindClassState)
  DECLARE_FUNCTION(GetClassNumOwnedStates)
  DECLARE_FUNCTION(GetClassFirstState)
  DECLARE_FUNCTION(GetClassGameObjName)

  // state methods
  DECLARE_FUNCTION(StateIsInRange)
  DECLARE_FUNCTION(StateIsInSequence)
  DECLARE_FUNCTION(GetStateSpriteName)
  DECLARE_FUNCTION(GetStateSpriteFrame)
  DECLARE_FUNCTION(GetStateSpriteFrameWidth)
  DECLARE_FUNCTION(GetStateSpriteFrameHeight)
  DECLARE_FUNCTION(GetStateSpriteFrameSize)
  DECLARE_FUNCTION(GetStateDuration)
  DECLARE_FUNCTION(GetStatePlus)
  DECLARE_FUNCTION(GetNextState)
  DECLARE_FUNCTION(GetNextStateInProg)
  DECLARE_FUNCTION(StateHasAction)
  DECLARE_FUNCTION(CallStateAction)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsX)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsY)
  DECLARE_FUNCTION(GetStateSpriteFrameOffset)
  DECLARE_FUNCTION(GetStateMisc1)
  DECLARE_FUNCTION(GetStateMisc2)
  DECLARE_FUNCTION(SetStateMisc1)
  DECLARE_FUNCTION(SetStateMisc2)
  DECLARE_FUNCTION(GetStateTicKind)
  DECLARE_FUNCTION(GetStateArgN)
  DECLARE_FUNCTION(SetStateArgN)
  DECLARE_FUNCTION(GetStateFRN)
  DECLARE_FUNCTION(SetStateFRN)

  // Iterators
  DECLARE_FUNCTION(AllObjects)
  DECLARE_FUNCTION(AllClasses)
  DECLARE_FUNCTION(AllClassStates)

  DECLARE_FUNCTION(SpawnObject)

  DECLARE_FUNCTION(GetInputKeyStrName)
  DECLARE_FUNCTION(GetInputKeyCode)

  DECLARE_FUNCTION(GetTimeOfDay)
  DECLARE_FUNCTION(DecodeTimeVal);
  DECLARE_FUNCTION(EncodeTimeVal);

  //DECLARE_FUNCTION(FindMObjId)
  //DECLARE_FUNCTION(FindScriptId)
  DECLARE_FUNCTION(FindClassByGameObjName)

  // cvar functions
  DECLARE_FUNCTION(CvarExists)
  DECLARE_FUNCTION(CvarGetFlags)
  DECLARE_FUNCTION(CreateCvar)
  DECLARE_FUNCTION(GetCvar)
  DECLARE_FUNCTION(SetCvar)
  DECLARE_FUNCTION(GetCvarF)
  DECLARE_FUNCTION(SetCvarF)
  DECLARE_FUNCTION(GetCvarS)
  DECLARE_FUNCTION(SetCvarS)
  DECLARE_FUNCTION(GetCvarB)
  DECLARE_FUNCTION(SetCvarB)
  DECLARE_FUNCTION(GetCvarHelp)
  DECLARE_FUNCTION(CvarUnlatchAll)

  DECLARE_FUNCTION(SetNamePutElement)
  DECLARE_FUNCTION(SetNameCheckElement)

  DECLARE_FUNCTION(RayLineIntersection2D)
  DECLARE_FUNCTION(RayLineIntersection2DDir)

  DECLARE_FUNCTION(PostEvent)
  DECLARE_FUNCTION(InsertEvent)
  DECLARE_FUNCTION(CountQueuedEvents)
  DECLARE_FUNCTION(PeekEvent)
  DECLARE_FUNCTION(GetEvent)
  DECLARE_FUNCTION(GetEventQueueSize)
