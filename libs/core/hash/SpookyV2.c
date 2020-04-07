// Spooky Hash
// A 128-bit noncryptographic hash, for checksums and table lookup
// By Bob Jenkins.  Public domain.
//   Oct 31 2010: published framework, disclaimer ShortHash isn't right
//   Nov 7 2010: disabled ShortHash
//   Oct 31 2011: replace End, ShortMix, ShortEnd, enable ShortHash again
//   April 10 2012: buffer overflow on platforms without unaligned reads
//   July 12 2012: was passing out variables in final to in/out in short
//   July 30 2012: I reintroduced the buffer overflow
//   August 5 2012: SpookyV2: d = should be d += in short hash, and remove extra mix from long hash
// C conversion by Ketmar Dark

#include <memory.h>
#include "SpookyV2.h"

#define ALLOW_UNALIGNED_READS  1

// size of the internal state
#define SPOOKY_HASH_CONST_BLOCKSIZE  (SPOOKY_HASH_CONST_NUMVARS*8)

// size of buffer of unhashed data, in bytes
#define SPOOKY_HASH_CONST_BUFSIZE   (2*SPOOKY_HASH_CONST_BLOCKSIZE)

// SPOOKY_HASH_CONST_CONST: a constant which:
//  * is not zero
//  * is odd
//  * is a not-very-regular mix of 1's and 0's
//  * does not need any other special mathematical properties
#define SPOOKY_HASH_CONST_CONST  (0xdeadbeefdeadbeefULL)


// left rotate a 64-bit value by k bytes
static inline uint64_t Rot64 (uint64_t x, int k) { return (x<<k)|(x>>(64-k)); }


// This is used if the input is 96 bytes long or longer.
//
// The internal state is fully overwritten every 96 bytes.
// Every input bit appears to cause at least 128 bits of entropy
// before 96 other bytes are combined, when run forward or backward
//   For every input bit,
//   Two inputs differing in just that input bit
//   Where "differ" means xor or subtraction
//   And the base value is random
//   When run forward or backwards one Mix
// I tried 3 pairs of each; they all differed by at least 212 bits.
/*
static inline void spookyMix (const uint64_t *data_,
                              uint64_t &s0_, uint64_t &s1_, uint64_t &s2_, uint64_t &s3_,
                              uint64_t &s4_, uint64_t &s5_, uint64_t &s6_, uint64_t &s7_,
                              uint64_t &s8_, uint64_t &s9_, uint64_t &s10_,uint64_t &s11_)
*/
#define spookyMix(data_,s0_,s1_,s2_,s3_,s4_,s5_,s6_,s7_,s8_,s9_,s10_,s11_) \
do { \
  (s0_) += (data_)[0];    (s2_) ^= (s10_); (s11_) ^= (s0_);   (s0_) = Rot64((s0_), 11);   (s11_) += (s1_); \
  (s1_) += (data_)[1];    (s3_) ^= (s11_);  (s0_) ^= (s1_);   (s1_) = Rot64((s1_), 32);   (s0_) += (s2_); \
  (s2_) += (data_)[2];    (s4_) ^= (s0_);   (s1_) ^= (s2_);   (s2_) = Rot64((s2_), 43);   (s1_) += (s3_); \
  (s3_) += (data_)[3];    (s5_) ^= (s1_);   (s2_) ^= (s3_);   (s3_) = Rot64((s3_), 31);   (s2_) += (s4_); \
  (s4_) += (data_)[4];    (s6_) ^= (s2_);   (s3_) ^= (s4_);   (s4_) = Rot64((s4_), 17);   (s3_) += (s5_); \
  (s5_) += (data_)[5];    (s7_) ^= (s3_);   (s4_) ^= (s5_);   (s5_) = Rot64((s5_), 28);   (s4_) += (s6_); \
  (s6_) += (data_)[6];    (s8_) ^= (s4_);   (s5_) ^= (s6_);   (s6_) = Rot64((s6_), 39);   (s5_) += (s7_); \
  (s7_) += (data_)[7];    (s9_) ^= (s5_);   (s6_) ^= (s7_);   (s7_) = Rot64((s7_), 57);   (s6_) += (s8_); \
  (s8_) += (data_)[8];   (s10_) ^= (s6_);   (s7_) ^= (s8_);   (s8_) = Rot64((s8_), 55);   (s7_) += (s9_); \
  (s9_) += (data_)[9];   (s11_) ^= (s7_);   (s8_) ^= (s9_);   (s9_) = Rot64((s9_), 54);   (s8_) += (s10_); \
  (s10_) += (data_)[10];  (s0_) ^= (s8_);   (s9_) ^= (s10_); (s10_) = Rot64((s10_), 22);  (s9_) += (s11_); \
  (s11_) += (data_)[11];  (s1_) ^= (s9_);  (s10_) ^= (s11_); (s11_) = Rot64((s11_), 46); (s10_) += (s0_); \
} while (0)


