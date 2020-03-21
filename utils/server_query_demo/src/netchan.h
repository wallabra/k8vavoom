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

  static bool GetAddrFromName (const char *hostname, sockaddr *addr, uint16_t port) noexcept;
};


class VNetChan {
public:
  enum { MAX_DATA_SIZE = VNetChanSocket::MAX_DGRAM_SIZE-6 };

protected:
  enum { MAX_QUEUE_SIZE = 32 };

protected:
  struct Message {
    uint8_t data[VNetChanSocket::MAX_DGRAM_SIZE];
    int len;
    Message *next;
    uint32_t pid;
    uint32_t seq;
    bool acked;
    bool close;
  };

public:
  VNetChanSocket sock;
  // client address
  sockaddr claddr;

protected:
  uint32_t lastSendSeq;
  uint32_t lastAckSeq;
  int resendCount;
  double lastSendTime;
  double lastRecvTime;
  bool bOpenAcked;
  bool bCloseAcked;
  bool bCloseSent;

  Message *inQueue;
  Message *outQueue;

public:
  VNetChan ();
  ~VNetChan ();

  inline bool isOpen () const noexcept { return sock.isOpen(); }

  inline bool isOpenAcked () const noexcept { return bOpenAcked; }
  inline bool isCloseAcked () const noexcept { return bCloseAcked; }
  inline bool isCloseSent () const noexcept { return bCloseSent; }

  void close ();

  bool requestClose ();

  void tick ();

  bool canSendData () const noexcept;
  bool hasIncomingData () const noexcept;

  bool send (const void *buf, int len);
  int read (void *buf, int maxlen=MAX_DATA_SIZE);

public:
  static bool InitialiseSockets ();
};


#endif
