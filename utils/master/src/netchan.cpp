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

#ifndef _WIN32
# include <netdb.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/select.h>
#endif
//#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#if defined(__linux__)
# include <sys/random.h>
#endif


#include "netchan.h"

#ifdef WIN32
static bool vnetchanSockInited = false;

class TWinSockHelper {
public:
  ~TWinSockHelper () { if (vnetchanSockInited) WSACleanup(); vnetchanSockInited = false; }
};

static TWinSockHelper vnetchanHelper__;
#endif


static inline uint32_t isaac_rra (uint32_t value, unsigned int count) { return (value>>count)|(value<<(32-count)); }
static inline uint32_t isaac_rla (uint32_t value, unsigned int count) { return (value<<count)|(value>>(32-count)); }

static inline void isaac_getu32 (uint8_t *p, const uint32_t v) {
  p[0] = (uint8_t)(v&0xffu);
  p[1] = (uint8_t)((v>>8)&0xffu);
  p[2] = (uint8_t)((v>>16)&0xffu);
  p[3] = (uint8_t)((v>>24)&0xffu);
}

/*
 * ISAAC+ "variant", the paper is not clear on operator precedence and other
 * things. This is the "first in, first out" option!
 */
typedef struct isaacp_state_t {
  uint32_t state[256];
  uint8_t buffer[1024];
  union __attribute__((packed)) {
    uint32_t abc[3];
    struct __attribute__((packed)) {
      uint32_t a, b, c;
    };
  };
  size_t left;
} isaacp_state;

#define isaacp_step(offset,mix) \
  x = mm[i+offset]; \
  a = (a^(mix))+(mm[(i+offset+128u)&0xffu]); \
  y = (a^b)+mm[(x>>2)&0xffu]; \
  mm[i+offset] = y; \
  b = (x+a)^mm[(y>>10)&0xffu]; \
  isaac_getu32(out+(i+offset)*4u, b);

static inline void isaacp_mix (isaacp_state *st) {
  uint32_t x, y;
  uint32_t a = st->a, b = st->b, c = st->c;
  uint32_t *mm = st->state;
  uint8_t *out = st->buffer;
  c = c+1u;
  b = b+c;
  for (unsigned i = 0u; i < 256u; i += 4u) {
    isaacp_step(0u, isaac_rla(a,13u))
    isaacp_step(1u, isaac_rra(a, 6u))
    isaacp_step(2u, isaac_rla(a, 2u))
    isaacp_step(3u, isaac_rra(a,16u))
  }
  st->a = a;
  st->b = b;
  st->c = c;
  st->left = 1024u;
}