// Mix all 12 inputs together so that h0, h1 are a hash of them all.
//
// For two inputs differing in just the input bits
// Where "differ" means xor or subtraction
// And the base value is random, or a counting value starting at that bit
// The final result will have each bit of h0, h1 flip
// For every input bit,
// with probability 50 +- .3%
// For every pair of input bits,
// with probability 50 +- 3%
//
// This does not rely on the last spookyMix() call having already mixed some.
// Two iterations was almost good enough for a 64-bit result, but a
// 128-bit result is reported, so spookyEnd() does three iterations.
/*
static inline void spookyEndPartial (uint64_t &h0, uint64_t &h1, uint64_t &h2, uint64_t &h3,
                                     uint64_t &h4, uint64_t &h5, uint64_t &h6, uint64_t &h7,
                                     uint64_t &h8, uint64_t &h9, uint64_t &h10, uint64_t &h11)
*/
#define spookyEndPartial(h0_,h1_,h2_,h3_,h4_,h5_,h6_,h7_,h8_,h9_,h10_,h11_) \
do { \
  (h11_)+= (h1_);  (h2_) ^= (h11_); (h1_) = Rot64((h1_), 44); \
  (h0_) += (h2_);  (h3_) ^= (h0_);  (h2_) = Rot64((h2_), 15); \
  (h1_) += (h3_);  (h4_) ^= (h1_);  (h3_) = Rot64((h3_), 34); \
  (h2_) += (h4_);  (h5_) ^= (h2_);  (h4_) = Rot64((h4_), 21); \
  (h3_) += (h5_);  (h6_) ^= (h3_);  (h5_) = Rot64((h5_), 38); \
  (h4_) += (h6_);  (h7_) ^= (h4_);  (h6_) = Rot64((h6_), 33); \
  (h5_) += (h7_);  (h8_) ^= (h5_);  (h7_) = Rot64((h7_), 10); \
  (h6_) += (h8_);  (h9_) ^= (h6_);  (h8_) = Rot64((h8_), 13); \
  (h7_) += (h9_);  (h10_)^= (h7_);  (h9_) = Rot64((h9_), 38); \
  (h8_) += (h10_); (h11_)^= (h8_);  (h10_)= Rot64((h10_), 53); \
  (h9_) += (h11_); (h0_) ^= (h9_);  (h11_)= Rot64((h11_), 42); \
  (h10_)+= (h0_);  (h1_) ^= (h10_); (h0_) = Rot64((h0_), 54); \
} while (0)


/*
static inline void spookyEnd (const uint64_t *data,
                              uint64_t &h0, uint64_t &h1, uint64_t &h2, uint64_t &h3,
                              uint64_t &h4, uint64_t &h5, uint64_t &h6, uint64_t &h7,
                              uint64_t &h8, uint64_t &h9, uint64_t &h10,uint64_t &h11)
*/
#define spookyEnd(data_,h0_,h1_,h2_,h3_,h4_,h5_,h6_,h7_,h8_,h9_,h10_,h11_) \
do { \
  (h0_) += (data_)[0]; (h1_) += (data_)[1];  (h2_) += (data_)[2];   (h3_) += (data_)[3]; \
  (h4_) += (data_)[4]; (h5_) += (data_)[5];  (h6_) += (data_)[6];   (h7_) += (data_)[7]; \
  (h8_) += (data_)[8]; (h9_) += (data_)[9]; (h10_) += (data_)[10]; (h11_) += (data_)[11]; \
  spookyEndPartial((h0_), (h1_), (h2_), (h3_), (h4_), (h5_), (h6_), (h7_), (h8_), (h9_), (h10_), (h11_)); \
  spookyEndPartial((h0_), (h1_), (h2_), (h3_), (h4_), (h5_), (h6_), (h7_), (h8_), (h9_), (h10_), (h11_)); \
  spookyEndPartial((h0_), (h1_), (h2_), (h3_), (h4_), (h5_), (h6_), (h7_), (h8_), (h9_), (h10_), (h11_)); \
} while (0)


