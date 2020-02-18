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
# ifdef NO_PTHREAD
#  define MYTHREAD_C11
# else
#  define MYTHREAD_POSIX
# endif
#endif

#include <inttypes.h>

#ifdef __cplusplus
# define VC_MYTHREAD_NOEXCEPT noexcept
#else
# define VC_MYTHREAD_NOEXCEPT
#endif


//////////////////////////////////////////
// atomic integer operations            //
// atomic_int guaranteed to be 32 bytes //
//////////////////////////////////////////

typedef int32_t atomic_int;


// compare var with val_check; set var to val_new if values are equal
// return old var value in any case
static __attribute__((unused)) inline atomic_int atomic_cmp_xchg (atomic_int *var, const atomic_int val_check, const atomic_int val_new) {
  // this is easier than `__atomic`
  return __sync_val_compare_and_swap(var, val_check, val_new);
}

// get var value
static __attribute__((unused)) inline atomic_int atomic_get (atomic_int *var) {
  //return atomic_cmp_xchg(var, 0, 0);
  return __atomic_load_n(var, __ATOMIC_SEQ_CST);
}

// set new value, return old
static __attribute__((unused)) inline atomic_int atomic_set (atomic_int *var, const atomic_int val_new) {
  /*
  atomic_int res = atomic_cmp_xchg(var, val_new, val_new);
  atomic_int v = res;
  while (v != val_new) v = atomic_cmp_xchg(var, v, val_new);
  return res;
  */
  return __atomic_exchange_n(var, val_new, __ATOMIC_SEQ_CST);
}

// set new value
static __attribute__((unused)) inline void atomic_store (atomic_int *var, const atomic_int val_new) {
  return __atomic_store_n(var, val_new, __ATOMIC_SEQ_CST);
}

// returns new value
static __attribute__((unused)) inline atomic_int atomic_increment (atomic_int *var) {
  return __atomic_add_fetch(var, 1, __ATOMIC_SEQ_CST);
}

// returns new value
static __attribute__((unused)) inline atomic_int atomic_decrement (atomic_int *var) {
  return __atomic_sub_fetch(var, 1, __ATOMIC_SEQ_CST);
}

static __attribute__((unused)) inline void atomic_fence () {
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}


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
#define MYTHREAD_RET_VALUE  NULL

typedef pthread_t         mythread;
typedef pthread_mutex_t   mythread_mutex;
typedef pthread_rwlock_t  mythread_rwlock;

struct mythread_cond {
  pthread_cond_t cond;
  // Clock ID (CLOCK_REALTIME or CLOCK_MONOTONIC) associated with
  // the condition variable.
  clockid_t clk_id;
};

typedef struct timespec mythread_condtime;


// You'd better declare it as global, not local static.
#define MYTHREAD_DECLARE_ONCE(name_)  static pthread_once_t name_ = PTHREAD_ONCE_INIT;

// Calls the given function once in a thread-safe way.
#define mythread_once(oncename_,func_)  do { \
  pthread_once(&oncename_, &func_); \
} while (0)


// Use pthread_sigmask() to set the signal mask in multi-threaded programs.
// Do nothing on OpenVMS since it lacks pthread_sigmask().
static __attribute__((unused)) inline void mythread_sigmask (int how, const sigset_t * /*restrict*/ set, sigset_t * /*restrict*/ oset) VC_MYTHREAD_NOEXCEPT {
#ifndef NO_SIGNALS
  /*int ret =*/ pthread_sigmask(how, set, oset);
  /*
  assert(ret == 0);
  (void)ret;
  */
#endif
}

// Creates a new thread with all signals blocked. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_create (mythread *thread, void *(*func) (void *arg), void *arg) VC_MYTHREAD_NOEXCEPT {
  sigset_t old;
  sigset_t all;
  sigfillset(&all);
  mythread_sigmask(SIG_SETMASK, &all, &old);
  const int ret = pthread_create(thread, NULL, func, arg);
  mythread_sigmask(SIG_SETMASK, &old, NULL);
  return ret;
}

