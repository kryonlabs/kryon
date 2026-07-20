#ifndef KRYON_MARKDOWN_H
#define KRYON_MARKDOWN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KryonMarkdownOptions {
    KRYON_MARKDOWN_GFM = 1 << 0,
    KRYON_MARKDOWN_UNSAFE_HTML = 1 << 1,
    KRYON_MARKDOWN_SMART = 1 << 2
} KryonMarkdownOptions;

char *KryonMarkdownToHTML(const char *markdown, size_t markdown_len, int options);
void KryonMarkdownFree(char *text);

#ifdef __cplusplus
}
#endif

#endif
