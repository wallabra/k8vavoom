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
//**  as published by the Free Software Foundation; either version 3
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "fsys.h"


// ////////////////////////////////////////////////////////////////////////// //
VStr fsysBaseDir = VStr("./"); // always ends with "/" (fill be fixed by `fsysInit()` if necessary)
bool fsysDiskFirst = true; // default is true


// ////////////////////////////////////////////////////////////////////////// //
enum { MaxOpenPaks = 1024 };

static FSysDriverBase *openPaks[MaxOpenPaks]; // 0 is always basedir
static int openPakCount = 0;


// ////////////////////////////////////////////////////////////////////////// //
//typedef FSysDriver* (*FSysOpenPakFn) (VStream *);

struct FSysDriverCreator {
  FSysOpenPakFn ldr;
  int prio;
  FSysDriverCreator *next;
};

static FSysDriverCreator *creators = nullptr;


void FSysRegisterDriver (FSysOpenPakFn ldr, int prio) {
  if (!ldr) return;

  FSysDriverCreator *prev = nullptr, *cur = creators;
  while (cur && cur->prio >= prio) {
    prev = cur;
    cur = cur->next;
  }

  auto it = new FSysDriverCreator;
  it->ldr = ldr;
  it->prio = prio;
  it->next = cur;

  if (prev) prev->next = it; else creators = it;
}


// ////////////////////////////////////////////////////////////////////////// //
FSysDriverBase::~FSysDriverBase () {
  delete htable;
  htable = nullptr;
  htableSize = 0;
}


void FSysDriverBase::buildNameHashTable () {
  delete htable;
  htable = nullptr;
  int dlen = getNameCount();
  htableSize = (vuint32)dlen;
  if (dlen == 0) return;
  htable = new HashTableEntry[dlen];
  for (int f = 0; f < dlen; ++f) htable[f] = HashTableEntry(); // just in case
  for (int idx = dlen-1; idx >= 0; --idx) {
    vuint32 nhash = fnvHashBufCI(getNameByIndex(idx)); // never zero
    vuint32 hidx = nhash%(vuint32)dlen;
    if (htable[hidx].didx == 0xffffffffU) {
      // first item
      htable[hidx].hash = nhash;
      htable[hidx].didx = (vuint32)idx;
      //assert(htable.ptr[hidx].prev == 0xffffffffU);
    } else {
      // chain
      while (htable[hidx].prev != 0xffffffffU) hidx = htable[hidx].prev;
      // find free slot
      vuint32 freeslot = hidx;
      for (vuint32 count = 0; count < (vuint32)dlen; ++count) {
        freeslot = (freeslot+1)%(vuint32)dlen;
        if (htable[freeslot].hash == 0) break; // i found her!
      }
      //if (htable.ptr[freeslot].hash != 0) assert(0, "wtf?!");
      htable[hidx].prev = freeslot;
      htable[freeslot].hash = nhash;
      htable[freeslot].didx = (vuint32)idx;
      //assert(htable.ptr[freeslot].prev == uint.max);
    }
  }
}


// index or -1
int FSysDriverBase::findName (const VStr &fname) const {
  vuint32 nhash = fnvHashBufCI(fname);
  vuint32 hidx = nhash%htableSize;
  while (hidx != 0xffffffffU && htable[hidx].hash != 0) {
    if (htable[hidx].hash == nhash) {
      vuint32 didx = htable[hidx].didx;
      if (getNameByIndex(didx).equ1251CI(fname)) return (int)didx;
    }
    hidx = htable[hidx].prev;
  }
  // alas, and it is guaranteed that we have no such file here
  return -1;
}


bool FSysDriverBase::hasFile (const VStr &fname) const {
  return (findName(fname) >= 0);
}


VStream *FSysDriverBase::open (const VStr &fname) const {
  int idx = findName(fname);
  if (idx < 0) return nullptr;
  return open(idx);
}


// ////////////////////////////////////////////////////////////////////////// //
class VStreamDiskReader : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

public:
  VStreamDiskReader (FILE *afl, const VStr &aname=VStr(), bool asWriter=false);
  virtual ~VStreamDiskReader () noexcept(false) override;

  virtual const VStr &GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};


VStreamDiskReader::VStreamDiskReader (FILE* afl, const VStr &aname, bool asWriter) : mFl(afl), mName(aname), size(-1) {
  if (afl) fseek(afl, 0, SEEK_SET);
  bLoading = !asWriter;
}

VStreamDiskReader::~VStreamDiskReader () noexcept(false) { Close(); }

const VStr &VStreamDiskReader::GetName () const { return mName; }

void VStreamDiskReader::Seek (int pos) {
  if (!mFl) { bError = true; return; }
  if (fseek(mFl, pos, SEEK_SET)) bError = true;
}

int VStreamDiskReader::Tell () { return (bError || !mFl ? 0 : ftell(mFl)); }

