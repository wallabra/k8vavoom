/*
chacha-ref.c version 20080118
D. J. Bernstein
Public domain.
*/
#ifndef CHACHA20_IMPLEMENTATION_INCLUDED_H
#define CHACHA20_IMPLEMENTATION_INCLUDED_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif


typedef struct {
  uint32_t input[16];
} chacha20_ctx;


/* Key size in bits: either 256 (32 bytes), or 128 (16 bytes) */
/* Nonce size in bits: 64 (8 bytes) */
/* returns 0 on success */
int chacha20_setup_ex (chacha20_ctx *ctx, const void *keydata, const void *noncedata, uint32_t keybits);


/* chacha setup for 256-bit keys */
static inline int chacha20_setup (chacha20_ctx *ctx, const void *keydata, const void *noncedata) {
  return chacha20_setup_ex(ctx, keydata, noncedata, 256);
}


/* chacha setup for 128-bit keys */
static inline int chacha20_setup_16 (chacha20_ctx *ctx, const void *keydata, const void *noncedata) {
  return chacha20_setup_ex(ctx, keydata, noncedata, 128);
}


/* encrypts or decrypts a full message */
/* cypher is symmetric, so `ciphertextdata` and `plaintextdata` can point to the same address */
void chacha20_xcrypt (chacha20_ctx *ctx, void *ciphertextdata, const void *plaintextdata, uint32_t msglen);


#if defined(__cplusplus)
}
#endif

#endif
