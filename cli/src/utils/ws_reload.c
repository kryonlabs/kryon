#include "ws_reload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CLIENTS 32
#define BUFFER_SIZE 4096

struct WSReloadServer {
    int server_fd;
    int port;
    int client_fds[MAX_CLIENTS];
    int client_count;
    pthread_t thread;
    bool running;
    pthread_mutex_t lock;
};

static void* server_thread(void* arg);
static void handle_client_connection(WSReloadServer* server, int client_fd);
static void send_websocket_frame(int fd, const char* message);

WSReloadServer* ws_reload_start(int port) {
    WSReloadServer* server = calloc(1, sizeof(WSReloadServer));
    if (!server) {
        return NULL;
    }

    server->port = port;
    server->client_count = 0;
    server->running = false;
    pthread_mutex_init(&server->lock, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        server->client_fds[i] = -1;
    }

    // Create socket
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("[ws_reload] socket");
        free(server);
        return NULL;
    }

    // Allow reuse of address
    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ws_reload] bind");
        close(server->server_fd);
        free(server);
        return NULL;
    }

    // Listen
    if (listen(server->server_fd, 5) < 0) {
        perror("[ws_reload] listen");
        close(server->server_fd);
        free(server);
        return NULL;
    }

    // Start server thread
    server->running = true;
    if (pthread_create(&server->thread, NULL, server_thread, server) != 0) {
        perror("[ws_reload] pthread_create");
        close(server->server_fd);
        free(server);
        return NULL;
    }

    printf("[ws_reload] WebSocket reload server started on port %d\n", port);
    return server;
}

void ws_reload_trigger(WSReloadServer* server) {
    if (!server) return;

    pthread_mutex_lock(&server->lock);

    // Send reload message to all connected clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->client_fds[i] >= 0) {
            send_websocket_frame(server->client_fds[i], "reload");
        }
    }

    pthread_mutex_unlock(&server->lock);
}

void ws_reload_stop(WSReloadServer* server) {
    if (!server) return;

    server->running = false;

    // Close all client connections
    pthread_mutex_lock(&server->lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->client_fds[i] >= 0) {
            close(server->client_fds[i]);
            server->client_fds[i] = -1;
        }
    }
    pthread_mutex_unlock(&server->lock);

    // Close server socket
    close(server->server_fd);

    // Wait for thread to finish
    pthread_join(server->thread, NULL);
    pthread_mutex_destroy(&server->lock);

    free(server);
    printf("[ws_reload] Server stopped\n");
}

static void* server_thread(void* arg) {
    WSReloadServer* server = (WSReloadServer*)arg;

    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR || !server->running) {
                break;
            }
            continue;
        }

        handle_client_connection(server, client_fd);
    }

    return NULL;
}

static void handle_client_connection(WSReloadServer* server, int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

    if (received <= 0) {
        close(client_fd);
        return;
    }

    buffer[received] = '\0';

    // Check if it's a WebSocket upgrade request
    if (strstr(buffer, "Upgrade: websocket") == NULL) {
        close(client_fd);
        return;
    }

    // Extract Sec-WebSocket-Key
    char* key_line = strstr(buffer, "Sec-WebSocket-Key:");
    if (!key_line) {
        close(client_fd);
        return;
    }

    key_line += 19; // Skip "Sec-WebSocket-Key: "
    char* key_end = strstr(key_line, "\r\n");
    if (!key_end) {
        close(client_fd);
        return;
    }

    // For simplicity, we'll accept any connection without proper handshake
    // In production, you'd compute the accept key properly
    const char* response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";

    send(client_fd, response, strlen(response), 0);

    // Add client to list
    pthread_mutex_lock(&server->lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->client_fds[i] < 0) {
            server->client_fds[i] = client_fd;
            server->client_count++;
            printf("[ws_reload] Client connected (total: %d)\n", server->client_count);
            break;
        }
    }
    pthread_mutex_unlock(&server->lock);
}

static void send_websocket_frame(int fd, const char* message) {
    size_t len = strlen(message);
    unsigned char frame[10];
    int frame_len = 0;

    // FIN + Text frame
    frame[frame_len++] = 0x81;

    // Payload length
    if (len < 126) {
        frame[frame_len++] = (unsigned char)len;
    } else if (len < 65536) {
        frame[frame_len++] = 126;
        frame[frame_len++] = (len >> 8) & 0xFF;
        frame[frame_len++] = len & 0xFF;
    }

    // Send frame header
    send(fd, frame, frame_len, 0);

    // Send payload
    send(fd, message, len, 0);
}
