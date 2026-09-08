#include "platform.h"

#include <stdio.h>

typedef struct ThreadTestState {
    Mutex mutex;
    int value;
} ThreadTestState;

static void *
thread_main(void *userdata)
{
    ThreadTestState *state = (ThreadTestState *)userdata;

    MutexLock(&state->mutex);
    state->value = 42;
    MutexUnlock(&state->mutex);
    return NULL;
}

int
main(void)
{
    ThreadTestState state = {0};
    Thread thread = {0};

    MutexInit(&state.mutex);
    if(!ThreadStart(&thread, thread_main, &state)) {
        fprintf(stderr, "ThreadStart failed\n");
        return 1;
    }
    ThreadJoin(&thread);

    MutexLock(&state.mutex);
    if(state.value != 42) {
        fprintf(stderr, "thread value mismatch: %d\n", state.value);
        MutexUnlock(&state.mutex);
        return 1;
    }
    MutexUnlock(&state.mutex);

    return 0;
}
