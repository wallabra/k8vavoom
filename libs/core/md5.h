/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */
struct MD5Context {
public:
  enum { DIGEST_SIZE = 16 };

private:
  vuint32 buf[4];
  vuint32 bytes[2];
  vuint32 in[16];

public:
  MD5Context () { Init(); }

  void Init ();
  void Update (const void *buff, unsigned len);
  void Final (vuint8 digest[DIGEST_SIZE]);
};

//void MD5Transform(vuint32 buf[4], vuint32 const in[16]);
