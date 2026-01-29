/**

 * @file error_system.c
 * @brief Kryon Error Handling System Implementation
 */
#include "lib9.h"


#include "error.h"
#include "memory.h"
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <dbghelp.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <execinfo.h>
    #include <sys/time.h>
#endif

// =============================================================================
// GLOBAL STATE
// =============================================================================

static bool g_error_system_initialized = false;
static KryonLogger g_logger = {0};

#ifdef _WIN32
static __declspec(thread) KryonErrorContext *g_error_context = NULL;
#else
static __thread KryonErrorContext *g_error_context __attribute__((unused)) = NULL;
static pthread_key_t g_error_context_key;
#endif

// =============================================================================
// ERROR MESSAGES
// =============================================================================

static const char *g_error_messages[] = {
    [KRYON_SUCCESS] = "Success",
    
    // General errors
    [KRYON_ERROR_INVALID_ARGUMENT] = "Invalid argument",
    [KRYON_ERROR_NULL_POINTER] = "Null pointer",
    [KRYON_ERROR_OUT_OF_BOUNDS] = "Out of bounds",
    [KRYON_ERROR_INVALID_STATE] = "Invalid state",
    [KRYON_ERROR_NOT_IMPLEMENTED] = "Not implemented",
    [KRYON_ERROR_TIMEOUT] = "Timeout",
    [KRYON_ERROR_CANCELLED] = "Cancelled",
    [KRYON_ERROR_UNKNOWN] = "Unknown error",
    
    // Memory errors
    [KRYON_ERROR_OUT_OF_MEMORY] = "Out of memory",
    [KRYON_ERROR_MEMORY_CORRUPTION] = "Memory corruption detected",
    [KRYON_ERROR_DOUBLE_FREE] = "Double free detected",
    [KRYON_ERROR_MEMORY_LEAK] = "Memory leak detected",
    [KRYON_ERROR_BUFFER_OVERFLOW] = "Buffer overflow",
    [KRYON_ERROR_BUFFER_UNDERFLOW] = "Buffer underflow",
    [KRYON_ERROR_ALIGNMENT_ERROR] = "Memory alignment error",
    
    // File/IO errors
    [KRYON_ERROR_FILE_NOT_FOUND] = "File not found",
    [KRYON_ERROR_FILE_ACCESS_DENIED] = "File access denied",
    [KRYON_ERROR_FILE_READ_ERROR] = "File read error",
    [KRYON_ERROR_FILE_WRITE_ERROR] = "File write error",
    [KRYON_ERROR_FILE_CORRUPT] = "File is corrupt",
    [KRYON_ERROR_DIRECTORY_NOT_FOUND] = "Directory not found",
    [KRYON_ERROR_DISK_FULL] = "Disk full",
    
    // Parsing/Compilation errors
    [KRYON_ERROR_SYNTAX_ERROR] = "Syntax error",
    [KRYON_ERROR_PARSE_ERROR] = "Parse error",
    [KRYON_ERROR_SEMANTIC_ERROR] = "Semantic error",
    [KRYON_ERROR_TYPE_ERROR] = "Type error",
    [KRYON_ERROR_COMPILATION_FAILED] = "Compilation failed",
    [KRYON_ERROR_INVALID_KRB_FILE] = "Invalid KRB file",
    [KRYON_ERROR_VERSION_MISMATCH] = "Version mismatch",
    
    [KRYON_ERROR_CODEGEN_FAILED] = "Code generation failed",
    [KRYON_ERROR_CODEGEN_UNSUPPORTED_NODE] = "Unsupported node type encountered during code generation",
    [KRYON_ERROR_CODEGEN_BUFFER_ERROR] = "An error occurred while writing to the codegen buffer",
    [KRYON_ERROR_CODEGEN_INVALID_AST] = "The AST provided for code generation is invalid or malformed",
    [KRYON_ERROR_CODEGEN_SYMBOL_CONFLICT] = "A symbol conflict was detected during code generation",

    // Runtime errors
    [KRYON_ERROR_RUNTIME_ERROR] = "Runtime error",
    [KRYON_ERROR_STACK_OVERFLOW] = "Stack overflow",
    [KRYON_ERROR_DIVISION_BY_ZERO] = "Division by zero",
    [KRYON_ERROR_ASSERTION_FAILED] = "Assertion failed",
    [KRYON_ERROR_UNHANDLED_EXCEPTION] = "Unhandled exception",
    [KRYON_ERROR_INVALID_OPERATION] = "Invalid operation",
    
    // Platform errors
    [KRYON_ERROR_PLATFORM_ERROR] = "Platform error",
    [KRYON_ERROR_WINDOW_CREATION_FAILED] = "Window creation failed",
    [KRYON_ERROR_RENDERER_INIT_FAILED] = "Renderer initialization failed",
    [KRYON_ERROR_AUDIO_INIT_FAILED] = "Audio initialization failed",
    [KRYON_ERROR_INPUT_INIT_FAILED] = "Input initialization failed",
    [KRYON_ERROR_GRAPHICS_DRIVER_ERROR] = "Graphics driver error",
    
    // Network errors
    [KRYON_ERROR_NETWORK_ERROR] = "Network error",
    [KRYON_ERROR_CONNECTION_FAILED] = "Connection failed",
    [KRYON_ERROR_CONNECTION_TIMEOUT] = "Connection timeout",
    [KRYON_ERROR_CONNECTION_REFUSED] = "Connection refused",
    [KRYON_ERROR_DNS_RESOLUTION_FAILED] = "DNS resolution failed",
    [KRYON_ERROR_HTTP_ERROR] = "HTTP error",
    [KRYON_ERROR_WEBSOCKET_ERROR] = "WebSocket error",
    
    // Script errors
    [KRYON_ERROR_SCRIPT_ERROR] = "Script error",
    [KRYON_ERROR_SCRIPT_COMPILE_ERROR] = "Script compilation error",
    [KRYON_ERROR_SCRIPT_RUNTIME_ERROR] = "Script runtime error",
    [KRYON_ERROR_SCRIPT_TYPE_ERROR] = "Script type error",
    [KRYON_ERROR_FUNCTION_NOT_FOUND] = "Function not found",
    [KRYON_ERROR_SCRIPT_TIMEOUT] = "Script timeout",
    
    // UI/Layout errors
    [KRYON_ERROR_UI_ERROR] = "UI error",
    [KRYON_ERROR_ELEMENT_NOT_FOUND] = "Element not found",
    [KRYON_ERROR_INVALID_PROPERTY] = "Invalid property",
    [KRYON_ERROR_LAYOUT_ERROR] = "Layout error",
    [KRYON_ERROR_RENDER_ERROR] = "Render error",
    [KRYON_ERROR_STYLE_ERROR] = "Style error",
    
    // Resource errors
    [KRYON_ERROR_RESOURCE_ERROR] = "Resource error",
    [KRYON_ERROR_RESOURCE_NOT_FOUND] = "Resource not found",
    [KRYON_ERROR_RESOURCE_LOAD_FAILED] = "Resource load failed",
    [KRYON_ERROR_INVALID_RESOURCE] = "Invalid resource",
    [KRYON_ERROR_RESOURCE_LOCKED] = "Resource locked",
};

