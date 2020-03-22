//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020 Ketmar Dark
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <time.h>
#ifdef _WIN32
# include <windows.h>
typedef int socklen_t;
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
//#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>


#include "netchan.h"

#ifdef WIN32
static bool vnetchanSockInited = false;

class TWinSockHelper {
public:
  ~TWinSockHelper () { if (vnetchanSockInited) WSACleanup(); vnetchanSockInited = false; }
};

static TWinSockHelper vnetchanHelper__;
#endif


// ////////////////////////////////////////////////////////////////////////// //
// SplitMix; mostly used to generate 64-bit seeds
static __attribute__((unused)) inline uint64_t splitmix64_next (uint64_t *state) {
  uint64_t result = *state;
  *state = result+(uint64_t)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(uint64_t)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(uint64_t)0x94D049BB133111EBULL;
  return result^(result>>31);
}

static __attribute__((unused)) inline void splitmix64_seedU32 (uint64_t *state, uint32_t seed) {
  // hashU32
  uint32_t res = seed;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  uint64_t n = res;
  n <<= 32;
  n |= seed;
  *state = n;
}


//**************************************************************************
// *Really* minimal PCG32_64 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
typedef struct __attribute__((packed)) {
  /*
  uint64_t state; // rng state: all values are possible
  uint64_t inc; // controls which RNG sequence (stream) is selected; must *always* be odd
  */
  uint32_t lo, hi;
} PCG3264Ctx_ClassChecker;

typedef uint64_t PCG3264_Ctx;

#if defined(__cplusplus)
  static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#else
  _Static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#endif


static __attribute__((unused)) inline void pcg3264_init (PCG3264_Ctx *rng) {
  *rng/*->state*/ = 0x853c49e6748fea9bULL;
  /*rng->inc = 0xda3e39cb94b95bdbULL;*/
}

static __attribute__((unused)) void pcg3264_seedU32 (PCG3264_Ctx *rng, uint32_t seed) {
  uint64_t smx;
  splitmix64_seedU32(&smx, seed);
  *rng/*->state*/ = splitmix64_next(&smx);
  /*rng->inc = splitmix64(&smx)|1u;*/
}

static __attribute__((unused)) inline uint32_t pcg3264_next (PCG3264_Ctx *rng) {
  const uint64_t oldstate = *rng/*->state*/;
  // advance internal state
  *rng/*->state*/ = oldstate*(uint64_t)6364136223846793005ULL+(/*rng->inc|1u*/(uint64_t)1442695040888963407ULL);
  // calculate output function (XSH RR), uses old state for max ILP
  const uint32_t xorshifted = ((oldstate>>18)^oldstate)>>27;
  const uint8_t rot = oldstate>>59;
  //return (xorshifted>>rot)|(xorshifted<<((-rot)&31));
  return (xorshifted>>rot)|(xorshifted<<(32-rot));
}


static PCG3264_Ctx g_pcg3264_ctx;
static bool g_pgc_inited = false;


static inline __attribute__((unused)) uint32_t GenRandomU32 () noexcept { return pcg3264_next(&g_pcg3264_ctx)&0xffffffffu; }


/*
  ISAAC+ "variant", the paper is not clear on operator precedence and other
  things. This is the "first in, first out" option!

  Not threadsafe or securely initialized, only for deterministic testing
*/
typedef struct isaacp_state_t {
  uint32_t state[256];
  unsigned char buffer[1024];
  union __attribute__((packed)) {
    uint32_t abc[3];
    struct __attribute__((packed)) {
      uint32_t a, b, c;
    };
  };
  size_t left;
} isaacp_state;

