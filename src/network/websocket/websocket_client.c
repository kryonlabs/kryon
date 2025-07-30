/**
 * @file websocket_client.c
 * @brief Kryon WebSocket Client Implementation
 */

#include "internal/network.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// =============================================================================
// WEBSOCKET PROTOCOL CONSTANTS
// =============================================================================

#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_VERSION "13"

// WebSocket opcodes
#define WS_OPCODE_CONTINUATION  0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

// WebSocket close codes
#define WS_CLOSE_NORMAL        1000
#define WS_CLOSE_GOING_AWAY    1001
#define WS_CLOSE_PROTOCOL_ERROR 1002
#define WS_CLOSE_UNSUPPORTED   1003
#define WS_CLOSE_INVALID_DATA  1007
#define WS_CLOSE_POLICY_VIOLATION 1008
#define WS_CLOSE_MESSAGE_TOO_BIG 1009

// =============================================================================
// WEBSOCKET STRUCTURES
// =============================================================================

typedef struct {
    uint8_t fin : 1;
    uint8_t rsv1 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv3 : 1;
    uint8_t opcode : 4;
    uint8_t mask : 1;
    uint64_t payload_length;
    uint8_t masking_key[4];
    uint8_t* payload_data;
} KryonWebSocketFrame;

typedef struct {
    int socket_fd;
    bool is_connected;
    bool is_client;
    
    // Connection info
    char* host;
    int port;
    char* path;
    char* origin;
    
    // WebSocket key for handshake
    char* websocket_key;
    
    // Callbacks
    KryonWebSocketMessageCallback on_message;
    KryonWebSocketCloseCallback on_close;
    KryonWebSocketErrorCallback on_error;
    void* user_data;
    
    // Message queue
    KryonWebSocketMessage* message_queue;
    size_t queue_size;
    size_t queue_capacity;
    
    // State
    KryonWebSocketState state;
    uint16_t close_code;
    char* close_reason;
    
} KryonWebSocketConnection;

// =============================================================================
// BASE64 ENCODING (for WebSocket handshake)
// =============================================================================

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const uint8_t* data, size_t len) {
    size_t encoded_len = 4 * ((len + 2) / 3);
    char* encoded = kryon_alloc(encoded_len + 1);
    if (!encoded) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t a = i < len ? data[i++] : 0;
        uint32_t b = i < len ? data[i++] : 0;
        uint32_t c = i < len ? data[i++] : 0;
        
        uint32_t triple = (a << 16) + (b << 8) + c;
        
        encoded[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded[j++] = base64_chars[triple & 0x3F];
    }
    
    // Add padding
    size_t actual_len = len;
    if (actual_len % 3 == 1) {
        encoded[encoded_len - 2] = '=';
        encoded[encoded_len - 1] = '=';
    } else if (actual_len % 3 == 2) {
        encoded[encoded_len - 1] = '=';
    }
    
    encoded[encoded_len] = '\0';
    return encoded;
}

// =============================================================================
// SHA1 HASH (simplified for WebSocket handshake)
// =============================================================================

