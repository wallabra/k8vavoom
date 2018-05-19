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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**
//**  Hash template.
//**
//**************************************************************************
#ifndef VAVOOM_CORE_LIB_HASHFUNC
#define VAVOOM_CORE_LIB_HASHFUNC


// fnv
static __attribute((unused)) inline vuint32 fnvHashBufCI (const void *buf, size_t len) {
  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  vuint32 hash = 2166136261U; // fnv offset basis
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) {
    vuint32 ch = *s++;
    if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
    hash ^= ch;
    hash *= 16777619U; // 32-bit fnv prime
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// fnv
static __attribute((unused)) inline vuint32 fnvHashBuf (const void *buf, size_t len) {
  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  vuint32 hash = 2166136261U; // fnv offset basis
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) {
    hash ^= *s++;
    hash *= 16777619U; // 32-bit fnv prime
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// djb
static __attribute((unused)) inline vuint32 djbHashBufCI (const void *buf, size_t len) {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) {
    vuint32 ch = *s++;
    if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
    hash = ((hash<<5)+hash)+ch;
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// djb
static __attribute((unused)) inline vuint32 djbHashBuf (const void *buf, size_t len) {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) hash = ((hash<<5)+hash)+(*s++);
  return (hash ? hash : 1); // this is unlikely, but...
}


#endif
