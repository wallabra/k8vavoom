/*
 * HMAC-SHA-224/256/384/512 implementation
 * Last update: 06/15/2005
 * Issue date:  06/15/2005
 *
 * Copyright (C) 2005 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>

#include "hmac_sha2.h"

/* HMAC-SHA-224 functions */

void hmac_sha224_init(hmac_sha224_ctx *ctx, const unsigned char *key,
                      unsigned int key_size)
{
    unsigned int fill;
    unsigned int num;

    const unsigned char *key_used;
    unsigned char key_temp[SHA224_DIGEST_SIZE];
    int i;

    if (key_size == SHA224_BLOCK_SIZE) {
        key_used = key;
        num = SHA224_BLOCK_SIZE;
    } else {
        if (key_size > SHA224_BLOCK_SIZE){
            num = SHA224_DIGEST_SIZE;
            sha224_buf(key_temp, key, key_size);
            key_used = key_temp;
        } else { /* key_size > SHA224_BLOCK_SIZE */
            key_used = key;
            num = key_size;
        }
        fill = SHA224_BLOCK_SIZE - num;

        memset(ctx->block_ipad + num, 0x36, fill);
        memset(ctx->block_opad + num, 0x5c, fill);
    }

    for (i = 0; i < (int) num; i++) {
        ctx->block_ipad[i] = key_used[i] ^ 0x36;
        ctx->block_opad[i] = key_used[i] ^ 0x5c;
    }

    sha224_init(&ctx->ctx_inside);
    sha224_update(&ctx->ctx_inside, ctx->block_ipad, SHA224_BLOCK_SIZE);

    sha224_init(&ctx->ctx_outside);
    sha224_update(&ctx->ctx_outside, ctx->block_opad,
                  SHA224_BLOCK_SIZE);

    /* for hmac_reinit */
    memcpy(&ctx->ctx_inside_reinit, &ctx->ctx_inside,
           sizeof(sha224_ctx));
    memcpy(&ctx->ctx_outside_reinit, &ctx->ctx_outside,
           sizeof(sha224_ctx));
}

void hmac_sha224_reinit(hmac_sha224_ctx *ctx)
{
    memcpy(&ctx->ctx_inside, &ctx->ctx_inside_reinit,
           sizeof(sha224_ctx));
    memcpy(&ctx->ctx_outside, &ctx->ctx_outside_reinit,
           sizeof(sha224_ctx));
}

void hmac_sha224_update(hmac_sha224_ctx *ctx, const unsigned char *message,
                        unsigned int message_len)
{
    sha224_update(&ctx->ctx_inside, message, message_len);
}

void hmac_sha224_final(hmac_sha224_ctx *ctx, unsigned char *mac,
                       unsigned int mac_size)
{
    unsigned char digest_inside[SHA224_DIGEST_SIZE];
    unsigned char mac_temp[SHA224_DIGEST_SIZE];

    sha224_final(&ctx->ctx_inside, digest_inside);
    sha224_update(&ctx->ctx_outside, digest_inside, SHA224_DIGEST_SIZE);
    sha224_final(&ctx->ctx_outside, mac_temp);
    memcpy(mac, mac_temp, mac_size);
}

void hmac_sha224(const unsigned char *key, unsigned int key_size,
          const unsigned char *message, unsigned int message_len,
          unsigned char *mac, unsigned mac_size)
{
    hmac_sha224_ctx ctx;

    hmac_sha224_init(&ctx, key, key_size);
    hmac_sha224_update(&ctx, message, message_len);
    hmac_sha224_final(&ctx, mac, mac_size);
}

/* HMAC-SHA-256 functions */

