/* based on the code from https://github.com/amosnier/sha-2 */
/* modified (streamified) by Ketmar Dark */
/* added HMAC-SHA256 and HKDF-SHA256 implementations */
/* public domain */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sha256pd.h"


struct string_vector {
  const char *input;
  const char *output;
};


static const struct string_vector STRING_VECTORS[] = {
  {
    "",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  },
  {
    "abc",
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
  },
  {
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    "a8ae6e6ee929abea3afcfc5258c8ccd6f85273e0d4626d26c7279f3250f77c8e"
  },
  {
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde",
    "057ee79ece0b9a849552ab8d3c335fe9a5f1c46ef5f1d9b190c295728628299c"
  },
  {
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0",
    "2a6ad82f3620d3ebe9d678c812ae12312699d673240d5be8fac0910a70000d93"
  },
  {
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
  },
  {
    "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
    "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
    "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"
  }
};


static uint8_t data1[] = { 0xbd };
static uint8_t data2[] = { 0xc9, 0x8c, 0x8e, 0x55 };
static uint8_t data7[1000];
static uint8_t data8[1000];
static uint8_t data9[1005];
#define SIZEOF_DATA11 536870912
#define SIZEOF_DATA12 1090519040
#define SIZEOF_DATA13 1610612798

struct vector {
  const uint8_t *input;
  size_t input_len;
  const char *output;
};

static struct vector vectors[] = {
  {
    data1,
    sizeof(data1),
    "68325720aabd7c82f30f554b313d0570c95accbb7dc4b5aae11204c08ffe732b"
  },
  {
    data2,
    sizeof(data2),
    "7abc22c0ae5af26ce93dbb94433a0e0b2e119d014f8e7f65bd56c61ccccd9504"
  },
  {
    data7,
    55,
    "02779466cdec163811d078815c633f21901413081449002f24aa3e80f0b88ef7"
  },
  {
    data7,
    56,
    "d4817aa5497628e7c77e6b606107042bbba3130888c5f47a375e6179be789fbb"
  },
  {
    data7,
    57,
    "65a16cb7861335d5ace3c60718b5052e44660726da4cd13bb745381b235a1785"
  },
  {
    data7,
    64,
    "f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b"
  },
  {
    data7,
    sizeof(data7),
    "541b3e9daa09b20bf85fa273e5cbd3e80185aa4ec298e765db87742b70138a53"
  },
  {
    data8,
    sizeof(data8),
    "c2e686823489ced2017f6059b8b239318b6364f6dcd835d0a519105a1eadd6e4"
  },
  {
    data9,
    sizeof(data9),
    "f4d62ddec0f3dd90ea1380fa16a5ff8dc4c54b21740650f24afc4120903552b0"
  },
  { // 9
    NULL,
    1000000,
    "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
  },
  {
    NULL,
    1000000,
    "d29751f2649b32ff572b5e0a9f541ea660a50f94ff0beedfb0b692b924cc8025"
  },
  {
    NULL,
    SIZEOF_DATA11,
    "15a1868c12cc53951e182344277447cd0979536badcc512ad24c67e9b2d4f3dd"
  },
  {
    NULL,
    SIZEOF_DATA12,
    "461c19a93bd4344f9215f5ec64357090342bc66b15a148317d276e31cbc20b53"
  },
  {
    NULL,
    SIZEOF_DATA13,
    "c23ce8a7895f4b21ec0daf37920ac0a262a220045a03eb2dfed48ef9b05aabea"
  }
};


static void construct_binary_messages (void) {
  memset(data7, 0x00, sizeof(data7));
  memset(data8, 0x41, sizeof(data8));
  memset(data9, 0x55, sizeof(data9));
}


static void init_binary_message (size_t idx) {
  if (idx >= 9 && idx <= 13) {
    const uint8_t filler = (idx == 9 ? 'a' : idx == 10 || idx == 12 ? 0x00u : idx == 11 ? 0x5au : 0x42u);
    vectors[idx].input = malloc(vectors[idx].input_len);
    memset((void *)vectors[idx].input, filler, vectors[idx].input_len);
  }
}