/* endian */
static inline void U32TO8_LE (unsigned char *p, const uint32_t v) {
  p[0] = (unsigned char)(v      );
  p[1] = (unsigned char)(v >>  8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

#define ROTL32(a,b) (((a) << (b)) | ((a) >> (32 - b)))
#define ROTR32(a,b) (((a) >> (b)) | ((a) << (32 - b)))

#define isaacp_step(offset, mix) \
  x = mm[i + offset]; \
  a = (a ^ (mix)) + (mm[(i + offset + 128) & 0xff]); \
  y = (a ^ b) + mm[(x >> 2) & 0xff]; \
  mm[i + offset] = y; \
  b = (x + a) ^ mm[(y >> 10) & 0xff]; \
  U32TO8_LE(out + (i + offset) * 4, b);

static void isaacp_mix (isaacp_state *st) {
  uint32_t i, x, y;
  uint32_t a = st->a, b = st->b, c = st->c;
  uint32_t *mm = st->state;
  unsigned char *out = st->buffer;

  c = c + 1;
  b = b + c;

  for (i = 0; i < 256; i += 4) {
    isaacp_step(0, ROTL32(a,13))
    isaacp_step(1, ROTR32(a, 6))
    isaacp_step(2, ROTL32(a, 2))
    isaacp_step(3, ROTR32(a,16))
  }

  st->a = a;
  st->b = b;
  st->c = c;
  st->left = 1024;
}


static void isaacp_random (isaacp_state *st, void *p, size_t len) {
  size_t use;
  unsigned char *c = (unsigned char *)p;
  while (len) {
    use = (len > st->left) ? st->left : len;
    memcpy(c, st->buffer + (sizeof(st->buffer) - st->left), use);

    st->left -= use;
    c += use;
    len -= use;

    if (!st->left)
      isaacp_mix(st);
  }
}


#ifdef WIN32
// ////////////////////////////////////////////////////////////////////////// //
// SplitMix; mostly used to generate 64-bit seeds
/*
static __attribute__((unused)) inline uint64_t splitmix64_next (uint64_t *state) {
  uint64_t result = *state;
  *state = result+(uint64_t)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(uint64_t)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(uint64_t)0x94D049BB133111EBULL;
  return result^(result>>31);
}
*/

static __attribute__((unused)) inline void splitmix64_seedU64 (uint64_t *state, uint32_t seed0, uint32_t seed1) {
  // hashU32
  uint32_t res = seed0;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  uint64_t n = res;
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

//#include <ntsecapi.h>
typedef BOOLEAN WINAPI (*RtlGenRandomFn) (PVOID RandomBuffer,ULONG RandomBufferLength);

static void RtlGenRandomX (PVOID RandomBuffer, ULONG RandomBufferLength) {
  if (RandomBufferLength <= 0) return;
  static __thread RtlGenRandomFn RtlGenRandomXX = NULL;
  static __thread int inited = 0;
  if (!inited) {
    inited = 1;
    HANDLE libh = LoadLibrary("advapi32.dll");
    if (libh) {
      RtlGenRandomXX = (RtlGenRandomFn)(void *)GetProcAddress(libh, "SystemFunction036");
      if (!RtlGenRandomXX) fprintf(stderr, "WARNING: `RtlGenRandom()` is not found!\n");
      //else fprintf(stderr, "MESSAGE: `RtlGenRandom()` found!\n");
    }
  }
  if (RtlGenRandomXX) {
    if (RtlGenRandomXX(RandomBuffer, RandomBufferLength)) return;
    fprintf(stderr, "WARNING: `RtlGenRandom()` fallback for %u bytes!\n", (unsigned)RandomBufferLength);
  }
  static __thread int initialized = 0;
  static __thread isaacp_state rng;
  // need init?
  if (!initialized) {
    // initialise isaacp with some shit
    initialized = 1;
    uint32_t smxseed0 = 0;
    uint32_t smxseed1 = (uint32_t)GetCurrentProcessId();
    SYSTEMTIME st;
    FILETIME ft;
    GetLocalTime(&st);
    if (!SystemTimeToFileTime(&st, &ft)) {
      fprintf(stderr, "SHIT: `SystemTimeToFileTime()` failed!\n");
      smxseed0 = /*hashU32*/(uint32_t)(GetTickCount());
    } else {
      smxseed0 = /*hashU32*/(uint32_t)(ft.dwLowDateTime);
      //fprintf(stderr, "ft=0x%08x (0x%08x)\n", (uint32_t)ft.dwLowDateTime, smxseed);
    }
    uint64_t smx;
    splitmix64_seedU64(&smx, smxseed0, smxseed1);
    for (unsigned n = 0; n < 256; ++n) rng.state[n] = splitmix64_next(&smx);
    rng.a = splitmix64_next(&smx);
    rng.b = splitmix64_next(&smx);
    rng.c = splitmix64_next(&smx);
    isaacp_mix(&rng);
    isaacp_mix(&rng);
  }
  // generate random bytes with ISAAC+
  isaacp_random(&rng, RandomBuffer, RandomBufferLength);
}
#endif


static void GenerateRandomBytes (void *p, size_t len) {
  static __thread int initialized = 0;
  static __thread isaacp_state rng;

  if (!initialized) {
    memset(&rng, 0, sizeof(rng));
    #ifdef __SWITCH__
    randomGet(rng.state, sizeof(rng.state));
    randomGet(rng.abc, sizeof(rng.abc));
    #elif !defined(WIN32)
    int fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC);
    if (fd >= 0) {
      read(fd, rng.state, sizeof(rng.state));
      read(fd, rng.abc, sizeof(rng.abc));
      close(fd);
    }
    #else
    RtlGenRandomX(rng.state, sizeof(rng.state));
    RtlGenRandomX(rng.abc, sizeof(rng.abc));
    #endif
    isaacp_mix(&rng);
    isaacp_mix(&rng);
    initialized = 1;
  }

  isaacp_random(&rng, p, len);
}


//==========================================================================
//
//  InitRandom
//
//==========================================================================
static void InitRandom () noexcept {
  if (!g_pgc_inited) {
    GenerateRandomBytes(&g_pcg3264_ctx, sizeof(g_pcg3264_ctx));
    g_pgc_inited = true;
  }
}


//**************************************************************************
//
// SHA512
//
//**************************************************************************

#define SHA512_HASH_BLOCK_SIZE  (128)
//#define HASH_DIGEST_SIZE (64)

struct sha512_state_t {
  uint64_t H[8];
  uint64_t T[2];
  uint32_t leftover;
  uint8_t buffer[SHA512_HASH_BLOCK_SIZE];
};


static const uint64_t sha512_constants[80] = {
  0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
  0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
  0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
  0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
  0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
  0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
  0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
  0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
  0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
  0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
  0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
  0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
  0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
  0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
  0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
  0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
  0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
  0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
  0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
  0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

static inline uint64_t sha512_ROTR64 (const uint64_t x, const int k) { return (x>>k)|(x<<(64-k)); }

static inline uint64_t sha512_LOAD64_BE (const uint8_t *p) {
  return
    ((uint64_t)p[0]<<56)|
    ((uint64_t)p[1]<<48)|
    ((uint64_t)p[2]<<40)|
    ((uint64_t)p[3]<<32)|
    ((uint64_t)p[4]<<24)|
    ((uint64_t)p[5]<<16)|
    ((uint64_t)p[6]<<8)|
    ((uint64_t)p[7]);
}

static inline void sha512_STORE64_BE (uint8_t *p, uint64_t v) {
  p[0] = (uint8_t)(v>>56);
  p[1] = (uint8_t)(v>>48);
  p[2] = (uint8_t)(v>>40);
  p[3] = (uint8_t)(v>>32);
  p[4] = (uint8_t)(v>>24);
  p[5] = (uint8_t)(v>>16);
  p[6] = (uint8_t)(v>>8);
  p[7] = (uint8_t)(v);
}

#define Ch(x,y,z)  (z^(x&(y^z)))
#define Maj(x,y,z) (((x|y)&z)|(x&y))
#define S0(x)      (sha512_ROTR64(x, 28)^sha512_ROTR64(x, 34)^sha512_ROTR64(x, 39))
#define S1(x)      (sha512_ROTR64(x, 14)^sha512_ROTR64(x, 18)^sha512_ROTR64(x, 41))
#define G0(x)      (sha512_ROTR64(x,  1)^sha512_ROTR64(x,  8)^(x>>7))
#define G1(x)      (sha512_ROTR64(x, 19)^sha512_ROTR64(x, 61)^(x>>6))
#define W0(in,i)   (sha512_LOAD64_BE(&in[i*8]))
#define W1(i)      (G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16])
#define STEP(i) \
  t1 = S0(r[0])+Maj(r[0], r[1], r[2]); \
  t0 = r[7]+S1(r[4])+Ch(r[4], r[5], r[6])+sha512_constants[i]+w[i]; \
  r[7] = r[6]; \
  r[6] = r[5]; \
  r[5] = r[4]; \
  r[4] = r[3]+t0; \
  r[3] = r[2]; \
  r[2] = r[1]; \
  r[1] = r[0]; \
  r[0] = t0+t1;

static void sha512_blocks (sha512_state_t *S, const void *invoid, size_t blocks) {
  uint64_t r[8], w[80], t0, t1;
  size_t i;
  const uint8_t *in = (const uint8_t *)invoid;

  for (i = 0; i < 8; i++) r[i] = S->H[i];

  while (blocks--) {
    for (i =  0; i < 16; i++) { w[i] = W0(in, i); }
    for (i = 16; i < 80; i++) { w[i] = W1(i); }
    for (i =  0; i < 80; i++) { STEP(i); }
    for (i =  0; i <  8; i++) { r[i] += S->H[i]; S->H[i] = r[i]; }
    S->T[0] += SHA512_HASH_BLOCK_SIZE*8;
    S->T[1] += (!S->T[0]) ? 1 : 0;
    in += SHA512_HASH_BLOCK_SIZE;
  }
}

static void sha512_init (sha512_state_t *S) {
  S->H[0] = 0x6a09e667f3bcc908ull;
  S->H[1] = 0xbb67ae8584caa73bull;
  S->H[2] = 0x3c6ef372fe94f82bull;
  S->H[3] = 0xa54ff53a5f1d36f1ull;
  S->H[4] = 0x510e527fade682d1ull;
  S->H[5] = 0x9b05688c2b3e6c1full;
  S->H[6] = 0x1f83d9abfb41bd6bull;
  S->H[7] = 0x5be0cd19137e2179ull;
  S->T[0] = 0;
  S->T[1] = 0;
  S->leftover = 0;
}

static void sha512_update (sha512_state_t *S, const void *invoid, size_t inlen) {
  size_t blocks, want;
  const uint8_t *in = (const uint8_t *)invoid;

  /* handle the previous data */
  if (S->leftover) {
    want = (SHA512_HASH_BLOCK_SIZE-S->leftover);
    want = (want < inlen) ? want : inlen;
    memcpy(S->buffer+S->leftover, in, want);
    S->leftover += (uint32_t)want;
    if (S->leftover < SHA512_HASH_BLOCK_SIZE)
      return;
    in += want;
    inlen -= want;
    sha512_blocks(S, S->buffer, 1);
  }

  /* handle the current data */
  blocks = (inlen&~(SHA512_HASH_BLOCK_SIZE-1));
  S->leftover = (uint32_t)(inlen-blocks);
  if (blocks) {
    sha512_blocks(S, in, blocks/SHA512_HASH_BLOCK_SIZE);
    in += blocks;
  }

  /* handle leftover data */
  if (S->leftover)
    memcpy(S->buffer, in, S->leftover);
}

static void sha512_final (sha512_state_t *S, VNetChanSocket::SHA512Digest hash) {
  uint64_t t0 = S->T[0]+(S->leftover*8), t1 = S->T[1];

  S->buffer[S->leftover] = 0x80;
  if (S->leftover <= 111) {
    memset(S->buffer+S->leftover+1, 0, 111-S->leftover);
  } else {
    memset(S->buffer+S->leftover+1, 0, 127-S->leftover);
    sha512_blocks(S, S->buffer, 1);
    memset(S->buffer, 0, 112);
  }

  sha512_STORE64_BE(S->buffer+112, t1);
  sha512_STORE64_BE(S->buffer+120, t0);
  sha512_blocks(S, S->buffer, 1);

  sha512_STORE64_BE(&hash[ 0], S->H[0]);
  sha512_STORE64_BE(&hash[ 8], S->H[1]);
  sha512_STORE64_BE(&hash[16], S->H[2]);
  sha512_STORE64_BE(&hash[24], S->H[3]);
  sha512_STORE64_BE(&hash[32], S->H[4]);
  sha512_STORE64_BE(&hash[40], S->H[5]);
  sha512_STORE64_BE(&hash[48], S->H[6]);
  sha512_STORE64_BE(&hash[56], S->H[7]);
}

static void sha512_hashBuffer (VNetChanSocket::SHA512Digest hash, const void *in, size_t inlen) {
  sha512_state_t ctx;
  sha512_init(&ctx);
  sha512_update(&ctx, in, inlen);
  sha512_final(&ctx, hash);
}


//==========================================================================
//
//  GetSysTime
//
//  return value should not be zero
//
//==========================================================================
static __attribute__((unused)) double GetSysTime () noexcept {
  #if defined(WIN32)
  static uint32_t lastTickCount = 0;
  static double addSeconds = 0;
  static double lastSeconds = 0;
  uint32_t res = (uint32_t)GetTickCount();
  if (lastTickCount > res) {
    addSeconds = lastSeconds;
    lastTickCount = res;
  } else {
    lastSeconds = addSeconds+(double)res/1000.0+1.0;
  }
  return lastSeconds;
  #elif defined(__linux__)
  static bool initialized = false;
  static time_t secbase = 0;
  struct timespec ts;
  if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) abort();
  if (!initialized) {
    initialized = true;
    secbase = ts.tv_sec;
  }
  return (ts.tv_sec-secbase)+ts.tv_nsec/1000000000.0+1.0;
  #else
  static bool initialized = false;
  struct timeval tp;
  struct timezone tzp;
  static int secbase = 0;
  gettimeofday(&tp, &tzp);
  if (!initialized) {
    initialized = true;
    secbase = tp.tv_sec;
  }
  return (tp.tv_sec-secbase)+tp.tv_usec/1000000.0+1.0;
  #endif
}


//**************************************************************************
//
// VNetChanSocket
//
//**************************************************************************

//==========================================================================
//
//  VNetChanSocket::create
//
//==========================================================================
bool VNetChanSocket::create () noexcept {
  close();

  // create UDP socket
  sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == -1) return false;

  // make socket non-blocking
  int trueval = 1;
  #ifdef _WIN32
  if (ioctlsocket(sockfd, FIONBIO, (u_long*)&trueval) == -1)
  #else
  if (ioctl(sockfd, FIONBIO, (char*)&trueval) == -1)
  #endif
  {
    #ifdef _WIN32
    ::closesocket(sockfd);
    #else
    ::close(sockfd);
    #endif
    sockfd = -1;
    return false;
  }

  // bind socket to the port
  /*
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(MASTER_SERVER_PORT);
  if (bind(sockfd, (sockaddr *)&address, sizeof(address)) == -1) {
    closesocket(sockfd);
    Logf("Unable to bind socket to a port");
    return -1;
  }
  */

  return true;
}


//==========================================================================
//
//  VNetChanSocket::bindToPort
//
//  bind socket to the port, with INADDR_ANY
//
//==========================================================================
bool VNetChanSocket::bindToPort (uint16_t port) noexcept {
  if (!port || sockfd <= 0) return false;
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(sockfd, (sockaddr *)&address, sizeof(address)) == -1) {
    return false;
  }
  return true;
}


