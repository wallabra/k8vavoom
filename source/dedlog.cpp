//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#ifndef CLIENT

#include <unistd.h>

static FILE *ddlogfout = nullptr;

bool ttyRefreshInputLine = true;
bool ttyExtraDisabled = false;
bool dedEnableTTYLog = false;

void UpdateTTYPrompt ();


//**************************************************************************
//
//  Dedicated server console streams
//
//**************************************************************************
class FConsoleDevice : public FOutputDevice {
public:
  virtual void Serialise (const char *V, EName) noexcept override;
};

static FConsoleDevice Console;
FOutputDevice *GCon = &Console;


class VDedLog : public VLogListener {
public:
  EName lastEvent;
  bool justNewlined;
  unsigned coLen;
  char collectedLine[8192];

private:
  void putStdOut (const char *s, int len=-1) {
    if (!s || !dedEnableTTYLog || !ttyIsAvailable()) return;
    if (len < 1) {
      if (!len || !s || !s[0]) return;
      len = (int)strlen(s);
    }
    if (len < 1) return;
    if (ttyExtraDisabled || !ttyIsGood()) {
      //ttyRawWrite(s);
      write(STDOUT_FILENO, s, (size_t)len);
      return;
    }
    // buffer TTY output
    while (len > 0) {
      // newline?
      if (s[0] == '\n') {
        // output collected line
        if (coLen) {
          collectedLine[coLen++] = '\x1b';
          collectedLine[coLen++] = '[';
          collectedLine[coLen++] = '0';
          collectedLine[coLen++] = 'm';
          collectedLine[coLen++] = '\x1b';
          collectedLine[coLen++] = '[';
          collectedLine[coLen++] = 'K';
          collectedLine[coLen++] = '\n';
          collectedLine[coLen] = 0;
          ttyRawWrite(collectedLine);
        }
        coLen = 0;
        collectedLine[coLen++] = '\x1b';
        collectedLine[coLen++] = '[';
        collectedLine[coLen++] = '0';
        collectedLine[coLen++] = 'm';
        collectedLine[coLen++] = '\x1b';
        collectedLine[coLen++] = '[';
        collectedLine[coLen++] = '1';
        collectedLine[coLen++] = 'G';
        collectedLine[coLen++] = '\x1b';
        collectedLine[coLen++] = '[';
        collectedLine[coLen++] = 'K';
        collectedLine[coLen] = 0;
        ttyRefreshInputLine = true;
        UpdateTTYPrompt();
        ++s;
        --len;
        continue;
      }
      if (coLen < ARRAY_COUNT(collectedLine)-64) collectedLine[coLen++] = *s;
      ++s;
      --len;
    }
  }

  void putStr (const char *s, int len=-1) {
    if (!s) return;
    if (len < 1) {
      if (!len || !s || !s[0]) return;
      len = (int)strlen(s);
      if (len < 1) return;
    }
    putStdOut(s, len);
    if (ddlogfout) fwrite(s, (size_t)len, 1, ddlogfout);
  }

public:
  inline VDedLog () noexcept : lastEvent(NAME_Log), justNewlined(true), coLen(0) {
    // the first line should be cleared
    collectedLine[coLen++] = '\x1b';
    collectedLine[coLen++] = '[';
    collectedLine[coLen++] = '1';
    collectedLine[coLen++] = 'G';
    collectedLine[coLen++] = '\x1b';
    collectedLine[coLen++] = '[';
    collectedLine[coLen++] = '0';
    collectedLine[coLen++] = 'm';
    collectedLine[coLen++] = '\x1b';
    collectedLine[coLen++] = '[';
    collectedLine[coLen++] = 'K';
  }

public:
  virtual void Serialise (const char *Text, EName Event) noexcept override {
    if (Event == NAME_Dev && !developer) return;
    //if (!ddlogfout) return;
    lastEvent = Event;
    VStr rc = VStr(Text).RemoveColors();
    const char *rstr = *rc;
    if (!rstr || !rstr[0]) return;
    // use scroll region that is one less than the TTY height
    /*
    #ifndef _WIN32
    if (ttyIsGood()) {
      char ssr[32];
      snprintf(ssr, sizeof(ssr), "\x1b[1;%dH", ttyGetHeight());
      ttyRawWrite(ssr);
    }
    #endif
    */
    while (rstr && rstr[0]) {
      if (justNewlined) {
        #if !defined(_WIN32)
        bool resetColor = true;
        if (ttyIsAvailable()) {
          const char *cs = VLog::GetColorInfoTTY(lastEvent, resetColor);
          if (cs) putStdOut(cs); else resetColor = false;
        } else {
          resetColor = false;
        }
        #endif
        putStr(VName::SafeString(lastEvent));
        putStr(":");
        if (lastEvent == NAME_DevNet) {
          unsigned msecs = unsigned(Sys_Time()*1000);
          char buf[64];
          snprintf(buf, sizeof(buf), "%u:", msecs);
          putStr(buf);
        }
        #if !defined(_WIN32)
        if (resetColor) putStdOut("\x1b[0m");
        #endif
        if (rstr[0] != '\n') putStr(" ");
        justNewlined = false;
      }
      const char *eol = strchr(rstr, '\n');
      if (eol) {
        // has newline; print it too
        ++eol;
        putStr(rstr, (int)(ptrdiff_t)(eol-rstr));
        rstr = eol;
        justNewlined = true;
      } else {
        putStr(rstr);
        break;
      }
    }
  }
};

static VDedLog DedLog;


//==========================================================================
//
//  FConsoleDevice::Serialise
//
//==========================================================================
void FConsoleDevice::Serialise (const char *V, EName Event) noexcept {
  DedLog.Serialise(V, Event);
  DedLog.Serialise("\n", Event);
}


//==========================================================================
//
//  DD_SysErrorCallback
//
//==========================================================================
static void DD_SysErrorCallback (const char *msg) noexcept {
  if (ddlogfout) {
    fprintf(ddlogfout, "%s\n", (msg ? msg : ""));
    fclose(ddlogfout);
    ddlogfout = nullptr;
  }
}


//==========================================================================
//
//  DD_ShutdownLog
//
//==========================================================================
static void DD_ShutdownLog () {
  if (ddlogfout) {
    fclose(ddlogfout);
    ddlogfout = nullptr;
  }
}


//==========================================================================
//
//  DD_SetupLog
//
//==========================================================================
static void DD_SetupLog () {
  if (cli_LogFileName && cli_LogFileName[0]) {
    ddlogfout = fopen(cli_LogFileName, "w");
  }

  #if defined(_WIN32) || (defined(__SWITCH__) && !defined(SWITCH_NXLINK))
  if (!ddlogfout) ddlogfout = fopen("conlog_ded.log", "w");
  #endif

  SysErrorCB = &DD_SysErrorCallback;

  GLog.AddListener(&DedLog);
  GLogTTYLog = false;
  GLogErrorToStderr = false;
  GLogWarningToStderr = false;
  GLogSkipLogTypeName = true;
}


#else
// not a dedicated server

//==========================================================================
//
//  DD_SetupLog
//
//==========================================================================
static void DD_SetupLog () {
}


//==========================================================================
//
//  DD_ShutdownLog
//
//==========================================================================
static void DD_ShutdownLog () {
}
#endif