void hmac_sha256_init(hmac_sha256_ctx *ctx, const unsigned char *key,
                      unsigned int key_size)
{
    unsigned int fill;
    unsigned int num;

    const unsigned char *key_used;
    unsigned char key_temp[SHA256_DIGEST_SIZE];
    int i;

    if (key_size == SHA256_BLOCK_SIZE) {
        key_used = key;
        num = SHA256_BLOCK_SIZE;
    } else {
        if (key_size > SHA256_BLOCK_SIZE){
            num = SHA256_DIGEST_SIZE;
            sha256_buf(key_temp, key, key_size);
            key_used = key_temp;
        } else { /* key_size > SHA256_BLOCK_SIZE */
            key_used = key;
            num = key_size;
        }
        fill = SHA256_BLOCK_SIZE - num;

        memset(ctx->block_ipad + num, 0x36, fill);
        memset(ctx->block_opad + num, 0x5c, fill);
    }

    for (i = 0; i < (int) num; i++) {
        ctx->block_ipad[i] = key_used[i] ^ 0x36;
        ctx->block_opad[i] = key_used[i] ^ 0x5c;
    }

    sha256_init(&ctx->ctx_inside);
    sha256_update(&ctx->ctx_inside, ctx->block_ipad, SHA256_BLOCK_SIZE);

    sha256_init(&ctx->ctx_outside);
    sha256_update(&ctx->ctx_outside, ctx->block_opad,
                  SHA256_BLOCK_SIZE);

    /* for hmac_reinit */
    memcpy(&ctx->ctx_inside_reinit, &ctx->ctx_inside,
           sizeof(sha256_ctx));
    memcpy(&ctx->ctx_outside_reinit, &ctx->ctx_outside,
           sizeof(sha256_ctx));
}

void hmac_sha256_reinit(hmac_sha256_ctx *ctx)
{
    memcpy(&ctx->ctx_inside, &ctx->ctx_inside_reinit,
           sizeof(sha256_ctx));
    memcpy(&ctx->ctx_outside, &ctx->ctx_outside_reinit,
           sizeof(sha256_ctx));
}

void hmac_sha256_update(hmac_sha256_ctx *ctx, const unsigned char *message,
                        unsigned int message_len)
{
    sha256_update(&ctx->ctx_inside, message, message_len);
}

void hmac_sha256_final(hmac_sha256_ctx *ctx, unsigned char *mac,
                       unsigned int mac_size)
{
    unsigned char digest_inside[SHA256_DIGEST_SIZE];
    unsigned char mac_temp[SHA256_DIGEST_SIZE];

    sha256_final(&ctx->ctx_inside, digest_inside);
    sha256_update(&ctx->ctx_outside, digest_inside, SHA256_DIGEST_SIZE);
    sha256_final(&ctx->ctx_outside, mac_temp);
    memcpy(mac, mac_temp, mac_size);
}

void hmac_sha256(const unsigned char *key, unsigned int key_size,
          const unsigned char *message, unsigned int message_len,
          unsigned char *mac, unsigned mac_size)
{
    hmac_sha256_ctx ctx;

    hmac_sha256_init(&ctx, key, key_size);
    hmac_sha256_update(&ctx, message, message_len);
    hmac_sha256_final(&ctx, mac, mac_size);
}

/* HMAC-SHA-384 functions */

void hmac_sha384_init(hmac_sha384_ctx *ctx, const unsigned char *key,
                      unsigned int key_size)
{
    unsigned int fill;
    unsigned int num;

    const unsigned char *key_used;
    unsigned char key_temp[SHA384_DIGEST_SIZE];
    int i;

    if (key_size == SHA384_BLOCK_SIZE) {
        key_used = key;
        num = SHA384_BLOCK_SIZE;
    } else {
        if (key_size > SHA384_BLOCK_SIZE){
            num = SHA384_DIGEST_SIZE;
            sha384_buf(key_temp, key, key_size);
            key_used = key_temp;
        } else { /* key_size > SHA384_BLOCK_SIZE */
            key_used = key;
            num = key_size;
        }
        fill = SHA384_BLOCK_SIZE - num;

        memset(ctx->block_ipad + num, 0x36, fill);
        memset(ctx->block_opad + num, 0x5c, fill);
    }

    for (i = 0; i < (int) num; i++) {
        ctx->block_ipad[i] = key_used[i] ^ 0x36;
        ctx->block_opad[i] = key_used[i] ^ 0x5c;
    }

    sha384_init(&ctx->ctx_inside);
    sha384_update(&ctx->ctx_inside, ctx->block_ipad, SHA384_BLOCK_SIZE);

    sha384_init(&ctx->ctx_outside);
    sha384_update(&ctx->ctx_outside, ctx->block_opad,
                  SHA384_BLOCK_SIZE);

    /* for hmac_reinit */
    memcpy(&ctx->ctx_inside_reinit, &ctx->ctx_inside,
           sizeof(sha384_ctx));
    memcpy(&ctx->ctx_outside_reinit, &ctx->ctx_outside,
           sizeof(sha384_ctx));
}