//==========================================================================
//
//  VNetChanSocket::close
//
//==========================================================================
void VNetChanSocket::close () noexcept {
  if (sockfd >= 0) {
    #ifdef _WIN32
    ::closesocket(sockfd);
    #else
    ::close(sockfd);
    #endif
  }
  sockfd = -1;
}


//==========================================================================
//
//  VNetChanSocket::send
//
//==========================================================================
bool VNetChanSocket::send (const sockaddr *addr, const void *buf, int len) noexcept {
  if (sockfd < 0 || !addr) return false;
  if (len < 0) return false;
  if (len == 0) return true;
  if (!buf) return false;
  return (::sendto(sockfd, buf, len, 0, addr, sizeof(*addr)) == len);
}


//==========================================================================
//
//  VNetChanSocket::hasData
//
//==========================================================================
bool VNetChanSocket::hasData () noexcept {
  if (sockfd < 0) return false;
  uint8_t buf[MAX_DGRAM_SIZE];
  return (recvfrom(sockfd, buf, MAX_DGRAM_SIZE, MSG_PEEK, nullptr, nullptr) > 0);
}


//==========================================================================
//
//  VNetChanSocket::recv
//
//  <0: error
//  0: no data
//  >0: data length
//  `addr` is sender address
//
//==========================================================================
int VNetChanSocket::recv (sockaddr *addr, void *buf, int maxlen) noexcept {
  if (maxlen < 0) return -1;
  if (maxlen == 0) return 0;
  if (!buf) return -1;
  sockaddr tmpaddr;
  if (!addr) addr = &tmpaddr;
  socklen_t addrlen = sizeof(sockaddr);
  int len;
  for (;;) {
    len = (int)recvfrom(sockfd, buf, maxlen, 0, addr, &addrlen);
    if (len < 0) {
      if (errno == EINTR) continue;
      len = (errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1);
    }
    break;
  }
  return len;
}


