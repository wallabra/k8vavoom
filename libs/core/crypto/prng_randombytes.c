/* coded by Ketmar Dark */
/* public domain */
/* get (almost) crypto-secure random bytes */
/* this will try to use the best PRNG available */
#include "prng_randombytes.h"

#include <stdint.h>
#include <stdlib.h>
#ifndef WIN32
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <time.h>
# include <sys/time.h>
# if defined(__SWITCH__)
#  include <switch/kernel/random.h> // for randomGet()
# elif defined(__linux__) && !defined(ANDROID)
#  include <sys/random.h>
# endif
#endif

#ifdef __cplusplus
extern "C" {
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
  #if defined(__linux__) && !defined(ANDROID)
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
void prng_randombytes (void *p, size_t len) {
  static __thread unsigned initialized = 0u;
  static __thread isaacp_state rng;

  if (!len || !p) return;

  if (!initialized) {
    randombytes_init(&rng);
    initialized = 1u;
  }

  isaacp_random(&rng, p, len);
}


#ifdef __cplusplus
}
#endif
