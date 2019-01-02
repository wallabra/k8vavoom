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

//==========================================================================
//
//  mlog2
//
//==========================================================================
/*
int mlog2 (int val) {
  int answer = 0;
  while (val >>= 1) ++answer;
  return answer;
}
*/


//==========================================================================
//
//  mround
//
//==========================================================================
/*
int mround (float Val) {
  return (int)floor(Val+0.5);
}
*/


//==========================================================================
//
//  ToPowerOf2
//
//==========================================================================
#if 0
int ToPowerOf2 (int val) {
  /*
  int answer = 1;
  while (answer < val) answer <<= 1;
  return answer;
  */
  if (val < 1) val = 1;
  --val;
  val |= val>>1;
  val |= val>>2;
  val |= val>>4;
  val |= val>>8;
  val |= val>>16;
  ++val;
  return val;
}
#endif


//==========================================================================
//
//  AngleMod
//
//==========================================================================
/*
float AngleMod (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < 0.0) angle += 360.0;
  while (angle >= 360.0) angle -= 360.0;
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}
*/


//==========================================================================
//
//  AngleMod180
//
//==========================================================================
/*
float AngleMod180 (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < -180.0) angle += 360.0;
  while (angle >= 180.0) angle -= 360.0;
#else
  angle += 180;
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
  angle -= 180;
#endif
  return angle;
}
*/
