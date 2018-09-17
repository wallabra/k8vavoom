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
//**
//**  Low-level OS-dependent functions
//**
//**************************************************************************

#include "core.h"

#ifndef WIN32
// normal OS
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>


struct DirInfo {
  DIR *dh;
  VStr path; // with slash
};


//==========================================================================
//
//  isDir
//
//==========================================================================
static bool isRegularFile (const VStr &filename) {
  struct stat st;
  if (filename.length() == 0) return false;
  if (stat(*filename, &st) == -1) return false;
  return (S_ISREG(st.st_mode) != 0);
}


//==========================================================================
//
//  Sys_FileExists
//
//==========================================================================
bool Sys_FileExists (const VStr &filename) {
  return (!filename.isEmpty() && access(*filename, R_OK) == 0 && isRegularFile(filename));
}


//==========================================================================
//
//  Sys_FileDelete
//
//==========================================================================
void Sys_FileDelete (const VStr &filename) {
  if (filename.length()) unlink(*filename);
}


//==========================================================================
//
//  Sys_FileTime
//
//  Returns -1 if not present
//
//==========================================================================
int Sys_FileTime (const VStr &path) {
  if (path.isEmpty()) return -1;
  struct stat buf;
  if (stat(*path, &buf) == -1) return -1;
  return (S_ISREG(buf.st_mode) ? buf.st_mtime : -1);
}


//==========================================================================
//
//  Sys_CreateDirectory
//
//==========================================================================
bool Sys_CreateDirectory (const VStr &path) {
  if (path.isEmpty()) return false;
  return (mkdir(*path, 0777) == 0);
}


//==========================================================================
//
//  Sys_OpenDir
//
//==========================================================================
void *Sys_OpenDir (const VStr &path) {
  if (path.isEmpty()) return nullptr;
  DIR *dh = opendir(*path);
  if (!dh) return nullptr;
  auto res = (DirInfo *)malloc(sizeof(DirInfo));
  if (!res) { closedir(dh); return nullptr; }
  memset((void *)res, 0, sizeof(DirInfo));
  res->dh = dh;
  res->path = path;
  if (res->path.length() == 0) res->path = "./";
  if (res->path[res->path.length()-1] != '/') res->path += "/";
  return (void *)res;
}


//==========================================================================
//
//  Sys_ReadDir
//
//==========================================================================
VStr Sys_ReadDir (void *adir) {
  if (!adir) return VStr();
  DirInfo *dh = (DirInfo *)adir;
  if (!dh->dh) return VStr();
  for (;;) {
    struct dirent *de = readdir(dh->dh);
    if (!de) break;
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
    if (!isRegularFile(dh->path+de->d_name)) continue;
    return de->d_name;
  }
  closedir(dh->dh);
  dh->dh = nullptr;
  dh->path.clear();
  return VStr();
}


//==========================================================================
//
//  Sys_CloseDir
//
//==========================================================================
void Sys_CloseDir (void *adir) {
  if (adir) {
    DirInfo *dh = (DirInfo *)adir;
    if (dh->dh) closedir(dh->dh);
    dh->path.clear();
    free((void *)dh);
  }
}


//==========================================================================
//
//  Sys_DirExists
//
//==========================================================================
bool Sys_DirExists (const VStr &path) {
  if (path.isEmpty()) return false;
  struct stat s;
  if (stat(*path, &s) == -1) return false;
  return !!S_ISDIR(s.st_mode);
}


//==========================================================================
//
//  Sys_Time
//
//==========================================================================
double Sys_Time () {
  struct timeval tp;
  struct timezone tzp;
  static int secbase = 0;

  gettimeofday(&tp, &tzp);
  if (!secbase) secbase = tp.tv_sec;
  return (tp.tv_sec-secbase)+tp.tv_usec/1000000.0;
}


//==========================================================================
//
//  Sys_Yield
//
//==========================================================================
void Sys_Yield () {
  //usleep(1);
  static const struct timespec sleepTime = {0, 28500000};
  nanosleep(&sleepTime, nullptr);
}


#else
// shitdoze

#include <signal.h>
#include <fcntl.h>
#define ftime fucked_ftime
#include <io.h>
#undef ftime
#include <direct.h>
#include <conio.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <windows.h>

#ifndef R_OK
# define R_OK  (4)
#endif


struct ShitdozeDir {
  HANDLE dir_handle;
  WIN32_FIND_DATA dir_buf;
  VStr path;
  bool gotName;
};


//==========================================================================
//
//  isDir
//
//==========================================================================
static bool isRegularFile (const VStr &filename) {
  if (filename.length() == 0) return false;
  DWORD attrs = GetFileAttributes(*filename);
  if (attrs == INVALID_FILE_ATTRIBUTES) return false;
  return ((attrs&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_DIRECTORY)) == 0);
}


//==========================================================================
//
//  Sys_FileExists
//
//==========================================================================
bool Sys_FileExists (const VStr &filename) {
  return (!filename.isEmpty() && access(*filename, R_OK) == 0 && isRegularFile(filename));
}


//==========================================================================
//
//  Sys_FileDelete
//
//==========================================================================
void Sys_FileDelete (const VStr &filename) {
  if (filename.length()) DeleteFile(*filename);
}