static const size_t g_error_message_count = sizeof(g_error_messages) / sizeof(g_error_messages[0]);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static uint64_t get_timestamp_us(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10; // Convert to microseconds
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
#endif
}

static uint32_t get_current_thread_id(void) {
#ifdef _WIN32
    return (uint32_t)GetCurrentThreadId();
#else
    return (uint32_t)pthread_self();
#endif
}

#ifndef _WIN32
static void cleanup_error_context(void *context) {
    KryonErrorContext *ctx = (KryonErrorContext*)context;
    if (ctx) {
        kryon_error_clear();
        if (ctx->log_file && ctx->log_file != stdout && ctx->log_file != stderr) {
            fclose(ctx->log_file);
        }
        kryon_free(ctx);
    }
}
#endif

// =============================================================================
// ERROR SYSTEM INITIALIZATION
// =============================================================================

KryonResult kryon_error_init(void) {
    if (g_error_system_initialized) {
        return KRYON_SUCCESS;
    }
    
#ifndef _WIN32
    if (pthread_key_create(&g_error_context_key, cleanup_error_context) != 0) {
        return KRYON_ERROR_PLATFORM_ERROR;
    }
#endif
    
    // Initialize default logger
    g_logger.callback = NULL;
    g_logger.user_data = NULL;
    g_logger.log_file = stderr;
    g_logger.min_level = KRYON_SEVERITY_INFO;
    g_logger.include_timestamp = true;
    g_logger.include_thread_id = false;
    g_logger.include_location = true;
    g_logger.color_output = false; // TODO: Detect terminal support
    g_logger.async_logging = false;
    
#ifdef _WIN32
    // Initialize symbol handling for stack traces
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif
    
    g_error_system_initialized = true;
    return KRYON_SUCCESS;
}

