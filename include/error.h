/**
 * @file error.h
 * @brief Kryon Error Handling System
 * 
 * Comprehensive error handling with codes, messages, stack traces, and logging.
 * Provides structured error propagation and debugging aids.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_ERROR_H
#define KRYON_INTERNAL_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonError KryonError;
typedef struct KryonErrorContext KryonErrorContext;
typedef struct KryonStackTrace KryonStackTrace;
typedef struct KryonLogger KryonLogger;

// =============================================================================
// ERROR CODES
// =============================================================================

/**
 * @brief Kryon result/error codes
 */
typedef enum {
    // Success
    KRYON_SUCCESS = 0,
    
    // General errors (1-99)
    KRYON_ERROR_INVALID_ARGUMENT = 1,
    KRYON_ERROR_NULL_POINTER = 2,
    KRYON_ERROR_OUT_OF_BOUNDS = 3,
    KRYON_ERROR_INVALID_STATE = 4,
    KRYON_ERROR_NOT_IMPLEMENTED = 5,
    KRYON_ERROR_TIMEOUT = 6,
    KRYON_ERROR_CANCELLED = 7,
    KRYON_ERROR_UNKNOWN = 99,
    
    // Memory errors (100-199)
    KRYON_ERROR_OUT_OF_MEMORY = 100,
    KRYON_ERROR_MEMORY_CORRUPTION = 101,
    KRYON_ERROR_DOUBLE_FREE = 102,
    KRYON_ERROR_MEMORY_LEAK = 103,
    KRYON_ERROR_BUFFER_OVERFLOW = 104,
    KRYON_ERROR_BUFFER_UNDERFLOW = 105,
    KRYON_ERROR_ALIGNMENT_ERROR = 106,
    
    // File/IO errors (200-299)
    KRYON_ERROR_FILE_NOT_FOUND = 200,
    KRYON_ERROR_FILE_ACCESS_DENIED = 201,
    KRYON_ERROR_FILE_READ_ERROR = 202,
    KRYON_ERROR_FILE_WRITE_ERROR = 203,
    KRYON_ERROR_FILE_CORRUPT = 204,
    KRYON_ERROR_DIRECTORY_NOT_FOUND = 205,
    KRYON_ERROR_DISK_FULL = 206,
    
    // Parsing/Compilation errors (300-399)
    KRYON_ERROR_SYNTAX_ERROR = 300,
    KRYON_ERROR_PARSE_ERROR = 301,
    KRYON_ERROR_SEMANTIC_ERROR = 302,
    KRYON_ERROR_TYPE_ERROR = 303,
    KRYON_ERROR_COMPILATION_FAILED = 304,
    KRYON_ERROR_INVALID_KRB_FILE = 305,
    KRYON_ERROR_VERSION_MISMATCH = 306,
    
    // Runtime errors (400-499)
    KRYON_ERROR_RUNTIME_ERROR = 400,
    KRYON_ERROR_STACK_OVERFLOW = 401,
    KRYON_ERROR_DIVISION_BY_ZERO = 402,
    KRYON_ERROR_ASSERTION_FAILED = 403,
    KRYON_ERROR_UNHANDLED_EXCEPTION = 404,
    KRYON_ERROR_INVALID_OPERATION = 405,
    
    // Platform errors (500-599)
    KRYON_ERROR_PLATFORM_ERROR = 500,
    KRYON_ERROR_WINDOW_CREATION_FAILED = 501,
    KRYON_ERROR_RENDERER_INIT_FAILED = 502,
    KRYON_ERROR_AUDIO_INIT_FAILED = 503,
    KRYON_ERROR_INPUT_INIT_FAILED = 504,
    KRYON_ERROR_GRAPHICS_DRIVER_ERROR = 505,
    
    // Network errors (600-699)
    KRYON_ERROR_NETWORK_ERROR = 600,
    KRYON_ERROR_CONNECTION_FAILED = 601,
    KRYON_ERROR_CONNECTION_TIMEOUT = 602,
    KRYON_ERROR_CONNECTION_REFUSED = 603,
    KRYON_ERROR_DNS_RESOLUTION_FAILED = 604,
    KRYON_ERROR_HTTP_ERROR = 605,
    KRYON_ERROR_WEBSOCKET_ERROR = 606,
    
    // Script errors (700-799)
    KRYON_ERROR_SCRIPT_ERROR = 700,
    KRYON_ERROR_SCRIPT_COMPILE_ERROR = 701,
    KRYON_ERROR_SCRIPT_RUNTIME_ERROR = 702,
    KRYON_ERROR_SCRIPT_TYPE_ERROR = 703,
    KRYON_ERROR_FUNCTION_NOT_FOUND = 704,
    KRYON_ERROR_SCRIPT_TIMEOUT = 705,
    
    // UI/Layout errors (800-899)
    KRYON_ERROR_UI_ERROR = 800,
    KRYON_ERROR_ELEMENT_NOT_FOUND = 801,
    KRYON_ERROR_INVALID_PROPERTY = 802,
    KRYON_ERROR_LAYOUT_ERROR = 803,
    KRYON_ERROR_RENDER_ERROR = 804,
    KRYON_ERROR_STYLE_ERROR = 805,
    
    // Resource errors (900-999)
    KRYON_ERROR_RESOURCE_ERROR = 900,
    KRYON_ERROR_RESOURCE_NOT_FOUND = 901,
    KRYON_ERROR_RESOURCE_LOAD_FAILED = 902,
    KRYON_ERROR_INVALID_RESOURCE = 903,
    KRYON_ERROR_RESOURCE_LOCKED = 904,
    
} KryonResult;

