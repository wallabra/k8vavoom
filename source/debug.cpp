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
#include "gamedefs.h"

//#define CLOSEDDF


class VDebugLog : public VLogListener {
public:
  virtual void Serialise (const char *Text, EName Event) override {
    dprintf("%s: %s", VName::SafeString(Event), *VStr(Text).RemoveColours());
  }
};


static FILE *df = nullptr;
static const char *debug_file_name = nullptr;
static VDebugLog DebugLog;


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
void OpenDebugFile (const char *name) {
  debug_file_name = name;
#ifdef CLOSEDDF
  df = fopen(debug_file_name, "w");
  fclose(df);
#else
#ifndef DEVELOPER
  if (GArgs.CheckParm("-debug"))
#endif
  {
    if (GArgs.CheckParm("-RHIDE")) {
      df = stderr;
    } else {
      df = fopen(debug_file_name, "w");
    }
  }
#endif
  if (df) GLog.AddListener(&DebugLog);
}


//==========================================================================
//
//  dprintf
//
//==========================================================================
int dprintf (const char *s, ...) {
  va_list v;

#ifdef CLOSEDDF
  df = fopen(debug_file_name, "a");
#endif

  va_start(v, s);
  if (df) {
    vfprintf(df, s, v);
    fflush(df);
  }
  va_end(v);

#ifdef CLOSEDDF
  fclose(df);
#endif

  return 0;
}
