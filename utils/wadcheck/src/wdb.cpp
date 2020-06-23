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
#include "wdb.h"


TMapDtor<FileHash, FileInfo> wadlist;
// all known wad names
TArray<VStr> wadnames;
TMap<VStrCI, int> wadnamesmap;

TArray<WDBHitResult> wdbFindResults;
TMap<VStrCI, int> wdbResultsMap;


//==========================================================================
//
//  getBaseWad
//
//==========================================================================
VStr getBaseWad (const VStr &s) {
  auto cpos = s.lastIndexOf(':');
  if (cpos <= 0) return VStr::EmptyString;
  return s.left(cpos);
}


//==========================================================================
//
//  getLumpName
//
//==========================================================================
VStr getLumpName (const VStr &s) {
  auto cpos = s.lastIndexOf(':');
  if (cpos < 0) return s;
  return s.mid(cpos+1, s.length());
}


//==========================================================================
//
//  normalizeName
//
//==========================================================================
VStr normalizeName (const VStr &s) {
  VStr bw = getBaseWad(s);
  VStr ln = getLumpName(s);
  if (bw.length()) {
    bw = bw.ExtractFileBaseName();
    if (bw.length()) bw += ':';
  }
  return bw+ln;
}


//==========================================================================
//
//  wdbClear
//
//==========================================================================
void wdbClear () {
  wadlist.clear();
  wadnames.clear();
  wadnamesmap.clear();
}


