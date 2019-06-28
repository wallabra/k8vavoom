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
bool GLogTTYLog = true;
bool GLogErrorToStderr = false;
bool GLogWarningToStderr = false;
#if defined(IN_WADCHECK)
bool GLogSkipLogTypeName = true;
#else
bool GLogSkipLogTypeName = false;
#endif

VLog::Listener *VLog::Listeners = nullptr;


//==========================================================================
//
//  VLog::VLog
//
//==========================================================================
VLog::VLog ()
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
void VLog::AddListener (VLogListener *lst) {
  if (!lst) return;
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
void VLog::RemoveListener (VLogListener *lst) {
  if (!lst || !Listeners) return;
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
//  VLog::doWrite
//
//==========================================================================
void VLog::doWriteStr (EName Type, const char *s) {
  if (!s || !s[0]) return;
  if (!Listeners) return;

  inWrite = true;
  for (Listener *ls = Listeners; ls; ls = ls->next) {
    try {
      ls->ls->Serialise(s, Type);
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
void VLog::doWrite (EName Type, const char *fmt, va_list ap, bool addEOL) {
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

  doWriteStr(Type, logbuf);
}


//==========================================================================
//
//  VLog::Logf
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void VLog::Logf (EName Type, const char *fmt, ...) {
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
__attribute__((format(printf, 2, 3))) void VLog::Logf (const char *fmt, ...) {
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
void VLog::Log (EName Type, const char *s) {
  doWriteStr(Type, s);
  doWriteStr(Type, "\n");
}


//==========================================================================
//
//  VLog::Log
//
//==========================================================================
void VLog::Log (const char *s) {
  doWriteStr(NAME_Log, s);
  doWriteStr(NAME_Log, "\n");
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


// ////////////////////////////////////////////////////////////////////////// //
#if !defined(_WIN32) || defined(IN_WADCHECK)
class VConLogger : public VLogListener {
private:
  bool wasNL;
  EName lastEvent;

public:
  VConLogger () : wasNL(true), lastEvent(NAME_None) {
    VName::StaticInit();
    GLog.AddListener(this);
  }

  FILE *outfile () const {
    if (!GLogTTYLog) return nullptr;
    if (lastEvent == NAME_Error && GLogErrorToStderr) return stderr;
    if (lastEvent == NAME_Warning && GLogWarningToStderr) return stderr;
    return stdout;
  }

  static void printStr (const char *s, size_t len, FILE *fo) {
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
      check(len > 0);
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

  static void printStr (const char *s, FILE *fo) {
    if (!s || !s[0]) return;
    printStr(s, strlen(s), fo);
  }

  virtual void Serialise (const char *Text, EName Event) override {
    if (!GLogTTYLog) return;
    //fprintf(stderr, "%s: <%s>\n", *VName(Event), *VStr(Text).quote());
    while (*Text) {
      if (lastEvent != Event) {
        if (!wasNL) fputc('\n', outfile());
        wasNL = true;
        lastEvent = Event;
      }
      if (Text[0] == '\r' && Text[1] == '\n') ++Text;
      if (Text[0] == '\n' || Text[0] == '\r') {
        if (wasNL) {
          lastEvent = Event;
          if (Event != NAME_Log || !GLogSkipLogTypeName) {
            printStr(*VName(Event), outfile());
            printStr(":", outfile());
          }
        }
        fputc('\n', outfile());
        ++Text;
        wasNL = true;
      } else {
        const char *eol0 = strchr(Text, '\n');
        const char *eol1 = strchr(Text, '\r');
        if (!eol0 && !eol1) {
          if (wasNL) {
            if (Event != NAME_Log || !GLogSkipLogTypeName) {
              printStr(*VName(Event), outfile());
              printStr(": ", outfile());
            }
            wasNL = false;
          }
          printStr(Text, outfile());
          return;
        }
        const char *eol = (eol0 && eol1 ? (eol0 < eol1 ? eol0 : eol1) : eol0 ? eol0 : eol1);
        check(eol != Text);
        // ends with eol
        if (wasNL) {
          if (Event != NAME_Log || !GLogSkipLogTypeName) {
            printStr(*VName(Event), outfile());
            printStr(": ", outfile());
          }
          wasNL = false;
        }
        printStr(Text, (size_t)(ptrdiff_t)(eol-Text), outfile());
        Text = eol;
      }
    }
  }
};

VConLogger conlogger;
#endif