// Joins a thread. Returns zero on success and non-zero on error.
// This also closes thread handle.
static __attribute__((unused)) inline int mythread_join (mythread thread) VC_MYTHREAD_NOEXCEPT {
  return pthread_join(thread, NULL);
}

// Detaches a thread, and closes thread handle. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_detach (mythread thread) VC_MYTHREAD_NOEXCEPT {
  return pthread_detach(thread);
}

// Initiatlizes a mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return pthread_mutex_init(mutex, NULL);
}

// Initiatlizes a recursive mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init_recursive (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  pthread_mutexattr_t attr;
  int res = pthread_mutexattr_init(&attr);
  if (res) return res;
  res = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
  if (res) { pthread_mutexattr_destroy(&attr); return res; }
  res = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  if (res) { pthread_mutexattr_destroy(&attr); return res; }
  res = pthread_mutex_init(mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  return res;
}

static __attribute__((unused)) inline void mythread_mutex_destroy (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (pthread_mutex_destroy(mutex) != 0) abort();
}

static __attribute__((unused)) inline void mythread_mutex_lock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (pthread_mutex_lock(mutex) != 0) abort();
}

static __attribute__((unused)) inline void mythread_mutex_unlock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (pthread_mutex_unlock(mutex) != 0) abort();
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_mutex_trylock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return pthread_mutex_trylock(mutex);
}


// Initializes a condition variable. Returns zero on success and non-zero on error.
//
// Using CLOCK_MONOTONIC instead of the default CLOCK_REALTIME makes the
// timeout in pthread_cond_timedwait() work correctly also if system time
// is suddenly changed. Unfortunately CLOCK_MONOTONIC isn't available
// everywhere while the default CLOCK_REALTIME is, so the default is
// used if CLOCK_MONOTONIC isn't available.
//
// If clock_gettime() isn't available at all, gettimeofday() will be used.
static __attribute__((unused)) inline int mythread_cond_init (mythread_cond *mycond) VC_MYTHREAD_NOEXCEPT {
  struct timespec ts;
  pthread_condattr_t condattr;

  // POSIX doesn't seem to *require* that pthread_condattr_setclock()
  // will fail if given an unsupported clock ID. Test that
  // CLOCK_MONOTONIC really is supported using clock_gettime().
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0 && pthread_condattr_init(&condattr) == 0) {
    int res = pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    if (res == 0) res = pthread_cond_init(&mycond->cond, &condattr);
    pthread_condattr_destroy(&condattr);
    if (res == 0) {
      mycond->clk_id = CLOCK_MONOTONIC;
      return 0;
    }
  }

  // If anything above fails, fall back to the default CLOCK_REALTIME.
  // POSIX requires that all implementations of clock_gettime() must
  // support at least CLOCK_REALTIME.
  mycond->clk_id = CLOCK_REALTIME;
  return pthread_cond_init(&mycond->cond, NULL);
}

static __attribute__((unused)) inline void mythread_cond_destroy (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  if (pthread_cond_destroy(&cond->cond) != 0) abort();
}


static __attribute__((unused)) inline void mythread_cond_signal (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  if (pthread_cond_signal(&cond->cond) != 0) abort();
}

static __attribute__((unused)) inline void mythread_cond_broadcast (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  if (pthread_cond_broadcast(&cond->cond) != 0) abort();
}

// Atomically releases the mutex, and waits for condition signal.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline void mythread_cond_wait (mythread_cond *cond, mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (pthread_cond_wait(&cond->cond, mutex) != 0) abort();
}

