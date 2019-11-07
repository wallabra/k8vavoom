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
//**  the Free Software Foundation, version 3 of the License ONLY.
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
#if !defined(_WIN32)
# include <unistd.h>
#endif


// ////////////////////////////////////////////////////////////////////////// //
VLog GLog;
#if !defined(_WIN32) || defined(IN_WADCHECK)
bool GLogTTYLog = true;
#else
bool GLogTTYLog = false;
#endif
bool GLogErrorToStderr = false;
bool GLogWarningToStderr = false;
#if defined(IN_WADCHECK)
bool GLogSkipLogTypeName = true;
#else
bool GLogSkipLogTypeName = false;
#endif

VLog::Listener *VLog::Listeners = nullptr;

mythread_mutex VLog::logLock;
volatile bool VLog::logLockInited = false;


//==========================================================================
//
//  InitLogLock
//
//==========================================================================
static inline void InitLogLock () noexcept {
  //WARNING! THIS IS NOT THREAD-SAFE!
  if (!VLog::logLockInited) {
    VLog::logLockInited = true;
    mythread_mutex_init(&VLog::logLock);
  }
}


//==========================================================================
//
//  VLog::VLog
//
//==========================================================================
VLog::VLog () noexcept
  : logbuf(nullptr)
  , logbufsize(0)
  , inWrite(false)
{
  // initial allocation, so we will have something to use on OOM
  logbufsize = INITIAL_BUFFER_SIZE;
  logbuf = (char *)Z_Malloc(logbufsize);
  if (!logbuf) { fprintf(stderr, "FATAL: out of memory for initial log buffer!\n"); abort(); } //FIXME
}


//==========================================================================
//
//  VLog::AddListener
//
//==========================================================================
void VLog::AddListener (VLogListener *lst) noexcept {
  if (!lst) return;
  InitLogLock();
  MyThreadLocker lock(&logLock);
  //if (inWrite) { fprintf(stderr, "FATAL: cannot add log listeners from log listener!\n"); abort(); }
  Listener *ls = (Listener *)Z_Malloc(sizeof(Listener));
  if (!ls) { fprintf(stderr, "FATAL: out of memory for log listener list!\n"); abort(); }
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
void VLog::RemoveListener (VLogListener *lst) noexcept {
  if (!lst || !Listeners) return;
  InitLogLock();
  MyThreadLocker lock(&logLock);
  //if (inWrite) { fprintf(stderr, "FATAL: cannot remove log listeners from log listener!\n"); abort(); }
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
    Z_Free(lastCurr);
  }
}


//==========================================================================
//
//  VLog::doWriteStr
//
//==========================================================================
void VLog::doWriteStr (EName Type, const char *s, bool addEOL) noexcept {
  static const char *eolstr = "\n";
  if (!s || !s[0]) return;

  MyThreadLocker lock(&logLock);
  if (!Listeners) return;

  inWrite = true;
  for (Listener *ls = Listeners; ls; ls = ls->next) {
    try {
      ls->ls->Serialise(s, Type);
      if (addEOL) ls->ls->Serialise(eolstr, Type);
    } catch (...) {
    }
  }
  inWrite = false;
}


//==========================================================================
//
//  VLog::doWrite
//
//==========================================================================
void VLog::doWrite (EName Type, const char *fmt, va_list ap, bool addEOL) noexcept {
  MyThreadLocker lock(&logLock);
  if (!Listeners) return;

  if (!addEOL && (!fmt || !fmt[0])) return;
  if (!fmt) fmt = "";

  // initial allocation
  if (!logbufsize) abort(); // the thing that should not be

  int size;

  if (fmt[0]) {
    va_list apcopy;

    va_copy(apcopy, ap);
    size = vsnprintf(logbuf, (size_t)(logbufsize-2), fmt, apcopy);
    va_end(apcopy);

    if (size < 0) return; // oops

    if (size >= logbufsize-4) {
      // not enough room, try again
      if (size > 0x1fffff-4) size = 0x1fffff-4;
      size = ((size+4)|0x1fff)+1;
      char *newlogbuf = (char *)Z_Realloc(logbuf, (size_t)size);
      if (!newlogbuf) {
        //FIXME
        fprintf(stderr, "FATAL: out of memory for log buffer (new=%d; old=%d)!\n", size, logbufsize);
      } else {
        //fprintf(stderr, "VLOG(%p): realloc log buffer! (old=%d; new=%d)\n", (void *)this, logbufsize, size);
        logbuf = newlogbuf;
        logbufsize = size;
      }
      va_copy(apcopy, ap);
      size = vsnprintf(logbuf, (size_t)(logbufsize-4), fmt, apcopy);
      va_end(apcopy);
      if (size < 0) return;
      if (size >= logbufsize) size = (int)strlen(logbuf);
    }
  } else {
    if (logbufsize < 4) {
      logbufsize = 0x1fff+1;
      logbuf = (char *)Z_Realloc(logbuf, (size_t)logbufsize);
      if (!logbuf) {
        //FIXME
        fprintf(stderr, "FATAL: out of memory for log buffer (new=%d)!\n", logbufsize);
        abort();
      }
    }
    logbuf[0] = 0;
    size = 0;
  }

  if (addEOL) { logbuf[size] = '\n'; logbuf[size+1] = 0; }

  //doWriteStr(Type, logbuf, false);
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
//  VLog::Logf
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void VLog::Logf (EName Type, const char *fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);
  doWrite(Type, fmt, ap, true);
  va_end(ap);
}


//==========================================================================
//
//  VLog::Logf
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VLog::Logf (const char *fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);
  doWrite(NAME_Log, fmt, ap, true);
  va_end(ap);
}


