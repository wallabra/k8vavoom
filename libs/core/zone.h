//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
//**
//**  Memory Allocation.
//**
//**************************************************************************

extern int zone_malloc_call_count;
extern int zone_realloc_call_count;
extern int zone_free_call_count;


inline void *Z_Malloc (size_t size) {
  ++zone_malloc_call_count;
  void *res = malloc(size > 0 ? size : size+1);
  if (!res) Sys_Error("out of memory!");
  return res;
}


inline void *Z_Realloc (void *ptr, size_t size) {
  ++zone_realloc_call_count;
  if (size) {
    void *res = realloc(ptr, size);
    if (!res) Sys_Error("out of memory!");
    return res;
  } else {
    if (ptr) free(ptr);
    return nullptr;
  }
}


inline void *Z_Calloc (size_t size) {
  void *res = malloc(size > 0 ? size : size+1);
  if (!res) Sys_Error("out of memory!");
  memset(res, 0, size > 0 ? size : size+1);
  return res;
}


inline void Z_Free (void *ptr) {
  ++zone_free_call_count;
  if (ptr) free(ptr);
}