// Atomically releases the mutex, and waits on a condition or until a timeout expires.
// If the timeout expires, non-zero is returned, otherwise zero is returned.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline int mythread_cond_timedwait (mythread_cond *cond, mythread_mutex *mutex, const mythread_condtime *condtime) VC_MYTHREAD_NOEXCEPT {
  return pthread_cond_timedwait(&cond->cond, mutex, condtime);
  //assert(ret == 0 || ret == ETIMEDOUT);
}

// Sets condtime to the absolute time that is timeout_ms milliseconds
// in the future. The type of the clock to use is taken from cond.
static __attribute__((unused)) inline void mythread_condtime_set (mythread_condtime *condtime, const mythread_cond *cond, uint32_t timeout_ms) VC_MYTHREAD_NOEXCEPT {
  condtime->tv_sec = timeout_ms/1000;
  condtime->tv_nsec = (timeout_ms%1000)*1000000;

  struct timespec now;
  if (clock_gettime(cond->clk_id, &now) != 0) abort();

  condtime->tv_sec += now.tv_sec;
  condtime->tv_nsec += now.tv_nsec;

  // tv_nsec must stay in the range [0, 999_999_999].
  if (condtime->tv_nsec >= 1000000000L) {
    condtime->tv_nsec -= 1000000000L;
    ++condtime->tv_sec;
  }
}


// Initiatlizes an rwlock. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_rwlock_init (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  return pthread_rwlock_init(rwlock, NULL);
}

static __attribute__((unused)) inline void mythread_rwlock_destroy (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  pthread_rwlock_destroy(rwlock);
}

static __attribute__((unused)) inline void mythread_rwlock_lock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  if (pthread_rwlock_rdlock(rwlock) != 0) abort();
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_rwlock_trylock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  return pthread_rwlock_tryrdlock(rwlock);
}

static __attribute__((unused)) inline void mythread_rwlock_unlock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  if (pthread_rwlock_unlock(rwlock) != 0) abort();
}

static __attribute__((unused)) inline void mythread_rwlock_lock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  if (pthread_rwlock_wrlock(rwlock) != 0) abort();
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_rwlock_trylock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  return pthread_rwlock_trywrlock(rwlock);
}

static __attribute__((unused)) inline void mythread_rwlock_unlock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  if (pthread_rwlock_unlock(rwlock) != 0) abort();
}


#elif defined(MYTHREAD_C11)
// ////////////////////////////////////////////////////////////////////////// //
// weird-ass platforms with no pthreads but with c11 threads, e.g. switch

#include <sys/time.h>
#include <threads.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MYTHREAD_RET_TYPE   int
#define MYTHREAD_RET_VALUE  0

typedef thrd_t mythread;
typedef mtx_t mythread_mutex;

struct mythread_cond {
  cnd_t cond;
  // Clock ID (CLOCK_REALTIME or CLOCK_MONOTONIC) associated with
  // the condition variable.
  clockid_t clk_id;
};

typedef struct timespec mythread_condtime;


// You'd better declare it as global, not local static.
#define MYTHREAD_DECLARE_ONCE(name_)  static once_flag name_ = ONCE_FLAG_INIT;

// Calls the given function once in a thread-safe way.
#define mythread_once(oncename_,func_)  do { \
  pthread_once(&oncename_, &func_); \
} while (0)


// Use pthread_sigmask() to set the signal mask in multi-threaded programs.
// Do nothing on OpenVMS or Switch since they lack pthread_sigmask().
#if 0
static __attribute__((unused)) inline void mythread_sigmask (int how, const sigset_t * /*restrict*/ set, sigset_t * /*restrict*/ oset) VC_MYTHREAD_NOEXCEPT {
}
#endif

// Creates a new thread with all signals blocked. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_create (mythread *thread, int (*func) (void *arg), void *arg) VC_MYTHREAD_NOEXCEPT {
  return (thrd_create(thread, func, arg) != thrd_success);
}

// Joins a thread. Returns zero on success and non-zero on error.
// This also closes thread handle.
static __attribute__((unused)) inline int mythread_join (mythread thread) VC_MYTHREAD_NOEXCEPT {
  return (thrd_join(thread, NULL) != thrd_success);
}

