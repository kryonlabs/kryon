/*
 * TCP Transport Layer Interface
 */

#ifndef KRYON_TCP_H
#define KRYON_TCP_H

#include <stdint.h>

/*
 * Create a TCP listening socket on the specified port
 */
int tcp_listen(int port);

/*
 * Accept a client connection
 */
int tcp_accept(int listen_fd);

/*
 * Receive a complete 9P message
 * Returns: >0 = message length, 0 = no data, <0 = error
 */
int tcp_recv_msg(int fd, uint8_t *buf, size_t buf_len);

/*
 * Send a 9P message
 */
int tcp_send_msg(int fd, const uint8_t *buf, size_t len);

/*
 * Close a TCP connection
 */
void tcp_close(int fd);

#endif /* KRYON_TCP_H */