void kryon_error_shutdown(void) {
    if (!g_error_system_initialized) {
        return;
    }
    
    // Cleanup thread-local storage
#ifdef _WIN32
    if (g_error_context) {
        cleanup_error_context(g_error_context);
        g_error_context = NULL;
    }
#else
    pthread_key_delete(g_error_context_key);
#endif
    
    // Close log file if not stdout/stderr
    if (g_logger.log_file && g_logger.log_file != stdout && g_logger.log_file != stderr) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }
    
#ifdef _WIN32
    SymCleanup(GetCurrentProcess());
#endif
    
    g_error_system_initialized = false;
}

KryonErrorContext *kryon_error_get_context(void) {
    if (!g_error_system_initialized) {
        kryon_error_init();
    }
    
#ifdef _WIN32
    if (!g_error_context) {
        g_error_context = kryon_alloc_type(KryonErrorContext);
        if (g_error_context) {
            memset(g_error_context, 0, sizeof(KryonErrorContext));
            g_error_context->capture_stack_traces = true;
            g_error_context->min_log_level = KRYON_SEVERITY_WARNING;
        }
    }
    return g_error_context;
#else
    KryonErrorContext *context = (KryonErrorContext*)pthread_getspecific(g_error_context_key);
    if (!context) {
        context = kryon_alloc_type(KryonErrorContext);
        if (context) {
            memset(context, 0, sizeof(KryonErrorContext));
            context->capture_stack_traces = true;
            context->min_log_level = KRYON_SEVERITY_WARNING;
            pthread_setspecific(g_error_context_key, context);
        }
    }
    return context;
#endif
}

// =============================================================================
// ERROR MANAGEMENT
// =============================================================================

KryonError *kryon_error_create(KryonResult code, KryonSeverity severity, const char *message,
                              const char *file, int line, const char *function) {
    KryonError *error = kryon_alloc_type(KryonError);
    if (!error) {
        return NULL;
    }
    
    memset(error, 0, sizeof(KryonError));
    
    error->code = code;
    error->severity = severity;
    error->message = message ? message : kryon_error_get_message(code);
    error->file = file;
    error->line = line;
    error->function = function;
    error->timestamp = get_timestamp_us();
    error->thread_id = get_current_thread_id();
    
    // Capture stack trace if enabled
    KryonErrorContext *context = kryon_error_get_context();
    if (context && context->capture_stack_traces) {
        error->stack_trace = kryon_alloc_type(KryonStackTrace);
        if (error->stack_trace) {
            kryon_error_capture_stack_trace(error->stack_trace);
        }
    }
    
    return error;
}

void kryon_error_destroy(KryonError *error) {
    if (!error) return;
    
    // Free chained errors
    if (error->cause) {
        kryon_error_destroy(error->cause);
    }
    
    // Free stack trace
    if (error->stack_trace) {
        kryon_free(error->stack_trace);
    }
    
    // Free user data
    if (error->user_data) {
        kryon_free(error->user_data);
    }
    
    kryon_free(error);
}

void kryon_error_set(KryonError *error) {
    if (!error) return;
    
    KryonErrorContext *context = kryon_error_get_context();
    if (!context) return;
    
    // Free previous error
    if (context->last_error) {
        kryon_error_destroy(context->last_error);
    }
    
    context->last_error = error;
    context->error_count++;
    
    // Log error if severity is high enough
    if (error->severity >= context->min_log_level) {
        kryon_error_print(error, context->log_file);
    }
    
    // Break into debugger if enabled
    if (context->break_on_error && error->severity >= KRYON_SEVERITY_ERROR) {
#ifdef _WIN32
        if (IsDebuggerPresent()) {
            DebugBreak();
        }
#else
        // TODO: Implement debugger break for Unix
#endif
    }
}

