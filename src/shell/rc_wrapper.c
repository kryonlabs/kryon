/*
 * Kryon RC Shell Wrapper - Plan 9 RC Shell Integration
 * C89/C90 compliant
 */

#include "kryon.h"
#include "rc_wrapper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>

/*
 * Maximum number of RC shells
 */
#define MAX_RCSHELLS 16

/*
 * RC shell states
 */
static RCState g_rc_states[MAX_RCSHELLS];
static int g_nrshells = 0;

/*
 * Environment variables storage
 */
static char g_env_names[RC_MAX_ENV][RC_MAX_ENV_LEN];
static char g_env_values[RC_MAX_ENV][RC_MAX_ENV_LEN];
static int g_nenv = 0;

/*
 * Find plan9port installation
 */
const char *rc_find_plan9(void)
{
    static char path[512] = "";
    DIR *dir;
    struct dirent *entry;

    /* Check cached result */
    if (path[0] != '\0') {
        return path;
    }

    /* Check environment variable */
    {
        const char *env_path = getenv("PLAN9");
        if (env_path != NULL && access(env_path, F_OK) == 0) {
            strncpy(path, env_path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
            fprintf(stderr, "rc_find_plan9: found via PLAN9=%s\n", path);
            return path;
        }
    }

    /* Search Nix store for plan9port */
    dir = opendir("/nix/store");
    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char full_path[512];

            /* Check if directory name contains "plan9" */
            if (strstr(entry->d_name, "plan9") == NULL) {
                continue;
            }

            /* Build full path */
            snprintf(full_path, sizeof(full_path), "/nix/store/%s", entry->d_name);

            /* Check if it contains bin/rc */
            snprintf(path, sizeof(path), "%s/bin/rc", full_path);
            if (access(path, F_OK | X_OK) == 0) {
                /* Found it - extract base path */
                snprintf(path, sizeof(path), "%s", full_path);
                closedir(dir);
                fprintf(stderr, "rc_find_plan9: found in Nix store: %s\n", path);
                return path;
            }
        }
        closedir(dir);
    }

    /* Check common locations */
    {
        const char *candidates[] = {
            "/usr/local/plan9",
            "/opt/plan9",
            NULL
        };
        int i;

        for (i = 0; candidates[i] != NULL; i++) {
            if (access(candidates[i], F_OK) == 0) {
                strncpy(path, candidates[i], sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
                fprintf(stderr, "rc_find_plan9: found at %s\n", path);
                return path;
            }
        }
    }

    fprintf(stderr, "rc_find_plan9: plan9port not found\n");
    return NULL;
}

/*
 * Initialize RC wrapper
 */
int rc_wrapper_init(void)
{
    int i;

    /* Initialize all RC states */
    for (i = 0; i < MAX_RCSHELLS; i++) {
        g_rc_states[i].pid = -1;
        g_rc_states[i].stdin_fd = -1;
        g_rc_states[i].stdout_fd = -1;
        g_rc_states[i].stderr_fd = -1;
        g_rc_states[i].active = 0;
        g_rc_states[i].user[0] = '\0';
        g_rc_states[i].plan9_path[0] = '\0';
    }

    g_nrshells = 0;

    /* Initialize default environment */
    g_nenv = 0;
    rc_set_env("service", "cpu");
    rc_set_env("PATH", "/bin:/usr/bin");

    fprintf(stderr, "rc_wrapper_init: initialized\n");

    return 0;
}

/*
 * Cleanup RC wrapper
 */
void rc_wrapper_cleanup(void)
{
    int i;

    /* Stop all RC shells */
    for (i = 0; i < MAX_RCSHELLS; i++) {
        if (g_rc_states[i].active) {
            rc_stop(g_rc_states[i].pid);
        }
    }

    g_nrshells = 0;
}

/*
 * Find free RC shell slot
 */
static int find_free_rc_slot(void)
{
    int i;

    for (i = 0; i < MAX_RCSHELLS; i++) {
        if (!g_rc_states[i].active) {
            return i;
        }
    }

    return -1;
}

