/*
	Public domain by Andrew M. <liquidsun@gmail.com>

	Ed25519 reference implementation using Ed25519-donna
*/
#ifndef ED25519_H
#define ED25519_H

#include <stdlib.h>
#include <stdint.h>
#include "sha2.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
To generate a private key, simply generate 32 bytes from a secure
cryptographic source:

  ed25519_secret_key sk;
  randombytes(sk, sizeof(ed25519_secret_key));

To generate a public key:

  ed25519_public_key pk;
  ed25519_publickey(sk, pk);

To sign a message:

  ed25519_signature sig;
  ed25519_sign(message, message_len, sk, pk, signature);

To verify a signature:

  int valid = ed25519_sign_open(message, message_len, pk, signature) == 0;

To batch verify signatures:

  const unsigned char *mp[num] = {message1, message2..}
  size_t ml[num] = {message_len1, message_len2..}
  const unsigned char *pkp[num] = {pk1, pk2..}
  const unsigned char *sigp[num] = {signature1, signature2..}
  int valid[num]

  // valid[i] will be set to 1 if the individual signature was valid, 0 otherwise
  int all_valid = ed25519_sign_open_batch(mp, ml, pkp, sigp, num, valid) == 0;

Unlike the http://bench.cr.yp.to/supercop.html version, signatures are not
appended to messages, and there is no need for padding in front of messages.
Additionally, the secret key does not contain a copy of the public key, so it is
32 bytes instead of 64 bytes, and the public key must be provided to the signing
function.

##### Curve25519

Curve25519 public keys can be generated thanks to
[Adam Langley](http://www.imperialviolet.org/2013/05/10/fastercurve25519.html)
leveraging Ed25519's precomputed basepoint scalar multiplication.

  curved25519_key sk, pk;
  randombytes(sk, sizeof(curved25519_key));
  curved25519_scalarmult_basepoint(pk, sk);

Note the name is curved25519, a combination of curve and ed25519, to prevent
name clashes. Performance is slightly faster than short message ed25519
signing due to both using the same code for the scalar multiply.
*/


typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];

typedef unsigned char curved25519_key[32];

void ed25519_publickey(const ed25519_secret_key sk, ed25519_public_key pk);
int ed25519_sign_open(const unsigned char *m, size_t mlen, const ed25519_public_key pk, const ed25519_signature RS);
void ed25519_sign(const unsigned char *m, size_t mlen, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature RS);

int ed25519_sign_open_batch(const unsigned char **m, size_t *mlen, const unsigned char **pk, const unsigned char **RS, size_t num, int *valid);

void ed25519_randombytes(void *out, size_t count);

void curved25519_scalarmult_basepoint(curved25519_key pk, const curved25519_key e);


#if 0
/* reference/slow SHA-512. really, do not use this */
#define SHA512_HASH_BLOCK_SIZE  (128)
typedef struct ed25519_sha512_state_t {
  uint64_t H[8];
  uint64_t T[2];
  uint32_t leftover;
  uint8_t buffer[SHA512_HASH_BLOCK_SIZE];
} ed25519_sha512_state;

typedef struct ed25519_sha512_state_t ed25519_hash_context;
enum { ed25519_sha512_hash_size = 512/8 };
typedef uint8_t ed25519_sha512_hash[ed25519_sha512_hash_size];


void ed25519_hash_init (ed25519_hash_context *S);
void ed25519_hash_update (ed25519_hash_context *S, const void *in, size_t inlen);
void ed25519_hash_final (ed25519_hash_context *S, ed25519_sha512_hash hash);

void ed25519_hash (ed25519_sha512_hash hash, const void *in, size_t inlen);

#else
typedef sha512_ctx  ed25519_hash_context;
typedef uint8_t  ed25519_sha512_hash[SHA512_DIGEST_SIZE];

#define ed25519_hash_init    sha512_init
#define ed25519_hash_update  sha512_update
#define ed25519_hash_final   sha512_final

#define ed25519_hash         sha512_buf

#endif


#if defined(__cplusplus)
}
#endif

#endif // ED25519_H