static void isaacp_random (isaacp_state *st, void *p, size_t len) {
  uint8_t *c = (uint8_t *)p;
  while (len) {
    const size_t use = (len > st->left ? st->left : len);
    memcpy(c, st->buffer+(sizeof(st->buffer)-st->left), use);
    st->left -= use;
    c += use;
    len -= use;
    if (!st->left) isaacp_mix(st);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// SplitMix; mostly used to generate 64-bit seeds
static inline uint64_t splitmix64_next (uint64_t *state) {
  uint64_t result = *state;
  *state = result+(uint64_t)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(uint64_t)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(uint64_t)0x94D049BB133111EBULL;
  return result^(result>>31);
}

static inline void splitmix64_seedU64 (uint64_t *state, uint32_t seed0, uint32_t seed1) {
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


#ifdef WIN32
#include <windows.h>
typedef BOOLEAN WINAPI (*RtlGenRandomFn) (PVOID RandomBuffer,ULONG RandomBufferLength);

static void RtlGenRandomX (PVOID RandomBuffer, ULONG RandomBufferLength) {
  if (RandomBufferLength <= 0) return;
  RtlGenRandomFn RtlGenRandomXX = NULL;
  HMODULE libh = LoadLibraryA("advapi32.dll");
  if (libh) {
    RtlGenRandomXX = (RtlGenRandomFn)(void *)GetProcAddress(libh, "SystemFunction036");
    //if (!RtlGenRandomXX) fprintf(stderr, "WARNING: `RtlGenRandom()` is not found!\n");
    //else fprintf(stderr, "MESSAGE: `RtlGenRandom()` found!\n");
    if (RtlGenRandomXX) {
      if (RtlGenRandomXX(RandomBuffer, RandomBufferLength)) return;
      //fprintf(stderr, "WARNING: `RtlGenRandom()` fallback for %u bytes!\n", (unsigned)RandomBufferLength);
    }
  }
  isaacp_state rng;
  // initialise isaacp with some shit
  uint32_t smxseed0 = 0;
  uint32_t smxseed1 = (uint32_t)GetCurrentProcessId();
  SYSTEMTIME st;
  FILETIME ft;
  GetLocalTime(&st);
  if (!SystemTimeToFileTime(&st, &ft)) {
    //fprintf(stderr, "SHIT: `SystemTimeToFileTime()` failed!\n");
    smxseed0 = (uint32_t)(GetTickCount());
  } else {
    smxseed0 = (uint32_t)(ft.dwLowDateTime);
  }
  uint64_t smx;
  splitmix64_seedU64(&smx, smxseed0, smxseed1);
  for (unsigned n = 0; n < 256; ++n) rng.state[n] = splitmix64_next(&smx);
  rng.a = splitmix64_next(&smx);
  rng.b = splitmix64_next(&smx);
  rng.c = splitmix64_next(&smx);
  isaacp_mix(&rng);
  isaacp_mix(&rng);
  // generate random bytes with ISAAC+
  isaacp_random(&rng, RandomBuffer, RandomBufferLength);
}
#endif


//==========================================================================
//
//  randombytes_init
//
//  initialize ISAAC+
//
//==========================================================================
static void randombytes_init (isaacp_state *rng) {
  static __thread uint32_t xstate[256+3]; /* and `abc` */
  for (unsigned f = 0u; f < 256u+3u; ++f) xstate[f] = f+666u;
  memset(rng, 0, sizeof(isaacp_state));
  #ifdef __SWITCH__
  randomGet(xstate, sizeof(xstate));
  #elif defined(WIN32)
  RtlGenRandomX(xstate, sizeof(xstate));
  #else
  size_t pos = 0;
  #if defined(__linux__)
  /* try to use kernel syscall first */
  while (pos < sizeof(xstate)) {
    /* reads up to 256 bytes should not be interrupted by signals */
    size_t len = sizeof(xstate)-pos;
    if (len > 256) len = 256;
    ssize_t rd = getrandom(((uint8_t *)xstate)+pos, len, 0);
    if (rd < 0) {
      if (errno != EINTR) break;
    } else {
      pos += (size_t)rd;
    }
  }
  /* do not mix additional sources if we got all random bytes from kernel */
  const unsigned mixother = (pos != sizeof(xstate));
  #else
  const unsigned mixother = 1u;
  #endif
  /* fill up what is left with "/dev/urandom" */
  if (pos < sizeof(xstate)) {
    int fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC);
    if (fd >= 0) {
      while (pos < sizeof(xstate)) {
        size_t len = sizeof(xstate)-pos;
        ssize_t rd = read(fd, ((uint8_t *)xstate)+pos, len);
        if (rd < 0) {
          if (errno != EINTR) break;
        } else {
          pos += (size_t)rd;
        }
      }
      close(fd);
    }
  }
  /* mix some other random sources, just in case */
  if (mixother) {
    uint32_t smxseed0 = 0;
    uint32_t smxseed1 = (uint32_t)getpid();
    #if defined(__linux__)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
      smxseed0 = ts.tv_sec^ts.tv_nsec;
    } else {
      struct timeval tp;
      struct timezone tzp;
      gettimeofday(&tp, &tzp);
      smxseed0 = tp.tv_sec^tp.tv_usec;
    }
    #else
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
    smxseed0 = tp.tv_sec^tp.tv_usec;
    #endif
    static __thread isaacp_state rngtmp;
    uint64_t smx;
    splitmix64_seedU64(&smx, smxseed0, smxseed1);
    for (unsigned n = 0; n < 256u; ++n) rngtmp.state[n] = splitmix64_next(&smx);
    rngtmp.a = splitmix64_next(&smx);
    rngtmp.b = splitmix64_next(&smx);
    rngtmp.c = splitmix64_next(&smx);
    isaacp_mix(&rngtmp);
    isaacp_mix(&rngtmp);
    isaacp_random(&rngtmp, rng->state, sizeof(rng->state));
    isaacp_random(&rngtmp, rng->abc, sizeof(rng->abc));
  }
  #endif
  /* xor ISAAC+ state with random bytes for from various sources */
  for (unsigned f = 0u; f < 256u; ++f) rng->state[f] ^= xstate[f];
  for (unsigned f = 0u; f < 3u; ++f) rng->abc[f] ^= xstate[256u+f];
  isaacp_mix(rng);
  isaacp_mix(rng);
}


//==========================================================================
//
//  prng_randombytes
//
//==========================================================================
static void prng_randombytes (void *p, size_t len) {
  static __thread unsigned initialized = 0u;
  static __thread isaacp_state rng;

  if (!len || !p) return;

  if (!initialized) {
    randombytes_init(&rng);
    initialized = 1u;
  }

  isaacp_random(&rng, p, len);
}


static inline __attribute__((unused)) uint32_t GenRandomU32 () noexcept { uint32_t res; prng_randombytes(&res, sizeof(res)); return res; }


//**************************************************************************
//
// SHA256
//
//**************************************************************************
#define SHA256PD_HASH_SIZE  (32u)

#define SHA256PD_CHUNK_SIZE     (64u)
#define SHA256PD_TOTALLEN_SIZE  (8u)
#define SHA256PD_HACCUM_SIZE    (8u)

/* sha256 context */
typedef struct sha256pd_ctx_t {
  uint8_t chunk[SHA256PD_CHUNK_SIZE]; /* 512-bit chunks is what we will operate on */
  size_t chunk_used; /* numbed of bytes used in the current chunk */
  size_t total_len; /* accumulator */
  uint32_t h[SHA256PD_HACCUM_SIZE]; /* current hash value */
} sha256pd_ctx;

static inline uint32_t sha26pd_rra (uint32_t value, unsigned int count) { return value>>count|value<<(32-count); }

static void sha256pd_round (uint32_t h[SHA256PD_HACCUM_SIZE], const uint8_t *chunk) {
  const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
  };

  uint32_t ah[SHA256PD_HACCUM_SIZE];
  uint32_t w[16];
  memcpy(ah, h, sizeof(ah));
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 16; j++) {
      if (i == 0) {
        w[j] = (uint32_t)(chunk[0]<<24)|((uint32_t)chunk[1]<<16)|((uint32_t)chunk[2]<<8)|(uint32_t)chunk[3];
        chunk += 4;
      } else {
        const uint32_t s0 = sha26pd_rra(w[(j+1)&0xf], 7)^sha26pd_rra(w[(j+1)&0xf], 18)^(w[(j+1)&0xf]>>3);
        const uint32_t s1 = sha26pd_rra(w[(j+14)&0xf], 17)^sha26pd_rra(w[(j+14)&0xf], 19)^(w[(j+14)&0xf]>>10);
        w[j] = w[j]+s0+w[(j+9)&0xf]+s1;
      }
      const uint32_t s1 = sha26pd_rra(ah[4], 6)^sha26pd_rra(ah[4], 11)^sha26pd_rra(ah[4], 25);
      const uint32_t ch = (ah[4]&ah[5])^(~ah[4]&ah[6]);
      const uint32_t temp1 = ah[7]+s1+ch+k[i<<4|j]+w[j];
      const uint32_t s0 = sha26pd_rra(ah[0], 2)^sha26pd_rra(ah[0], 13)^sha26pd_rra(ah[0], 22);
      const uint32_t maj = (ah[0]&ah[1])^(ah[0]&ah[2])^(ah[1]&ah[2]);
      const uint32_t temp2 = s0+maj;
      ah[7] = ah[6];
      ah[6] = ah[5];
      ah[5] = ah[4];
      ah[4] = ah[3]+temp1;
      ah[3] = ah[2];
      ah[2] = ah[1];
      ah[1] = ah[0];
      ah[0] = temp1+temp2;
    }
  }
  for (unsigned i = 0; i < SHA256PD_HACCUM_SIZE; ++i) h[i] += ah[i];
}