// The goal is for each bit of the input to expand into 128 bits of
// apparent entropy before it is fully overwritten.
// n trials both set and cleared at least m bits of h0 h1 h2 h3
//   n: 2   m: 29
//   n: 3   m: 46
//   n: 4   m: 57
//   n: 5   m: 107
//   n: 6   m: 146
//   n: 7   m: 152
// when run forwards or backwards
// for all 1-bit and 2-bit diffs
// with diffs defined by either xor or subtraction
// with a base of all zeros plus a counter, or plus another bit, or random
//static inline void spookyShortMix (uint64_t &h0, uint64_t &h1, uint64_t &h2, uint64_t &h3)
#define spookyShortMix(h0_,h1_,h2_,h3_) \
do { \
  (h2_) = Rot64((h2_), 50); (h2_) += (h3_); (h0_) ^= (h2_); \
  (h3_) = Rot64((h3_), 52); (h3_) += (h0_); (h1_) ^= (h3_); \
  (h0_) = Rot64((h0_), 30); (h0_) += (h1_); (h2_) ^= (h0_); \
  (h1_) = Rot64((h1_), 41); (h1_) += (h2_); (h3_) ^= (h1_); \
  (h2_) = Rot64((h2_), 54); (h2_) += (h3_); (h0_) ^= (h2_); \
  (h3_) = Rot64((h3_), 48); (h3_) += (h0_); (h1_) ^= (h3_); \
  (h0_) = Rot64((h0_), 38); (h0_) += (h1_); (h2_) ^= (h0_); \
  (h1_) = Rot64((h1_), 37); (h1_) += (h2_); (h3_) ^= (h1_); \
  (h2_) = Rot64((h2_), 62); (h2_) += (h3_); (h0_) ^= (h2_); \
  (h3_) = Rot64((h3_), 34); (h3_) += (h0_); (h1_) ^= (h3_); \
  (h0_) = Rot64((h0_), 5);  (h0_) += (h1_); (h2_) ^= (h0_); \
  (h1_) = Rot64((h1_), 36); (h1_) += (h2_); (h3_) ^= (h1_); \
} while (0)


// Mix all 4 inputs together so that h0, h1 are a hash of them all.
//
// For two inputs differing in just the input bits
// Where "differ" means xor or subtraction
// And the base value is random, or a counting value starting at that bit
// The final result will have each bit of h0, h1 flip
// For every input bit,
// with probability 50 +- .3% (it is probably better than that)
// For every pair of input bits,
// with probability 50 +- .75% (the worst case is approximately that)
//static inline void spookyShortEnd (uint64_t &h0, uint64_t &h1, uint64_t &h2, uint64_t &h3)
#define spookyShortEnd(h0_,h1_,h2_,h3_) \
do { \
  (h3_) ^= (h2_); (h2_) = Rot64((h2_), 15); (h3_) += (h2_); \
  (h0_) ^= (h3_); (h3_) = Rot64((h3_), 52); (h0_) += (h3_); \
  (h1_) ^= (h0_); (h0_) = Rot64((h0_), 26); (h1_) += (h0_); \
  (h2_) ^= (h1_); (h1_) = Rot64((h1_), 51); (h2_) += (h1_); \
  (h3_) ^= (h2_); (h2_) = Rot64((h2_), 28); (h3_) += (h2_); \
  (h0_) ^= (h3_); (h3_) = Rot64((h3_), 9);  (h0_) += (h3_); \
  (h1_) ^= (h0_); (h0_) = Rot64((h0_), 47); (h1_) += (h0_); \
  (h2_) ^= (h1_); (h1_) = Rot64((h1_), 54); (h2_) += (h1_); \
  (h3_) ^= (h2_); (h2_) = Rot64((h2_), 32); (h3_) += (h2_); \
  (h0_) ^= (h3_); (h3_) = Rot64((h3_), 25); (h0_) += (h3_); \
  (h1_) ^= (h0_); (h0_) = Rot64((h0_), 63); (h1_) += (h0_); \
} while (0)


