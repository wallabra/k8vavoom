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

#include "cmdlib.h"
using namespace VavoomUtils;

#undef clock
#include <time.h>
#ifdef _WIN32
# include <windows.h>
# include <sys/types.h>
typedef int socklen_t;
#else
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <errno.h>
# include <unistd.h>
# define closesocket   close
#endif
#include <sys/select.h>
#include <sys/time.h>


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
  vuint8 pver0; // protocol version
  vuint8 pver1; // protocol version
  // request rate limiter
  double lastReqTime;
  int rateViolationCount;
};

#ifdef _WIN32
class TWinSockHelper {
public:
  ~TWinSockHelper () { WSACleanup(); }
};
#endif


static int acceptSocket = -1; // socket for fielding new connections
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
//  GetSysTime
//
//  return value should not be zero
//
//==========================================================================
static __attribute__((unused)) double GetSysTime () noexcept {
  #if defined(WIN32)
  static uint32_t lastTickCount = 0;
  static double addSeconds = 0;
  static double lastSeconds = 0;
  uint32_t res = (uint32_t)GetTickCount();
  if (lastTickCount > res) {
    addSeconds = lastSeconds;
    lastTickCount = res;
  } else {
    lastSeconds = addSeconds+(double)res/1000.0+1.0;
  }
  return lastSeconds;
  #elif defined(__linux__)
  static bool initialized = false;
  static time_t secbase = 0;
  struct timespec ts;
  if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) abort();
  if (!initialized) {
    initialized = true;
    secbase = ts.tv_sec;
  }
  return (ts.tv_sec-secbase)+ts.tv_nsec/1000000000.0+1.0;
  #else
  static bool initialized = false;
  struct timeval tp;
  struct timezone tzp;
  static int secbase = 0;
  gettimeofday(&tp, &tzp);
  if (!initialized) {
    initialized = true;
    secbase = tp.tv_sec;
  }
  return (tp.tv_sec-secbase)+tp.tv_usec/1000000.0+1.0;
  #endif
}


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
//  AddrToString
//
//==========================================================================
static char *AddrToString (const sockaddr *addr) {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d:%d", (haddr>>24)&0xff, (haddr>>16)&0xff,
    (haddr>>8)&0xff, haddr&0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
}


