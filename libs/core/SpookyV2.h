//
// SpookyHash: a 128-bit noncryptographic hash function
// By Bob Jenkins, public domain
//   Oct 31 2010: alpha, framework + SpookyHash::Mix appears right
//   Oct 31 2011: alpha again, Mix only good to 2^^69 but rest appears right
//   Dec 31 2011: beta, improved Mix, tested it for 2-bit deltas
//   Feb  2 2012: production, same bits as beta
//   Feb  5 2012: adjusted definitions of uint* to be more portable
//   Mar 30 2012: 3 bytes/cycle, not 4.  Alpha was 4 but wasn't thorough enough.
//   August 5 2012: SpookyV2 (different results)
// C conversion by Ketmar Dark
//
// Up to 3 bytes/cycle for long messages.  Reasonably fast for short messages.
// All 1 or 2 bit deltas achieve avalanche within 1% bias per output bit.
//
// This was developed for and tested on 64-bit x86-compatible processors.
// It assumes the processor is little-endian.  There is a macro
// controlling whether unaligned reads are allowed (by default they are).
// This should be an equally good hash on big-endian machines, but it will
// compute different results on them than on little-endian machines.
//
// Google's CityHash has similar specs to SpookyHash, and CityHash is faster
// on new Intel boxes.  MD4 and MD5 also have similar specs, but they are orders
// of magnitude slower.  CRCs are two or more times slower, but unlike
// SpookyHash, they have nice math for combining the CRCs of pieces to form
// the CRCs of wholes.  There are also cryptographic hashes, but those are even
// slower than MD5.
//
#ifndef SPOOKY_HASH_C_HEADER
#define SPOOKY_HASH_C_HEADER

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// number of uint64_t's in internal state
#define SPOOKY_HASH_CONST_NUMVARS  (12u)


typedef struct {
  uint64_t m_data[2*SPOOKY_HASH_CONST_NUMVARS]; // unhashed data, for partial messages
  uint64_t m_state[SPOOKY_HASH_CONST_NUMVARS]; // internal state of the hash
  size_t m_length; // total length of the input so far
  uint8_t m_remainder; // length of unhashed data stashed in m_data
} SpookyHash_Ctx;


// short is used for messages under 192 bytes in length.
// it could be used on any message, but it's used by Spooky just for short messages.
// short has a low startup cost, the normal mode is good for long
// keys, the cost crossover is at about 192 bytes.
// the two modes were held to the same quality bar.
// exported here just for completeness
// `hash1`: in/out: in seed 1, out hash value 1; any 64-bit value will do, including 0
// `hash2`: in/out: in seed 2, out hash value 2; different seeds produce independent hashes
extern void spooky_short (const void *message, size_t length, uint64_t *hash1, uint64_t *hash2);


// hash a single message in one call, produce 128-bit output
//
// `message`: message to hash
// `length`: length of message in bytes
// `hash1`: in/out: in seed 1, out hash value 1; any 64-bit value will do, including 0
// `hash2`: in/out: in seed 2, out hash value 2; different seeds produce independent hashes
extern void spooky_hash128 (const void *message, size_t length, uint64_t *hash1, uint64_t *hash2);


// hash a single message in one call, produce 64-bit output
//
// `message`: message to hash
// `length`: length of message in bytes
// `hash1`: in/out: in seed 1, out hash value 1; any 64-bit value will do, including 0
// `hash2`: in/out: in seed 2, out hash value 2; different seeds produce independent hashes
static inline __attribute__((unused)) uint64_t spooky_hash64 (const void *message, size_t length, uint64_t seed) {
  uint64_t hash1 = seed;
  spooky_hash128(message, length, &hash1, &seed);
  return hash1;
}


// hash a single message in one call, produce 32-bit output
//
// `message`: message to hash
// `length`: length of message in bytes
// `hash1`: in/out: in seed 1, out hash value 1; any 64-bit value will do, including 0
// `hash2`: in/out: in seed 2, out hash value 2; different seeds produce independent hashes
static inline __attribute__((unused)) uint32_t spooky_hash32 (const void *message, size_t length, uint32_t seed) {
  uint64_t hash1 = seed, hash2 = seed;
  spooky_hash128(message, length, &hash1, &hash2);
  return (uint32_t)hash1;
}


// initialize the context of a SpookyHash
// `seed1`: any 64-bit value will do, including 0
// `seed2`: different seeds produce independent hashes
static inline __attribute__((unused)) void spooky_init (SpookyHash_Ctx *ctx, uint64_t seed1, uint64_t seed2) {
  ctx->m_length = 0;
  ctx->m_remainder = 0;
  ctx->m_state[0] = seed1;
  ctx->m_state[1] = seed2;
}


// add a piece of a message to a SpookyHash state
extern void spooky_update (SpookyHash_Ctx *ctx, const void *message, size_t length);


// compute the hash for the current state
//
// this does not modify the state; you can keep updating it afterward
//
// the result is the same as if SpookyHash had been called with
// all the pieces concatenated into one message.
//
// `hash1`: out only: first 64 bits of hash value.
// `hash2`: out only: second 64 bits of hash value.
extern void spooky_final (const SpookyHash_Ctx *ctx, uint64_t *hash1, uint64_t *hash2);


#ifdef __cplusplus
}
#endif

#endif
