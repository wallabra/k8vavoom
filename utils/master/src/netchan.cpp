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
    HMODULE libh = LoadLibraryA("advapi32.dll");
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
// SHA256
//
//**************************************************************************

#define SHA256_BLOCK_SIZE  (512/8)

struct sha256_ctx {
  uint32_t tot_len;
  uint32_t len;
  uint8_t block[2*SHA256_BLOCK_SIZE];
  uint32_t h[8];
};

static const uint32_t sha256_h0[8] =
            {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

static const uint32_t sha256_k[64] =
            {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
             0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
             0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
             0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
             0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
             0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
             0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
             0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
             0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define UNPACK32_SHA256(x, str)                      \
{                                             \
    *((str) + 3) = (uint8_t) ((x)      );       \
    *((str) + 2) = (uint8_t) ((x) >>  8);       \
    *((str) + 1) = (uint8_t) ((x) >> 16);       \
    *((str) + 0) = (uint8_t) ((x) >> 24);       \
}

#define PACK32_SHA256(str, x)                        \
{                                             \
    *(x) =   ((uint32_t) *((str) + 3)      )    \
           | ((uint32_t) *((str) + 2) <<  8)    \
           | ((uint32_t) *((str) + 1) << 16)    \
           | ((uint32_t) *((str) + 0) << 24);   \
}

#define SHFR_SHA256(x, n)    (x >> n)
#define ROTR_SHA256(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define CH_SHA256(x, y, z)  ((x & y) ^ (~x & z))
#define MAJ_SHA256(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define SHA256_F1(x) (ROTR_SHA256(x,  2) ^ ROTR_SHA256(x, 13) ^ ROTR_SHA256(x, 22))
#define SHA256_F2(x) (ROTR_SHA256(x,  6) ^ ROTR_SHA256(x, 11) ^ ROTR_SHA256(x, 25))
#define SHA256_F3(x) (ROTR_SHA256(x,  7) ^ ROTR_SHA256(x, 18) ^ SHFR_SHA256(x,  3))
#define SHA256_F4(x) (ROTR_SHA256(x, 17) ^ ROTR_SHA256(x, 19) ^ SHFR_SHA256(x, 10))

#define SHA256_SCR(i)                         \
{                                             \
    w[i] =  SHA256_F4(w[i -  2]) + w[i -  7]  \
          + SHA256_F3(w[i - 15]) + w[i - 16]; \
}

static void sha256_transf(sha256_ctx *ctx, const uint8_t *message,
                   uint32_t block_nb)
{
    uint32_t w[64];
    uint32_t wv[8];
    uint32_t t1, t2;
    const uint8_t *sub_block;
    int i, j;

    for (i = 0; i < (int) block_nb; i++) {
        sub_block = message + (i << 6);

        for (j = 0; j < 16; j++) {
            PACK32_SHA256(&sub_block[j << 2], &w[j]);
        }

        for (j = 16; j < 64; j++) {
            SHA256_SCR(j);
        }

        for (j = 0; j < 8; j++) {
            wv[j] = ctx->h[j];
        }

        for (j = 0; j < 64; j++) {
            t1 = wv[7] + SHA256_F2(wv[4]) + CH_SHA256(wv[4], wv[5], wv[6])
                + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + MAJ_SHA256(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }

        for (j = 0; j < 8; j++) {
            ctx->h[j] += wv[j];
        }
    }
}

void sha256_init(sha256_ctx *ctx)
{
    for (int i = 0; i < 8; i++) {
        ctx->h[i] = sha256_h0[i];
    }

    ctx->len = 0;
    ctx->tot_len = 0;
}

void sha256_update(sha256_ctx *ctx, const void *messagep,
                   size_t lenbig)
{
    uint32_t block_nb;
    uint32_t new_len, rem_len, tmp_len;
    const uint8_t *shifted_message;
    const uint8_t *message = (const uint8_t *)messagep;
    uint32_t len = (uint32_t)lenbig;

    tmp_len = SHA256_BLOCK_SIZE - ctx->len;
    rem_len = len < tmp_len ? len : tmp_len;

    memcpy(&ctx->block[ctx->len], message, rem_len);

    if (ctx->len + len < SHA256_BLOCK_SIZE) {
        ctx->len += len;
        return;
    }

    new_len = len - rem_len;
    block_nb = new_len / SHA256_BLOCK_SIZE;

    shifted_message = message + rem_len;

    sha256_transf(ctx, ctx->block, 1);
    sha256_transf(ctx, shifted_message, block_nb);

    rem_len = new_len % SHA256_BLOCK_SIZE;

    memcpy(ctx->block, &shifted_message[block_nb << 6],
           rem_len);

    ctx->len = rem_len;
    ctx->tot_len += (block_nb + 1) << 6;
}

void sha256_final(sha256_ctx *ctx, void *digestp)
{
    uint32_t block_nb;
    uint32_t pm_len;
    uint32_t len_b;
    uint8_t *digest = (uint8_t *)digestp;

    block_nb = (1 + ((SHA256_BLOCK_SIZE - 9)
                     < (ctx->len % SHA256_BLOCK_SIZE)));

    len_b = (ctx->tot_len + ctx->len) << 3;
    pm_len = block_nb << 6;

    memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
    ctx->block[ctx->len] = 0x80;
    UNPACK32_SHA256(len_b, ctx->block + pm_len - 4);

    sha256_transf(ctx, ctx->block, block_nb);

    for (int i = 0 ; i < 8; i++) {
        UNPACK32_SHA256(ctx->h[i], &digest[i << 2]);
    }
}

static void sha256_hashBuffer (VNetChanSocket::SHA256Digest hash, const void *in, size_t inlen) {
  sha256_ctx ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, in, inlen);
  sha256_final(&ctx, hash);
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
//  VNetChanSocket::SHA256Init
//
//==========================================================================
VNetChanSocket::SHA256Context VNetChanSocket::SHA256Init () noexcept {
  sha256_ctx *ctx = (sha256_ctx *)malloc(sizeof(sha256_ctx));
  if (!ctx) return nullptr;
  sha256_init(ctx);
  return (SHA256Context)ctx;
}


//==========================================================================
//
//  VNetChanSocket::SHA256Update
//
//==========================================================================
void VNetChanSocket::SHA256Update (VNetChanSocket::SHA256Context ctx, const void *in, size_t inlen) noexcept {
  if (!ctx) return;
  sha256_update((sha256_ctx *)ctx, in, inlen);
}


//==========================================================================
//
//  VNetChanSocket::SHA256Finish
//
//  this frees context
//
//==========================================================================
void VNetChanSocket::SHA256Finish (VNetChanSocket::SHA256Context ctx, VNetChanSocket::SHA256Digest hash) noexcept {
  if (!ctx) { if (hash) memset(hash, 0, SHA256DigestSize); return; }
  if (hash) sha256_final((sha256_ctx *)ctx, hash);
  free(ctx);
}


//==========================================================================
//
//  VNetChanSocket::SHA256Buffer
//
//==========================================================================
void VNetChanSocket::SHA256Buffer (VNetChanSocket::SHA256Digest hash, const void *in, size_t inlen) noexcept {
  sha256_hashBuffer(hash, in, inlen);
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