//==========================================================================
//
//  VNetChanSocket::AddrToString
//
//==========================================================================
const char *VNetChanSocket::AddrToString (const sockaddr *addr) noexcept {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d:%d", (haddr>>24)&0xff, (haddr>>16)&0xff,
    (haddr>>8)&0xff, haddr&0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
}


//==========================================================================
//
//  VNetChanSocket::AddrToStringNoPort
//
//==========================================================================
const char *VNetChanSocket::AddrToStringNoPort (const sockaddr *addr) noexcept {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", (haddr>>24)&0xff, (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff);
  return buffer;
}


//==========================================================================
//
//  VNetChanSocket::AddrEqu
//
//==========================================================================
bool VNetChanSocket::AddrEqu (const sockaddr *addr1, const sockaddr *addr2) noexcept {
  if (addr1 == addr2) return (addr1 != nullptr);
  if (addr1->sa_family != addr2->sa_family) return false;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return false;
  if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return false;
  return true;
}


//==========================================================================
//
//  VNetChanSocket::AddrEquNoPort
//
//==========================================================================
bool VNetChanSocket::AddrEquNoPort (const sockaddr *addr1, const sockaddr *addr2) noexcept {
  if (addr1 == addr2) return (addr1 != nullptr);
  if (addr1->sa_family != addr2->sa_family) return false;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return false;
  return true;
}