//==========================================================================
//
//  spooky_short
//
//  short is used for messages under 192 bytes in length.
//  it could be used on any message, but it's used by Spooky just for short messages.
//  short has a low startup cost, the normal mode is good for long
//  keys, the cost crossover is at about 192 bytes.
//  the two modes were held to the same quality bar.
//
//==========================================================================
void spooky_short (const void *message, size_t length, uint64_t *hash1, uint64_t *hash2) {
  uint64_t buf[2*SPOOKY_HASH_CONST_NUMVARS];
  union __attribute__((packed)) {
    const uint8_t *p8;
    uint32_t *p32;
    uint64_t *p64;
    size_t i;
  } u;

  u.p8 = (const uint8_t *)message;

  if (!ALLOW_UNALIGNED_READS && (u.i&0x7)) {
    memcpy(buf, message, length);
    u.p64 = buf;
  }

  size_t remainder = length%32;
  uint64_t a = *hash1;
  uint64_t b = *hash2;
  uint64_t c = SPOOKY_HASH_CONST_CONST;
  uint64_t d = SPOOKY_HASH_CONST_CONST;

  if (length > 15) {
    const uint64_t *end = u.p64+(length/32)*4;
    // handle all complete sets of 32 bytes
    for (; u.p64 < end; u.p64 += 4) {
      c += u.p64[0];
      d += u.p64[1];
      spookyShortMix(a, b, c, d);
      a += u.p64[2];
      b += u.p64[3];
    }
    // handle the case of 16+ remaining bytes
    if (remainder >= 16) {
      c += u.p64[0];
      d += u.p64[1];
      spookyShortMix(a, b, c, d);
      u.p64 += 2;
      remainder -= 16;
    }
  }

  // handle the last 0..15 bytes, and its length
  d += ((uint64_t)length)<<56;
  switch (remainder) {
    case 15:
      d += ((uint64_t)u.p8[14])<<48; /* fallthrough */
    case 14:
      d += ((uint64_t)u.p8[13])<<40; /* fallthrough */
    case 13:
      d += ((uint64_t)u.p8[12])<<32; /* fallthrough */
    case 12:
      d += u.p32[2];
      c += u.p64[0];
      break;
    case 11:
      d += ((uint64_t)u.p8[10])<<16; /* fallthrough */
    case 10:
      d += ((uint64_t)u.p8[9])<<8; /* fallthrough */
    case 9:
      d += (uint64_t)u.p8[8]; /* fallthrough */
    case 8:
      c += u.p64[0];
      break;
    case 7:
      c += ((uint64_t)u.p8[6])<<48; /* fallthrough */
    case 6:
      c += ((uint64_t)u.p8[5])<<40; /* fallthrough */
    case 5:
      c += ((uint64_t)u.p8[4])<<32; /* fallthrough */
    case 4:
      c += u.p32[0];
      break;
    case 3:
      c += ((uint64_t)u.p8[2])<<16; /* fallthrough */
    case 2:
      c += ((uint64_t)u.p8[1])<<8; /* fallthrough */
    case 1:
      c += (uint64_t)u.p8[0];
      break;
    case 0:
      c += SPOOKY_HASH_CONST_CONST;
      d += SPOOKY_HASH_CONST_CONST;
  }
  spookyShortEnd(a, b, c, d);
  *hash1 = a;
  *hash2 = b;
}


