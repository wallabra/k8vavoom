/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
// event-based socket i/o
#include "mod_socket.h"

#ifdef WIN32
# include <errno.h>
# include <windows.h>
# define socklen_t  int
# define MSG_NOSIGNAL  (0)
# define SHITSOCKCAST(expr)  (char *)(expr)
# define GetSockError()  WSAGetLastError()
# define SHITSOCKWOULDBLOCK  (10035)
#else
# include <sys/types.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <errno.h>
# include <unistd.h>
# include <netdb.h>
# include <sys/ioctl.h>
# define closesocket ::close
# define SHITSOCKCAST(expr)  (expr)
# define GetSockError()  errno
# define SHITSOCKWOULDBLOCK  EWOULDBLOCK
#endif
#define sockaddr_t sockaddr

#ifdef USE_GNU_TLS
# include <gnutls/gnutls.h>
# include <gnutls/x509.h>
#endif


// ////////////////////////////////////////////////////////////////////////// //
#ifdef WIN32
static bool initSuccess = false;
# define INITED  initSuccess
#else
# define INITED  true
#endif
static mythread_mutex mainLock;
static mythread mainThread;


// ////////////////////////////////////////////////////////////////////////// //
static void sockmodInit ();

class SockModAutoInit {
public:
  SockModAutoInit () { sockmodInit(); }
  //~SockModAutoInit () { fprintf(stderr, "*** DEINIT!\n"); }
};

static SockModAutoInit smautoinit;


// ////////////////////////////////////////////////////////////////////////// //
void (*sockmodPostEventCB) (int code, int sockid, int data, bool wantAck);


// ////////////////////////////////////////////////////////////////////////// //
enum {
  evsock_connected,
  evsock_disconnected,
  evsock_cantconnect,
  evsock_gotdata, // received some data
  evsock_sqempty, // sent queue is empty (generated when all queued data was sent)
  evsock_gotclient, // new connection comes for listening sockets
  evsock_error, // some error occured, socket will be closed after dispatching this message
  evsock_timeout,
};


// WARNING! keep this in sync with "Object_vccrun.vc"!
enum SockIOCTL {
  RecvTimeout, // in msecs
  SendTimeout, // in msecs
  ConnectTimeout, // in msecs
  Type, // tcp or udp
  RecvQLength, // number of received data in queue
  SendQLength, // number of unsent data in queue
  RecvQMax, // default: 1MB
  SendQMax, // default: 1MB
  IsValidId, // is this socket id valid?
  IsAlive, // is this socket alive and operational? errored/closed sockets retains id for some time, but they are not "alive"
  Port,
  Address,
};

enum SockType {
  UDP,
  TCP,
  TLS,
};

struct SocketOptions {
  vint32 RecvTimeout; // in msecs (0: default -- 30 seconds)
  vint32 SendTimeout; // in msecs (0: default -- 30 seconds)
  vint32 ConnectTimeout; // in msecs (0: default -- 30 seconds)
};


// ////////////////////////////////////////////////////////////////////////// //
struct SocketBuf {
  vuint8 *data;
  size_t used;
  size_t size;
  size_t maxsize;

  SocketBuf () : data(nullptr), used(0), size(0), maxsize(1024*1024) {}
  ~SocketBuf () { clear(); }
  SocketBuf (const SocketBuf &) = delete;
  SocketBuf &operator = (const SocketBuf &) = delete;

  inline void initialize () {
    data = nullptr;
    used = size = 0;
    maxsize = 1024*1024;
  }

  inline void clear () {
    if (data) Z_Free(data);
    data = nullptr;
    used = size = 0;
  }

  inline void reset () {
    used = 0;
  }

  bool setMax (int newmax) {
    if (newmax < 1 || newmax > 32*1024*1024) return false; // arbitrary limit
    if (used > (size_t)newmax) return false;
    if (size > (size_t)newmax) {
      size = (size_t)newmax;
      data = (vuint8 *)Z_Realloc(data, size);
    }
    maxsize = (size_t)newmax;
    return true;
  }

  inline bool isEmpty () const {
    return (used == 0);
  }

  inline bool isFull () const {
    return (used == maxsize);
  }

  // either all, or nothing
  bool put (const void *buf, size_t len) {
    if (!len) return true;
    if (!buf) return false;
    if (maxsize-used < len) return false;
    if (used+len > size) {
      size_t newsz = ((used+len)|0xfff)+1;
      if (newsz > maxsize) newsz = maxsize;
      data = (vuint8 *)Z_Realloc(data, newsz);
      size = newsz;
    }
    check(size <= maxsize);
    check(used+len <= size);
    memcpy(data+used, buf, len);
    used += len;
    return true;
  }

