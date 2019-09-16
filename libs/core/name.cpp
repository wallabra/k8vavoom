//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"


// ////////////////////////////////////////////////////////////////////////// //
enum { HASH_SIZE = 32768 }; // ~6/4 per bucket

VName::VNameEntry **VName::Names = nullptr;
size_t VName::NamesAlloced = 0;
size_t VName::NamesCount = 0;
bool VName::Initialised = false;
static VName::VNameEntry *HashTable[HASH_SIZE];

// check alignment
static_assert(__builtin_offsetof(VName::VNameEntry, length)%8 == 0, "invalid vstr store emulation (alignment)");
static_assert(__builtin_offsetof(VName::VNameEntry, rc)%8 == 0, "invalid vstr store emulation (rc alignment)");

static_assert(__builtin_offsetof(VName::VNameEntry, Name)-__builtin_offsetof(VName::VNameEntry, length) == sizeof(VStr::Store), "invalid vstr store emulation (size)");
static_assert(__builtin_offsetof(VName::VNameEntry, length)-__builtin_offsetof(VName::VNameEntry, length) == __builtin_offsetof(VStr::Store, length), "invalid vstr store emulation (length field)");
static_assert(__builtin_offsetof(VName::VNameEntry, alloted)-__builtin_offsetof(VName::VNameEntry, length) == __builtin_offsetof(VStr::Store, alloted), "invalid vstr store emulation (alloted field)");
static_assert(__builtin_offsetof(VName::VNameEntry, rc)-__builtin_offsetof(VName::VNameEntry, length) == __builtin_offsetof(VStr::Store, rc), "invalid vstr store emulation (rc field)");


// ////////////////////////////////////////////////////////////////////////// //
/*
static int constexpr stlen (const char *s) { return (s && *s ? 1+stlen(s+1) : 0); }
*/


#define REGISTER_NAME(name)   { HashNext:nullptr, Index:NAME_##name, length:0/*stlen(#name)*/, alloted:0/*stlen(#name)+1*/, rc:-0x00ffffff, dummy:0, /*Name:*/ #name },
VName::VNameEntry VName::AutoNames[] = {
  { HashNext:nullptr, Index:NAME_none, length:0, alloted:1, rc:-0x00ffffff, dummy:0, /*Name:*/"" },
#include "names.h"
};


//==========================================================================
//
//  VName::GetAutoNameCounter
//
//==========================================================================
int VName::GetAutoNameCounter () {
  return (int)ARRAY_COUNT(AutoNames);
}


//==========================================================================
//
//  VName::AppendNameEntry
//
//==========================================================================
int VName::AppendNameEntry (VNameEntry *e) {
  vassert(e);
  if (NamesCount >= NamesAlloced) {
    if (NamesAlloced > 0x1fffffff) Sys_Error("too many names");
    size_t newsz = ((NamesCount+1)|0x3fffu)+1;
    //fprintf(stderr, "VName::AppendNameEntry: going from %u to %u\n", (unsigned)NamesAlloced, (unsigned)newsz);
    Names = (VNameEntry **)Z_Realloc(Names, newsz*sizeof(VNameEntry *));
    NamesAlloced = newsz;
  }
  int res = (int)NamesCount;
  Names[NamesCount++] = e;
  e->Index = res;
  //fprintf(stderr, "VName::AppendNameEntry: added <%s> (index=%d)\n", e->Name, res);
  return res;
}


//==========================================================================
//
//  AllocateNameEntry
//
//==========================================================================
static VName::VNameEntry *AllocateNameEntry (const char *Name, VName::VNameEntry *HashNext) {
  const int slen = int(VStr::Length(Name));
  size_t size = sizeof(VName::VNameEntry)-(PREDEFINED_NAME_SIZE+1)+slen+1;
  VName::VNameEntry *e = (VName::VNameEntry *)Z_Calloc(size);
  e->/*vstr.*/rc = -0x00ffffff; // "immutable" VStr flag
  e->/*vstr.*/length = slen;
  e->/*vstr.*/alloted = slen+1;
  if (Name && Name[0]) strcpy(e->Name, Name);
  e->HashNext = HashNext;
  return e;
}


