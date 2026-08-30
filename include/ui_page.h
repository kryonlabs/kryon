#ifndef UI_PAGE_H
#define UI_PAGE_H

#include "kryon_compat.generated.h"
#include "ui_picture.h"
#include "ui_tree.h"

typedef enum UISemanticKind {
    UI_SEMANTIC_NONE = 0,
    UI_SEMANTIC_PAGE,
    UI_SEMANTIC_SECTION,
    UI_SEMANTIC_HEADING,
    UI_SEMANTIC_PARAGRAPH,
    UI_SEMANTIC_LINK,
    UI_SEMANTIC_PICTURE,
    UI_SEMANTIC_BUTTON
} UISemanticKind;

typedef struct PageProps {
    Rectangle bounds;
    const char *title;
    const char *description;
    const char *canonical_url;
    Color theme_color;
    Color background;
    int gap;
    int padding;
    KeyID key;
} PageProps;

typedef struct SectionProps {
    Rectangle bounds;
    const char *label;
    int gap;
    int padding;
    KeyID key;
} SectionProps;

typedef struct HeadingProps {
    Rectangle bounds;
    const char *text;
    int level;
    int font;
    Color color;
    KeyID key;
} HeadingProps;

typedef struct ParagraphTextProps {
    Rectangle bounds;
    const char *text;
    int font;
    Color color;
    int line_gap;
    KeyID key;
} ParagraphTextProps;

typedef struct LinkProps {
    Rectangle bounds;
    const char *text;
    const char *href;
    int font;
    int focus_id;
    int disabled;
    Color color;
    Color hover_color;
} LinkProps;

typedef struct FlowProps {
    Rectangle bounds;
    int gap;
    int padding;
    KeyID key;
} FlowProps;

typedef struct GridProps {
    Rectangle bounds;
    int columns;
    int gap;
    int padding;
    KeyID key;
} GridProps;

void SetPageTitle(const char *title);
void SetPageDescription(const char *description);
void SetPageCanonicalURL(const char *url);
void SetPageThemeColor(Color color);
const char *GetRoutePath(void);
const char *GetRouteHash(void);
int GetRouteVersion(void);
void PushRoute(const char *path);
void ReplaceRoute(const char *path);

NodeId Page(PageProps props);
NodeId Section(SectionProps props);
void Heading(HeadingProps props);
void ParagraphText(ParagraphTextProps props);
int Link(LinkProps props);
void PagePicture(PictureProps picture, const char *alt_text);
NodeId Flow(FlowProps props);
NodeId PageGrid(GridProps props);

#endif
