//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2019-2020 Ketmar Dark
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
//**
//** k8vavoom master server
//**
//**************************************************************************

//#include "cmdlib.h"
//using namespace VavoomUtils;

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
#endif

#include "netchan.h"


// ////////////////////////////////////////////////////////////////////////// //
template<class T> class TArray {
private:
  T *data;
  size_t used;
  size_t size;

public:
  inline TArray () noexcept : data(nullptr), used(0), size(0) {}
  inline ~TArray () noexcept { clear(); }

  inline T &operator [] (int idx) noexcept { if (idx < 0 || (size_t)idx >= used) abort(); return data[(size_t)idx]; }

  inline void clear () noexcept {
    for (size_t f = 0; f < used; ++f) data[f].~T();
    if (data) ::free(data);
    data = nullptr;
    used = size = 0;
  }

  inline int length () const noexcept { return (int)used; }

  inline T &alloc () noexcept {
    if (used >= size) {
      const size_t newsz = size+4096;
      data = (T *)::realloc(data, sizeof(T)*newsz);
      if (!data) abort();
      size = newsz;
      memset((void *)(data+used), 0, sizeof(T)*(size-used));
    }
    memset((void *)(data+used), 0, sizeof(T)); // just in case
    return data[used++];
  }

  inline void removeAt (int idx) noexcept {
    if (idx < 0 || (size_t)idx >= used) return;
    if (used == 0) abort();
    data[(size_t)idx].~T();
    memset((void *)(data+idx), 0, sizeof(T));
    for (size_t f = (size_t)idx+1; f < used; ++f) data[f-1] = data[f];
    --used;
    memset((void *)(data+used), 0, sizeof(T));
  }

  inline T *begin () noexcept { return data; }
  inline const T *begin () const noexcept { return data; }
  inline T *end () noexcept { return (data ? data+used : nullptr); }
  inline const T *end () const noexcept { return (data ? data+used : nullptr); }
};


// ////////////////////////////////////////////////////////////////////////// //
enum {
  MCREQ_JOIN   = 1,
  MCREQ_QUIT   = 2,
  MCREQ_LIST   = 3,
  MCREQ_LISTEX = 4,
};

enum {
  MCREP_LIST   = 1,
  MCREP_LISTEX = 2,
};

enum {
  MAX_MSGLEN = 1400,

  MASTER_SERVER_PORT   = 26002,
  MASTER_PROTO_VERSION = 1,
};


struct TSrvItem {
  sockaddr addr;
  time_t time; // for blocked: unblock time; 0: never
  uint8_t pver0; // protocol version
  uint8_t pver1; // protocol version
  // request rate limiter
  double lastReqTime;
  int rateViolationCount;
};


static VNetChanSocket acceptSocket; // socket for fielding new connections
static TArray<TSrvItem> srvList;
static TArray<TSrvItem> srvBlocked;
static TArray<TSrvItem> srvReqested;

static bool logallowed = true;


#define Logf(...)  do { \
  PrintTime(stdout); printf(__VA_ARGS__); printf("%s", "\n"); \
  if (logallowed) { \
    FILE *fo = fopen("master.log", "a"); \
    if (fo) { PrintTime(fo); fprintf(fo, __VA_ARGS__); fprintf(fo, "%s", "\n"); fclose(fo); } \
  } \
} while (0)


//==========================================================================
//
//  PrintTime
//
//==========================================================================
static void PrintTime (FILE *fo) {
  time_t t = time(nullptr);
  const tm *xtm = localtime(&t);
  fprintf(fo, "%04d/%02d/%02d %02d:%02d:%02d: ", xtm->tm_year+1900, xtm->tm_mon+1, xtm->tm_mday, xtm->tm_hour, xtm->tm_min, xtm->tm_sec);
}


//==========================================================================
//
//  CheckGameSignature
//
//  checks and removed game signature from packet data
//
//==========================================================================
static bool CheckGameSignature (char *buf, int &len) {
  if (len < 10) return false;
  if (memcmp(buf, "K8VAVOOM", 8) != 0) return false;
  if (buf[8] != MASTER_PROTO_VERSION) return false;
  len -= 9;
  if (len > 0) memmove(buf, buf+9, len);
  return true;
}


//==========================================================================
//
//  IsBlocked
//
//==========================================================================
static bool IsBlocked (const sockaddr *clientaddr) {
  for (auto &&blocked : srvBlocked) {
    if (VNetChanSocket::AddrEquNoPort(&blocked.addr, clientaddr)) return true;
  }
  return false;
}