static void deinit_binary_message (size_t idx) {
  if (idx >= 9 && idx <= 13) {
    free((void *)vectors[idx].input);
    vectors[idx].input = NULL;
  }
}


static void hash_to_string (char string[65], const uint8_t hash[32]) {
  for (size_t i = 0; i < 32; i++) string += sprintf(string, "%02x", hash[i]);
}


static int string_test (const char input[], const char output[]) {
  uint8_t hash[32];
  char hash_string[65];
  printf("input length %lu: ", (unsigned long)strlen(input));
  sha256pd_buf(hash, input, strlen(input));
  hash_to_string(hash_string, hash);
  //printf("input: %s\n", input);
  //printf("hash : %s\n", hash_string);
  if (strcmp(output, hash_string)) {
    printf("FAILURE!\n");
    return 1;
  } else {
    printf("PASSED\n");
    return 0;
  }
}


/*
 * Limitation:
 * - The variable input_len will be truncated to its LONG_BIT least
 * significant bits in the print output. This will never be a problem
 * for values that in practice are less than 2^32 - 1. Rationale: ANSI
 * C-compatibility and keeping it simple.
 */
static int test (const uint8_t *input, size_t input_len, const char output[]) {
  uint8_t hash[32];
  char hash_string[65];
  printf("input starts with 0x%02x, length %lu: ", *input, (unsigned long) input_len);
  fflush(stdout);
  sha256pd_buf(hash, input, input_len);
  hash_to_string(hash_string, hash);
  //printf("hash : %s\n", hash_string);
  if (strcmp(output, hash_string)) {
    printf("FAILURE!\n");
    return 1;
  } else {
    printf("PASSED\n");
    return 0;
  }
}


int hmac_test (void) {
  const char *vectors[] = {
    /* HMAC-SHA-256 */
    "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
    "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
    "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
    "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
    "a3b6167473100ee06e0c796c2955552b",
    "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
    "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
  };

  char *messages[] = {
    "Hi There",
    "what do ya want for nothing?",
    NULL,
    NULL,
    "Test With Truncation",
    "Test Using Larger Than Block-Size Key - Hash Key First",
    "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm.",
  };

  uint8_t mac[SHA256PD_HASH_SIZE];
  uint8_t *keys[7];
  size_t keys_len[7] = {20, 4, 20, 25, 20, 131, 131};
  size_t messages2and3_len = 50;

  for (size_t i = 0; i < 7; ++i) {
    keys[i] = malloc(keys_len[i]);
    if (!keys[i]) { fprintf(stderr, "Can't allocate memory\n"); abort(); }
  }

  memset(keys[0], 0x0b, keys_len[0]);
  strcpy((char *)keys[1], "Jefe");
  memset(keys[2], 0xaa, keys_len[2]);
  for (size_t i = 0; i < keys_len[3]; ++i) keys[3][i] = (uint8_t)(i&0xffu)+1;
  memset(keys[4], 0x0c, keys_len[4]);
  memset(keys[5], 0xaa, keys_len[5]);
  memset(keys[6], 0xaa, keys_len[6]);

  messages[2] = malloc(messages2and3_len+1);
  messages[3] = malloc(messages2and3_len+1);
  if (!messages[2] || !messages[3]) { fprintf(stderr, "Can't allocate memory\n"); abort(); }

  messages[2][messages2and3_len] = '\0';
  messages[3][messages2and3_len] = '\0';

  memset(messages[2], 0xdd, messages2and3_len);
  memset(messages[3], 0xcd, messages2and3_len);

  char output[2*SHA256PD_HASH_SIZE+1];

  printf("HMAC-SHA-2 IETF Validation tests\n");
  for (size_t i = 0; i < 7; ++i) {
    size_t mac_256_size = (i != 4 ? SHA256PD_HASH_SIZE : 128/8);
    printf("Test %u: ", (unsigned)i+1);
    sha256pd_hmac_buf(mac, mac_256_size, keys[i], keys_len[i], messages[i], strlen(messages[i]));
    //hmac_check(vectors[i], mac, mac_256_size);
    for (size_t i = 0; i < mac_256_size; ++i) sprintf(output+2*i, "%02x", mac[i]);
    if (strcmp(vectors[i], output) != 0) {
      printf("FAILURE.\n");
      return 1;
    }
    printf("PASSED\n");
  }

  return 0;
}