static void sha256pd_init (sha256pd_ctx *state) {
  /* initial hash values (first 32 bits of the fractional parts of the square roots of the first 8 primes 2..19) */
  const uint32_t hinit[SHA256PD_HACCUM_SIZE] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  state->chunk_used = 0;
  state->total_len = 0;
  memcpy(state->h, hinit, sizeof(hinit));
}


static void sha256pd_update (sha256pd_ctx *state, const void *input, size_t len) {
  if (!len) return;
  const uint8_t *src = (const uint8_t *)input;
  state->total_len += len;
  /* complete current chunk (if it is not empty) */
  if (state->chunk_used) {
    const size_t cleft = SHA256PD_CHUNK_SIZE-state->chunk_used;
    const size_t ccpy = (len <= cleft ? len : cleft);
    memcpy(state->chunk+state->chunk_used, src, ccpy);
    state->chunk_used += ccpy;
    src += ccpy;
    len -= ccpy;
    /* process chunk if it is full */
    if (state->chunk_used == SHA256PD_CHUNK_SIZE) {
      sha256pd_round(state->h, state->chunk);
      state->chunk_used = 0;
    }
  }
  /* process full chunks, if there are any */
  while (len >= SHA256PD_CHUNK_SIZE) {
    sha256pd_round(state->h, src);
    src += SHA256PD_CHUNK_SIZE;
    len -= SHA256PD_CHUNK_SIZE;
  }
  /* save data for next update */
  if (len) {
    /* if we came here, we have no accumulated chunk data */
    memcpy(state->chunk, src, len);
    state->chunk_used = len;
  }
}

