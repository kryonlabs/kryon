/*
 * Kryon CPU Server - Plan 9 CPU Server Implementation
 * C89/C90 compliant
 */

#include "kryon.h"
#include "cpu_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>

/*
 * Global CPU server state
 */
static CPUSession g_sessions[MAX_CPU_CLIENTS];
static int g_nsessions = 0;
static P9Node *g_root = NULL;

/*
 * Signal handler for child processes
 */
static void child_sig_handler(int sig)
{
    pid_t pid;
    int status;

    (void)sig;

    /* Reap zombie children */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int i;
        fprintf(stderr, "cpu_server: child %d exited (status=%d)\n", pid, status);

        /* Find and close corresponding session */
        for (i = 0; i < MAX_CPU_CLIENTS; i++) {
            if (g_sessions[i].active && g_sessions[i].rc_pid == pid) {
                fprintf(stderr, "cpu_server: closing session %d for pid %d\n",
                        i, pid);
                cpu_close_session(i);
                break;
            }
        }
    }
}

/*
 * Initialize CPU server subsystem
 */
int cpu_server_init(P9Node *root)
{
    int i;

    if (root == NULL) {
        fprintf(stderr, "cpu_server_init: root is NULL\n");
        return -1;
    }

    g_root = root;

    /* Initialize all sessions */
    for (i = 0; i < MAX_CPU_CLIENTS; i++) {
        g_sessions[i].client_id = -1;
        g_sessions[i].active = 0;
        g_sessions[i].mnt_term = NULL;
        g_sessions[i].mnt_term_dev = NULL;
        g_sessions[i].mnt_term_env = NULL;
        g_sessions[i].rc_pid = -1;
        g_sessions[i].rc_stdin = -1;
        g_sessions[i].rc_stdout = -1;
        g_sessions[i].rc_stderr = -1;
        g_sessions[i].user[0] = '\0';
        g_sessions[i].aname[0] = '\0';
    }

    g_nsessions = 0;

    /* Setup signal handler for child processes */
    signal(SIGCHLD, child_sig_handler);

    fprintf(stderr, "cpu_server_init: initialized (max clients=%d)\n",
            MAX_CPU_CLIENTS);

    return 0;
}

/*
 * Cleanup CPU server subsystem
 */
void cpu_server_cleanup(void)
{
    int i;

    /* Close all active sessions */
    for (i = 0; i < MAX_CPU_CLIENTS; i++) {
        if (g_sessions[i].active) {
            cpu_close_session(i);
        }
    }

    g_nsessions = 0;
    g_root = NULL;
}

/*
 * Find a free session slot
 */
static int find_free_session(void)
{
    int i;

    for (i = 0; i < MAX_CPU_CLIENTS; i++) {
        if (!g_sessions[i].active) {
            return i;
        }
    }

    return -1;
}

/*
 * Find plan9port installation path
 */
const char *cpu_find_plan9_path(void)
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
            fprintf(stderr, "cpu_find_plan9_path: found via PLAN9=%s\n", path);
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
                fprintf(stderr, "cpu_find_plan9_path: found in Nix store: %s\n", path);
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
                fprintf(stderr, "cpu_find_plan9_path: found at %s\n", path);
                return path;
            }
        }
    }

    fprintf(stderr, "cpu_find_plan9_path: plan9port not found\n");
    return NULL;
}

/*
 * Create /mnt/term structure for a client
 */
