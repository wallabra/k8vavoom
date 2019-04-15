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
#include "sharedmap.h"


//==========================================================================
//
//  getchar_e
//
//==========================================================================
void getchar_e (FILE *f, int &i) {
  i = fgetc(f);
  if (i == EOF) throw exception::eof();
}


//==========================================================================
//
//  getbyte_e
//
//==========================================================================
void getbyte_e (FILE *f, short &i) {
  int v = fgetc(f);
  if (v == EOF) throw exception::eof();
  i &= v&0xff;
}


//==========================================================================
//
//  getbyte_e
//
//==========================================================================
void getbyte_e (FILE *f, int &i) {
  i = fgetc(f);
  if (i == EOF) throw exception::eof();
  i &= 0xff;
}


//==========================================================================
//
//  getshort_e
//
//==========================================================================
void getshort_e (FILE *f, short &v) {
  int i;
  getchar_e(f, i);
  v = i;
  getchar_e(f, i);
  v |= (i<<8);
}


//==========================================================================
//
//  getshort_e
//
//==========================================================================
void getshort_e (FILE *f, int &v) {
  int i;
  getchar_e(f, i);
  v = i;
  getchar_e(f, i);
  v |= (i<<8);
}


//==========================================================================
//
//  getchar_8
//
//==========================================================================
void getchar_8 (FILE *f, char *s) {
  memset(s, 0, 8);
  for (int count = 8; count > 0; --count) {
    int ch = fgetc(f);
    if (ch == EOF) throw exception::eof();
    *s++ = ch;
  }
}


//==========================================================================
//
//  sidedef::read
//
//==========================================================================
void sidedef::read (FILE *fl) {
  getshort_e(fl, xoffset);
  getshort_e(fl, yoffset);
  getchar_8(fl, upper);
  getchar_8(fl, lower);
  getchar_8(fl, middle);
  getshort_e(fl, faces);
}


//==========================================================================
//
//  sidedef::convert
//
//==========================================================================
udmf::block sidedef::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block us;
  if (xoffset != 0) us.put("offsetx", xoffset);
  if (yoffset != 0) us.put("offsety", yoffset);
  if (strcmp("-", upper) != 0) us.put("texturetop", upper, 8);
  if (strcmp("-", lower) != 0) us.put("texturebottom", lower, 8);
  if (strcmp("-", middle) != 0) us.put("texturemiddle", middle, 8);
  us.put("sector", faces);
  return us;
}


//==========================================================================
//
//  vertex::read
//
//==========================================================================
void vertex::read (FILE *fl) {
  getshort_e(fl, x);
  getshort_e(fl, y);
}


//==========================================================================
//
//  vertex::convert
//
//==========================================================================
udmf::block vertex::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block uv;
  uv.put("x", x*xfactor);
  uv.put("y", y*yfactor);
  return uv;
}


//==========================================================================
//
//  sector::read
//
//==========================================================================
void sector::read (FILE *fl) {
  getshort_e(fl, floorz);
  getshort_e(fl, ceilz);
  getchar_8(fl, floortex);
  getchar_8(fl, ceiltex);
  getshort_e(fl, light);
  getshort_e(fl, special);
  getshort_e(fl, tag);
}


//==========================================================================
//
//  sector::convert
//
//==========================================================================
udmf::block sector::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block us;
  if (floorz != 0) us.put("heightfloor", floorz*zfactor);
  if (ceilz != 0) us.put("heightceiling", ceilz*zfactor);
  us.put("texturefloor", floortex, 8);
  us.put("textureceiling", ceiltex, 8);
  if (light != 160) us.put("lightlevel", light);
  if (special != 0) us.put("special", (unsigned short)special);
  if (tag != 0) us.put("id", (unsigned short)tag);
  return us;
}
