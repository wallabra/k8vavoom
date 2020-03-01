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

//==========================================================================
//
//  VLogListener
//
//==========================================================================
class VLogListener : VInterface {
public:
  virtual void Serialise (const char *Text, EName Event) noexcept = 0;
};


//==========================================================================
//
//  VLog
//
//==========================================================================
class VLog {
private:
  enum { INITIAL_BUFFER_SIZE = 32768 };

  struct Listener {
    VLogListener *ls;
    Listener *next;
  };

private:
  static Listener *Listeners;
  char *logbuf;
  int logbufsize;
  bool inWrite;

public:
  void doWriteStr (EName Type, const char *s, bool addEOL) noexcept;
  void doWrite (EName Type, const char *fmt, va_list ap, bool addEOL) noexcept;

public:
  VLog () noexcept;

  static void AddListener (VLogListener *Listener) noexcept;
  static void RemoveListener (VLogListener *Listener) noexcept;

  void Write (EName Type, const char *fmt, ...) noexcept __attribute__((format(printf, 3, 4)));
  void WriteLine (EName Type, const char *fmt, ...) noexcept __attribute__((format(printf, 3, 4)));

  void Write (const char *fmt, ...) noexcept __attribute__((format(printf, 2, 3)));
  void WriteLine (const char *fmt, ...) noexcept __attribute__((format(printf, 2, 3)));

  void DWrite (const char *fmt, ...) noexcept __attribute__((format(printf, 2, 3)));
  void DWriteLine (const char *fmt, ...) noexcept __attribute__((format(printf, 2, 3)));

  void Logf (EName Type, const char *fmt, ...) noexcept __attribute__((format(printf, 3, 4)));
  void Logf (const char *fmt, ...) noexcept __attribute__((format(printf, 2, 3)));

  void Log (EName Type, const char *s) noexcept;
  void Log (const char *s) noexcept;

public:
  static const char *GetColorInfoTTY (EName type, bool &reset) noexcept;
  static const char *GetColorInfoEngine (EName type, bool &reset) noexcept;

  static void SetColorInfoTTY (EName type, const char *clrstr, bool reset=true) noexcept;
  static void SetColorInfoEngine (EName type, const char *clrstr, bool reset=true) noexcept;

public: // fuck you, shitplusplus!
  static mythread_mutex logLock;
  static volatile bool logLockInited;
};


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! THERE SHOULD BE ONLY ONE!
// FIXME: implement proper singleton
extern VLog GLog;
extern bool GLogTTYLog; // true
extern bool GLogSkipLogTypeName; // false
extern bool GLogErrorToStderr; // false
extern bool GLogWarningToStderr; // false
