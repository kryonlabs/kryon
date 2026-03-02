/*
 * WebAssembly Platform Layer - 9P Simulation
 * C89 compliant
 *
 * Simulates 9P protocol over Web Workers for WebAssembly target.
 * Messages are serialized and sent to JavaScript via postMessage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* WebAssembly 9P message buffer size */
#define WASM_9P_BUF_SIZE 8192

/* Simulated 9P connection */
typedef struct Wasm9PConn {
    int connected;
    char rx_buffer[WASM_9P_BUF_SIZE];
    size_t rx_len;
    char tx_buffer[WASM_9P_BUF_SIZE];
    size_t tx_len;
} Wasm9PConn;

/* Global 9P connection */
static Wasm9PConn g_wasm_9p_conn = {0};

/*
 * Initialize 9P connection (called from JavaScript)
 */
void wasm_9p_init(void)
{
    memset(&g_wasm_9p_conn, 0, sizeof(Wasm9PConn));
    g_wasm_9p_conn.connected = 1;
}

/*
 * Send 9P message (called from JavaScript)
 */
void wasm_9p_send(const char *data, int len)
{
    if (!g_wasm_9p_conn.connected || len > WASM_9P_BUF_SIZE) {
        return;
    }

    memcpy(g_wasm_9p_conn.rx_buffer, data, len);
    g_wasm_9p_conn.rx_len = len;

    /* TODO: Process message through dispatch_9p */
}

/*
 * Receive 9P message (called from JavaScript)
 */
const char* wasm_9p_recv(int *len)
{
    if (!g_wasm_9p_conn.connected || g_wasm_9p_conn.tx_len == 0) {
        *len = 0;
        return NULL;
    }

    *len = g_wasm_9p_conn.tx_len;
    return g_wasm_9p_conn.tx_buffer;
}

/*
 * Connect to Marrow server via WebSocket (simulated)
 */
int wasm_9p_connect(const char *url)
{
    /* TODO: Establish WebSocket connection */
    /* For now, assume connection succeeds */
    g_wasm_9p_conn.connected = 1;
    return 0;
}

/*
 * Disconnect from server
 */
void wasm_9p_disconnect(void)
{
    g_wasm_9p_conn.connected = 0;
    g_wasm_9p_conn.rx_len = 0;
    g_wasm_9p_conn.tx_len = 0;
}

/*
 * 9P mount (simulates mounting a 9P server)
 */
void* wasm_9p_mount(const char *mount_point, const char *server_url)
{
    /* TODO: Implement 9P filesystem mount */
    /* For now, return a fake handle */
    return (void*)1;
}
