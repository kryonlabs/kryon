/**
 * @file fetch_api.c
 * @brief Kryon Fetch API Implementation (Web-compatible)
 */

#include "internal/network.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// FETCH STRUCTURES
// =============================================================================

struct KryonFetchRequest {
    char* url;
    char* method;
    KryonHttpHeaders* headers;
    char* body;
    size_t body_size;
    
    // Fetch options
    KryonFetchMode mode;
    KryonFetchCredentials credentials;
    KryonFetchCache cache;
    KryonFetchRedirect redirect;
    
    // Timeouts and limits
    int timeout;
    int max_redirects;
    
    // Body type
    KryonFetchBodyType body_type;
};

struct KryonFetchResponse {
    int status;
    char* status_text;
    KryonHttpHeaders* headers;
    char* url; // Final URL after redirects
    
    // Body data
    char* body;
    size_t body_size;
    
    // Response metadata
    bool ok;
    bool redirected;
    KryonFetchResponseType type;
    
    // Internal HTTP client response
    KryonHttpResponse* http_response;
};

typedef struct {
    KryonFetchRequest* request;
    KryonFetchResponse* response;
    KryonFetchCallback callback;
    void* user_data;
    
    // Async state
    bool is_complete;
    bool has_error;
    char* error_message;
    
    // Promise-like state
    KryonFetchPromiseState state;
    
} KryonFetchOperation;

// =============================================================================
// MIME TYPE DETECTION
// =============================================================================

static const char* detect_mime_type(const char* data, size_t size) {
    if (!data || size == 0) return "text/plain";
    
    // Check common binary signatures
    if (size >= 4) {
        if (memcmp(data, "\x89PNG", 4) == 0) return "image/png";
        if (memcmp(data, "GIF8", 4) == 0) return "image/gif";
        if (memcmp(data, "\xFF\xD8\xFF", 3) == 0) return "image/jpeg";
        if (memcmp(data, "RIFF", 4) == 0 && size >= 12 && memcmp(data + 8, "WEBP", 4) == 0) return "image/webp";
        if (memcmp(data, "%PDF", 4) == 0) return "application/pdf";
        if (memcmp(data, "PK\x03\x04", 4) == 0) return "application/zip";
    }
    
    // Check for JSON
    if (data[0] == '{' || data[0] == '[') {
        return "application/json";
    }
    
    // Check for XML
    if (data[0] == '<') {
        if (strncmp(data, "<?xml", 5) == 0) return "application/xml";
        if (strncmp(data, "<!DOCTYPE html", 14) == 0 || strncmp(data, "<html", 5) == 0) return "text/html";
        return "text/xml";
    }
    
    // Default to text/plain
    return "text/plain";
}

// =============================================================================
// REQUEST BUILDING
// =============================================================================

static KryonHttpHeaders* build_fetch_headers(KryonFetchRequest* req) {
    KryonHttpHeaders* headers = kryon_http_headers_create();
    if (!headers) return NULL;
    
    // Add user-provided headers first
    if (req->headers) {
        for (size_t i = 0; i < req->headers->count; i++) {
            kryon_http_headers_add(headers, req->headers->headers[i].name, req->headers->headers[i].value);
        }
    }
    
    // Add default headers if not already present
    if (!kryon_http_headers_get(headers, "User-Agent")) {
        kryon_http_headers_add(headers, "User-Agent", "Kryon-Fetch/1.0");
    }
    
    if (!kryon_http_headers_get(headers, "Accept")) {
        kryon_http_headers_add(headers, "Accept", "*/*");
    }
    
    // Add content-type for requests with body
    if (req->body && req->body_size > 0 && !kryon_http_headers_get(headers, "Content-Type")) {
        switch (req->body_type) {
            case KRYON_FETCH_BODY_JSON:
                kryon_http_headers_add(headers, "Content-Type", "application/json");
                break;
            case KRYON_FETCH_BODY_FORM:
                kryon_http_headers_add(headers, "Content-Type", "application/x-www-form-urlencoded");
                break;
            case KRYON_FETCH_BODY_TEXT:
                kryon_http_headers_add(headers, "Content-Type", "text/plain");
                break;
            case KRYON_FETCH_BODY_BINARY:
                kryon_http_headers_add(headers, "Content-Type", "application/octet-stream");
                break;
            default:
                // Auto-detect
                kryon_http_headers_add(headers, "Content-Type", detect_mime_type(req->body, req->body_size));
                break;
        }
    }
    
    // Add cache control headers
    switch (req->cache) {
        case KRYON_FETCH_CACHE_NO_CACHE:
            kryon_http_headers_add(headers, "Cache-Control", "no-cache");
            break;
        case KRYON_FETCH_CACHE_NO_STORE:
            kryon_http_headers_add(headers, "Cache-Control", "no-store");
            break;
        case KRYON_FETCH_CACHE_RELOAD:
            kryon_http_headers_add(headers, "Cache-Control", "no-cache");
            kryon_http_headers_add(headers, "Pragma", "no-cache");
            break;
        case KRYON_FETCH_CACHE_FORCE_CACHE:
            kryon_http_headers_add(headers, "Cache-Control", "only-if-cached");
            break;
        default:
            break;
    }
    
    return headers;
}