// Detaches a thread, and closes thread handle. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_detach (mythread thread) VC_MYTHREAD_NOEXCEPT {
  return (thrd_detach(thread) != thrd_success);
}


// Initiatlizes a mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return (mtx_init(mutex, mtx_plain) != thrd_success);
}

// Initiatlizes a recursive mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init_recursive (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return (mtx_init(mutex, mtx_plain|mtx_recursive) != thrd_success);
}

static __attribute__((unused)) inline void mythread_mutex_destroy (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  mtx_destroy(mutex);
}

static __attribute__((unused)) inline void mythread_mutex_lock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (mtx_lock(mutex) != thrd_success) abort();
}

static __attribute__((unused)) inline void mythread_mutex_unlock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (mtx_unlock(mutex) != thrd_success) abort();
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_mutex_trylock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return (mtx_trylock(mutex) != thrd_success);
}


// Initializes a condition variable. Returns zero on success and non-zero on error.
//
// Using CLOCK_MONOTONIC instead of the default CLOCK_REALTIME makes the
// timeout in pthread_cond_timedwait() work correctly also if system time
// is suddenly changed. Unfortunately CLOCK_MONOTONIC isn't available
// everywhere while the default CLOCK_REALTIME is, so the default is
// used if CLOCK_MONOTONIC isn't available.
//
// If clock_gettime() isn't available at all, gettimeofday() will be used.
static __attribute__((unused)) inline int mythread_cond_init (mythread_cond *mycond) VC_MYTHREAD_NOEXCEPT {
  // If anything above fails, fall back to the default CLOCK_REALTIME.
  // POSIX requires that all implementations of clock_gettime() must
  // support at least CLOCK_REALTIME.
  mycond->clk_id = CLOCK_REALTIME;
  return (cnd_init(&mycond->cond) != thrd_success);
}

static __attribute__((unused)) inline void mythread_cond_destroy (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  cnd_destroy(&cond->cond);
}

static __attribute__((unused)) inline void mythread_cond_signal (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  if (cnd_signal(&cond->cond) != thrd_success) abort();
}

static __attribute__((unused)) inline void mythread_cond_broadcast (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  if (cnd_broadcast(&cond->cond) != thrd_success) abort();
}

// Atomically releases the mutex, and waits for condition signal.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline void mythread_cond_wait (mythread_cond *cond, mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (cnd_wait(&cond->cond, mutex) != thrd_success) abort();
}

// Atomically releases the mutex, and waits on a condition or until a timeout expires.
// If the timeout expires, non-zero is returned, otherwise zero is returned.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline int mythread_cond_timedwait (mythread_cond *cond, mythread_mutex *mutex, const mythread_condtime *condtime) VC_MYTHREAD_NOEXCEPT {
  return (cnd_timedwait(&cond->cond, mutex, condtime) != thrd_success);
}

// Sets condtime to the absolute time that is timeout_ms milliseconds
// in the future. The type of the clock to use is taken from cond.
static __attribute__((unused)) inline void mythread_condtime_set (mythread_condtime *condtime, const mythread_cond *cond, uint32_t timeout_ms) VC_MYTHREAD_NOEXCEPT {
  condtime->tv_sec = timeout_ms/1000;
  condtime->tv_nsec = (timeout_ms%1000)*1000000;

  struct timespec now;
  if (clock_gettime(cond->clk_id, &now) != 0) abort();

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
typedef CRITICAL_SECTION    mythread_mutex;
typedef CONDITION_VARIABLE  mythread_cond;
typedef SRWLOCK             mythread_rwlock;

typedef DWORD mythread_condtime;


// You'd better declare it as global, not local static.
#define MYTHREAD_DECLARE_ONCE(name_)  static INIT_ONCE name_ = INIT_ONCE_STATIC_INIT;

// mythread_once() is only available with Vista threads.
// but this is what we target anyway
#define mythread_once(oncename_,func_) do { \
  BOOL pending_; \
  if (!InitOnceBeginInitialize(&oncename_, 0, &pending_, NULL)) abort(); \
  if (pending_) func_(); \
  /*if (!InitOnceComplete(&oncename_, 0, NULL)) abort();*/ \
  InitOnceComplete(&oncename_, 0, NULL); \
} while (0)


