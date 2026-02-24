/*
 * Kryon 9P Server Test Suite
 * C89/C90 compliant
 */

#include "p9client.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Network includes for fork/waitpid/signal/sleep */
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define sleep(n) Sleep((n) * 1000)
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
typedef int pid_t;
#endif

/* Test configuration */
#define TEST_PORT 17020
#define TEST_HOST "127.0.0.1"
#define MAX_WAIT_RETRIES 15
#define WAIT_RETRY_DELAY 1

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("\n--- Test %d: %s ---\n", tests_run, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return 0; \
        } \
    } while(0)

/* Test function declarations */
static int test_root_listing(P9Client *client);
static int test_version_file(P9Client *client);
static int test_dev_listing(P9Client *client);
static int test_read_dev_cons(P9Client *client);

/* Server management */
static int start_server(void);
static void stop_server(int pid);
static int wait_for_server(const char *host, int port);

int main(void) {
    P9Client *client;
    int server_pid;

    printf("========================================\n");
    printf("Kryon 9P Server Test Suite\n");
    printf("========================================\n");
    printf("Test port: %d\n", TEST_PORT);
    printf("Test host: %s\n", TEST_HOST);
    fflush(stdout);

    /* Kill any existing servers on test port */
    system("pkill -9 'kryon-server --port 17020' 2>/dev/null");
    sleep(1);

    /* Start isolated server */
    server_pid = start_server();
    if (server_pid < 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    printf("Server PID: %d\n", server_pid);
    fflush(stdout);

    /* Wait for server to be ready */
    if (wait_for_server(TEST_HOST, TEST_PORT) < 0) {
        fprintf(stderr, "Server did not become ready\n");
        stop_server(server_pid);
        return 1;
    }
    printf("Server is ready\n");

    /* Connect to server */
    client = p9_client_connect(TEST_HOST, TEST_PORT);
    if (!client) {
        fprintf(stderr, "Failed to connect to server\n");
        stop_server(server_pid);
        return 1;
    }
    printf("Connected to server\n");
    fflush(stdout);

    /* Attach to root */
    printf("Attaching to root...\n");
    fflush(stdout);
    if (p9_client_attach(client, "") < 0) {
        fprintf(stderr, "Failed to attach: %s\n", p9_client_error(client));
        p9_client_disconnect(client);
        stop_server(server_pid);
        return 1;
    }
    printf("Attached to root\n");

    /* Run tests */
    printf("\n========================================\n");
    printf("Running Tests\n");
    printf("========================================\n");

    test_root_listing(client);
    test_version_file(client);
    test_dev_listing(client);
    test_read_dev_cons(client);

    /* Cleanup */
    p9_client_disconnect(client);
    stop_server(server_pid);

    /* Print summary */
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("\n");

    if (tests_failed == 0) {
        printf("=== All tests passed ===\n");
        return 0;
    } else {
        printf("=== Some tests failed ===\n");
        return 1;
    }
}

/* Test implementations using p9client.h API */

static int test_root_listing(P9Client *client) {
    int fid;
    char buf[8192];
    ssize_t n;

    TEST_START("Root directory listing");

    fid = p9_client_open_path(client, "/", P9_OREAD);
    ASSERT(fid >= 0, "Failed to open root directory");

    n = p9_client_read(client, fid, buf, sizeof(buf) - 1);
    ASSERT(n > 0, "Failed to read root directory");

    printf("Read %ld bytes from root directory\n", (long)n);

    p9_client_clunk(client, fid);
    TEST_PASS();
    return 1;
}

static int test_version_file(P9Client *client) {
    char *version;
    size_t size;

    TEST_START("Read /mnt/wm/version file");

    version = p9_client_read_file(client, "/mnt/wm/version", &size);
    ASSERT(version != NULL, "Failed to read version file");
    ASSERT(size > 0, "Version file is empty");

    /* Null terminate for printing */
    if (size > 0) {
        printf("Version: %.*s\n", (int)size, version);
    }

    ASSERT(strstr(version, "Kryon") != NULL, "Version string invalid");

    free(version);
    TEST_PASS();
    return 1;
}

static int test_dev_listing(P9Client *client) {
    int fid;
    char buf[8192];
    ssize_t n;

    TEST_START("/dev directory listing");

    fid = p9_client_open_path(client, "/dev", P9_OREAD);
    ASSERT(fid >= 0, "Failed to open /dev directory");

    n = p9_client_read(client, fid, buf, sizeof(buf) - 1);
    ASSERT(n > 0, "Failed to read /dev directory");

    printf("Read %ld bytes from /dev directory\n", (long)n);

    TEST_PASS();
    return 1;
}

static int test_read_dev_cons(P9Client *client) {
    int fid;
    char buf[256];
    ssize_t n;

    TEST_START("Read /dev/cons (should be empty initially)");

    fid = p9_client_open_path(client, "/dev/cons", P9_OREAD);
    ASSERT(fid >= 0, "Failed to open /dev/cons");

    n = p9_client_read(client, fid, buf, sizeof(buf));
    ASSERT(n >= 0, "Failed to read /dev/cons");

    printf("Read %ld bytes from /dev/cons\n", (long)n);

    TEST_PASS();
    return 1;
}

/* Server management using fork/exec */

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static int start_server(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process: start server using shell */
        /* Close stdin/stdout/stderr to avoid interference */
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
        /* Kill any existing server on port 17020 first */
        (void)system("pkill -9 'kryon-server --port 17020' 2>/dev/null");
        sleep(1); /* Give pkill time to work */
        execl("/bin/sh", "sh", "-c", "./bin/kryon-server --port 17020", (char *)NULL);
        exit(1);
    }

    /* Parent process: wait for server to start */
    sleep(3); /* Give server time to fully initialize */
    return pid;
}

static void stop_server(int pid) {
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

static int wait_for_server(const char *host, int port) {
    int i;
    P9Client *test_client;

    for (i = 0; i < MAX_WAIT_RETRIES; i++) {
        test_client = p9_client_connect(host, port);
        if (test_client != NULL) {
            /* Try to attach to verify server is fully ready */
            if (p9_client_attach(test_client, "") >= 0) {
                p9_client_disconnect(test_client);
                return 0;
            }
            /* Connect worked but attach failed - server not ready yet */
            p9_client_disconnect(test_client);
        }
        printf("Waiting for server... (%d/%d)\n", i + 1, MAX_WAIT_RETRIES);
        fflush(stdout);
        sleep(WAIT_RETRY_DELAY);
    }

    return -1;
}

#else
/* Windows implementation */

static int start_server(void) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char cmd[] = "bin\\kryon-server --port 17020";

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    /* Return process handle as PID */
    return (int)pi.hProcess;
}

static void stop_server(int pid) {
    if (pid > 0) {
        TerminateProcess((HANDLE)pid, 0);
        CloseHandle((HANDLE)pid);
    }
}

static int wait_for_server(const char *host, int port) {
    int i;
    P9Client *test_client;

    for (i = 0; i < MAX_WAIT_RETRIES; i++) {
        test_client = p9_client_connect(host, port);
        if (test_client != NULL) {
            p9_client_disconnect(test_client);
            return 0;
        }
        printf("Waiting for server... (%d/%d)\n", i + 1, MAX_WAIT_RETRIES);
        sleep(WAIT_RETRY_DELAY);
    }

    return -1;
}

#endif
