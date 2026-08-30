#include "kryon.h"

#include <stdio.h>
#include <string.h>

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

static Rectangle
page_bounds_or_view(Rectangle bounds)
{
    if(bounds.width <= 0)
        bounds.width = (float)GetUIViewWidth();
    if(bounds.height <= 0)
        bounds.height = (float)GetUIViewHeight();
    return bounds;
}

static Color
page_color_or(Color color, Color fallback)
{
    return color.a != 0 ? color : fallback;
}

static void
page_semantic_box(UISemanticKind kind, Rectangle bounds, const char *label)
{
    if(kry_dom_semantic_box != NULL)
        kry_dom_semantic_box((int)kind, bounds, label);
}

static void
page_semantic_next(UISemanticKind kind, const char *label, const char *href,
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

NodeId
Page(PageProps props)
{
    Rectangle bounds = page_bounds_or_view(props.bounds);
    KeyID key = props.key != 0 ? props.key : Key(props.title);

    if(props.title != NULL)
        SetPageTitle(props.title);
    if(props.description != NULL)
        SetPageDescription(props.description);
    if(props.canonical_url != NULL)
        SetPageCanonicalURL(props.canonical_url);
    if(props.theme_color.a != 0)
        SetPageThemeColor(props.theme_color);
    if(props.background.a != 0)
        Background(props.background);
    page_semantic_box(UI_SEMANTIC_PAGE, bounds, props.title);
    return Column((ColumnProps){bounds, props.gap, props.padding, key});
}

NodeId
Section(SectionProps props)
{
    Rectangle bounds = page_bounds_or_view(props.bounds);
    KeyID key = props.key != 0 ? props.key : Key(props.label);

    page_semantic_box(UI_SEMANTIC_SECTION, bounds, props.label);
    return Column((ColumnProps){bounds, props.gap, props.padding, key});
}

void
Heading(HeadingProps props)
{
    Rectangle bounds = props.bounds;
    const char *text = props.text != NULL ? props.text : "";
    int level = props.level;
    int font = props.font > 0 ? props.font : Text24;
    Color color = page_color_or(props.color, GetThemeText());

    if(level < 1)
        level = 1;
    if(level > 6)
        level = 6;
    if(bounds.width <= 0)
        bounds.width = (float)TextWidth(text, font);
    if(bounds.height <= 0)
        bounds.height = (float)TextHeight(text, font);
    page_semantic_next(UI_SEMANTIC_HEADING, text, NULL, NULL, level, -1);
    TextInRect(text, bounds, font, color);
}

void
ParagraphText(ParagraphTextProps props)
{
    ParagraphSpec paragraph;
    const char *text = props.text != NULL ? props.text : "";
    int y = (int)props.bounds.y;
    int width = (int)props.bounds.width;

    if(width <= 0)
        width = GetUIViewWidth() - (int)props.bounds.x;
    if(width < 0)
        width = 0;
    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = text;
    paragraph.width = width;
    paragraph.font = props.font > 0 ? props.font : GetUIFontSize();
    paragraph.line_gap = props.line_gap;
    paragraph.color = page_color_or(props.color, GetThemeText());
    page_semantic_next(UI_SEMANTIC_PARAGRAPH, text, NULL, NULL, 0, -1);
    Paragraph(paragraph, (int)props.bounds.x, &y);
}

int
Link(LinkProps props)
{
    HrefProps href;

    memset(&href, 0, sizeof(href));
    href.bounds = props.bounds;
    href.text = props.text;
    href.href = props.href;
    href.font = props.font;
    href.focus_id = props.focus_id;
    href.disabled = props.disabled;
    href.color = props.color;
    href.hover_color = props.hover_color;
    page_semantic_next(UI_SEMANTIC_LINK, props.text, props.href, "link", 0,
                       props.focus_id);
    return Href(href);
}

void
PagePicture(PictureProps picture, const char *alt_text)
{
    page_semantic_next(UI_SEMANTIC_PICTURE, alt_text, NULL, "img", 0, -1);
    Picture(picture);
}

NodeId
Flow(FlowProps props)
{
    return Row((RowProps){props.bounds, props.gap, props.padding, props.key});
}

NodeId
PageGrid(GridProps props)
{
    return GridLayout((GridLayoutProps){props.bounds, props.columns,
                                        props.gap, props.padding, props.key});
}