// mythread_sigmask() isn't available on Windows. Even a dummy version would
// make no sense because the other POSIX signal functions are missing anyway.


// Creates a new thread with all signals blocked. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_create (mythread *thread, unsigned int (__stdcall *func) (void *arg), void *arg) VC_MYTHREAD_NOEXCEPT {
  uintptr_t ret = _beginthreadex(NULL, 0, func, arg, 0, NULL);
  if (ret == 0) { *thread = (HANDLE)0; return -1; }
  *thread = (HANDLE)ret;
  return 0;
}

// Joins a thread. Returns zero on success and non-zero on error.
// This also closes thread handle.
static __attribute__((unused)) inline int mythread_join (mythread thread) VC_MYTHREAD_NOEXCEPT {
  if (!thread) return -1;
  int ret = 0;
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) ret = -1;
  if (!CloseHandle(thread)) ret = -1;
  return ret;
}

// Detaches a thread, and closes thread handle. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_detach (mythread thread) VC_MYTHREAD_NOEXCEPT {
  if (!thread) return -1;
  return (CloseHandle(thread) ? 0 : -1);
}


// Initiatlizes a mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  InitializeCriticalSection(mutex);
  return 0;
}

// Initiatlizes a recursive mutex. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_mutex_init_recursive (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  InitializeCriticalSection(mutex);
  return 0;
}

static __attribute__((unused)) inline void mythread_mutex_destroy (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  DeleteCriticalSection(mutex);
}

static __attribute__((unused)) inline void mythread_mutex_lock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  EnterCriticalSection(mutex);
}

static __attribute__((unused)) inline void mythread_mutex_unlock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  LeaveCriticalSection(mutex);
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_mutex_trylock (mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  return (TryEnterCriticalSection(mutex) ? 0 : 1);
}


// Initializes a condition variable. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_cond_init (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  InitializeConditionVariable(cond);
  return 0;
}

static __attribute__((unused)) inline void mythread_cond_destroy (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  (void)cond;
}

static __attribute__((unused)) inline void mythread_cond_signal (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  WakeConditionVariable(cond);
}

static __attribute__((unused)) inline void mythread_cond_broadcast (mythread_cond *cond) VC_MYTHREAD_NOEXCEPT {
  WakeAllConditionVariable(cond);
}

// Atomically releases the mutex, and waits for condition signal.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline void mythread_cond_wait (mythread_cond *cond, mythread_mutex *mutex) VC_MYTHREAD_NOEXCEPT {
  if (!SleepConditionVariableCS(cond, mutex, INFINITE)) abort();
}

// Atomically releases the mutex, and waits on a condition or until a timeout expires.
// If the timeout expires, non-zero is returned, otherwise zero is returned.
// Upon return, the mutex is locked again.
static __attribute__((unused)) inline int mythread_cond_timedwait (mythread_cond *cond, mythread_mutex *mutex, const mythread_condtime *condtime) VC_MYTHREAD_NOEXCEPT {
  BOOL ret = SleepConditionVariableCS(cond, mutex, *condtime);
  if (!ret) {
    if (GetLastError() != ERROR_TIMEOUT) abort();
    return 1;
  }
  return 0;
}

// Sets condtime to the absolute time that is timeout_ms milliseconds
// in the future. The type of the clock to use is taken from cond.
static __attribute__((unused)) inline void mythread_condtime_set (mythread_condtime *condtime, const mythread_cond *cond, uint32_t timeout) VC_MYTHREAD_NOEXCEPT {
  (void)cond;
  *condtime = timeout;
}


