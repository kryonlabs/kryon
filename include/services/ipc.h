/**
 * @file ipc.h
 * @brief IPC Service - Platform-specific inter-process communication
 *
 * Provides access to OS-specific IPC mechanisms:
 * - Named pipes / FIFOs
 * - Message queues
 * - Shared memory
 * - Platform-specific features (Inferno channels, etc.)
 *
 * This is a placeholder for future expansion. Initial focus is on
 * file I/O, namespace, and process control services.
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_SERVICE_IPC_H
#define KRYON_SERVICE_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// IPC CHANNEL
// =============================================================================

/**
 * @brief Opaque IPC channel handle
 */
typedef struct KryonIPCChannel KryonIPCChannel;

// =============================================================================
// IPC SERVICE INTERFACE
// =============================================================================

/**
 * @brief IPC service interface
 *
 * Apps receive this interface via kryon_services_get_interface().
 * This is a placeholder for future IPC features.
 */
typedef struct {
    /**
     * Create a named channel
     *
     * @param name Channel name
     * @return Channel handle, or NULL on failure
     */
    KryonIPCChannel* (*create_channel)(const char *name);

    /**
     * Connect to a named channel
     *
     * @param name Channel name
     * @return Channel handle, or NULL on failure
     */
    KryonIPCChannel* (*connect_channel)(const char *name);

    /**
     * Close a channel
     *
     * @param channel Channel to close
     */
    void (*close_channel)(KryonIPCChannel *channel);

    /**
     * Send message on channel
     *
     * @param channel Channel handle
     * @param data Data to send
     * @param size Size of data
     * @return true on success, false on failure
     */
    bool (*send)(KryonIPCChannel *channel, const void *data, size_t size);

    /**
     * Receive message from channel
     *
     * @param channel Channel handle
     * @param buf Buffer to receive into
     * @param size Maximum size to receive
     * @return Bytes received, or -1 on error
     */
    int (*recv)(KryonIPCChannel *channel, void *buf, size_t size);

} KryonIPCService;

#ifdef __cplusplus
}
#endif

#endif // KRYON_SERVICE_IPC_H
