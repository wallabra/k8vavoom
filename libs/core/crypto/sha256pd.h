/* based on the code from https://github.com/amosnier/sha-2 */
/* modified (streamified) by Ketmar Dark */
/* added HMAC-SHA256 and HKDF-SHA256 implementations */
/* public domain */
#ifndef SHA256_PD_HEADER
#define SHA256_PD_HEADER

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* uncomment this to use `sha256pd_hkdf_buf()` as PRK derivation in balloon */
/*#define SHA256PD_BALLOON_USE_HKDF*/

/* uncomment this to use ISAAC PRNG as random block selector in balloon */
/*#define SHA256PD_BALLOON_USE_ISAAC*/

/* comment this to disable `sha256pd_randombytes()` */
#define SHA256PD_ENABLE_RANDOMBYTES


/* no serviceable parts beyond this point */

/* sha256 digest size, in bytes */
#define SHA256PD_HASH_SIZE   (32u)
/* sha256 chunk size, in bytes */
#define SHA256PD_CHUNK_SIZE  (64u)


/* this can be used instead of `memset()` to zero sensitive memory */
/* GCC should not optimise it out */
void *sha256pd_memerase (void *p, size_t size);


/* generate random bytes; more-or-less cryptographically secure */
/* use only if you have no other cryptographically secure PRNG */
#ifdef SHA256PD_ENABLE_RANDOMBYTES
void sha256pd_randombytes (void *p, size_t len);
#endif


/* sha256 context */
typedef struct sha256pd_ctx_t {
  uint8_t chunk[SHA256PD_CHUNK_SIZE]; /* 512-bit chunks is what we will operate on */
  size_t chunk_used; /* numbed of bytes used in the current chunk */
  size_t total_len; /* accumulator */
  uint32_t h[8]; /* current hash value */
} sha256pd_ctx;

/* this can be called at any time to reset state */
void sha256pd_init (sha256pd_ctx *state);
/* don't use this for more than 2^32-1 bytes of data */
/* note that no checks are made */
void sha256pd_update (sha256pd_ctx *state, const void *input, size_t len);
/* note that you can continue updating, this fill not destroy the state */
/* also note that this will not reset the state */
void sha256pd_finish (const sha256pd_ctx *state, uint8_t hash[SHA256PD_HASH_SIZE]);

/* for convenience */
void sha256pd_buf (uint8_t hash[SHA256PD_HASH_SIZE], const void *input, size_t len);


/* hmac context */
typedef struct sha256pd_hmac_ctx_t {
  sha256pd_ctx inner_hash;
  sha256pd_ctx outer_hash;
} sha256pd_hmac_ctx;

/* you can store hmac context after initialising, and reuse it to calculate more hmacs for the same key */
void sha256pd_hmac_init (sha256pd_hmac_ctx *ctx, const void *key, size_t keysize);
/* don't use this for more than 2^32-1 bytes of data */
/* note that no checks are made */
void sha256pd_hmac_update (sha256pd_hmac_ctx *ctx, const void *msg, size_t msgsize);
/* note that you can continue updating, this fill not destroy the state */
/* also note that this will not reset the state */
/* note that `macsize` cannot be bigger than `SHA256PD_HASH_SIZE` */
/* (actually, it can, but you will get only `SHA256PD_HASH_SIZE` bytes of hmac */
void sha256pd_hmac_finish (const sha256pd_hmac_ctx *ctx, void *mac, size_t macsize);

/* for convenience */
/* note that `macsize` cannot be bigger than `SHA256PD_HASH_SIZE` */
void sha256pd_hmac_buf (void *mac, size_t macsize, const void *key, size_t keysize, const void *msg, size_t msgsize);


/* out: reskey, reskeylen: resulting key (can be of 255*SHA256PD_HASH_SIZE bytes) */
/* in: inkey, inkeysize: input key */
/* in: salt, saltsize: optional salt; can be zero size (and you can pass nullptr for empty salt) */
/* in: info, infosize: optional arbitrary application data; can be zero size (and you can pass nullptr for empty info) */
/* returns -1 on error (invalid result key size, for example), and 0 on success */
int sha256pd_hkdf_buf (void *reskey, size_t reskeylen,
                       const void *inkey, size_t inkeysize,
                       const void *salt, size_t saltsize,
                       const void *info, size_t infosize);


/* you can set this to your `malloc()` function */
/* it will never be called with zero size */
/* return NULL on allocation error */
extern void *(*sha256pd_malloc_fn) (size_t size);
/* you can set this to your `free()` function */
/* it will never be called with NULL pointer */
extern void (*sha256pd_free_fn) (void *p);


#define SHA256PD_BALLOON_DEFAULT_SCOST  (1024u*1024u)
#define SHA256PD_BALLOON_DEFAULT_TCOST  (16u)

/* balloon-based key derivation, using sha256 as hash function */
/* WARNING: this allocates */
/* out: reskey is array of `SHA256PD_HASH_SIZE` bytes */
/* in: inkey, inkeysize: input key */
/* in: salt, saltsize: optional salt; can be zero size (and you can pass nullptr for empty salt) */
/* returns -1 on error (invalid result key size, for example), and 0 on success */
int sha256pd_balloon (uint8_t reskey[SHA256PD_HASH_SIZE],
                      unsigned scost, /* space cost, main buffer size, in bytes; 0 is default (1MB) */
                      unsigned tcost, /* time cost, number of rounds; 0 is default (16) */
                      const void *inkey, size_t inkeysize,
                      const void *salt, size_t saltsize);

#ifdef __cplusplus
}
#endif
#endif
