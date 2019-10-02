/*
  Copyright (c) 2015 Orson Peters <orsonpeters@gmail.com>
  Modified by Ketmar Dark // Invisible Vector

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not
     be misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.

  g++ -DCHACHA_SINGLE_HEADER_TEST -O2 -Wall -march=native -mtune=native chacha.cpp -o chacha
*/
#ifndef CHACHA_SINGLE_HEADER
#define CHACHA_SINGLE_HEADER

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//#define CHACHA_DISABLE_SSE

#if !defined(CHACHA_DISABLE_SSE) && defined(__SSE2__)
# include "emmintrin.h"
# if !defined(__XOP__)
#  if defined(__SSSE3__)
#   include <tmmintrin.h>
#  endif
# else
#  include <xopintrin.h>
# endif
#endif


template<size_t R> class ChaCha {
  static_assert(R > 0 && R <= 64 && (R&1) == 0, "invalid number of rounds for ChaCha");
private:
#if !defined(CHACHA_DISABLE_SSE) && defined(__SSE2__)
  alignas(16)
#endif
  uint32_t block[16];
  uint32_t keysetup[8];
  uint64_t ctr;

public:
  static constexpr inline uint32_t get_state_size () { return (uint32_t)(sizeof(block)+sizeof(keysetup)+sizeof(ctr)+1); }

  inline void save_state (void *dest) {
    static_assert(sizeof(char) == 1, "invalid `char` size");
    char *p = (char *)dest;
    memcpy(p, block, sizeof(block)); p += sizeof(block);
    memcpy(p, keysetup, sizeof(keysetup)); p += sizeof(keysetup);
    memcpy(p, &ctr, sizeof(ctr)); p += sizeof(ctr);
    *p = (char)R;
  }

  // this won't restore number of rounds; alas
  inline void restore_state (const void *dest) {
    static_assert(sizeof(char) == 1, "invalid `char` size");
    const char *p = (const char *)dest;
    memcpy(block, p, sizeof(block)); p += sizeof(block);
    memcpy(keysetup, p, sizeof(keysetup)); p += sizeof(keysetup);
    memcpy(&ctr, p, sizeof(ctr));
  }

private:
  static __attribute__((unused)) inline uint64_t cha_splitmix64_next (uint64_t *state) {
    uint64_t result = *state;
    *state = result+(uint64_t)0x9E3779B97f4A7C15ULL;
    result = (result^(result>>30))*(uint64_t)0xBF58476D1CE4E5B9ULL;
    result = (result^(result>>27))*(uint64_t)0x94D049BB133111EBULL;
    return result^(result>>31);
  }

  static __attribute__((unused)) inline void cha_splitmix64_seedU32 (uint64_t *state, uint32_t seed) {
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

  void generate_block () {
    static const uint32_t constants[4] = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};
    uint32_t input[16];
    for (unsigned i = 0u; i < 4u; ++i) input[i] = constants[i];
    for (unsigned i = 0u; i < 8u; ++i) input[4u+i] = keysetup[i];
    input[12] = (ctr/16u)&0xffffffffu;
    input[13] = (ctr/16u)>>32;
    input[14] = input[15] = 0xdeadbeef; // could use 128-bit counter
    for (unsigned i = 0u; i < 16u; ++i) block[i] = input[i];
    chacha_core();
    for (unsigned i = 0u; i < 16u; ++i) block[i] += input[i];
  }

  #if !defined(CHACHA_DISABLE_SSE) && defined(__SSE2__)
    /*#include "emmintrin.h"*/

    // get an efficient _mm_roti_epi32 based on enabled features
    #if !defined(__XOP__)
      #if defined(__SSSE3__)
        /*#include <tmmintrin.h>*/
        #define _mm_roti_epi32_chacha(r, c) ( \
          ((c) == 8) ? \
              _mm_shuffle_epi8((r), _mm_set_epi8(14, 13, 12, 15,  \
                                                 10,  9,  8, 11,  \
                                                  6,  5,  4,  7,  \
                                                  2,  1,  0,  3)) \
          : ((c) == 16) ? \
              _mm_shuffle_epi8((r), _mm_set_epi8(13, 12, 15, 14,  \
                                                  9,  8, 11, 10,  \
                                                  5,  4,  7,  6,  \
                                                  1,  0,  3,  2)) \
          : ((c) == 24) ? \
              _mm_shuffle_epi8((r), _mm_set_epi8(12, 15, 14, 13,  \
                                                  8, 11, 10,  9,  \
                                                  4,  7,  6,  5,  \
                                                  0,  3,  2,  1)) \
          : \
              _mm_xor_si128(_mm_slli_epi32((r), (c)), \
                            _mm_srli_epi32((r), 32-(c))) \
        )
      #else
        #define _mm_roti_epi32_chacha(r, c) _mm_xor_si128(_mm_slli_epi32((r), (c)), \
                                                          _mm_srli_epi32((r), 32-(c)))
      #endif
      #define chacha_undef_mm_roti_epi32
    #else
      /*#include <xopintrin.h>*/
      #define _mm_roti_epi32_chacha  _mm_roti_epi32
      #ifdef chacha_undef_mm_roti_epi32
      # undef chacha_undef_mm_roti_epi32
      #endif
    #endif

    inline void chacha_core () {
      // ROTVn rotates the elements in the given vector n places to the left
      #define CHACHA_ROTV1(x) _mm_shuffle_epi32((__m128i) x, 0x39)
      #define CHACHA_ROTV2(x) _mm_shuffle_epi32((__m128i) x, 0x4e)
      #define CHACHA_ROTV3(x) _mm_shuffle_epi32((__m128i) x, 0x93)

      __m128i a = _mm_load_si128((__m128i*) (block));
      __m128i b = _mm_load_si128((__m128i*) (block+4));
      __m128i c = _mm_load_si128((__m128i*) (block+8));
      __m128i d = _mm_load_si128((__m128i*) (block+12));

      for (unsigned i = 0; i < R; i += 2) {
        a = _mm_add_epi32(a, b);
        d = _mm_xor_si128(d, a);
        d = _mm_roti_epi32_chacha(d, 16);
        c = _mm_add_epi32(c, d);
        b = _mm_xor_si128(b, c);
        b = _mm_roti_epi32_chacha(b, 12);
        a = _mm_add_epi32(a, b);
        d = _mm_xor_si128(d, a);
        d = _mm_roti_epi32_chacha(d, 8);
        c = _mm_add_epi32(c, d);
        b = _mm_xor_si128(b, c);
        b = _mm_roti_epi32_chacha(b, 7);

        b = CHACHA_ROTV1(b);
        c = CHACHA_ROTV2(c);
        d = CHACHA_ROTV3(d);

        a = _mm_add_epi32(a, b);
        d = _mm_xor_si128(d, a);
        d = _mm_roti_epi32_chacha(d, 16);
        c = _mm_add_epi32(c, d);
        b = _mm_xor_si128(b, c);
        b = _mm_roti_epi32_chacha(b, 12);
        a = _mm_add_epi32(a, b);
        d = _mm_xor_si128(d, a);
        d = _mm_roti_epi32_chacha(d, 8);
        c = _mm_add_epi32(c, d);
        b = _mm_xor_si128(b, c);
        b = _mm_roti_epi32_chacha(b, 7);

        b = CHACHA_ROTV3(b);
        c = CHACHA_ROTV2(c);
        d = CHACHA_ROTV1(d);
      }

      _mm_store_si128((__m128i*) (block), a);
      _mm_store_si128((__m128i*) (block+4), b);
      _mm_store_si128((__m128i*) (block+8), c);
      _mm_store_si128((__m128i*) (block+12), d);

      #undef CHACHA_ROTV3
      #undef CHACHA_ROTV2
      #undef CHACHA_ROTV1
    }
    #ifdef chacha_undef_mm_roti_epi32
      #undef chacha_undef_mm_roti_epi32
      #undef _mm_roti_epi32_chacha
    #endif
  #else
    inline void chacha_core () {
      #define CHACHA_ROTL32(x, n) (((x)<<(n))|((x)>>(32-(n))))

      #define CHACHA_QUARTERROUND(x, a, b, c, d) \
          x[a] = x[a]+x[b]; x[d] ^= x[a]; x[d] = CHACHA_ROTL32(x[d], 16); \
          x[c] = x[c]+x[d]; x[b] ^= x[c]; x[b] = CHACHA_ROTL32(x[b], 12); \
          x[a] = x[a]+x[b]; x[d] ^= x[a]; x[d] = CHACHA_ROTL32(x[d],  8); \
          x[c] = x[c]+x[d]; x[b] ^= x[c]; x[b] = CHACHA_ROTL32(x[b],  7)

      for (unsigned i = 0; i < R; i += 2) {
        CHACHA_QUARTERROUND(block, 0, 4,  8, 12);
        CHACHA_QUARTERROUND(block, 1, 5,  9, 13);
        CHACHA_QUARTERROUND(block, 2, 6, 10, 14);
        CHACHA_QUARTERROUND(block, 3, 7, 11, 15);
        CHACHA_QUARTERROUND(block, 0, 5, 10, 15);
        CHACHA_QUARTERROUND(block, 1, 6, 11, 12);
        CHACHA_QUARTERROUND(block, 2, 7,  8, 13);
        CHACHA_QUARTERROUND(block, 3, 4,  9, 14);
      }

      #undef CHACHA_QUARTERROUND
      #undef CHACHA_ROTL32
    }
  #endif