//==========================================================================
//
//  VLog::Log
//
//==========================================================================
void VLog::Log (EName Type, const char *s) noexcept {
  doWriteStr(Type, s, true);
}


//==========================================================================
//
//  VLog::Log
//
//==========================================================================
void VLog::Log (const char *s) noexcept {
  doWriteStr(NAME_Log, s, true);
}


//==========================================================================
//
//  VLog::Write
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void VLog::Write (EName Type, const char *fmt, ...) noexcept {
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
__attribute__((format(printf, 3, 4))) void VLog::WriteLine (EName Type, const char *fmt, ...) noexcept {
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
__attribute__((format(printf, 2, 3))) void VLog::Write (const char *fmt, ...) noexcept {
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
__attribute__((format(printf, 2, 3))) void VLog::WriteLine (const char *fmt, ...) noexcept {
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
__attribute__((format(printf, 2, 3))) void VLog::DWrite (const char *fmt, ...) noexcept {
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
__attribute__((format(printf, 2, 3))) void VLog::DWriteLine (const char *fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);
  doWrite(NAME_Dev, fmt, ap, true);
  va_end(ap);
}


// ////////////////////////////////////////////////////////////////////////// //
#if !defined(_WIN32) || defined(IN_WADCHECK) || 1
class VConLogger : public VLogListener {
private:
  bool lastWasNL;
  EName lastEvent;

public:
  VConLogger () noexcept : lastWasNL(true), lastEvent(NAME_None) {
    VName::StaticInit();
    GLog.AddListener(this);
  }

  FILE *outfile () const noexcept {
    if (!GLogTTYLog) return nullptr;
    if (lastEvent == NAME_Error && GLogErrorToStderr) return stderr;
    if (lastEvent == NAME_Warning && GLogWarningToStderr) return stderr;
    return stdout;
  }

  static void printStr (const char *s, size_t len, FILE *fo) noexcept {
    if (!s || !fo) return;
    while (len) {
      const char *esc = s;
      size_t left = len;
      while (left) {
        vuint8 ch = *(const vuint8 *)esc;
        if (ch == 127) break;
        if (ch == TEXT_COLOR_ESCAPE) break;
        if (ch < ' ') {
          if (ch != '\r' && ch != '\n' && ch != '\t' && ch != 8) break;
        }
        ++esc;
        --left;
      }
      if (!left) {
        fwrite(s, len, 1, fo);
        return;
      }
      if (left < len) {
        fwrite(s, len-left, 1, fo);
        len -= left;
        s = esc;
      }
      vassert(len > 0);
      ++s;
      --len;
      if (s[-1] != TEXT_COLOR_ESCAPE) continue;
      if (len == 0) break;
      if (*s == '[') {
        while (len > 0 && *s != ']') { --len; ++s; }
        if (len && *s == ']') { --len; ++s; }
      } else {
        ++s;
        --len;
      }
    }
  }

  static void printStr (const char *s, FILE *fo) noexcept {
    if (!s || !s[0]) return;
    printStr(s, strlen(s), fo);
  }

  static void xprintStr (const char *s, FILE *fo) noexcept {
    if (fo && s && s[0]) fwrite(s, strlen(s), 1, fo);
  }

  void printEvent (EName event) noexcept {
    if (event == NAME_None) event = NAME_Log;
    lastEvent = event;
    FILE *fo = outfile();
    if (fo) {
      if (event != NAME_Log || !GLogSkipLogTypeName) {
        #if !defined(_WIN32)
        bool resetColor = true;
        int fd = fileno(fo);
        if (fd >= 0 && isatty(fd)) {
               if (event == NAME_Init) xprintStr("\x1b[1m", fo);
          else if (event == NAME_Warning) xprintStr("\x1b[0;33;1m", fo);
          else if (event == NAME_Error) xprintStr("\x1b[0;31;1m", fo);
          else if (event == NAME_Log) xprintStr("\x1b[0;32m", fo);
          else if (event == NAME_Debug) xprintStr("\x1b[0;35;1m", fo);
          else xprintStr("\x1b[0;36;1m", fo);
        } else {
          resetColor = false;
        }
        #endif
        xprintStr(*VName(event), fo);
        xprintStr(":", fo);
        #if !defined(_WIN32)
        if (resetColor) xprintStr("\x1b[0m", fo);
        #endif
      }
    }
  }

  virtual void Serialise (const char *Text, EName Event) noexcept override {
    if (Event == NAME_None) Event = NAME_Log;
    if (!GLogTTYLog) { lastEvent = NAME_None; return; }
    //printf("===(%s)\n%s\n===\n", *VName(Event), Text);
    while (*Text) {
      if (Text[0] == '\r' && Text[1] == '\n') ++Text;
      // find line terminator
      const char *eol = Text;
      while (*eol && *eol != '\n' && *eol != '\r') ++eol;
      // print string until terminator
      //{ fwrite("|", 1, 1, stdout); fwrite(Text, (size_t)(ptrdiff_t)(eol-Text), 1, stdout); fwrite("|\n", 2, 1, stdout); }
      //printf("===(%s)\n%s\n===\n", *VName(Event), Text);
      if (eol != Text) {
        // has something to print
        if (lastWasNL || Event != lastEvent) {
          // force new event
          if (!lastWasNL) xprintStr("\n", outfile());
          printEvent(Event);
          if (!GLogSkipLogTypeName) xprintStr(" ", outfile());
        }
        lastWasNL = false;
        printStr(Text, (size_t)(ptrdiff_t)(eol-Text), outfile());
      }
      if (!eol[0]) break; // no more
      // new line
      vassert(eol[0] == '\r' || eol[0] == '\n');
      if (!lastWasNL) xprintStr("\n", outfile());
      lastWasNL = true;
      Text = eol+1;
    }
  }
};

VConLogger conlogger;
#endif
