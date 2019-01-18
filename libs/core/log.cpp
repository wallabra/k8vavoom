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
#include "core.h"


// ////////////////////////////////////////////////////////////////////////// //
VLog GLog;


//==========================================================================
//
//  VLog::VLog
//
//==========================================================================
VLog::VLog ()
  : Listeners(nullptr)
  , logbuf(nullptr)
  , logbufsize(0)
  , inWrite(false)
{
}


//==========================================================================
//
//  VLog::AddListener
//
//==========================================================================
void VLog::AddListener (VLogListener *lst) {
  if (!lst) return;
  if (inWrite) { fprintf(stderr, "FATAL: cannot add log listeners from log listener!\n"); abort(); }
  Listener *ls = (Listener *)malloc(sizeof(Listener));
  if (!ls) { fprintf(stderr, "FATAL: out of memory for log!\n"); abort(); }
  ls->ls = lst;
  ls->next = nullptr;
  if (!Listeners) {
    Listeners = ls;
  } else {
    Listener *curr = Listeners;
    while (curr->next) curr = curr->next;
    curr->next = ls;
  }
}


//==========================================================================
//
//  VLog::RemoveListener
//
//==========================================================================
void VLog::RemoveListener (VLogListener *lst) {
  if (!lst || !Listeners) return;
  if (inWrite) { fprintf(stderr, "FATAL: cannot remove log listeners from log listener!\n"); abort(); }
  Listener *lastCurr = nullptr, *lastPrev = nullptr;
  Listener *curr = Listeners, *prev = nullptr;
  for (; curr; prev = curr, curr = curr->next) {
    if (curr->ls == lst) {
      lastCurr = curr;
      lastPrev = prev;
    }
  }
  if (lastCurr) {
    // i found her!
    if (lastPrev) lastPrev = lastCurr->next; else Listeners = lastCurr->next;
    free(lastCurr);
  }
}


//==========================================================================
//
//  VLog::doWrite
//
//==========================================================================
void VLog::doWrite (EName Type, const char *fmt, va_list ap, bool addEOL) {
  if (!addEOL && (!fmt || !fmt[0])) return;
  if (!fmt) fmt = "";

  // initial allocation
  if (!logbufsize) {
    logbufsize = INITIAL_BUFFER_SIZE;
    logbuf = (char *)Z_Malloc(logbufsize);
  }

  va_list apcopy;

  va_copy(apcopy, ap);
  int size = vsnprintf(logbuf, (size_t)logbufsize, fmt, apcopy);
  va_end(apcopy);

  if (size < 0) return; // oops

  if (size >= logbufsize-2) {
    // not enough room, try again
    if (size > 0x1fffffff) abort(); // oops
    size = ((size+2)|0x1fff)+1;
    logbuf = (char *)realloc(logbuf, (size_t)logbufsize);
    if (!logbuf) { fprintf(stderr, "FATAL: out of memory for log!\n"); abort(); } //FIXME
    va_copy(apcopy, ap);
    size = vsnprintf(logbuf, (size_t)logbufsize, fmt, apcopy);
    va_end(apcopy);
    if (size < 0) return;
  }

  if (addEOL) { logbuf[size] = '\n'; logbuf[size+1] = 0; }

  inWrite = true;
  for (Listener *ls = Listeners; ls; ls = ls->next) {
    try {
      ls->ls->Serialise(logbuf, Type);
    } catch (...) {
    }
  }
  inWrite = false;
}


//==========================================================================
//
//  VLog::Write
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void VLog::Write (EName Type, const char *fmt, ...) {
  va_list ap;
  if (!fmt || !fmt[0]) return;
  va_start(ap, fmt);
  doWrite(Type, fmt, ap, false);
  va_end(ap);
}


//==========================================================================
//
//  VLog::WriteLine
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void VLog::WriteLine (EName Type, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  doWrite(Type, fmt, ap, true);
  va_end(ap);
}


//==========================================================================
//
//  VLog::Write
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VLog::Write (const char *fmt, ...) {
  va_list ap;
  if (!fmt || !fmt[0]) return;
  va_start(ap, fmt);
  doWrite(NAME_Log, fmt, ap, false);
  va_end(ap);
}


//==========================================================================
//
//  VLog::WriteLine
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VLog::WriteLine (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  doWrite(NAME_Log, fmt, ap, true);
  va_end(ap);
}


//==========================================================================
//
//  VLog::DWrite
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VLog::DWrite (const char *fmt, ...) {
  va_list ap;
  if (!fmt || !fmt[0]) return;
  va_start(ap, fmt);
  doWrite(NAME_Dev, fmt, ap, false);
  va_end(ap);
}


//==========================================================================
//
//  VLog::DWriteLine
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VLog::DWriteLine (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  doWrite(NAME_Dev, fmt, ap, true);
  va_end(ap);
}