// Initiatlizes an rwlock. Returns zero on success and non-zero on error.
static __attribute__((unused)) inline int mythread_rwlock_init (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  InitializeSRWLock(rwlock);
  return 0;
}

static __attribute__((unused)) inline void mythread_rwlock_destroy (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
}

static __attribute__((unused)) inline void mythread_rwlock_lock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  AcquireSRWLockShared(rwlock);
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_rwlock_trylock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  return (TryAcquireSRWLockShared(rwlock) ? 0 : 1);
}

static __attribute__((unused)) inline void mythread_rwlock_unlock_read (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  ReleaseSRWLockShared(rwlock);
}

static __attribute__((unused)) inline void mythread_rwlock_lock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  AcquireSRWLockExclusive(rwlock);
}

// Returns zero if the lock was aquired.
static __attribute__((unused)) inline int mythread_rwlock_trylock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  return (TryAcquireSRWLockExclusive(rwlock) ? 0 : 1);
}

static __attribute__((unused)) inline void mythread_rwlock_unlock_write (mythread_rwlock *rwlock) VC_MYTHREAD_NOEXCEPT {
  ReleaseSRWLockExclusive(rwlock);
}

#endif


// ////////////////////////////////////////////////////////////////////////// //
// high-level lock structures
// shitdoze can suck cock
// ////////////////////////////////////////////////////////////////////////// //

// ////////////////////////////////////////////////////////////////////////// //
// event (with possible info int)
// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  /*WARNING! NO USER-SERVICEABLE PARTS INSIDE!*/
  mythread_mutex mutex;
  mythread_cond cond;
  volatile unsigned flags;
  volatile int info;
} mythread_event;

#define MYTHREAD_EVENT_NO_FLAGS   (0u)
#define MYTHREAD_EVENT_SIGNALED   (1u)
#define MYTHREAD_EVENT_AUTORESET  (2u)

// Returns zero if succesfully created.
static __attribute__((unused)) inline int mythread_event_init (mythread_event *evt, unsigned flags) {
  if (!evt) return -1;
  memset((void *)evt, 0, sizeof(*evt));
  evt->flags = flags;
  int res = mythread_mutex_init(&evt->mutex);
  if (res) return res;
  res = mythread_cond_init(&evt->cond);
  if (res) { mythread_mutex_destroy(&evt->mutex); return res; }
  return 0;
}

static __attribute__((unused)) inline void mythread_event_destroy (mythread_event *evt) {
  if (evt) {
    mythread_cond_destroy(&evt->cond);
    mythread_mutex_destroy(&evt->mutex);
    memset((void *)evt, 0, sizeof(*evt));
  }
}

// Returns non-zero if event signalled and not reset yet.
// Note that event may be reset before this function returns, so it is not reliable.
static __attribute__((unused)) inline int mythread_event_is_signaled (mythread_event *evt) {
  if (!evt) return 0;
  mythread_mutex_lock(&evt->mutex);
  const int res = (evt->flags&MYTHREAD_EVENT_SIGNALED ? 1 : 0);
  mythread_mutex_unlock(&evt->mutex);
  return res;
}

// Returns zero if succesfully signaled.
// Already signaled and not reset event will not signal, and non-zero will be returned.
static __attribute__((unused)) inline int mythread_event_signal (mythread_event *evt) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  int res = 1;
  // only set and signal if we are unset
  if (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
    res = 0;
    evt->flags |= MYTHREAD_EVENT_SIGNALED;
    mythread_cond_signal(&evt->cond);
  }
  mythread_mutex_unlock(&evt->mutex);
  return res;
}