//==========================================================================
//
//  BlockIt
//
//==========================================================================
static void BlockIt (const sockaddr *clientaddr) {
  for (int i = srvBlocked.length()-1; i >= 0; --i) {
    if (VNetChanSocket::AddrEquNoPort(&srvBlocked[i].addr, clientaddr)) {
      srvBlocked[i].time = time(0)+60; // block for one minute
      srvBlocked[i].pver0 = 0;
      srvBlocked[i].pver1 = 0;
      return;
    }
  }
  Logf("something at %s is blocked", VNetChanSocket::AddrToStringNoPort(clientaddr));
  TSrvItem &it = srvBlocked.alloc();
  it.addr = *clientaddr;
  it.time = time(0)+60*3; // block for three minutes
  it.pver0 = 0;
  it.pver1 = 0;
}


//==========================================================================
//
//  CheckRateLimit
//
//  returns `false` if blocked due to rate limit
//
//==========================================================================
static bool CheckRateLimit (const sockaddr *clientaddr) {
  // find record, drop old records
  int fidx = -1;
  time_t currtm = time(0);
  for (int i = 0; i < srvReqested.length(); ++i) {
    if (fidx < 0 && VNetChanSocket::AddrEquNoPort(&srvReqested[i].addr, clientaddr)) {
      fidx = i;
    } else if (srvReqested[i].time+10 <= currtm) {
      srvReqested.removeAt(i);
      --i;
    }
  }

  // known?
  if (fidx >= 0) {
    // check rate limit
    const double ctt = VNetChanSocket::GetTime();
    TSrvItem *srv = &srvReqested[fidx];
    srv->time = currtm;
    if (ctt-srv->lastReqTime <= 50.0/1000.0) {
      // viloation
      if (++srv->rateViolationCount > 4) {
        // too many violations, block it
        Logf("something at %s is too fast", VNetChanSocket::AddrToStringNoPort(clientaddr));
        BlockIt(clientaddr);
        // remove from the list
        srvReqested.removeAt(fidx);
        return false;
      }
      Logf("something at %s is almost too fast (%d) %g : %g", VNetChanSocket::AddrToStringNoPort(clientaddr), srv->rateViolationCount, srv->lastReqTime, ctt);
    } else if (ctt-srv->lastReqTime > 300.0/1000.0) {
      srv->rateViolationCount = 0;
    }
    srv->lastReqTime = ctt;
  } else {
    // new query
    TSrvItem *srv = &srvReqested.alloc();
    srv->time = currtm;
    srv->addr = *clientaddr;
    srv->lastReqTime = VNetChanSocket::GetTime();
    srv->rateViolationCount = 0;
  }

  return true;
}