const KryonError *kryon_error_get_last(void) {
    KryonErrorContext *context = kryon_error_get_context();
    return context ? context->last_error : NULL;
}

void kryon_error_clear(void) {
    KryonErrorContext *context = kryon_error_get_context();
    if (!context) return;
    
    if (context->last_error) {
        kryon_error_destroy(context->last_error);
        context->last_error = NULL;
    }
    
    // Clear error chain
    KryonError *error = context->error_chain;
    while (error) {
        KryonError *next = error->cause;
        kryon_error_destroy(error);
        error = next;
    }
    context->error_chain = NULL;
    context->error_count = 0;
}

void kryon_error_chain(KryonError *cause) {
    if (!cause) return;
    
    KryonErrorContext *context = kryon_error_get_context();
    if (!context || !context->last_error) return;
    
    context->last_error->cause = cause;
}

const char *kryon_error_get_message(KryonResult code) {
    if (code >= 0 && (size_t)code < g_error_message_count && g_error_messages[code]) {
        return g_error_messages[code];
    }
    return "Unknown error";
}

size_t kryon_error_format(const KryonError *error, char *buffer, size_t buffer_size) {
    if (!error || !buffer || buffer_size == 0) {
        return 0;
    }
    
    size_t written = 0;
    
    // Format main error
    written += snprint(buffer + written, buffer_size - written,
                       "Error %d (%s): %s",
                       error->code,
                       error->severity == KRYON_SEVERITY_FATAL ? "FATAL" :
                       error->severity == KRYON_SEVERITY_CRITICAL ? "CRITICAL" :
                       error->severity == KRYON_SEVERITY_ERROR ? "ERROR" :
                       error->severity == KRYON_SEVERITY_WARNING ? "WARNING" :
                       error->severity == KRYON_SEVERITY_INFO ? "INFO" :
                       error->severity == KRYON_SEVERITY_DEBUG ? "DEBUG" : "TRACE",
                       error->message);
    
    // Add location information
    if (error->file && written < buffer_size) {
        written += snprint(buffer + written, buffer_size - written,
                           "\n  at %s:%d in %s()",
                           error->file, error->line,
                           error->function ? error->function : "unknown");
    }
    
    // Add details if available
    if (error->details && written < buffer_size) {
        written += snprint(buffer + written, buffer_size - written,
                           "\n  Details: %s", error->details);
    }
    
    // Add stack trace if available
    if (error->stack_trace && written < buffer_size) {
        written += snprint(buffer + written, buffer_size - written, "\n  Stack trace:");
        if (written < buffer_size) {
            written += kryon_error_format_stack_trace(error->stack_trace,
                                                     buffer + written,
                                                     buffer_size - written);
        }
    }
    
    // Add chained errors
    const KryonError *cause = error->cause;
    while (cause && written < buffer_size) {
        written += snprint(buffer + written, buffer_size - written,
                           "\nCaused by: Error %d: %s",
                           cause->code, cause->message);
        if (cause->file && written < buffer_size) {
            written += snprint(buffer + written, buffer_size - written,
                               " at %s:%d", cause->file, cause->line);
        }
        cause = cause->cause;
    }
    
    return written;
}

void kryon_error_print(const KryonError *error, FILE *file) {
    if (!error) return;
    if (!file) file = stderr;
    
    char buffer[4096];
    kryon_error_format(error, buffer, sizeof(buffer));
    fprintf(file, "%s\n", buffer);
    fflush(file);
}

// =============================================================================
// STACK TRACE IMPLEMENTATION
// =============================================================================