//==========================================================================
//
//  AddrToStringNoPort
//
//==========================================================================
static char *AddrToStringNoPort (const sockaddr *addr) {
  static char buffer[64];
  int haddr = ntohl(((const sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", (haddr>>24)&0xff, (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff);
  return buffer;
}


//==========================================================================
//
//  AddrCompare
//
//==========================================================================
static int AddrCompare (const sockaddr *addr1, const sockaddr *addr2) {
  if (addr1->sa_family != addr2->sa_family) return -1;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return -1;
  if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return 1;
  return 0;
}


//==========================================================================
//
//  AddrCompareNoPort
//
//==========================================================================
static int AddrCompareNoPort (const sockaddr *addr1, const sockaddr *addr2) {
  if (addr1->sa_family != addr2->sa_family) return -1;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return -1;
  //if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return 1;
  return 0;
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
    if (AddrCompareNoPort(&blocked.addr, clientaddr) == 0) {
      return true;
    }
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
    if (AddrCompareNoPort(&srvBlocked[i].addr, clientaddr) == 0) {
      srvBlocked[i].time = time(0)+60; // block for one minute
      srvBlocked[i].pver0 = 0;
      srvBlocked[i].pver1 = 0;
      return;
    }
  }
  Logf("something at %s is blocked", AddrToStringNoPort(clientaddr));
  TSrvItem &it = srvBlocked.Alloc();
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
    if (fidx < 0 && AddrCompareNoPort(&srvReqested[i].addr, clientaddr) == 0) {
      fidx = i;
    } else if (srvReqested[i].time+10 <= currtm) {
      srvReqested.removeAt(i);
      --i;
    }
  }

  // known?
  if (fidx >= 0) {
    // check rate limit
    const double ctt = GetSysTime();
    TSrvItem *srv = &srvReqested[fidx];
    srv->time = currtm;
    if (ctt-srv->lastReqTime <= 50.0/1000.0) {
      // viloation
      if (++srv->rateViolationCount > 4) {
        // too many violations, block it
        Logf("something at %s is too fast", AddrToStringNoPort(clientaddr));
        BlockIt(clientaddr);
        // remove from the list
        srvReqested.removeAt(fidx);
        return false;
      }
      Logf("something at %s is almost too fast (%d) %g : %g", AddrToStringNoPort(clientaddr), srv->rateViolationCount, srv->lastReqTime, ctt);
    } else if (ctt-srv->lastReqTime > 300.0/1000.0) {
      srv->rateViolationCount = 0;
    }
    srv->lastReqTime = ctt;
  } else {
    // new query
    TSrvItem *srv = &srvReqested.alloc();
    srv->time = currtm;
    srv->addr = *clientaddr;
    srv->lastReqTime = GetSysTime();
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

  // check if there's any packet waiting
  if (recvfrom(acceptSocket, buf, MAX_MSGLEN, MSG_PEEK, nullptr, nullptr) < 0) return;

  // read packet
  sockaddr clientaddr;
  socklen_t addrlen = sizeof(sockaddr);
  int len = (int)recvfrom(acceptSocket, buf, MAX_MSGLEN, 0, &clientaddr, &addrlen);

  if (len < 0) return; // some error

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
            if (!AddrCompare(&srvList[i].addr, &clientaddr)) {
              srvList[i].time = time(0);
              srvList[i].pver0 = buf[1];
              srvList[i].pver1 = buf[2];
              return;
            }
          }
          Logf("server at %s is joined, protocol version is %u", AddrToString(&clientaddr), (unsigned)buf[1]);
          TSrvItem &it = srvList.Alloc();
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
            if (AddrCompare(&srvList[i].addr, &clientaddr) == 0) {
              Logf("server at %s leaves", AddrToString(&srvList[i].addr));
              srvList.RemoveIndex(i);
              break;
            }
          }
          return;
        }
        break;
      case MCREQ_LIST:
        if (len == 1) {
          Logf("query from %s", AddrToString(&clientaddr));
          if (srvList.length() == 0) {
            // answer with the empty packet
            memcpy(buf, "K8VAVOOM", 8);
            buf[8] = MASTER_PROTO_VERSION;
            int bufstpos = 9;
            buf[bufstpos++] = MCREP_LIST;
            buf[bufstpos++] = 3; // seq id: bit 0 set means 'first', bit 1 set means 'last'
            sendto(acceptSocket, buf, bufstpos, 0, &clientaddr, sizeof(sockaddr));
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
              sendto(acceptSocket, buf, mlen, 0, &clientaddr, sizeof(sockaddr));
            }
          }
          return;
        }
        break;
      case MCREQ_LISTEX:
        if (len == 1) {
          Logf("extended query from %s", AddrToString(&clientaddr));
          if (srvList.length() == 0) {
            // answer with the empty packet
            memcpy(buf, "K8VAVOOM", 8);
            buf[8] = MASTER_PROTO_VERSION;
            int bufstpos = 9;
            buf[bufstpos++] = MCREP_LISTEX;
            buf[bufstpos++] = 0x80; // seq id; bit 7 means "last"
            sendto(acceptSocket, buf, bufstpos, 0, &clientaddr, sizeof(sockaddr));
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
              sendto(acceptSocket, buf, mlen, 0, &clientaddr, sizeof(sockaddr));
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
  Logf("k8vavoom master server at port %d.", MASTER_SERVER_PORT);

#ifdef _WIN32
  WSADATA winsockdata;
  //MAKEWORD(2, 2)
  int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);
  if (r) {
    Logf("Winsock initialisation failed.");
    return -1;
  }
  TWinSockHelper Helper;
#endif

  // open socket for listening for requests
  acceptSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (acceptSocket == -1) {
    Logf("Unable to open accept socket");
    return -1;
  }

  // make socket non-blocking
  int trueval = true;
#ifdef _WIN32
  if (ioctlsocket(acceptSocket, FIONBIO, (u_long*)&trueval) == -1)
#else
  if (ioctl(acceptSocket, FIONBIO, (char*)&trueval) == -1)
#endif
  {
    closesocket(acceptSocket);
    Logf("Unable to make socket non-blocking");
    return -1;
  }

  // bind socket to the port
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(MASTER_SERVER_PORT);
  if (bind(acceptSocket, (sockaddr *)&address, sizeof(address)) == -1) {
    closesocket(acceptSocket);
    Logf("Unable to bind socket to a port");
    return -1;
  }

  // main loop
  for (;;) {
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(acceptSocket, &rd);
    int res = select(acceptSocket+1, &rd, nullptr, nullptr, nullptr);
    if (res <= 0) continue;

    // clean up list from old records
    bool dumpServers = false;
    time_t CurTime = time(0);
    for (int i = srvList.length()-1; i >= 0; --i) {
      if (CurTime-srvList[i].time >= 15*60) {
        Logf("server at %s leaves by timeout", AddrToString(&srvList[i].addr));
        srvList.removeAt(i);
        dumpServers = true;
      }
    }

    // clean blocklist
    for (int i = srvBlocked.length()-1; i >= 0; --i) {
      if (srvBlocked[i].time > 0 && srvBlocked[i].time <= CurTime) {
        Logf("lifted block from %s", AddrToStringNoPort(&srvBlocked[i].addr));
        srvBlocked.removeAt(i);
      }
    }

    int scount = srvList.length();
    ReadNet();

    if (dumpServers || srvList.length() != scount) {
      Logf("===== SERVERS =====");
      for (int f = 0; f < srvList.length(); ++f) {
        Logf("%3d: %s (version: %u.%u)", f, AddrToString(&srvList[f].addr), srvList[f].pver0, srvList[f].pver1);
      }
    }
  }

  // close socket
  closesocket(acceptSocket);
  return 0;
}