static void sha256pd_finish (const sha256pd_ctx *state, uint8_t hash[SHA256PD_HASH_SIZE]) {
  uint8_t tmpchunk[SHA256PD_CHUNK_SIZE]; /* we need temporary workspace */
  uint32_t hh[SHA256PD_HACCUM_SIZE]; /* we don't want to destroy our current hash accumulator */
  memcpy(hh, state->h, sizeof(hh));
  size_t pos = state->chunk_used;
  /* we need to put 0x80 and 8-byte length */
  if (pos) memcpy(tmpchunk, state->chunk, pos);
  /* put trailing bit (there is always room for at least one byte in the current chunk) */
  tmpchunk[pos++] = 0x80;
  /* clear chunk padding */
  if (pos < SHA256PD_CHUNK_SIZE) memset(tmpchunk+pos, 0, SHA256PD_CHUNK_SIZE-pos);
  /* if we don't have enough room for size, flush current chunk */
  if (SHA256PD_CHUNK_SIZE-pos < SHA256PD_TOTALLEN_SIZE) {
    /* flush it */
    sha256pd_round(hh, tmpchunk);
    /* clear chunk, so we may put the length there */
    memset(tmpchunk, 0, SHA256PD_CHUNK_SIZE);
  }
  /* put length (in bits) */
  /* don't multiply it, so it won't overflow on 32-bit systems */
  size_t len = state->total_len;
  tmpchunk[SHA256PD_CHUNK_SIZE-1u] = (uint8_t)((len<<3)&0xffu);
  len >>= 5;
  for (unsigned i = 0; i < SHA256PD_TOTALLEN_SIZE-1u; ++i) {
    tmpchunk[SHA256PD_CHUNK_SIZE-2u-i] = (uint8_t)(len&0xffu);
    len >>= 8;
  }
  /* final round */
  sha256pd_round(hh, tmpchunk);
  /* produce the final big-endian hash value */
  for (unsigned i = 0; i < 8; ++i) {
    hash[(i<<2)+0] = (uint8_t)((hh[i]>>24)&0xffu);
    hash[(i<<2)+1] = (uint8_t)((hh[i]>>16)&0xffu);
    hash[(i<<2)+2] = (uint8_t)((hh[i]>>8)&0xffu);
    hash[(i<<2)+3] = (uint8_t)(hh[i]&0xffu);
  }
}


static void sha256pd_buf (VNetChanSocket::SHA256Digest hash, const void *in, size_t inlen) {
  sha256pd_ctx state;
  sha256pd_init(&state);
  sha256pd_update(&state, in, inlen);
  sha256pd_finish(&state, hash);
}


//**************************************************************************
//
// CP25519 math
//
//**************************************************************************
#define C25519_KEY_SIZE  (32)

static inline void unpack25519 (int64_t o[16], const uint8_t *n) {
  for (unsigned i = 0; i < 16; ++i) o[i]=n[2*i]+((int64_t)n[2*i+1]<<8);
  o[15] &= 0x7fff;
}

static inline void sel25519 (int64_t p[16], int64_t q[16], int b) {
  const int64_t c = ~(b-1);
  for (unsigned i = 0; i < 16; ++i) {
    const int64_t t = c&(p[i]^q[i]);
    p[i] ^= t;
    q[i] ^= t;
  }
}

