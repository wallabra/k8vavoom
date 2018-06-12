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
#ifndef VCCMOD_INI_HEADER_FILE
#define VCCMOD_INI_HEADER_FILE

#include "../vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
class VIniFile : public VObject {
  DECLARE_CLASS(VIniFile, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VIniFile)

private:
  struct KVItem {
    VStr path;
    VStr key;
    VStr value;
  };

private:
  TArray<KVItem> items;

  bool loadFrom (VStream &strm);
  int findKey (const VStr &path, const VStr &key) const;
  void setKey (const VStr &path, const VStr &key, const VStr &value);

  // `path`: path from `KVItem`; `pat`: pattern
  static bool isPathEqu (const VStr &path, const VStr &pat);

  static bool write (VStream &strm, const VStr &s);
  static bool writeln (VStream &strm, const VStr &s);

  static void knsplit (const VStr &keyname, VStr &path, VStr &key);

public:
  virtual void Destroy () override;

public:
  bool load (const VStr &fname);
  bool save (const VStr &fname) const;
  void clear ();
  int count () const { return items.length(); }
  bool keyExists (const VStr &key) const;
  VStr keyAt (int idx) const;
  VStr get (const VStr &key) const;
  void set (const VStr &key, const VStr &value);
  void remove (const VStr &key);

  DECLARE_FUNCTION(load)
  DECLARE_FUNCTION(save)
  DECLARE_FUNCTION(clear)
  DECLARE_FUNCTION(count)
  DECLARE_FUNCTION(keyExists)
  DECLARE_FUNCTION(keyAt)
  DECLARE_FUNCTION(getValue)
  DECLARE_FUNCTION(setValue)
  DECLARE_FUNCTION(remove)

  DECLARE_FUNCTION(allPathes)
  DECLARE_FUNCTION(allKeys)
  DECLARE_FUNCTION(allKeysValues)

  friend class VIniPathIterator;
  friend class VIniKeyValueIterator;
};


// ////////////////////////////////////////////////////////////////////////// //
class VIniPathIterator : public VScriptIterator {
private:
  TArray<VStr> mItems;
  int mIndex;
  VStr *sptr;

public:
  VIniPathIterator (const VIniFile *aini, VStr *asptr);

  virtual ~VIniPathIterator () override;

  virtual bool GetNext () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VIniKeyValueIterator : public VScriptIterator {
private:
  struct KeyValue {
    VStr key;
    VStr value;
  };

  TArray<KeyValue> mItems;
  int mIndex;
  VStr *pkey;
  VStr *pvalue;

public:
  VIniKeyValueIterator (const VIniFile *aini, const VStr &path, VStr *apkey, VStr *apvalue);

  virtual ~VIniKeyValueIterator () override;

  virtual bool GetNext () override;
};


#endif