  // either all, or nothing
  bool get (void *buf, size_t len) {
    if (!len) return true;
    if (!buf) return false;
    if (len > used) return false;
    memcpy(buf, data, len);
    if (len < used) {
      memmove(data, data+len, used-len);
      used -= len;
    } else {
      used = 0;
    }
    return true;
  }

  // either all, or nothing
  bool drop (size_t len) {
    if (!len) return true;
    if (len > used) return false;
    if (len < used) {
      memmove(data, data+len, used-len);
      used -= len;
    } else {
      used = 0;
    }
    return true;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct SocketObj {
  enum {
    ST_DEAD,
    ST_CLOSE_REQUEST,
    ST_ABORT_REQUEST, // immediate close
    ST_ERROR,
    ST_CONNECTING,
    ST_NORMAL,
  };

  int id; // socket id; set this to 0 to mark as "completely dead" (will be removed in worker thread)
  int fd; // socket fd
  int state; // ST_XXX
  int toRecv; // receive timeout
  int toSend; // send timeout, msecs
  int toConnect; // connect timeout
  SocketBuf rbuf; // recv buffer
  SocketBuf sbuf; // send buffer
  bool isUDP;
  //bool sendSQEmpty; // should we send "sbuf is empty" event if it is empty?
  double timeLastRecv;
  double timeLastSend;

#ifdef USE_GNU_TLS
  bool tls;
  bool handshakeComplete;
  gnutls_certificate_credentials_t xcred;
  gnutls_session_t session;
#endif

  inline bool isAlive () const { return (state >= ST_CONNECTING); }

  bool initialize () {
    if (fd < 0) return false;
#ifdef USE_GNU_TLS
    if (tls) {
      // x509 stuff
      gnutls_certificate_allocate_credentials(&xcred);

      // sets the trusted certificate authority file (no need for us, as we aren't checking any certificate)
      //gnutls_certificate_set_x509_trust_file(xcred, CAFILE, GNUTLS_X509_FMT_PEM);

      // initialize TLS session
      gnutls_init(&session, GNUTLS_CLIENT);

      // use default priorities
      const char *err;
      auto ret = gnutls_priority_set_direct(session, "PERFORMANCE", &err);
      if (ret < 0) return false;

      // put the x509 credentials to the current session
      gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);

      gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t)(uintptr_t)fd);
    }
#endif
    return true;
  }

  void close (bool forced) {
    if (fd >= 0) {
#ifdef USE_GNU_TLS
      if (forced) {
        setZeroLinger();
      } else {
#ifndef WIN32
        if (!tls) shutdown(fd, SHUT_RDWR);
#endif
      }
      if (tls) {
        if (!forced) gnutls_bye(session, GNUTLS_SHUT_RDWR);
        gnutls_deinit(session);
        gnutls_certificate_free_credentials(xcred);
        tls = false;
      }
#endif
      closesocket(fd);
      fd = -1;
    }
  }

  bool needHandshake () const {
    if (fd < 0) return false;
#ifdef USE_GNU_TLS
    return (tls && !handshakeComplete);
#else
    return false;
#endif
  }

  // <0: error; 0: try again; 1: ok
  int handshake () {
    if (fd < 0) return -1;
#ifdef USE_GNU_TLS
    if (handshakeComplete) return 1;
    if (tls) {
      auto ret = gnutls_handshake(session);
      if (ret < 0) {
        //fprintf(stderr, "GNUTLS: handshake failed(%d): %s\n", gnutls_error_is_fatal(ret), gnutls_strerror(ret));
        if (gnutls_error_is_fatal(ret)) return -1;
        return 0;
      }
    }
    handshakeComplete = true;
#endif
    return 1;
  }

  void setZeroLinger () {
    if (fd < 0) return;
    linger l;
    l.l_onoff = 1;
    l.l_linger = 0;
#ifdef WIN32
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&l, sizeof(l));
#else
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&l, sizeof(l));
#endif
  }

  int send () {
    if (fd < 0) return -1;
#ifdef USE_GNU_TLS
    if (tls) {
      if (!handshakeComplete) return -1;
      return (int)gnutls_record_send(session, SHITSOCKCAST(sbuf.data), sbuf.used);
    }
    else
#endif
    return (int)::send(fd, SHITSOCKCAST(sbuf.data), sbuf.used, MSG_NOSIGNAL);
  }

  int recv (void *readbuf, int toread) {
    if (fd < 0) return -1;
    if (toread < 1) return -1;
#ifdef USE_GNU_TLS
    if (tls) {
      if (!handshakeComplete) return -1;
      return (int)gnutls_record_recv(session, SHITSOCKCAST(readbuf), toread);
    }
    else
#endif
    return (int)::recv(fd, SHITSOCKCAST(readbuf), toread, 0);
  }
};


