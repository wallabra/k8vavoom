//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2020 Ketmar Dark
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
#ifndef WADCHECK_WDB_HEADER
#define WADCHECK_WDB_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"


VStr getBaseWad (const VStr &s);
VStr getLumpName (const VStr &s);
VStr normalizeName (const VStr &s);


struct FileHash {
  XXH64_hash_t hash;
  inline bool operator == (const FileHash &b) const { return (b.hash == hash); }
};

static VVA_OKUNUSED inline vuint32 GetTypeHash (const FileHash &fh) { return ((vuint32)fh.hash)^((vuint32)(fh.hash>>32)); }


struct LumpInfo {
  VStr name;
  vint32 size;
};


struct FileInfo {
  FileHash hash;
  TArray<LumpInfo> names;
};


extern TMapDtor<FileHash, FileInfo> wadlist;
// all known wad names
extern TArray<VStr> wadnames;
extern TMap<VStrCI, int> wadnamesmap;

struct WDBHitResult {
  VStr origName;
  VStr dbName;
};

extern TArray<WDBHitResult> wdbFindResults;


void wdbClear ();
void wdbAppend (XXH64_hash_t hash, VStr fname, vint32 size);

void wdbClearResults ();
void wdbSortResults ();
void wdbFind (XXH64_hash_t hash, VStr fname, vint32 size);

// save database
void wdbWrite (VStream *fo);
// load database
void wdbRead (VStream *fo);

bool wdbIsValid (VStream *fl);


#endif
