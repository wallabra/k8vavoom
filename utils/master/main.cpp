//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//** k8Vavoom master server
//**
//**************************************************************************

#include "cmdlib.h"
using namespace VavoomUtils;

#undef clock
#include <time.h>
#ifdef _WIN32
# include <windows.h>
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


// ////////////////////////////////////////////////////////////////////////// //
enum {
  MCREQ_JOIN = 1,
  MCREQ_QUIT = 2,
  MCREQ_LIST = 3,
};

enum {
  MCREP_LIST = 1,
};

enum {
  MAX_MSGLEN         = 1024,
  MASTER_SERVER_PORT = 26002,
};


struct TSrvItem {
  sockaddr addr;
  time_t time;
  vuint8 pver; // protocol version
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


// ////////////////////////////////////////////////////////////////////////// //
static char *AddrToString (sockaddr *addr) {
  static char buffer[22];
  int haddr = ntohl(((sockaddr_in *)addr)->sin_addr.s_addr);
  sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff,
    (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
}


// ////////////////////////////////////////////////////////////////////////// //
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
  int len = recvfrom(acceptSocket, buf, MAX_MSGLEN, 0, &clientaddr, &addrlen);

  // check if it is not blocked
  for (int i = 0; i < srvBlocked.length(); ++i) {
    if (AddrCompare(&srvBlocked[i].addr, &clientaddr) == 0) {
      return; // ignore it
    }
  }

  if (len >= 1) {
    switch (buf[0]) {
      case MCREQ_JOIN: // payload: protocol version; can't be 0 or 255
        if (len == 2 && buf[1] != 0 && buf[1] != 255) {
          for (int i = 0; i < srvList.length(); ++i) {
            if (!AddrCompare(&srvList[i].addr, &clientaddr)) {
              srvList[i].time = time(0);
              srvList[i].pver = buf[1];
              return;
            }
          }
          printf("server at %s is joined, protocol version is %u\n", AddrToString(&clientaddr), (unsigned)buf[1]);
          TSrvItem &it = srvList.Alloc();
          it.addr = clientaddr;
          it.time = time(0);
          it.pver = buf[1];
          return;
        }
        break;
      case MCREQ_QUIT:
        if (len == 1) {
          for (int i = 0; i < srvList.length(); ++i) {
            if (AddrCompare(&srvList[i].addr, &clientaddr) == 0) {
              printf("server at %s leaves\n", AddrToString(&srvList[i].addr));
              srvList.RemoveIndex(i);
              break;
            }
          }
          return;
        }
        break;
      case MCREQ_LIST:
        if (len == 1) {
          printf("query from %s\n", AddrToString(&clientaddr));
          int sidx = 0;
          while (sidx < srvList.length()) {
            buf[0] = MCREP_LIST;
            buf[1] = (sidx == 0 ? 1 : 0); // seq id: bit 0 set means 'first', bit 1 set means 'last'
            int mlen = 2;
            while (sidx < srvList.length()) {
              if (mlen+7 > MAX_MSGLEN-1) break;
              buf[mlen+0] = srvList[sidx].pver;
              memcpy(&buf[mlen+1], srvList[sidx].addr.sa_data+2, 4);
              memcpy(&buf[mlen+5], srvList[sidx].addr.sa_data+0, 2);
              mlen += 7;
              ++sidx;
            }
            if (sidx >= srvList.length()) buf[1] |= 0x02; // set "last packet" flag
            sendto(acceptSocket, buf, mlen, 0, &clientaddr, sizeof(sockaddr));
          }
          return;
        }
        break;
    }
  }

  // if it sent invalid command, remove it immediately, and block access for 60 seconds
  for (int i = 0; i < srvList.length(); ++i) {
    if (AddrCompare(&srvList[i].addr, &clientaddr) == 0) {
      srvList.RemoveIndex(i);
      break;
    }
  }
  // append to blocklist
  {
    printf("something at %s is blocked\n", AddrToString(&clientaddr));
    TSrvItem &it = srvBlocked.Alloc();
    it.addr = clientaddr;
    it.time = time(0);
    it.pver = 0;
  }
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, const char **argv) {
  printf("k8Vavoom master server at port %d.\n", MASTER_SERVER_PORT);

#ifdef _WIN32
  WSADATA winsockdata;
  //MAKEWORD(2, 2)
  int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);
  if (r) {
    printf("Winsock initialisation failed.\n");
    return -1;
  }
  TWinSockHelper Helper;
#endif

  // open socket for listening for requests
  acceptSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (acceptSocket == -1) {
    printf("Unable to open accept socket\n");
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
    printf("Unable to make socket non-blocking\n");
    return -1;
  }

  // bind socket to the port
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(MASTER_SERVER_PORT);
  if (bind(acceptSocket, (sockaddr *)&address, sizeof(address)) == -1) {
    closesocket(acceptSocket);
    printf("Unable to bind socket to a port\n");
    return -1;
  }

  // main loop
  for (;;) {
    for (int i = 0; i < 1000; ++i) {
      ReadNet();
#ifdef _WIN32
      Sleep(1);
#else
      //usleep(1);
      static const struct timespec sleepTime = {0, 28500000};
      nanosleep(&sleepTime, nullptr);
#endif
    }

    // clean up list from old records
    time_t CurTime = time(0);
    for (int i = 0; i < srvList.length(); ++i) {
      if (CurTime-srvList[i].time >= 15*60) {
        printf("server at %s leaves by timeout\n", AddrToString(&srvList[i].addr));
        srvList.RemoveIndex(i);
        --i;
      }
    }

    // clean blocklist
    for (int i = 0; i < srvBlocked.length(); ++i) {
      if (CurTime-srvBlocked[i].time >= 60) {
        srvBlocked.RemoveIndex(i);
        --i;
      }
    }
  }

  // close socket
  closesocket(acceptSocket);
  return 0;
}
