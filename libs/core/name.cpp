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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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

#include "core.h"


// ////////////////////////////////////////////////////////////////////////// //
TArray<VNameEntry *> VName::Names;
VNameEntry *VName::HashTable[VName::HASH_SIZE];
bool VName::Initialised;


// ////////////////////////////////////////////////////////////////////////// //
#define REGISTER_NAME(name)   { nullptr, NAME_##name, #name },
static VNameEntry AutoNames[] = {
#include "names.h"
};


// ////////////////////////////////////////////////////////////////////////// //
VStream &operator << (VStream &Strm, VNameEntry &E) {
  guard(operator VStream << VNameEntry);
  vuint8 Size;
  if (Strm.IsSaving()) Size = (vuint8)(VStr::Length(E.Name)+1);
  Strm << Size;
  Strm.Serialise(E.Name, Size);
  return Strm;
  unguard;
}


VNameEntry *AllocateNameEntry (const char *Name, vint32 Index, VNameEntry *HashNext) {
  guard(AllocateNameEntry);
  size_t Size = sizeof(VNameEntry)-NAME_SIZE+int(VStr::Length(Name))+1;
  VNameEntry *E = (VNameEntry *)Z_Malloc(Size);
  memset(E, 0, Size);
  VStr::Cpy(E->Name, Name);
  E->Index = Index;
  E->HashNext = HashNext;
  return E;
  unguard;
}


VName::VName (const char *Name, ENameFindType FindType) {
  guard(VName::VName);

  char NameBuf[NAME_SIZE+1];

  Index = NAME_None;
  // make sure name is valid
  if (!Name || !*Name) return;

  // map 'none' to 'None'
  if (VStr::Cmp(Name, "none") == 0) return;

  memset(NameBuf, 0, sizeof(NameBuf));
  size_t nlen = strlen(Name);
  if (nlen >= NAME_SIZE) nlen = NAME_SIZE-1;

  // copy name localy, make sure it's not longer than 64 characters
  if (FindType == AddLower8) {
    if (nlen > 8) nlen = 8;
    for (size_t i = 0; i < nlen; ++i) NameBuf[i] = VStr::ToLower(Name[i]);
    NameBuf[8] = 0;
  } else if (FindType == AddLower) {
    for (size_t i = 0; i < nlen; ++i) NameBuf[i] = VStr::ToLower(Name[i]);
  } else {
    memcpy(NameBuf, Name, nlen);
  }

  // search in cache
  int HashIndex = GetTypeHash(NameBuf)&(HASH_SIZE-1);
  VNameEntry *TempHash = HashTable[HashIndex];
  while (TempHash) {
    if (!VStr::Cmp(NameBuf, TempHash->Name)) {
      Index = TempHash->Index;
      break;
    }
    TempHash = TempHash->HashNext;
  }

  // add new name if not found
  if (!TempHash && FindType != Find) {
    Index = Names.Num();
    Names.Append(AllocateNameEntry(NameBuf, Index, HashTable[HashIndex]));
    HashTable[HashIndex] = Names[Index];
  }

  // map 'none' to 'None'
  //if (Index == NAME_none) Index = NAME_None;
  unguard;
}


bool VName::operator == (const VStr &s) const { return (Index == 0 ? (s.isEmpty() || s == "none" || s == "None") : (s == Names[Index]->Name)); }
bool VName::operator != (const VStr &s) const { return !(*this == s); }

bool VName::operator == (const char *s) const { return (Index == 0 ? (!s || !s[0] || VStr::Cmp(s, "none") == 0 || VStr::Cmp(s, "None") == 0) : (VStr::Cmp(s, Names[Index]->Name) == 0)); }
bool VName::operator != (const char *s) const { return !(*this == s); }


void VName::StaticInit () {
  guard(VName::StaticInit);
  // register hardcoded names
  for (int i = 0; i < (int)ARRAY_COUNT(AutoNames); ++i) {
    Names.Append(&AutoNames[i]);
    int HashIndex = GetTypeHash(AutoNames[i].Name)&(HASH_SIZE-1);
    AutoNames[i].HashNext = HashTable[HashIndex];
    HashTable[HashIndex] = &AutoNames[i];
  }
  // we are now initialised
  Initialised = true;
  unguard;
}


void VName::StaticExit () {
  guard(VName::StaticExit);
  for (int i = NUM_HARDCODED_NAMES; i < Names.Num(); ++i) Z_Free(Names[i]);
  Names.Clear();
  Initialised = false;
  unguard;
}
