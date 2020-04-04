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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <stdint.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
#endif

#include "netchan.h"


// ////////////////////////////////////////////////////////////////////////// //
enum {
  MCREQ_LIST   = 3,
  MCREQ_LISTEX = 4,
};

enum {
  MCREP_LIST   = 1,
  MCREP_LISTEX = 2,
};

enum {
  MAX_MSGLEN = VNetChanSocket::MAX_DGRAM_SIZE,

  MASTER_SERVER_PORT   = 26002,
  MASTER_PROTO_VERSION = 1,

  NET_PROTOCOL_VERSION_HI = 7,
  NET_PROTOCOL_VERSION_LO = 12,

  CCREQ_SERVER_INFO = 2,
  CCREP_SERVER_INFO = 13,

  DEFAULT_SERVER_PORT = 26000,

  RCON_PROTO_VERSION = 1u, // 16-bit value
  CCREQ_RCON_COMMAND = 25,
  CCREP_RCON_COMMAND = 26,
};


struct TSrvItem {
  sockaddr addr;
  uint8_t pver0; // protocol version
  uint8_t pver1; // protocol version
};


#define MAX_SERVERS  (64)
static TSrvItem servers[MAX_SERVERS];
static unsigned serverCount = 0;


//==========================================================================
//
//  WriteMasterGameSignature
//
//==========================================================================
static int WriteMasterGameSignature (void *buf) {
  memcpy(buf, "K8VAVOOM", 8);
  ((uint8_t *)buf)[8] = MASTER_PROTO_VERSION;
  return 9;
}


//==========================================================================
//
//  CheckMasterGameSignature
//
//  checks and removed game signature from packet data
//
//==========================================================================
static bool CheckMasterGameSignature (void *data, int &len) {
  if (len < 10) return false;
  uint8_t *buf = (uint8_t *)data;
  if (memcmp(buf, "K8VAVOOM", 8) != 0) return false;
  if (buf[8] != MASTER_PROTO_VERSION) return false;
  len -= 9;
  if (len > 0) memmove(buf, buf+9, len);
  return true;
}


//==========================================================================
//
//  WriteHostGameSignature
//
//==========================================================================
static int WriteHostGameSignature (void *buf) {
  memcpy(buf, "K8VAVOOM", 8);
  return 8;
}


//==========================================================================
//
//  CheckHostGameSignature
//
//==========================================================================
static bool CheckHostGameSignature (void *data, int &len) {
  if (len < 8) return false;
  uint8_t *buf = (uint8_t *)data;
  if (memcmp(buf, "K8VAVOOM", 8) != 0) return false;
  len -= 8;
  if (len > 0) memmove(buf, buf+8, len);
  return true;
}


//==========================================================================
//
//  CheckByte
//
//==========================================================================
static bool CheckByte (void *data, int &len, uint8_t v) {
  if (len < 1) return false;
  uint8_t *buf = (uint8_t *)data;
  if (buf[0] != v) return false;
  len -= 1;
  if (len > 0) memmove(buf, buf+1, len);
  return true;
}


//==========================================================================
//
//  ReadByte
//
//==========================================================================
static int ReadByte (void *data, int &len) {
  if (len < 1) return -1;
  uint8_t *buf = (uint8_t *)data;
  int res = buf[0];
  len -= 1;
  if (len > 0) memmove(buf, buf+1, len);
  return res;
}


//==========================================================================
//
//  ReadVString
//
//==========================================================================
static bool ReadVString (char *dest, void *data, int &len) {
  if (len < 2) return false; // length and trailing zero
  uint8_t *buf = (uint8_t *)data;
  int slen = buf[0];
  if (slen < 0 || slen > 127) return false; // string is too long (the thing that should not be)
  if (buf[slen+1] != 0) return false; // no trailing zero
  memcpy(dest, buf+1, slen+1); // just copy it
  slen += 2;
  len -= slen;
  if (len > 0) memmove(buf, buf+slen, len);
  return true;
}


//==========================================================================
//
//  ReadCString
//
//==========================================================================
static bool ReadCString (char *dest, void *data, int &len) {
  uint8_t *buf = (uint8_t *)data;
  int pos = 0;
  while (pos < len && buf[pos] != 0) ++pos;
  pos += 1; // zero
  if (pos > 127 || pos > len) return false;
  memcpy(dest, buf, pos);
  len -= pos;
  if (len > 0) memmove(buf, buf+pos, len);
  return true;
}


