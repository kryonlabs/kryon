/**
 * KRY Allocator - Chunk-based memory allocation implementation
 */

#include "kry_allocator.h"
#include "kry_ast.h"
#include <stdlib.h>
#include <string.h>

void* kry_alloc(KryParser* parser, size_t size) {
    if (!parser) return NULL;

    // Ensure we have a chunk with enough space
    if (!parser->current_chunk || parser->current_chunk->used + size > KRY_CHUNK_SIZE) {
        KryChunk* chunk = (KryChunk*)malloc(sizeof(KryChunk));
        if (!chunk) return NULL;

        chunk->used = 0;
        chunk->next = NULL;

        if (!parser->first_chunk) {
            parser->first_chunk = chunk;
        } else {
            parser->current_chunk->next = chunk;
        }
        parser->current_chunk = chunk;
    }

    void* ptr = parser->current_chunk->data + parser->current_chunk->used;
    parser->current_chunk->used += size;
    return ptr;
}

char* kry_strdup(KryParser* parser, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)kry_alloc(parser, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

char* kry_strndup(KryParser* parser, const char* str, size_t len) {
    if (!str) return NULL;
    char* copy = (char*)kry_alloc(parser, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}
