// ////////////////////////////////////////////////////////////////////////// //
// Some threading related helper macros and functions
// Author: Lasse Collin
//
// This file has been put into the public domain.
// You can do whatever you want with this file.
//
// changes by Ketmar // Invisible Vector
#ifndef VC_MYTHREAD_H
#define VC_MYTHREAD_H

#ifndef WIN32
# define MYTHREAD_POSIX
#endif

#include <inttypes.h>


////////////////////////////////////////
// Shared between all threading types //
////////////////////////////////////////

// Locks a mutex for a duration of a block.
//
// Perform mythread_mutex_lock(&mutex) in the beginning of a block
// and mythread_mutex_unlock(&mutex) at the end of the block. "break"
// may be used to unlock the mutex and jump out of the block.
// mythread_sync blocks may be nested.
//
// Example:
//
//     mythread_sync(mutex) {
//         foo();
//         if (some_error)
//             break; // Skips bar()
//         bar();
//     }
//
// At least GCC optimizes the loops completely away so it doesn't slow
// things down at all compared to plain mythread_mutex_lock(&mutex)
// and mythread_mutex_unlock(&mutex) calls.
//
#define mythread_sync(mutex)                mythread_sync_helper1(mutex, __LINE__)
#define mythread_sync_helper1(mutex, line)  mythread_sync_helper2(mutex, line)
#define mythread_sync_helper2(mutex, line) \
  for (unsigned int mythread_i_ ## line = 0; \
      mythread_i_ ## line \
        ? (mythread_mutex_unlock(&(mutex)), 0) \
        : (mythread_mutex_lock(&(mutex)), 1); \
      mythread_i_ ## line = 1) \
    for (unsigned int mythread_j_ ## line = 0; \
        !mythread_j_ ## line; \
        mythread_j_ ## line = 1)


// ////////////////////////////////////////////////////////////////////////// //
// pthreads
#if defined(MYTHREAD_POSIX)

#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MYTHREAD_RET_TYPE   void *
#define MYTHREAD_RET_VALUE  nullptr

typedef pthread_t mythread;
typedef pthread_mutex_t mythread_mutex;

struct mythread_cond {
  pthread_cond_t cond;
  // Clock ID (CLOCK_REALTIME or CLOCK_MONOTONIC) associated with
  // the condition variable.
  clockid_t clk_id;
};

typedef struct timespec mythread_condtime;


// Calls the given function once in a thread-safe way.
#define mythread_once(func)  do { \
  static pthread_once_t once_ = PTHREAD_ONCE_INIT; \
  pthread_once(&once_, &func); \
} while (0)


// Use pthread_sigmask() to set the signal mask in multi-threaded programs.
// Do nothing on OpenVMS since it lacks pthread_sigmask().
static __attribute__((unused)) inline void mythread_sigmask (int how, const sigset_t * /*restrict*/ set, sigset_t * /*restrict*/ oset) {
  /*int ret =*/ pthread_sigmask(how, set, oset);
  /*
  assert(ret == 0);
  (void)ret;
  */
}

// Creates a new thread with all signals blocked. Returns zero on success
// and non-zero on error.
static __attribute__((unused)) inline int mythread_create (mythread *thread, void *(*func) (void *arg), void *arg) {
  sigset_t old;
  sigset_t all;
  sigfillset(&all);
  mythread_sigmask(SIG_SETMASK, &all, &old);
  const int ret = pthread_create(thread, nullptr, func, arg);
  mythread_sigmask(SIG_SETMASK, &old, nullptr);
  return ret;
}

// Joins a thread. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_join (mythread thread) {
  return pthread_join(thread, nullptr);
}

// Initiatlizes a mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init (mythread_mutex *mutex) {
  return pthread_mutex_init(mutex, nullptr);
}

static __attribute__((unused)) inline void mythread_mutex_destroy (mythread_mutex *mutex) {
  /*int ret =*/ pthread_mutex_destroy(mutex);
  /*
  assert(ret == 0);
  (void)ret;
  */
}

static __attribute__((unused)) inline void mythread_mutex_lock (mythread_mutex *mutex) {
  /*int ret =*/ pthread_mutex_lock(mutex);
  /*
  assert(ret == 0);
  (void)ret;
  */
}

static __attribute__((unused)) inline void mythread_mutex_unlock (mythread_mutex *mutex) {
  /*int ret =*/ pthread_mutex_unlock(mutex);
  /*
  assert(ret == 0);
  (void)ret;
  */
}