public:
  inline ChaCha () { seed(0x29au); }
  explicit inline ChaCha (uint64_t seedval, uint64_t stream) { seed(seedval, stream); }
  explicit inline ChaCha (uint32_t seedval) { seed(seedval); }

  inline void seed (uint64_t seedval, uint64_t stream) {
    ctr = 0u;
    keysetup[0] = seedval&0xffffffffu;
    keysetup[1] = seedval>>32;
    keysetup[2] = keysetup[3] = 0xdeadbeef; // could use 128-bit seed
    keysetup[4] = stream&0xffffffffu;
    keysetup[5] = stream>>32;
    keysetup[6] = keysetup[7] = 0xdeadbeef; // could use 128-bit stream
  }

  inline void seed (uint32_t seedval) {
    uint64_t smx;
    cha_splitmix64_seedU32(&smx, seedval);
    uint64_t s64 = cha_splitmix64_next(&smx);
    seed(s64, 0);
  }

  inline uint32_t operator () () {
    const unsigned idx = ctr%16u;
    if (idx == 0) generate_block();
    ++ctr;
    return block[idx];
  }

  inline void discard (unsigned n) {
    if (n) {
      const unsigned idx = ctr%16u;
      ctr += n;
      if (idx+n >= 16u && ctr%16u != 0) generate_block();
    }
  }

  inline bool operator == (const ChaCha<R> &rhs) const {
    uint64_t cx = (this->ctr^rhs.ctr);
    uint32_t kx = 0u;
    for (unsigned i = 0u; i < 8u; ++i) kx |= (this->keysetup[i]^rhs.keysetup[i]);
    return !(cx|kx);
  }

  inline bool operator != (const ChaCha<R> &rhs) const { return !(*this == rhs); }

  static constexpr inline uint32_t min () { return 0u; }
  static constexpr inline uint32_t max () { return 0xffffffffu; }
};


#ifdef CHACHA_SINGLE_HEADER_TEST
#include <stdio.h>
int main () {
  auto cha = ChaCha<20>(0x29a);
  printf("state size: %u bytes\n", cha.get_state_size());
  for (unsigned n = 0; n < 32; ++n) {
    auto val = cha();
    printf("%4u: %u\n", n, val);
  }
  if (cha != cha) abort();
  return 0;
}
#endif


#endif
