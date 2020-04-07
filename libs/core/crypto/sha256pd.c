/* based on the code from https://github.com/amosnier/sha-2 */
/* modified (streamified) by Ketmar Dark */
/* added HMAC-SHA256 and HKDF-SHA256 implementations */
/* public domain */
#include "sha256pd.h"

#ifdef __cplusplus
extern "C" {
#endif


static inline uint32_t sha26pd_rra (uint32_t value, unsigned int count) { return value>>count|value<<(32-count); }


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


#ifdef __cplusplus
}
#endif