void hmac_sha384_reinit(hmac_sha384_ctx *ctx)
{
    memcpy(&ctx->ctx_inside, &ctx->ctx_inside_reinit,
           sizeof(sha384_ctx));
    memcpy(&ctx->ctx_outside, &ctx->ctx_outside_reinit,
           sizeof(sha384_ctx));
}

void hmac_sha384_update(hmac_sha384_ctx *ctx, const unsigned char *message,
                        unsigned int message_len)
{
    sha384_update(&ctx->ctx_inside, message, message_len);
}

void hmac_sha384_final(hmac_sha384_ctx *ctx, unsigned char *mac,
                       unsigned int mac_size)
{
    unsigned char digest_inside[SHA384_DIGEST_SIZE];
    unsigned char mac_temp[SHA384_DIGEST_SIZE];

    sha384_final(&ctx->ctx_inside, digest_inside);
    sha384_update(&ctx->ctx_outside, digest_inside, SHA384_DIGEST_SIZE);
    sha384_final(&ctx->ctx_outside, mac_temp);
    memcpy(mac, mac_temp, mac_size);
}

void hmac_sha384(const unsigned char *key, unsigned int key_size,
          const unsigned char *message, unsigned int message_len,
          unsigned char *mac, unsigned mac_size)
{
    hmac_sha384_ctx ctx;

    hmac_sha384_init(&ctx, key, key_size);
    hmac_sha384_update(&ctx, message, message_len);
    hmac_sha384_final(&ctx, mac, mac_size);
}

/* HMAC-SHA-512 functions */

void hmac_sha512_init(hmac_sha512_ctx *ctx, const unsigned char *key,
                      unsigned int key_size)
{
    unsigned int fill;
    unsigned int num;

    const unsigned char *key_used;
    unsigned char key_temp[SHA512_DIGEST_SIZE];
    int i;

    if (key_size == SHA512_BLOCK_SIZE) {
        key_used = key;
        num = SHA512_BLOCK_SIZE;
    } else {
        if (key_size > SHA512_BLOCK_SIZE){
            num = SHA512_DIGEST_SIZE;
            sha512_buf(key_temp, key, key_size);
            key_used = key_temp;
        } else { /* key_size > SHA512_BLOCK_SIZE */
            key_used = key;
            num = key_size;
        }
        fill = SHA512_BLOCK_SIZE - num;

        memset(ctx->block_ipad + num, 0x36, fill);
        memset(ctx->block_opad + num, 0x5c, fill);
    }

    for (i = 0; i < (int) num; i++) {
        ctx->block_ipad[i] = key_used[i] ^ 0x36;
        ctx->block_opad[i] = key_used[i] ^ 0x5c;
    }

    sha512_init(&ctx->ctx_inside);
    sha512_update(&ctx->ctx_inside, ctx->block_ipad, SHA512_BLOCK_SIZE);

    sha512_init(&ctx->ctx_outside);
    sha512_update(&ctx->ctx_outside, ctx->block_opad,
                  SHA512_BLOCK_SIZE);

    /* for hmac_reinit */
    memcpy(&ctx->ctx_inside_reinit, &ctx->ctx_inside,
           sizeof(sha512_ctx));
    memcpy(&ctx->ctx_outside_reinit, &ctx->ctx_outside,
           sizeof(sha512_ctx));
}

void hmac_sha512_reinit(hmac_sha512_ctx *ctx)
{
    memcpy(&ctx->ctx_inside, &ctx->ctx_inside_reinit,
           sizeof(sha512_ctx));
    memcpy(&ctx->ctx_outside, &ctx->ctx_outside_reinit,
           sizeof(sha512_ctx));
}

void hmac_sha512_update(hmac_sha512_ctx *ctx, const unsigned char *message,
                        unsigned int message_len)
{
    sha512_update(&ctx->ctx_inside, message, message_len);
}

void hmac_sha512_final(hmac_sha512_ctx *ctx, unsigned char *mac,
                       unsigned int mac_size)
{
    unsigned char digest_inside[SHA512_DIGEST_SIZE];
    unsigned char mac_temp[SHA512_DIGEST_SIZE];

    sha512_final(&ctx->ctx_inside, digest_inside);
    sha512_update(&ctx->ctx_outside, digest_inside, SHA512_DIGEST_SIZE);
    sha512_final(&ctx->ctx_outside, mac_temp);
    memcpy(mac, mac_temp, mac_size);
}

