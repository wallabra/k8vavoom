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
#include "core.h"


BJPRNGCtx g_bjprng_ctx;


//==========================================================================
//
//  RandomInit
//
//  call this to seed with random seed
//
//==========================================================================
void RandomInit () {
  vint32 rn;
  do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
  bjprng_raninit(&g_bjprng_ctx, rn);
}


//==========================================================================
//
//  Random
//
//==========================================================================
float Random () {
  // tries to be uniform by using rejecting
  for (;;) {
    float v = ((double)(GenRandomU31()))/((double)0x7fffffffu);
    if (!isFiniteF(v)) continue;
    if (v < 1.0f) return v;
  }
}


//==========================================================================
//
//  RandomFull
//
//==========================================================================
float RandomFull () {
  for (;;) {
    float v = ((double)(GenRandomU31()))/((double)0x7fffffffu);
    if (!isFiniteF(v)) continue;
    return v;
  }
}


//==========================================================================
//
//  RandomBetween
//
//==========================================================================
float RandomBetween (float minv, float maxv) {
  if (!isFiniteF(minv)) {
    if (!isFiniteF(maxv)) return 0.0f;
    return maxv;
  } else if (!isFiniteF(maxv)) {
    return minv;
  }
  double range = maxv-minv;
  if (range == 0) return minv;
  if (range < 0) { float tmp = minv; minv = maxv; maxv = tmp; range = -range; }
  for (;;) {
    float v = minv+(((double)(GenRandomU31()))/((double)0x7fffffffu)*range);
    if (!isFiniteF(v)) continue;
    return v;
  }
}