// =============================================================================
// RESPONSE PROCESSING
// =============================================================================

static KryonFetchResponse* create_fetch_response(KryonHttpResponse* http_response, const char* final_url) {
    if (!http_response) return NULL;
    
    KryonFetchResponse* response = kryon_alloc(sizeof(KryonFetchResponse));
    if (!response) return NULL;
    
    memset(response, 0, sizeof(KryonFetchResponse));
    
    response->status = http_response->status_code;
    response->ok = http_response->success;
    response->http_response = http_response;
    
    // Copy status text
    if (http_response->status_message) {
        response->status_text = kryon_alloc(strlen(http_response->status_message) + 1);
        if (response->status_text) {
            strcpy(response->status_text, http_response->status_message);
        }
    }
    
    // Copy headers
    if (http_response->headers) {
        response->headers = kryon_http_headers_create();
        if (response->headers) {
            for (size_t i = 0; i < http_response->headers->count; i++) {
                kryon_http_headers_add(response->headers, 
                                     http_response->headers->headers[i].name,
                                     http_response->headers->headers[i].value);
            }
        }
    }
    
    // Copy final URL
    if (final_url) {
        response->url = kryon_alloc(strlen(final_url) + 1);
        if (response->url) {
            strcpy(response->url, final_url);
        }
    }
    
    // Copy body
    if (http_response->body && http_response->body_size > 0) {
        response->body = kryon_alloc(http_response->body_size + 1);
        if (response->body) {
            memcpy(response->body, http_response->body, http_response->body_size);
            response->body[http_response->body_size] = '\0';
            response->body_size = http_response->body_size;
        }
    }
    
    // Determine response type
    const char* content_type = kryon_http_headers_get(response->headers, "Content-Type");
    if (content_type) {
        if (strstr(content_type, "application/json")) {
            response->type = KRYON_FETCH_RESPONSE_JSON;
        } else if (strstr(content_type, "text/")) {
            response->type = KRYON_FETCH_RESPONSE_TEXT;
        } else if (strstr(content_type, "image/") || strstr(content_type, "audio/") || 
                  strstr(content_type, "video/") || strstr(content_type, "application/octet-stream")) {
            response->type = KRYON_FETCH_RESPONSE_BLOB;
        } else {
            response->type = KRYON_FETCH_RESPONSE_TEXT;
        }
    } else {
        response->type = KRYON_FETCH_RESPONSE_TEXT;
    }
    
    return response;
}

// =============================================================================
// PROMISE-LIKE IMPLEMENTATION
// =============================================================================

static KryonFetchOperation* create_fetch_operation(KryonFetchRequest* request) {
    KryonFetchOperation* op = kryon_alloc(sizeof(KryonFetchOperation));
    if (!op) return NULL;
    
    memset(op, 0, sizeof(KryonFetchOperation));
    op->request = request;
    op->state = KRYON_FETCH_PENDING;
    
    return op;
}

static void complete_fetch_operation(KryonFetchOperation* op, KryonFetchResponse* response) {
    if (!op) return;
    
    op->response = response;
    op->is_complete = true;
    op->state = KRYON_FETCH_FULFILLED;
    
    if (op->callback) {
        op->callback(response, NULL, op->user_data);
    }
}

