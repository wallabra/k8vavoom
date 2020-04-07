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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
typedef struct BJPRNGCtx_t {
  vuint32 a, b, c, d;
} BJPRNGCtx;

#define bjprng_rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static inline VVA_OKUNUSED VVA_CHECKRESULT
vuint32 bjprng_ranval (BJPRNGCtx *x) {
  vuint32 e = x->a-bjprng_rot(x->b, 27);
  x->a = x->b^bjprng_rot(x->c, 17);
  x->b = x->c+x->d;
  x->c = x->d+e;
  x->d = e+x->a;
  return x->d;
}

static inline VVA_OKUNUSED void bjprng_raninit (BJPRNGCtx *x, vuint32 seed) {
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
static VVA_OKUNUSED inline vuint64 splitmix64_next (vuint64 *state) {
  vuint64 result = *state;
  *state = result+(vuint64)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(vuint64)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(vuint64)0x94D049BB133111EBULL;
  return result^(result>>31);
}

static VVA_OKUNUSED inline void splitmix64_seedU32 (vuint64 *state, vuint32 seed) {
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

static VVA_OKUNUSED inline void splitmix64_seedU64 (vuint64 *state, vuint32 seed0, vuint32 seed1) {
  // hashU32
  vuint32 res = seed0;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  vuint64 n = res;
  n <<= 32;
  // hashU32
  res = seed1;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  n |= res;
  *state = n;
}


// ////////////////////////////////////////////////////////////////////////// //
//**************************************************************************
// *Really* minimal PCG32_64 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
typedef struct __attribute__((packed)) {
  /*
  vuint64 state; // rng state: all values are possible
  vuint64 inc; // controls which RNG sequence (stream) is selected; must *always* be odd
  */
  vint32 lo, hi;
} PCG3264Ctx_ClassChecker;

typedef vuint64 PCG3264_Ctx;

#if defined(__cplusplus)
  static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#else
  _Static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#endif


static VVA_OKUNUSED inline void pcg3264_init (PCG3264_Ctx *rng) {
  *rng/*->state*/ = 0x853c49e6748fea9bULL;
  /*rng->inc = 0xda3e39cb94b95bdbULL;*/
}

static VVA_OKUNUSED void pcg3264_seedU32 (PCG3264_Ctx *rng, vuint32 seed) {
  vuint64 smx;
  splitmix64_seedU32(&smx, seed);
  *rng/*->state*/ = splitmix64_next(&smx);
  /*rng->inc = splitmix64(&smx)|1u;*/
}

static VVA_OKUNUSED inline vuint32 pcg3264_next (PCG3264_Ctx *rng) {
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

static VVA_OKUNUSED inline void pcg32_seedU32 (PCG32_Ctx *rng, vuint32 seed) {
  vuint64 smx;
  splitmix64_seedU32(&smx, seed);
  const vuint32 incr = (vuint32)splitmix64_next(&smx)|1u; // why not?
  *rng = incr+seed;
  *rng = (*rng)*747796405U+incr;
}

static VVA_OKUNUSED inline void pcg32_init (PCG32_Ctx *rng) {
  pcg32_seedU32(rng, 0x29au);
}

static VVA_OKUNUSED inline vuint32 pcg32_next (PCG32_Ctx *rng) {
  const vuint32 oldstate = *rng;
  *rng = oldstate*747796405U+2891336453U; //pcg_oneseq_32_step_r(rng);
  //return (vuint16)(((oldstate>>11u)^oldstate)>>((oldstate>>30u)+11u)); //pcg_output_xsh_rs_32_16(oldstate);
  //pcg_output_rxs_m_xs_32_32(oldstate);
  const vuint32 word = ((oldstate>>((oldstate>>28u)+4u))^oldstate)*277803737u;
  return (word>>22u)^word;
}


//**************************************************************************
// Bob Jenkins' ISAAC Crypto-PRNG
//**************************************************************************
// this will throw away four initial blocks, in attempt to avoid
// potential weakness in the first 8192 bits of output.
// this is deemed to be unnecessary, though.
#define ISAAC_BETTER_INIT


#define ISAAC_RAND_SIZE_SHIFT  (8)
#define ISAAC_RAND_SIZE        (1u<<ISAAC_RAND_SIZE_SHIFT)

// context of random number generator
typedef struct {
  vuint32 randcnt;
  vuint32 randrsl[ISAAC_RAND_SIZE];
  vuint32 randmem[ISAAC_RAND_SIZE];
  vuint32 randa;
  vuint32 randb;
  vuint32 randc;
} ISAAC_Ctx;


#define ISAAC_MIX(a,b,c,d,e,f,g,h)  { \
  a ^= b<<11;               d += a; b += c; \
  b ^= (c&0xffffffffu)>>2;  e += b; c += d; \
  c ^= d<<8;                f += c; d += e; \
  d ^= (e&0xffffffffu)>>16; g += d; e += f; \
  e ^= f<<10;               h += e; f += g; \
  f ^= (g&0xffffffffu)>>4;  a += f; g += h; \
  g ^= h<<8;                b += g; h += a; \
  h ^= (a&0xffffffffu)>>9;  c += h; a += b; \
}


#define ISAAC_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RAND_SIZE-1)])
#define ISAAC_step(mix,a,b,mm,m,m2,r,x)  { \
  x = *m;  \
  a = ((a^(mix))+(*(m2++))); \
  *(m++) = y = (ISAAC_ind(mm, x)+a+b); \
  *(r++) = b = (ISAAC_ind(mm, y>>ISAAC_RAND_SIZE_SHIFT)+x)&0xffffffffu; \
}


static VVA_OKUNUSED void ISAAC_nextblock (ISAAC_Ctx *ctx) {
  vuint32 x, y, *m, *m2, *mend;
  vuint32 *mm = ctx->randmem;
  vuint32 *r = ctx->randrsl;
  vuint32 a = ctx->randa;
  vuint32 b = ctx->randb+(++ctx->randc);
  for (m = mm, mend = m2 = m+(ISAAC_RAND_SIZE/2u); m < mend; ) {
    ISAAC_step(a<<13, a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>6 , a, b, mm, m, m2, r, x);
    ISAAC_step(a<<2 , a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>16, a, b, mm, m, m2, r, x);
  }
  for (m2 = mm; m2 < mend; ) {
    ISAAC_step(a<<13, a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>6 , a, b, mm, m, m2, r, x);
    ISAAC_step(a<<2 , a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>16, a, b, mm, m, m2, r, x);
  }
  ctx->randa = a;
  ctx->randb = b;
}


// if (flag==TRUE), then use the contents of randrsl[0..ISAAC_RAND_SIZE-1] to initialize mm[]
static VVA_OKUNUSED void ISAAC_init (ISAAC_Ctx *ctx, unsigned flag) {
  vuint32 a, b, c, d, e, f, g, h;
  vuint32 *m;
  vuint32 *r;

  ctx->randa = ctx->randb = ctx->randc = 0;
  m = ctx->randmem;
  r = ctx->randrsl;
  a = b = c = d = e = f = g = h = 0x9e3779b9u; // the golden ratio

  // scramble it
  for (unsigned i = 0; i < 4u; ++i) ISAAC_MIX(a, b, c, d, e, f, g, h);

  if (flag) {
    // initialize using the contents of r[] as the seed
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      a += r[i  ]; b += r[i+1];
      c += r[i+2]; d += r[i+3];
      e += r[i+4]; f += r[i+5];
      g += r[i+6]; h += r[i+7];
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
    // do a second pass to make all of the seed affect all of m
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      a += m[i  ]; b += m[i+1];
      c += m[i+2]; d += m[i+3];
      e += m[i+4]; f += m[i+5];
      g += m[i+6]; h += m[i+7];
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
  } else {
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      // fill in mm[] with messy stuff
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
  }

  #ifdef ISAAC_BETTER_INIT
  // throw away first 8192 bits
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  #endif
  ISAAC_nextblock(ctx); // fill in the first set of results
  ctx->randcnt = ISAAC_RAND_SIZE; // prepare to use the first set of results
}


// call to retrieve a single 32-bit random value
static VVA_OKUNUSED inline vuint32 ISAAC_next (ISAAC_Ctx *ctx) {
  return
   ctx->randcnt-- ?
     (vuint32)ctx->randrsl[ctx->randcnt] :
     (ISAAC_nextblock(ctx), ctx->randcnt = ISAAC_RAND_SIZE-1, (vuint32)ctx->randrsl[ctx->randcnt]);
}


// ////////////////////////////////////////////////////////////////////////// //
// initialized with `RandomInit()`
//extern BJPRNGCtx g_bjprng_ctx;
extern PCG3264_Ctx g_pcg3264_ctx;

void RandomInit (); // call this to seed with random seed


static inline VVA_OKUNUSED VVA_CHECKRESULT
//vuint32 GenRandomU31 () { return bjprng_ranval(&g_bjprng_ctx)&0x7fffffffu; }
vuint32 GenRandomU31 () { return pcg3264_next(&g_pcg3264_ctx)&0x7fffffffu; }

static inline VVA_OKUNUSED VVA_CHECKRESULT
vuint32 GenRandomU32 () { return pcg3264_next(&g_pcg3264_ctx)&0xffffffffu; }


VVA_CHECKRESULT float Random (); // [0..1)
VVA_CHECKRESULT float RandomFull (); // [0..1]
VVA_CHECKRESULT float RandomBetween (float minv, float maxv); // [minv..maxv]
// [0..255]
static inline VVA_OKUNUSED VVA_CHECKRESULT
//vuint8 P_Random () { return bjprng_ranval(&g_bjprng_ctx)&0xff; }
vuint8 P_Random () { return pcg3264_next(&g_pcg3264_ctx)&0xff; }
