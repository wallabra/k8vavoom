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
#ifndef SHARED_MAPDATA_H
#define SHARED_MAPDATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exceptions.h"
#include "udmf.h"


struct sidedef {
  short xoffset;
  short yoffset;
  char upper[8];
  char lower[8];
  char middle[8];
  short faces;

  sidedef () : xoffset(0), yoffset(0), faces(0) { memset(upper, 0, 8); memset(lower, 0, 8); memset(middle, 0, 8); }
  virtual ~sidedef () {};

  virtual void read (FILE *fl);
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const;
};


struct vertex {
  short x;
  short y;

  vertex () : x(0), y(0) {}
  virtual ~vertex () {}

  virtual void read (FILE *fl);
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const;
};


struct sector {
  short floorz;
  short ceilz;
  char floortex[8];
  char ceiltex[8];
  short light;
  short special;
  short tag;

  sector () : floorz(0), ceilz(0), light(0), special(0), tag(0) { memset(floortex, 0, 8); memset(ceiltex, 0, 8); }
  virtual ~sector () {}

  virtual void read (FILE *fl);
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const;
};


struct thing {
  short x;
  short y;
  short angle;
  short doomednum;
  short flags;

  thing () : x(0), y(0), angle(0), doomednum(0), flags(0) {}
  virtual ~thing () {}

  virtual void read (FILE *fl) = 0;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const = 0;
};


struct linedef {
  short start;
  short end;
  short flags;
  short special;
  short right;
  short left;

  linedef () : start(0), end(0), flags(0), special(0), right(0), left(0) {}
  virtual ~linedef () {}

  virtual void read (FILE *fl) = 0;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const = 0;
};


extern void getchar_e (FILE *f, int &i);
extern void getbyte_e (FILE *f, short &i);
extern void getbyte_e (FILE *f, int &i);
extern void getshort_e (FILE *f, short &v);
extern void getshort_e (FILE *f, int &v);
extern void getchar_8 (FILE *f, char *s);


#endif
