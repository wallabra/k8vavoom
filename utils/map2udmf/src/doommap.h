//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2009, 2010 Brendon Duncan
//**  Copyright (C) 2019 Ketmar Dark
//**  Based on https://github.com/CO2/UDMF-Convert
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
#ifndef DOOM_MAPDATA_H
#define DOOM_MAPDATA_H

#include "sharedmap.h"


struct DoomThing : public thing {
  enum {
    TF_EASY = 0x1,
    TF_MEDIUM = 0x2,
    TF_HARD = 0x4,
    TF_AMBUSH = 0x8,
    TF_MULTI = 0x10,
    TF_NODM = 0x20,
    TF_NOCOOP = 0x40,
    TF_FRIEND = 0x80
  };

  virtual void read (FILE *fl) override;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const override;
};


struct DoomLine : public linedef {
  enum {
    LDF_IMPASSIBLE = 0x1,
    LDF_BLOCKMONSTER = 0x2,
    LDF_TWOSIDED = 0x4,
    LDF_UPPERUNPEGGED = 0x8,
    LDF_LOWERUNPEGGED = 0x10,
    LDF_SECRET = 0x20,
    LDF_BLOCKSOUND = 0x40,
    LDF_NOAUTOMAP = 0x80,
    LDF_VISIBLE = 0x100,
    LDF_PASSUSE = 0x200
  };

  short tag;

  DoomLine () : linedef(), tag(0) {}

  virtual void read (FILE *fl) override;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const override;
};


#endif
