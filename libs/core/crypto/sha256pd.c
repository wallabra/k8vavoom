/* based on the code from https://github.com/amosnier/sha-2 */
/* modified (streamified) by Ketmar Dark */
/* added HMAC-SHA256 and HKDF-SHA256 implementations */
/* public domain */
#include "sha256pd.h"
#include <stdlib.h>
#ifdef SHA256PD_ENABLE_RANDOMBYTES
# ifndef WIN32
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <time.h>
#  include <sys/time.h>
#  if defined(__SWITCH__)
#   include <switch/kernel/random.h> // for randomGet()
/*
# elif defined(__linux__) && !defined(ANDROID)
#   include <sys/random.h>
*/
#  endif
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define SHA256PD_TOTALLEN_SIZE  (8u)
#define SHA256PD_HACCUM_SIZE    (8u)


// ////////////////////////////////////////////////////////////////////////// //
void *(*sha256pd_malloc_fn) (size_t size) = NULL;
void (*sha256pd_free_fn) (void *p) = NULL;


// ////////////////////////////////////////////////////////////////////////// //
static inline uint32_t sha26pd_rra (uint32_t value, unsigned int count) { return (value>>count)|(value<<(32-count)); }

static inline void U32TO8_LE (uint8_t *p, const uint32_t v) {
  p[0] = (uint8_t)(v&0xffu);
  p[1] = (uint8_t)((v>>8)&0xffu);
  p[2] = (uint8_t)((v>>16)&0xffu);
  p[3] = (uint8_t)((v>>24)&0xffu);
}

static inline uint32_t GET_U32 (const void *buf) {
  const uint8_t *p = (const uint8_t *)buf;
  return ((uint32_t)p[0])|((uint32_t)p[0]<<8)|((uint32_t)p[0]<<16)|((uint32_t)p[0]<<24);
}


#if defined(SHA256PD_ENABLE_RANDOMBYTES) || defined(SHA256PD_BALLOON_USE_ISAAC)
/*
 * ISAAC+ "variant", the paper is not clear on operator precedence and other
 * things. This is the "first in, first out" option!
 */
static inline uint32_t sha26pd_rla (uint32_t value, unsigned int count) { return (value<<count)|(value>>(32-count)); }

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
  U32TO8_LE(out+(i+offset)*4u, b);

