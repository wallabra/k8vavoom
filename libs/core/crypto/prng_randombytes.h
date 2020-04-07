/* coded by Ketmar Dark */
/* public domain */
/* get (almost) crypto-secure random bytes */
/* this will try to use the best PRNG available */
#ifndef VV_PRNG_RANDOMBYTES_HEADER
#define VV_PRNG_RANDOMBYTES_HEADER

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


void prng_randombytes (void *p, size_t len);


#ifdef __cplusplus
}
#endif
#endif