static int nextSockId = 1;
static SocketObj *socklist = nullptr;
static int sockused = 0;
static int socksize = 0;


// ////////////////////////////////////////////////////////////////////////// //
static SocketObj *allocSocket () {
  if (nextSockId <= 0) ++nextSockId;
  if (sockused == socksize) {
    if (socksize >= 512) return nullptr; // too many open sockets
    //int oldsz = socksize;
    socksize += 256;
    socklist = (SocketObj *)Z_Realloc(socklist, socksize*sizeof(SocketObj));
    //memset((void *)(socklist+oldsz), 0, (socksize-oldsz)*sizeof(SocketObj));
  }
  check(sockused < socksize);
  SocketObj *so = &socklist[sockused++];
  memset((void *)so, 0, sizeof(SocketObj));
  so->id = nextSockId++;
  so->fd = -1;
  // default timeouts
  so->toRecv = 30*1000;
  so->toSend = 30*1000;
  so->toConnect = 30*1000;
  so->rbuf.initialize();
  so->sbuf.initialize();
  return so;
}


static SocketObj *findSocket (int sockid) {
  if (sockid <= 0) return nullptr;
  for (int f = 0; f < sockused; ++f) if (socklist[f].id == sockid) return &socklist[f];
  return nullptr;
}




static void deallocSocket (int idx) {
  if (idx < 0 || idx >= sockused) return;
  SocketObj *so = &socklist[idx];
  so->close(true); // forced
  so->rbuf.clear();
  so->sbuf.clear();
  for (int c = idx+1; c < sockused; ++c) {
    memcpy((void *)(&socklist[c-1]), (void *)(&socklist[c]), sizeof(SocketObj));
  }
  --sockused;
  //fprintf(stderr, "deallocSocket: idx=%d; sockused=%d\n", idx, sockused);
}


// ////////////////////////////////////////////////////////////////////////// //
static void closeSO (SocketObj *so, bool immed) {
  if (!so) return;
  so->close(immed);
}


void sockmodAckEvent (int code, int sockid, int data, bool eaten, bool cancelled) {
  if (eaten || cancelled) {
    if (code != evsock_disconnected && code != evsock_error && code != evsock_cantconnect && code != evsock_timeout) return;
  }
  MyThreadLocker lock(&mainLock);
  SocketObj *so = findSocket(sockid);
  if (!so) return;
  //closeSO(so);
  if (so->isAlive()) {
    so->state = SocketObj::ST_ABORT_REQUEST;
  } else {
    //if (so->id != 0) fprintf(stderr, "socked #%d marked for removal\n", so->id);
    so->id = 0; // mark for removal
  }
}