static void sha1_hash(const char* input, size_t len, uint8_t hash[20]) {
    // This is a simplified SHA1 implementation
    // In a production system, you would use a proper crypto library
    
    // For demonstration, we'll use a simple hash function
    // This is NOT cryptographically secure and should not be used in production
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    
    for (size_t i = 0; i < len; i++) {
        h[0] ^= (uint32_t)input[i] << (i % 32);
        h[1] ^= (uint32_t)input[i] << ((i + 8) % 32);
        h[2] ^= (uint32_t)input[i] << ((i + 16) % 32);
        h[3] ^= (uint32_t)input[i] << ((i + 24) % 32);
        h[4] ^= (uint32_t)input[i];
        
        // Simple mixing
        uint32_t temp = h[0];
        h[0] = h[1];
        h[1] = h[2];
        h[2] = h[3];
        h[3] = h[4];
        h[4] = temp ^ (h[0] << 5) ^ (h[1] >> 27);
    }
    
    // Convert to bytes
    for (int i = 0; i < 5; i++) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

// =============================================================================
// WEBSOCKET HANDSHAKE
// =============================================================================

static char* generate_websocket_key(void) {
    uint8_t random_bytes[16];
    
    // Generate random bytes (simple random for demo)
    srand(time(NULL));
    for (int i = 0; i < 16; i++) {
        random_bytes[i] = rand() % 256;
    }
    
    return base64_encode(random_bytes, 16);
}

static char* calculate_accept_key(const char* websocket_key) {
    if (!websocket_key) return NULL;
    
    // Concatenate key with magic string
    size_t combined_len = strlen(websocket_key) + strlen(WS_MAGIC_STRING);
    char* combined = kryon_alloc(combined_len + 1);
    if (!combined) return NULL;
    
    strcpy(combined, websocket_key);
    strcat(combined, WS_MAGIC_STRING);
    
    // Calculate SHA1 hash
    uint8_t hash[20];
    sha1_hash(combined, combined_len, hash);
    kryon_free(combined);
    
    // Base64 encode the hash
    return base64_encode(hash, 20);
}

static bool send_handshake_request(KryonWebSocketConnection* conn) {
    if (!conn || conn->socket_fd < 0) return false;
    
    // Generate WebSocket key
    conn->websocket_key = generate_websocket_key();
    if (!conn->websocket_key) return false;
    
    // Build handshake request
    char request[2048];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: %s\r\n"
        "%s%s%s"
        "\r\n",
        conn->path ? conn->path : "/",
        conn->host, conn->port,
        conn->websocket_key,
        WS_VERSION,
        conn->origin ? "Origin: " : "",
        conn->origin ? conn->origin : "",
        conn->origin ? "\r\n" : ""
    );
    
    // Send request
    ssize_t sent = send(conn->socket_fd, request, strlen(request), 0);
    return sent == (ssize_t)strlen(request);
}

static bool validate_handshake_response(KryonWebSocketConnection* conn) {
    char response[4096];
    ssize_t received = recv(conn->socket_fd, response, sizeof(response) - 1, 0);
    
    if (received <= 0) return false;
    
    response[received] = '\0';
    
    // Check status line
    if (strncmp(response, "HTTP/1.1 101", 12) != 0) {
        return false;
    }
    
    // Check required headers
    if (!strstr(response, "Upgrade: websocket") && !strstr(response, "upgrade: websocket")) {
        return false;
    }
    
    if (!strstr(response, "Connection: Upgrade") && !strstr(response, "connection: upgrade")) {
        return false;
    }
    
    // Validate Sec-WebSocket-Accept
    char* accept_header = strstr(response, "Sec-WebSocket-Accept:");
    if (!accept_header) {
        accept_header = strstr(response, "sec-websocket-accept:");
    }
    
    if (accept_header) {
        // Extract accept value
        char* accept_value = accept_header + strlen("Sec-WebSocket-Accept:");
        while (*accept_value == ' ' || *accept_value == '\t') accept_value++;
        
        char* line_end = strchr(accept_value, '\r');
        if (!line_end) line_end = strchr(accept_value, '\n');
        
        if (line_end) {
            size_t accept_len = line_end - accept_value;
            char* received_accept = kryon_alloc(accept_len + 1);
            if (received_accept) {
                strncpy(received_accept, accept_value, accept_len);
                received_accept[accept_len] = '\0';
                
                // Calculate expected accept value
                char* expected_accept = calculate_accept_key(conn->websocket_key);
                bool valid = expected_accept && strcmp(received_accept, expected_accept) == 0;
                
                kryon_free(received_accept);
                kryon_free(expected_accept);
                
                return valid;
            }
        }
    }
    
    return false;
}

// =============================================================================
// WEBSOCKET FRAME HANDLING
// =============================================================================