size_t kryon_error_capture_stack_trace(KryonStackTrace *trace) {
    if (!trace) return 0;
    
    memset(trace, 0, sizeof(KryonStackTrace));
    trace->timestamp = get_timestamp_us();
    trace->thread_id = get_current_thread_id();
    
#ifdef _WIN32
    void *stack[KRYON_MAX_STACK_FRAMES];
    USHORT frame_count = CaptureStackBackTrace(1, KRYON_MAX_STACK_FRAMES, stack, NULL);
    
    HANDLE process = GetCurrentProcess();
    SYMBOL_INFO *symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256);
    if (!symbol) return 0;
    
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    for (USHORT i = 0; i < frame_count && i < KRYON_MAX_STACK_FRAMES; i++) {
        trace->frames[i].address = stack[i];
        
        DWORD64 displacement = 0;
        if (SymFromAddr(process, (DWORD64)stack[i], &displacement, symbol)) {
            // TODO: Store function name (need to allocate and copy)
        }
        
        // TODO: Get file and line information using SymGetLineFromAddr64
    }
    
    trace->frame_count = frame_count;
    free(symbol);
    
#else
    void *stack[KRYON_MAX_STACK_FRAMES];
    int frame_count = backtrace(stack, KRYON_MAX_STACK_FRAMES);
    
    for (int i = 1; i < frame_count && i < KRYON_MAX_STACK_FRAMES; i++) {
        trace->frames[i-1].address = stack[i];
        // TODO: Use backtrace_symbols or addr2line to get function names
    }
    
    trace->frame_count = frame_count > 1 ? frame_count - 1 : 0;
#endif
    
    return trace->frame_count;
}

size_t kryon_error_format_stack_trace(const KryonStackTrace *trace, char *buffer, size_t buffer_size) {
    if (!trace || !buffer || buffer_size == 0) {
        return 0;
    }
    
    size_t written = 0;
    
    for (size_t i = 0; i < trace->frame_count && written < buffer_size; i++) {
        const KryonStackFrame *frame = &trace->frames[i];
        
        written += snprint(buffer + written, buffer_size - written,
                           "\n    #%zu: %p", i, frame->address);
        
        if (frame->function && written < buffer_size) {
            written += snprint(buffer + written, buffer_size - written,
                               " in %s()", frame->function);
        }
        
        if (frame->file && written < buffer_size) {
            written += snprint(buffer + written, buffer_size - written,
                               " at %s:%d", frame->file, frame->line);
        }
    }
    
    return written;
}

// =============================================================================
// LOGGING IMPLEMENTATION
// =============================================================================

KryonResult kryon_logger_init(const KryonLogger *logger) {
    if (!logger) return KRYON_ERROR_INVALID_ARGUMENT;
    
    g_logger = *logger;
    return KRYON_SUCCESS;
}

void kryon_logger_shutdown(void) {
    if (g_logger.log_file && g_logger.log_file != stdout && g_logger.log_file != stderr) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }
    memset(&g_logger, 0, sizeof(g_logger));
}

void kryon_log(KryonSeverity severity, const char *file, int line, const char *function,
              const char *format, ...) {
    if (severity < g_logger.min_level) {
        return;
    }
    
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Use callback if available
    if (g_logger.callback) {
        g_logger.callback(severity, message, file, line, function, g_logger.user_data);
        return;
    }
    
    // Default file logging
    FILE *output = g_logger.log_file ? g_logger.log_file : stderr;
    
    if (g_logger.include_timestamp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output, "[%s] ", timestamp);
    }
    
    if (g_logger.include_thread_id) {
        fprintf(output, "[%u] ", get_current_thread_id());
    }
    
    // Severity level
    const char *level_str = 
        severity == KRYON_SEVERITY_FATAL ? "FATAL" :
        severity == KRYON_SEVERITY_CRITICAL ? "CRITICAL" :
        severity == KRYON_SEVERITY_ERROR ? "ERROR" :
        severity == KRYON_SEVERITY_WARNING ? "WARNING" :
        severity == KRYON_SEVERITY_INFO ? "INFO" :
        severity == KRYON_SEVERITY_DEBUG ? "DEBUG" : "TRACE";
    
    fprintf(output, "%-8s: %s", level_str, message);
    
    if (g_logger.include_location && file) {
        fprintf(output, " (%s:%d in %s)", file, line, function ? function : "unknown");
    }
    
    fprintf(output, "\n");
    fflush(output);
}

void kryon_log_set_level(KryonSeverity level) {
    g_logger.min_level = level;
}

KryonSeverity kryon_log_get_level(void) {
    return g_logger.min_level;
}
