/*
chacha-ref.c version 20080118
D. J. Bernstein
Public domain.
*/
#include "chacha20.h"

#if defined(__cplusplus)
extern "C" {
#endif


static inline uint32_t CHACHA20_U8TO32_LITTLE (const void *p) { return ((uint32_t *)p)[0]; }
static inline void CHACHA20_U32TO8_LITTLE (void *p, const uint32_t v) { ((uint32_t *)p)[0] = v; }


#define CHACHA20_ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define CHACHA20_QUARTERROUND(a, b, c, d) ( \
  a += b,  d ^= a,  d = CHACHA20_ROTL(d,16), \
  c += d,  b ^= c,  b = CHACHA20_ROTL(b,12), \
  a += b,  d ^= a,  d = CHACHA20_ROTL(d, 8), \
  c += d,  b ^= c,  b = CHACHA20_ROTL(b, 7))

#define CHACHA20_ROUNDS  20

static inline void salsa20_wordtobyte (uint8_t output[64], const uint32_t input[16]) {
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
int chacha20_setup_ex (chacha20_ctx *ctx, const void *keydata, const void *noncedata, uint32_t keybits) {
  static const char *sigma = "expand 32-byte k";
  static const char *tau = "expand 16-byte k";

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
void chacha20_xcrypt (chacha20_ctx *ctx, void *ciphertextdata, const void *plaintextdata, uint32_t msglen) {
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


#if defined(__cplusplus)
}
#endif
