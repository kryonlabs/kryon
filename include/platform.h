#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
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
#include <windows.h>
#elif defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#define KRYON_PLATFORM_NO_THREADS 1
#else
#include <pthread.h>
#endif

typedef void *(*KryThreadMain)(void *userdata);

typedef struct KryThread {
#if defined(_WIN32)
    HANDLE handle;
#elif defined(KRYON_PLATFORM_NO_THREADS)
    int handle;
#else
    pthread_t handle;
#endif
} KryThread;

typedef struct KryMutex {
#if defined(_WIN32)
    SRWLOCK lock;
#elif defined(KRYON_PLATFORM_NO_THREADS)
    int lock;
#else
    pthread_mutex_t lock;
#endif
} KryMutex;

#if defined(_WIN32)
#define KRY_MUTEX_INIT { SRWLOCK_INIT }
#elif defined(KRYON_PLATFORM_NO_THREADS)
#define KRY_MUTEX_INIT { 0 }
#else
#define KRY_MUTEX_INIT { PTHREAD_MUTEX_INITIALIZER }
#endif

int KryThreadStart(KryThread *thread, KryThreadMain fn, void *userdata);
void KryThreadDetach(KryThread *thread);
void KryThreadJoin(KryThread *thread);
void KrySleepSeconds(int seconds);
void KryMutexInit(KryMutex *mutex);
void KryMutexLock(KryMutex *mutex);
void KryMutexUnlock(KryMutex *mutex);

#endif