//==========================================================================
//
//  wdbAppend
//
//==========================================================================
void wdbAppend (XXH64_hash_t hash, VStr fname, vint32 size) {
  fname = normalizeName(fname);
  // register in wad list
  int colpos = fname.indexOf(':');
  if (colpos > 0) {
    VStr wad = fname.left(colpos);
    if (!wadnamesmap.has(wad)) {
      wadnamesmap.put(wad, wadnames.length());
      wadnames.append(wad);
    }
  }
  FileHash fh;
  fh.hash = hash;
  auto fip = wadlist.find(fh);
  if (fip) {
    for (int f = 0; f < fip->names.length(); ++f) {
      if (fip->names[f].size != size) continue;
      if (fname.strEquCI(fip->names[f].name)) return;
    }
    LumpInfo li;
    li.name = fname;
    li.size = size;
    fip->names.append(li);
  } else {
    FileInfo fi;
    fi.hash = fh;
    LumpInfo li;
    li.name = fname;
    li.size = size;
    fi.names.append(li);
    wadlist.put(fh, fi);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static int ts_strcmp (const void *aa, const void *bb, void *) {
  VStr *a = (VStr *)aa;
  VStr *b = (VStr *)bb;
  return a->ICmp(*b);
}


static int wadres_cmp (const void *aa, const void *bb, void *) {
  const WDBHitResult *a = (const WDBHitResult *)aa;
  const WDBHitResult *b = (const WDBHitResult *)bb;
  return a->dbName.ICmp(b->dbName);
}


//==========================================================================
//
//  wdbClearResults
//
//==========================================================================
void wdbClearResults () {
  wdbFindResults.clear();
  wdbResultsMap.clear();
}


//==========================================================================
//
//  wdbSortResults
//
//==========================================================================
void wdbSortResults () {
  if (wdbFindResults.length() == 0) return;
  timsort_r(wdbFindResults.ptr(), wdbFindResults.length(), sizeof(wdbFindResults[0]), &wadres_cmp, nullptr);
  wdbResultsMap.reset();
  for (auto &&it : wdbFindResults.itemsIdx()) {
    wdbResultsMap.put(it.value().dbName, it.index());
  }
}


//==========================================================================
//
//  wdbFind
//
//==========================================================================
void wdbFind (XXH64_hash_t hash, VStr fname, vint32 size) {
  VStr origName = fname;
  fname = normalizeName(fname);
  FileHash fh;
  fh.hash = hash;
  auto fip = wadlist.find(fh);
  if (!fip) return;

  TArray<VStr> hits;
  for (int f = 0; f < fip->names.length(); ++f) {
    if (fip->names[f].size != size) continue;
    hits.append(VStr(fip->names[f].name));
  }
  if (hits.length() == 0) return;

  timsort_r(hits.ptr(), hits.length(), sizeof(hits[0]), &ts_strcmp, nullptr);

  // remove duplicates
  for (int f = 0; f < hits.length()-1; ) {
    if (hits[f].strEquCI(hits[f+1])) {
      hits.removeAt(f+1);
    } else {
      ++f;
    }
  }
  if (hits.length() == 0) return;

  for (auto &&s : hits) {
    if (!wdbResultsMap.has(s)) {
      wdbResultsMap.put(s, wdbFindResults.length());
      WDBHitResult &res = wdbFindResults.alloc();
      res.origName = origName;
      res.dbName = s;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static const char *DBSignature = "WLHDv0.1";


//==========================================================================
//
//  wdbWrite
//
//==========================================================================
void wdbWrite (VStream *fo) {
  TArray<VStr> strpool;
  TMap<VStr, int> spmap;

  for (auto it = wadlist.first(); it; ++it) {
    const FileInfo &fi = it.getValue();
    for (int f = 0; f < fi.names.length(); ++f) {
      VStr s = fi.names[f].name;
      VStr bw = getBaseWad(s); //.toLowerCase();
      auto ip = spmap.find(bw);
      if (!ip) {
        auto idx = strpool.length();
        strpool.append(bw);
        spmap.put(bw, idx);
      }
      bw = getLumpName(s); //.toLowerCase();
      ip = spmap.find(bw);
      if (!ip) {
        auto idx = strpool.length();
        strpool.append(bw);
        spmap.put(bw, idx);
      }
    }
  }

  fo->Serialise(DBSignature, 8);
  // write list of names
  vint32 nlen = strpool.length();
  *fo << STRM_INDEX(nlen);
  for (int f = 0; f < strpool.length(); ++f) *fo << strpool[f];
  // list of lumps
  vint32 count = wadlist.length();
  *fo << STRM_INDEX(count);
  for (auto it = wadlist.first(); it; ++it) {
    const FileInfo &fi = it.getValue();
    //fo->Serialise(&fi.hash.hash, (int)sizeof(fi.hash.hash));
    //XXH64_hash_t hash = fi.hash.hash;
    //*fo << hash;
    *fo << fi.hash.hash;
    // list of names and sizes
    vint32 ncount = fi.names.length();
    *fo << STRM_INDEX(ncount);
    for (int f = 0; f < fi.names.length(); ++f) {
      VStr s = fi.names[f].name;
      VStr bw = getBaseWad(s);
      auto ip = spmap.find(bw);
      vassert(ip);
      *fo << STRM_INDEX(*ip);
      VStr ln = getLumpName(s);
      ip = spmap.find(ln);
      vassert(ip);
      *fo << STRM_INDEX(*ip);
      // size
      vint32 size = fi.names[f].size;
      *fo << STRM_INDEX(size);
    }
  }
}


//==========================================================================
//
//  wdbRead
//
//==========================================================================
void wdbRead (VStream *fo) {
  char sign[8];
  fo->Serialise(sign, 8);
  if (memcmp(sign, DBSignature, 8) != 0) Sys_Error("invalid database signature");
  // read names
  vint32 scount = 0;
  *fo << STRM_INDEX(scount);
  if (fo->IsError()) Sys_Error("error reading database");
  if (scount < 0 || scount > 1024*1024*32) Sys_Error("database corrupted");
  TArray<VStr> strpool;
  strpool.setLength(scount);
  for (int f = 0; f < scount; ++f) {
    *fo << strpool[f];
    if (fo->IsError()) Sys_Error("error reading database");
  }
  // read list
  vint32 count = 0;
  *fo << STRM_INDEX(count);
  if (fo->IsError()) Sys_Error("error reading database");
  if (count < 0 || count > 1024*1024*32) Sys_Error("database corrupted");
  while (count--) {
    XXH64_hash_t hash;
    //fo->Serialise(&hash, (int)sizeof(hash));
    *fo << hash;
    if (fo->IsError()) Sys_Error("error reading database");
    vint32 ncount = 0;
    *fo << STRM_INDEX(ncount);
    if (fo->IsError()) Sys_Error("error reading database");
    if (ncount < 1 || ncount > 1024*1024) Sys_Error("database corrupted");
    while (ncount-- > 0) {
      vint32 bwidx, lnidx;
      *fo << STRM_INDEX(bwidx) << STRM_INDEX(lnidx);
      if (fo->IsError()) Sys_Error("error reading database");
      VStr bw = strpool[bwidx];
      VStr ln = strpool[lnidx];
      if (bw.length()) bw += ':';
      vint32 size;
      *fo << STRM_INDEX(size);
      if (fo->IsError()) Sys_Error("error reading database");
      wdbAppend(hash, bw+ln, size);
    }
  }
}
