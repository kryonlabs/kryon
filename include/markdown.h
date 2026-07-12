#ifndef FLINT_MARKDOWN_H
#define FLINT_MARKDOWN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FlintMarkdownOptions {
    FLINT_MARKDOWN_GFM = 1 << 0,
    FLINT_MARKDOWN_UNSAFE_HTML = 1 << 1,
    FLINT_MARKDOWN_SMART = 1 << 2
} FlintMarkdownOptions;

char *FlintMarkdownToHTML(const char *markdown, size_t markdown_len, int options);
void FlintMarkdownFree(char *text);

#ifdef __cplusplus
}
#endif

#endif