//==========================================================================
//
//  VNetChanSocket::GetAddrFromName
//
//==========================================================================
bool VNetChanSocket::GetAddrFromName (const char *hostname, sockaddr *addr, uint16_t port) noexcept {
  if (!addr) return false;
  memset((void *)addr, 0, sizeof(*addr));

  if (!hostname || !hostname[0]) return false;

  hostent *hostentry = gethostbyname(hostname);
  if (!hostentry) return false;

  addr->sa_family = AF_INET;
  ((sockaddr_in *)addr)->sin_port = htons(port);
  ((sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

  return true;
}


//==========================================================================
//
//  VNetChanSocket::InitialiseSockets
//
//==========================================================================
bool VNetChanSocket::InitialiseSockets () noexcept {
  #ifdef WIN32
  if (!vnetchanSockInited) {
    WSADATA winsockdata;
    //MAKEWORD(2, 2)
    int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);
    if (r) return false;
    vnetchanSockInited = true;
  }
  #endif
  return true;
}


//==========================================================================
//
//  VNetChanSocket::GetTime
//
//==========================================================================
double VNetChanSocket::GetTime () noexcept {
  return GetSysTime();
}


//==========================================================================
//
//  VNetChanSocket::TVMsecs
//
//==========================================================================
void VNetChanSocket::TVMsecs (timeval *dest, int msecs) noexcept {
  if (!dest) return;
  if (msecs < 0) msecs = 0;
  dest->tv_sec = msecs/1000;
  dest->tv_usec = msecs%1000;
  dest->tv_usec *= 100000;
}


uint32_t VNetChanSocket::GenRandomU32 () noexcept {
  if (!g_pgc_inited) InitRandom();
  return ::GenRandomU32();
}


/* from RFC 3309 */
#define CRC32C_POLY       0x1EDC6F41U
#define CRC32C_STEP(c,d)  (c=(c>>8)^crc_c[(c^(d))&0xffU])

static const uint32_t crc_c[256] = {
  0x00000000U, 0xF26B8303U, 0xE13B70F7U, 0x1350F3F4U,
  0xC79A971FU, 0x35F1141CU, 0x26A1E7E8U, 0xD4CA64EBU,
  0x8AD958CFU, 0x78B2DBCCU, 0x6BE22838U, 0x9989AB3BU,
  0x4D43CFD0U, 0xBF284CD3U, 0xAC78BF27U, 0x5E133C24U,
  0x105EC76FU, 0xE235446CU, 0xF165B798U, 0x030E349BU,
  0xD7C45070U, 0x25AFD373U, 0x36FF2087U, 0xC494A384U,
  0x9A879FA0U, 0x68EC1CA3U, 0x7BBCEF57U, 0x89D76C54U,
  0x5D1D08BFU, 0xAF768BBCU, 0xBC267848U, 0x4E4DFB4BU,
  0x20BD8EDEU, 0xD2D60DDDU, 0xC186FE29U, 0x33ED7D2AU,
  0xE72719C1U, 0x154C9AC2U, 0x061C6936U, 0xF477EA35U,
  0xAA64D611U, 0x580F5512U, 0x4B5FA6E6U, 0xB93425E5U,
  0x6DFE410EU, 0x9F95C20DU, 0x8CC531F9U, 0x7EAEB2FAU,
  0x30E349B1U, 0xC288CAB2U, 0xD1D83946U, 0x23B3BA45U,
  0xF779DEAEU, 0x05125DADU, 0x1642AE59U, 0xE4292D5AU,
  0xBA3A117EU, 0x4851927DU, 0x5B016189U, 0xA96AE28AU,
  0x7DA08661U, 0x8FCB0562U, 0x9C9BF696U, 0x6EF07595U,
  0x417B1DBCU, 0xB3109EBFU, 0xA0406D4BU, 0x522BEE48U,
  0x86E18AA3U, 0x748A09A0U, 0x67DAFA54U, 0x95B17957U,
  0xCBA24573U, 0x39C9C670U, 0x2A993584U, 0xD8F2B687U,
  0x0C38D26CU, 0xFE53516FU, 0xED03A29BU, 0x1F682198U,
  0x5125DAD3U, 0xA34E59D0U, 0xB01EAA24U, 0x42752927U,
  0x96BF4DCCU, 0x64D4CECFU, 0x77843D3BU, 0x85EFBE38U,
  0xDBFC821CU, 0x2997011FU, 0x3AC7F2EBU, 0xC8AC71E8U,
  0x1C661503U, 0xEE0D9600U, 0xFD5D65F4U, 0x0F36E6F7U,
  0x61C69362U, 0x93AD1061U, 0x80FDE395U, 0x72966096U,
  0xA65C047DU, 0x5437877EU, 0x4767748AU, 0xB50CF789U,
  0xEB1FCBADU, 0x197448AEU, 0x0A24BB5AU, 0xF84F3859U,
  0x2C855CB2U, 0xDEEEDFB1U, 0xCDBE2C45U, 0x3FD5AF46U,
  0x7198540DU, 0x83F3D70EU, 0x90A324FAU, 0x62C8A7F9U,
  0xB602C312U, 0x44694011U, 0x5739B3E5U, 0xA55230E6U,
  0xFB410CC2U, 0x092A8FC1U, 0x1A7A7C35U, 0xE811FF36U,
  0x3CDB9BDDU, 0xCEB018DEU, 0xDDE0EB2AU, 0x2F8B6829U,
  0x82F63B78U, 0x709DB87BU, 0x63CD4B8FU, 0x91A6C88CU,
  0x456CAC67U, 0xB7072F64U, 0xA457DC90U, 0x563C5F93U,
  0x082F63B7U, 0xFA44E0B4U, 0xE9141340U, 0x1B7F9043U,
  0xCFB5F4A8U, 0x3DDE77ABU, 0x2E8E845FU, 0xDCE5075CU,
  0x92A8FC17U, 0x60C37F14U, 0x73938CE0U, 0x81F80FE3U,
  0x55326B08U, 0xA759E80BU, 0xB4091BFFU, 0x466298FCU,
  0x1871A4D8U, 0xEA1A27DBU, 0xF94AD42FU, 0x0B21572CU,
  0xDFEB33C7U, 0x2D80B0C4U, 0x3ED04330U, 0xCCBBC033U,
  0xA24BB5A6U, 0x502036A5U, 0x4370C551U, 0xB11B4652U,
  0x65D122B9U, 0x97BAA1BAU, 0x84EA524EU, 0x7681D14DU,
  0x2892ED69U, 0xDAF96E6AU, 0xC9A99D9EU, 0x3BC21E9DU,
  0xEF087A76U, 0x1D63F975U, 0x0E330A81U, 0xFC588982U,
  0xB21572C9U, 0x407EF1CAU, 0x532E023EU, 0xA145813DU,
  0x758FE5D6U, 0x87E466D5U, 0x94B49521U, 0x66DF1622U,
  0x38CC2A06U, 0xCAA7A905U, 0xD9F75AF1U, 0x2B9CD9F2U,
  0xFF56BD19U, 0x0D3D3E1AU, 0x1E6DCDEEU, 0xEC064EEDU,
  0xC38D26C4U, 0x31E6A5C7U, 0x22B65633U, 0xD0DDD530U,
  0x0417B1DBU, 0xF67C32D8U, 0xE52CC12CU, 0x1747422FU,
  0x49547E0BU, 0xBB3FFD08U, 0xA86F0EFCU, 0x5A048DFFU,
  0x8ECEE914U, 0x7CA56A17U, 0x6FF599E3U, 0x9D9E1AE0U,
  0xD3D3E1ABU, 0x21B862A8U, 0x32E8915CU, 0xC083125FU,
  0x144976B4U, 0xE622F5B7U, 0xF5720643U, 0x07198540U,
  0x590AB964U, 0xAB613A67U, 0xB831C993U, 0x4A5A4A90U,
  0x9E902E7BU, 0x6CFBAD78U, 0x7FAB5E8CU, 0x8DC0DD8FU,
  0xE330A81AU, 0x115B2B19U, 0x020BD8EDU, 0xF0605BEEU,
  0x24AA3F05U, 0xD6C1BC06U, 0xC5914FF2U, 0x37FACCF1U,
  0x69E9F0D5U, 0x9B8273D6U, 0x88D28022U, 0x7AB90321U,
  0xAE7367CAU, 0x5C18E4C9U, 0x4F48173DU, 0xBD23943EU,
  0xF36E6F75U, 0x0105EC76U, 0x12551F82U, 0xE03E9C81U,
  0x34F4F86AU, 0xC69F7B69U, 0xD5CF889DU, 0x27A40B9EU,
  0x79B737BAU, 0x8BDCB4B9U, 0x988C474DU, 0x6AE7C44EU,
  0xBE2DA0A5U, 0x4C4623A6U, 0x5F16D052U, 0xAD7D5351U,
};


//==========================================================================
//
//  VNetChanSocket::CRC32C
//
//==========================================================================
uint32_t VNetChanSocket::CRC32C (uint32_t crc32, const void *buf, size_t length) noexcept {
  crc32 = ~crc32;
  const uint8_t *buffer = (const uint8_t *)buf;
  for (size_t i = 0; i < length; ++i) {
    CRC32C_STEP(crc32, buffer[i]);
  }
  crc32 = ~crc32;
  return crc32;
}


//**************************************************************************
//
// ChaCha20
//
//**************************************************************************

static inline uint32_t CHACHA20_U8TO32_LITTLE (const void *p) noexcept { return ((uint32_t *)p)[0]; }
static inline void CHACHA20_U32TO8_LITTLE (void *p, const uint32_t v) noexcept { ((uint32_t *)p)[0] = v; }


#define CHACHA20_ROTL(a,b) (((a)<<(b))|((a)>>(32-(b))))
#define CHACHA20_QUARTERROUND(a, b, c, d) ( \
  a += b, d ^= a, d = CHACHA20_ROTL(d,16), \
  c += d, b ^= c, b = CHACHA20_ROTL(b,12), \
  a += b, d ^= a, d = CHACHA20_ROTL(d, 8), \
  c += d, b ^= c, b = CHACHA20_ROTL(b, 7))