static void fail_fetch_operation(KryonFetchOperation* op, const char* error) {
    if (!op) return;
    
    op->has_error = true;
    op->is_complete = true;
    op->state = KRYON_FETCH_REJECTED;
    
    if (error) {
        op->error_message = kryon_alloc(strlen(error) + 1);
        if (op->error_message) {
            strcpy(op->error_message, error);
        }
    }
    
    if (op->callback) {
        op->callback(NULL, op->error_message, op->user_data);
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

KryonFetchRequest* kryon_fetch_request_create(const char* url) {
    if (!url) return NULL;
    
    KryonFetchRequest* request = kryon_alloc(sizeof(KryonFetchRequest));
    if (!request) return NULL;
    
    memset(request, 0, sizeof(KryonFetchRequest));
    
    request->url = kryon_alloc(strlen(url) + 1);
    if (!request->url) {
        kryon_free(request);
        return NULL;
    }
    strcpy(request->url, url);
    
    // Set defaults
    request->method = kryon_alloc(4);
    if (request->method) {
        strcpy(request->method, "GET");
    }
    
    request->mode = KRYON_FETCH_MODE_CORS;
    request->credentials = KRYON_FETCH_CREDENTIALS_SAME_ORIGIN;
    request->cache = KRYON_FETCH_CACHE_DEFAULT;
    request->redirect = KRYON_FETCH_REDIRECT_FOLLOW;
    request->timeout = 30;
    request->max_redirects = 5;
    request->body_type = KRYON_FETCH_BODY_AUTO;
    
    return request;
}

void kryon_fetch_request_destroy(KryonFetchRequest* request) {
    if (!request) return;
    
    kryon_free(request->url);
    kryon_free(request->method);
    kryon_free(request->body);
    
    if (request->headers) {
        kryon_http_headers_destroy(request->headers);
    }
    
    kryon_free(request);
}

bool kryon_fetch_request_set_method(KryonFetchRequest* request, const char* method) {
    if (!request || !method) return false;
    
    kryon_free(request->method);
    request->method = kryon_alloc(strlen(method) + 1);
    if (!request->method) return false;
    
    strcpy(request->method, method);
    return true;
}

bool kryon_fetch_request_set_body(KryonFetchRequest* request, const char* body, size_t body_size, KryonFetchBodyType type) {
    if (!request) return false;
    
    kryon_free(request->body);
    request->body = NULL;
    request->body_size = 0;
    
    if (body && body_size > 0) {
        request->body = kryon_alloc(body_size + 1);
        if (!request->body) return false;
        
        memcpy(request->body, body, body_size);
        request->body[body_size] = '\0';
        request->body_size = body_size;
    }
    
    request->body_type = type;
    return true;
}

bool kryon_fetch_request_set_json_body(KryonFetchRequest* request, const char* json) {
    if (!request || !json) return false;
    return kryon_fetch_request_set_body(request, json, strlen(json), KRYON_FETCH_BODY_JSON);
}

bool kryon_fetch_request_add_header(KryonFetchRequest* request, const char* name, const char* value) {
    if (!request || !name || !value) return false;
    
    if (!request->headers) {
        request->headers = kryon_http_headers_create();
        if (!request->headers) return false;
    }
    
    return kryon_http_headers_add(request->headers, name, value);
}

void kryon_fetch_request_set_timeout(KryonFetchRequest* request, int timeout_seconds) {
    if (request) {
        request->timeout = timeout_seconds;
    }
}

void kryon_fetch_request_set_cache(KryonFetchRequest* request, KryonFetchCache cache) {
    if (request) {
        request->cache = cache;
    }
}

KryonFetchPromise* kryon_fetch(KryonFetchRequest* request) {
    if (!request) return NULL;
    
    KryonFetchOperation* op = create_fetch_operation(request);
    if (!op) return NULL;
    
    // Create HTTP client
    KryonHttpClient* client = kryon_http_client_create();
    if (!client) {
        kryon_free(op);
        return NULL;
    }
    
    kryon_http_client_set_timeout(client, request->timeout, request->timeout);
    
    // Build headers
    KryonHttpHeaders* headers = build_fetch_headers(request);
    
    // Make HTTP request
    KryonHttpResponse* http_response = kryon_http_request(client, request->method, request->url, 
                                                         headers, request->body, request->body_size);
    
    if (headers) {
        kryon_http_headers_destroy(headers);
    }
    
    kryon_http_client_destroy(client);
    
    if (http_response) {
        KryonFetchResponse* response = create_fetch_response(http_response, request->url);
        if (response) {
            complete_fetch_operation(op, response);
        } else {
            fail_fetch_operation(op, "Failed to create response");
        }
    } else {
        fail_fetch_operation(op, "HTTP request failed");
    }
    
    return (KryonFetchPromise*)op;
}

KryonFetchPromise* kryon_fetch_url(const char* url) {
    KryonFetchRequest* request = kryon_fetch_request_create(url);
    if (!request) return NULL;
    
    KryonFetchPromise* promise = kryon_fetch(request);
    kryon_fetch_request_destroy(request);
    
    return promise;
}

void kryon_fetch_promise_then(KryonFetchPromise* promise, KryonFetchCallback callback, void* user_data) {
    if (!promise) return;
    
    KryonFetchOperation* op = (KryonFetchOperation*)promise;
    op->callback = callback;
    op->user_data = user_data;
    
    // If already complete, call callback immediately
    if (op->is_complete) {
        if (op->has_error) {
            callback(NULL, op->error_message, user_data);
        } else {
            callback(op->response, NULL, user_data);
        }
    }
}

bool kryon_fetch_promise_is_complete(KryonFetchPromise* promise) {
    if (!promise) return true;
    
    KryonFetchOperation* op = (KryonFetchOperation*)promise;
    return op->is_complete;
}

KryonFetchPromiseState kryon_fetch_promise_get_state(KryonFetchPromise* promise) {
    if (!promise) return KRYON_FETCH_REJECTED;
    
    KryonFetchOperation* op = (KryonFetchOperation*)promise;
    return op->state;
}

KryonFetchResponse* kryon_fetch_promise_get_response(KryonFetchPromise* promise) {
    if (!promise) return NULL;
    
    KryonFetchOperation* op = (KryonFetchOperation*)promise;
    return op->response;
}

void kryon_fetch_promise_destroy(KryonFetchPromise* promise) {
    if (!promise) return;
    
    KryonFetchOperation* op = (KryonFetchOperation*)promise;
    
    if (op->response) {
        kryon_fetch_response_destroy(op->response);
    }
    
    kryon_free(op->error_message);
    kryon_free(op);
}

void kryon_fetch_response_destroy(KryonFetchResponse* response) {
    if (!response) return;
    
    kryon_free(response->status_text);
    kryon_free(response->url);
    kryon_free(response->body);
    
    if (response->headers) {
        kryon_http_headers_destroy(response->headers);
    }
    
    if (response->http_response) {
        kryon_http_response_destroy(response->http_response);
    }
    
    kryon_free(response);
}

const char* kryon_fetch_response_text(KryonFetchResponse* response) {
    if (!response || !response->body) return NULL;
    return response->body;
}

const char* kryon_fetch_response_json(KryonFetchResponse* response) {
    if (!response || !response->body) return NULL;
    if (response->type != KRYON_FETCH_RESPONSE_JSON) return NULL;
    return response->body;
}

const uint8_t* kryon_fetch_response_bytes(KryonFetchResponse* response, size_t* size) {
    if (!response || !response->body) {
        if (size) *size = 0;
        return NULL;
    }
    
    if (size) *size = response->body_size;
    return (const uint8_t*)response->body;
}

int kryon_fetch_response_status(KryonFetchResponse* response) {
    if (!response) return 0;
    return response->status;
}

bool kryon_fetch_response_ok(KryonFetchResponse* response) {
    if (!response) return false;
    return response->ok;
}

const char* kryon_fetch_response_header(KryonFetchResponse* response, const char* name) {
    if (!response || !response->headers || !name) return NULL;
    return kryon_http_headers_get(response->headers, name);
}