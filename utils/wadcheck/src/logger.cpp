//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core/src/core.h"


// ////////////////////////////////////////////////////////////////////////// //
class VConLogger : public VLogListener {
private:
  bool wasNL;
  EName lastEvent;

public:
  VConLogger () : wasNL(true), lastEvent(NAME_None) { /*GLog.AddListener(this);*/
    VName::StaticInit();
    GLog.AddListener(this);
  }

  virtual void Serialise (const char *Text, EName Event) override {
    //fprintf(stderr, "%s: %s", *VName(Event), Text);
    while (*Text) {
      if (lastEvent != Event) { if (!wasNL) fputc('\n', stdout); wasNL = true; lastEvent = Event; }
      if (Text[0] == '\n') {
        if (wasNL) { fputs(*VName(Event), stdout); fputs(": ", stdout); }
        fputc('\n', stdout);
        ++Text;
        wasNL = true;
        lastEvent = Event;
      } else {
        const char *eol = strchr(Text, '\n');
        if (!eol) {
          if (wasNL) { fputs(*VName(Event), stdout); fputs(": ", stdout); wasNL = false; }
          fputs(Text, stdout);
          return;
        } else {
          // ends with eol
          if (wasNL) { fputs(*VName(Event), stdout); fputs(": ", stdout); wasNL = false; }
          fwrite(Text, (size_t)(ptrdiff_t)(eol+1-Text), 1, stdout);
          Text = eol+1;
        }
      }
    }
  }
};

VConLogger conlogger;