// Initializes a condition variable.
//
// Using CLOCK_MONOTONIC instead of the default CLOCK_REALTIME makes the
// timeout in pthread_cond_timedwait() work correctly also if system time
// is suddenly changed. Unfortunately CLOCK_MONOTONIC isn't available
// everywhere while the default CLOCK_REALTIME is, so the default is
// used if CLOCK_MONOTONIC isn't available.
//
// If clock_gettime() isn't available at all, gettimeofday() will be used.
static __attribute__((unused)) inline int mythread_cond_init (mythread_cond *mycond) {
  struct timespec ts;
  pthread_condattr_t condattr;

  // POSIX doesn't seem to *require* that pthread_condattr_setclock()
  // will fail if given an unsupported clock ID. Test that
  // CLOCK_MONOTONIC really is supported using clock_gettime().
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0 && pthread_condattr_init(&condattr) == 0) {
    int ret = pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    if (ret == 0) ret = pthread_cond_init(&mycond->cond, &condattr);
    pthread_condattr_destroy(&condattr);
    if (ret == 0) {
      mycond->clk_id = CLOCK_MONOTONIC;
      return 0;
    }
  }

  // If anything above fails, fall back to the default CLOCK_REALTIME.
  // POSIX requires that all implementations of clock_gettime() must
  // support at least CLOCK_REALTIME.
  mycond->clk_id = CLOCK_REALTIME;
  return pthread_cond_init(&mycond->cond, nullptr);
}

static __attribute__((unused)) inline void mythread_cond_destroy (mythread_cond *cond) {
  /*int ret =*/ pthread_cond_destroy(&cond->cond);
  /*
  assert(ret == 0);
  (void)ret;
  */
}


static __attribute__((unused)) inline void mythread_cond_signal (mythread_cond *cond) {
  /*int ret =*/ pthread_cond_signal(&cond->cond);
  /*
  assert(ret == 0);
  (void)ret;
  */
}

static __attribute__((unused)) inline void mythread_cond_wait (mythread_cond *cond, mythread_mutex *mutex) {
  /*int ret =*/ pthread_cond_wait(&cond->cond, mutex);
  /*
  assert(ret == 0);
  (void)ret;
  */
}

// Waits on a condition or until a timeout expires. If the timeout expires,
// non-zero is returned, otherwise zero is returned.
static __attribute__((unused)) inline int mythread_cond_timedwait (mythread_cond *cond, mythread_mutex *mutex, const mythread_condtime *condtime) {
  /*int ret =*/ return pthread_cond_timedwait(&cond->cond, mutex, condtime);
  /*
  assert(ret == 0 || ret == ETIMEDOUT);
  return ret;
  */
}

// Sets condtime to the absolute time that is timeout_ms milliseconds
// in the future. The type of the clock to use is taken from cond.
static __attribute__((unused)) inline void mythread_condtime_set (mythread_condtime *condtime, const mythread_cond *cond, uint32_t timeout_ms) {
  condtime->tv_sec = timeout_ms/1000;
  condtime->tv_nsec = (timeout_ms%1000)*1000000;

  struct timespec now;
  /*int ret =*/ clock_gettime(cond->clk_id, &now);
  /*
  assert(ret == 0);
  (void)ret;
  */

  condtime->tv_sec += now.tv_sec;
  condtime->tv_nsec += now.tv_nsec;

  // tv_nsec must stay in the range [0, 999_999_999].
  if (condtime->tv_nsec >= 1000000000L) {
    condtime->tv_nsec -= 1000000000L;
    ++condtime->tv_sec;
  }
}


#else
// ////////////////////////////////////////////////////////////////////////// //
// shitdoze

#define WIN32_LEAN_AND_MEAN
/*
#ifdef MYTHREAD_VISTA
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0600
#endif
*/
#include <windows.h>
#include <process.h>

// fuck you, shitdoze!
#ifdef RGB
# undef RGB
#endif

#ifdef RGBA
# undef RGBA
#endif

#define MYTHREAD_RET_TYPE   unsigned int __stdcall
#define MYTHREAD_RET_VALUE  0

typedef HANDLE mythread;
typedef CRITICAL_SECTION mythread_mutex;

typedef HANDLE mythread_cond;
//typedef CONDITION_VARIABLE mythread_cond;