//==========================================================================
//
//  VName::VName
//
//==========================================================================
VName::VName (const char *Name, ENameFindType FindType) {
  Index = NAME_None;
  // make sure name is valid
  if (!Name || !Name[0]) return;

  //k8: `none` is a valid name, and it is not equal to `NAME_none`!
  // 'None' is not the same as 'none`!
  //if (VStr::Cmp(Name, "none") == 0) return;

  char NameBuf[NAME_SIZE+1];
  size_t nlen;
  //memset(NameBuf, 0, sizeof(NameBuf));

  // copy name localy, make sure it's not longer than allowed name size
  if (FindType == AddLower8 || FindType == FindLower8) {
    for (size_t i = 0; i < 8; ++i) {
      char ch = Name[i];
      NameBuf[i] = VStr::ToLower(ch);
      if (!ch) break;
    }
    NameBuf[8] = 0;
    nlen = strlen(NameBuf);
    vassert(nlen > 0 && nlen <= 8);
  } else {
    nlen = strlen(Name);
    vassert(nlen > 0);
    if (nlen > NAME_SIZE) nlen = NAME_SIZE;
    memcpy(NameBuf, Name, nlen);
    NameBuf[nlen] = 0;
    if (FindType == AddLower || FindType == FindLower) {
      char *nbtmp = NameBuf;
      for (size_t i = nlen; i--; ++nbtmp) *nbtmp = VStr::ToLower(*nbtmp);
      //for (size_t i = 0; i < nlen; ++i) NameBuf[i] = VStr::ToLower(Name[i]);
      //if (FindType == FindLower8) NameBuf[8] = 0; // shrink it
    }
  }

  if (!Initialised) {
    // find in autonames
    if (nlen <= PREDEFINED_NAME_SIZE) {
      for (size_t aidx = 1; aidx < ARRAY_COUNT(AutoNames); ++aidx) {
        if (VStr::Cmp(NameBuf, AutoNames[aidx].Name) == 0) {
          Index = (int)aidx;
          return;
        }
      }
    }
    StaticInit();
  }

  // search in cache
  vuint32 HashIndex = GetTypeHash(NameBuf)&(HASH_SIZE-1);
  VNameEntry *TempHash = HashTable[HashIndex];
  while (TempHash) {
    if (nlen == (unsigned)TempHash->length && VStr::Cmp(NameBuf, TempHash->Name) == 0) {
      Index = TempHash->Index;
      return;
    }
    TempHash = TempHash->HashNext;
  }

  // add new name if not found
  if (FindType != Find && FindType != FindLower && FindType != FindLower8) {
    VNameEntry *e = AllocateNameEntry(NameBuf, HashTable[HashIndex]);
    Index = AppendNameEntry(e);
    HashTable[HashIndex] = Names[Index];
  }
}


//==========================================================================
//
//  VName::operator ==
//
//==========================================================================
bool VName::operator == (const VStr &s) const {
  if (Index == NAME_None) return s.isEmpty();
  if (Initialised) {
    vassert(Index >= 0 && Index < (int)NamesCount);
    return (s == Names[Index]->Name);
  } else {
    vassert(Index >= 0 && Index < (int)ARRAY_COUNT(AutoNames));
    return (s == AutoNames[Index].Name);
  }
}


//==========================================================================
//
//  VName::operator ==
//
//==========================================================================
bool VName::operator == (const char *s) const {
  if (!s) s = "";
  if (Index == NAME_None) return (s[0] == 0);
  if (Initialised) {
    vassert(Index >= 0 && Index < (int)NamesCount);
    return (VStr::Cmp(s, Names[Index]->Name) == 0);
  } else {
    vassert(Index >= 0 && Index < (int)ARRAY_COUNT(AutoNames));
    return (VStr::Cmp(s, AutoNames[Index].Name) == 0);
  }
}


