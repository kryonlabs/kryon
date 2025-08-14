/**
 * @file http_client.c
 * @brief Kryon HTTP Client Implementation
 */

#include "network.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

// =============================================================================
// HTTP CLIENT STRUCTURES
// =============================================================================

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} KryonHttpBuffer;

typedef struct {
    char* url;
    char* method;
    KryonHttpHeaders* headers;
    char* body;
    size_t body_size;
    
    // Response data
    int status_code;
    char* status_message;
    KryonHttpHeaders* response_headers;
    KryonHttpBuffer response_body;
    
    // Connection info
    int socket_fd;
    bool is_connected;
    char* host;
    int port;
    bool use_ssl;
    
    // Timeouts
    int connect_timeout;
    int read_timeout;
    
} KryonHttpRequest;

// =============================================================================
// HTTP BUFFER MANAGEMENT
// =============================================================================

static bool http_buffer_init(KryonHttpBuffer* buffer, size_t initial_capacity) {
    if (!buffer) return false;
    
    buffer->capacity = initial_capacity > 0 ? initial_capacity : 1024;
    buffer->data = kryon_alloc(buffer->capacity);
    if (!buffer->data) return false;
    
    buffer->size = 0;
    return true;
}

static void http_buffer_cleanup(KryonHttpBuffer* buffer) {
    if (buffer && buffer->data) {
        kryon_free(buffer->data);
        buffer->data = NULL;
        buffer->size = 0;
        buffer->capacity = 0;
    }
}