// ////////////////////////////////////////////////////////////////////////// //
//TODO: IPv6
static int SocketConnect (SockType type, const VStr &host, int port, SocketOptions &opts) {
  if (!INITED) return 0;

#ifndef USE_GNU_TLS
  if (type == SockType::TLS) return 0;
#endif

  if (host.isEmpty()) return 0;
  if (port < 1 || port > 65535) return 0;

  if (opts.RecvTimeout == 0) opts.RecvTimeout = 30*1000;
  if (opts.SendTimeout == 0) opts.SendTimeout = 30*1000;
  if (opts.ConnectTimeout == 0) opts.ConnectTimeout = 30*1000;

  sockaddr_t addr;

  // resolve address
  hostent *hostentry = gethostbyname(*host);
  if (!hostentry) return 0;

  addr.sa_family = AF_INET;
  ((sockaddr_in *)&addr)->sin_port = htons(port);
  ((sockaddr_in *)&addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

  // create socket fd
  int sfd = socket(PF_INET, (type == SockType::UDP ? SOCK_DGRAM : SOCK_STREAM), (type == SockType::UDP ? IPPROTO_UDP : IPPROTO_TCP));
  if (sfd == -1) return 0;

  // switch to non-blocking mode
#ifdef WIN32
  DWORD trueval = 1;
  if (ioctlsocket(sfd, FIONBIO, &trueval) == -1)
#else
  u_long trueval = 1;
  if (ioctl(sfd, FIONBIO, (char *)&trueval) == -1)
#endif
  {
    closesocket(sfd);
    return 0;
  }

  // initiate connection; do it before allocating internal socket object, so
  // if connection fails immediately, we can save one sockid
  bool connected = false;
  if (connect(sfd, &addr, (socklen_t)sizeof(addr)) != 0) {
    int skerr = GetSockError();
    //fprintf(stderr, "skerr=%d\n", skerr);
    if (skerr != EINPROGRESS && skerr != SHITSOCKWOULDBLOCK) {
      closesocket(sfd);
      return 0;
    }
  } else {
    // udp sockets (and rarely tcp sockets) can do it in no time
    connected = true;
  }

  int resid;
  SocketObj *so;
  {
    MyThreadLocker lock(&mainLock);
    // allocate internal socket object
    so = allocSocket();
    if (!so) {
      closesocket(sfd);
      return 0;
    }

    so->fd = sfd;
    so->isUDP = (type == SockType::UDP);
#ifdef USE_GNU_TLS
    so->tls = (type == SockType::TLS);
#endif
    so->state = (connected ? SocketObj::ST_NORMAL : SocketObj::ST_CONNECTING);
    so->toRecv = opts.RecvTimeout;
    so->toSend = opts.SendTimeout;
    so->toConnect = opts.ConnectTimeout;

    so->timeLastRecv = so->timeLastSend = Sys_Time();

    resid = so->id;
  }

  if (!so->initialize()) {
    MyThreadLocker lock(&mainLock);
    deallocSocket(so->id);
    return 0;
  }

  if (connected) {
    if (so->needHandshake()) {
      int res = so->handshake();
      if (res < 0) {
        MyThreadLocker lock(&mainLock);
        deallocSocket(so->id);
        return 0;
      }
      if (res == 0) {
        // try again
        so->state = SocketObj::ST_CONNECTING;
      }
    }
    // send connected event
    sockmodPostEventCB(evsock_connected, resid, 0, true);
  }

  return resid;
}


// create socket, initiate connection
// if some timeout is zero, default timeout will be returned in opts
// returns 0 on error, or positive socket id
//native static final int SocketConnectUDP (string host, int port, optional ref SocketOptions opts);
IMPLEMENT_FUNCTION(VObject, SocketConnectUDP) {
  P_GET_PTR_OPT_NOSP(SocketOptions, opts);
  P_GET_INT(port);
  P_GET_STR(host);
  RET_INT(SocketConnect(SockType::UDP, host, port, *opts));
}

//native static final int SocketConnectTCP (string host, int port, optional ref SocketOptions opts);
IMPLEMENT_FUNCTION(VObject, SocketConnectTCP) {
  P_GET_PTR_OPT_NOSP(SocketOptions, opts);
  P_GET_INT(port);
  P_GET_STR(host);
  RET_INT(SocketConnect(SockType::TCP, host, port, *opts));
}

//native static final int SocketConnectTLS (string host, int port, optional ref SocketOptions opts);
IMPLEMENT_FUNCTION(VObject, SocketConnectTLS) {
  P_GET_PTR_OPT_NOSP(SocketOptions, opts);
  P_GET_INT(port);
  P_GET_STR(host);
  RET_INT(SocketConnect(SockType::TLS, host, port, *opts));
}


// returns `false` if sockid is invalid or if socket is already errored/disconnected
// note that sockid will be destroyed after dispatching `ev_socket` event
// immediate disconnect means "don't do shutdown, use SO_LIGER 0"
//native static final bool SocketDisconnect (int sockid, optional bool immediate);
IMPLEMENT_FUNCTION(VObject, SocketDisconnect) {
  P_GET_BOOL_OPT(immed, false);
  //(void)immed;
  P_GET_INT(sockid);
  //int sid = 0;
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_BOOL(false); return; }
    if (so->fd < 0) { RET_BOOL(false); return; }
    if (!so->isAlive() && so->state != SocketObj::ST_CONNECTING) { RET_BOOL(false); return; }
    so->state = (immed ? SocketObj::ST_CLOSE_REQUEST : SocketObj::ST_ABORT_REQUEST);
    //sid = so->id; // send "disconnected" event
  }
  //if (sid) sockmodPostEventCB(evsock_disconnected, sid, 0, true);
  RET_BOOL(true);
}


