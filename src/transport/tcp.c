/*
 * TCP Transport Layer
 */

#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

/*
 * Create a TCP listening socket
 */
int tcp_listen(int port)
{
    int listen_fd;
    struct sockaddr_in addr;
    int opt;

    /* Create socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    /* Set SO_REUSEADDR */
    opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(listen_fd);
        return -1;
    }

    /* Bind to address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    /* Listen */
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

/*
 * Accept a client connection
 */
int tcp_accept(int listen_fd)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    client_len = sizeof(client_addr);
    client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    return client_fd;
}

/*
 * Set socket to non-blocking mode
 */
static int set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/*
 * Receive a complete 9P message
 * Returns: >0 = message length, 0 = connection closed, <0 = error
 */
int tcp_recv_msg(int fd, uint8_t *buf, size_t buf_len)
{
    uint8_t size_buf[4];
    uint32_t msg_size;
    ssize_t n;
    size_t total;

    /* First, read the 4-byte size field */
    n = recv(fd, size_buf, 4, MSG_WAITALL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* No data available */
        }
        return -1;
    }
    if (n == 0) {
        return 0;  /* Connection closed */
    }
    if (n != 4) {
        return -1;  /* Short read */
    }

    /* Decode message size */
    msg_size = le_get32(size_buf);

    /* Sanity check */
    if (msg_size < 7 || msg_size > buf_len) {
        return -1;
    }

    /* Copy size to buffer */
    memcpy(buf, size_buf, 4);

    /* Read the rest of the message */
    total = 4;
    while (total < msg_size) {
        n = recv(fd, buf + total, msg_size - total, MSG_WAITALL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;  /* Connection closed unexpectedly */
        }
        total += n;
    }

    return (int)msg_size;
}

/*
 * Send a 9P message
 */
int tcp_send_msg(int fd, const uint8_t *buf, size_t len)
{
    ssize_t n;
    size_t total = 0;

    while (total < len) {
        n = send(fd, buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        total += n;
    }

    return 0;
}

/*
 * Close a TCP connection
 */
void tcp_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