// =============================================================================
// ERROR SEVERITY LEVELS
// =============================================================================

/**
 * @brief Error severity levels
 */
typedef enum {
    KRYON_SEVERITY_TRACE = 0,    ///< Detailed tracing information
    KRYON_SEVERITY_DEBUG = 1,    ///< Debug information
    KRYON_SEVERITY_INFO = 2,     ///< General information
    KRYON_SEVERITY_WARNING = 3,  ///< Warning conditions
    KRYON_SEVERITY_ERROR = 4,    ///< Error conditions
    KRYON_SEVERITY_CRITICAL = 5, ///< Critical error conditions
    KRYON_SEVERITY_FATAL = 6     ///< Fatal error conditions
} KryonSeverity;

// =============================================================================
// STACK TRACE
// =============================================================================

#define KRYON_MAX_STACK_FRAMES 32

/**
 * @brief Stack frame information
 */
typedef struct {
    void *address;          ///< Address of the frame
    const char *function;   ///< Function name (if available)
    const char *file;       ///< Source file name (if available)
    int line;               ///< Source line number (if available)
    size_t offset;          ///< Offset within function
} KryonStackFrame;

/**
 * @brief Stack trace information
 */
struct KryonStackTrace {
    KryonStackFrame frames[KRYON_MAX_STACK_FRAMES]; ///< Stack frames
    size_t frame_count;                             ///< Number of frames
    uint64_t timestamp;                             ///< When trace was captured
    uint32_t thread_id;                             ///< Thread that generated trace
};

// =============================================================================
// ERROR STRUCTURE
// =============================================================================

/**
 * @brief Comprehensive error information
 */
struct KryonError {
    KryonResult code;               ///< Error code
    KryonSeverity severity;         ///< Error severity
    const char *message;            ///< Error message
    const char *details;            ///< Detailed error description
    const char *file;               ///< Source file where error occurred
    int line;                       ///< Source line where error occurred
    const char *function;           ///< Function where error occurred
    uint64_t timestamp;             ///< When error occurred
    uint32_t thread_id;             ///< Thread that generated error
    KryonStackTrace *stack_trace;   ///< Stack trace (if available)
    struct KryonError *cause;       ///< Underlying cause (chained errors)
    void *user_data;                ///< User-defined data
    size_t user_data_size;          ///< Size of user data
};

// =============================================================================
// ERROR CONTEXT
// =============================================================================