//==========================================================================
//
//  spooky_hash128
//
//  do the whole hash in one call
//
//  `message`: message to hash
//  `length`: length of message in bytes
//  `hash1`: in/out: in seed 1, out hash value 1; any 64-bit value will do, including 0
//  `hash2`: in/out: in seed 2, out hash value 2; different seeds produce independent hashes
//
//  produce 128-bit output
//
//==========================================================================
void spooky_hash128 (const void *message, size_t length, uint64_t *hash1, uint64_t *hash2) {
  if (length < SPOOKY_HASH_CONST_BUFSIZE) {
    spooky_short(message, length, hash1, hash2);
    return;
  }

  uint64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
  uint64_t buf[SPOOKY_HASH_CONST_NUMVARS];
  uint64_t *end;
  union __attribute__((packed)) {
    const uint8_t *p8;
    uint64_t *p64;
    size_t i;
  } u;
  size_t remainder;

  h0 = h3 = h6 = h9  = *hash1;
  h1 = h4 = h7 = h10 = *hash2;
  h2 = h5 = h8 = h11 = SPOOKY_HASH_CONST_CONST;

  u.p8 = (const uint8_t *)message;
  end = u.p64+(length/SPOOKY_HASH_CONST_BLOCKSIZE)*SPOOKY_HASH_CONST_NUMVARS;

  // handle all whole SPOOKY_HASH_CONST_BLOCKSIZE blocks of bytes
  if (ALLOW_UNALIGNED_READS || ((u.i&0x7) == 0)) {
    while (u.p64 < end) {
      spookyMix(u.p64, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += SPOOKY_HASH_CONST_NUMVARS;
    }
  } else {
    while (u.p64 < end) {
      memcpy(buf, u.p64, SPOOKY_HASH_CONST_BLOCKSIZE);
      spookyMix(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += SPOOKY_HASH_CONST_NUMVARS;
    }
  }

  // handle the last partial block of SPOOKY_HASH_CONST_BLOCKSIZE bytes
  remainder = (length-((const uint8_t *)end-(const uint8_t *)message));
  memcpy(buf, end, remainder);
  memset(((uint8_t *)buf)+remainder, 0, SPOOKY_HASH_CONST_BLOCKSIZE-remainder);
  ((uint8_t *)buf)[SPOOKY_HASH_CONST_BLOCKSIZE-1] = remainder;

  // do some final mixing
  spookyEnd(buf, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
  *hash1 = h0;
  *hash2 = h1;
}


//==========================================================================
//
//  spooky_update
//
//  add a message fragment to the state
//
//==========================================================================
void spooky_update (SpookyHash_Ctx *ctx, const void *message, size_t length) {
  if (length == 0) return; //k8: why not?

  uint64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
  size_t newLength = length+ctx->m_remainder;
  uint8_t remainder;
  union __attribute__((packed)) {
    const uint8_t *p8;
    uint64_t *p64;
    size_t i;
  } u;
  const uint64_t *end;

  // is this message fragment too short? if it is, stuff it away
  if (newLength < SPOOKY_HASH_CONST_BUFSIZE) {
    memcpy(&((uint8_t *)ctx->m_data)[ctx->m_remainder], message, length);
    ctx->m_length = length+ctx->m_length;
    ctx->m_remainder = (uint8_t)newLength;
    return;
  }

  // init the variables
  if (ctx->m_length < SPOOKY_HASH_CONST_BUFSIZE) {
    h0 = h3 = h6 = h9  = ctx->m_state[0];
    h1 = h4 = h7 = h10 = ctx->m_state[1];
    h2 = h5 = h8 = h11 = SPOOKY_HASH_CONST_CONST;
  } else {
    h0 = ctx->m_state[0];
    h1 = ctx->m_state[1];
    h2 = ctx->m_state[2];
    h3 = ctx->m_state[3];
    h4 = ctx->m_state[4];
    h5 = ctx->m_state[5];
    h6 = ctx->m_state[6];
    h7 = ctx->m_state[7];
    h8 = ctx->m_state[8];
    h9 = ctx->m_state[9];
    h10 = ctx->m_state[10];
    h11 = ctx->m_state[11];
  }
  ctx->m_length = length+ctx->m_length;

  // if we've got anything stuffed away, use it now
  if (ctx->m_remainder) {
    uint8_t prefix = SPOOKY_HASH_CONST_BUFSIZE-ctx->m_remainder;
    memcpy(&(((uint8_t *)ctx->m_data)[ctx->m_remainder]), message, prefix);
    u.p64 = ctx->m_data;
    spookyMix(u.p64, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
    spookyMix(&u.p64[SPOOKY_HASH_CONST_NUMVARS], h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
    u.p8 = ((const uint8_t *)message)+prefix;
    length -= prefix;
  } else {
    u.p8 = (const uint8_t *)message;
  }

  // handle all whole blocks of SPOOKY_HASH_CONST_BLOCKSIZE bytes
  end = u.p64+(length/SPOOKY_HASH_CONST_BLOCKSIZE)*SPOOKY_HASH_CONST_NUMVARS;
  remainder = (uint8_t)(length-((const uint8_t *)end-u.p8));
  if (ALLOW_UNALIGNED_READS || (u.i&0x7) == 0) {
    while (u.p64 < end) {
      spookyMix(u.p64, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += SPOOKY_HASH_CONST_NUMVARS;
    }
  } else {
    while (u.p64 < end) {
      memcpy(ctx->m_data, u.p8, SPOOKY_HASH_CONST_BLOCKSIZE);
      spookyMix(ctx->m_data, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
      u.p64 += SPOOKY_HASH_CONST_NUMVARS;
    }
  }

  // stuff away the last few bytes
  ctx->m_remainder = remainder;
  memcpy(ctx->m_data, end, remainder);

  // stuff away the variables
  ctx->m_state[0] = h0;
  ctx->m_state[1] = h1;
  ctx->m_state[2] = h2;
  ctx->m_state[3] = h3;
  ctx->m_state[4] = h4;
  ctx->m_state[5] = h5;
  ctx->m_state[6] = h6;
  ctx->m_state[7] = h7;
  ctx->m_state[8] = h8;
  ctx->m_state[9] = h9;
  ctx->m_state[10] = h10;
  ctx->m_state[11] = h11;
}


//==========================================================================
//
//  spooky_final
//
//  compute the hash for the current state
//
//  this does not modify the state; you can keep updating it afterward
//
//  the result is the same as if SpookyHash() had been called with
//  all the pieces concatenated into one message.
//
//  `hash1`: out only: first 64 bits of hash value.
//  `hash2`: out only: second 64 bits of hash value.
//
//==========================================================================
void spooky_final (const SpookyHash_Ctx *ctx, uint64_t *hash1, uint64_t *hash2) {
  // init the variables
  if (ctx->m_length < SPOOKY_HASH_CONST_BUFSIZE) {
    *hash1 = ctx->m_state[0];
    *hash2 = ctx->m_state[1];
    spooky_short(ctx->m_data, ctx->m_length, hash1, hash2);
    return;
  }

  const uint64_t *data = (const uint64_t *)ctx->m_data;
  uint8_t remainder = ctx->m_remainder;

  uint64_t h0 = ctx->m_state[0];
  uint64_t h1 = ctx->m_state[1];
  uint64_t h2 = ctx->m_state[2];
  uint64_t h3 = ctx->m_state[3];
  uint64_t h4 = ctx->m_state[4];
  uint64_t h5 = ctx->m_state[5];
  uint64_t h6 = ctx->m_state[6];
  uint64_t h7 = ctx->m_state[7];
  uint64_t h8 = ctx->m_state[8];
  uint64_t h9 = ctx->m_state[9];
  uint64_t h10 = ctx->m_state[10];
  uint64_t h11 = ctx->m_state[11];

  if (remainder >= SPOOKY_HASH_CONST_BLOCKSIZE) {
    // m_data can contain two blocks; handle any whole first block
    spookyMix(data, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
    data += SPOOKY_HASH_CONST_NUMVARS;
    remainder -= SPOOKY_HASH_CONST_BLOCKSIZE;
  }

  // mix in the last partial block, and the length mod SPOOKY_HASH_CONST_BLOCKSIZE
  memset(&((uint8_t *)data)[remainder], 0, (SPOOKY_HASH_CONST_BLOCKSIZE-remainder));

  ((uint8_t *)data)[SPOOKY_HASH_CONST_BLOCKSIZE-1] = remainder;

  // do some final mixing
  spookyEnd(data, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);

  *hash1 = h0;
  *hash2 = h1;
}
