#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
#define ANDROID_BUILD 1
#else
#define ANDROID_BUILD 0
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
#elif defined(KRYON_PLATFORM_PLAN9)
#define KRYON_PLATFORM_NO_THREADS 1
#elif defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#define KRYON_PLATFORM_NO_THREADS 1
#else
#include <pthread.h>
#endif

typedef void *(*ThreadMain)(void *userdata);

typedef struct Thread {
#if defined(_WIN32)
    HANDLE handle;
#elif defined(KRYON_PLATFORM_NO_THREADS)
    int handle;
#else
    pthread_t handle;
#endif
} Thread;

typedef struct Mutex {
#if defined(_WIN32)
    SRWLOCK lock;
#elif defined(KRYON_PLATFORM_NO_THREADS)
    int lock;
#else
    pthread_mutex_t lock;
#endif
} Mutex;

#if defined(_WIN32)
#define MUTEX_INIT { SRWLOCK_INIT }
#elif defined(KRYON_PLATFORM_NO_THREADS)
#define MUTEX_INIT { 0 }
#else
#define MUTEX_INIT { PTHREAD_MUTEX_INITIALIZER }
#endif

int ThreadStart(Thread *thread, ThreadMain fn, void *userdata);
void ThreadDetach(Thread *thread);
void ThreadJoin(Thread *thread);
void SleepSeconds(int seconds);
void MutexInit(Mutex *mutex);
void MutexLock(Mutex *mutex);
void MutexUnlock(Mutex *mutex);

#endif
