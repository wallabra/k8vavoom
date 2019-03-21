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
static VName::VNameEntry *HashTableSpc[HASH_SIZE];


// ////////////////////////////////////////////////////////////////////////// //
#define REGISTER_NAME(name)   { nullptr, NAME_##name, 0, 0, -0x0fffffff, #name },
VName::VNameEntry VName::AutoNames[] = {
  { nullptr, NAME_none, 0, 0, -0x0fffffff, "" },
#include "names.h"
};


int VName::GetAutoNameCounter () {
  return (int)ARRAY_COUNT(AutoNames);
}


// ////////////////////////////////////////////////////////////////////////// //
int VName::AppendNameEntry (VNameEntry *e) {
  check(e);
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


static VName::VNameEntry *AllocateNameEntry (const char *Name, VName::VNameEntry *HashNext) {
  const int slen = int(VStr::Length(Name));
  size_t size = sizeof(VName::VNameEntry)-NAME_SIZE+slen+1;
  VName::VNameEntry *e = (VName::VNameEntry *)Z_Calloc(size);
  e->rc = -0x0fffffff; // "immutable" VStr flag
  e->length = slen;
  e->alloted = slen+1;
  if (Name && Name[0]) strcpy(e->Name, Name);
  e->HashNext = HashNext;
  return e;
}


// ////////////////////////////////////////////////////////////////////////// //
VName::VName (const char *Name, ENameFindType FindType) {
  Index = NAME_None;
  // make sure name is valid
  if (!Name || !Name[0]) return;

  //k8: `none` is a valid name, and it is not equal to `NAME_none`!
  // 'None' is not the same as 'none`!
  //if (VStr::Cmp(Name, "none") == 0) return;

  char NameBuf[NAME_SIZE+1];
  //memset(NameBuf, 0, sizeof(NameBuf));

  // copy name localy, make sure it's not longer than allowed name size
  if (FindType == AddLower8) {
    for (size_t i = 0; i < 8; ++i) {
      char ch = Name[i];
      NameBuf[i] = VStr::ToLower(ch);
      if (!ch) break;
    }
    NameBuf[8] = 0;
  } else {
    size_t nlen = strlen(Name);
    check(nlen > 0);
    if (nlen >= NAME_SIZE) nlen = NAME_SIZE;
    if (FindType == AddLower || FindType == FindLower) {
      for (size_t i = 0; i < nlen; ++i) NameBuf[i] = VStr::ToLower(Name[i]);
    } else {
      memcpy(NameBuf, Name, nlen);
    }
    NameBuf[nlen] = 0;
  }

  if (!Initialised) {
    // find in autonames
    for (size_t aidx = 1; aidx < ARRAY_COUNT(AutoNames); ++aidx) {
      if (VStr::Cmp(NameBuf, AutoNames[aidx].Name) == 0) {
        Index = (int)aidx;
        return;
      }
    }
    StaticInit();
  }

  VNameEntry **htbl = HashTable;
  for (const vuint8 *ss = (const vuint8 *)NameBuf; *ss; ++ss) if (*ss <= ' ') { htbl = HashTableSpc; break; }

  // search in cache
  vuint32 HashIndex = GetTypeHash(NameBuf)&(HASH_SIZE-1);
  VNameEntry *TempHash = htbl[HashIndex];
  while (TempHash) {
    if (VStr::Cmp(NameBuf, TempHash->Name) == 0) {
      Index = TempHash->Index;
      return;
    }
    TempHash = TempHash->HashNext;
  }

  // add new name if not found
  if (FindType != Find && FindType != FindLower) {
    VNameEntry *e = AllocateNameEntry(NameBuf, htbl[HashIndex]);
    Index = AppendNameEntry(e);
    htbl[HashIndex] = Names[Index];
  }
}


bool VName::operator == (const VStr &s) const {
  if (Index == NAME_None) return s.isEmpty();
  if (Initialised) {
    check(Index >= 0 && Index < (int)NamesCount);
    return (s == Names[Index]->Name);
  } else {
    check(Index >= 0 && Index < (int)ARRAY_COUNT(AutoNames));
    return (s == AutoNames[Index].Name);
  }
}


bool VName::operator == (const char *s) const {
  if (!s) s = "";
  if (Index == NAME_None) return (s[0] == 0);
  if (Initialised) {
    check(Index >= 0 && Index < (int)NamesCount);
    return (VStr::Cmp(s, Names[Index]->Name) == 0);
  } else {
    check(Index >= 0 && Index < (int)ARRAY_COUNT(AutoNames));
    return (VStr::Cmp(s, AutoNames[Index].Name) == 0);
  }
}


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


void VName::StaticInit () {
  if (!Initialised) {
    memset((void *)HashTable, 0, sizeof(HashTable));
    memset((void *)HashTableSpc, 0, sizeof(HashTableSpc));
    // register hardcoded names
    for (int i = 0; i < (int)ARRAY_COUNT(AutoNames); ++i) {
      // fixup name entry
      VNameEntry &e = AutoNames[i];
      check(e.rc == -0x0fffffff);
      e.length = VStr::Length(e.Name);
      e.alloted = e.length+1;
      AppendNameEntry(&AutoNames[i]);
      if (i) {
        vuint32 HashIndex = GetTypeHash(e.Name)&(HASH_SIZE-1);
        VNameEntry **htbl = HashTable;
        for (const vuint8 *ss = (const vuint8 *)e.Name; *ss; ++ss) if (*ss <= ' ') { htbl = HashTableSpc; break; }
        e.HashNext = htbl[HashIndex];
        htbl[HashIndex] = &AutoNames[i];
      } else {
        e.Index = 0;
      }
      check(e.Index == i);
    }
    // we are now initialised
    Initialised = true;
  }
}


void VName::DebugDumpHashStats () {
  unsigned bkUsed = 0, bkMax = 0;
  unsigned bkUsedSpc = 0, bkMaxSpc = 0;
  for (unsigned f = 0; f < HASH_SIZE; ++f) {
    const VNameEntry *e = HashTable[f];
    if (e) {
      ++bkUsed;
      unsigned emax = 0;
      while (e) {
        ++emax;
        e = e->HashNext;
      }
      if (bkMax < emax) bkMax = emax;
    }
    e = HashTableSpc[f];
    if (e) {
      ++bkUsedSpc;
      unsigned emax = 0;
      while (e) {
        ++emax;
        e = e->HashNext;
      }
      if (bkMaxSpc < emax) bkMaxSpc = emax;
    }
  }
  fprintf(stderr, "***VNAME: %u names (%u array entries allocated), bucket stats (used/max): %u/%u, spc:%u/%u\n", (unsigned)NamesCount, (unsigned)NamesAlloced, bkUsed, bkMax, bkUsedSpc, bkMaxSpc);
}


struct VNameAutoIniter {
  VNameAutoIniter () { VName::StaticInit(); }
  //~VNameAutoIniter () { VName::DebugDumpHashStats(); }
};

VNameAutoIniter vNameAutoIniter;

/*k8: there is no reason to do this
void VName::StaticExit () {
  for (int i = NUM_HARDCODED_NAMES; i < Names.Num(); ++i) Z_Free(Names[i]);
  Names.Clear();
  Initialised = false;
}
*/
