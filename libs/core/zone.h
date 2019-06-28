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
#ifdef __cplusplus
extern "C" {
#endif

#ifdef VAVOOM_CORE_COUNT_ALLOCS
extern int zone_malloc_call_count;
extern int zone_realloc_call_count;
extern int zone_free_call_count;
#endif


// shitdoze, for some idiotic shitdoze reason, calls our `delete` on
// a thing it allocated with standard `new`. this causes segfault in mi-malloc.
// so call this function before returning from `main()`.
void Z_ShuttingDown ();


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Malloc (size_t size);

__attribute__((alloc_size(2)))
void *Z_Realloc (void *ptr, size_t size);

__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Calloc (size_t size);

void Z_Free (void *ptr);

// call this when exiting a thread function, to reclaim thread heaps
void Z_ThreadDone ();


#ifdef __cplusplus
}
#endif