struct mythread_condtime {
  // Tick count (milliseconds) in the beginning of the timeout.
  // NOTE: This is 32 bits so it wraps around after 49.7 days.
  // Multi-day timeouts may not work as expected.
  DWORD start;

  // Length of the timeout in milliseconds. The timeout expires
  // when the current tick count minus "start" is equal or greater
  // than "timeout".
  DWORD timeout;
};


// mythread_once() is only available with Vista threads.
/*
#ifdef MYTHREAD_VISTA
#define mythread_once(func) \
  do { \
    static INIT_ONCE once_ = INIT_ONCE_STATIC_INIT; \
    BOOL pending_; \
    if (!InitOnceBeginInitialize(&once_, 0, &pending_, nullptr)) \
      abort(); \
    if (pending_) \
      func(); \
    if (!InitOnceComplete(&once, 0, nullptr)) \
      abort(); \
  } while (0)
#endif
*/


// mythread_sigmask() isn't available on Windows. Even a dummy version would
// make no sense because the other POSIX signal functions are missing anyway.


static __attribute__((unused)) inline int mythread_create (mythread *thread, unsigned int (__stdcall *func) (void *arg), void *arg) {
  uintptr_t ret = _beginthreadex(nullptr, 0, func, arg, 0, nullptr);
  if (ret == 0) return -1;
  *thread = (HANDLE)ret;
  return 0;
}

static __attribute__((unused)) inline int mythread_join (mythread thread) {
  int ret = 0;
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) ret = -1;
  if (!CloseHandle(thread)) ret = -1;
  return ret;
}

static __attribute__((unused)) inline int mythread_mutex_init (mythread_mutex *mutex) {
  InitializeCriticalSection(mutex);
  return 0;
}

static __attribute__((unused)) inline void mythread_mutex_destroy (mythread_mutex *mutex) {
  DeleteCriticalSection(mutex);
}

static __attribute__((unused)) inline void mythread_mutex_lock (mythread_mutex *mutex) {
  EnterCriticalSection(mutex);
}

static __attribute__((unused)) inline void mythread_mutex_unlock (mythread_mutex *mutex) {
  LeaveCriticalSection(mutex);
}

static __attribute__((unused)) inline int mythread_cond_init (mythread_cond *cond) {
  *cond = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  return *cond == nullptr ? -1 : 0;
  //InitializeConditionVariable(cond);
  //return 0;
}

static __attribute__((unused)) inline void mythread_cond_destroy (mythread_cond *cond) {
  CloseHandle(*cond);
  //(void)cond;
}

static __attribute__((unused)) inline void mythread_cond_signal (mythread_cond *cond) {
  SetEvent(*cond);
  //WakeConditionVariable(cond);
}

static __attribute__((unused)) inline void mythread_cond_wait (mythread_cond *cond, mythread_mutex *mutex) {
  LeaveCriticalSection(mutex);
  WaitForSingleObject(*cond, INFINITE);
  EnterCriticalSection(mutex);
  //BOOL ret = SleepConditionVariableCS(cond, mutex, INFINITE);
  //assert(ret);
  //(void)ret;
}

static __attribute__((unused)) inline int mythread_cond_timedwait (mythread_cond *cond, mythread_mutex *mutex, const mythread_condtime *condtime) {
  LeaveCriticalSection(mutex);
  DWORD elapsed = GetTickCount()-condtime->start;
  DWORD timeout = (elapsed >= condtime->timeout ? 0 : condtime->timeout-elapsed);
  DWORD ret = WaitForSingleObject(*cond, timeout);
  //assert(ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT);
  EnterCriticalSection(mutex);
  return ret == WAIT_TIMEOUT;
  //BOOL ret = SleepConditionVariableCS(cond, mutex, timeout);
  //assert(ret || GetLastError() == ERROR_TIMEOUT);
  //return !ret;
}

static __attribute__((unused)) inline void mythread_condtime_set (mythread_condtime *condtime, const mythread_cond *cond, uint32_t timeout) {
  (void)cond;
  condtime->start = GetTickCount();
  condtime->timeout = timeout;
}

#endif


class MyThreadLocker {
private:
  MyThreadLocker (const MyThreadLocker &);
  void operator = (const MyThreadLocker &);

public:
  mythread_mutex *mutex;

  MyThreadLocker (mythread_mutex *amutex) : mutex(amutex) { mythread_mutex_lock(mutex); }
  ~MyThreadLocker () { mythread_mutex_unlock(mutex); }
};


#endif