struct HKDF_test_t {
  const char *name;
  const char *ikm;
  const char *salt;
  const char *info;
  size_t reslen;
  const char *prk;
  const char *okm;
};


static unsigned hexdigit (const char ch) {
  return
    ch >= '0' && ch <= '9' ? ch-'0' :
    ch >= 'A' && ch <= 'F' ? ch-'A'+10 :
    ch >= 'a' && ch <= 'f' ? ch-'a'+10 :
    0;
}


static size_t hexlen (const char *buf) {
  return (buf && buf[0] ? strlen(buf)/2 : 0);
}


static uint8_t *hex2bin (const char *buf) {
  if (!buf || !buf[0]) return NULL;
  uint8_t *res = (uint8_t *)malloc(strlen(buf)/2);
  uint8_t *d = res;
  while (*buf) {
    const unsigned d0 = hexdigit(*buf++);
    const unsigned d1 = hexdigit(*buf++);
    *d++ = (d0<<4)|d1;
  }
  return res;
}


static void hexfree (uint8_t *hbuf) {
  if (hbuf) free(hbuf);
}


static int test_hkdf_one (const struct HKDF_test_t *nfo) {
  uint8_t *ikm = hex2bin(nfo->ikm);
  uint8_t *salt = hex2bin(nfo->salt);
  uint8_t *info = hex2bin(nfo->info);
  uint8_t *prk = hex2bin(nfo->prk);
  uint8_t *okm = hex2bin(nfo->okm);
  if (nfo->reslen != hexlen(nfo->okm)) abort();
  uint8_t *reskey = (uint8_t *)malloc(nfo->reslen);

  sha256pd_hkdf_buf(reskey, nfo->reslen, ikm, hexlen(nfo->ikm), salt, hexlen(nfo->salt), info, hexlen(nfo->info));

  int res = 0;
  if (memcmp(reskey, okm, nfo->reslen) != 0) {
    printf("%s: FAILED!\n", nfo->name);
    res = 1;
  } else {
    printf("%s: PASSED\n", nfo->name);
  }

  hexfree(ikm);
  hexfree(salt);
  hexfree(info);
  hexfree(prk);
  hexfree(okm);
  free(reskey);

  return res;
}


static int test_hkdf (void) {
  printf("HMAC-HKDF Validation tests\n");

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

  if (test_hkdf_one(&vectors[0])) return 1;
  if (test_hkdf_one(&vectors[1])) return 1;
  if (test_hkdf_one(&vectors[2])) return 1;

  printf("All tests passed.\n");

  return 0;
}


int main (void) {
  printf("SHA2-256 Simple Tests\n");
  for (size_t i = 0; i < (sizeof(STRING_VECTORS)/sizeof(struct string_vector)); ++i) {
    const struct string_vector *vector = &STRING_VECTORS[i];
    if (string_test(vector->input, vector->output)) return 1;
  }

  if (hmac_test()) return 1;

  if (test_hkdf()) return 1;

  printf("SHA2-256 Long Tests\n");
  construct_binary_messages();
  for (size_t i = 0; i < (sizeof(vectors)/sizeof(struct vector)); ++i) {
    const struct vector *vector = &vectors[i];
    //if (vector->input_len > 1000000) continue;
    //if (vector->input_len > 536870912) continue;
    init_binary_message(i);
    if (test(vector->input, vector->input_len, vector->output)) return 1;
    deinit_binary_message(i);
  }

  return 0;
}