#define CHACHA20_ROUNDS  20

static inline void salsa20_wordtobyte (uint8_t output[64], const uint32_t input[16]) noexcept {
  uint32_t x[16];
  for (unsigned i = 0; i < 16; ++i) x[i] = input[i];
  for (unsigned i = 0; i < CHACHA20_ROUNDS; i += 2) {
    // odd round
    CHACHA20_QUARTERROUND(x[0], x[4], x[ 8], x[12]); // column 0
    CHACHA20_QUARTERROUND(x[1], x[5], x[ 9], x[13]); // column 1
    CHACHA20_QUARTERROUND(x[2], x[6], x[10], x[14]); // column 2
    CHACHA20_QUARTERROUND(x[3], x[7], x[11], x[15]); // column 3
    // Even round
    CHACHA20_QUARTERROUND(x[0], x[5], x[10], x[15]); // diagonal 1 (main diagonal)
    CHACHA20_QUARTERROUND(x[1], x[6], x[11], x[12]); // diagonal 2
    CHACHA20_QUARTERROUND(x[2], x[7], x[ 8], x[13]); // diagonal 3
    CHACHA20_QUARTERROUND(x[3], x[4], x[ 9], x[14]); // diagonal 4
  }
  for (unsigned i = 0; i < 16; ++i) x[i] += input[i];
  for (unsigned i = 0; i < 16; ++i) CHACHA20_U32TO8_LITTLE(output+4*i, x[i]);
}