// Returns zero if succesfully signaled.
// Already signaled and not reset event will not signal, and non-zero will be returned.
static __attribute__((unused)) inline int mythread_event_signal_broadcast (mythread_event *evt) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  int res = 1;
  // only set and signal if we are unset
  if (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
    res = 0;
    evt->flags |= MYTHREAD_EVENT_SIGNALED;
    mythread_cond_broadcast(&evt->cond);
  }
  mythread_mutex_unlock(&evt->mutex);
  return res;
}

// Returns zero if succesfully signaled.
// Already signaled and not reset event will not signal, and non-zero will be returned.
static __attribute__((unused)) inline int mythread_event_signal_info (mythread_event *evt, int nfo) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  int res = 1;
  // only set and signal if we are unset
  if (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
    res = 0;
    evt->flags |= MYTHREAD_EVENT_SIGNALED;
    evt->info = nfo;
    mythread_cond_signal(&evt->cond);
  }
  mythread_mutex_unlock(&evt->mutex);
  return res;
}

// Returns zero if succesfully signaled.
// Already signaled and not reset event will not signal, and non-zero will be returned.
static __attribute__((unused)) inline int mythread_event_signal_info_broadcast (mythread_event *evt, int nfo) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  int res = 1;
  // only set and signal if we are unset
  if (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
    res = 0;
    evt->flags |= MYTHREAD_EVENT_SIGNALED;
    evt->info = nfo;
    mythread_cond_broadcast(&evt->cond);
  }
  mythread_mutex_unlock(&evt->mutex);
  return res;
}

static __attribute__((unused)) inline void mythread_event_reset (mythread_event *evt) {
  if (!evt) return;
  mythread_mutex_lock(&evt->mutex);
  evt->flags &= ~MYTHREAD_EVENT_SIGNALED;
  mythread_mutex_unlock(&evt->mutex);
}

// Returns zero if wait succeed.
// `outinfo` can be `NULL`, and it is unchanged if wait didn't succeed.
static __attribute__((unused)) inline int mythread_event_wait (mythread_event *evt, int *outinfo) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  while (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) mythread_cond_wait(&evt->cond, &evt->mutex);
  if (outinfo) *outinfo = evt->info;
  if (evt->flags&MYTHREAD_EVENT_AUTORESET) evt->flags &= ~MYTHREAD_EVENT_SIGNALED;
  mythread_mutex_unlock(&evt->mutex);
  return 0;
}

// timeout < 0: infinite
// timeout == 0: no wait
// otherwise -- milliseconds to wait
// Returns zero if wait succeed.
// `outinfo` can be `NULL`, and it is unchanged if wait didn't succeed.
static __attribute__((unused)) inline int mythread_event_wait_timeout (mythread_event *evt, int timeout, int *outinfo) {
  if (!evt) return -1;
  mythread_mutex_lock(&evt->mutex);
  if (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
    if (timeout < 0) {
      while (!(evt->flags&MYTHREAD_EVENT_SIGNALED)) mythread_cond_wait(&evt->cond, &evt->mutex);
    } else {
      mythread_condtime condtime;
      mythread_condtime_set(&condtime, &evt->cond, (uint32_t)timeout);
      int rc = mythread_cond_timedwait(&evt->cond, &evt->mutex, &condtime);
      if (rc != 0 || !(evt->flags&MYTHREAD_EVENT_SIGNALED)) {
        mythread_mutex_unlock(&evt->mutex);
        return 1;
      }
    }
  }
  if (outinfo) *outinfo = evt->info;
  if (evt->flags&MYTHREAD_EVENT_AUTORESET) evt->flags &= ~MYTHREAD_EVENT_SIGNALED;
  mythread_mutex_unlock(&evt->mutex);
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
// non-recursive MREW lock, prefer writers
// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  /*WARNING! NO USER-SERVICEABLE PARTS INSIDE!*/
  atomic_int readers_active;
  atomic_int writers_waiting;
  atomic_int writer_active;
  mythread_mutex lock; /* global lock */
  mythread_cond cond;
} mythread_mrew;