void hmac_sha512(const unsigned char *key, unsigned int key_size,
          const unsigned char *message, unsigned int message_len,
          unsigned char *mac, unsigned mac_size)
{
    hmac_sha512_ctx ctx;

    hmac_sha512_init(&ctx, key, key_size);
    hmac_sha512_update(&ctx, message, message_len);
    hmac_sha512_final(&ctx, mac, mac_size);
}


// ketmar: i added HKDF

// reskey, reskeylen: resulting key (can be of 255*DIGEST_SIZE bytes)
// inkey, inkeysize: input key
// salt, saltsize: optional salt; can be zero size (and you can pass nullptr for empty salt)
// info, infosize: optional arbitrary application data; can be zero size (and you can pass nullptr for empty info)
#define HKDF_SHA_ANY(shasize_) \
void hkdf_sha##shasize_ (void *reskey, unsigned int reskeylen, const void *inkey, unsigned int inkeysize, \
                         const void *salt, unsigned int saltsize, \
                         const void *info, unsigned int infosize) \
{ \
  if (reskeylen == 0) return; \
 \
  unsigned char tmpsalt[SHA##shasize_##_DIGEST_SIZE]; \
  if (!salt || saltsize == 0) { \
    memset(tmpsalt, 0, SHA##shasize_##_DIGEST_SIZE); \
    salt = tmpsalt; \
    saltsize = SHA##shasize_##_DIGEST_SIZE; \
  } \
 \
  unsigned char prk[SHA##shasize_##_DIGEST_SIZE]; \
  hmac_sha##shasize_(salt, saltsize, inkey, inkeysize, prk, SHA##shasize_##_DIGEST_SIZE); \
 \
  unsigned char tbuf[SHA##shasize_##_DIGEST_SIZE]; \
  memset(tbuf, 0, SHA##shasize_##_DIGEST_SIZE); \
 \
  unsigned int steps = (reskeylen+SHA##shasize_##_DIGEST_SIZE-1)/SHA##shasize_##_DIGEST_SIZE; \
  unsigned char *dest = (unsigned char *)reskey; \
 \
  hmac_sha##shasize_##_ctx hctx; \
  unsigned char currstepnum = 0; \
  while (steps > 0) { \
    hmac_sha##shasize_##_init(&hctx, prk, SHA##shasize_##_DIGEST_SIZE); \
    /* previous T */ \
    if (currstepnum) hmac_sha##shasize_##_update(&hctx, tbuf, SHA##shasize_##_DIGEST_SIZE); \
    /* app-specific info */ \
    if (info && infosize) hmac_sha##shasize_##_update(&hctx, (const unsigned char *)info, infosize); \
    /* step number */ \
    ++currstepnum; \
    hmac_sha##shasize_##_update(&hctx, &currstepnum, 1); \
    /* get new T */ \
    hmac_sha##shasize_##_final(&hctx, tbuf, SHA##shasize_##_DIGEST_SIZE); \
    /* copy T to output key */ \
    const unsigned int cplen = (reskeylen >= SHA##shasize_##_DIGEST_SIZE ? SHA##shasize_##_DIGEST_SIZE : reskeylen); \
    memcpy(dest, tbuf, cplen); \
    dest += cplen; \
    reskeylen -= cplen; \
    --steps; \
  } \
}

HKDF_SHA_ANY(224)
HKDF_SHA_ANY(256)
HKDF_SHA_ANY(384)
HKDF_SHA_ANY(512)


#ifdef TEST_VECTORS

/* IETF Validation tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test(const char *vector, unsigned char *digest,
          unsigned int digest_size)
{
    char output[2 * SHA512_DIGEST_SIZE + 1];
    int i;

    output[2 * digest_size] = '\0';

    for (i = 0; i < (int) digest_size ; i++) {
       sprintf(output + 2*i, "%02x", digest[i]);
    }

    printf("H: %s\n", output);
    if (strcmp(vector, output)) {
        fprintf(stderr, "Test failed.\n");
        exit(1);
    }
}

struct HKDF_test_t {
  const char *name;
  const char *ikm;
  const char *salt;
  const char *info;
  unsigned int reslen;
  const char *prk;
  const char *okm;
};

static unsigned int hexdigit (const char ch) {
  return
    ch >= '0' && ch <= '9' ? ch-'0' :
    ch >= 'A' && ch <= 'F' ? ch-'A'+10 :
    ch >= 'a' && ch <= 'f' ? ch-'a'+10 :
    0;
}

static unsigned int hexlen (const char *buf) {
  return (buf && buf[0] ? strlen(buf)/2 : 0);
}

static unsigned char *hex2bin (const char *buf) {
  if (!buf || !buf[0]) return NULL;
  unsigned char *res = (unsigned char *)malloc(strlen(buf)/2);
  unsigned char *d = res;
  while (*buf) {
    const unsigned int d0 = hexdigit(*buf++);
    const unsigned int d1 = hexdigit(*buf++);
    *d++ = (d0<<4)|d1;
  }
  return res;
}

static void hexfree (unsigned char *hbuf) {
  if (hbuf) free(hbuf);
}

static void dumphex (const char *pfx, const unsigned char *buf, unsigned int buflen) {
  if (pfx && pfx[0]) printf("%s", pfx);
  for (unsigned int f = 0; f < buflen; ++f) printf("%02x", buf[f]);
  printf("\n");
}

static void test_hkdf_one (const struct HKDF_test_t *nfo) {
  unsigned char *ikm = hex2bin(nfo->ikm);
  unsigned char *salt = hex2bin(nfo->salt);
  unsigned char *info = hex2bin(nfo->info);
  unsigned char *prk = hex2bin(nfo->prk);
  unsigned char *okm = hex2bin(nfo->okm);
  if (nfo->reslen != hexlen(nfo->okm)) abort();
  unsigned char *reskey = (unsigned char *)malloc(nfo->reslen);

  hkdf_sha256(reskey, nfo->reslen, ikm, hexlen(nfo->ikm), salt, hexlen(nfo->salt), info, hexlen(nfo->info));

  if (memcmp(reskey, okm, nfo->reslen) != 0) {
    printf("%s: FAILED!\n", nfo->name);
    dumphex("EXPECTED = ", okm, hexlen(nfo->okm));
    dumphex("     GOT = ", reskey, nfo->reslen);
    abort();
  } else {
    printf("%s: OK!\n", nfo->name);
  }

  hexfree(ikm);
  hexfree(salt);
  hexfree(info);
  hexfree(prk);
  hexfree(okm);
  free(reskey);
}


static void test_hkdf (void) {
  printf("HMAC-HKDF Validation tests\n\n");

  const struct HKDF_test_t vectors[] = {
   {
     "SHA-256-EMPTY", /* name */
     "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", /* IKM (22 octets) */
     "", /* salt */
     "", /* info */
     42, /* L */
     "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04", /* PRK (32 octets) */
     "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8", /* OKM (42 octets) */
   },
   {
     "SHA-256-BASIC", /* name */
     "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", /* IKM (22 octets) */
     "000102030405060708090a0b0c", /* salt (13 octets) */
     "f0f1f2f3f4f5f6f7f8f9", /* info (10 octets) */
     42, /* L */
     "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5", /* PRK (32 octets) */
     "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", /* OKM (42 octets) */
   },
   {
     "SHA-256-LONGER",
     "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f", /* IKM (80 octets) */
     "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeaf", /* salt (80 octets) */
     "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", /* info (80 octets) */
     82, /* L */
     "06a6b88c5853361a06104c9ceb35b45cef760014904671014a193f40c15fc244", /* PRK (32 octets) */
     "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87", /* OKM (82 octets) */
   },
 };

  test_hkdf_one(&vectors[0]);
  test_hkdf_one(&vectors[1]);
  test_hkdf_one(&vectors[2]);

  printf("All tests passed.\n");
}


int main(void)
{
  #ifdef SHA2_TEST_VECTORS
  sha2_test();
  #endif


    const char *vectors[] =
    {
        /* HMAC-SHA-224 */
        "896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22",
        "a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44",
        "7fb3cb3588c6c1f6ffa9694d7d6ad2649365b0c1f65d69d1ec8333ea",
        "6c11506874013cac6a2abc1bb382627cec6a90d86efc012de7afec5a",
        "0e2aea68a90c8d37c988bcdb9fca6fa8",
        "95e9a0db962095adaebe9b2d6f0dbce2d499f112f2d2b7273fa6870e",
        "3a854166ac5d9f023f54d517d0b39dbd946770db9c2b95c9f6f565d1",
        /* HMAC-SHA-256 */
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
        "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
        "a3b6167473100ee06e0c796c2955552b",
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
        "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
        /* HMAC-SHA-384 */
        "afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59c"
        "faea9ea9076ede7f4af152e8b2fa9cb6",
        "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e"
        "8e2240ca5e69e2c78b3239ecfab21649",
        "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b"
        "2a5ab39dc13814b94e3ab6e101a34f27",
        "3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e1f573b4e"
        "6801dd23c4a7d679ccf8a386c674cffb",
        "3abf34c3503b2a23a46efc619baef897",
        "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c6"
        "0c2ef6ab4030fe8296248df163f44952",
        "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5"
        "a678cc31e799176d3860e6110c46523e",
        /* HMAC-SHA-512 */
        "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
        "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854",
        "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
        "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",
        "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
        "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",
        "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3db"
        "a91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd",
        "415fad6271580a531d4179bc891d87a6",
        "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
        "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598",
        "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944"
        "b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58"
    };

    static char *messages[] =
    {
        "Hi There",
        "what do ya want for nothing?",
        NULL,
        NULL,
        "Test With Truncation",
        "Test Using Larger Than Block-Size Key - Hash Key First",
        "This is a test using a larger than block-size key "
        "and a larger than block-size data. The key needs"
        " to be hashed before being used by the HMAC algorithm."
    };

    unsigned char mac[SHA512_DIGEST_SIZE];
    unsigned char *keys[7];
    unsigned int keys_len[7] = {20, 4, 20, 25, 20, 131, 131};
    unsigned int messages2and3_len = 50;
    unsigned int mac_224_size, mac_256_size, mac_384_size, mac_512_size;
    int i;

    for (i = 0; i < 7; i++) {
        keys[i] = malloc(keys_len[i]);
        if (keys[i] == NULL) {
            fprintf(stderr, "Can't allocate memory\n");
            return 1;
        }
    }

    memset(keys[0], 0x0b, keys_len[0]);
    strcpy((char *) keys[1], "Jefe");
    memset(keys[2], 0xaa, keys_len[2]);
    for (i = 0; i < (int) keys_len[3]; i++)
        keys[3][i] = (unsigned char) i + 1;
    memset(keys[4], 0x0c, keys_len[4]);
    memset(keys[5], 0xaa, keys_len[5]);
    memset(keys[6], 0xaa, keys_len[6]);

    messages[2] = malloc(messages2and3_len + 1);
    messages[3] = malloc(messages2and3_len + 1);

    if (messages[2] == NULL || messages[3] == NULL) {
        fprintf(stderr, "Can't allocate memory\n");
        return 1;
    }

    messages[2][messages2and3_len] = '\0';
    messages[3][messages2and3_len] = '\0';

    memset(messages[2], 0xdd, messages2and3_len);
    memset(messages[3], 0xcd, messages2and3_len);

    printf("HMAC-SHA-2 IETF Validation tests\n\n");

    for (i = 0; i < 7; i++) {
        if (i != 4) {
            mac_224_size = SHA224_DIGEST_SIZE;
            mac_256_size = SHA256_DIGEST_SIZE;
            mac_384_size = SHA384_DIGEST_SIZE;
            mac_512_size = SHA512_DIGEST_SIZE;
        } else {
            mac_224_size = 128 / 8; mac_256_size = 128 / 8;
            mac_384_size = 128 / 8; mac_512_size = 128 / 8;
        }

        printf("Test %d:\n", i + 1);

        hmac_sha224(keys[i], keys_len[i], (unsigned char *) messages[i],
                    strlen(messages[i]), mac, mac_224_size);
        test(vectors[i], mac, mac_224_size);
        hmac_sha256(keys[i], keys_len[i], (unsigned char *) messages[i],
                    strlen(messages[i]), mac, mac_256_size);
        test(vectors[7 + i], mac, mac_256_size);
        hmac_sha384(keys[i], keys_len[i], (unsigned char *) messages[i],
                    strlen(messages[i]), mac, mac_384_size);
        test(vectors[14 + i], mac, mac_384_size);
        hmac_sha512(keys[i], keys_len[i], (unsigned char *) messages[i],
                    strlen(messages[i]), mac, mac_512_size);
        test(vectors[21 + i], mac, mac_512_size);
    }

    printf("All tests passed.\n");

    test_hkdf();

    return 0;
}

#endif /* TEST_VECTORS */