int VStreamDiskReader::TotalSize () {
  if (size < 0 && mFl && !bError) {
    auto opos = ftell(mFl);
    fseek(mFl, 0, SEEK_END);
    size = (int)ftell(mFl);
    fseek(mFl, opos, SEEK_SET);
  }
  return size;
}

bool VStreamDiskReader::AtEnd () { return (bError || !mFl || Tell() >= TotalSize()); }

bool VStreamDiskReader::Close () {
  if (mFl) { fclose(mFl); mFl = nullptr; }
  mName = VStr();
  return !bError;
}

void VStreamDiskReader::Serialise (void *buf, int len) {
  if (bError || !mFl || len < 0) { bError = true; return; }
  if (len == 0) return;
  if (bLoading) {
    if (fread(buf, len, 1, mFl) != 1) bError = true;
  } else {
    if (fwrite(buf, len, 1, mFl) != 1) bError = true;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
class FSysDriverDisk : public FSysDriverBase {
private:
  VStr path;

protected:
  virtual const VStr &getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;

protected:
  virtual VStream *open (int idx) const override;

public:
  FSysDriverDisk (const VStr &apath);
  virtual ~FSysDriverDisk () override;

  virtual bool hasFile (const VStr &fname) const;
  virtual VStream *open (const VStr &fname) const;
};


FSysDriverDisk::FSysDriverDisk (const VStr &apath) : FSysDriverBase() {
  path = apath;
  if (path.length() == 0) path = "./";
  if (!path.endsWith("/")) path += "/";
}

FSysDriverDisk::~FSysDriverDisk () {}

const VStr &FSysDriverDisk::getNameByIndex (int idx) const { *(int *)0 = 0; return VStr::EmptyString; } // the thing that should not be
int FSysDriverDisk::getNameCount () const { *(int *)0 = 0; return 0; } // the thing that should not be
VStream *FSysDriverDisk::open (int idx) const { *(int *)0 = 0; return nullptr; } // the thing that should not be

bool FSysDriverDisk::hasFile (const VStr &fname) const {
  if (fname.length() == 0) return false;
  VStr newname = path+fname;
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return false;
  fclose(fl);
  return true;
}

VStream *FSysDriverDisk::open (const VStr &fname) const {
  if (fname.length() == 0) return nullptr;
  VStr newname = path+fname;
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname);
}


// ////////////////////////////////////////////////////////////////////////// //
// `fsysBaseDir` should be set before calling this
void fsysInit () {
  if (openPakCount == 0) {
         if (fsysBaseDir.length() == 0) fsysBaseDir = VStr("./");
    else if (!fsysBaseDir.endsWith("/")) fsysBaseDir += "/"; // fuck you, shitdoze
    openPaks[0] = new FSysDriverDisk(fsysBaseDir);
    openPakCount = 1;
  }
}


void fsysShutdown () {
}


// ////////////////////////////////////////////////////////////////////////// //
// append disk directory to the list of archives
void fsysAppendDir (const VStr &path) {
  if (path.length() == 0) return;
  if (openPakCount >= MaxOpenPaks) Sys_Error("too many pak files");
  openPaks[openPakCount++] = new FSysDriverDisk(path);
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
bool fsysAppendPak (const VStr &fname) {
  if (fname.length() == 0) return false;
  FILE *fl = fopen(*fname, "rb");
  if (!fl) return false;
  return fsysAppendPak(new VStreamDiskReader(fl, fname));
}


// this will take ownership of `strm` (or kill it on error)
bool fsysAppendPak (VStream *strm) {
  if (!strm) return false;
  delete strm;
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
bool fsysFileExists (const VStr &fname) {
  if (openPakCount == 0) fsysInit();
  // try basedir first, if the corresponding flag is set
  if (fsysDiskFirst) {
    if (openPaks[0]->hasFile(fname)) return true;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (openPaks[f]->hasFile(fname)) return true;
  }
  // try basedir last, if the corresponding flag is set
  if (!fsysDiskFirst) {
    if (openPaks[0]->hasFile(fname)) return true;
  }
  return false;
}


// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFile (const VStr &fname) {
  if (openPakCount == 0) fsysInit();
  // try basedir first, if the corresponding flag is set
  if (fsysDiskFirst) {
    auto res = openPaks[0]->open(fname);
    if (res) return res;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    auto res = openPaks[f]->open(fname);
    if (res) return res;
  }
  // try basedir last, if the corresponding flag is set
  if (!fsysDiskFirst) {
    auto res = openPaks[0]->open(fname);
    if (res) return res;
  }
  return nullptr;
}


// open file for reading, NOT relative to basedir
VStream *fsysOpenDiskFileWrite (const VStr &fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "wb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname, true);
}


VStream *fsysOpenDiskFile (const VStr &fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname);
}
