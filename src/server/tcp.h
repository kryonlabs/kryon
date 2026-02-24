/*
 * Simple TCP wrapper for Kryon Server
 * C89/C90 compliant
 */

#ifndef TCP_H
#define TCP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/*
 * Byte order conversion (little-endian)
 */
#ifndef le32toh
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define le32toh(x) __builtin_bswap32(x)
#else
#define le32toh(x) (x)
#endif
#endif

/*
 * Listen on a TCP port
 * Returns socket fd on success, -1 on failure
 */
static int tcp_listen(int port)
{
    int fd;
    struct sockaddr_in addr;
    int reuse = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* Set reuse address */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    /* Bind to address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    /* Listen */
    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

/*
 * Accept a new connection
 * Returns client fd on success, -1 on failure
 */
static int tcp_accept(int listen_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("accept");
        }
        return -1;
    }

    return client_fd;
}

/*
 * Close a connection
 */
static void tcp_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * Receive a 9P message (with length prefix)
 * Returns message length on success, 0 if no data, -1 on error
 */
static int tcp_recv_msg(int fd, unsigned char *buf, size_t buf_size)
{
    uint32_t msg_len;
    ssize_t nread;
    size_t total;

    /* Read message length (4 bytes, little-endian) */
    nread = recv(fd, &msg_len, 4, MSG_PEEK | MSG_DONTWAIT);
    if (nread < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;  /* No data available */
        }
        return -1;
    }
    if (nread == 0) {
        return -1;  /* Connection closed */
    }
    if (nread < 4) {
        return 0;  /* Not enough data yet */
    }

    /* Convert from network byte order */
    msg_len = le32toh(msg_len);

    if (msg_len > buf_size) {
        fprintf(stderr, "Message too large: %u\n", msg_len);
        return -1;
    }

    /* Read full message */
    total = 0;
    while (total < msg_len) {
        nread = recv(fd, buf + total, msg_len - total, MSG_DONTWAIT);
        if (nread < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (total == 0) {
                    return 0;  /* No data available */
                }
                /* Partial read, continue */
                continue;
            }
            return -1;
        }
        if (nread == 0) {
            return -1;  /* Connection closed */
        }
        total += nread;
    }

    return (int)msg_len;
}

/*
 * Send a 9P message (with length prefix)
 * Returns 0 on success, -1 on error
 */
static int tcp_send_msg(int fd, const unsigned char *buf, size_t msg_len)
{
    ssize_t nsent;
    size_t total;

    fprintf(stderr, "tcp_send_msg: fd=%d msg_len=%lu\n", fd, (unsigned long)msg_len);

    total = 0;
    while (total < msg_len) {
        nsent = send(fd, buf + total, msg_len - total, 0);
        if (nsent < 0) {
            perror("send");
            return -1;
        }
        fprintf(stderr, "tcp_send_msg: sent %ld bytes (total %lu/%lu)\n",
                (long)nsent, (unsigned long)(total + nsent), (unsigned long)msg_len);
        total += nsent;
    }

    fprintf(stderr, "tcp_send_msg: successfully sent %lu bytes total\n", (unsigned long)total);

    return 0;
}

#endif /* TCP_H */
