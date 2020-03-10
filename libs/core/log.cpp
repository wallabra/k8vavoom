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
//**  Copyright (C) 2018-2020 Ketmar Dark
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


// ////////////////////////////////////////////////////////////////////////// //
struct ColorInfo {
  VStr color;
  bool resetColor;
  EName type;
};

// colors for various predefined types
static TMap<EName, ColorInfo> ttyColors;
static TMap<EName, ColorInfo> engineColors;

static inline const char *GetColorInfoFromMap (TMap<EName, ColorInfo> &map, EName type, bool &reset) noexcept {
  auto pp = map.find(type);
  if (!pp) pp = map.find(NAME_None);
  if (pp) {
    reset = pp->resetColor;
    return *pp->color;
  } else {
    reset = false;
    return nullptr;
  }
}

static inline void SetColorInfoToMap (TMap<EName, ColorInfo> &map, EName type, const char *clrstr, bool reset) noexcept {
  if (clrstr && clrstr[0]) {
    ColorInfo ci;
    ci.color = clrstr;
    ci.resetColor = reset;
    ci.type = type;
    map.put(type, ci);
  } else {
    map.remove(type);
  }
}

const char *VLog::GetColorInfoTTY (EName type, bool &reset) noexcept { return GetColorInfoFromMap(ttyColors, type, reset); }
const char *VLog::GetColorInfoEngine (EName type, bool &reset) noexcept { return GetColorInfoFromMap(engineColors, type, reset); }

void VLog::SetColorInfoTTY (EName type, const char *clrstr, bool reset) noexcept { SetColorInfoToMap(ttyColors, type, clrstr, reset); }
void VLog::SetColorInfoEngine (EName type, const char *clrstr, bool reset) noexcept { SetColorInfoToMap(engineColors, type, clrstr, reset); }


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

  static inline bool IsPrintBreakChar (const char ch) noexcept {
    return
      ch == 127 ||
      ch == TEXT_COLOR_ESCAPE ||
      ((vuint8)ch < 32 && ch != '\r' && ch != '\n' && ch != '\t' && ch != 8);
  }

  static void printStr (const char *s, size_t len, FILE *fo) noexcept {
    if (!s || !fo || !len) return;
    while (len) {
      const char *esc = s;
      size_t pfxlen = 0;
      while (pfxlen < len && !IsPrintBreakChar(*esc)) { ++esc; ++pfxlen; }
      if (pfxlen == len) { fwrite(s, len, 1, fo); return; } // done
      // found some break char
      if (pfxlen) {
        fwrite(s, pfxlen, 1, fo);
        len -= pfxlen;
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
          const char *cs = VLog::GetColorInfoTTY(event, resetColor);
          if (cs) xprintStr(cs, fo);
        } else {
          resetColor = false;
        }
        #endif
        xprintStr(*VName(event), fo);
        xprintStr(":", fo);
        #if !defined(_WIN32)
        if (resetColor) xprintStr("\x1b[0m", fo);
        #endif
        if (event == NAME_DevNet) {
          unsigned msecs = unsigned(Sys_Time()*1000);
          char buf[64];
          snprintf(buf, sizeof(buf), "%u:", msecs);
          xprintStr(buf, fo);
        }
      }
    }
  }

  virtual void Serialise (const char *Text, EName Event) noexcept override {
    //if (Text[0]) { FILE *fo = fopen("z.txt", "a"); fwrite(Text, strlen(Text), 1, fo); fputc('|', fo); fclose(fo); }
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
        //{ FILE *fo = fopen("z1.txt", "a"); fwrite(Text, (size_t)(ptrdiff_t)(eol-Text), 1, fo); fputc('|', fo); fputc('\n', fo); fclose(fo); }
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


class LoggerSetupPredefinedColors {
public:
  LoggerSetupPredefinedColors () {
    // tty colors
    VLog::SetColorInfoTTY(NAME_None, "\x1b[0;36;1m"); // unknown
    VLog::SetColorInfoTTY(NAME_Init, "\x1b[1m");
    VLog::SetColorInfoTTY(NAME_Warning, "\x1b[0;33;1m");
    VLog::SetColorInfoTTY(NAME_Error, "\x1b[0;31;1m");
    VLog::SetColorInfoTTY(NAME_Log, "\x1b[0;32m");
    VLog::SetColorInfoTTY(NAME_Debug, "\x1b[0;35;1m");
    VLog::SetColorInfoTTY(NAME_Dev, "\x1b[0;35;1m");
    VLog::SetColorInfoTTY(NAME_Chat, "\x1b[0;37;1m");
    // bots, don't reset
    VLog::SetColorInfoTTY(NAME_Bot, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDev, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevAI, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevRoam, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevCheckPos, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevItems, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevAttack, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevPath, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevCrumbs, "\x1b[0;33m", false);
    VLog::SetColorInfoTTY(NAME_BotDevPlanPath, "\x1b[0;33m", false);

    // engine colors
    //SetColorInfo(NAME_None, ""); // unknown
    VLog::SetColorInfoEngine(NAME_Init, TEXT_COLOR_ESCAPE_STR "[InitCyan]");
    VLog::SetColorInfoEngine(NAME_Warning, TEXT_COLOR_ESCAPE_STR "[WarningYellow]");
    VLog::SetColorInfoEngine(NAME_Error, TEXT_COLOR_ESCAPE_STR "[RedError]");
    VLog::SetColorInfoEngine(NAME_Debug, TEXT_COLOR_ESCAPE_STR "[DebugGreen]");
    VLog::SetColorInfoEngine(NAME_Dev, TEXT_COLOR_ESCAPE_STR "[DebugGreen]");
    VLog::SetColorInfoEngine(NAME_Chat, TEXT_COLOR_ESCAPE_STR "[Gold]");
    // bots, don't reset
    VLog::SetColorInfoEngine(NAME_Bot, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDev, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevAI, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevRoam, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevCheckPos, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevItems, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevAttack, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevPath, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevCrumbs, TEXT_COLOR_ESCAPE_STR "[Black]", false);
    VLog::SetColorInfoEngine(NAME_BotDevPlanPath, TEXT_COLOR_ESCAPE_STR "[Black]", false);
  }
};


LoggerSetupPredefinedColors loggerSetupPredefinedColors__;
