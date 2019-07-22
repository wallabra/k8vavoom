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
//#include "mimalloc/mimalloc.h"
#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef VAVOOM_CORE_COUNT_ALLOCS
int zone_malloc_call_count = 0;
int zone_realloc_call_count = 0;
int zone_free_call_count = 0;
#endif

static bool zShuttingDown = false;

#ifdef VAVOOM_USE_MIMALLOC
# define malloc_fn   mi_malloc
# define realloc_fn  mi_realloc
# define free_fn     mi_free
#else
# define malloc_fn   malloc
# define realloc_fn  realloc
# define free_fn     free
#endif


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Malloc (size_t size) {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_malloc_call_count;
#endif
  void *res = malloc_fn(size > 0 ? size : size+1);
  if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
  memset(res, 0, size+(size ? 0 : 1)); // just in case
  return res;
}


__attribute__((alloc_size(2))) void *Z_Realloc (void *ptr, size_t size) {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_realloc_call_count;
#endif
  if (size) {
    void *res = realloc_fn(ptr, size);
    if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
    return res;
  } else {
    if (ptr) free_fn(ptr);
    return nullptr;
  }
}


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Calloc (size_t size) {
#if !defined(VAVOOM_USE_MIMALLOC)
  void *res = calloc(1, (size > 0 ? size : 1));
#else
  void *res = mi_zalloc(size > 0 ? size : 1);
#endif
  if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
#if !defined(VAVOOM_USE_MIMALLOC)
  memset(res, 0, size+(size ? 0 : 1)); // just in case
#endif
  return res;
}


void Z_Free (void *ptr) {
  if (zShuttingDown) return; // shitdoze hack
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_free_call_count;
#endif
  //fprintf(stderr, "Z_FREE! (%p)\n", ptr);
  if (ptr) free_fn(ptr);
}


// call this when exiting a thread function, to reclaim thread heaps
void Z_ThreadDone () {
#if defined(VAVOOM_USE_MIMALLOC)
  mi_thread_done();
#endif
}


void Z_ShuttingDown () {
  zShuttingDown = true;
}


__attribute__((noreturn)) void Z_Exit (int exitcode) {
  Z_ShuttingDown();
  exit(exitcode);
}


#ifdef __cplusplus
}
#endif


void *operator new (size_t size) noexcept(false) {
  //fprintf(stderr, "NEW: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}

void *operator new[] (size_t size) noexcept(false) {
  //fprintf(stderr, "NEW[]: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}

void operator delete (void *p) {
  if (zShuttingDown) return; // shitdoze hack
  //fprintf(stderr, "delete (%p)\n", p);
  Z_Free(p);
}


void operator delete[] (void *p) {
  if (zShuttingDown) return; // shitdoze hack
  //fprintf(stderr, "delete[] (%p)\n", p);
  Z_Free(p);
}
