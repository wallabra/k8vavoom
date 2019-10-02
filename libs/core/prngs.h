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

// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
// SplitMix; mostly used to generate 64-bit seeds
static __attribute__((unused)) inline vuint64 splitmix64_next (vuint64 *state) {
  vuint64 result = *state;
  *state = result+(vuint64)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(vuint64)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(vuint64)0x94D049BB133111EBULL;
  return result^(result>>31);
}

static __attribute__((unused)) inline void splitmix64_seedU32 (vuint64 *state, vuint32 seed) {
  // hashU32
  vuint32 res = seed;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  vuint64 n = res;
  n <<= 32;
  n |= seed;
  *state = n;
}


// ////////////////////////////////////////////////////////////////////////// //
//**************************************************************************
// *Really* minimal PCG32_64 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
/*
typedef struct {
  vuint64 state; // rng state: all values are possible
  vuint64 inc; // controls which RNG sequence (stream) is selected; must *always* be odd
} PCG3264_Ctx;
*/
typedef vuint64 PCG3264_Ctx;

static __attribute__((unused)) inline void pcg3264_init (PCG3264_Ctx *rng) {
  *rng/*->state*/ = 0x853c49e6748fea9bULL;
  /*rng->inc = 0xda3e39cb94b95bdbULL;*/
}

static __attribute__((unused)) void pcg3264_seedU32 (PCG3264_Ctx *rng, vuint32 seed) {
  vuint64 smx;
  splitmix64_seedU32(&smx, seed);
  *rng/*->state*/ = splitmix64_next(&smx);
  /*rng->inc = splitmix64(&smx)|1u;*/
}

static __attribute__((unused)) inline vuint32 pcg3264_next (PCG3264_Ctx *rng) {
  const vuint64 oldstate = *rng/*->state*/;
  // advance internal state
  *rng/*->state*/ = oldstate*(vuint64)6364136223846793005ULL+(/*rng->inc|1u*/(vuint64)1442695040888963407ULL);
  // calculate output function (XSH RR), uses old state for max ILP
  const vuint32 xorshifted = ((oldstate>>18)^oldstate)>>27;
  const vuint8 rot = oldstate>>59;
  //return (xorshifted>>rot)|(xorshifted<<((-rot)&31));
  return (xorshifted>>rot)|(xorshifted<<(32-rot));
}


// ////////////////////////////////////////////////////////////////////////// //
// this one doesn't repeat values (it seems)
//**************************************************************************
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
typedef vuint32 PCG32_Ctx;

static __attribute__((unused)) inline void pcg32_seedU32 (PCG32_Ctx *rng, vuint32 seed) {
  vuint64 smx;
  splitmix64_seedU32(&smx, seed);
  const vuint32 incr = (vuint32)splitmix64_next(&smx)|1u; // why not?
  *rng = incr+seed;
  *rng = (*rng)*747796405U+incr;
}

static __attribute__((unused)) inline void pcg32_init (PCG32_Ctx *rng) {
  pcg32_seedU32(rng, 0x29au);
}

static __attribute__((unused)) inline vuint32 pcg32_next (PCG32_Ctx *rng) {
  const vuint32 oldstate = *rng;
  *rng = oldstate*747796405U+2891336453U; //pcg_oneseq_32_step_r(rng);
  //return (vuint16)(((oldstate>>11u)^oldstate)>>((oldstate>>30u)+11u)); //pcg_output_xsh_rs_32_16(oldstate);
  //pcg_output_rxs_m_xs_32_32(oldstate);
  const vuint32 word = ((oldstate>>((oldstate>>28u)+4u))^oldstate)*277803737u;
  return (word>>22u)^word;
}


// ////////////////////////////////////////////////////////////////////////// //
// initialized with `RandomInit()`
//extern BJPRNGCtx g_bjprng_ctx;
extern PCG3264_Ctx g_pcg3264_ctx;

void RandomInit (); // call this to seed with random seed


static inline __attribute__((unused)) __attribute__((warn_unused_result))
//vuint32 GenRandomU31 () { return bjprng_ranval(&g_bjprng_ctx)&0x7fffffffu; }
vuint32 GenRandomU31 () { return pcg3264_next(&g_pcg3264_ctx)&0x7fffffffu; }


__attribute__((warn_unused_result)) float Random (); // [0..1)
__attribute__((warn_unused_result)) float RandomFull (); // [0..1]
__attribute__((warn_unused_result)) float RandomBetween (float minv, float maxv); // [minv..maxv]
// [0..255]
static inline __attribute__((unused)) __attribute__((warn_unused_result))
//vuint8 P_Random () { return bjprng_ranval(&g_bjprng_ctx)&0xff; }
vuint8 P_Random () { return pcg3264_next(&g_pcg3264_ctx)&0xff; }
