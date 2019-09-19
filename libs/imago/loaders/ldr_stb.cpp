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
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; version 3
//**  of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "../../core/core.h"
#include "../imago.h"


static void *X_Malloc (size_t size) {
  size += !size;
  void *res = ::malloc(size);
  if (!res) Sys_Error("out of memory!");
  return res;
}


static void X_Free (void *p) {
  if (p) ::free(p);
}


static void *X_Realloc (void *p, size_t size) {
  if (!size) {
    if (p) ::free(p);
    return nullptr;
  }
  p = ::realloc(p, size);
  if (!p) Sys_Error("out of memory!");
  return p;
}


#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_PNG  /* we have our own loader for this */
#define STBI_NO_TGA  /* we have our own loader for this */
#define STBI_NO_HDR  /* we aren't interested in this */
#define STBI_NO_PIC  /* we aren't interested in this */
#define STBI_MALLOC   X_Malloc
#define STBI_REALLOC  X_Realloc
#define STBI_FREE     X_Free
#include "stb_image.h"


// ////////////////////////////////////////////////////////////////////////// //
struct CBReadInfo {
  VStream *strm;
  int strmStart;
  int strmSize;
  int strmPos;
};


static const stbi_io_callbacks stbcbacks = {
  // fill 'data' with 'size' bytes; return number of bytes actually read
  .read = [](void *user, char *data, int size) -> int {
    if (size <= 0) return 0; // just in case
    CBReadInfo *nfo = (CBReadInfo *)user;
    int left = nfo->strmSize-nfo->strmPos;
    if (size > left) size = left;
    if (size) nfo->strm->Serialise(data, size);
    nfo->strmPos += size;
    return size;
  },
  // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
  .skip = [](void *user, int n) -> void {
    if (n == 0) return;
    CBReadInfo *nfo = (CBReadInfo *)user;
    n = nfo->strmPos+n; // ignore overflow, meh
    if (n < 0 || n > nfo->strmSize) abort(); // this should not happen
    nfo->strmPos = n;
    nfo->strm->Seek(n);
  },
  // returns nonzero if we are at end of file/data
  .eof = [](void *user) -> int {
    CBReadInfo *nfo = (CBReadInfo *)user;
    return !!(nfo->strmPos >= nfo->strmSize);
  },
};


// ////////////////////////////////////////////////////////////////////////// //
VImage *imagoLoadSTB (VStream *strm) {
  strm->Seek(0);

  CBReadInfo nfo;
  nfo.strm = strm;
  nfo.strmStart = 0;
  nfo.strmSize = strm->TotalSize();
  nfo.strmPos = 0;
  if (strm->IsError()) Sys_Error("error reading image from '%s'", *strm->GetName());

  int imgwidth = 0, imgheight = 0, imgchans = 0;
  vuint8 *data = (vuint8 *)stbi_load_from_callbacks(&stbcbacks, (void *)&nfo, &imgwidth, &imgheight, &imgchans, 4); // request RGBA
  if (strm->IsError()) {
    if (data) stbi_image_free(data);
    Sys_Error("error reading image from '%s'", *strm->GetName());
  }

  if (!data) return nullptr;

  VImage *res = new VImage(VImage::IT_RGBA, imgwidth, imgheight);
  // copy pixels
  const vuint8 *s = data;
  for (int y = 0; y < imgheight; ++y) {
    for (int x = 0; x < imgwidth; ++x, s += 4) {
      auto clr = VImage::RGBA(s[0], s[1], s[2], s[3]);
      res->setPixel(x, y, clr);
    }
  }

  // free memory
  stbi_image_free(data);

  return res;
}
