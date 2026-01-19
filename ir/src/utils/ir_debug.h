#ifndef IR_DEBUG_H
#define IR_DEBUG_H

#include <stdio.h>

// Debug levels
#define IR_DEBUG_LEVEL_ERROR 1
#define IR_DEBUG_LEVEL_WARN  2
#define IR_DEBUG_LEVEL_INFO  3
#define IR_DEBUG_LEVEL_TRACE 4

// Set default debug level based on DEBUG macro
#ifndef IR_DEBUG_LEVEL
  #ifdef DEBUG
    #define IR_DEBUG_LEVEL IR_DEBUG_LEVEL_INFO
  #else
    #define IR_DEBUG_LEVEL IR_DEBUG_LEVEL_ERROR
  #endif
#endif

// Debug macros - compile out at appropriate levels
#define IR_DEBUG_ERR(fmt, ...) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_ERROR) \
        fprintf(stderr, "[IR ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)

#define IR_DEBUG_WARN(fmt, ...) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_WARN) \
        fprintf(stderr, "[IR WARN] " fmt "\n", ##__VA_ARGS__); } while(0)

#define IR_DEBUG_INFO(fmt, ...) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_INFO) \
        fprintf(stderr, "[IR INFO] " fmt "\n", ##__VA_ARGS__); } while(0)

#define IR_DEBUG_TRACE(fmt, ...) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_TRACE) \
        fprintf(stderr, "[IR TRACE] " fmt "\n", ##__VA_ARGS__); } while(0)

// Memory operation tracing
#define IR_DEBUG_MEM(op, ptr, size) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_TRACE) \
        fprintf(stderr, "[IR MEM] %s: ptr=%p size=%zu\n", op, (void*)ptr, (size_t)size); } while(0)

// Buffer operation tracing
#define IR_DEBUG_BUFFER(op, buffer, size) \
    do { if (IR_DEBUG_LEVEL >= IR_DEBUG_LEVEL_TRACE) \
        fprintf(stderr, "[IR BUFFER] %s: base=%p data=%p size=%zu capacity=%zu\n", \
            op, (void*)(buffer)->base, (void*)(buffer)->data, (size_t)size, (buffer)->capacity); } while(0)

#endif // IR_DEBUG_H
