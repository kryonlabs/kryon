#include "markdown.h"
#include <stdio.h>
#include <string.h>

static int contains(const char *haystack, const char *needle)
{
    return haystack != NULL && strstr(haystack, needle) != NULL;
}

int main(void)
{
    const char *markdown = "# Title\n\n- [x] done\n\n| A | B |\n|---|---|\n| 1 | 2 |\n\n~~gone~~\n";
    char *html = KryonMarkdownToHTML(markdown, strlen(markdown), KRYON_MARKDOWN_GFM);
    int ok = 1;

    if(html == NULL) {
        fprintf(stderr, "KryonMarkdownToHTML returned NULL\n");
        return 1;
    }
    ok &= contains(html, "<h1>Title</h1>");
    ok &= contains(html, "<table>");
    ok &= contains(html, "checkbox");
    ok &= contains(html, "<del>gone</del>");
    if(!ok)
        fprintf(stderr, "unexpected html:\n%s\n", html);
    KryonMarkdownFree(html);
    return ok ? 0 : 1;
}