//==========================================================================
//
//  ReadNet
//
//==========================================================================
static void ReadNet () {
  char buf[MAX_MSGLEN];

  // read packet
  sockaddr clientaddr;
  int len = acceptSocket.recv(&clientaddr, buf, MAX_MSGLEN);
  if (len <= 0) return; // some error

  // check if it is not blocked
  if (IsBlocked(&clientaddr)) return; // ignore it

  if (!CheckGameSignature(buf, len)) {
    BlockIt(&clientaddr);
    return;
  }

  if (!CheckRateLimit(&clientaddr)) return;

  if (len >= 1) {
    switch (buf[0]) {
      case MCREQ_JOIN: // payload: protocol version; can't be 0 or 255
        if (len == 3 && buf[1] != 0 && buf[1] != 255) {
          for (int i = 0; i < srvList.length(); ++i) {
            if (VNetChanSocket::AddrEqu(&srvList[i].addr, &clientaddr)) {
              srvList[i].time = time(0);
              srvList[i].pver0 = buf[1];
              srvList[i].pver1 = buf[2];
              return;
            }
          }
          Logf("server at %s is joined, protocol version is %u", VNetChanSocket::AddrToString(&clientaddr), (unsigned)buf[1]);
          TSrvItem &it = srvList.alloc();
          it.addr = clientaddr;
          it.time = time(0);
          it.pver0 = buf[1];
          it.pver1 = buf[2];
          return;
        }
        break;
      case MCREQ_QUIT:
        if (len == 1) {
          for (int i = 0; i < srvList.length(); ++i) {
            if (VNetChanSocket::AddrEqu(&srvList[i].addr, &clientaddr)) {
              Logf("server at %s leaves", VNetChanSocket::AddrToString(&srvList[i].addr));
              srvList.removeAt(i);
              break;
            }
          }
          return;
        }
        break;
      case MCREQ_LIST:
        if (len == 1) {
          Logf("query from %s", VNetChanSocket::AddrToString(&clientaddr));
          if (srvList.length() == 0) {
            // answer with the empty packet
            memcpy(buf, "K8VAVOOM", 8);
            buf[8] = MASTER_PROTO_VERSION;
            int bufstpos = 9;
            buf[bufstpos++] = MCREP_LIST;
            buf[bufstpos++] = 3; // seq id: bit 0 set means 'first', bit 1 set means 'last'
            acceptSocket.send(&clientaddr, buf, bufstpos);
          } else {
            int sidx = 0;
            while (sidx < srvList.length()) {
              memcpy(buf, "K8VAVOOM", 8);
              buf[8] = MASTER_PROTO_VERSION;
              int bufstpos = 9;
              buf[bufstpos+0] = MCREP_LIST;
              buf[bufstpos+1] = (sidx == 0 ? 1 : 0); // seq id: bit 0 set means 'first', bit 1 set means 'last'
              int mlen = bufstpos+2;
              while (sidx < srvList.length()) {
                if (mlen+8 > MAX_MSGLEN-1) break;
                buf[mlen+0] = srvList[sidx].pver0;
                buf[mlen+1] = srvList[sidx].pver1;
                memcpy(&buf[mlen+2], srvList[sidx].addr.sa_data+2, 4);
                memcpy(&buf[mlen+6], srvList[sidx].addr.sa_data+0, 2);
                mlen += 8;
                ++sidx;
              }
              if (sidx >= srvList.length()) buf[bufstpos+1] |= 0x02; // set "last packet" flag
              acceptSocket.send(&clientaddr, buf, mlen);
            }
          }
          return;
        }
        break;
      case MCREQ_LISTEX:
        if (len == 1) {
          Logf("extended query from %s", VNetChanSocket::AddrToString(&clientaddr));
          if (srvList.length() == 0) {
            // answer with the empty packet
            memcpy(buf, "K8VAVOOM", 8);
            buf[8] = MASTER_PROTO_VERSION;
            int bufstpos = 9;
            buf[bufstpos++] = MCREP_LISTEX;
            buf[bufstpos++] = 0x80; // seq id; bit 7 means "last"
            acceptSocket.send(&clientaddr, buf, bufstpos);
          } else {
            int sidx = 0;
            int seq = 0;
            while (seq <= 0x7f && sidx < srvList.length()) {
              memcpy(buf, "K8VAVOOM", 8);
              buf[8] = MASTER_PROTO_VERSION;
              int bufstpos = 9;
              buf[bufstpos+0] = MCREP_LISTEX;
              buf[bufstpos+1] = seq; // seq id; bit 7 means "last"
              int mlen = bufstpos+2;
              while (sidx < srvList.length()) {
                if (mlen+8 > MAX_MSGLEN-1) break;
                buf[mlen+0] = srvList[sidx].pver0;
                buf[mlen+1] = srvList[sidx].pver1;
                memcpy(&buf[mlen+2], srvList[sidx].addr.sa_data+2, 4);
                memcpy(&buf[mlen+6], srvList[sidx].addr.sa_data+0, 2);
                mlen += 8;
                ++sidx;
              }
              if (seq == 0x7f || sidx >= srvList.length()) buf[bufstpos+1] |= 0x80; // set "last packet" flag
              acceptSocket.send(&clientaddr, buf, mlen);
              ++seq;
            }
          }
          return;
        }
        break;
    }
  }

  // if it sent invalid command, remove it immediately, and block access for 60 seconds
  BlockIt(&clientaddr);
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, const char **argv) {
  VNetChanSocket::InitialiseSockets();

  Logf("k8vavoom master server at port %d.", MASTER_SERVER_PORT);

  // open socket for listening for requests
  if (!acceptSocket.create()) {
    Logf("Unable to open accept socket");
    return -1;
  }

  if (!acceptSocket.bindToPort(MASTER_SERVER_PORT)) {
    Logf("Unable to bind socket to a port");
    return -1;
  }

  // main loop
  for (;;) {
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(acceptSocket.getFD(), &rd);
    int res = select(acceptSocket.getFD()+1, &rd, nullptr, nullptr, nullptr);
    if (res <= 0) continue;

    // clean up list from old records
    bool dumpServers = false;
    time_t CurTime = time(0);
    for (int i = srvList.length()-1; i >= 0; --i) {
      if (CurTime-srvList[i].time >= 15*60) {
        Logf("server at %s leaves by timeout", VNetChanSocket::AddrToString(&srvList[i].addr));
        srvList.removeAt(i);
        dumpServers = true;
      }
    }

    // clean blocklist
    for (int i = srvBlocked.length()-1; i >= 0; --i) {
      if (srvBlocked[i].time > 0 && srvBlocked[i].time <= CurTime) {
        Logf("lifted block from %s", VNetChanSocket::AddrToStringNoPort(&srvBlocked[i].addr));
        srvBlocked.removeAt(i);
      }
    }

    int scount = srvList.length();
    ReadNet();

    if (dumpServers || srvList.length() != scount) {
      Logf("===== SERVERS =====");
      for (int f = 0; f < srvList.length(); ++f) {
        Logf("%3d: %s (version: %u.%u)", f, VNetChanSocket::AddrToString(&srvList[f].addr), srvList[f].pver0, srvList[f].pver1);
      }
    }
  }

  // close socket
  acceptSocket.close();
  return 0;
}