static inline void isaacp_mix (isaacp_state *st) {
  uint32_t x, y;
  uint32_t a = st->a, b = st->b, c = st->c;
  uint32_t *mm = st->state;
  uint8_t *out = st->buffer;
  c = c+1;
  b = b+c;
  for (unsigned i = 0; i < 256; i += 4) {
    isaacp_step(0, sha26pd_rla(a,13))
    isaacp_step(1, sha26pd_rra(a, 6))
    isaacp_step(2, sha26pd_rla(a, 2))
    isaacp_step(3, sha26pd_rra(a,16))
  }
  st->a = a;
  st->b = b;
  st->c = c;
  st->left = 1024;
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
#endif


#ifdef SHA256PD_ENABLE_RANDOMBYTES

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
      BOOLEAN res = RtlGenRandomXX(RandomBuffer, RandomBufferLength);
      FreeLibrary(libh);
      if (res) return;
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

#if defined(__linux__) && !defined(ANDROID)
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
/*#include <stdio.h>*/

#ifdef SYS_getrandom
static unsigned int skip_getrandom = 0; // 0:nope
#endif

static ssize_t why_glibc_is_so_fucked_get_random (void *buf, size_t bufsize) {
  #ifdef SYS_getrandom
  # ifndef GRND_NONBLOCK
  #  define GRND_NONBLOCK (1)
  # endif
  if (!skip_getrandom) {
    for (;;) {
      ssize_t ret = syscall(SYS_getrandom, buf, bufsize, GRND_NONBLOCK);
      if (ret >= 0) {
        /*fprintf(stderr, "*** SYS_getrandom is here! read %u bytes out of %u\n", (unsigned)ret, (unsigned)bufsize);*/
        return ret;
      }
      if (ret != -EINTR) break;
    }
    skip_getrandom = 1; // fall back to /dev/urandom
  }
  #endif
  return -1;
}
#endif


//==========================================================================
//
//  sha256pd_randombytes_init
//
//  initialize ISAAC+
//
//==========================================================================
static void sha256pd_randombytes_init (isaacp_state *rng) {
  static __thread uint32_t xstate[256+3]; /* and `abc` */
  for (unsigned f = 0u; f < 256u+3u; ++f) xstate[f] = f+666u;
  memset(rng, 0, sizeof(isaacp_state));
  #ifdef __SWITCH__
  randomGet(xstate, sizeof(xstate));
  #elif defined(WIN32)
  RtlGenRandomX(xstate, sizeof(xstate));
  #else
  size_t pos = 0;
  #if defined(__linux__) && !defined(ANDROID)
  /* try to use kernel syscall first */
  while (pos < sizeof(xstate)) {
    /* reads up to 256 bytes should not be interrupted by signals */
    size_t len = sizeof(xstate)-pos;
    if (len > 256) len = 256;
    /*
    ssize_t rd = getrandom(((uint8_t *)xstate)+pos, len, 0);
    if (rd < 0) {
      if (errno != EINTR) break;
    } else {
      pos += (size_t)rd;
    }
    */
    ssize_t rd = why_glibc_is_so_fucked_get_random(((uint8_t *)xstate)+pos, len);
    if (rd < 0) break;
    pos += (size_t)rd;
  }
  /* do not mix additional sources if we got all random bytes from kernel */
  const unsigned mixother = (pos != sizeof(xstate));
  #else
  const unsigned mixother = 1u;
  #endif
  /* fill up what is left with "/dev/urandom" */
  if (pos < sizeof(xstate)) {
    int fd = open("/dev/urandom", O_RDONLY/*|O_CLOEXEC*/);
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
//  sha256pd_randombytes
//
//==========================================================================
void sha256pd_randombytes (void *p, size_t len) {
  static __thread unsigned initialized = 0u;
  static __thread isaacp_state rng;

  if (!len || !p) return;

  if (!initialized) {
    sha256pd_randombytes_init(&rng);
    initialized = 1u;
  }

  isaacp_random(&rng, p, len);
}
#endif


//==========================================================================
//
//  sha256pd_memerase
//
//  use `volatile`, so GCC won't optimise it out
//
//==========================================================================
void *sha256pd_memerase (void *p, size_t size) {
  volatile unsigned char *dest = p;
  while (size--) *dest++ = 0;
  return p;
}


//==========================================================================
//
//  sha256pd_round
//
//  chunk must be fully constructed
//
//==========================================================================
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


//==========================================================================
//
//  sha256pd_init
//
//==========================================================================
void sha256pd_init (sha256pd_ctx *state) {
  /* initial hash values (first 32 bits of the fractional parts of the square roots of the first 8 primes 2..19) */
  const uint32_t hinit[SHA256PD_HACCUM_SIZE] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  state->chunk_used = 0;
  state->total_len = 0;
  memcpy(state->h, hinit, sizeof(hinit));
}


//==========================================================================
//
//  sha256pd_update
//
//==========================================================================
void sha256pd_update (sha256pd_ctx *state, const void *input, size_t len) {
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


//==========================================================================
//
//  sha256pd_finish
//
//==========================================================================
void sha256pd_finish (const sha256pd_ctx *state, uint8_t hash[SHA256PD_HASH_SIZE]) {
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


//==========================================================================
//
//  sha256pd_buf
//
//==========================================================================
void sha256pd_buf (uint8_t hash[SHA256PD_HASH_SIZE], const void *input, size_t len) {
  sha256pd_ctx state;
  sha256pd_init(&state);
  sha256pd_update(&state, input, len);
  sha256pd_finish(&state, hash);
  /* clear state, why not */
  sha256pd_memerase(&state, sizeof(state));
}



//==========================================================================
//
//  sha256pd_hmac_init
//
//==========================================================================
void sha256pd_hmac_init (sha256pd_hmac_ctx *ctx, const void *key, size_t keysize) {
  uint8_t block_ipad[SHA256PD_CHUNK_SIZE];
  uint8_t block_opad[SHA256PD_CHUNK_SIZE];
  /* compress/extend key */
  if (keysize <= SHA256PD_CHUNK_SIZE) {
    /* if key size is less than sha256 chunk, it is used as is (possibly padded with zeroes) */
    if (keysize) memcpy(block_ipad, key, keysize);
    if (keysize < SHA256PD_CHUNK_SIZE) memset(block_ipad+keysize, 0, SHA256PD_CHUNK_SIZE-keysize);
  } else {
    /* key size is greater than sha256 chunk, compress the key */
    sha256pd_buf(block_ipad, key, keysize);
    /* digest size is smaller than block size, so pad with zeroes */
    memset(block_ipad+SHA256PD_HASH_SIZE, 0, SHA256PD_CHUNK_SIZE-SHA256PD_HASH_SIZE);
  }
  /* copy to opad */
  memcpy(block_opad, block_ipad, SHA256PD_CHUNK_SIZE);
  /* xor pads; use two loops for better locality */
  for (size_t i = 0; i < SHA256PD_CHUNK_SIZE; ++i) block_ipad[i] ^= 0x36;
  for (size_t i = 0; i < SHA256PD_CHUNK_SIZE; ++i) block_opad[i] ^= 0x5c;
  /* init "inside" sha */
  sha256pd_init(&ctx->inner_hash);
  sha256pd_update(&ctx->inner_hash, block_ipad, SHA256PD_CHUNK_SIZE);
  /* init "outside" sha */
  sha256pd_init(&ctx->outer_hash);
  sha256pd_update(&ctx->outer_hash, block_opad, SHA256PD_CHUNK_SIZE);
  /* clear pads */
  sha256pd_memerase(block_ipad, sizeof(block_ipad));
  sha256pd_memerase(block_opad, sizeof(block_opad));
}


//==========================================================================
//
//  sha256pd_hmac_update
//
//==========================================================================
void sha256pd_hmac_update (sha256pd_hmac_ctx *ctx, const void *msg, size_t msgsize) {
  sha256pd_update(&ctx->inner_hash, msg, msgsize);
}


//==========================================================================
//
//  sha256pd_hmac_finish
//
//==========================================================================
void sha256pd_hmac_finish (const sha256pd_hmac_ctx *ctx, void *mac, size_t macsize) {
  if (!macsize) return; // just in case
  if (macsize > SHA256PD_HASH_SIZE) macsize = SHA256PD_HASH_SIZE;
  uint8_t inner_digest[SHA256PD_HASH_SIZE];
  uint8_t outer_digest[SHA256PD_HASH_SIZE];
  /* complete inner hash */
  sha256pd_finish(&ctx->inner_hash, inner_digest);
  /* complete outer hash using copied context */
  sha256pd_ctx tmp;
  memcpy(&tmp, &ctx->outer_hash, sizeof(tmp));
  sha256pd_update(&tmp, inner_digest, SHA256PD_HASH_SIZE);
  sha256pd_finish(&tmp, outer_digest);
  /* copy final mac */
  memcpy(mac, outer_digest, macsize);
  /* clear innter digest, why not */
  sha256pd_memerase(&inner_digest, sizeof(inner_digest));
}


//==========================================================================
//
//  sha256pd_hmac_buf
//
//==========================================================================
void sha256pd_hmac_buf (void *mac, size_t macsize, const void *key, size_t keysize, const void *msg, size_t msgsize) {
  if (!macsize) return;
  sha256pd_hmac_ctx ctx;
  sha256pd_hmac_init(&ctx, key, keysize);
  sha256pd_hmac_update(&ctx, msg, msgsize);
  sha256pd_hmac_finish(&ctx, mac, macsize);
}



//==========================================================================
//
//  sha256pd_hkdf_buf
//
//==========================================================================
int sha256pd_hkdf_buf (void *reskey, size_t reskeylen, const void *inkey, size_t inkeysize,
                       const void *salt, size_t saltsize,
                       const void *info, size_t infosize)
{
  if (reskeylen == 0 || reskeylen > 255*SHA256PD_HASH_SIZE) return -1;
  if (!reskey) return -1;
  if ((infosize && !info) || (saltsize && !salt)) return -1;

  /* prepare salt */
  uint8_t tmpsalt[SHA256PD_HASH_SIZE];
  if (saltsize == 0) {
    memset(tmpsalt, 0, SHA256PD_HASH_SIZE);
    salt = tmpsalt;
    saltsize = SHA256PD_HASH_SIZE;
  }

  /* calculate prk */
  uint8_t prk[SHA256PD_HASH_SIZE];
  sha256pd_hmac_buf(prk, SHA256PD_HASH_SIZE, salt, saltsize, inkey, inkeysize);

  /* no need to clear it, it is unsed in the first round */
  uint8_t tbuf[SHA256PD_HASH_SIZE];

  /* it is guaranteed to be in range, but let's use `size_t` for safety */
  size_t steps = (reskeylen+SHA256PD_HASH_SIZE-1u)/SHA256PD_HASH_SIZE;

  uint8_t *dest = (uint8_t *)reskey;
  sha256pd_hmac_ctx hctx;
  uint8_t currstepnum = 0; /* step counter, we need to mix it */
  while (steps--) {
    sha256pd_hmac_init(&hctx, prk, SHA256PD_HASH_SIZE);
    /* previous T */
    if (currstepnum) sha256pd_hmac_update(&hctx, tbuf, SHA256PD_HASH_SIZE);
    /* app-specific info */
    if (infosize) sha256pd_hmac_update(&hctx, (const uint8_t *)info, infosize);
    /* step number */
    ++currstepnum;
    sha256pd_hmac_update(&hctx, &currstepnum, 1);
    /* get new T */
    sha256pd_hmac_finish(&hctx, tbuf, SHA256PD_HASH_SIZE);
    /* copy T to output key */
    const size_t cplen = (reskeylen >= SHA256PD_HASH_SIZE ? SHA256PD_HASH_SIZE : reskeylen);
    memcpy(dest, tbuf, cplen);
    dest += cplen;
    reskeylen -= cplen;
  }

  /* remove prk, why not */
  sha256pd_memerase(prk, sizeof(prk));
  /* and clear hmac state */
  sha256pd_memerase(&hctx, sizeof(hctx));

  return 0;
}



//==========================================================================
//
//  sha256pd_balloon_hash_u32
//
//==========================================================================
static inline void sha256pd_balloon_hash_u32 (sha256pd_ctx *state, unsigned v) {
  uint8_t cbytes[4];
  U32TO8_LE(cbytes, v);
  sha256pd_update(state, cbytes, 4);
}


//==========================================================================
//
//  sha256pd_balloon
//
//==========================================================================
int sha256pd_balloon (uint8_t reskey[SHA256PD_HASH_SIZE],
                      unsigned scost, /* space cost, main buffer size, in blocks */
                      unsigned tcost, /* time cost, number of rounds */
                      const void *inkey, size_t inkeysize,
                      const void *salt, size_t saltsize)
{
  if (!reskey) return -1;
  if (!inkey || inkeysize == 0u) return -1;
  if (!salt || saltsize == 0u) return -1;
  if (scost > 0x40000000u) return -1;

  if (!scost) scost = SHA256PD_BALLOON_DEFAULT_SCOST;
  if (!tcost) tcost = SHA256PD_BALLOON_DEFAULT_TCOST;

  // convert to number of blocks
  scost = (scost+SHA256PD_HASH_SIZE-1u)/SHA256PD_HASH_SIZE;

  const unsigned delta = 3u; /* number of dependencies per block */
  unsigned cnt = 0u; /* counter, used in security proof */

  /* one more for temporary block */
  uint8_t *buf = (uint8_t *)(sha256pd_malloc_fn ? sha256pd_malloc_fn : malloc)((scost+1u)*SHA256PD_HASH_SIZE);
  if (!buf) return -1;
  uint8_t *tempbuf = buf+SHA256PD_HASH_SIZE*scost;

  sha256pd_ctx state;

  #ifdef SHA256PD_BALLOON_USE_HKDF
  /* create initial key with HKDF */
  U32TO8_LE(tempbuf, cnt++);
  if (sha256pd_hkdf_buf(buf, SHA256PD_HASH_SIZE, inkey, inkeysize, salt, saltsize, tempbuf, 4) != 0) {
    (sha256pd_free_fn ? sha256pd_free_fn : free)(buf);
    return -1;
  }
  #else
  /* create initial key */
  sha256pd_init(&state);
  sha256pd_update(&state, salt, saltsize);
  sha256pd_update(&state, inkey, inkeysize);
  sha256pd_balloon_hash_u32(&state, cnt++);
  sha256pd_finish(&state, buf+SHA256PD_HASH_SIZE*0u); /* [0] */
  #endif

  /* expand input */
  for (unsigned m = 1u; m < scost; ++m) {
    sha256pd_init(&state);
    sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*(m-1u), SHA256PD_HASH_SIZE); /* [m-1] */
    sha256pd_balloon_hash_u32(&state, cnt++);
    sha256pd_finish(&state, buf+SHA256PD_HASH_SIZE*m); /* [m] */
  }

  #ifdef SHA256PD_BALLOON_USE_ISAAC
  isaacp_state rng;
  memset(&rng, 0, sizeof(rng));
  #endif

  /* mix buffer contents */
  for (unsigned t = 0u; t < tcost; ++t) {
    for (unsigned m = 0u; m < scost; ++m) {
      /* hash last and current blocks */
      sha256pd_init(&state);
      sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*m, SHA256PD_HASH_SIZE); /* [m] */
      sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*((m+scost-1u)%scost), SHA256PD_HASH_SIZE); /* [m-1] */
      sha256pd_balloon_hash_u32(&state, cnt++);
      sha256pd_finish(&state, buf+SHA256PD_HASH_SIZE*m); /* [m] */
      /* hash in pseudorandomly choosen blocks */
      for (unsigned i = 0u; i < delta; ++i) {
        #ifdef SHA256PD_BALLOON_USE_ISAAC
        //const unsigned bidx = ints_to_block(t, m, i);
        sha256pd_init(&state);
        sha256pd_balloon_hash_u32(&state, i);
        sha256pd_balloon_hash_u32(&state, m);
        sha256pd_balloon_hash_u32(&state, t);
        sha256pd_finish(&state, (uint8_t *)rng.state);
        for (unsigned f = 1u; f < 8u; ++f) {
          sha256pd_init(&state);
          sha256pd_update(&state, ((uint8_t *)rng.state)+(f-1u)*SHA256PD_HASH_SIZE, SHA256PD_HASH_SIZE);
          sha256pd_finish(&state, ((uint8_t *)rng.state)+f*SHA256PD_HASH_SIZE);
        }
        sha256pd_init(&state);
        sha256pd_update(&state, ((uint8_t *)rng.state)+(7u)*SHA256PD_HASH_SIZE, SHA256PD_HASH_SIZE);
        sha256pd_finish(&state, tempbuf);
        rng.abc[0] = GET_U32(tempbuf+4u*0u);
        rng.abc[1] = GET_U32(tempbuf+4u*1u);
        rng.abc[2] = GET_U32(tempbuf+4u*2u);
        isaacp_mix(&rng);
        isaacp_mix(&rng);
        isaacp_random(&rng, tempbuf, 4);
        const unsigned bidx = GET_U32(tempbuf)%scost;
        /* hash(cnt++, salt, buf+SHA256PD_HASH_SIZE*bidx) */
        sha256pd_init(&state);
        sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*bidx, SHA256PD_HASH_SIZE); /* [bidx] */
        #else
        //const unsigned bidx = ints_to_block(cnt++, salt, t, m, i);
        sha256pd_init(&state);
        sha256pd_balloon_hash_u32(&state, i);
        sha256pd_balloon_hash_u32(&state, m);
        sha256pd_balloon_hash_u32(&state, t);
        sha256pd_update(&state, salt, saltsize);
        sha256pd_balloon_hash_u32(&state, cnt++);
        sha256pd_finish(&state, tempbuf);
        const unsigned bidx = GET_U32(tempbuf)%scost;
        sha256pd_init(&state);
        sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*bidx, SHA256PD_HASH_SIZE); /* [bidx] */
        #endif
        sha256pd_update(&state, salt, saltsize);
        sha256pd_balloon_hash_u32(&state, cnt++);
        sha256pd_finish(&state, tempbuf);
        /* get other block number */
        const unsigned other = GET_U32(tempbuf)%scost;
        /* mix other block */
        sha256pd_init(&state);
        sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*other, SHA256PD_HASH_SIZE); /* [other] */
        sha256pd_update(&state, buf+SHA256PD_HASH_SIZE*m, SHA256PD_HASH_SIZE); /* [m] */
        sha256pd_balloon_hash_u32(&state, cnt++);
        sha256pd_finish(&state, buf+SHA256PD_HASH_SIZE*m); /* [m] */
      }
    }
  }

  /* return key */
  memcpy(reskey, buf+SHA256PD_HASH_SIZE*(scost-1u), SHA256PD_HASH_SIZE);

  #ifdef SHA256PD_BALLOON_USE_ISAAC
  /* clear rng state */
  sha256pd_memerase(&rng, sizeof(rng));
  #endif
  /* clear sha256 state */
  sha256pd_memerase(&state, sizeof(state));
  /* clear buffer */
  sha256pd_memerase(buf, (scost+1u)*SHA256PD_HASH_SIZE);
  /* free buffer */
  (sha256pd_free_fn ? sha256pd_free_fn : free)(buf);

  return 0;
}


#ifdef __cplusplus
}
#endif