/**
 * @brief Error context for thread-local error handling
 */
struct KryonErrorContext {
    KryonError *last_error;         ///< Last error that occurred
    KryonError *error_chain;        ///< Chain of errors
    size_t error_count;             ///< Number of errors in chain
    bool capture_stack_traces;      ///< Whether to capture stack traces
    bool break_on_error;            ///< Whether to break into debugger
    FILE *log_file;                 ///< Log file for errors
    KryonSeverity min_log_level;    ///< Minimum level to log
};

// =============================================================================
// LOGGING SYSTEM
// =============================================================================

/**
 * @brief Log callback function type
 */
typedef void (*KryonLogCallback)(KryonSeverity severity, const char *message, 
                                const char *file, int line, const char *function,
                                void *user_data);

/**
 * @brief Logger configuration
 */
struct KryonLogger {
    KryonLogCallback callback;      ///< Log callback function
    void *user_data;                ///< User data for callback
    FILE *log_file;                 ///< Log file handle
    KryonSeverity min_level;        ///< Minimum log level
    bool include_timestamp;         ///< Include timestamp in logs
    bool include_thread_id;         ///< Include thread ID in logs
    bool include_location;          ///< Include file/line/function
    bool color_output;              ///< Use colored output (if supported)
    bool async_logging;             ///< Use asynchronous logging
};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize error handling system
 * @return KRYON_SUCCESS on success, error code on failure
 */
KryonResult kryon_error_init(void);

/**
 * @brief Shutdown error handling system
 */
void kryon_error_shutdown(void);

/**
 * @brief Get thread-local error context
 * @return Pointer to error context, or NULL on failure
 */
KryonErrorContext *kryon_error_get_context(void);

/**
 * @brief Create a new error
 * @param code Error code
 * @param severity Error severity
 * @param message Error message
 * @param file Source file name
 * @param line Source line number
 * @param function Source function name
 * @return Pointer to new error, or NULL on failure
 */
KryonError *kryon_error_create(KryonResult code, KryonSeverity severity, const char *message,
                              const char *file, int line, const char *function);

/**
 * @brief Destroy an error
 * @param error Error to destroy
 */
void kryon_error_destroy(KryonError *error);

/**
 * @brief Set the last error for current thread
 * @param error Error to set (takes ownership)
 */
void kryon_error_set(KryonError *error);

/**
 * @brief Get the last error for current thread
 * @return Pointer to last error, or NULL if no error
 */
const KryonError *kryon_error_get_last(void);

/**
 * @brief Clear the last error for current thread
 */
void kryon_error_clear(void);

/**
 * @brief Chain an error (set as cause of current error)
 * @param cause Underlying cause error
 */
void kryon_error_chain(KryonError *cause);

/**
 * @brief Get error message for error code
 * @param code Error code
 * @return Error message string
 */
const char *kryon_error_get_message(KryonResult code);

/**
 * @brief Format error as string
 * @param error Error to format
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written
 */
size_t kryon_error_format(const KryonError *error, char *buffer, size_t buffer_size);

/**
 * @brief Print error to file
 * @param error Error to print
 * @param file File to print to (NULL for stderr)
 */
void kryon_error_print(const KryonError *error, FILE *file);

/**
 * @brief Capture stack trace
 * @param trace Output stack trace structure
 * @return Number of frames captured
 */
size_t kryon_error_capture_stack_trace(KryonStackTrace *trace);

/**
 * @brief Format stack trace as string
 * @param trace Stack trace to format
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written
 */
size_t kryon_error_format_stack_trace(const KryonStackTrace *trace, char *buffer, size_t buffer_size);

// =============================================================================
// LOGGING API
// =============================================================================

/**
 * @brief Initialize logger
 * @param logger Logger configuration
 * @return KRYON_SUCCESS on success, error code on failure
 */
KryonResult kryon_logger_init(const KryonLogger *logger);

/**
 * @brief Shutdown logger
 */
void kryon_logger_shutdown(void);