//==========================================================================
//
//  queryMasterServer
//
//==========================================================================
static void queryMasterServer (bool extended) {
  VNetChanSocket sock;
  if (!sock.create()) abort();

  sockaddr masteraddr;
  if (!sock.GetAddrFromName("ketmar.no-ip.org", &masteraddr, MASTER_SERVER_PORT)) {
    sock.close();
    fprintf(stderr, "ERROR: cannot resolve master server host!\n");
    abort();
  }

  uint8_t buf[MAX_MSGLEN];

  // send request
  printf("sending request to master server at %s...\n", sock.AddrToString(&masteraddr));

  const double startTime = VNetChanSocket::GetTime();
  double lastReqTime = 0;
  int nextSeq = 0;

  for (;;) {
    const double ctt = VNetChanSocket::GetTime();
    if (ctt-startTime >= 1500.0/1000.0) {
      sock.close();
      fprintf(stderr, "ERROR: master server timeout!\n");
      abort();
    }

    // (re)send request
    if (lastReqTime == 0 || lastReqTime+200.0/1000.0 >= ctt) {
      lastReqTime = ctt;
      int pos = WriteMasterGameSignature(buf);
      buf[pos++] = (extended ? MCREQ_LISTEX : MCREQ_LIST);
      if (!sock.send(&masteraddr, buf, pos)) {
        sock.close();
        fprintf(stderr, "ERROR: cannot send query to master server!\n");
        abort();
      }
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(sock.getFD(), &rd);
    timeval to;
    VNetChanSocket::TVMsecs(&to, 100);
    int res = select(sock.getFD()+1, &rd, nullptr, nullptr, &to);
    if (res < 0) break;
    if (res == 0) continue;

    int len = 0;
    sockaddr clientaddr;
    for (int trc = 32; trc > 0; --trc) {
      len = sock.recv(&clientaddr, buf, MAX_MSGLEN);
      if (len < 0) {
        printf("error reading data from %s\n", sock.AddrToString(&clientaddr));
        break;
      }
      if (len == 0) continue;
      if (!VNetChanSocket::AddrEqu(&clientaddr, &masteraddr)) { len = 0; continue; }
      break;
    }
    if (!len) continue;

    printf("got packet from %s\n", sock.AddrToString(&clientaddr));

    if (!CheckMasterGameSignature(buf, len)) continue;
    if (len < 2) continue;

    if (!extended && buf[0] == MCREP_LIST) {
      printf("got reply from the master!\n");
      bool last = (buf[1]&0x02u);
      int pos = 2;
      while (pos+2+4+2 <= len) {
        if (serverCount >= MAX_SERVERS) break;
        TSrvItem *sv = &servers[serverCount++];
        sv->pver0 = buf[pos+0];
        sv->pver1 = buf[pos+1];
        memcpy(sv->addr.sa_data+2, &buf[pos+2], 4);
        memcpy(sv->addr.sa_data+0, &buf[pos+6], 2);
        printf("  server #%u: %s (%u.%u)\n", serverCount, sock.AddrToString(&sv->addr), sv->pver0, sv->pver1);
        pos += 8;
      }
      if (last) break;
    } else if (extended && buf[0] == MCREP_LISTEX) {
      uint8_t seq = buf[1]&0x7f;
      if (seq != nextSeq) continue;
      ++nextSeq;
      int pos = 2;
      while (pos+2+4+2 <= len) {
        if (serverCount >= MAX_SERVERS) break;
        TSrvItem *sv = &servers[serverCount++];
        sv->pver0 = buf[pos+0];
        sv->pver1 = buf[pos+1];
        memcpy(sv->addr.sa_data+2, &buf[pos+2], 4);
        memcpy(sv->addr.sa_data+0, &buf[pos+6], 2);
        printf("  server #%u: %s (%u.%u)\n", serverCount, sock.AddrToString(&sv->addr), sv->pver0, sv->pver1);
        pos += 8;
      }
      if (buf[1]&0x80) break; // last packet
    }
  }

  sock.close();
}


//==========================================================================
//
//  mode2str
//
//==========================================================================
static const char *mode2str (int dmmode) {
  switch (dmmode) {
    case 0: return "coop";
    case 1: return "dm";
    case 2: return "altdm";
  }
  return "unknown";
}


//==========================================================================
//
//  queryGameServer
//
//==========================================================================
static bool queryGameServer (const TSrvItem &srv) {
  VNetChanSocket sock;
  if (!sock.create()) return false;

  // send request
  uint8_t edata[MAX_MSGLEN];

  uint8_t buf[MAX_MSGLEN];

  // 10 times, with 100 msec pause is one second
  for (int tries = 10; tries > 0; --tries) {
    printf("*** sending request to game server at %s...\n", sock.AddrToString(&srv.addr));

    int pos = WriteHostGameSignature(buf);
    buf[pos++] = CCREQ_SERVER_INFO;
    buf[pos++] = NET_PROTOCOL_VERSION_HI;
    buf[pos++] = NET_PROTOCOL_VERSION_LO;

    uint8_t origKey[VNetChanSocket::ChaCha20KeySize];
    VNetChanSocket::GenerateKey(origKey);
    printf("  generated key...\n");
    int elen = VNetChanSocket::EncryptInfoPacket(edata, buf, pos, origKey);
    printf("  encrypted %d bytes to %d bytes...\n", pos, elen);

    /* test
    uint8_t xkey[VNetChanSocket::ChaCha20KeySize];
    uint8_t xdata[MAX_MSGLEN];
    int xlen = VNetChanSocket::DecryptInfoPacket(xkey, xdata, edata, elen);
    if (xlen != pos) { fprintf(stderr, "ERROR: packet decryption failed (error)!\n"); abort(); }
    if (memcmp(xdata, buf, xlen) != 0) { fprintf(stderr, "ERROR: packet decryption failed (bad data)!\n"); abort(); }
    */

    if (elen < 0) {
      fprintf(stderr, "ERROR: packet encryption failed!\n");
      abort();
    }

    if (!sock.send(&srv.addr, edata, elen)) {
      sock.close();
      fprintf(stderr, "ERROR: cannot send query to game server!\n");
      return false;
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(sock.getFD(), &rd);
    timeval to;
    VNetChanSocket::TVMsecs(&to, 100);
    int res = select(sock.getFD()+1, &rd, nullptr, nullptr, &to);
    if (res <= 0) continue;

    // parse reply packet(s)
    for (int rpc = 32; rpc > 0; --rpc) {
      sockaddr clientaddr;
      int len = sock.recv(&clientaddr, edata, MAX_MSGLEN);
      if (len < 0) {
        printf("error reading data from %s\n", sock.AddrToString(&clientaddr));
        break;
      }
      if (len == 0) break;

      printf("got packet from %s\n", sock.AddrToString(&clientaddr));

      uint8_t key[VNetChanSocket::ChaCha20KeySize];
      int dlen = VNetChanSocket::DecryptInfoPacket(key, buf, edata, len);
      if (dlen < 0) {
        printf("got invalid packet from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      printf("decrypted packet from %s (from %d bytes to %d bytes)\n", sock.AddrToString(&clientaddr), len, dlen);

      if (memcmp(key, origKey, VNetChanSocket::ChaCha20KeySize) != 0) {
        printf("got packet with wrong key from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      if (!CheckHostGameSignature(buf, dlen)) {
        printf("got packet with wrong signature %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      if (!CheckByte(buf, dlen, CCREP_SERVER_INFO)) {
        printf("got packet with wrong command %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      // 128 is maximum string length
      char srvname[128];
      char mapname[128];
      int protoVerHi, protoVerLo;
      int currPlr, maxPlr, dmmode;
      int extflags;

      if ((protoVerHi = ReadByte(buf, dlen)) < 0) { printf("cannot read protoVerHi\n"); continue; }
      if ((protoVerLo = ReadByte(buf, dlen)) < 0) { printf("cannot read protoVerLo\n"); continue; }
      if ((extflags = ReadByte(buf, dlen)) < 0) { printf("cannot read extflags\n"); continue; }

      if ((currPlr = ReadByte(buf, dlen)) < 0) { printf("cannot read currPlr\n"); continue; }
      if ((maxPlr = ReadByte(buf, dlen)) < 0) { printf("cannot read maxPlr\n"); continue; }
      if ((dmmode = ReadByte(buf, dlen)) < 0) { printf("cannot read dmmode\n"); continue; }

      // 4 bytes of wadlist hash
      if (ReadByte(buf, dlen) < 0) { printf("cannot read hash0\n"); continue; }
      if (ReadByte(buf, dlen) < 0) { printf("cannot read hash1\n"); continue; }
      if (ReadByte(buf, dlen) < 0) { printf("cannot read hash2\n"); continue; }
      if (ReadByte(buf, dlen) < 0) { printf("cannot read hash3\n"); continue; }

      if (!ReadVString(srvname, buf, dlen)) { printf("cannot read srvname\n"); continue; }
      if (!ReadVString(mapname, buf, dlen)) { printf("cannot read mapname\n"); continue; }

      // print info
      printf("  name  : %s\n", srvname);
      printf("  map   : %s\n", mapname);
      printf("  proto : %d.%d\n", protoVerHi, protoVerLo);
      printf("  plrs  : %d/%d\n", currPlr, maxPlr);
      printf("  mode  : %s\n", mode2str(dmmode));
      printf("  passwd: %s\n", (extflags&2 ? "YES" : "no"));

      // print wad list (this is optional)
      if (extflags&1) {
        char wadname[128];
        while (ReadCString(wadname, buf, dlen)) {
          if (!wadname[0]) break; // empty string terminates it
          printf("    <%s>\n", wadname);
        }
      }

      // done
      sock.close();
      return true;
    }
  }

  sock.close();
  return true;
}


//==========================================================================
//
//  sendRConCommand
//
//==========================================================================
static void sendRConCommand (const char *hostname, const char *rconsecret, const char *command) {
  if (!hostname || !hostname[0]) return;
  if (!rconsecret || !rconsecret[0]) return;
  if (!command || !command[0]) return;

  sockaddr srvaddr;
  int port = DEFAULT_SERVER_PORT;
  char *host = (char *)alloca(strlen(hostname)+1);
  strcpy(host, hostname);

  char *colon = strrchr(host, ':');
  if (colon && colon[1]) {
    int n = 0;
    char *s = colon+1;
    while (*s) {
      int ch = *s++;
      if (ch < '0' || ch > '9') { n = -1; break; }
      n = n*10+ch-'0';
      if (n > 65535) {
        fprintf(stderr, "ERROR: invalid port!\n");
        abort();
      }
    }
    if (n == 0) {
      fprintf(stderr, "ERROR: invalid port!\n");
      abort();
    }
    if (n != -1) {
      port = n;
      *colon = 0;
    }
  }

  if (!VNetChanSocket::GetAddrFromName(host, &srvaddr, port)) {
    fprintf(stderr, "ERROR: cannot resolve server host!\n");
    abort();
  }

  VNetChanSocket sock;
  if (!sock.create()) return;

  // send request
  uint8_t edata[MAX_MSGLEN];
  uint8_t buf[MAX_MSGLEN];

  // generate unique command key (it is used as command unique id, so server can detect duplicate packets)
  uint8_t key[VNetChanSocket::ChaCha20KeySize];
  VNetChanSocket::GenerateKey(key);

  // 10 times, with 100 msec pause is one second
  for (int tries = 10; tries > 0; --tries) {
    printf("*** sending request to game server at %s...\n", sock.AddrToString(&srvaddr));

    int pos = WriteHostGameSignature(buf);
    buf[pos++] = CCREQ_RCON_COMMAND;
    buf[pos++] = RCON_PROTO_VERSION&0xff;
    buf[pos++] = (RCON_PROTO_VERSION>>8)&0xff;

    // we'll write the secret later
    int spos = pos;
    memset(buf+spos, 0, VNetChanSocket::SHA256DigestSize);
    pos += VNetChanSocket::SHA256DigestSize;

    // write command
    for (const char *s = command; *s; ++s) {
      if (pos <= MAX_MSGLEN-VNetChanSocket::ChaCha20HeaderSize) buf[pos++] = (uint8_t)s[0];
    }

    if (pos >= MAX_MSGLEN-VNetChanSocket::ChaCha20HeaderSize) {
      sock.close();
      fprintf(stderr, "ERROR: rcon packet too long!\n");
      abort();
    }

    // write secret
    VNetChanSocket::SHA256Context ctx = VNetChanSocket::SHA256Init();
    if (!ctx) abort();
    VNetChanSocket::SHA256Digest dig;
    // hash key
    VNetChanSocket::SHA256Update(ctx, key, VNetChanSocket::ChaCha20KeySize);
    // hash whole packet
    VNetChanSocket::SHA256Update(ctx, buf, pos);
    // hash password
    VNetChanSocket::SHA256Update(ctx, rconsecret, strlen(rconsecret));
    VNetChanSocket::SHA256Finish(ctx, dig);
    // copy to buffer
    memcpy(buf+spos, dig, VNetChanSocket::SHA256DigestSize);

    int elen = VNetChanSocket::EncryptInfoPacket(edata, buf, pos, key);
    printf("  encrypted %d bytes to %d bytes...\n", pos, elen);

    if (elen < 0) {
      fprintf(stderr, "ERROR: packet encryption failed!\n");
      abort();
    }

    if (!sock.send(&srvaddr, edata, elen)) {
      sock.close();
      fprintf(stderr, "ERROR: cannot send query to game server!\n");
      return;
    }

    // wait for the answer
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(sock.getFD(), &rd);
    timeval to;
    VNetChanSocket::TVMsecs(&to, 100);
    int res = select(sock.getFD()+1, &rd, nullptr, nullptr, &to);
    if (res <= 0) continue;

    // parse reply packet(s)
    for (int rpc = 32; rpc > 0; --rpc) {
      sockaddr clientaddr;
      int len = sock.recv(&clientaddr, edata, MAX_MSGLEN);
      if (len < 0) {
        printf("error reading data from %s\n", sock.AddrToString(&clientaddr));
        break;
      }
      if (len == 0) break;

      printf("got packet from %s\n", sock.AddrToString(&clientaddr));

      uint8_t theirkey[VNetChanSocket::ChaCha20KeySize];
      int dlen = VNetChanSocket::DecryptInfoPacket(theirkey, buf, edata, len);
      if (dlen < 0) {
        printf("got invalid packet from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      printf("decrypted packet from %s (from %d bytes to %d bytes)\n", sock.AddrToString(&clientaddr), len, dlen);

      if (memcmp(key, theirkey, VNetChanSocket::ChaCha20KeySize) != 0) {
        printf("got packet with wrong key from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      if (!CheckHostGameSignature(buf, dlen)) {
        printf("got packet with wrong signature from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      if (!CheckByte(buf, dlen, CCREP_RCON_COMMAND)) {
        printf("got packet with wrong command from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      if (!CheckByte(buf, dlen, RCON_PROTO_VERSION&0xff) ||
          !CheckByte(buf, dlen, (RCON_PROTO_VERSION>>8)&0xff))
      {
        printf("got packet with wrong version from %s\n", sock.AddrToString(&clientaddr));
        continue;
      }

      // just a zero-terminated message, nothing more
      char msg[MAX_MSGLEN+1];
      int dpos = 0;
      while (dpos < MAX_MSGLEN) {
        int ch = ReadByte(buf, dlen);
        if (ch <= 0) break;
        msg[dpos++] = ch;
      }
      msg[dpos] = 0;
      printf("server %s replied: <%s>\n", sock.AddrToString(&clientaddr), msg);

      // done
      sock.close();
      return;
    }
  }

  sock.close();
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, const char **argv) {
  if (!VNetChanSocket::InitialiseSockets()) {
    fprintf(stderr, "FATAL: cannot initialise sockets!\n");
    abort();
  }

  if (argc > 1 && strcmp(argv[1], "rcon") == 0) {
    if (argc != 5) {
      fprintf(stderr, "usage: %s rcon host secret cmd\n", argv[0]);
      return -1;
    }
    sendRConCommand(argv[2], argv[3], argv[4]);
    return 0;
  }

  bool ext = true;
  for (int f = 1; f < argc; ++f) if (strcmp(argv[f], "old") == 0) ext = false;

  queryMasterServer(ext);

  for (unsigned f = 0; f < serverCount; ++f) {
    queryGameServer(servers[f]);
    //queryGameServer(servers[f]);
  }

  return 0;
}