#undef CHACHA20_ROUNDS
#undef CHACHA20_QUARTERROUND
#undef CHACHA20_ROTL


/* Key size in bits: either 256 (32 bytes), or 128 (16 bytes) */
/* Nonce size in bits: 64 (8 bytes) */
/* returns 0 on success */
int VNetChanSocket::ChaCha20SetupEx (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata, uint32_t keybits) noexcept {
  const char *sigma = "expand 32-byte k";
  const char *tau = "expand 16-byte k";

  if (!keydata || !noncedata) return -1;

  const char *constants;
  const uint8_t *key = (const uint8_t *)keydata;
  if (keybits != 128 && keybits != 256) return -2;

  ctx->input[4] = CHACHA20_U8TO32_LITTLE(key+0);
  ctx->input[5] = CHACHA20_U8TO32_LITTLE(key+4);
  ctx->input[6] = CHACHA20_U8TO32_LITTLE(key+8);
  ctx->input[7] = CHACHA20_U8TO32_LITTLE(key+12);
  if (keybits == 256) {
    /* recommended */
    key += 16;
    constants = sigma;
  } else {
    /* keybits == 128 */
    constants = tau;
  }
  ctx->input[8] = CHACHA20_U8TO32_LITTLE(key+0);
  ctx->input[9] = CHACHA20_U8TO32_LITTLE(key+4);
  ctx->input[10] = CHACHA20_U8TO32_LITTLE(key+8);
  ctx->input[11] = CHACHA20_U8TO32_LITTLE(key+12);
  ctx->input[0] = CHACHA20_U8TO32_LITTLE(constants+0);
  ctx->input[1] = CHACHA20_U8TO32_LITTLE(constants+4);
  ctx->input[2] = CHACHA20_U8TO32_LITTLE(constants+8);
  ctx->input[3] = CHACHA20_U8TO32_LITTLE(constants+12);

  /* nonce setup */
  const uint8_t *iv = (const uint8_t *)noncedata;
  ctx->input[12] = 0;
  ctx->input[13] = 0;
  ctx->input[14] = CHACHA20_U8TO32_LITTLE(iv+0);
  ctx->input[15] = CHACHA20_U8TO32_LITTLE(iv+4);

  return 0;
}


/* encrypts or decrypts a full message */
/* cypher is symmetric, so `ciphertextdata` and `plaintextdata` can point to the same address */
void VNetChanSocket::ChaCha20XCrypt (ChaCha20Ctx *ctx, void *ciphertextdata, const void *plaintextdata, uint32_t msglen) noexcept {
  uint8_t output[64];
  if (!msglen) return;

  const uint8_t *plaintext = (const uint8_t *)plaintextdata;
  uint8_t *ciphertext = (uint8_t *)ciphertextdata;

  for (;;) {
    salsa20_wordtobyte(output, ctx->input);
    ++ctx->input[12];
    if (!ctx->input[12]) {
      ++ctx->input[13];
      /* stopping at 2^70 bytes per nonce is user's responsibility */
    }
    if (msglen <= 64) {
      for (unsigned i = 0; i < msglen; ++i) ciphertext[i] = plaintext[i]^output[i];
      return;
    }
    for (unsigned i = 0; i < 64; ++i) ciphertext[i] = plaintext[i]^output[i];
    msglen -= 64;
    ciphertext += 64;
    plaintext += 64;
  }
}


//==========================================================================
//
//  VNetChanSocket::SHA512Init
//
//==========================================================================
VNetChanSocket::SHA512Context VNetChanSocket::SHA512Init () noexcept {
  sha512_state_t *ctx = (sha512_state_t *)malloc(sizeof(sha512_state_t));
  if (!ctx) return nullptr;
  sha512_init(ctx);
  return (SHA512Context)ctx;
}