static bool http_buffer_append(KryonHttpBuffer* buffer, const char* data, size_t len) {
    if (!buffer || !data || len == 0) return false;
    
    // Expand buffer if needed
    while (buffer->size + len >= buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        char* new_data = kryon_alloc(new_capacity);
        if (!new_data) return false;
        
        if (buffer->data) {
            memcpy(new_data, buffer->data, buffer->size);
            kryon_free(buffer->data);
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    memcpy(buffer->data + buffer->size, data, len);
    buffer->size += len;
    buffer->data[buffer->size] = '\0';
    
    return true;
}

// =============================================================================
// URL PARSING
// =============================================================================

static bool parse_url(const char* url, char** host, int* port, char** path, bool* use_ssl) {
    if (!url || !host || !port || !path || !use_ssl) return false;
    
    *host = NULL;
    *path = NULL;
    *port = 80;
    *use_ssl = false;
    
    // Check for protocol
    if (strncmp(url, "https://", 8) == 0) {
        *use_ssl = true;
        *port = 443;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    }
    
    // Find end of host
    const char* path_start = strchr(url, '/');
    const char* port_start = strchr(url, ':');
    
    size_t host_len;
    if (port_start && (!path_start || port_start < path_start)) {
        // Port specified
        host_len = port_start - url;
        *port = atoi(port_start + 1);
    } else if (path_start) {
        host_len = path_start - url;
    } else {
        host_len = strlen(url);
    }
    
    // Extract host
    *host = kryon_alloc(host_len + 1);
    if (!*host) return false;
    
    strncpy(*host, url, host_len);
    (*host)[host_len] = '\0';
    
    // Extract path
    if (path_start) {
        size_t path_len = strlen(path_start);
        *path = kryon_alloc(path_len + 1);
        if (!*path) {
            kryon_free(*host);
            *host = NULL;
            return false;
        }
        strcpy(*path, path_start);
    } else {
        *path = kryon_alloc(2);
        if (*path) {
            strcpy(*path, "/");
        }
    }
    
    return true;
}

// =============================================================================
// SOCKET OPERATIONS
// =============================================================================

static int create_socket_connection(const char* host, int port, int timeout) {
    struct hostent* server = gethostbyname(host);
    if (!server) return -1;
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    // Set timeout
    if (timeout > 0) {
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

static ssize_t socket_send_all(int sockfd, const char* data, size_t len) {
    size_t total_sent = 0;
    
    while (total_sent < len) {
        ssize_t sent = send(sockfd, data + total_sent, len - total_sent, 0);
        if (sent <= 0) return -1;
        total_sent += sent;
    }
    
    return total_sent;
}

static ssize_t socket_recv_line(int sockfd, char* buffer, size_t buffer_size) {
    size_t total_read = 0;
    
    while (total_read < buffer_size - 1) {
        char c;
        ssize_t bytes_read = recv(sockfd, &c, 1, 0);
        
        if (bytes_read <= 0) break;
        
        buffer[total_read++] = c;
        
        if (c == '\n') break;
    }
    
    buffer[total_read] = '\0';
    return total_read;
}

// =============================================================================
// HTTP PROTOCOL
// =============================================================================

static char* build_http_request(KryonHttpRequest* req) {
    if (!req) return NULL;
    
    // Calculate buffer size needed
    size_t buffer_size = 1024;
    if (req->body) buffer_size += req->body_size;
    
    // Add headers size estimation
    if (req->headers) {
        for (size_t i = 0; i < req->headers->count; i++) {
            buffer_size += strlen(req->headers->headers[i].name) + 
                          strlen(req->headers->headers[i].value) + 4;
        }
    }
    
    char* request = kryon_alloc(buffer_size);
    if (!request) return NULL;
    
    // Build request line
    char* path = req->url;
    if (strstr(req->url, "://")) {
        path = strchr(req->url + 8, '/');
        if (!path) path = "/";
    }
    
    snprintf(request, buffer_size, "%s %s HTTP/1.1\r\nHost: %s\r\n", 
             req->method, path, req->host);
    
    // Add headers
    if (req->headers) {
        for (size_t i = 0; i < req->headers->count; i++) {
            char header_line[512];
            snprintf(header_line, sizeof(header_line), "%s: %s\r\n",
                    req->headers->headers[i].name, req->headers->headers[i].value);
            strcat(request, header_line);
        }
    }
    
    // Add content length if body exists
    if (req->body && req->body_size > 0) {
        char content_length[64];
        snprintf(content_length, sizeof(content_length), "Content-Length: %zu\r\n", req->body_size);
        strcat(request, content_length);
    }
    
    // Add connection header
    strcat(request, "Connection: close\r\n");
    
    // End headers
    strcat(request, "\r\n");
    
    // Add body if present
    if (req->body && req->body_size > 0) {
        size_t current_len = strlen(request);
        if (current_len + req->body_size < buffer_size) {
            memcpy(request + current_len, req->body, req->body_size);
            request[current_len + req->body_size] = '\0';
        }
    }
    
    return request;
}

static bool parse_response_headers(KryonHttpRequest* req, int sockfd) {
    char line[1024];
    
    // Read status line
    if (socket_recv_line(sockfd, line, sizeof(line)) <= 0) {
        return false;
    }
    
    // Parse status line: HTTP/1.1 200 OK
    char* status_str = strchr(line, ' ');
    if (status_str) {
        req->status_code = atoi(status_str + 1);
        
        char* message_start = strchr(status_str + 1, ' ');
        if (message_start) {
            message_start++;
            // Remove \r\n
            char* line_end = strchr(message_start, '\r');
            if (line_end) *line_end = '\0';
            
            req->status_message = kryon_alloc(strlen(message_start) + 1);
            if (req->status_message) {
                strcpy(req->status_message, message_start);
            }
        }
    }
    
    // Initialize response headers
    req->response_headers = kryon_alloc(sizeof(KryonHttpHeaders));
    if (!req->response_headers) return false;
    
    req->response_headers->count = 0;
    req->response_headers->capacity = 16;
    req->response_headers->headers = kryon_alloc(sizeof(KryonHttpHeader) * req->response_headers->capacity);
    if (!req->response_headers->headers) return false;
    
    // Read headers
    while (socket_recv_line(sockfd, line, sizeof(line)) > 0) {
        // Remove \r\n
        char* line_end = strchr(line, '\r');
        if (line_end) *line_end = '\0';
        
        // Empty line indicates end of headers
        if (strlen(line) == 0) break;
        
        // Parse header: Name: Value
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char* value = colon + 1;
            
            // Skip leading whitespace in value
            while (*value == ' ' || *value == '\t') value++;
            
            // Expand headers array if needed
            if (req->response_headers->count >= req->response_headers->capacity) {
                size_t new_capacity = req->response_headers->capacity * 2;
                KryonHttpHeader* new_headers = kryon_alloc(sizeof(KryonHttpHeader) * new_capacity);
                if (!new_headers) break;
                
                memcpy(new_headers, req->response_headers->headers, 
                       sizeof(KryonHttpHeader) * req->response_headers->count);
                kryon_free(req->response_headers->headers);
                
                req->response_headers->headers = new_headers;
                req->response_headers->capacity = new_capacity;
            }
            
            // Add header
            KryonHttpHeader* header = &req->response_headers->headers[req->response_headers->count];
            header->name = kryon_alloc(strlen(line) + 1);
            header->value = kryon_alloc(strlen(value) + 1);
            
            if (header->name && header->value) {
                strcpy(header->name, line);
                strcpy(header->value, value);
                req->response_headers->count++;
            }
        }
    }
    
    return true;
}

static bool read_response_body(KryonHttpRequest* req, int sockfd) {
    if (!http_buffer_init(&req->response_body, 4096)) {
        return false;
    }
    
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        if (!http_buffer_append(&req->response_body, buffer, bytes_read)) {
            break;
        }
    }
    
    return true;
}

// =============================================================================
// PUBLIC API
// =============================================================================

KryonHttpClient* kryon_http_client_create(void) {
    KryonHttpClient* client = kryon_alloc(sizeof(KryonHttpClient));
    if (!client) return NULL;
    
    memset(client, 0, sizeof(KryonHttpClient));
    
    client->connect_timeout = 30;
    client->read_timeout = 30;
    client->max_redirects = 5;
    client->follow_redirects = true;
    
    return client;
}

void kryon_http_client_destroy(KryonHttpClient* client) {
    if (!client) return;
    
    if (client->user_agent) {
        kryon_free(client->user_agent);
    }
    
    kryon_free(client);
}

void kryon_http_client_set_timeout(KryonHttpClient* client, int connect_timeout, int read_timeout) {
    if (!client) return;
    
    client->connect_timeout = connect_timeout;
    client->read_timeout = read_timeout;
}

void kryon_http_client_set_user_agent(KryonHttpClient* client, const char* user_agent) {
    if (!client || !user_agent) return;
    
    if (client->user_agent) {
        kryon_free(client->user_agent);
    }
    
    client->user_agent = kryon_alloc(strlen(user_agent) + 1);
    if (client->user_agent) {
        strcpy(client->user_agent, user_agent);
    }
}

KryonHttpResponse* kryon_http_request(KryonHttpClient* client, const char* method, 
                                     const char* url, KryonHttpHeaders* headers, 
                                     const char* body, size_t body_size) {
    if (!client || !method || !url) return NULL;
    
    // Create request
    KryonHttpRequest req = {0};
    req.method = (char*)method;
    req.url = (char*)url;
    req.headers = headers;
    req.body = (char*)body;
    req.body_size = body_size;
    req.connect_timeout = client->connect_timeout;
    req.read_timeout = client->read_timeout;
    
    // Parse URL
    char* host = NULL;
    char* path = NULL;
    if (!parse_url(url, &host, &req.port, &path, &req.use_ssl)) {
        return NULL;
    }
    
    req.host = host;
    
    // Create connection
    req.socket_fd = create_socket_connection(host, req.port, req.connect_timeout);
    if (req.socket_fd < 0) {
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    req.is_connected = true;
    
    // Build and send request
    char* request_str = build_http_request(&req);
    if (!request_str) {
        close(req.socket_fd);
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    if (socket_send_all(req.socket_fd, request_str, strlen(request_str)) < 0) {
        kryon_free(request_str);
        close(req.socket_fd);
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    kryon_free(request_str);
    
    // Read response
    if (!parse_response_headers(&req, req.socket_fd) || 
        !read_response_body(&req, req.socket_fd)) {
        close(req.socket_fd);
        kryon_free(host);
        kryon_free(path);
        return NULL;
    }
    
    close(req.socket_fd);
    
    // Create response object
    KryonHttpResponse* response = kryon_alloc(sizeof(KryonHttpResponse));
    if (!response) {
        kryon_free(host);
        kryon_free(path);
        http_buffer_cleanup(&req.response_body);
        return NULL;
    }
    
    response->status_code = req.status_code;
    response->status_message = req.status_message;
    response->headers = req.response_headers;
    response->body = req.response_body.data;
    response->body_size = req.response_body.size;
    response->success = (req.status_code >= 200 && req.status_code < 300);
    
    kryon_free(host);
    kryon_free(path);
    
    return response;
}

KryonHttpResponse* kryon_http_get(KryonHttpClient* client, const char* url) {
    return kryon_http_request(client, "GET", url, NULL, NULL, 0);
}

KryonHttpResponse* kryon_http_post(KryonHttpClient* client, const char* url, 
                                  const char* body, size_t body_size) {
    return kryon_http_request(client, "POST", url, NULL, body, body_size);
}

void kryon_http_response_destroy(KryonHttpResponse* response) {
    if (!response) return;
    
    if (response->status_message) {
        kryon_free(response->status_message);
    }
    
    if (response->headers) {
        for (size_t i = 0; i < response->headers->count; i++) {
            kryon_free(response->headers->headers[i].name);
            kryon_free(response->headers->headers[i].value);
        }
        kryon_free(response->headers->headers);
        kryon_free(response->headers);
    }
    
    if (response->body) {
        kryon_free(response->body);
    }
    
    kryon_free(response);
}

KryonHttpHeaders* kryon_http_headers_create(void) {
    KryonHttpHeaders* headers = kryon_alloc(sizeof(KryonHttpHeaders));
    if (!headers) return NULL;
    
    headers->count = 0;
    headers->capacity = 8;
    headers->headers = kryon_alloc(sizeof(KryonHttpHeader) * headers->capacity);
    
    if (!headers->headers) {
        kryon_free(headers);
        return NULL;
    }
    
    return headers;
}

void kryon_http_headers_destroy(KryonHttpHeaders* headers) {
    if (!headers) return;
    
    for (size_t i = 0; i < headers->count; i++) {
        kryon_free(headers->headers[i].name);
        kryon_free(headers->headers[i].value);
    }
    
    kryon_free(headers->headers);
    kryon_free(headers);
}

bool kryon_http_headers_add(KryonHttpHeaders* headers, const char* name, const char* value) {
    if (!headers || !name || !value) return false;
    
    // Expand if needed
    if (headers->count >= headers->capacity) {
        size_t new_capacity = headers->capacity * 2;
        KryonHttpHeader* new_headers = kryon_alloc(sizeof(KryonHttpHeader) * new_capacity);
        if (!new_headers) return false;
        
        memcpy(new_headers, headers->headers, sizeof(KryonHttpHeader) * headers->count);
        kryon_free(headers->headers);
        
        headers->headers = new_headers;
        headers->capacity = new_capacity;
    }
    
    // Add header
    KryonHttpHeader* header = &headers->headers[headers->count];
    header->name = kryon_alloc(strlen(name) + 1);
    header->value = kryon_alloc(strlen(value) + 1);
    
    if (!header->name || !header->value) {
        kryon_free(header->name);
        kryon_free(header->value);
        return false;
    }
    
    strcpy(header->name, name);
    strcpy(header->value, value);
    headers->count++;
    
    return true;
}

const char* kryon_http_headers_get(KryonHttpHeaders* headers, const char* name) {
    if (!headers || !name) return NULL;
    
    for (size_t i = 0; i < headers->count; i++) {
        if (strcasecmp(headers->headers[i].name, name) == 0) {
            return headers->headers[i].value;
        }
    }
    
    return NULL;
}