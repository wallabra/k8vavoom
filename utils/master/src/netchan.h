//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#ifndef VAVOOM_NETCHAN_H
#define VAVOOM_NETCHAN_H

#include <stdint.h>
#include <time.h>
// for `sockaddr`
#ifndef _WIN32
# include <sys/socket.h>
#endif


class VNetChanSocket {
public:
  enum { MAX_DGRAM_SIZE = 1400 };

protected:
  int sockfd;

public:
  inline VNetChanSocket () noexcept : sockfd(-1) {}
  inline ~VNetChanSocket () noexcept { close(); }

  inline bool isOpen () const noexcept { return (sockfd >= 0); }
  inline int getFD () const noexcept { return sockfd; }

  bool create () noexcept;
  // bind socket to the port, with INADDR_ANY
  bool bindToPort (uint16_t port) noexcept;

  void close () noexcept;

  bool send (const sockaddr *addr, const void *buf, int len) noexcept;

  // <0: error
  //  0: no data
  // >0: data length
  // `addr` is sender address
  int recv (sockaddr *addr, void *buf, int maxlen=MAX_DGRAM_SIZE) noexcept;

  bool hasData () noexcept;

  static const char *AddrToString (const sockaddr *addr) noexcept;
  static const char *AddrToStringNoPort (const sockaddr *addr) noexcept;
  static bool AddrEqu (const sockaddr *addr1, const sockaddr *addr2) noexcept;
  static bool AddrEquNoPort (const sockaddr *addr1, const sockaddr *addr2) noexcept;

  static bool GetAddrFromName (const char *hostname, sockaddr *addr, uint16_t port) noexcept;

  static double GetTime () noexcept;

  static bool InitialiseSockets () noexcept;

  static void TVMsecs (timeval *dest, int msecs) noexcept;

  static uint32_t GenRandomU32 () noexcept;

  // start with 0
  static uint32_t CRC32C (uint32_t crc32, const void *buf, size_t length) noexcept;

  // // ChaCha20 // //
  struct ChaCha20Ctx {
    uint32_t input[16];
  };

  enum {
    ChaCha20KeySize = 32,
    ChaCha20NonceSize = 4,
    ChaCha20CheckSumSize = 4,
    ChaCha20HeaderSize = ChaCha20KeySize+ChaCha20NonceSize+ChaCha20CheckSumSize,

    // for real setups
    ChaCha20RealNonceSize = 8,
  };

  /* Key size in bits: either 256 (32 bytes), or 128 (16 bytes) */
  /* Nonce size in bits: 64 (8 bytes) */
  /* returns 0 on success */
  static int ChaCha20SetupEx (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata, uint32_t keybits) noexcept;

  /* chacha setup for 256-bit keys */
  static inline int ChaCha20Setup32 (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata) noexcept {
    return ChaCha20SetupEx(ctx, keydata, noncedata, 256);
  }

  /* chacha setup for 128-bit keys */
  static inline int ChaCha20Setup16 (ChaCha20Ctx *ctx, const void *keydata, const void *noncedata) noexcept {
    return ChaCha20SetupEx(ctx, keydata, noncedata, 128);
  }

  /* chacha setup for 128-bit keys and 32-bit nonce */
  static inline int ChaCha20Setup (ChaCha20Ctx *ctx, const uint8_t keydata[ChaCha20KeySize], const uint32_t nonce) noexcept {
    uint8_t noncebuf[8];
    memset(noncebuf, 0, sizeof(noncebuf));
    noncebuf[0] = nonce&0xffu;
    noncebuf[1] = (nonce>>8)&0xffu;
    noncebuf[2] = (nonce>>16)&0xffu;
    noncebuf[3] = (nonce>>24)&0xffu;
    noncebuf[5] = 0x02;
    noncebuf[6] = 0x9a;
    return ChaCha20SetupEx(ctx, keydata, noncebuf, 256);
  }

  /* encrypts or decrypts a full message */
  /* cypher is symmetric, so `ciphertextdata` and `plaintextdata` can point to the same address */
  static void ChaCha20XCrypt (ChaCha20Ctx *ctx, void *ciphertextdata, const void *plaintextdata, uint32_t msglen) noexcept;

  // sha256
  enum {
    SHA256DigestSize = 32,
  };

  typedef uint8_t SHA256Digest[VNetChanSocket::SHA256DigestSize];
  typedef void *SHA256Context;

  static SHA256Context SHA256Init () noexcept;
  static void SHA256Update (SHA256Context ctx, const void *in, size_t inlen) noexcept;
  // this frees context
  static void SHA256Finish (SHA256Context ctx, SHA256Digest hash) noexcept;

  static void SHA256Buffer (SHA256Digest hash, const void *in, size_t inlen) noexcept;

  // generate ChaCha20 encryption key
  static void GenerateKey (uint8_t key[ChaCha20KeySize]) noexcept;

  // WARNING! cannot do it in-place
  // needs 24 extra bytes (key, nonce, crc)
  // returns new length or -1 on error
  static int EncryptInfoPacket (void *destbuf, const void *srcbuf, int srclen, const uint8_t key[ChaCha20KeySize]) noexcept;
  // it can decrypt in-place
  // returns new length or -1 on error
  // also sets key
  static int DecryptInfoPacket (uint8_t key[ChaCha20KeySize], void *destbuf, const void *srcbuf, int srclen) noexcept;
};


#endif
