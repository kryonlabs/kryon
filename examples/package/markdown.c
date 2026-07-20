#include "kryon.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *markdown = "# Flint\n\n- [x] packaged\n";
    char *html = FlintMarkdownToHTML(markdown, strlen(markdown), FLINT_MARKDOWN_GFM);
    if (!html) {
        return 1;
    }
    int ok = strstr(html, "<h1") != NULL && strstr(html, "<input") != NULL;
    if (!ok) {
        fputs(html, stderr);
    }
    FlintMarkdownFree(html);
    return ok ? 0 : 1;
}