static inline void car25519 (int64_t o[16]) {
  for (unsigned i = 0; i < 16; ++i) {
    o[i] += (1LL<<16);
    const int64_t c = o[i]>>16;
    o[(i+1)*(i<15)] += c-1+37*(c-1)*(i==15);
    o[i] -= c<<16;
  }
}

static void pack25519 (uint8_t *o, const int64_t n[16]) {
  int b;
  int64_t m[16];
  int64_t t[16];
  for (unsigned i = 0; i < 16; ++i) t[i] = n[i];
  car25519(t);
  car25519(t);
  car25519(t);
  for (unsigned j = 0; j < 2; ++j) {
    m[0] = t[0]-0xffed;
    for (unsigned i = 1; i < 15; ++i) {
      m[i] = t[i]-0xffff-((m[i-1]>>16)&1);
      m[i-1] &= 0xffff;
    }
    m[15] = t[15]-0x7fff-((m[14]>>16)&1);
    b = (m[15]>>16)&1;
    m[14] &= 0xffff;
    sel25519(t, m, 1-b);
  }
  for (unsigned i = 0; i < 16; ++i) {
    o[2*i] = t[i]&0xff;
    o[2*i+1] = t[i]>>8;
  }
}

static void M (int64_t o[16], const int64_t a[16], const int64_t b[16]) {
  int64_t t[31];
  for (unsigned i = 0; i < 31; ++i) t[i] = 0;
  for (unsigned i = 0; i < 16; ++i) for (unsigned j = 0; j < 16; ++j) t[i+j] += a[i]*b[j];
  for (unsigned i = 0; i < 15; ++i) t[i] += 38*t[i+16];
  for (unsigned i = 0; i < 16; ++i) o[i] = t[i];
  car25519(o);
  car25519(o);
}

static inline void S (int64_t o[16], const int64_t a[16]) {
  M(o, a, a);
}

static void inv25519 (int64_t o[16], const int64_t i[16]) {
  int64_t c[16];
  for (unsigned a = 0; a < 16; ++a) c[a] = i[a];
  for (int a = 253; a >= 0; --a) {
    S(c, c);
    if (a != 2 && a != 4) M(c, c, i);
  }
  for (unsigned a = 0; a < 16; ++a) o[a] = c[a];
}

static inline void A (int64_t o[16], const int64_t a[16], const int64_t b[16]) {
  for (unsigned i = 0; i < 16; ++i) o[i] = a[i]+b[i];
}

static inline void Z (int64_t o[16], const int64_t a[16], const int64_t b[16]) {
  for (unsigned i = 0; i < 16; ++i) o[i] = a[i]-b[i];
}

static void crypto_scalarmult (uint8_t q[C25519_KEY_SIZE], const uint8_t n[C25519_KEY_SIZE], const uint8_t p[C25519_KEY_SIZE]) {
  const int64_t k121665[16] = {0xDB41, 1};

  uint8_t z[32];
  int64_t x[80];
  //int64_t r, i;
  int64_t a[16];
  int64_t b[16];
  int64_t c[16];
  int64_t d[16];
  int64_t e[16];
  int64_t f[16];
  for (unsigned i = 0; i < 31; ++i) z[i] = n[i];
  z[31] = (n[31]&127)|64;
  z[0] &= 248;
  unpack25519(x, p);
  for (unsigned i = 0; i < 16; ++i) {
    b[i] = x[i];
    d[i] = a[i] = c[i] = 0;
  }
  a[0] = d[0] = 1;
  for (int i = 254; i >= 0; --i) {
    const int r = (z[i>>3]>>(i&7))&1;
    sel25519(a, b, r);
    sel25519(c, d, r);
    A(e, a, c);
    Z(a, a, c);
    A(c, b, d);
    Z(b, b, d);
    S(d, e);
    S(f, a);
    M(a, c, a);
    M(c, b, e);
    A(e, a, c);
    Z(a, a, c);
    S(b, a);
    Z(c, d, f);
    M(a, c, k121665);
    A(a, a, d);
    M(c, c, a);
    M(a, d, f);
    M(d, b, x);
    S(b, e);
    sel25519(a, b, r);
    sel25519(c, d, r);
  }
  for (unsigned i = 0; i < 16; ++i) {
    x[i+16] = a[i];
    x[i+32] = c[i];
    x[i+48] = b[i];
    x[i+64] = d[i];
  }
  inv25519(x+32, x+32);
  M(x+16, x+16, x+32);
  pack25519(q, x+16);
}

