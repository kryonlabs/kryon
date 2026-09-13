#include "kryon.h"
#include "ui_internal.h"
#include "ui_style_internal.h"

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
        bounds.width = (float)GetViewWidth();
    if(bounds.height <= 0)
        bounds.height = (float)GetViewHeight();
    return bounds;
}

static Style
page_text_style(int style_kind, int class_name, int fallback_font)
{
    StyleData base = {
        .fields = (uint32_t)(StyleOpacity | StyleFontSize),
        .opacity = 1.0f,
        .font_size = (float)fallback_font
    };
    StyleFacts facts = StyleDefaultFacts(style_kind);
    facts.class_name = class_name;
    facts.state = ButtonStateNormal;
    return ui_unpack_style(ResolveActiveStyle(base, facts, ButtonStateNormal));
}

static void
page_semantic_box(SemanticKind kind, Rectangle bounds, const char *label)
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
    page_semantic_box(SEMANTIC_PAGE, bounds, props.title);
    return Column((ColumnProps){bounds, props.gap, props.padding, key});
}

NodeId
Section(SectionProps props)
{
    Rectangle bounds = page_bounds_or_view(props.bounds);
    KeyID key = props.key != 0 ? props.key : Key(props.label);

    page_semantic_box(SEMANTIC_SECTION, bounds, props.label);
    return Column((ColumnProps){bounds, props.gap, props.padding, key});
}

void
Heading(HeadingProps props)
{
    Rectangle bounds = props.bounds;
    const char *text = props.text != NULL ? props.text : "";
    int level = props.level;
    Style text_style = page_text_style(StyleKindHeading(), props.class_name,
                                       Text24);
    int font = (int)text_style.font_size;
    if(font <= 0)
        font = Text24;
    Color color = Fade(text_style.foreground, text_style.opacity);

    if(level < 1)
        level = 1;
    if(level > 6)
        level = 6;
    if(bounds.width <= 0)
        bounds.width = (float)TextWidth(text, font);
    if(bounds.height <= 0)
        bounds.height = (float)TextHeight(text, font);
    ui_tree_heading(text, bounds, font, color, level);
}

void
ParagraphText(ParagraphTextProps props)
{
    ParagraphSpec paragraph;
    const char *text = props.text != NULL ? props.text : "";
    int y = (int)props.bounds.y;
    int width = (int)props.bounds.width;

    if(width <= 0)
        width = GetViewWidth() - (int)props.bounds.x;
    if(width < 0)
        width = 0;
    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = text;
    paragraph.width = width;
    Style text_style = page_text_style(StyleKindParagraphText(),
                                       props.class_name, GetFontSize());
    paragraph.font = (int)text_style.font_size;
    if(paragraph.font <= 0)
        paragraph.font = GetFontSize();
    if(text_style.gap > 0.0f)
        paragraph.line_gap = (int)(text_style.gap + 0.5f);
    paragraph.color = Fade(text_style.foreground, text_style.opacity);
    ui_page_semantic_next(SEMANTIC_PARAGRAPH, text, NULL, NULL, 0, -1);
    Paragraph(paragraph, (int)props.bounds.x, &y);
}

int
Link(LinkProps props)
{
    ui_page_semantic_next(SEMANTIC_LINK, props.text, props.link, "link", 0,
                          props.focus_id);
    return RenderLink(props);
}

NodeId
Flow(FlowProps props)
{
    return Row((RowProps){props.bounds, props.gap, props.padding, props.key});
}