// for most queries, returns -1 for invalid sockid
//native static final int SocketGetIOCTL (int sockid, SockIOCTL opcode);
IMPLEMENT_FUNCTION(VObject, SocketGetIOCTL) {
  P_GET_INT(opcode);
  P_GET_INT(sockid);
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) {
      if (opcode == IsValidId || opcode == IsAlive) RET_INT(0); else RET_INT(-1);
      return;
    }
    if (opcode == IsValidId) { RET_INT(1); return; }
    if (opcode == IsAlive) { RET_INT((so->fd >= 0 && so->isAlive() ? 1 : 0)); return; }
    switch (opcode) {
      case RecvTimeout: RET_INT(so->toRecv); return;
      case SendTimeout: RET_INT(so->toSend); return;
      case ConnectTimeout: RET_INT(so->toConnect); return;
      case Type: RET_INT(so->isUDP ? SockType::UDP : SockType::UDP); return;
      case RecvQLength: RET_INT((int)so->rbuf.used); return;
      case SendQLength: RET_INT((int)so->sbuf.used); return;
      case RecvQMax: RET_INT((int)so->rbuf.maxsize); return;
      case SendQMax: RET_INT((int)so->sbuf.maxsize); return;
      case Port:
      case Address:
        if (so->fd >= 0) {
          socklen_t addrlen = sizeof(sockaddr_t);
          sockaddr_t addr;
          memset(&addr, 0, sizeof(sockaddr_t));
          if (getsockname(so->fd, (sockaddr *)&addr, &addrlen) != 0) { RET_INT(-1); return; }
          const sockaddr_in *sin = (const sockaddr_in *)&addr;
          if (opcode == Port) {
            RET_INT(sin->sin_port);
          } else {
            RET_INT(sin->sin_addr.s_addr);
          }
        } else {
          RET_INT(0);
        }
        return;
      default:
        RET_INT(-1);
        return;
    }
  }
}

//native static final bool SocketSetIOCTL (int sockid, SockIOCTL opcode, optional int arg);
IMPLEMENT_FUNCTION(VObject, SocketSetIOCTL) {
  P_GET_INT_OPT(arg, 0);
  P_GET_INT(opcode);
  P_GET_INT(sockid);
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_BOOL(false); return; }
    if (so->fd < 0 || !so->isAlive()) { RET_BOOL(false); return; }
    switch (opcode) {
      case RecvTimeout: // and reset timer
        so->timeLastRecv = Sys_Time();
        so->toRecv = (arg < 0 ? -1 : arg == 0 ? 30*1000 : arg);
        return;
      case SendTimeout: // and reset timer
        so->timeLastSend = Sys_Time();
        so->toSend = (arg < 0 ? -1 : arg == 0 ? 30*1000 : arg);
        return;
      case ConnectTimeout: // and reset timer
        so->timeLastSend = Sys_Time();
        so->toConnect = (arg < 0 ? -1 : arg == 0 ? 30*1000 : arg);
        return;
      case RecvQMax: RET_BOOL(so->rbuf.setMax(arg)); return;
      case SendQMax: RET_BOOL(so->sbuf.setMax(arg)); return;
      default:
        RET_BOOL(false);
        return;
    }
  }
}


// on error, socket will be automatically closed
// note that sockid will be destroyed after dispatching `ev_socket` event
//native static final bool SocketSendStr (int sockid, string data);
IMPLEMENT_FUNCTION(VObject, SocketSendStr) {
  P_GET_STR(data);
  P_GET_INT(sockid);
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_BOOL(false); return; }
    if (so->fd < 0 || !so->isAlive()) { RET_BOOL(false); return; }
    if (data.length() == 0) { RET_BOOL(true); return; } // don't send zero datagrams (we can't do it anyway)
    RET_BOOL(so->sbuf.put(*data, (size_t)data.length()));
  }
}