/**
 * @brief Log a message
 * @param severity Message severity
 * @param file Source file name
 * @param line Source line number
 * @param function Source function name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void kryon_log(KryonSeverity severity, const char *file, int line, const char *function,
              const char *format, ...);

/**
 * @brief Set minimum log level
 * @param level Minimum level to log
 */
void kryon_log_set_level(KryonSeverity level);

/**
 * @brief Get current log level
 * @return Current minimum log level
 */
KryonSeverity kryon_log_get_level(void);

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

// Error creation macros
#define KRYON_ERROR_CREATE(code, severity, message) \
    kryon_error_create((code), (severity), (message), __FILE__, __LINE__, __FUNCTION__)

#define KRYON_ERROR_SET(code, severity, message) \
    kryon_error_set(KRYON_ERROR_CREATE((code), (severity), (message)))

// Result checking macros
#define KRYON_RETURN_IF_ERROR(result) \
    do { \
        KryonResult _kr_result = (result); \
        if (_kr_result != KRYON_SUCCESS) { \
            return _kr_result; \
        } \
    } while (0)

#define KRYON_GOTO_IF_ERROR(result, label) \
    do { \
        KryonResult _kr_result = (result); \
        if (_kr_result != KRYON_SUCCESS) { \
            goto label; \
        } \
    } while (0)

#define KRYON_SET_ERROR_AND_RETURN(code, severity, message) \
    do { \
        KRYON_ERROR_SET((code), (severity), (message)); \
        return (code); \
    } while (0)

// Logging macros
#ifdef KRYON_DEBUG
    #define KRYON_LOG_TRACE(format, ...) \
        kryon_log(KRYON_SEVERITY_TRACE, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
    #define KRYON_LOG_DEBUG(format, ...) \
        kryon_log(KRYON_SEVERITY_DEBUG, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#else
    #define KRYON_LOG_TRACE(format, ...) ((void)0)
    #define KRYON_LOG_DEBUG(format, ...) ((void)0)
#endif

#define KRYON_LOG_INFO(format, ...) \
    kryon_log(KRYON_SEVERITY_INFO, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define KRYON_LOG_WARNING(format, ...) \
    kryon_log(KRYON_SEVERITY_WARNING, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define KRYON_LOG_ERROR(format, ...) \
    kryon_log(KRYON_SEVERITY_ERROR, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define KRYON_LOG_CRITICAL(format, ...) \
    kryon_log(KRYON_SEVERITY_CRITICAL, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define KRYON_LOG_FATAL(format, ...) \
    kryon_log(KRYON_SEVERITY_FATAL, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

// Assertion macros
#ifdef KRYON_DEBUG
    #define KRYON_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                KRYON_LOG_FATAL("Assertion failed: %s - %s", #condition, message); \
                KRYON_ERROR_SET(KRYON_ERROR_ASSERTION_FAILED, KRYON_SEVERITY_FATAL, \
                               "Assertion failed: " #condition); \
                abort(); \
            } \
        } while (0)
#else
    #define KRYON_ASSERT(condition, message) ((void)0)
#endif

#define KRYON_ASSERT_NOT_NULL(ptr) \
    KRYON_ASSERT((ptr) != NULL, "Pointer must not be NULL")

#define KRYON_ASSERT_VALID_INDEX(index, size) \
    KRYON_ASSERT((index) < (size), "Index out of bounds")

// Error propagation helpers
#define KRYON_TRY(expression) \
    do { \
        KryonResult _kr_result = (expression); \
        if (_kr_result != KRYON_SUCCESS) { \
            KRYON_LOG_ERROR("Operation failed: %s", kryon_error_get_message(_kr_result)); \
            return _kr_result; \
        } \
    } while (0)

#define KRYON_TRY_GOTO(expression, label) \
    do { \
        KryonResult _kr_result = (expression); \
        if (_kr_result != KRYON_SUCCESS) { \
            KRYON_LOG_ERROR("Operation failed: %s", kryon_error_get_message(_kr_result)); \
            goto label; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_ERROR_H