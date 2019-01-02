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
//**  INFO STRINGS
//**
//**************************************************************************
#include "gamedefs.h"
#include "infostr.h"


#define MAX_INFO_STRING  (1024)


//==========================================================================
//
//  Info_ValueForKey
//
//  Searches the string for the given key and returns the associated value,
//  or an empty string.
//
//==========================================================================
VStr Info_ValueForKey (const VStr &s, const VStr &key) {
  guard(Info_ValueForKey);
  if (s.IsEmpty() || key.IsEmpty()) return VStr();
  //if (s.Length() >= MAX_INFO_STRING) Host_Error("Info_ValueForKey: oversize infostring");

  int i = 0;
  if (s[i] == '\\') ++i;
  for (;;) {
    int Start = i;
    while (s[i] != '\\') {
      if (!s[i]) return VStr();
      ++i;
    }
    VStr pkey(s, Start, i-Start);
    ++i;

    Start = i;
    while (s[i] != '\\' && s[i]) ++i;

    if (!key.ICmp(pkey)) return VStr(s, Start, i-Start);

    if (!s[i]) return VStr();
    ++i;
  }
  unguard;
}


//==========================================================================
//
//  Info_RemoveKey
//
//==========================================================================
void Info_RemoveKey (VStr &s, const VStr &key) {
  guard(Info_RemoveKey);
  if (s.IsEmpty()) return;
  //if (s.Length() >= MAX_INFO_STRING) Host_Error("Info_RemoveKey: oversize infostring");
  if (strchr(*key, '\\')) { GCon->Logf("Can't use a key with a \\ (%s)", *key.quote()); return; }

  int i = 0;
  for (;;) {
    int start = i;
    if (s[i] == '\\') ++i;
    int KeyStart = i;
    while (s[i] != '\\') {
      if (!s[i]) return;
      ++i;
    }
    VStr pkey(s, KeyStart, i-KeyStart);
    ++i;

    int ValStart = i;
    while (s[i] != '\\' && s[i]) ++i;
    VStr value(s, ValStart, i-ValStart);

    if (!key.Cmp(pkey)) {
      s = VStr(s, 0, start)+VStr(s, i, s.Length()-i); // remove this part
      return;
    }

    if (!s[i]) return;
  }
  unguard;
}


//==========================================================================
//
//  Info_SetValueForKey
//
//  Changes or adds a key/value pair
//
//==========================================================================
void Info_SetValueForKey (VStr &s, const VStr &key, const VStr &value) {
  guard(Info_SetValueForKey);
  //if (s.Length() >= MAX_INFO_STRING) Host_Error("Info_SetValueForKey: oversize infostring");

  if (strchr(*key, '\\')) { GCon->Logf("Can't use keys with a \\ (%s)", *key.quote()); return;}
  if (strchr(*value, '\\')) { GCon->Logf("Can't use values with a \\ (%s)", *value.quote()); return; }

  if (strchr(*key, '\"')) { GCon->Logf("Can't use keys with a \" (%s)", *key.quote()); return;}
  if (strchr(*value, '\"')) { GCon->Logf("Can't use values with a \" (%s)", *value.quote()); return; }

  // this next line is kinda trippy
  VStr v = Info_ValueForKey(s, key);
  if (v.IsNotEmpty()) {
    // key exists, make sure we have enough room for new value, if we don't, don't change it!
    if (value.Length()-v.Length()+s.Length() > MAX_INFO_STRING) {
      GCon->Logf("Info string '%s' length exceeded (%s:%s)", *key.quote(), *v.quote(), *value.quote());
      return;
    }
  }

  Info_RemoveKey(s, key);
  if (value.IsEmpty()) return;

  VStr newi = VStr("\\")+key+"\\"+value;
  if (newi.Length()+s.Length() > MAX_INFO_STRING) {
    GCon->Logf("Info string '%s' length exceeded: (%s:%s)", *key.quote(), *s.quote(), *newi.quote());
    return;
  }

  s = s+newi;
  unguard;
}