//native static final bool SocketSendBuf (int sockid, const ref array!ubyte data, optional int ofs, optional int len);
IMPLEMENT_FUNCTION(VObject, SocketSendBuf) {
  P_GET_INT_OPT(len, -1);
  P_GET_INT_OPT(ofs, 0);
  P_GET_PTR(TArray<vuint8>, data);
  P_GET_INT(sockid);
  if (specified_len && len < 0) { RET_BOOL(false); return; }
  if (ofs < 0) {
    // from back
    ofs = -ofs;
    if (ofs < 0) { RET_BOOL(false); return; }
    if (ofs > data->length()) { RET_BOOL(false); return; }
    ofs = data->length()-ofs;
  }
  if (ofs > data->length()) { RET_BOOL(false); return; }
  if (!specified_len) len = data->length()-ofs;
  if (len < 0) { RET_BOOL(false); return; }
  if (len > data->length()-ofs) { RET_BOOL(false); return; }
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_BOOL(false); return; }
    if (so->fd < 0 || !so->isAlive()) { RET_BOOL(false); return; }
    if (len == 0) { RET_BOOL(true); return; } // don't send zero datagrams (we can't do it anyway)
    RET_BOOL(so->sbuf.put(data->ptr()+ofs, (size_t)len));
  }
}


// can return empty string if there is noting in recv queue (or on error)
// you can check for errors using `IsAlive` IOCTL request
//native static final string SocketRecvStr (int sockid, optional int maxlen);
IMPLEMENT_FUNCTION(VObject, SocketRecvStr) {
  P_GET_INT_OPT(maxlen, 0x7fffffff);
  P_GET_INT(sockid);
  if (maxlen < 1) { RET_STR(VStr::EmptyString); return; }
  if (maxlen > 1024*1024) maxlen = 1024*1024; // arbitrary limit
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_STR(VStr::EmptyString); return; }
    if (so->rbuf.isEmpty()) { RET_STR(VStr::EmptyString); return; }
    if ((size_t)maxlen > so->rbuf.used) maxlen = (int)so->rbuf.used;
    VStr res;
    res.setLength(maxlen);
    if (!so->rbuf.get(res.getMutableCStr(), (size_t)maxlen)) res.clear();
    RET_STR(res);
  }
}

