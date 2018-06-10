//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018 Ketmar Dark
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

#include "mod_ini.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, IniFile);


// ////////////////////////////////////////////////////////////////////////// //
bool VIniFile::loadFrom (VStream &strm) {
  clear();
  if (!strm.IsLoading()) return false;
  int sz = strm.TotalSize()-strm.Tell();
  if (sz < 1) return true;
  if (sz > 1024*1024*8) return false;
  char* buf = new char[sz+1];
  strm.Serialize(buf, sz);
  if (strm.IsError()) { delete[] buf; return false; }
  buf[sz] = 0;
  for (int f = 0; f < sz; ++f) if (buf[f] == '\r') buf[f] = '\n';
  int pos = 0;
  VStr currPath;
  while (pos < sz) {
    while (pos < sz && (vuint8)buf[pos] <= ' ') ++pos;
    if (pos >= sz) break;
    int epos = pos;
    while (epos < sz && buf[epos] != '\n') ++epos;
    if (buf[pos] == ';' || buf[pos] == '\n') { pos = epos+1; continue; }
    if (buf[pos] == '/' && buf[pos+1] == '/') { pos = epos+1; continue; }
    // new path?
    if (buf[pos] == '[') {
      if (epos-pos < 2) { currPath = "<invalid>"; pos = epos+1; continue; }
      currPath = VStr(buf+pos+1, epos-pos-2);
      pos = epos+1;
      continue;
    }
    // new key
    int eqpos = pos;
    while (eqpos < epos && buf[eqpos] != '=') ++eqpos;
    if (eqpos >= epos) { pos = epos+1; continue; }
    VStr kn = VStr(buf+pos, eqpos-pos);
    while (kn.length() > 0 && (vuint8)kn[kn.length()-1] <= ' ') kn = kn.mid(0, (int)kn.length()-1);
    pos = eqpos+1;
    while (pos < epos && (vuint8)buf[pos] <= ' ') ++pos;
    VStr v;
    if (pos < epos) {
      v = VStr(buf+pos, epos-pos);
      while (v.length() > 0 && (vuint8)v[v.length()-1] <= ' ') v = v.mid(0, (int)v.length()-1);
    }
    setKey(currPath, kn, v);
    pos = epos+1;
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
// `path`: path from `KVItem`; `pat`: pattern
bool VIniFile::isPathEqu (const VStr &path, const VStr &pat) {
  const char *pp = *path;
  const char *ppat = *pat;
  while (*pp == '/') ++pp;
  while (*ppat == '/') ++ppat;
  while (*pp && *ppat) {
    if (*pp == '/' || *ppat == '/') {
      if (*pp == '/') {
        if (!ppat[0]) break;
        if (*ppat != '/') return false;
      } else {
        if (!pp[0]) break;
        if (*pp != '/') return false;
      }
      while (*pp == '/') ++pp;
      while (*ppat == '/') ++ppat;
      continue;
    }

    if (VStr::locase1251(*pp) != VStr::locase1251(*ppat)) return false;
    ++pp;
    ++ppat;
  }
  while (*pp == '/') ++pp;
  while (*ppat == '/') ++ppat;
  return (pp[0] == 0 && ppat[0] == 0);
}


// ////////////////////////////////////////////////////////////////////////// //
int VIniFile::findKey (const VStr &path, const VStr &key) const {
  for (int f = 0; f < items.length(); ++f) {
    if (!path.equ1251CI(items[f].path)) continue;
    if (!key.equ1251CI(items[f].key)) continue;
    return f;
  }
  return -1;
}

void VIniFile::setKey (const VStr &path, const VStr &key, const VStr &value) {
  int lastOurPath = -1;
  for (int f = 0; f < items.length(); ++f) {
    if (!path.equ1251CI(items[f].path)) continue;
    lastOurPath = f;
    if (key.equ1251CI(items[f].key)) { items[f].value = value; return; }
  }
  KVItem it;
  it.path = path;
  it.key = key;
  it.value = value;
  if (lastOurPath < 0 || lastOurPath+1 >= items.length()) { items.Append(it); return; }
  items.Insert(lastOurPath+1, it);
}


bool VIniFile::load (const VStr &fname) {
  VStream *st = fsysOpenFile(fname);
  if (!st) return false;
  auto res = loadFrom(*st);
  delete st;
  return res;
}


bool VIniFile::write (VStream &strm, const VStr &s) {
  if (strm.IsLoading()) return false;
  if (s.length()) strm.Serialize((void *)*s, s.length());
  return !strm.IsError();
}


bool VIniFile::writeln (VStream &strm, const VStr &s) {
  if (!write(strm, s)) return false;
  char eol = '\n';
  strm.Serialize(&eol, 1);
  return !strm.IsError();
}


bool VIniFile::save (const VStr &fname) const {
  VStream *st = fsysOpenDiskFileWrite(fname);
  if (!st) return false;
  for (int f = 0; f < items.length(); ++f) {
    if (items[f].path.length() == 0) {
      if (!write(*st, items[f].key)) { delete st; return false; }
      if (!write(*st, "=")) { delete st; return false; }
      if (!writeln(*st, items[f].value)) { delete st; return false; }
    }
  }
  VStr currPath;
  for (int f = 0; f < items.length(); ++f) {
    if (items[f].path.length() == 0) continue;
    if (!items[f].path.equ1251CI(currPath)) {
      currPath = items[f].path;
      if (!write(*st, "[")) { delete st; return false; }
      if (!write(*st, currPath)) { delete st; return false; }
      if (!writeln(*st, "]")) { delete st; return false; }
    }
    if (!write(*st, items[f].key)) { delete st; return false; }
    if (!write(*st, "=")) { delete st; return false; }
    if (!writeln(*st, items[f].value)) { delete st; return false; }
  }
  delete st;
  return true;
}


void VIniFile::clear () {
  items.SetNum(0);
}


void VIniFile::knsplit (const VStr &keyname, VStr &path, VStr &key) {
  auto pos = keyname.length();
  while (pos > 0 && keyname[pos-1] != '/') --pos;
  path = keyname.mid(0, pos);
  key = keyname.mid(pos+1, (int)keyname.length()-(pos+1));
}


bool VIniFile::keyExists (const VStr &key) const {
  VStr p, k;
  knsplit(key, p, k);
  return (findKey(p, k) >= 0);
}


VStr VIniFile::keyAt (int idx) const {
  if (idx < 0 || idx >= items.length()) return VStr();
  if (items[idx].path.length() == 0) return items[idx].key;
  return items[idx].path+"/"+items[idx].key;
}


VStr VIniFile::get (const VStr &key) const {
  VStr p, k;
  knsplit(key, p, k);
  int idx = findKey(p, k);
  return (idx >= 0 ? items[idx].value : VStr());
}

void VIniFile::set (const VStr &key, const VStr &value) {
  VStr p, k;
  knsplit(key, p, k);
  setKey(p, k, value);
}


void VIniFile::remove (const VStr &key) {
  VStr p, k;
  knsplit(key, p, k);
  int idx = findKey(p, k);
  if (idx >= 0) items.RemoveIndex(idx);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VIniFile, load) {
  P_GET_STR(fname);

  VClass *iclass = VClass::FindClass("IniFile");
  if (iclass) {
    auto ifileo = VObject::StaticSpawnObject(iclass);
    auto ifile = (VIniFile *)ifileo;
    if (!ifile->load(fname)) { delete ifileo; ifileo = nullptr; }
    RET_REF((VObject *)ifileo);
  } else {
    RET_REF(nullptr);
  }
}

IMPLEMENT_FUNCTION(VIniFile, save) {
  P_GET_STR(fname);
  P_GET_SELF;
  RET_BOOL(Self->save(fname));
}

IMPLEMENT_FUNCTION(VIniFile, clear) {
  P_GET_SELF;
  Self->clear();
}

IMPLEMENT_FUNCTION(VIniFile, count) {
  P_GET_SELF;
  RET_INT(Self->count());
}

IMPLEMENT_FUNCTION(VIniFile, keyExists) {
  P_GET_STR(key);
  P_GET_SELF;
  RET_BOOL(Self->keyExists(key));
}

IMPLEMENT_FUNCTION(VIniFile, keyAt) {
  P_GET_INT(idx);
  P_GET_SELF;
  RET_STR(Self->keyAt(idx));
}

IMPLEMENT_FUNCTION(VIniFile, getValue) {
  P_GET_STR(key);
  P_GET_SELF;
  RET_STR(Self->get(key));
}

IMPLEMENT_FUNCTION(VIniFile, setValue) {
  P_GET_STR(value);
  P_GET_STR(key);
  P_GET_SELF;
  Self->set(key, value);
}

IMPLEMENT_FUNCTION(VIniFile, remove) {
  P_GET_STR(key);
  P_GET_SELF;
  Self->remove(key);
}


// ////////////////////////////////////////////////////////////////////////// //
VIniPathIterator::VIniPathIterator (const VIniFile *aini, VStr *asptr) : mItems(), mIndex(0), sptr(asptr) {
  if (aini) {
    TMap<VStr, bool> known;
    for (int f = 0; f < aini->items.length(); ++f) {
      VStr path = aini->items[f].path;
      VStr lwpath = path.toLowerCase1251();
      if (!known.has(lwpath)) {
        known.put(lwpath, true);
        mItems.append(path);
      }
    }
  }
}


VIniPathIterator::~VIniPathIterator () {
  if (sptr) sptr->clear();
  mItems.clear();
}


bool VIniPathIterator::GetNext () {
  if (sptr && mIndex < mItems.length()) {
    *sptr = mItems[mIndex++];
    return true;
  } else {
    if (sptr) sptr->clear();
    mItems.clear();
    return false;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
VIniKeyValueIterator::VIniKeyValueIterator (const VIniFile *aini, const VStr &path, VStr *apkey, VStr *apvalue)
  : mItems()
  , mIndex(0)
  , pkey(apkey)
  , pvalue(apvalue)
{
  if (aini) {
    for (int f = 0; f < aini->items.length(); ++f) {
      if (VIniFile::isPathEqu(aini->items[f].path, path)) {
        KeyValue kv;
        kv.key = aini->items[f].key;
        kv.value = aini->items[f].value;
        mItems.append(kv);
      } else {
        //fprintf(stderr, "<%s> != <%s>\n", *aini->items[f].path, *path);
      }
    }
  }
}


VIniKeyValueIterator::~VIniKeyValueIterator () {
  if (pkey) pkey->clear();
  if (pvalue) pvalue->clear();
  mItems.clear();
}


bool VIniKeyValueIterator::GetNext () {
  if ((pkey || pvalue) && mIndex < mItems.length()) {
    if (pkey) *pkey = mItems[mIndex].key;
    if (pvalue) *pvalue = mItems[mIndex].value;
    ++mIndex;
    return true;
  } else {
    if (pkey) pkey->clear();
    if (pvalue) pvalue->clear();
    mItems.clear();
    return false;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// native iterator allPathes (out string path);
IMPLEMENT_FUNCTION(VIniFile, allPathes) {
  P_GET_PTR(VStr, pstr);
  P_GET_SELF;
  RET_PTR(new VIniPathIterator(Self, pstr));
}

//native iterator allKeys (out string key, string path);
IMPLEMENT_FUNCTION(VIniFile, allKeys) {
  P_GET_STR(path);
  P_GET_PTR(VStr, pkey);
  P_GET_SELF;
  RET_PTR(new VIniKeyValueIterator(Self, path, pkey, nullptr));
}

//native iterator allKeysValues (out string key, out string value, string path);
IMPLEMENT_FUNCTION(VIniFile, allKeysValues) {
  P_GET_STR(path);
  P_GET_PTR(VStr, pvalue);
  P_GET_PTR(VStr, pkey);
  P_GET_SELF;
  RET_PTR(new VIniKeyValueIterator(Self, path, pkey, pvalue));
}