//==========================================================================
//
//  Sys_FileTime
//
//  Returns -1 if not present
//
//==========================================================================
int Sys_FileTime (const VStr &path) {
  if (path.isEmpty()) return -1;
  if (!isRegularFile(path)) return -1;
  struct stat buf;
  if (stat(*path, &buf) == -1) return -1;
  return buf.st_mtime;
}


//==========================================================================
//
//  Sys_CreateDirectory
//
//==========================================================================
bool Sys_CreateDirectory (const VStr &path) {
  if (path.isEmpty()) return false;
  return (mkdir(*path) == 0);
}


//==========================================================================
//
//  Sys_OpenDir
//
//==========================================================================
void *Sys_OpenDir (const VStr &dirname) {
  if (dirname.isEmpty()) return nullptr;
  auto sd = (ShitdozeDir *)malloc(sizeof(ShitdozeDir));
  if (!sd) return nullptr;
  memset((void *)sd, 0, sizeof(ShitdozeDir));
  VStr path = dirname;
  if (path.length() == 0) path = "./";
  if (path[path.length()-1] != '/' && path[path.length()-1] != '\\' && path[path.length()-1] != ':') path += "/";
  sd->dir_handle = FindFirstFile(va("%s*.*", *path), &sd->dir_buf);
  if (sd->dir_handle == INVALID_HANDLE_VALUE) { free(sd); return nullptr; }
  sd->gotName = true;
  sd->path = path;
  return (void *)sd;
}


//==========================================================================
//
//  Sys_ReadDir
//
//==========================================================================
VStr Sys_ReadDir (void *adir) {
  if (!adir) return VStr();
  auto sd = (ShitdozeDir *)adir;
  for (;;) {
    if (!sd->gotName) {
      if (FindNextFile(sd->dir_handle, &sd->dir_buf) != TRUE) return VStr();
    }
    sd->gotName = false;
    auto res = VStr(sd->dir_buf.cFileName);
    if (res != "." && res != "..") return res;
    if (isRegularFile(sd->path+res)) return res;
  }
}


//==========================================================================
//
//  Sys_CloseDir
//
//==========================================================================
void Sys_CloseDir (void *adir) {
  if (adir) {
    auto sd = (ShitdozeDir *)adir;
    FindClose(sd->dir_handle);
    sd->path.clear();
    free(sd);
  }
}


//==========================================================================
//
//  Sys_DirExists
//
//==========================================================================
bool Sys_DirExists (const VStr &path) {
  if (path.isEmpty()) return false;
  struct stat s;
  if (stat(*path, &s) == -1) return false;
  return !!(s.st_mode & S_IFDIR);
}


//==========================================================================
//
//  Sys_Time
//
//==========================================================================
double Sys_Time () {
  static double pfreq;
  static double curtime = 0.0;
  static double lastcurtime = 0.0;
  static vuint32 oldtime;
  static int lowshift;
  static bool initialized = false;

  if (!initialized) {
    LARGE_INTEGER PerformanceFreq;
    LARGE_INTEGER PerformanceCount;
    vuint32 lowpart, highpart;

    if (!QueryPerformanceFrequency(&PerformanceFreq)) Sys_Error("No hardware timer available");
    // get 32 out of the 64 time bits such that we have around
    // 1 microsecond resolution
    lowpart = (vuint32)PerformanceFreq.u.LowPart;
    highpart = (vuint32)PerformanceFreq.u.HighPart;
    lowshift = 0;
    while (highpart || lowpart > 2000000.0) {
      ++lowshift;
      lowpart >>= 1;
      lowpart |= (highpart&1)<<31;
      highpart >>= 1;
    }

    pfreq = 1.0 / (double)lowpart;

    // read current time and set old time
    QueryPerformanceCounter(&PerformanceCount);

    oldtime = ((vuint32)PerformanceCount.u.LowPart>>lowshift)|((vuint32)PerformanceCount.u.HighPart<<(32-lowshift));

    // set start time
    /*
    const char *p = GArgs.CheckValue("-starttime");
    if (p) {
      curtime = (double)atof(p);
    } else {
      curtime = 0.0;
    }
    */
    curtime = 0.0;
    lastcurtime = curtime;
    initialized = true;
  }

  static int sametimecount;
  LARGE_INTEGER PerformanceCount;
  vuint32 temp, t2;
  double time;

  QueryPerformanceCounter(&PerformanceCount);

  temp = ((unsigned int)PerformanceCount.u.LowPart>>lowshift)|((unsigned int)PerformanceCount.u.HighPart<<(32-lowshift));

  // check for turnover or backward time
  if (temp <= oldtime && oldtime-temp < 0x10000000) {
    oldtime = temp; // so we can't get stuck
  } else {
    t2 = temp-oldtime;

    time = (double)t2*pfreq;
    oldtime = temp;

    curtime += time;

    if (curtime == lastcurtime) {
      ++sametimecount;
      if (sametimecount > 100000) {
        curtime += 1.0;
        sametimecount = 0;
      }
    } else {
      sametimecount = 0;
    }
    lastcurtime = curtime;
  }

  return curtime;
}


//==========================================================================
//
//  Sys_Yield
//
//==========================================================================
void Sys_Yield () {
  Sleep(1);
}


#endif
