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
#ifndef FSYS_HEADER_FILE
#define FSYS_HEADER_FILE

#include "../../../libs/core/core.h"


// ////////////////////////////////////////////////////////////////////////// //
extern VStr fsysBaseDir; // always ends with "/" (fill be fixed by `fsysInit()` if necessary)
extern bool fsysDiskFirst; // default is true


// ////////////////////////////////////////////////////////////////////////// //
// `fsysBaseDir` should be set before calling this
void fsysInit ();
void fsysShutdown ();

// append disk directory to the list of archives
void fsysAppendDir (const VStr &path);

// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
bool fsysAppendPak (const VStr &fname);

// this will take ownership of `strm` (or kill it on error)
bool fsysAppendPak (VStream *strm);


bool fsysFileExists (const VStr &fname);
//void fsysCreatePath (const VStr &path);

// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFile (const VStr &fname);

// open file for reading, NOT relative to basedir
VStream *fsysOpenDiskFileWrite (const VStr &fname);
VStream *fsysOpenDiskFile (const VStr &fname);


// ////////////////////////////////////////////////////////////////////////// //
class FSysDriverBase {
protected:
  virtual const VStr &getNameByIndex (int idx) const = 0;
  virtual int getNameCount () const = 0;

protected:
  struct HashTableEntry {
    vuint32 hash; // name hash; name is lowercased
    vuint32 prev; // previous name with the same reduced hash position
    vuint32 didx; // dir index

    HashTableEntry () : hash(0), prev(0xffffffffU), didx(0xffffffffU) {}
  };

  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  static inline vuint32 fnvHashBufCI (const void *buf, size_t len) {
    vuint32 hash = 2166136261U; // fnv offset basis
    const vuint8 *s = (const vuint8 *)buf;
    while (len-- > 0) {
      hash ^= (vuint8)(VStr::locase1251(*s++));
      hash *= 16777619U; // 32-bit fnv prime
    }
    return (hash ? hash : 1); // this is unlikely, but...
  }

  static inline vuint32 fnvHashBufCI (const VStr &s) {
    if (s.length() == 0) return 1;
    return fnvHashBufCI(*s, s.length());
  }

protected:
  VStr mPrefix; // this can be used to open named paks
  vuint32 htableSize;
  HashTableEntry* htable; // for names, in reverse order; so name lookups will be faster
    // the algo is:
    //   htable[hashStr(name)%htable.length]: check if hash is ok, and name is ok
    //   if not ok, jump to htable[curht.prev], repeat

protected:
  // call this after you done building directory
  // never modify directory after that (or call `buildNameHashTable()` again)
  void buildNameHashTable ();

  // index or -1
  int findName (const VStr &fname) const;

protected:
  // should return `nullptr` on failure
  virtual VStream *open (int idx) const = 0;

public:
  FSysDriverBase () : mPrefix(VStr()), htableSize(0), htable(nullptr) {}
  virtual ~FSysDriverBase ();

  inline const VStr &getPrefix () const { return mPrefix; }

  virtual bool hasFile (const VStr &fname) const;

  // should return `nullptr` on failure
  virtual VStream *open (const VStr &fname) const;
};


// ////////////////////////////////////////////////////////////////////////// //
typedef FSysDriverBase* (*FSysOpenPakFn) (VStream *);

// loaders with higher priority will be tried first
void FSysRegisterDriver (FSysOpenPakFn ldr, int prio=1000);


// ////////////////////////////////////////////////////////////////////////// //
class VStreamDiskFile : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:
  void setError ();

public:
  VStreamDiskFile (FILE *afl, const VStr &aname=VStr(), bool asWriter=false);
  virtual ~VStreamDiskFile () noexcept(false) override;

  virtual const VStr &GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VPartialStreamReader : public VStream {
private:
  VStream *srcStream;
  int stpos;
  int srccurpos;
  int partlen;

private:
  void setError ();

public:
  // doesn't own stream
  VPartialStreamReader (VStream *ASrcStream, int astpos, int apartlen=-1);
  virtual ~VPartialStreamReader () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
#ifdef USE_INTERNAL_ZLIB
# include "../../../libs/zlib/zlib.h"
#else
# include <zlib.h>
#endif

class VZipStreamReader : public VStream {
private:
  enum { BUFFER_SIZE = 16384 };

  VStream *srcStream;
  int stpos;
  int srccurpos;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;
  vuint32 compressedSize;
  vuint32 uncompressedSize;
  int nextpos;
  int currpos;
  bool zipArchive;

private:
  void setError ();

  // just read, no `nextpos` advancement
  // returns number of bytes read, -1 on error, or 0 on EOF
  int readSomeBytes (void *buf, int len);

public:
  // doesn't own stream
  VZipStreamReader (VStream *ASrcStream, vuint32 ACompressedSize=0xffffffffU, vuint32 AUncompressedSize=0xffffffffU, bool asZipArchive=false);
  virtual ~VZipStreamReader () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


class VZipStreamWriter : public VStream {
private:
  enum { BUFFER_SIZE = 16384 };

  VStream *dstStream;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;

private:
  void setError ();

public:
  VZipStreamWriter (VStream *);
  virtual ~VZipStreamWriter () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
// returns -1 if not present
int fsysDiskFileTime (const VStr &path);
bool fsysDirExists (const VStr &path);
bool fysCreateDirectory (const VStr &path);

bool fsysOpenDir (const VStr &);
VStr fsysReadDir ();
void fsysCloseDir ();

// kinda like `GetTickCount()`, in seconds
double fsysCurrTick ();


#endif
