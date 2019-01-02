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


VLog GLog;


//==========================================================================
//
//  VLog::VLog
//
//==========================================================================
VLog::VLog () {
  memset(Listeners, 0, sizeof(Listeners));
}


//==========================================================================
//
//  VLog::AddListener
//
//==========================================================================
void VLog::AddListener (VLogListener *Listener) {
  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (!Listeners[i]) {
      Listeners[i] = Listener;
      return;
    }
  }
}


//==========================================================================
//
//  VLog::RemoveListener
//
//==========================================================================
void VLog::RemoveListener (VLogListener *Listener) {
  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i] == Listener) Listeners[i] = nullptr;
  }
}


//==========================================================================
//
//  VLog::Write
//
//==========================================================================
void VLog::Write (EName Type, const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, Type);
      } catch (...) {
      }
    }
  }
}


//==========================================================================
//
//  VLog::WriteLine
//
//==========================================================================
void VLog::WriteLine (EName Type, const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);
  strcat(String, "\n");

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, Type);
      } catch (...) {
      }
    }
  }
}


//==========================================================================
//
//  VLog::Write
//
//==========================================================================
void VLog::Write (const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, NAME_Log);
      } catch (...) {
      }
    }
  }
}


//==========================================================================
//
//  VLog::WriteLine
//
//==========================================================================
void VLog::WriteLine (const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);
  strcat(String, "\n");

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, NAME_Log);
      } catch (...) {
      }
    }
  }
}


//==========================================================================
//
//  VLog::DWrite
//
//==========================================================================
void VLog::DWrite (const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, NAME_Dev);
      } catch (...) {
      }
    }
  }
}


//==========================================================================
//
//  VLog::DWriteLine
//
//==========================================================================
void VLog::DWriteLine (const char *Fmt, ...) {
  va_list ArgPtr;
  char String[1024];

  va_start(ArgPtr, Fmt);
  vsprintf(String, Fmt, ArgPtr);
  va_end(ArgPtr);
  strcat(String, "\n");

  for (int i = 0; i < MAX_LISTENERS; ++i) {
    if (Listeners[i]) {
      try {
        Listeners[i]->Serialise(String, NAME_Dev);
      } catch (...) {
      }
    }
  }
}
