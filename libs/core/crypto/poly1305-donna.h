#ifndef POLY1305_DONNA_H
#define POLY1305_DONNA_H

#include <stddef.h>

/*
https://github.com/floodyberry/ed25519-donna

The poly1305_auth function, viewed as a function of the message for a uniform random key, is
designed to meet the standard notion of unforgeability after a single message. After the sender
authenticates one message, an attacker cannot find authenticators for any other messages.

The sender **MUST NOT** use poly1305_auth to authenticate more than one message under the same key.
Authenticators for two messages under the same key should be expected to reveal enough information
to allow forgeries of authenticators on other messages.

## Functions

`poly1305_context` is an opaque structure large enough to support every underlying platform specific
implementation. It should be size_t aligned, which should be handled already with the size_t member
`aligner`.


== void poly1305_init(poly1305_context *ctx, const unsigned char key[32]);

  `key` is the 32 byte key that is **only used for this message and is discarded immediately after**


== void poly1305_update(poly1305_context *ctx, const unsigned char *m, size_t bytes);

  `m` is a pointer to the message fragment to be processed, and
  `bytes` is the length of the message fragment


== void poly1305_finish(poly1305_context *ctx, unsigned char mac[16]);

  `mac` is the buffer which receives the 16 byte authenticator. After calling finish, the underlying
        implementation will zero out `ctx`.


== void poly1305_auth(unsigned char mac[16], const unsigned char *m, size_t bytes, const unsigned char key[32]);

  `mac` is the buffer which receives the 16 byte authenticator,
  `m` is a pointer to the message to be processed,
  `bytes` is the number of bytes in the message, and
  `key` is the 32 byte key that is **only used for this message and is discarded immediately after**.


== int poly1305_verify(const unsigned char mac1[16], const unsigned char mac2[16]);

  `mac1` is compared to `mac2` in constant time and returns `1` if they are equal and `0` if they are not


## Example

### Simple

    unsigned char key[32] = {...}, mac[16];
    unsigned char msg[] = {...};

    poly1305_auth(mac, msg, msglen, key);


# LICENSE

[MIT](http://www.opensource.org/licenses/mit-license.php) or PUBLIC DOMAIN
*/


#ifdef __cplusplus
extern "C" {
#endif

typedef struct poly1305_context {
  size_t aligner;
  unsigned char opaque[136];
} poly1305_context;

void poly1305_init(poly1305_context *ctx, const unsigned char key[32]);
void poly1305_update(poly1305_context *ctx, const unsigned char *m, size_t bytes);
void poly1305_finish(poly1305_context *ctx, unsigned char mac[16]);
void poly1305_auth(unsigned char mac[16], const unsigned char *m, size_t bytes, const unsigned char key[32]);

int poly1305_verify(const unsigned char mac1[16], const unsigned char mac2[16]);

#ifdef __cplusplus
}
#endif

#endif /* POLY1305_DONNA_H */

