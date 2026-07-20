#include "kryon.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *markdown = "# Kryon\n\n- [x] packaged\n";
    char *html = KryonMarkdownToHTML(markdown, strlen(markdown), KRYON_MARKDOWN_GFM);
    if (!html) {
        return 1;
    }
    int ok = strstr(html, "<h1") != NULL && strstr(html, "<input") != NULL;
    if (!ok) {
        fputs(html, stderr);
    }
    KryonMarkdownFree(html);
    return ok ? 0 : 1;
}