//==========================================================================
//
//  VName::SafeString
//
//==========================================================================
const char *VName::SafeString (EName N) {
  if (N == NAME_None) return "";
  if (!Initialised) {
    if (N > NAME_None && N < (int)ARRAY_COUNT(AutoNames)) return AutoNames[N].Name;
    return "*VName::Uninitialised*";
  } else {
    if (N < 0 || N >= (int)NamesCount) return "*VName::Uninitialised*";
    return Names[N]->Name;
  }
}


//==========================================================================
//
//  VName::getCStr
//
//  won't abort on invalid index
//
//==========================================================================
const char *VName::getCStr () const {
  if (Initialised) {
    if (Index >= 0 && Index < (int)NamesCount) return Names[Index]->Name;
  } else {
    if (Index >= 0 && Index < GetAutoNameCounter()) return AutoNames[Index].Name;
  }
  return va("<invalid name index %d>", Index);
}


//==========================================================================
//
//  VName::StaticInit
//
//==========================================================================
void VName::StaticInit () {
  if (!Initialised) {
    memset((void *)HashTable, 0, sizeof(HashTable));
    // register hardcoded names
    for (int i = 0; i < (int)ARRAY_COUNT(AutoNames); ++i) {
      // fixup name entry
      VNameEntry &e = AutoNames[i];
      //if (e.rc != -0x00ffffff) { printf("fuck0! <%s> (%d : %d)\n", e.Name, e.rc, -0x00ffffff); }
      //if (e.length != VStr::Length(e.Name)) { printf("fuck1! <%s> (%d : %d)\n", e.Name, e.length, VStr::Length(e.Name)); }
      //if (e.alloted != VStr::Length(e.Name)+1) { printf("fuck2! <%s> (%d : %d)\n", e.Name, e.alloted, VStr::Length(e.Name)+1); }
      e./*vstr.*/length = VStr::Length(e.Name);
      e./*vstr.*/alloted = e./*vstr.*/length+1;
      vassert(e./*vstr.*/rc == -0x00ffffff);
      vassert(e./*vstr.*/length == VStr::Length(e.Name));
      vassert(e./*vstr.*/alloted == e./*vstr.*/length+1);
      vassert((i == 0 && e.length == 0) || (i != 0 && e.length > 0));
      AppendNameEntry(&AutoNames[i]);
      if (i) {
        vuint32 HashIndex = GetTypeHash(e.Name)&(HASH_SIZE-1);
        e.HashNext = HashTable[HashIndex];
        HashTable[HashIndex] = &AutoNames[i];
      } else {
        e.Index = 0;
      }
      vassert(e.Index == i);
    }
    // we are now initialised
    Initialised = true;
  }
}


//==========================================================================
//
//  VName::DebugDumpHashStats
//
//==========================================================================
void VName::DebugDumpHashStats () {
  unsigned bkUsed = 0, bkMax = 0;
  unsigned spacedNames = 0;
  double bkSum = 0.0;
  for (unsigned f = 0; f < HASH_SIZE; ++f) {
    const VNameEntry *e = HashTable[f];
    if (e) {
      ++bkUsed;
      unsigned emax = 0;
      while (e) {
        for (const vuint8 *s = (const vuint8 *)e->Name; *s; ++s) if (*s <= ' ') { ++spacedNames; break; }
        ++emax;
        e = e->HashNext;
      }
      bkSum += bkMax;
      if (bkMax < emax) bkMax = emax;
    }
  }
  fprintf(stderr, "***VNAME: %u names (%u array entries allocated), bucket stats (used/max/average): %u/%u/%g (names with spaces: %u)\n", (unsigned)NamesCount, (unsigned)NamesAlloced, bkUsed, bkMax, bkSum/(double)bkUsed, spacedNames);
}


struct VNameAutoIniter {
  VNameAutoIniter () { VName::StaticInit(); }
  //~VNameAutoIniter () { VName::DebugDumpHashStats(); }
};

__attribute__((used)) VNameAutoIniter vNameAutoIniter;

/*k8: there is no reason to do this
void VName::StaticExit () {
  for (int i = NUM_HARDCODED_NAMES; i < Names.Num(); ++i) Z_Free(Names[i]);
  Names.Clear();
  Initialised = false;
}
*/
