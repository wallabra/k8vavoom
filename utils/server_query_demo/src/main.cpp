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
  MCREQ_LIST = 3,
};

enum {
  MCREP_LIST = 1,
};

enum {
  MAX_MSGLEN = VNetChanSocket::MAX_DGRAM_SIZE,

  MASTER_SERVER_PORT   = 26002,
  MASTER_PROTO_VERSION = 1,

  NETPACKET_CTL = 0x80,

  NET_PROTOCOL_VERSION_HI = 7,
  NET_PROTOCOL_VERSION_LO = 8,

  CCREQ_SERVER_INFO = 2,
  CCREP_SERVER_INFO = 13,
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
static void queryMasterServer () {
  VNetChanSocket sock;
  if (!sock.create()) abort();

  sockaddr masteraddr;
  if (!sock.GetAddrFromName("ketmar.no-ip.org", &masteraddr, MASTER_SERVER_PORT)) {
    sock.close();
    fprintf(stderr, "ERROR: cannot resolve master server host!\n");
    abort();
  }

  uint8_t buf[MAX_MSGLEN];
  int pos = WriteMasterGameSignature(buf);
  buf[pos++] = MCREQ_LIST;

  // send request
  printf("sending request to master server at %s...\n", sock.AddrToString(&masteraddr));
  if (!sock.send(&masteraddr, buf, pos)) {
    sock.close();
    fprintf(stderr, "ERROR: cannot send query to master server!\n");
    abort();
  }

  // wait for the reply
  for (int tries = 3; tries > 0; --tries) {
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(sock.getFD(), &rd);
    timeval to;
    to.tv_sec = 1;
    to.tv_usec = 0;
    int res = select(sock.getFD()+1, &rd, nullptr, nullptr, &to);
    if (res <= 0) break;

    sockaddr clientaddr;
    int len = sock.recv(&clientaddr, buf, MAX_MSGLEN);
    if (len < 0) {
      printf("error reading data from %s\n", sock.AddrToString(&clientaddr));
      break;
    }
    if (len == 0) continue;

    printf("got packet from %s\n", sock.AddrToString(&clientaddr));

    if (!CheckMasterGameSignature(buf, len)) continue;
    if (len < 2) continue;
    if (buf[0] != MCREP_LIST) continue;

    printf("got reply from the master!\n");
    bool last = (buf[1]&0x02u); // seq id: bit 0 set means 'first', bit 1 set means 'last'
    pos = 2;
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
  uint8_t buf[MAX_MSGLEN];
  buf[0] = NETPACKET_CTL;
  int pos = WriteHostGameSignature(buf+1)+1;
  buf[pos++] = CCREQ_SERVER_INFO;
  buf[pos++] = NET_PROTOCOL_VERSION_HI;
  buf[pos++] = NET_PROTOCOL_VERSION_LO;

  for (int tries = 3; tries > 0; --tries) {
    printf("sending request to game server at %s...\n", sock.AddrToString(&srv.addr));
    if (!sock.send(&srv.addr, buf, pos)) {
      sock.close();
      fprintf(stderr, "ERROR: cannot send query to game server!\n");
      return false;
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(sock.getFD(), &rd);
    timeval to;
    to.tv_sec = 1;
    to.tv_usec = 0;
    int res = select(sock.getFD()+1, &rd, nullptr, nullptr, &to);
    if (res <= 0) continue;

    // parse reply packet
    sockaddr clientaddr;
    int len = sock.recv(&clientaddr, buf, MAX_MSGLEN);
    if (len < 0) {
      printf("error reading data from %s\n", sock.AddrToString(&clientaddr));
      break;
    }

    printf("got packet from %s\n", sock.AddrToString(&clientaddr));

    if (!CheckByte(buf, len, NETPACKET_CTL)) continue;
    if (!CheckHostGameSignature(buf, len)) continue;
    if (!CheckByte(buf, len, CCREP_SERVER_INFO)) continue;

    // 128 is maximum string length
    char srvname[128];
    char mapname[128];
    int protoVerHi, protoVerLo;
    int currPlr, maxPlr, dmmode;

    if (!ReadVString(srvname, buf, len)) continue;
    if (!ReadVString(mapname, buf, len)) continue;
    if ((currPlr = ReadByte(buf, len)) < 0) continue;
    if ((maxPlr = ReadByte(buf, len)) < 0) continue;
    if ((protoVerHi = ReadByte(buf, len)) < 0) continue;
    if ((protoVerLo = ReadByte(buf, len)) < 0) continue;
    if (protoVerHi < 7) continue;

    // is this version 7.8 and up?
    if (protoVerHi > 7 || (protoVerHi == 7 && protoVerLo >= 8)) {
      if ((dmmode = ReadByte(buf, len)) < 0) continue;
    } else {
      dmmode = 2; // reasonable default
    }
    // end of 7.8 extensions

    // 4 bytes of wadlist hash
    if (ReadByte(buf, len) < 0) continue;
    if (ReadByte(buf, len) < 0) continue;
    if (ReadByte(buf, len) < 0) continue;
    if (ReadByte(buf, len) < 0) continue;

    // print info
    printf("  name : %s\n", srvname);
    printf("  map  : %s\n", mapname);
    printf("  proto: %d.%d\n", protoVerHi, protoVerLo);
    printf("  plrs : %d/%d\n", currPlr, maxPlr);
    printf("  mode : %s\n", mode2str(dmmode));

    // print wad list (this is optional)
    char wadname[128];
    while (ReadCString(wadname, buf, len)) {
      if (!wadname[0]) break; // empty string terminates it
      printf("    <%s>\n", wadname);
    }

    // done
    break;
  }

  sock.close();
  return true;
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, const char **argv) {
  if (!VNetChan::InitialiseSockets()) {
    fprintf(stderr, "FATAL: cannot initialise sockets!\n");
    abort();
  }

  queryMasterServer();

  for (unsigned f = 0; f < serverCount; ++f) queryGameServer(servers[f]);

  return 0;
}
