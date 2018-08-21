/*
chacha-ref.c version 20080118
D. J. Bernstein
Public domain.
*/
#ifndef CHACHA20_IMPLEMENTATION_INCLUDED
#define CHACHA20_IMPLEMENTATION_INCLUDED

#include <stdint.h>


#define CHACHA20_U32V(v)  ((uint32_t)(v)&0xFFFFFFFFU)
#define CHACHA20_ROTL32(v, n)  (CHACHA20_U32V((v)<<(n))|((v)>>(32-(n))))

#define CHACHA20_ROTATE(v,c) (CHACHA20_ROTL32(v,c))
#define CHACHA20_XOR(v,w) ((v)^(w))
#define CHACHA20_PLUS(v,w) (CHACHA20_U32V((v)+(w)))
#define CHACHA20_PLUSONE(v) (CHACHA20_PLUS((v),1))

#define CHACHA20_QUARTERROUND(a,b,c,d) \
  x[a] = CHACHA20_PLUS(x[a],x[b]); x[d] = CHACHA20_ROTATE(CHACHA20_XOR(x[d],x[a]),16); \
  x[c] = CHACHA20_PLUS(x[c],x[d]); x[b] = CHACHA20_ROTATE(CHACHA20_XOR(x[b],x[c]),12); \
  x[a] = CHACHA20_PLUS(x[a],x[b]); x[d] = CHACHA20_ROTATE(CHACHA20_XOR(x[d],x[a]), 8); \
  x[c] = CHACHA20_PLUS(x[c],x[d]); x[b] = CHACHA20_ROTATE(CHACHA20_XOR(x[b],x[c]), 7);


#define CHACHA20_U32TO32_LITTLE(v) (v)
#define CHACHA20_U32TO8_LITTLE(p, v) (((uint32_t*)(p))[0] = CHACHA20_U32TO32_LITTLE(v))
#define CHACHA20_U8TO32_LITTLE(p) CHACHA20_U32TO32_LITTLE(((uint32_t*)(p))[0])


class ChaCha20 {
private:
  uint32_t input[16];

public:
  ChaCha20 () {}

  /* Key size in bits: either 256 (32 bytes), or 128 (16 bytes) */
  /* Nonce size in bits: 64 (8 bytes) */
  bool setup (const void *keydata, const void *noncedata, uint32_t keybits=256) {
    static const char *sigma = "expand 32-byte k";
    static const char *tau = "expand 16-byte k";

    const char *constants;
    const uint8_t *key = (const uint8_t *)keydata;
    if (keybits != 128 && keybits != 256) return false;

    this->input[4] = CHACHA20_U8TO32_LITTLE(key+0);
    this->input[5] = CHACHA20_U8TO32_LITTLE(key+4);
    this->input[6] = CHACHA20_U8TO32_LITTLE(key+8);
    this->input[7] = CHACHA20_U8TO32_LITTLE(key+12);
    if (keybits == 256) {
      /* recommended */
      key += 16;
      constants = sigma;
    } else {
      /* keybits == 128 */
      constants = tau;
    }
    this->input[8] = CHACHA20_U8TO32_LITTLE(key+0);
    this->input[9] = CHACHA20_U8TO32_LITTLE(key+4);
    this->input[10] = CHACHA20_U8TO32_LITTLE(key+8);
    this->input[11] = CHACHA20_U8TO32_LITTLE(key+12);
    this->input[0] = CHACHA20_U8TO32_LITTLE(constants+0);
    this->input[1] = CHACHA20_U8TO32_LITTLE(constants+4);
    this->input[2] = CHACHA20_U8TO32_LITTLE(constants+8);
    this->input[3] = CHACHA20_U8TO32_LITTLE(constants+12);

    /* nonce setup */
    const uint8_t *iv = (const uint8_t *)noncedata;
    this->input[12] = 0;
    this->input[13] = 0;
    this->input[14] = CHACHA20_U8TO32_LITTLE(iv+0);
    this->input[15] = CHACHA20_U8TO32_LITTLE(iv+4);

    return true;
  }

  // encrypts or decrypts a full message (cypher is symmetric)
  void process (void *ciphertextdata, const void *plaintextdata, uint32_t msglen) {
    uint8_t output[64];
    if (!msglen) return;

    const uint8_t *plaintext = (const uint8_t *)plaintextdata;
    uint8_t *ciphertext = (uint8_t *)ciphertextdata;

    for (;;) {
      salsa20_wordtobyte(output, this->input);
      this->input[12] = CHACHA20_PLUSONE(this->input[12]);
      if (!this->input[12]) {
        this->input[13] = CHACHA20_PLUSONE(this->input[13]);
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


private:
  static void salsa20_wordtobyte (uint8_t output[64], const uint32_t input[16]) {
    uint32_t x[16];
    for (unsigned i = 0;i < 16;++i) x[i] = input[i];
    for (unsigned i = 8;i > 0;i -= 2) {
      CHACHA20_QUARTERROUND( 0, 4, 8, 12)
      CHACHA20_QUARTERROUND( 1, 5, 9, 13)
      CHACHA20_QUARTERROUND( 2, 6, 10, 14)
      CHACHA20_QUARTERROUND( 3, 7, 11, 15)
      CHACHA20_QUARTERROUND( 0, 5, 10, 15)
      CHACHA20_QUARTERROUND( 1, 6, 11, 12)
      CHACHA20_QUARTERROUND( 2, 7, 8, 13)
      CHACHA20_QUARTERROUND( 3, 4, 9, 14)
    }
    for (unsigned i = 0;i < 16;++i) x[i] = CHACHA20_PLUS(x[i], input[i]);
    for (unsigned i = 0;i < 16;++i) CHACHA20_U32TO8_LITTLE(output+4*i, x[i]);
  }
};


#undef CHACHA20_U32V
#undef CHACHA20_ROTL32

#undef CHACHA20_ROTATE
#undef CHACHA20_XOR
#undef CHACHA20_PLUS
#undef CHACHA20_PLUSONE

#undef CHACHA20_QUARTERROUND

#undef CHACHA20_U32TO32_LITTLE
#undef CHACHA20_U32TO8_LITTLE
#undef CHACHA20_U8TO32_LITTLE


#endif
