/*
 * Kryon Authentication - Session Management
 * C89/C90 compliant
 */

#include "devfactotum.h"
#include "auth_p9any.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Global session table
 */
static AuthSession *g_sessions[MAX_AUTH_SESSIONS];
static int g_session_count = 0;
static int g_session_initialized = 0;

/*
 * Initialize session tracking
 */
int auth_session_init(void)
{
    int i;

    if (g_session_initialized) {
        return 0;
    }

    for (i = 0; i < MAX_AUTH_SESSIONS; i++) {
        g_sessions[i] = NULL;
    }

    g_session_count = 0;
    g_session_initialized = 1;

    return 0;
}

/*
 * Find free session slot
 */
static int find_free_slot(void)
{
    int i;

    for (i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (g_sessions[i] == NULL) {
            return i;
        }
    }

    return -1;
}

/*
 * Find session by client fd
 */
static int find_session_by_fd(int client_fd)
{
    int i;

    for (i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (g_sessions[i] != NULL && g_sessions[i]->client_fd == client_fd) {
            return i;
        }
    }

    return -1;
}

/*
 * Create new session for client fd
 */
AuthSession *auth_session_new(int client_fd)
{
    AuthSession *sess;
    int slot;

    if (!g_session_initialized) {
        fprintf(stderr, "auth_session_new: sessions not initialized\n");
        return NULL;
    }

    /* Check if session already exists for this fd */
    slot = find_session_by_fd(client_fd);
    if (slot >= 0) {
        fprintf(stderr, "auth_session_new: session already exists for fd=%d\n",
                client_fd);
        return g_sessions[slot];
    }

    /* Find free slot */
    slot = find_free_slot();
    if (slot < 0) {
        fprintf(stderr, "auth_session_new: no free session slots\n");
        return NULL;
    }

    /* Allocate session */
    sess = (AuthSession *)malloc(sizeof(AuthSession));
    if (sess == NULL) {
        fprintf(stderr, "auth_session_new: malloc failed\n");
        return NULL;
    }

    memset(sess, 0, sizeof(AuthSession));

    sess->client_fd = client_fd;
    sess->proto_type = AUTH_PROTO_NONE;
    sess->proto_state.p9any = NULL;
    sess->ai = NULL;
    sess->state = 0;
    sess->start_time = time(NULL);

    g_sessions[slot] = sess;
    g_session_count++;

    return sess;
}

/*
 * Get session for client fd
 */
AuthSession *auth_session_get(int client_fd)
{
    int slot;

    if (!g_session_initialized) {
        return NULL;
    }

    slot = find_session_by_fd(client_fd);
    if (slot < 0) {
        return NULL;
    }

    return g_sessions[slot];
}

/*
 * Delete session for client fd
 */
void auth_session_delete(int client_fd)
{
    int slot;

    if (!g_session_initialized) {
        return;
    }

    slot = find_session_by_fd(client_fd);
    if (slot < 0) {
        return;
    }

    /* Free protocol-specific state */
    if (g_sessions[slot]->proto_type == AUTH_PROTO_P9ANY) {
        if (g_sessions[slot]->proto_state.p9any != NULL) {
            p9any_session_free(g_sessions[slot]->proto_state.p9any);
        }
    }

    /* Free auth info */
    if (g_sessions[slot]->ai != NULL) {
        if (g_sessions[slot]->ai->suid != NULL) {
            free(g_sessions[slot]->ai->suid);
        }
        if (g_sessions[slot]->ai->cuid != NULL) {
            free(g_sessions[slot]->ai->cuid);
        }
        if (g_sessions[slot]->ai->secret != NULL) {
            /* Zero out secret before freeing */
            memset(g_sessions[slot]->ai->secret, 0,
                   g_sessions[slot]->ai->nsecret);
            free(g_sessions[slot]->ai->secret);
        }
        free(g_sessions[slot]->ai);
    }

    /* Free session */
    free(g_sessions[slot]);
    g_sessions[slot] = NULL;
    g_session_count--;
}

/*
 * Cleanup all sessions (on shutdown)
 */
void auth_session_cleanup(void)
{
    int i;

    if (!g_session_initialized) {
        return;
    }

    for (i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (g_sessions[i] != NULL) {
            auth_session_delete(g_sessions[i]->client_fd);
        }
    }

    g_session_initialized = 0;
}

/*
 * Check for session timeouts (call periodically)
 * Timeout: 30 seconds
 */
void auth_session_check_timeouts(void)
{
    int i;
    time_t now;
    double elapsed;

    if (!g_session_initialized) {
        return;
    }

    now = time(NULL);

    for (i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (g_sessions[i] == NULL) {
            continue;
        }

        elapsed = difftime(now, g_sessions[i]->start_time);

        if (elapsed > 30.0) {
            fprintf(stderr, "auth_session: timeout for fd=%d\n",
                    g_sessions[i]->client_fd);
            auth_session_delete(g_sessions[i]->client_fd);
        }
    }
}
