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
#ifndef CLIENT

#include <unistd.h>

static FILE *ddlogfout = nullptr;


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
  bool justNewlined;
  EName lastEvent;
  bool stdoutIsTTY;

private:
  inline void putStdOut (const char *s, int len=-1) {
    if (!stdoutIsTTY) return;
    if (len < 1) {
      if (!len || !s || !s[0]) return;
      len = (int)strlen(s);
      if (len < 1) return;
    }
    write(STDOUT_FILENO, s, (size_t)len);
  }

  inline void putStr (const char *s, int len=-1) {
    if (len < 1) {
      if (!len || !s || !s[0]) return;
      len = (int)strlen(s);
      if (len < 1) return;
    }
    if (stdoutIsTTY) write(STDOUT_FILENO, s, (size_t)len);
    if (ddlogfout) fwrite(s, (size_t)len, 1, ddlogfout);
  }

public:
  inline VDedLog () noexcept : justNewlined(true) {
    stdoutIsTTY = !!isatty(STDOUT_FILENO);
    //stdoutIsTTY = false;
  }

public:
  virtual void Serialise (const char *Text, EName Event) noexcept override {
    if (Event == NAME_Dev && !developer) return;
    //if (!ddlogfout) return;
    lastEvent = Event;
    VStr rc = VStr(Text).RemoveColors();
    const char *rstr = *rc;
    while (rstr && *rstr) {
      if (justNewlined) {
        #if !defined(_WIN32)
             if (lastEvent == NAME_Init) putStdOut("\x1b[1m");
        else if (lastEvent == NAME_Warning) putStdOut("\x1b[0;33;1m");
        else if (lastEvent == NAME_Error) putStdOut("\x1b[0;31;1m");
        else if (lastEvent == NAME_Log) putStdOut("\x1b[0;32m");
        else if (lastEvent == NAME_Debug) putStdOut("\x1b[0;35;1m");
        else putStdOut("\x1b[0;36;1m");
        #endif
        putStr(VName::SafeString(lastEvent));
        if (rstr[0] != '\n') putStr(": "); else putStr(":");
        #if !defined(_WIN32)
        putStdOut("\x1b[0m");
        #endif
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

  #if defined(_WIN32)
  if (!ddlogfout) ddlogfout = fopen("conlog.log", "w");
  #elif defined(__SWITCH__) && !defined(SWITCH_NXLINK)
  if (!ddlogfout) ddlogfout = fopen("/switch/k8vavoom/conlog.log", "w");
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