// append data to buffer; returns number of appended bytes or 0 on error/empty queue
//native static final int SocketRecvBuf (int sockid, ref array!ubyte data, optional int maxlen);
IMPLEMENT_FUNCTION(VObject, SocketRecvBuf) {
  P_GET_INT_OPT(maxlen, 0x7fffffff);
  P_GET_PTR(TArray<vuint8>, data);
  P_GET_INT(sockid);
  if (maxlen < 1) { RET_INT(0); return; }
  if (maxlen > 1024*1024) maxlen = 1024*1024; // arbitrary limit
  if (data->length() >= 16*1024*1024) { RET_INT(0); return; }
  {
    MyThreadLocker lock(&mainLock);
    SocketObj *so = findSocket(sockid);
    if (!so) { RET_INT(0); return; }
    if (so->rbuf.isEmpty()) { RET_INT(0); return; }
    if ((size_t)maxlen > so->rbuf.used) maxlen = (int)so->rbuf.used;
    if (maxlen > 16*1024*1024-data->length()) maxlen = 16*1024*1024-data->length();
    int olen = data->length();
    data->SetNumWithReserve(olen+maxlen);
    if (!so->rbuf.get(data->ptr()+olen, (size_t)maxlen)) data->setLength(olen, false); // don't resize
    RET_INT(data->length()-olen);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct DsEvData {
  int code;
  int sid;
  int data;
  bool wantAck;
};


static TArray<DsEvData> dsevids;
static vuint8 readbuf[1024*128];


// schedule, we should not hold a lock
static void sockQueueEvent (SocketObj *so, int evt, bool wantAck=true) {
  if (!so || so->id <= 0) return;
  DsEvData &dev = dsevids.alloc();
  dev.code = evt;
  dev.sid = so->id;
  dev.data = 0;
  dev.wantAck = wantAck;
}


// schedule, we should not hold a lock
static void sockQueueEventWithData (SocketObj *so, int evt, int data, bool wantAck=true) {
  if (!so || so->id <= 0) return;
  DsEvData &dev = dsevids.alloc();
  dev.code = evt;
  dev.sid = so->id;
  dev.data = data;
  dev.wantAck = wantAck;
}


static void sockErrored (SocketObj *so, int evt) {
  closeSO(so, true);
  so->state = SocketObj::ST_ERROR;
  sockQueueEvent(so, evt, true);
}


static MYTHREAD_RET_TYPE mainTrd (void *xarg) {
  //fprintf(stderr, "thread started\n");
  fd_set rds, wrs;
  for (;;) {
    int nfds = -1;
    FD_ZERO(&rds);
    FD_ZERO(&wrs);
    dsevids.reset();
    {
      MyThreadLocker lock(&mainLock);
      int sidx = 0;
      while (sidx < sockused) {
        SocketObj *so = &socklist[sidx];
        if (so->id == 0) {
          // remove it
          closeSO(so, true);
          deallocSocket(sidx);
          //fprintf(stderr, "dealloced socket\n");
          continue;
        }
        if (!so->isAlive()) {
          if (so->state == SocketObj::ST_CLOSE_REQUEST || so->state == SocketObj::ST_ABORT_REQUEST) {
            //fprintf(stderr, "closing socket #%d (%d:%d)\n", so->id, sidx, sockused);
            closeSO(so, (so->state == SocketObj::ST_ABORT_REQUEST));
            sockQueueEvent(so, evsock_disconnected);
          }
        } else {
          if (nfds < so->fd) nfds = so->fd;
          if (so->state == SocketObj::ST_CONNECTING) {
            //fprintf(stderr, "socket #%d is waiting for connection...\n", so->id);
            FD_SET(so->fd, &wrs);
          } else {
            FD_SET(so->fd, &rds);
            if (!so->sbuf.isEmpty()) FD_SET(so->fd, &wrs);
          }
        }
        ++sidx;
      }
    } // done creating sets
    // send scheduled events
    for (int f = 0; f < dsevids.length(); ++f) sockmodPostEventCB(dsevids[f].code, dsevids[f].sid, dsevids[f].data, dsevids[f].wantAck);
    // wait
    timeval to;
    to.tv_sec = 0;
    to.tv_usec = 10000; //(1000000) // 100 msecs
    int sres = select(nfds+1, &rds, &wrs, nullptr, &to);
    if (sres <= 0) continue; // nothing interesting happens
    // process sockets
    dsevids.reset();
    {
      MyThreadLocker lock(&mainLock);
      double currt = Sys_Time();
      for (int sidx = 0; sidx < sockused; ++sidx) {
        SocketObj *so = &socklist[sidx];
        if (so->id == 0 || !so->isAlive()) continue;
        if (so->fd < 0) { fprintf(stderr, "MODSOCK: fuuuu000!\n"); abort(); }
        // connecting?
        if (so->state == SocketObj::ST_CONNECTING) {
          if (!FD_ISSET(so->fd, &wrs)) {
            // check timeout
            if (so->toConnect >= 0 && currt-so->timeLastSend >= ((double)so->toConnect)/1000.0) {
              sockErrored(so, evsock_cantconnect);
            }
          } else {
            //fprintf(stderr, "socket #%d connected (or failed)\n", so->id);
            // connection complete
            int err = -1;
            socklen_t sl = sizeof(err);
            if (getsockopt(so->fd, SOL_SOCKET, SO_ERROR, SHITSOCKCAST(&err), &sl) != 0) {
              sockErrored(so, evsock_cantconnect);
            } else {
              if (err != 0) {
                sockErrored(so, evsock_cantconnect);
              } else {
                // update times
                so->timeLastRecv = so->timeLastSend = currt;
                if (so->needHandshake()) {
                  int cres = so->handshake();
                  if (cres < 0) {
                    sockErrored(so, evsock_cantconnect);
                    continue;
                  }
                  if (cres == 0) continue; // in progress
                }
                so->state = SocketObj::ST_NORMAL;
                sockQueueEvent(so, evsock_connected);
              }
            }
          }
        } else if (so->state == SocketObj::ST_NORMAL) {
          // write data, if we can
          if (FD_ISSET(so->fd, &wrs)) {
            while (!so->sbuf.isEmpty()) {
              //auto wr = send(so->fd, SHITSOCKCAST(so->sbuf.data), so->sbuf.used, MSG_NOSIGNAL);
              auto wr = so->send();
              if (wr == 0) {
                // if we sent zero bytes, it means that socket is closed (not really, but meh)
                closeSO(so, true);
                so->state = SocketObj::ST_DEAD;
                sockQueueEvent(so, evsock_disconnected);
                break;
              } else if (wr < 0) {
                int skerr = GetSockError();
                if (skerr != EAGAIN && skerr == SHITSOCKWOULDBLOCK) sockErrored(so, evsock_error);
                break;
              } else {
                if ((size_t)wr > so->sbuf.used) abort();
                so->sbuf.drop((size_t)wr);
                if (so->sbuf.isEmpty()) {
                  sockQueueEvent(so, evsock_sqempty, false); // no ack, nobody cares
                  break;
                }
                break;
                /*
                // try to send more
                fd_set wtmp;
                FD_ZERO(&wtmp);
                FD_SET(so->fd, &wtmp);
                to.tv_sec = 0;
                to.tv_usec = 10000; //(1000000) // 100 msecs
                if (select(so->fd+1, nullptr, &wtmp, nullptr, &to) != 1) break;
                if (!FD_ISSET(so->fd, &wtmp)) break;
                */
              }
            }
            // update times
            so->timeLastSend = currt;
          } else if (!so->sbuf.isEmpty()) {
            // check timeout
            if (so->toSend >= 0 && currt-so->timeLastSend >= ((double)so->toSend)/1000.0) {
              sockErrored(so, evsock_timeout);
              continue;
            }
          }
          // read data
          if (FD_ISSET(so->fd, &rds)) {
            int rdgotsize = 0;
            for (;;) {
              if (so->rbuf.isFull()) {
                if (rdgotsize > 0) sockQueueEventWithData(so, evsock_gotdata, rdgotsize);
                rdgotsize = 0;
                sockErrored(so, evsock_error);
                break;
              }
              size_t toread = sizeof(readbuf);
              if (toread > so->rbuf.maxsize-so->rbuf.used) toread = so->rbuf.maxsize-so->rbuf.used;
              //auto rd = recv(so->fd, SHITSOCKCAST(readbuf), toread, 0);
              auto rd = so->recv(readbuf, toread);
              //fprintf(stderr, "socket #%d: rd=%d\n", so->id, (int)rd);
              if (rd == 0) {
                // if we received zero bytes, it means that socket is closed (not really, but meh)
                // UDP datagrams can have empty payload, though, and cannot be closed
                if (!so->isUDP) {
                  if (rdgotsize > 0) sockQueueEventWithData(so, evsock_gotdata, rdgotsize);
                  rdgotsize = 0;
                  closeSO(so, true);
                  so->state = SocketObj::ST_DEAD;
                  sockQueueEvent(so, evsock_disconnected);
                  break;
                } else {
                  // zero-payload datagram; just ignore it for now
                  // update times
                  so->timeLastRecv = currt;
                }
              } else if (rd < 0) {
                int skerr = GetSockError();
                if (skerr != EAGAIN && skerr == SHITSOCKWOULDBLOCK) sockErrored(so, evsock_error);
                break;
              } else {
                // received some data, put it into buffer
                if (!so->rbuf.put(readbuf, (size_t)rd)) abort(); // the thing that should not be
                // update times
                so->timeLastRecv = currt;
                rdgotsize += (int)rd;
                if (rdgotsize >= 1024*1024) break;
                break;
                /*
                // try to recv more
                fd_set rtmp;
                FD_ZERO(&rtmp);
                FD_SET(so->fd, &rtmp);
                to.tv_sec = 0;
                to.tv_usec = 10000; //(1000000) // 100 msecs
                if (select(so->fd+1, &rtmp, nullptr, nullptr, &to) != 1) break;
                if (!FD_ISSET(so->fd, &rtmp)) break;
                */
              }
            }
            if (rdgotsize > 0) sockQueueEventWithData(so, evsock_gotdata, rdgotsize);
          } else /*if (!so->recv.isEmpty())*/ {
            // check timeout
            if (so->toRecv >= 0 && currt-so->timeLastRecv >= ((double)so->toRecv)/1000.0) {
              sockErrored(so, evsock_timeout);
              continue;
            }
          }
        } else {
          // just in case
          sockErrored(so, evsock_error);
        }
      }
    }
    // send scheduled events
    for (int f = 0; f < dsevids.length(); ++f) sockmodPostEventCB(dsevids[f].code, dsevids[f].sid, dsevids[f].data, dsevids[f].wantAck);
  }
  return MYTHREAD_RET_VALUE;
}


// ////////////////////////////////////////////////////////////////////////// //
static void sockmodInit () {
#ifdef WIN32
  static WSADATA winsockdata;
  if (WSAStartup(MAKEWORD(1, 1), &winsockdata) == 0) {
    initSuccess = true;
  } else {
    initSuccess = false;
    //fprintf(stderr, "VCCRUN: shitsock init failed!\n");
  }
#endif
#ifdef USE_GNU_TLS
  gnutls_global_init();
#endif
  mythread_mutex_init(&mainLock);
  if (mythread_create(&mainThread, &mainTrd, nullptr) != 0) Sys_Error("cannot create socket thread");
}