//==========================================================================
//
//  VNetChanSocket::SHA512Update
//
//==========================================================================
void VNetChanSocket::SHA512Update (VNetChanSocket::SHA512Context ctx, const void *in, size_t inlen) noexcept {
  if (!ctx) return;
  sha512_update((sha512_state_t *)ctx, in, inlen);
}


//==========================================================================
//
//  VNetChanSocket::SHA512Finish
//
//  this frees context
//
//==========================================================================
void VNetChanSocket::SHA512Finish (VNetChanSocket::SHA512Context ctx, VNetChanSocket::SHA512Digest hash) noexcept {
  if (!ctx) { if (hash) memset(hash, 0, SHA512DigestSize); return; }
  if (hash) sha512_final((sha512_state_t *)ctx, hash);
  free(ctx);
}


//==========================================================================
//
//  VNetChanSocket::SHA512Buffer
//
//==========================================================================
void VNetChanSocket::SHA512Buffer (VNetChanSocket::SHA512Digest hash, const void *in, size_t inlen) noexcept {
  sha512_hashBuffer(hash, in, inlen);
}


//==========================================================================
//
//  VNetChanSocket::GenerateKey
//
//==========================================================================
void VNetChanSocket::GenerateKey (uint8_t key[VNetChanSocket::ChaCha20KeySize]) noexcept {
  uint32_t *dest = (uint32_t *)key;
  for (int f = 0; f < VNetChanSocket::ChaCha20KeySize/4; ++f) *dest++ = GenRandomU32();
}


//==========================================================================
//
//  VNetChanSocket::EncryptInfoPacket
//
//  WARNING! cannot do it in-place
//  needs 24 extra bytes (key, nonce, crc)
//  returns new length or -1 on error
//
//==========================================================================
int VNetChanSocket::EncryptInfoPacket (void *destbuf, const void *srcbuf, int srclen, const uint8_t key[VNetChanSocket::ChaCha20KeySize]) noexcept {
  if (srclen < 0) return -1;
  if (!destbuf) return -1;
  if (srclen > 0 && !srcbuf) return -1;
  const uint32_t nonce = GenRandomU32();
  uint8_t *dest = (uint8_t *)destbuf;
  // copy key
  memcpy(dest, key, ChaCha20KeySize);
  // copy nonce
  dest[ChaCha20KeySize+0] = nonce&0xffU;
  dest[ChaCha20KeySize+1] = (nonce>>8)&0xffU;
  dest[ChaCha20KeySize+2] = (nonce>>16)&0xffU;
  dest[ChaCha20KeySize+3] = (nonce>>24)&0xffU;
  // copy crc32
  const uint32_t crc32 = CRC32C(0, srcbuf, (unsigned)srclen);
  dest[ChaCha20KeySize+4] = crc32&0xffU;
  dest[ChaCha20KeySize+5] = (crc32>>8)&0xffU;
  dest[ChaCha20KeySize+6] = (crc32>>16)&0xffU;
  dest[ChaCha20KeySize+7] = (crc32>>24)&0xffU;
  // copy data
  if (srclen) memcpy(dest+ChaCha20HeaderSize, srcbuf, (unsigned)srclen);
  // encrypt crc32 and data
  ChaCha20Ctx cctx;
  ChaCha20Setup(&cctx, key, nonce);
  ChaCha20XCrypt(&cctx, dest+ChaCha20KeySize+ChaCha20NonceSize, dest+ChaCha20KeySize+ChaCha20NonceSize, (unsigned)(srclen+ChaCha20CheckSumSize));
  return srclen+ChaCha20HeaderSize;
}


//==========================================================================
//
//  VNetChanSocket::DecryptInfoPacket
//
//  it can decrypt in-place
//  returns new length or -1 on error
//  also sets key
//
//==========================================================================
int VNetChanSocket::DecryptInfoPacket (uint8_t key[VNetChanSocket::ChaCha20KeySize], void *destbuf, const void *srcbuf, int srclen) noexcept {
  if (srclen < ChaCha20HeaderSize) return -1;
  if (!destbuf) return -1;
  if (!srcbuf) return -1;
  srclen -= ChaCha20KeySize+ChaCha20NonceSize; // key and nonce
  const uint8_t *src = (const uint8_t *)srcbuf;
  uint8_t *dest = (uint8_t *)destbuf;
  // get key
  memcpy(key, srcbuf, ChaCha20KeySize);
  // get nonce
  uint32_t nonce =
    ((uint32_t)src[ChaCha20KeySize+0])|
    (((uint32_t)src[ChaCha20KeySize+1])<<8)|
    (((uint32_t)src[ChaCha20KeySize+2])<<16)|
    (((uint32_t)src[ChaCha20KeySize+3])<<24);
  // decrypt packet
  ChaCha20Ctx cctx;
  ChaCha20Setup(&cctx, key, nonce);
  ChaCha20XCrypt(&cctx, dest, src+ChaCha20KeySize+ChaCha20NonceSize, (unsigned)srclen);
  // calculate and check crc32
  srclen -= ChaCha20CheckSumSize;
  //vassert(srclen >= 0);
  uint32_t crc32 = CRC32C(0, dest+ChaCha20CheckSumSize, (unsigned)srclen);
  if ((crc32&0xff) != dest[0] ||
      ((crc32>>8)&0xff) != dest[1] ||
      ((crc32>>16)&0xff) != dest[2] ||
      ((crc32>>24)&0xff) != dest[3])
  {
    // oops
    return -1;
  }
  // copy decrypted data
  if (srclen > 0) memcpy(dest, dest+ChaCha20CheckSumSize, (unsigned)srclen);
  return srclen;
}