static void mask_payload(uint8_t* payload, size_t len, const uint8_t mask[4]) {
    for (size_t i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

static bool send_websocket_frame(KryonWebSocketConnection* conn, uint8_t opcode, 
                                const uint8_t* payload, size_t payload_len, bool fin) {
    if (!conn || conn->socket_fd < 0) return false;
    
    uint8_t frame[14]; // Maximum header size
    size_t frame_len = 0;
    
    // First byte: FIN + opcode
    frame[0] = (fin ? 0x80 : 0x00) | (opcode & 0x0F);
    frame_len++;
    
    // Payload length
    if (payload_len < 126) {
        frame[1] = 0x80 | (uint8_t)payload_len; // MASK bit + length
        frame_len++;
    } else if (payload_len < 65536) {
        frame[1] = 0x80 | 126; // MASK bit + extended length indicator
        frame[2] = (payload_len >> 8) & 0xFF;
        frame[3] = payload_len & 0xFF;
        frame_len += 3;
    } else {
        frame[1] = 0x80 | 127; // MASK bit + extended length indicator
        // 64-bit length (only using lower 32 bits for simplicity)
        for (int i = 0; i < 4; i++) {
            frame[2 + i] = 0; // Upper 32 bits
        }
        frame[6] = (payload_len >> 24) & 0xFF;
        frame[7] = (payload_len >> 16) & 0xFF;
        frame[8] = (payload_len >> 8) & 0xFF;
        frame[9] = payload_len & 0xFF;
        frame_len += 8;
    }
    
    // Masking key (required for client frames)
    uint8_t mask[4];
    if (conn->is_client) {
        for (int i = 0; i < 4; i++) {
            mask[i] = rand() % 256;
            frame[frame_len + i] = mask[i];
        }
        frame_len += 4;
    }
    
    // Send header
    if (send(conn->socket_fd, frame, frame_len, 0) != (ssize_t)frame_len) {
        return false;
    }
    
    // Send payload (with masking if client)
    if (payload && payload_len > 0) {
        if (conn->is_client) {
            // Create masked copy of payload
            uint8_t* masked_payload = kryon_alloc(payload_len);
            if (!masked_payload) return false;
            
            memcpy(masked_payload, payload, payload_len);
            mask_payload(masked_payload, payload_len, mask);
            
            ssize_t sent = send(conn->socket_fd, masked_payload, payload_len, 0);
            kryon_free(masked_payload);
            
            return sent == (ssize_t)payload_len;
        } else {
            return send(conn->socket_fd, payload, payload_len, 0) == (ssize_t)payload_len;
        }
    }
    
    return true;
}

static bool receive_websocket_frame(KryonWebSocketConnection* conn, KryonWebSocketFrame* frame) {
    if (!conn || !frame || conn->socket_fd < 0) return false;
    
    memset(frame, 0, sizeof(KryonWebSocketFrame));
    
    // Read first two bytes
    uint8_t header[2];
    if (recv(conn->socket_fd, header, 2, 0) != 2) {
        return false;
    }
    
    // Parse first byte
    frame->fin = (header[0] & 0x80) != 0;
    frame->rsv1 = (header[0] & 0x40) != 0;
    frame->rsv2 = (header[0] & 0x20) != 0;
    frame->rsv3 = (header[0] & 0x10) != 0;
    frame->opcode = header[0] & 0x0F;
    
    // Parse second byte
    frame->mask = (header[1] & 0x80) != 0;
    uint8_t payload_len = header[1] & 0x7F;
    
    // Read extended payload length
    if (payload_len == 126) {
        uint8_t ext_len[2];
        if (recv(conn->socket_fd, ext_len, 2, 0) != 2) return false;
        frame->payload_length = (ext_len[0] << 8) | ext_len[1];
    } else if (payload_len == 127) {
        uint8_t ext_len[8];
        if (recv(conn->socket_fd, ext_len, 8, 0) != 8) return false;
        // Only use lower 32 bits for simplicity
        frame->payload_length = (uint64_t)(ext_len[4] << 24) | (ext_len[5] << 16) | 
                               (ext_len[6] << 8) | ext_len[7];
    } else {
        frame->payload_length = payload_len;
    }
    
    // Read masking key
    if (frame->mask) {
        if (recv(conn->socket_fd, frame->masking_key, 4, 0) != 4) return false;
    }
    
    // Read payload
    if (frame->payload_length > 0) {
        frame->payload_data = kryon_alloc(frame->payload_length + 1);
        if (!frame->payload_data) return false;
        
        size_t total_received = 0;
        while (total_received < frame->payload_length) {
            ssize_t received = recv(conn->socket_fd, 
                                  frame->payload_data + total_received,
                                  frame->payload_length - total_received, 0);
            if (received <= 0) {
                kryon_free(frame->payload_data);
                frame->payload_data = NULL;
                return false;
            }
            total_received += received;
        }
        
        // Unmask payload if needed
        if (frame->mask) {
            mask_payload(frame->payload_data, frame->payload_length, frame->masking_key);
        }
        
        frame->payload_data[frame->payload_length] = '\0'; // Null terminate for text frames
    }
    
    return true;
}

// =============================================================================
// MESSAGE HANDLING
// =============================================================================

static void handle_websocket_message(KryonWebSocketConnection* conn, KryonWebSocketFrame* frame) {
    if (!conn || !frame) return;
    
    switch (frame->opcode) {
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY: {
            if (conn->on_message) {
                KryonWebSocketMessage msg;
                msg.type = (frame->opcode == WS_OPCODE_TEXT) ? 
                          KRYON_WEBSOCKET_MESSAGE_TEXT : KRYON_WEBSOCKET_MESSAGE_BINARY;
                msg.data = frame->payload_data;
                msg.size = frame->payload_length;
                msg.is_final = frame->fin;
                
                conn->on_message(conn, &msg, conn->user_data);
            }
            break;
        }
        
        case WS_OPCODE_CLOSE: {
            uint16_t close_code = WS_CLOSE_NORMAL;
            char* close_reason = NULL;
            
            if (frame->payload_length >= 2) {
                close_code = (frame->payload_data[0] << 8) | frame->payload_data[1];
                
                if (frame->payload_length > 2) {
                    size_t reason_len = frame->payload_length - 2;
                    close_reason = kryon_alloc(reason_len + 1);
                    if (close_reason) {
                        memcpy(close_reason, frame->payload_data + 2, reason_len);
                        close_reason[reason_len] = '\0';
                    }
                }
            }
            
            conn->close_code = close_code;
            conn->close_reason = close_reason;
            conn->state = KRYON_WEBSOCKET_STATE_CLOSING;
            
            // Send close frame response
            uint8_t close_payload[2] = {(close_code >> 8) & 0xFF, close_code & 0xFF};
            send_websocket_frame(conn, WS_OPCODE_CLOSE, close_payload, 2, true);
            
            if (conn->on_close) {
                conn->on_close(conn, close_code, close_reason, conn->user_data);
            }
            
            conn->state = KRYON_WEBSOCKET_STATE_CLOSED;
            break;
        }
        
        case WS_OPCODE_PING: {
            // Respond with pong
            send_websocket_frame(conn, WS_OPCODE_PONG, frame->payload_data, 
                               frame->payload_length, true);
            break;
        }
        
        case WS_OPCODE_PONG: {
            // Handle pong response (could be used for keepalive)
            break;
        }
        
        default:
            // Unknown opcode - could send close frame with protocol error
            break;
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

KryonWebSocketClient* kryon_websocket_client_create(void) {
    KryonWebSocketClient* client = kryon_alloc(sizeof(KryonWebSocketClient));
    if (!client) return NULL;
    
    memset(client, 0, sizeof(KryonWebSocketClient));
    return client;
}

void kryon_websocket_client_destroy(KryonWebSocketClient* client) {
    if (!client) return;
    
    if (client->connection) {
        kryon_websocket_close(client->connection, WS_CLOSE_GOING_AWAY, "Client destroyed");
    }
    
    kryon_free(client);
}

KryonWebSocketConnection* kryon_websocket_connect(KryonWebSocketClient* client, const char* url) {
    if (!client || !url) return NULL;
    
    // Parse WebSocket URL (ws://host:port/path)
    char* host = NULL;
    int port = 80;
    char* path = NULL;
    bool use_ssl = false;
    
    if (strncmp(url, "wss://", 6) == 0) {
        use_ssl = true;
        port = 443;
        url += 6;
    } else if (strncmp(url, "ws://", 5) == 0) {
        url += 5;
    }
    
    // Parse host, port, and path
    const char* path_start = strchr(url, '/');
    const char* port_start = strchr(url, ':');
    
    size_t host_len;
    if (port_start && (!path_start || port_start < path_start)) {
        host_len = port_start - url;
        port = atoi(port_start + 1);
    } else if (path_start) {
        host_len = path_start - url;
    } else {
        host_len = strlen(url);
    }
    
    host = kryon_alloc(host_len + 1);
    if (!host) return NULL;
    
    strncpy(host, url, host_len);
    host[host_len] = '\0';
    
    if (path_start) {
        path = kryon_alloc(strlen(path_start) + 1);
        if (path) {
            strcpy(path, path_start);
        }
    } else {
        path = kryon_alloc(2);
        if (path) {
            strcpy(path, "/");
        }
    }
    
    // Create socket connection
    struct hostent* server = gethostbyname(host);
    if (!server) {
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    // Create WebSocket connection
    KryonWebSocketConnection* conn = kryon_alloc(sizeof(KryonWebSocketConnection));
    if (!conn) {
        close(sockfd);
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    memset(conn, 0, sizeof(KryonWebSocketConnection));
    conn->socket_fd = sockfd;
    conn->is_client = true;
    conn->host = host;
    conn->port = port;
    conn->path = path;
    conn->state = KRYON_WEBSOCKET_STATE_CONNECTING;
    
    // Perform WebSocket handshake
    if (!send_handshake_request(conn) || !validate_handshake_response(conn)) {
        kryon_websocket_close(conn, WS_CLOSE_PROTOCOL_ERROR, "Handshake failed");
        return NULL;
    }
    
    conn->state = KRYON_WEBSOCKET_STATE_OPEN;
    conn->is_connected = true;
    client->connection = conn;
    
    return conn;
}

void kryon_websocket_set_callbacks(KryonWebSocketConnection* conn,
                                  KryonWebSocketMessageCallback on_message,
                                  KryonWebSocketCloseCallback on_close,
                                  KryonWebSocketErrorCallback on_error,
                                  void* user_data) {
    if (!conn) return;
    
    conn->on_message = on_message;
    conn->on_close = on_close;
    conn->on_error = on_error;
    conn->user_data = user_data;
}

bool kryon_websocket_send_text(KryonWebSocketConnection* conn, const char* text) {
    if (!conn || !text || conn->state != KRYON_WEBSOCKET_STATE_OPEN) {
        return false;
    }
    
    return send_websocket_frame(conn, WS_OPCODE_TEXT, (const uint8_t*)text, strlen(text), true);
}

bool kryon_websocket_send_binary(KryonWebSocketConnection* conn, const uint8_t* data, size_t size) {
    if (!conn || !data || size == 0 || conn->state != KRYON_WEBSOCKET_STATE_OPEN) {
        return false;
    }
    
    return send_websocket_frame(conn, WS_OPCODE_BINARY, data, size, true);
}

bool kryon_websocket_ping(KryonWebSocketConnection* conn, const uint8_t* data, size_t size) {
    if (!conn || conn->state != KRYON_WEBSOCKET_STATE_OPEN) {
        return false;
    }
    
    return send_websocket_frame(conn, WS_OPCODE_PING, data, size, true);
}

void kryon_websocket_process_events(KryonWebSocketConnection* conn) {
    if (!conn || conn->socket_fd < 0 || conn->state != KRYON_WEBSOCKET_STATE_OPEN) {
        return;
    }
    
    // Set socket to non-blocking for polling
    int flags = fcntl(conn->socket_fd, F_GETFL, 0);
    fcntl(conn->socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    KryonWebSocketFrame frame;
    while (receive_websocket_frame(conn, &frame)) {
        handle_websocket_message(conn, &frame);
        
        if (frame.payload_data) {
            kryon_free(frame.payload_data);
        }
        
        if (conn->state == KRYON_WEBSOCKET_STATE_CLOSED) {
            break;
        }
    }
    
    // Restore blocking mode
    fcntl(conn->socket_fd, F_SETFL, flags);
}

void kryon_websocket_close(KryonWebSocketConnection* conn, uint16_t code, const char* reason) {
    if (!conn || conn->state == KRYON_WEBSOCKET_STATE_CLOSED) {
        return;
    }
    
    if (conn->state == KRYON_WEBSOCKET_STATE_OPEN) {
        // Send close frame
        uint8_t close_payload[2 + 125]; // Max reason length
        close_payload[0] = (code >> 8) & 0xFF;
        close_payload[1] = code & 0xFF;
        
        size_t payload_len = 2;
        if (reason) {
            size_t reason_len = strlen(reason);
            if (reason_len > 125) reason_len = 125;
            memcpy(close_payload + 2, reason, reason_len);
            payload_len += reason_len;
        }
        
        send_websocket_frame(conn, WS_OPCODE_CLOSE, close_payload, payload_len, true);
        conn->state = KRYON_WEBSOCKET_STATE_CLOSING;
    }
    
    // Close socket
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
        conn->socket_fd = -1;
    }
    
    conn->is_connected = false;
    conn->state = KRYON_WEBSOCKET_STATE_CLOSED;
    
    // Clean up
    kryon_free(conn->host);
    kryon_free(conn->path);
    kryon_free(conn->origin);
    kryon_free(conn->websocket_key);
    kryon_free(conn->close_reason);
    kryon_free(conn);
}

KryonWebSocketState kryon_websocket_get_state(KryonWebSocketConnection* conn) {
    return conn ? conn->state : KRYON_WEBSOCKET_STATE_CLOSED;
}

bool kryon_websocket_is_connected(KryonWebSocketConnection* conn) {
    return conn && conn->is_connected && conn->state == KRYON_WEBSOCKET_STATE_OPEN;
}