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
#include "doommap.h"


//==========================================================================
//
//  DoomThing::read
//
//==========================================================================
void DoomThing::read (FILE *fl) {
  getshort_e(fl, x);
  getshort_e(fl, y);
  getshort_e(fl, angle);
  getshort_e(fl, doomednum);
  getshort_e(fl, flags);
}


//==========================================================================
//
//  DoomThing::convert
//
//==========================================================================
udmf::block DoomThing::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block ut;
  ut.put("x", x*xfactor);
  ut.put("y", y*yfactor);
  if (angle != 0) ut.put("angle", angle);
  ut.put("type", (unsigned short)doomednum);
  if (flags&TF_EASY) {
    ut.put("skill1", true);
    ut.put("skill2", true);
  }
  if (flags&TF_MEDIUM) ut.put("skill3", true);
  if (flags&TF_HARD) {
    ut.put("skill4", true);
    ut.put("skill5", true);
  }
  if (flags&TF_AMBUSH) ut.put("ambush", true);
  if (!(flags&TF_MULTI)) ut.put("single", true);
  if (!(flags&TF_NODM)) ut.put("dm", true);
  if (!(flags&TF_NOCOOP)) ut.put("coop", true);
  if (flags&TF_FRIEND) ut.put("friend", true);
  return ut;
}


//==========================================================================
//
//  DoomLine::read
//
//==========================================================================
void DoomLine::read (FILE *fl) {
  getshort_e(fl, start);
  getshort_e(fl, end);
  getshort_e(fl, flags);
  getshort_e(fl, special);
  getshort_e(fl, tag);
  getshort_e(fl, right);
  getshort_e(fl, left);
}


//==========================================================================
//
//  DoomLine::convert
//
//==========================================================================
udmf::block DoomLine::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block ul;
  if (tag != 0) {
    ul.put("id", (unsigned short)tag);
    ul.put("arg0", (unsigned short)tag);
  }
  ul.put("v1", (unsigned short)start);
  ul.put("v2", (unsigned short)end);
  if (flags&LDF_IMPASSIBLE) ul.put("blocking", true);
  if (flags&LDF_BLOCKMONSTER) ul.put("blockmonsters", true);
  if (flags&LDF_TWOSIDED) ul.put("twosided", true);
  if (flags&LDF_UPPERUNPEGGED) ul.put("dontpegtop", true);
  if (flags&LDF_LOWERUNPEGGED) ul.put("dontpegbottom", true);
  if (flags&LDF_SECRET) ul.put("secret", true);
  if (flags&LDF_BLOCKSOUND) ul.put("blocksound", true);
  if (flags&LDF_NOAUTOMAP) ul.put("dontdraw", true);
  if (flags&LDF_VISIBLE) ul.put("mapped", true);
  if (flags&LDF_PASSUSE) ul.put("passuse", true);
  if (special != 0) ul.put("special", (unsigned short)special);
  ul.put("sidefront", (unsigned short)right);
  if (left != -1) ul.put("sideback", (unsigned short)left);
  return ul;
}


//==========================================================================
//
//  DoomZDoomSector::convert
//
//==========================================================================
udmf::block DoomZDoomSector::convert (double xfactor, double yfactor, double zfactor) const {
  auto res = sector::convert(xfactor, yfactor, zfactor);
  if (tag) res.put("dropactors", true);
  return res;
}
