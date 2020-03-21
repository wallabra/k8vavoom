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
#include <sys/socket.h>


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

  // start with 0
  static uint32_t CRC32C (uint32_t crc32, const void *buf, size_t length) noexcept;

  // // ChaCha20 // //
  struct ChaCha20Ctx {
    uint32_t input[16];
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

  /* encrypts or decrypts a full message */
  /* cypher is symmetric, so `ciphertextdata` and `plaintextdata` can point to the same address */
  static void ChaCha20XCrypt (ChaCha20Ctx *ctx, void *ciphertextdata, const void *plaintextdata, uint32_t msglen) noexcept;
};


#endif
