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
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "cmdlib.h"

namespace VavoomUtils {


//==========================================================================
//
//  isSlash
//
//==========================================================================
static inline bool isSlash (const char ch) {
#ifdef _WIN32
  return (ch == '/' || ch == '\\' || ch == ':');
#else
  return (ch == '/');
#endif
}

//==========================================================================
//
//  isAbsolutePath
//
//==========================================================================
static inline bool isAbsolutePath (const char *s) {
  if (!s || !s[0]) return false;
#ifdef _WIN32
  return (s[0] == '/' || s[0] == '\\' || s[1] == ':');
#else
  return (s[0] == '/');
#endif
}


//==========================================================================
//
//  Error
//
//==========================================================================
__attribute__((noreturn)) __attribute__((format(printf, 1, 2))) void Error (const char *error, ...) {
  va_list argptr;
  va_start(argptr, error);
  vfprintf(stderr, error, argptr);
  va_end(argptr);
  exit(1);
}


//==========================================================================
//
//  DefaultPath
//
//==========================================================================
void DefaultPath (char *path, size_t pathsize, const char *basepath) {
  static char temp[16384];
  if (isAbsolutePath(path)) return; // absolute path location
  if (pathsize > 8192) pathsize = 8192;
  if (strlen(path)+strlen(basepath) >= pathsize) Error("path too long");
  strcpy(temp, path);
  strcpy(path, basepath);
  strcat(path, temp);
}


//==========================================================================
//
//  DefaultExtension
//
//  if path doesn't have an .EXT, append extension
//  (extension should include the leading dot)
//
//==========================================================================
void DefaultExtension (char *path, size_t pathsize, const char *extension) {
  if (!extension || !extension[0]) return;
  size_t plen = strlen(path);
  char *src = path+plen-1;
  while (src != path && !isSlash(*src)) {
    if (*src == '.') return; // it has an extension
    --src;
  }
  if (plen+strlen(extension)+1 > pathsize) Error("path too long");
  strcat(path, extension);
}


//==========================================================================
//
//  StripFilename
//
//==========================================================================
void StripFilename (char *path) {
  int length = int(strlen(path)-1);
  while (length > 0 && !isSlash(path[length])) --length;
  path[length] = 0;
}


//==========================================================================
//
//  StripExtension
//
//==========================================================================
void StripExtension (char *path) {
  char *search = path+strlen(path)-1;
  while (!isSlash(*search) && search != path) {
    if (*search == '.') {
      *search = 0;
      return;
    }
    --search;
  }
}


//==========================================================================
//
//  ExtractFilePath
//
//==========================================================================
void ExtractFilePath (const char *path, char *dest, size_t destsize) {
  const char *src = path+strlen(path)-1;
  // back up until a \ or the start
  while (src != path && !isSlash(src[-1])) --src;
  if ((size_t)(src-path)+1 > destsize) Error("path too long");
  memcpy(dest, path, src-path);
  dest[src-path] = 0;
}


//==========================================================================
//
//  ExtractFileBase
//
//==========================================================================
void ExtractFileBase (const char *path, char *dest, size_t destsize) {
  const char *src = path+strlen(path)-1;
  // back up until a \ or the start
  while (src != path && !isSlash(src[-1])) --src;
  while (*src && *src != '.') {
    if (destsize == 0) Error("path too long");
    *dest++ = *src++;
    --destsize;
  }
  if (destsize == 0) Error("path too long");
  *dest = 0;
}


//==========================================================================
//
//  ExtractFileExtension
//
//==========================================================================
void ExtractFileExtension (const char *path, char *dest, size_t destsize) {
  const char *src = path+strlen(path)-1;
  // back up until a . or the start
  while (src != path && src[-1] != '.') {
    if (isSlash(src[-1])) {
      if (destsize == 0) Error("path too long");
      *dest = 0; // no extension
      return;
    }
    --src;
  }
  if (src == path) {
    if (destsize == 0) Error("path too long");
    *dest = 0; // no extension
    return;
  }
  --src; // take dot
  if (strlen(src)+1 > destsize) Error("path too long");
  strcpy(dest, src);
}


//==========================================================================
//
//  FixFileSlashes
//
//==========================================================================
void FixFileSlashes (char *path) {
  while (*path) {
    if (*path == '\\') *path = '/';
    ++path;
  }
}


//==========================================================================
//
//  LoadFile
//
//==========================================================================
int LoadFile (const char *name, void **bufferptr) {
  FILE *f;
  int length;
  int count;
  void *buffer;

  f = fopen(name, "rb");
  if (!f) Error("Couldn't open file \"%s\".", name);

  fseek(f, 0, SEEK_END);
  length = ftell(f);
  fseek(f, 0, SEEK_SET);

  buffer = Z_Malloc(length);

  count = int(fread(buffer, 1, length, f));
  fclose (f);

  if (count != length) {
    Z_Free(buffer);
    Error("Couldn't read file \"%s\".", name);
  }

  *bufferptr = buffer;
  return length;
}

} // namespace VavoomUtils
