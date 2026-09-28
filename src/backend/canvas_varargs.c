/* raylib's TextFormat and TraceLog take C varargs, which Ziran code cannot
 * define. These forward the argument list, as a pointer, to the Ziran
 * functions that do the formatting. Built only for the WebAssembly target. */
#include <stdarg.h>
#include <stdio.h>

extern const char *canvas_format_text(const char *format, void *arguments);
extern const char *canvas_format_log(const char *format, void *arguments);
extern int canvas_trace_callback(int level, const char *format, void *arguments);

const char *TextFormat(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const char *text = canvas_format_text(format, (void *)arguments);
    va_end(arguments);
    return text;
}

void TraceLog(int level, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    if (!canvas_trace_callback(level, format, (void *)arguments)) {
        const char *message = canvas_format_log(format, (void *)arguments);
        fprintf(stderr, "%s\n", message);
    }
    va_end(arguments);
}
