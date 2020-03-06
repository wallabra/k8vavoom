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

enum {
  GAME_Doom    = 0x01,
  GAME_Heretic = 0x02,
  GAME_Hexen   = 0x04,
  GAME_Strife  = 0x08,
  GAME_Raven   = GAME_Heretic|GAME_Hexen,
  GAME_Chex    = 0x10,
  GAME_Any     = 0xff,
};

enum { MAX_DECORATE_TRANSLATIONS = 0xffff };
enum { MAX_BLOOD_TRANSLATIONS = 0xffff };

struct VLineSpecInfo {
  VStr Name;
  int Number;
};

void ReadLineSpecialInfos ();
void ProcessDecorateScripts ();
void ShutdownDecorate ();
void CompilerReportMemory ();

extern TArray<VLineSpecInfo> LineSpecialInfos;

// list of all classes that need to be limited in some way
extern TArray<VClass *> NumberLimitedClasses;

// GameInfo is not spawned yet, so keep them here
extern TArray<VDamageFactor> CustomDamageFactors;

// do not spawn classes from this set
// it is filled by decorate parser, and used to block spawning of known blood
extern TMapNC<VName, bool> BlockedSpawnSet;

extern bool decoIgnorePlayerSpeed;