/*
 * Find RC shell by PID
 */
static RCState *find_rc_by_pid(pid_t pid)
{
    int i;

    for (i = 0; i < MAX_RCSHELLS; i++) {
        if (g_rc_states[i].active && g_rc_states[i].pid == pid) {
            return &g_rc_states[i];
        }
    }

    return NULL;
}

/*
 * Start RC shell with CPU server environment
 */
pid_t rc_start_cpu(const char *user, P9Node *mnt_term)
{
    RCState *state;
    pid_t pid;
    const char *plan9;
    char rc_path[512];
    int slot;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    (void)mnt_term;  /* May be used later for namespace setup */

    /* Find plan9port */
    plan9 = rc_find_plan9();
    if (plan9 == NULL) {
        fprintf(stderr, "rc_start_cpu: plan9port not found\n");
        return -1;
    }

    /* Find free slot */
    slot = find_free_rc_slot();
    if (slot < 0) {
        fprintf(stderr, "rc_start_cpu: no free slots\n");
        return -1;
    }

    state = &g_rc_states[slot];

    /* Build path to rc binary */
    snprintf(rc_path, sizeof(rc_path), "%s/bin/rc", plan9);

    /* Create pipes */
    if (pipe(stdin_pipe) < 0) {
        fprintf(stderr, "rc_start_cpu: pipe(stdin) failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (pipe(stdout_pipe) < 0) {
        fprintf(stderr, "rc_start_cpu: pipe(stdout) failed: %s\n",
                strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return -1;
    }

    if (pipe(stderr_pipe) < 0) {
        fprintf(stderr, "rc_start_cpu: pipe(stderr) failed: %s\n",
                strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return -1;
    }

    /* Fork and exec */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "rc_start_cpu: fork failed: %s\n", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process */

        /* Close pipe ends we don't need */
        close(stdin_pipe[1]);   /* Close write end of stdin */
        close(stdout_pipe[0]);  /* Close read end of stdout */
        close(stderr_pipe[0]);  /* Close read end of stderr */

        /* Dup pipes to stdin/stdout/stderr */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        /* Close original pipe fds */
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Set environment variables */
        setenv("service", "cpu", 1);
        setenv("PATH", "/bin:/usr/bin", 1);
        setenv("PLAN9", plan9, 1);

        if (user != NULL) {
            setenv("user", user, 1);
        }

        /* Exec rc shell */
        execl(rc_path, "rc", "-i", NULL);

        /* If we get here, exec failed */
        fprintf(stderr, "rc_start_cpu: exec(%s) failed: %s\n",
                rc_path, strerror(errno));
        exit(1);
    }

    /* Parent process */

    /* Close pipe ends we don't need */
    close(stdin_pipe[0]);   /* Close read end of stdin */
    close(stdout_pipe[1]);  /* Close write end of stdout */
    close(stderr_pipe[1]);  /* Close write end of stderr */

    /* Initialize state */
    memset(state, 0, sizeof(RCState));
    state->pid = pid;
    state->stdin_fd = stdin_pipe[1];
    state->stdout_fd = stdout_pipe[0];
    state->stderr_fd = stderr_pipe[0];
    state->active = 1;
    strncpy(state->plan9_path, plan9, sizeof(state->plan9_path) - 1);
    state->plan9_path[sizeof(state->plan9_path) - 1] = '\0';

    if (user != NULL) {
        strncpy(state->user, user, sizeof(state->user) - 1);
        state->user[sizeof(state->user) - 1] = '\0';
    }

    g_nrshells++;

    fprintf(stderr, "rc_start_cpu: started rc (pid=%d, user=%s)\n",
            pid, user ? user : "(none)");

    return pid;
}

/*
 * Stop RC shell
 */
int rc_stop(pid_t pid)
{
    RCState *state;

    state = find_rc_by_pid(pid);
    if (state == NULL) {
        fprintf(stderr, "rc_stop: pid %d not found\n", pid);
        return -1;
    }

    fprintf(stderr, "rc_stop: stopping rc (pid=%d)\n", pid);

    /* Close pipes */
    if (state->stdin_fd >= 0) {
        close(state->stdin_fd);
        state->stdin_fd = -1;
    }

    if (state->stdout_fd >= 0) {
        close(state->stdout_fd);
        state->stdout_fd = -1;
    }

    if (state->stderr_fd >= 0) {
        close(state->stderr_fd);
        state->stderr_fd = -1;
    }

    /* Kill process */
    if (state->pid > 0) {
        kill(state->pid, SIGTERM);
        state->pid = -1;
    }

    state->active = 0;
    g_nrshells--;

    return 0;
}

/*
 * Check if RC shell is running
 */
int rc_is_running(pid_t pid)
{
    RCState *state;
    int status;

    state = find_rc_by_pid(pid);
    if (state == NULL) {
        return 0;
    }

    /* Check if process is still alive */
    if (waitpid(state->pid, &status, WNOHANG) == state->pid) {
        /* Process has exited */
        state->active = 0;
        return 0;
    }

    return state->active;
}

/*
 * Write to RC shell's stdin
 */
ssize_t rc_write_stdin(pid_t pid, const char *buf, size_t count)
{
    RCState *state;
    ssize_t n;

    state = find_rc_by_pid(pid);
    if (state == NULL || !state->active) {
        return -1;
    }

    if (state->stdin_fd < 0) {
        return -1;
    }

    n = write(state->stdin_fd, buf, count);
    if (n < 0) {
        fprintf(stderr, "rc_write_stdin: write failed: %s\n",
                strerror(errno));
        return -1;
    }

    return n;
}

/*
 * Read from RC shell's stdout
 */
ssize_t rc_read_stdout(pid_t pid, char *buf, size_t count)
{
    RCState *state;
    ssize_t n;

    state = find_rc_by_pid(pid);
    if (state == NULL || !state->active) {
        return -1;
    }

    if (state->stdout_fd < 0) {
        return -1;
    }

    n = read(state->stdout_fd, buf, count);
    if (n < 0) {
        fprintf(stderr, "rc_read_stdout: read failed: %s\n",
                strerror(errno));
        return -1;
    }

    return n;
}

/*
 * Read from RC shell's stderr
 */
ssize_t rc_read_stderr(pid_t pid, char *buf, size_t count)
{
    RCState *state;
    ssize_t n;

    state = find_rc_by_pid(pid);
    if (state == NULL || !state->active) {
        return -1;
    }

    if (state->stderr_fd < 0) {
        return -1;
    }

    n = read(state->stderr_fd, buf, count);
    if (n < 0) {
        fprintf(stderr, "rc_read_stderr: read failed: %s\n",
                strerror(errno));
        return -1;
    }

    return n;
}

/*
 * Set environment variable
 */
int rc_set_env(const char *name, const char *value)
{
    int i;

    if (name == NULL || value == NULL) {
        return -1;
    }

    /* Check if variable already exists */
    for (i = 0; i < g_nenv; i++) {
        if (strcmp(g_env_names[i], name) == 0) {
            /* Update existing */
            strncpy(g_env_values[i], value, RC_MAX_ENV_LEN - 1);
            g_env_values[i][RC_MAX_ENV_LEN - 1] = '\0';
            return 0;
        }
    }

    /* Add new variable */
    if (g_nenv >= RC_MAX_ENV) {
        fprintf(stderr, "rc_set_env: too many environment variables\n");
        return -1;
    }

    strncpy(g_env_names[g_nenv], name, RC_MAX_ENV_LEN - 1);
    g_env_names[g_nenv][RC_MAX_ENV_LEN - 1] = '\0';

    strncpy(g_env_values[g_nenv], value, RC_MAX_ENV_LEN - 1);
    g_env_values[g_nenv][RC_MAX_ENV_LEN - 1] = '\0';

    g_nenv++;

    return 0;
}

/*
 * Get environment variable value
 */
const char *rc_get_env(const char *name)
{
    int i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < g_nenv; i++) {
        if (strcmp(g_env_names[i], name) == 0) {
            return g_env_values[i];
        }
    }

    return NULL;
}
