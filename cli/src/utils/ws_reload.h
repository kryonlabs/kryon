#ifndef WS_RELOAD_H
#define WS_RELOAD_H

typedef struct WSReloadServer WSReloadServer;

/**
 * Start WebSocket reload server on the specified port
 * Returns NULL on failure
 */
WSReloadServer* ws_reload_start(int port);

/**
 * Broadcast reload message to all connected clients
 */
void ws_reload_trigger(WSReloadServer* server);

/**
 * Stop the server and close all connections
 */
void ws_reload_stop(WSReloadServer* server);

#endif // WS_RELOAD_H
