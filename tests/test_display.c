/*
 * Test program to launch Marrow server and Kryon display client
 * C89/C90 compliant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/*
 * kill and wait prototype for C89
 */
extern pid_t fork(void);
extern int kill(pid_t pid, int sig);

static volatile int running = 1;

void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char **argv)
{
    pid_t marrow_pid;
    int status;
    char *marrow_path;
    char *display_path;

    (void)argc;
    (void)argv;

    /* Paths to binaries */
    marrow_path = "../marrow/bin/marrow";
    display_path = "./bin/kryon-display";

    /* Kill any existing instances */
    (void)system("pkill -f 'marrow --port 17019' 2>/dev/null");
    (void)system("pkill -f 'kryon-display' 2>/dev/null");
    sleep(1);

    /* Check if binaries exist */
    {
        FILE *f = fopen(marrow_path, "r");
        if (f == NULL) {
            fprintf(stderr, "Error: Marrow binary not found at %s\n", marrow_path);
            fprintf(stderr, "Please build Marrow first: cd ../marrow && make\n");
            return 1;
        }
        fclose(f);
    }

    {
        FILE *f = fopen(display_path, "r");
        if (f == NULL) {
            fprintf(stderr, "Error: Kryon display binary not found at %s\n", display_path);
            fprintf(stderr, "Please build Kryon first: make\n");
            return 1;
        }
        fclose(f);
    }

    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Fork and start Marrow */
    marrow_pid = fork();
    if (marrow_pid == 0) {
        /* Child process - run Marrow */
        execl(marrow_path, marrow_path, "--port", "17019", (char *)NULL);
        perror("execl marrow");
        exit(1);
    } else if (marrow_pid < 0) {
        perror("fork");
        return 1;
    }

    /* Parent process */
    printf("Starting Marrow server on port 17019...\n");
    sleep(2);

    /* Check if Marrow is still running */
    if (kill(marrow_pid, 0) < 0) {
        fprintf(stderr, "Error: Marrow server failed to start\n");
        return 1;
    }

    printf("Marrow server started (PID: %d)\n\n", (int)marrow_pid);
    printf("Starting Kryon display client...\n");
    printf("Press Ctrl+C to stop both server and client\n\n");

    /* Fork and start display client */
    {
        pid_t display_pid = fork();
        if (display_pid == 0) {
            /* Child process - run display client */
            execl(display_path, display_path, "--host", "127.0.0.1", "--port", "17019", (char *)NULL);
            perror("execl kryon-display");
            exit(1);
        } else if (display_pid < 0) {
            perror("fork");
            kill(marrow_pid, SIGTERM);
            return 1;
        }

        /* Wait for display client to finish */
        waitpid(display_pid, &status, 0);
    }

    /* Clean up */
    printf("\nStopping Marrow server...\n");
    kill(marrow_pid, SIGTERM);
    waitpid(marrow_pid, &status, 0);

    printf("Test complete\n");
    return 0;
}
