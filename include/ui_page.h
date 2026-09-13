#ifndef KRYON_PAGE_H
#define KRYON_PAGE_H

#include "kryon_compat.generated.h"
#include "ui_image.h"
#include "ui_link_props.generated.h"
#include "ui_page_props.generated.h"
#include "ui_tree.h"

typedef enum SemanticKind {
    SEMANTIC_NONE = 0,
    SEMANTIC_PAGE,
    SEMANTIC_SECTION,
    SEMANTIC_HEADING,
    SEMANTIC_PARAGRAPH,
    SEMANTIC_LINK,
    SEMANTIC_IMAGE,
    SEMANTIC_BUTTON
} SemanticKind;

typedef ColumnProps FlowProps;

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
NodeId Flow(FlowProps props);

#endif
