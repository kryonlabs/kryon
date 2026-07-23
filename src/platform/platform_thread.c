#include "platform.h"

#if defined(_WIN32)
#include <stdlib.h>

typedef struct KryWinThreadArgs {
    KryThreadMain fn;
    void *userdata;
} KryWinThreadArgs;

static DWORD WINAPI
kry_win_thread_entry(void *userdata)
{
    KryWinThreadArgs *args = (KryWinThreadArgs *)userdata;
    KryThreadMain fn;
    void *fn_userdata;

    if(args == NULL)
        return 0;
    fn = args->fn;
    fn_userdata = args->userdata;
    free(args);
    if(fn != NULL)
        fn(fn_userdata);
    return 0;
}
#elif defined(KRYON_PLATFORM_NO_THREADS)
#else
#include <unistd.h>
#endif

int
KryThreadStart(KryThread *thread, KryThreadMain fn, void *userdata)
{
    if(thread == NULL || fn == NULL)
        return 0;
#if defined(_WIN32)
    {
        KryWinThreadArgs *args = malloc(sizeof(*args));

        if(args == NULL)
            return 0;
        args->fn = fn;
        args->userdata = userdata;
        thread->handle = CreateThread(NULL, 0, kry_win_thread_entry, args, 0, NULL);
        if(thread->handle == NULL) {
            free(args);
            return 0;
        }
        return 1;
    }
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)userdata;
    return 0;
#else
    return pthread_create(&thread->handle, NULL, fn, userdata) == 0;
#endif
}

void
KryThreadDetach(KryThread *thread)
{
    if(thread == NULL)
        return;
#if defined(_WIN32)
    if(thread->handle != NULL) {
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)thread;
#else
    pthread_detach(thread->handle);
#endif
}

void
KryThreadJoin(KryThread *thread)
{
    if(thread == NULL)
        return;
#if defined(_WIN32)
    if(thread->handle != NULL) {
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)thread;
#else
    pthread_join(thread->handle, NULL);
#endif
}

void
KrySleepSeconds(int seconds)
{
    if(seconds <= 0)
        return;
#if defined(_WIN32)
    Sleep((DWORD)seconds * 1000u);
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)seconds;
#else
    sleep((unsigned int)seconds);
#endif
}

void
KryMutexInit(KryMutex *mutex)
{
    if(mutex == NULL)
        return;
#if defined(_WIN32)
    InitializeSRWLock(&mutex->lock);
#elif defined(KRYON_PLATFORM_NO_THREADS)
    mutex->lock = 0;
#else
    pthread_mutex_init(&mutex->lock, NULL);
#endif
}

void
KryMutexLock(KryMutex *mutex)
{
    if(mutex == NULL)
        return;
#if defined(_WIN32)
    AcquireSRWLockExclusive(&mutex->lock);
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)mutex;
#else
    pthread_mutex_lock(&mutex->lock);
#endif
}

void
KryMutexUnlock(KryMutex *mutex)
{
    if(mutex == NULL)
        return;
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&mutex->lock);
#elif defined(KRYON_PLATFORM_NO_THREADS)
    (void)mutex;
#else
    pthread_mutex_unlock(&mutex->lock);
#endif
}
