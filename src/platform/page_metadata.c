#include "../ui/ui_internal.h"

#if defined(__GNUC__) || defined(__clang__)
#define KRY_WEAK __attribute__((weak))
#else
#define KRY_WEAK
#endif

KRY_WEAK extern void kry_dom_set_page_title(const char *title);
KRY_WEAK extern void kry_dom_set_page_description(const char *description);
KRY_WEAK extern void kry_dom_set_page_canonical_url(const char *url);
KRY_WEAK extern void kry_dom_set_page_theme_color(Color color);
KRY_WEAK extern const char *kry_dom_get_route_path(void);
KRY_WEAK extern const char *kry_dom_get_route_hash(void);
KRY_WEAK extern int kry_dom_get_route_version(void);
KRY_WEAK extern void kry_dom_push_route(const char *path);
KRY_WEAK extern void kry_dom_replace_route(const char *path);
KRY_WEAK extern const char *kry_web_get_route_path(void);
KRY_WEAK extern const char *kry_web_get_route_hash(void);
KRY_WEAK extern int kry_web_get_route_version(void);
KRY_WEAK extern void kry_web_push_route(const char *path);
KRY_WEAK extern void kry_web_replace_route(const char *path);
KRY_WEAK extern void kry_dom_semantic_box(int kind, Rectangle bounds,
                                          const char *label);
KRY_WEAK extern void kry_dom_semantic_next(int kind, const char *label,
                                           const char *href,
                                           const char *role, int level,
                                           int tab_index);

static const char kry_route_root[] = "/";
static const char kry_route_empty[] = "";

void
ui_page_semantic_box(SemanticKind kind, Rectangle bounds, const char *label)
{
    if(kry_dom_semantic_box != NULL)
        kry_dom_semantic_box((int)kind, bounds, label);
}

void
ui_page_semantic_next(SemanticKind kind, const char *label, const char *href,
                      const char *role, int level, int tab_index)
{
    if(kry_dom_semantic_next != NULL)
        kry_dom_semantic_next((int)kind, label, href, role, level, tab_index);
}

void
SetPageTitle(const char *title)
{
    if(kry_dom_set_page_title != NULL)
        kry_dom_set_page_title(title);
}

void
SetPageDescription(const char *description)
{
    if(kry_dom_set_page_description != NULL)
        kry_dom_set_page_description(description);
}

void
SetPageCanonicalURL(const char *url)
{
    if(kry_dom_set_page_canonical_url != NULL)
        kry_dom_set_page_canonical_url(url);
}

void
SetPageThemeColor(Color color)
{
    if(kry_dom_set_page_theme_color != NULL)
        kry_dom_set_page_theme_color(color);
}

const char *
GetRoutePath(void)
{
#if (defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && !defined(KRYON_BACKEND_DOM)
    if(kry_web_get_route_path != NULL)
        return kry_web_get_route_path();
#endif
    if(kry_dom_get_route_path != NULL)
        return kry_dom_get_route_path();
    if(kry_web_get_route_path != NULL)
        return kry_web_get_route_path();
    return kry_route_root;
}

const char *
GetRouteHash(void)
{
#if (defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && !defined(KRYON_BACKEND_DOM)
    if(kry_web_get_route_hash != NULL)
        return kry_web_get_route_hash();
#endif
    if(kry_dom_get_route_hash != NULL)
        return kry_dom_get_route_hash();
    if(kry_web_get_route_hash != NULL)
        return kry_web_get_route_hash();
    return kry_route_empty;
}

int
GetRouteVersion(void)
{
#if (defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && !defined(KRYON_BACKEND_DOM)
    if(kry_web_get_route_version != NULL)
        return kry_web_get_route_version();
#endif
    if(kry_dom_get_route_version != NULL)
        return kry_dom_get_route_version();
    if(kry_web_get_route_version != NULL)
        return kry_web_get_route_version();
    return 0;
}

void
PushRoute(const char *path)
{
#if (defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && !defined(KRYON_BACKEND_DOM)
    if(kry_web_push_route != NULL) {
        kry_web_push_route(path);
        return;
    }
#endif
    if(kry_dom_push_route != NULL)
        kry_dom_push_route(path);
    else if(kry_web_push_route != NULL)
        kry_web_push_route(path);
}

void
ReplaceRoute(const char *path)
{
#if (defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && !defined(KRYON_BACKEND_DOM)
    if(kry_web_replace_route != NULL) {
        kry_web_replace_route(path);
        return;
    }
#endif
    if(kry_dom_replace_route != NULL)
        kry_dom_replace_route(path);
    else if(kry_web_replace_route != NULL)
        kry_web_replace_route(path);
}