// Returns zero if succesfully created.
static __attribute__((unused)) inline int mythread_mrew_init (mythread_mrew *mrew) {
  if (!mrew) return -1;
  memset((void *)mrew, 0, sizeof(*mrew));
  atomic_store(&mrew->readers_active, 0);
  atomic_store(&mrew->writers_waiting, 0);
  atomic_store(&mrew->writer_active, 0);
  int res = mythread_mutex_init(&mrew->lock);
  if (res) return res;
  res = mythread_cond_init(&mrew->cond);
  if (res) { mythread_mutex_destroy(&mrew->lock); return res; }
  return 0;
}

// Make sure that all readers and writers are gone.
static __attribute__((unused)) inline void mythread_mrew_destroy (mythread_mrew *mrew) {
  if (mrew) {
    mythread_mutex_destroy(&mrew->lock);
    mythread_cond_destroy(&mrew->cond);
    memset((void *)mrew, 0, sizeof(*mrew));
  }
}

static __attribute__((unused)) inline void mythread_mrew_lock_read (mythread_mrew *mrew) {
  if (!mrew) return;
  // make sure we have no writers
  mythread_mutex_lock(&mrew->lock);
  while (atomic_get(&mrew->writers_waiting) || atomic_get(&mrew->writer_active)) {
    mythread_cond_wait(&mrew->cond, &mrew->lock);
  }
  // global mutex is locked here
  (void)atomic_increment(&mrew->readers_active);
  mythread_mutex_unlock(&mrew->lock);
}

static __attribute__((unused)) inline void mythread_mrew_unlock_read (mythread_mrew *mrew) {
  if (!mrew) return;
  mythread_mutex_lock(&mrew->lock);
  if (atomic_decrement(&mrew->readers_active) == 0) mythread_cond_broadcast(&mrew->cond);
  mythread_mutex_unlock(&mrew->lock);
}

static __attribute__((unused)) inline void mythread_mrew_lock_write (mythread_mrew *mrew) {
  if (!mrew) return;
  mythread_mutex_lock(&mrew->lock);
  (void)atomic_increment(&mrew->writers_waiting);
  // global mutex is locked here
  while (atomic_get(&mrew->readers_active) || atomic_get(&mrew->writer_active)) {
    mythread_cond_wait(&mrew->cond, &mrew->lock);
  }
  (void)atomic_decrement(&mrew->writers_waiting);
  atomic_set(&mrew->writer_active, 1);
  mythread_mutex_unlock(&mrew->lock);
}

static __attribute__((unused)) inline void mythread_mrew_unlock_write (mythread_mrew *mrew) {
  if (!mrew) return;
  mythread_mutex_lock(&mrew->lock);
  atomic_set(&mrew->writer_active, 0);
  mythread_cond_broadcast(&mrew->cond);
  mythread_mutex_unlock(&mrew->lock);
}


// ////////////////////////////////////////////////////////////////////////// //
// helper APIs
// ////////////////////////////////////////////////////////////////////////// //
#ifdef __cplusplus
class MyThreadLocker {
public:
  mythread_mutex *mutex;

  inline MyThreadLocker (mythread_mutex *amutex) noexcept : mutex(amutex) { if (mutex) mythread_mutex_lock(mutex); }
  inline ~MyThreadLocker () noexcept { if (mutex) { mythread_mutex_unlock(mutex); mutex = NULL; } }

  // you can one-time reset a lock
  // WARNING! there is no way to re-aquire the lock after a reset!
  // this is mainly so you can tail-call the function that aquires the lock itself
  inline void resetLock () noexcept { if (mutex) { mythread_mutex_unlock(mutex); mutex = NULL; } }

  // no copies!
  MyThreadLocker (const MyThreadLocker &) = delete;
  MyThreadLocker &operator = (const MyThreadLocker &) = delete;
};
#endif


#endif
