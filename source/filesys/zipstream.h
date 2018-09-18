//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**    $Id$
//**
//**    Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**    This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**    This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#ifndef _ZIPSTREAM_H
#define _ZIPSTREAM_H

#ifdef USE_INTERNAL_ZLIB
# include "../../libs/zlib/zlib.h"
#else
# include <zlib.h>
#endif


class VZipStreamReader : public VStream {
private:
  enum { BUFFER_SIZE = 65536 };

  VStream *SrcStream;
  Bytef Buffer[BUFFER_SIZE];
  z_stream ZStream;
  bool Initialised;
  vuint32 UncompressedSize;
  int srcStartPos;

private:
  void initialize ();

public:
  VZipStreamReader (VStream *ASrcStream, vuint32 AUncompressedSize=0xffffffff);
  VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize=0xffffffff);
  virtual ~VZipStreamReader () override;
  virtual const VStr &GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


class VZipStreamWriter : public VStream {
private:
  enum { BUFFER_SIZE = 65536 };

  VStream *DstStream;
  Bytef Buffer[BUFFER_SIZE];
  z_stream ZStream;
  bool Initialised;

public:
  VZipStreamWriter (VStream *, int clevel=6);
  virtual ~VZipStreamWriter () override;
  virtual const VStr &GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};


#endif
