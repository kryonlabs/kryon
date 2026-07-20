#include "markdown.h"

#if defined(KRYON_HAS_CMARK_GFM)
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "cmark-gfm-extension_api.h"
#include <stdlib.h>
#include <string.h>

static const char *gfm_extensions[] = {
    "table",
    "strikethrough",
    "autolink",
    "tagfilter",
    "tasklist"
};

char *
FlintMarkdownToHTML(const char *markdown, size_t markdown_len, int options)
{
    cmark_parser *parser;
    cmark_node *doc;
    cmark_llist *attached = NULL;
    cmark_mem *mem;
    char *html;
    int cmark_options = CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;

    if(markdown == NULL)
        markdown = "";
    if((options & FLINT_MARKDOWN_UNSAFE_HTML) != 0)
        cmark_options |= CMARK_OPT_UNSAFE;
    if((options & FLINT_MARKDOWN_SMART) != 0)
        cmark_options |= CMARK_OPT_SMART;

    mem = cmark_get_default_mem_allocator();
    parser = cmark_parser_new(cmark_options);
    if(parser == NULL)
        return NULL;

    if((options & FLINT_MARKDOWN_GFM) != 0) {
        cmark_gfm_core_extensions_ensure_registered();
        for(size_t i = 0; i < sizeof(gfm_extensions) / sizeof(gfm_extensions[0]); i++) {
            cmark_syntax_extension *ext = cmark_find_syntax_extension(gfm_extensions[i]);
            if(ext != NULL && cmark_parser_attach_syntax_extension(parser, ext))
                attached = cmark_llist_append(mem, attached, ext);
        }
    }

    cmark_parser_feed(parser, markdown, markdown_len);
    doc = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    if(doc == NULL) {
        cmark_llist_free(mem, attached);
        return NULL;
    }

    html = cmark_render_html(doc, cmark_options, attached);
    cmark_node_free(doc);
    cmark_llist_free(mem, attached);
    return html;
}

void
FlintMarkdownFree(char *text)
{
    free(text);
}
#else
#include <stdlib.h>
#include <string.h>

char *
FlintMarkdownToHTML(const char *markdown, size_t markdown_len, int options)
{
    char *copy;
    (void)options;

    if(markdown == NULL) {
        markdown = "";
        markdown_len = 0;
    }
    copy = (char *)malloc(markdown_len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, markdown, markdown_len);
    copy[markdown_len] = '\0';
    return copy;
}

void
FlintMarkdownFree(char *text)
{
    free(text);
}
#endif
