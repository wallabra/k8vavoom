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
//**  Memory Allocation.
//**
//**************************************************************************

inline void *Z_Malloc (size_t size) {
  void *res = malloc(size > 0 ? size : size+1);
  if (!res) { fprintf(stdout, "FATAL: out of memory!\n"); *(int*)0 = 0; }
  return res;
}


inline void *Z_Realloc (void *ptr, size_t size) {
  if (size) {
    void *res = realloc(ptr, size);
    if (!res) { fprintf(stdout, "FATAL: out of memory!\n"); *(int*)0 = 0; }
    return res;
  } else {
    if (ptr) free(ptr);
    return nullptr;
  }
}


inline void *Z_Calloc (size_t size) {
  void *res = malloc(size > 0 ? size : size+1);
  if (!res) { fprintf(stdout, "FATAL: out of memory!\n"); *(int*)0 = 0; }
  memset(res, 0, size > 0 ? size : size+1);
  return res;
}


inline void Z_Free (void *ptr) {
  if (ptr) free(ptr);
}
