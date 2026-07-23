#include "platform.h"

#include <stdio.h>

typedef struct ThreadTestState {
    KryMutex mutex;
    int value;
} ThreadTestState;

static void *
thread_main(void *userdata)
{
    ThreadTestState *state = (ThreadTestState *)userdata;

    KryMutexLock(&state->mutex);
    state->value = 42;
    KryMutexUnlock(&state->mutex);
    return NULL;
}

int
main(void)
{
    ThreadTestState state = {0};
    KryThread thread = {0};

    KryMutexInit(&state.mutex);
    if(!KryThreadStart(&thread, thread_main, &state)) {
        fprintf(stderr, "KryThreadStart failed\n");
        return 1;
    }
    KryThreadJoin(&thread);

    KryMutexLock(&state.mutex);
    if(state.value != 42) {
        fprintf(stderr, "thread value mismatch: %d\n", state.value);
        KryMutexUnlock(&state.mutex);
        return 1;
    }
    KryMutexUnlock(&state.mutex);

    return 0;
}
