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

// http://burtleburtle.net/bob/rand/smallprng.html
struct BJPRNGCtx {
  vuint32 a, b, c, d;
};

#define bjprng_rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint32 bjprng_ranval (BJPRNGCtx *x) {
  vuint32 e = x->a-bjprng_rot(x->b, 27);
  x->a = x->b^bjprng_rot(x->c, 17);
  x->b = x->c+x->d;
  x->c = x->d+e;
  x->d = e+x->a;
  return x->d;
}

static inline __attribute__((unused)) void bjprng_raninit (BJPRNGCtx *x, vuint32 seed) {
  x->a = 0xf1ea5eed;
  x->b = x->c = x->d = seed;
  for (unsigned i = 0; i < 32; ++i) {
    //(void)bjprng_ranval(x);
    vuint32 e = x->a-bjprng_rot(x->b, 27);
    x->a = x->b^bjprng_rot(x->c, 17);
    x->b = x->c+x->d;
    x->c = x->d+e;
    x->d = e+x->a;
  }
}


// initialized with `RandomInit()`
extern BJPRNGCtx g_bjprng_ctx;

void RandomInit (); // call this to seed with random seed


static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint32 GenRandomU31 () { return bjprng_ranval(&g_bjprng_ctx)&0x7fffffffu; }


__attribute__((warn_unused_result)) float Random (); // [0..1)
__attribute__((warn_unused_result)) float RandomFull (); // [0..1]
__attribute__((warn_unused_result)) float RandomBetween (float minv, float maxv); // [minv..maxv]
// [0..255]
static inline __attribute__((unused)) __attribute__((warn_unused_result))
vuint8 P_Random () { return bjprng_ranval(&g_bjprng_ctx)&0xff; }
