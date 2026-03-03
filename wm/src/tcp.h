/*
 * Simple TCP wrapper for Kryon Server
 * C89/C90 compliant
 */

#ifndef TCP_H
#define TCP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
 * Listen on a TCP port (IPv6 dual-stack for IPv4+IPv6)
 * Returns socket fd on success, -1 on failure
 */
static int tcp_listen(int port)
{
    int fd;
    struct sockaddr_in6 addr;
    int reuse = 1;
    int ipv6only = 0;

    /* Create IPv6 socket that can also accept IPv4 connections */
    fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket IPv6");
        /* Fall back to IPv4 only */
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("socket IPv4");
            return -1;
        }

        /* IPv4 fallback */
        struct sockaddr_in addr4;
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_addr.s_addr = INADDR_ANY;
        addr4.sin_port = htons(port);

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&reuse, sizeof(reuse)) < 0) {
            perror("setsockopt SO_REUSEADDR");
            close(fd);
            return -1;
        }

        if (bind(fd, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
            perror("bind IPv4");
            close(fd);
            return -1;
        }

        fprintf(stderr, "Listening on IPv4 port %d\n", port);
    } else {
        /* IPv6 socket - disable IPV6_V6ONLY to allow IPv4 connections */
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                       (const char *)&ipv6only, sizeof(ipv6only)) < 0) {
            perror("setsockopt IPV6_V6ONLY");
            /* This is OK on some systems, continue anyway */
        }

        /* Set reuse address */
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&reuse, sizeof(reuse)) < 0) {
            perror("setsockopt SO_REUSEADDR");
            close(fd);
            return -1;
        }

        /* Bind to IPv6 address */
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind IPv6");
            close(fd);
            return -1;
        }

        fprintf(stderr, "Listening on IPv4+IPv6 dual-stack port %d\n", port);
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
    struct sockaddr_in6 client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("accept");
        }
        return -1;
    }

    /* Log connection details */
    if (client_addr.sin6_family == AF_INET6) {
        char addr_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &client_addr.sin6_addr, addr_str, sizeof(addr_str));
        fprintf(stderr, "Accepted IPv6 connection from %s\n", addr_str);
    } else {
        /* IPv4-mapped IPv6 address */
        char addr_str[INET_ADDRSTRLEN];
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &addr4->sin_addr, addr_str, sizeof(addr_str));
        fprintf(stderr, "Accepted IPv4 connection from %s\n", addr_str);
    }

    /* Disable Nagle's algorithm for immediate delivery of small packets (auth, 9P) */
    {
        int flag = 1;
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY,
                       (char *)&flag, sizeof(flag)) < 0) {
            fprintf(stderr, "Warning: failed to set TCP_NODELAY: %s\n",
                    strerror(errno));
            /* Continue anyway - not fatal */
        }
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

    /* Declare variables at top of block for C89 */
    {
        unsigned char *len_bytes;
        unsigned char *peek_bytes;

        if (msg_len > buf_size) {
            fprintf(stderr, "Message too large: %u (max: %u)\n", msg_len, (unsigned int)buf_size);
            /* Log hex of length field for debugging */
            len_bytes = (unsigned char *)&msg_len;
            fprintf(stderr, "  Raw length bytes: %02x %02x %02x %02x\n",
                    len_bytes[0], len_bytes[1], len_bytes[2], len_bytes[3]);
            /* Log hex of what we received */
            peek_bytes = (unsigned char *)&msg_len;
            fprintf(stderr, "  Peeked bytes: %02x %02x %02x %02x\n",
                    peek_bytes[0], peek_bytes[1], peek_bytes[2], peek_bytes[3]);
            return -1;
        }

        if (msg_len < 4) {
            fprintf(stderr, "Message length too small: %u (minimum 4)\n", msg_len);
            len_bytes = (unsigned char *)&msg_len;
            fprintf(stderr, "  Raw length bytes: %02x %02x %02x %02x\n",
                    len_bytes[0], len_bytes[1], len_bytes[2], len_bytes[3]);
            return -1;
        }
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