static void c25519_derive_key (uint8_t dkey[C25519_KEY_SIZE], const uint8_t sk[C25519_KEY_SIZE], const uint8_t pk[C25519_KEY_SIZE]) {
  const uint8_t basepoint[C25519_KEY_SIZE] = {9};
  if (!pk) pk = basepoint;
  uint8_t mysecret[C25519_KEY_SIZE];
  for (unsigned f = 0; f < C25519_KEY_SIZE; ++f) mysecret[f] = sk[f];
  mysecret[0] &= 248;
  mysecret[31] &= 127;
  mysecret[31] |= 64;
  crypto_scalarmult(dkey, mysecret, pk);
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
  return (::sendto(sockfd, (const char *)buf, len, 0, addr, sizeof(*addr)) == len);
}


//==========================================================================
//
//  VNetChanSocket::hasData
//
//==========================================================================
bool VNetChanSocket::hasData () noexcept {
  if (sockfd < 0) return false;
  uint8_t buf[MAX_DGRAM_SIZE];
  return (recvfrom(sockfd, (char *)buf, MAX_DGRAM_SIZE, MSG_PEEK, nullptr, nullptr) > 0);
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
    len = (int)recvfrom(sockfd, (char *)buf, maxlen, 0, addr, &addrlen);
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
  return ::GetSysTime();
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
  dest->tv_usec *= 1000;
}


uint32_t VNetChanSocket::GenRandomU32 () noexcept {
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
//  VNetChanSocket::SHA256Init
//
//==========================================================================
VNetChanSocket::SHA256Context VNetChanSocket::SHA256Init () noexcept {
  sha256pd_ctx *ctx = (sha256pd_ctx *)malloc(sizeof(sha256pd_ctx));
  if (!ctx) return nullptr;
  sha256pd_init(ctx);
  return (SHA256Context)ctx;
}


//==========================================================================
//
//  VNetChanSocket::SHA256Update
//
//==========================================================================
void VNetChanSocket::SHA256Update (SHA256Context ctx, const void *in, size_t inlen) noexcept {
  if (!ctx) return;
  sha256pd_update((sha256pd_ctx *)ctx, in, inlen);
}


//==========================================================================
//
//  VNetChanSocket::SHA256Finish
//
//  this frees context
//
//==========================================================================
void VNetChanSocket::SHA256Finish (SHA256Context ctx, SHA256Digest hash) noexcept {
  if (!ctx) { if (hash) memset(hash, 0, SHA256DigestSize); return; }
  if (hash) sha256pd_finish((sha256pd_ctx *)ctx, hash);
  free(ctx);
}


//==========================================================================
//
//  VNetChanSocket::SHA256Buffer
//
//==========================================================================
void VNetChanSocket::SHA256Buffer (SHA256Digest hash, const void *in, size_t inlen) noexcept {
  sha256pd_buf(hash, in, inlen);
}


//==========================================================================
//
//  VNetChanSocket::GenerateKey
//
//==========================================================================
void VNetChanSocket::GenerateKey (uint8_t key[ChaCha20KeySize]) noexcept {
  prng_randombytes(key, ChaCha20KeySize);
}


//==========================================================================
//
//  VNetChanSocket::DerivePublicKey
//
//  derive public key from secret one
//
//==========================================================================
void VNetChanSocket::DerivePublicKey (uint8_t mypk[ChaCha20KeySize], const uint8_t mysk[ChaCha20KeySize]) {
  ::c25519_derive_key(mypk, mysk, nullptr);
}


//==========================================================================
//
//  VNetChanSocket::DeriveSharedKey
//
//  derive shared secret from our secret and their public
//
//==========================================================================
void VNetChanSocket::DeriveSharedKey (uint8_t sharedk[ChaCha20KeySize], const uint8_t mysk[ChaCha20KeySize], const uint8_t theirpk[ChaCha20KeySize]) {
  ::c25519_derive_key(sharedk, mysk, theirpk);
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
int VNetChanSocket::EncryptInfoPacket (void *destbuf, const void *srcbuf, int srclen, const uint8_t key[ChaCha20KeySize]) noexcept {
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
int VNetChanSocket::DecryptInfoPacket (uint8_t key[ChaCha20KeySize], void *destbuf, const void *srcbuf, int srclen) noexcept {
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
