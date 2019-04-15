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
#include "zdoommap.h"


//==========================================================================
//
//  ZDoomThing::read
//
//==========================================================================
void ZDoomThing::read (FILE *fl) {
  getshort_e(fl, tid);
  getshort_e(fl, x);
  getshort_e(fl, y);
  getshort_e(fl, height);
  getshort_e(fl, angle);
  getshort_e(fl, doomednum);
  getshort_e(fl, flags);
  getbyte_e(fl, special);
  for (int f = 0; f < 5; ++f) getbyte_e(fl, arg[f]);
}


//==========================================================================
//
//  ZDoomThing::convert
//
//==========================================================================
udmf::block ZDoomThing::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block ut;
  if (tid != 0) ut.put("id", tid);
  ut.put("x", x*xfactor);
  ut.put("y", y*yfactor);
  if (angle != 0) ut.put("angle", angle);
  ut.put("type", doomednum);
  if (height != 0) ut.put("height", height*zfactor);
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
  if (flags&TF_SINGLE) ut.put("single", true);
  if (flags&TF_DMATCH) ut.put("dm", true);
  if (flags&TF_COOP) ut.put("coop", true);
  if (flags&TF_DORMANT) ut.put("dormant", true);
  if (flags&TF_FIGHTER) ut.put("class1", true);
  if (flags&TF_CLERIC) ut.put("class2", true);
  if (flags&TF_MAGE) ut.put("class3", true);
  if (flags&TF_TRANSLUCENT) ut.put("translucent", true);
  if (flags&TF_INVISIBLE) ut.put("invisible", true);
  if (flags&TF_FRIENDLY) ut.put("strifeally", true);
  if (flags&TF_STANDSTILL) ut.put("standing", true);
  if (special != 0) ut.put("special", special);
  if (arg[0] != 0) ut.put("arg0", arg[0]);
  if (arg[1] != 0) ut.put("arg1", arg[1]);
  if (arg[2] != 0) ut.put("arg2", arg[2]);
  if (arg[3] != 0) ut.put("arg3", arg[3]);
  if (arg[4] != 0) ut.put("arg4", arg[4]);
  return ut;
}


//==========================================================================
//
//  ZDoomLine::read
//
//==========================================================================
void ZDoomLine::read (FILE *fl) {
  getshort_e(fl, start);
  getshort_e(fl, end);
  getshort_e(fl, flags);
  getbyte_e(fl, special);
  for (int f = 0; f < 5; ++f) getbyte_e(fl, arg[f]);
  getshort_e(fl, right);
  getshort_e(fl, left);
}


//==========================================================================
//
//  ZDoomLine::convert
//
//==========================================================================
udmf::block ZDoomLine::convert (double xfactor, double yfactor, double zfactor) const {
  udmf::block ul;
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
  if ((flags&0x1C00) == LDA_PLAYERCROSS) ul.put("playercross", true);
  if ((flags&0x1C00) == LDA_PLAYERUSE) ul.put("playeruse", true);
  if ((flags&0x1C00) == LDA_MONSTERCROSS) ul.put("monstercross", true);
  if (((flags&0x1C00) == LDA_PLAYERCROSS) && (flags&LDF_MONSTERUSE)) ul.put("monsteruse", true);
  if ((flags&0x1C00) == LDA_PROJECTILEHIT) ul.put("impact", true);
  if ((flags&0x1C00) == LDA_PLAYERBUMP) ul.put("playerpush", true);
  if (((flags&0x1C00) == LDA_PLAYERCROSS) && (flags&LDF_MONSTERUSE)) ul.put("monsterpush", true);
  if ((flags&0x1C00) == LDA_PROJECTILECROSS) ul.put("missilecross", true);
  if (flags&LDF_REPEATABLE) ul.put("repeatspecial", true);
  if (special != 0) ul.put("special", special);
  if (arg[0] != 0) ul.put("arg0", arg[0]);
  if (arg[1] != 0) ul.put("arg1", arg[1]);
  if (arg[2] != 0) ul.put("arg2", arg[2]);
  if (arg[3] != 0) ul.put("arg3", arg[3]);
  if (arg[4] != 0) ul.put("arg4", arg[4]);
  ul.put("sidefront", (unsigned short)right);
  if (left != -1) ul.put("sideback", (unsigned short)left);
  if (flags&LDF_MONSTERUSE) ul.put("monsteractivate", true);
  if (flags&LDF_BLOCKPLAYERS) ul.put("blockplayers", true);
  if (flags&LDF_BLOCKALL) ul.put("blockeverything", true);
  if ((flags&0x1C00) == LDA_PROJECTILEANY) {
    ul.put("impact", true);
    ul.put("missilecross", true);
  }

  switch (special) {
    case 121:
      ul.put("id", ul.get("arg0"));
      ul.del("special");
      ul.del("arg0");
      ul.del("arg1");
      ul.del("arg2");
      ul.del("arg3");
      ul.del("arg4");
      ul.put("id", arg[0]|(arg[4]<<8));
      if (arg[1]&LDFE_ZONEBOUNDARY) ul.put("zoneboundary", true);
      if (arg[1]&LDFE_RAILING) ul.put("jumpover", true);
      if (arg[1]&LDFE_BLOCKFLOATERS) ul.put("blockfloaters", true);
      if (arg[1]&LDFE_CLIPMIDTEX) ul.put("clipmidtex", true);
      if (arg[1]&LDFE_WRAPMIDTEX) ul.put("wrapmidtex", true);
      if (arg[1]&LDFE_3DMIDTEX) ul.put("midtex3d", true);
      if (arg[1]&LDFE_CHECKRANGE) ul.put("checkswitchrange", true);
      if (arg[1]&LDFE_FIRSTSIDE) ul.put("firstsideonly", true);
      break;
    case 208:
      ul.put("id", arg[0]);
      if (arg[3]&LDFE_ZONEBOUNDARY) ul.put("zoneboundary", true);
      if (arg[3]&LDFE_RAILING) ul.put("jumpover", true);
      if (arg[3]&LDFE_BLOCKFLOATERS) ul.put("blockfloaters", true);
      if (arg[3]&LDFE_CLIPMIDTEX) ul.put("clipmidtex", true);
      if (arg[3]&LDFE_WRAPMIDTEX) ul.put("wrapmidtex", true);
      if (arg[3]&LDFE_3DMIDTEX) ul.put("midtex3d", true);
      if (arg[3]&LDFE_CHECKRANGE) ul.put("checkswitchrange", true);
      if (arg[3]&LDFE_FIRSTSIDE) ul.put("firstsideonly", true);
      break;
    case 1:
      ul.put("id", ul.get("arg3"));
      ul.del("arg3");
      break;
    case 5:
      ul.put("id", ul.get("arg4"));
      ul.del("arg4");
      break;
    case 215:
      ul.put("id", ul.get("arg0"));
      ul.del("arg0");
      break;
    case 222:
      ul.put("id", ul.get("arg0"));
      break;
    case 181:
      ul.put("id", ul.get("arg2"));
      ul.del("arg2");
      break;
  }

  return ul;
}