P9Node *cpu_create_mnt_term(P9Node *root, int client_id)
{
    P9Node *mnt_node;
    P9Node *term_node;
    P9Node *dev_node;
    P9Node *env_node;
    char dirname[64];

    if (root == NULL) {
        fprintf(stderr, "cpu_create_mnt_term: root is NULL\n");
        return NULL;
    }

    /* Create /mnt if it doesn't exist */
    mnt_node = tree_walk(root, "mnt");
    if (mnt_node == NULL) {
        mnt_node = tree_create_dir(root, "mnt");
        if (mnt_node == NULL) {
            fprintf(stderr, "cpu_create_mnt_term: failed to create /mnt\n");
            return NULL;
        }
    }

    /* Create /mnt/term if it doesn't exist */
    term_node = tree_walk(mnt_node, "term");
    if (term_node == NULL) {
        term_node = tree_create_dir(mnt_node, "term");
        if (term_node == NULL) {
            fprintf(stderr, "cpu_create_mnt_term: failed to create /mnt/term\n");
            return NULL;
        }
    }

    /* Create /mnt/term/[client_id] directory */
    snprintf(dirname, sizeof(dirname), "%d", client_id);
    term_node = tree_create_dir(term_node, dirname);
    if (term_node == NULL) {
        fprintf(stderr, "cpu_create_mnt_term: failed to create /mnt/term/%s\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/dev directory */
    dev_node = tree_create_dir(term_node, "dev");
    if (dev_node == NULL) {
        fprintf(stderr, "cpu_create_mnt_term: failed to create /mnt/term/%s/dev\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/env directory */
    env_node = tree_create_dir(term_node, "env");
    if (env_node == NULL) {
        fprintf(stderr, "cpu_create_mnt_term: failed to create /mnt/term/%s/env\n",
                dirname);
        return NULL;
    }

    fprintf(stderr, "cpu_create_mnt_term: created /mnt/term/%s\n", dirname);

    return term_node;
}

/*
 * Start RC shell for a CPU session
 */
static pid_t start_rc_shell(CPUSession *session)
{
    pid_t pid;
    const char *plan9;
    char rc_path[512];
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (session == NULL) {
        return -1;
    }

    /* Find plan9port */
    plan9 = cpu_find_plan9_path();
    if (plan9 == NULL) {
        fprintf(stderr, "start_rc_shell: plan9port not found\n");
        return -1;
    }

    /* Build path to rc binary */
    snprintf(rc_path, sizeof(rc_path), "%s/bin/rc", plan9);

    /* Create pipes for stdin, stdout, stderr */
    if (pipe(stdin_pipe) < 0) {
        fprintf(stderr, "start_rc_shell: pipe(stdin) failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (pipe(stdout_pipe) < 0) {
        fprintf(stderr, "start_rc_shell: pipe(stdout) failed: %s\n",
                strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return -1;
    }

    if (pipe(stderr_pipe) < 0) {
        fprintf(stderr, "start_rc_shell: pipe(stderr) failed: %s\n",
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
        fprintf(stderr, "start_rc_shell: fork failed: %s\n", strerror(errno));
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

        /* Set PLAN9 if found */
        setenv("PLAN9", plan9, 1);

        /* Exec rc shell */
        execl(rc_path, "rc", "-i", NULL);

        /* If we get here, exec failed */
        fprintf(stderr, "start_rc_shell: exec(%s) failed: %s\n",
                rc_path, strerror(errno));
        exit(1);
    }

    /* Parent process */

    /* Close pipe ends we don't need */
    close(stdin_pipe[0]);   /* Close read end of stdin */
    close(stdout_pipe[1]);  /* Close write end of stdout */
    close(stderr_pipe[1]);  /* Close write end of stderr */

    /* Store pipe fds */
    session->rc_stdin = stdin_pipe[1];    /* Write to stdin */
    session->rc_stdout = stdout_pipe[0];  /* Read from stdout */
    session->rc_stderr = stderr_pipe[0];  /* Read from stderr */

    fprintf(stderr, "start_rc_shell: started rc (pid=%d, path=%s)\n",
            pid, rc_path);

    return pid;
}

/*
 * Handle new CPU client connection
 */
int cpu_handle_new_client(int client_fd, const char *user, const char *aname)
{
    int session_id;
    CPUSession *session;
    P9Node *mnt_term;
    pid_t rc_pid;

    (void)client_fd;  /* May be used later for fd tracking */

    /* Find free session slot */
    session_id = find_free_session();
    if (session_id < 0) {
        fprintf(stderr, "cpu_handle_new_client: no free session slots\n");
        return -1;
    }

    session = &g_sessions[session_id];

    /* Initialize session */
    memset(session, 0, sizeof(CPUSession));
    session->client_id = session_id;
    session->active = 1;
    session->rc_pid = -1;
    session->rc_stdin = -1;
    session->rc_stdout = -1;
    session->rc_stderr = -1;

    /* Copy user and aname */
    if (user != NULL) {
        strncpy(session->user, user, sizeof(session->user) - 1);
        session->user[sizeof(session->user) - 1] = '\0';
    }

    if (aname != NULL) {
        strncpy(session->aname, aname, sizeof(session->aname) - 1);
        session->aname[sizeof(session->aname) - 1] = '\0';
    }

    /* Create /mnt/term structure */
    mnt_term = cpu_create_mnt_term(g_root, session_id);
    if (mnt_term == NULL) {
        fprintf(stderr, "cpu_handle_new_client: failed to create /mnt/term\n");
        session->active = 0;
        return -1;
    }

    session->mnt_term = mnt_term;

    /* Find /mnt/term/dev and /mnt/term/env */
    session->mnt_term_dev = tree_walk(mnt_term, "dev");
    session->mnt_term_env = tree_walk(mnt_term, "env");

    /* Start RC shell */
    rc_pid = start_rc_shell(session);
    if (rc_pid < 0) {
        fprintf(stderr, "cpu_handle_new_client: failed to start rc shell\n");
        cpu_close_session(session_id);
        return -1;
    }

    session->rc_pid = rc_pid;

    g_nsessions++;

    fprintf(stderr, "cpu_handle_new_client: created session %d for user '%s' (rc pid=%d)\n",
            session_id, session->user, rc_pid);

    return session_id;
}

/*
 * Get CPU session by client ID
 */
CPUSession *cpu_get_session(int client_id)
{
    if (client_id < 0 || client_id >= MAX_CPU_CLIENTS) {
        return NULL;
    }

    if (!g_sessions[client_id].active) {
        return NULL;
    }

    return &g_sessions[client_id];
}

/*
 * Get CPU session by file descriptor (placeholder)
 * In a real implementation, we'd track fd->session mappings
 */
CPUSession *cpu_get_session_by_fd(int fd)
{
    /* For now, return first active session */
    /* TODO: Implement proper fd tracking */
    int i;

    (void)fd;

    for (i = 0; i < MAX_CPU_CLIENTS; i++) {
        if (g_sessions[i].active) {
            return &g_sessions[i];
        }
    }

    return NULL;
}

/*
 * Close and cleanup a CPU session
 */
void cpu_close_session(int client_id)
{
    CPUSession *session;

    if (client_id < 0 || client_id >= MAX_CPU_CLIENTS) {
        return;
    }

    session = &g_sessions[client_id];

    if (!session->active) {
        return;
    }

    fprintf(stderr, "cpu_close_session: closing session %d\n", client_id);

    /* Close pipes */
    if (session->rc_stdin >= 0) {
        close(session->rc_stdin);
        session->rc_stdin = -1;
    }

    if (session->rc_stdout >= 0) {
        close(session->rc_stdout);
        session->rc_stdout = -1;
    }

    if (session->rc_stderr >= 0) {
        close(session->rc_stderr);
        session->rc_stderr = -1;
    }

    /* Kill RC shell if still running */
    if (session->rc_pid > 0) {
        kill(session->rc_pid, SIGTERM);
        session->rc_pid = -1;
    }

    /* TODO: Cleanup /mnt/term structure */

    session->active = 0;
    g_nsessions--;

    fprintf(stderr, "cpu_close_session: session %d closed (remaining=%d)\n",
            client_id, g_nsessions);
}

/*
 * Write to client's /mnt/term/dev/cons (for RC shell output)
 */
int cpu_write_cons(int client_id, const char *buf, size_t count)
{
    CPUSession *session;
    ssize_t n;

    session = cpu_get_session(client_id);
    if (session == NULL) {
        return -1;
    }

    /* Write to stdin of RC shell */
    if (session->rc_stdin < 0) {
        return -1;
    }

    n = write(session->rc_stdin, buf, count);
    if (n < 0) {
        fprintf(stderr, "cpu_write_cons: write failed: %s\n", strerror(errno));
        return -1;
    }

    return (int)n;
}

/*
 * Read from client's /mnt/term/dev/cons (for keyboard input)
 */
int cpu_read_cons(int client_id, char *buf, size_t count)
{
    CPUSession *session;
    ssize_t n;

    session = cpu_get_session(client_id);
    if (session == NULL) {
        return -1;
    }

    /* Read from stdout of RC shell */
    if (session->rc_stdout < 0) {
        return -1;
    }

    n = read(session->rc_stdout, buf, count);
    if (n < 0) {
        fprintf(stderr, "cpu_read_cons: read failed: %s\n", strerror(errno));
        return -1;
    }

    return (int)n;
}

/*
 * Check if a connection is a CPU client
 */
int cpu_is_cpu_client(int client_fd)
{
    CPUSession *session;

    (void)client_fd;

    session = cpu_get_session_by_fd(client_fd);
    return (session != NULL) ? 1 : 0;
}
