#include "kss_parser.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KSS_TOKEN_MAX 128

typedef enum KssTokenKind {
    KSS_TOKEN_COLOR,
    KSS_TOKEN_LENGTH,
    KSS_TOKEN_MATERIAL,
    KSS_TOKEN_DURATION
} KssTokenKind;

typedef struct KssToken {
    KssTokenKind kind;
    char name[64];
    uint32_t color;
    float number;
    int material;
} KssToken;

typedef struct KssParser {
    const char *source;
    const char *cursor;
    StyleRule *rules;
    int rule_capacity;
    int rule_count;
    int layer;
    KssToken tokens[KSS_TOKEN_MAX];
    int token_count;
    char pack_id[64];
    char *diagnostic;
    size_t diagnostic_size;
} KssParser;

static bool
kss_fail(KssParser *p, const char *format, ...)
{
    va_list args;
    long offset = (long)(p->cursor - p->source);

    if(p->diagnostic != NULL && p->diagnostic_size > 0) {
        va_start(args, format);
        (void)vsnprintf(p->diagnostic, p->diagnostic_size, format, args);
        va_end(args);
        if(strlen(p->diagnostic) + 32 < p->diagnostic_size) {
            size_t len = strlen(p->diagnostic);
            (void)snprintf(p->diagnostic + len, p->diagnostic_size - len,
                           " at byte %ld", offset);
        }
    }
    return false;
}

static bool
kss_ieq(const char *a, const char *b)
{
    while(*a != '\0' && *b != '\0') {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void
kss_skip_ws(KssParser *p)
{
    for(;;) {
        while(isspace((unsigned char)*p->cursor))
            p->cursor++;
        if(p->cursor[0] == '/' && p->cursor[1] == '/') {
            p->cursor += 2;
            while(*p->cursor != '\0' && *p->cursor != '\n')
                p->cursor++;
            continue;
        }
        if(p->cursor[0] == '/' && p->cursor[1] == '*') {
            p->cursor += 2;
            while(*p->cursor != '\0' &&
                  !(p->cursor[0] == '*' && p->cursor[1] == '/'))
                p->cursor++;
            if(*p->cursor != '\0')
                p->cursor += 2;
            continue;
        }
        break;
    }
}

static bool
kss_ident_start(char c)
{
    return isalpha((unsigned char)c) || c == '_' || c == '-';
}

static bool
kss_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.';
}

static bool
kss_selector_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static bool
kss_read_ident(KssParser *p, char *out, size_t out_size)
{
    size_t len = 0;

    kss_skip_ws(p);
    if(!kss_ident_start(*p->cursor))
        return false;
    while(kss_ident_char(*p->cursor)) {
        if(len + 1 < out_size)
            out[len++] = *p->cursor;
        p->cursor++;
    }
    if(out_size > 0)
        out[len] = '\0';
    return true;
}

static bool
kss_read_selector_ident(KssParser *p, char *out, size_t out_size)
{
    size_t len = 0;

    kss_skip_ws(p);
    if(!kss_ident_start(*p->cursor))
        return false;
    while(kss_selector_ident_char(*p->cursor)) {
        if(len + 1 < out_size)
            out[len++] = *p->cursor;
        p->cursor++;
    }
    if(out_size > 0)
        out[len] = '\0';
    return true;
}

static bool
kss_read_ident_view(KssParser *p, String *out)
{
    const char *start;

    kss_skip_ws(p);
    if(!kss_ident_start(*p->cursor))
        return false;
    start = p->cursor;
    while(kss_ident_char(*p->cursor))
        p->cursor++;
    *out = StringView(start, (size_t)(p->cursor - start));
    return true;
}

static bool
kss_expect(KssParser *p, char c)
{
    kss_skip_ws(p);
    if(*p->cursor != c)
        return false;
    p->cursor++;
    return true;
}

static int
kss_style_kind(const char *name)
{
    if(kss_ieq(name, "*") || kss_ieq(name, "Any"))
        return StyleKindAny();
    if(kss_ieq(name, "App"))
        return StyleKindApp();
    if(kss_ieq(name, "Button"))
        return StyleKindButton();
    if(kss_ieq(name, "Text"))
        return StyleKindText();
    if(kss_ieq(name, "TextField"))
        return StyleKindTextField();
    if(kss_ieq(name, "TextArea"))
        return StyleKindTextArea();
    if(kss_ieq(name, "Surface"))
        return StyleKindSurface();
    if(kss_ieq(name, "Dropdown"))
        return StyleKindDropdown();
    if(kss_ieq(name, "Card"))
        return StyleKindCard();
    if(kss_ieq(name, "Slider"))
        return StyleKindSlider();
    if(kss_ieq(name, "SliderThumb"))
        return StyleKindSliderThumb();
    if(kss_ieq(name, "Toggle"))
        return StyleKindToggle();
    if(kss_ieq(name, "ToggleThumb"))
        return StyleKindToggleThumb();
    if(kss_ieq(name, "Scroll"))
        return StyleKindScroll();
    if(kss_ieq(name, "ScrollThumb"))
        return StyleKindScrollThumb();
    if(kss_ieq(name, "Checkbox"))
        return StyleKindCheckbox();
    if(kss_ieq(name, "Radio"))
        return StyleKindRadio();
    if(kss_ieq(name, "Progress"))
        return StyleKindProgress();
    if(kss_ieq(name, "Separator"))
        return StyleKindSeparator();
    if(kss_ieq(name, "NavigationBar"))
        return StyleKindNavigationBar();
    if(kss_ieq(name, "NavigationBarItem"))
        return StyleKindNavigationBarItem();
    if(kss_ieq(name, "Selectable"))
        return StyleKindSelectable();
    if(kss_ieq(name, "Fieldset"))
        return StyleKindFieldset();
    if(kss_ieq(name, "Plot"))
        return StyleKindPlot();
    if(kss_ieq(name, "PlotMark"))
        return StyleKindPlotMark();
    if(kss_ieq(name, "Link"))
        return StyleKindLink();
    if(kss_ieq(name, "TabBar"))
        return StyleKindTabBar();
    if(kss_ieq(name, "Tab"))
        return StyleKindTab();
    if(kss_ieq(name, "TabClose"))
        return StyleKindTabClose();
    if(kss_ieq(name, "SegmentedControl"))
        return StyleKindSegmentedControl();
    if(kss_ieq(name, "Segment"))
        return StyleKindSegment();
    if(kss_ieq(name, "Menu"))
        return StyleKindMenu();
    if(kss_ieq(name, "MenuItem"))
        return StyleKindMenuItem();
    if(kss_ieq(name, "MenuSeparator"))
        return StyleKindMenuSeparator();
    if(kss_ieq(name, "ListBox"))
        return StyleKindListBox();
    if(kss_ieq(name, "ListBoxItem"))
        return StyleKindListBoxItem();
    if(kss_ieq(name, "TreeView"))
        return StyleKindTreeView();
    if(kss_ieq(name, "TreeViewItem"))
        return StyleKindTreeViewItem();
    if(kss_ieq(name, "ListBoxMulti"))
        return StyleKindListBoxMulti();
    if(kss_ieq(name, "ListBoxMultiItem"))
        return StyleKindListBoxMultiItem();
    if(kss_ieq(name, "DragDropTarget"))
        return StyleKindDragDropTarget();
    if(kss_ieq(name, "Spinbox"))
        return StyleKindSpinbox();
    if(kss_ieq(name, "SpinboxValue"))
        return StyleKindSpinboxValue();
    if(kss_ieq(name, "ColorPickerSwatch"))
        return StyleKindColorPickerSwatch();
    if(kss_ieq(name, "PanedView"))
        return StyleKindPanedView();
    if(kss_ieq(name, "Toast"))
        return StyleKindToast();
    if(kss_ieq(name, "Collapsible"))
        return StyleKindCollapsible();
    if(kss_ieq(name, "TitleBar"))
        return StyleKindTitleBar();
    if(kss_ieq(name, "Toolbar"))
        return StyleKindToolbar();
    if(kss_ieq(name, "Modal"))
        return StyleKindModal();
    if(kss_ieq(name, "TableView"))
        return StyleKindTableView();
    if(kss_ieq(name, "Guide"))
        return StyleKindGuide();
    if(kss_ieq(name, "Image"))
        return StyleKindImage();
    if(kss_ieq(name, "Focus"))
        return StyleKindFocus();
    if(kss_ieq(name, "Popup"))
        return StyleKindPopup();
    if(kss_ieq(name, "Canvas"))
        return StyleKindCanvas();
    if(kss_ieq(name, "Drag"))
        return StyleKindDrag();
    if(kss_ieq(name, "DragValue"))
        return StyleKindDragValue();
    if(kss_ieq(name, "Heading"))
        return StyleKindHeading();
    if(kss_ieq(name, "ParagraphText"))
        return StyleKindParagraphText();
    return -999999;
}

static int
kss_state(const char *name)
{
    if(kss_ieq(name, "any"))
        return StyleStateAny();
    if(kss_ieq(name, "normal"))
        return ButtonStateNormal;
    if(kss_ieq(name, "hover"))
        return ButtonStateHover;
    if(kss_ieq(name, "pressed") || kss_ieq(name, "press"))
        return ButtonStatePressed;
    if(kss_ieq(name, "focus") || kss_ieq(name, "focused"))
        return ButtonStateFocus;
    if(kss_ieq(name, "disabled"))
        return ButtonStateDisabled;
    if(kss_ieq(name, "loading"))
        return ButtonStateLoading;
    if(kss_ieq(name, "selected"))
        return ButtonStateSelected;
    return -999999;
}

static int
kss_role(const char *name)
{
    if(kss_ieq(name, "Any"))
        return StyleAny();
    if(kss_ieq(name, "Bar") || kss_ieq(name, "MenuBar"))
        return 1;
    if(kss_ieq(name, "Popup") || kss_ieq(name, "Panel") ||
       kss_ieq(name, "MenuPopup"))
        return 2;
    if(kss_ieq(name, "Context") || kss_ieq(name, "MenuContext"))
        return 3;
    if(kss_ieq(name, "Track"))
        return 4;
    if(kss_ieq(name, "Fill"))
        return 5;
    if(kss_ieq(name, "Label") || kss_ieq(name, "Text"))
        return 6;
    if(kss_ieq(name, "Line"))
        return 7;
    if(kss_ieq(name, "Bullet"))
        return 8;
    if(kss_ieq(name, "Box"))
        return 9;
    if(kss_ieq(name, "Mark") || kss_ieq(name, "Check"))
        return 10;
    if(kss_ieq(name, "Ring"))
        return 11;
    if(kss_ieq(name, "Handle"))
        return 12;
    if(kss_ieq(name, "Header"))
        return 13;
    if(kss_ieq(name, "TreeHeader"))
        return 14;
    if(kss_ieq(name, "Close"))
        return 15;
    if(kss_ieq(name, "Title"))
        return 16;
    if(kss_ieq(name, "Action"))
        return 17;
    if(kss_ieq(name, "Divider"))
        return 18;
    if(kss_ieq(name, "Scrim"))
        return 19;
    if(kss_ieq(name, "Message"))
        return 20;
    if(kss_ieq(name, "Row"))
        return 21;
    if(kss_ieq(name, "Cell"))
        return 22;
    if(kss_ieq(name, "Selection") || kss_ieq(name, "Selected"))
        return 23;
    if(kss_ieq(name, "Anchor"))
        return 24;
    return -999999;
}

static int
kss_tone(const char *name)
{
    if(kss_ieq(name, "any"))
        return StyleAny();
    if(kss_ieq(name, "Neutral"))
        return ButtonToneNeutral;
    if(kss_ieq(name, "Accent"))
        return ButtonToneAccent;
    if(kss_ieq(name, "Danger"))
        return ButtonToneDanger;
    if(kss_ieq(name, "Success"))
        return ButtonToneSuccess;
    if(kss_ieq(name, "Warning"))
        return ButtonToneWarning;
    return -999999;
}

static int
kss_emphasis(const char *name)
{
    if(kss_ieq(name, "any"))
        return StyleAny();
    if(kss_ieq(name, "Filled"))
        return ButtonEmphasisFilled;
    if(kss_ieq(name, "Soft"))
        return ButtonEmphasisSoft;
    if(kss_ieq(name, "Outline"))
        return ButtonEmphasisOutline;
    if(kss_ieq(name, "Ghost"))
        return ButtonEmphasisGhost;
    if(kss_ieq(name, "Link"))
        return ButtonEmphasisLink;
    return -999999;
}

static int
kss_size(const char *name)
{
    if(kss_ieq(name, "any"))
        return StyleAny();
    if(kss_ieq(name, "Medium"))
        return ControlSizeMedium;
    if(kss_ieq(name, "Small"))
        return ControlSizeSmall;
    if(kss_ieq(name, "Large"))
        return ControlSizeLarge;
    return -999999;
}

static int
kss_material(const char *name)
{
    if(kss_ieq(name, "Lightfield"))
        return MaterialLightfield;
    if(kss_ieq(name, "Flat"))
        return MaterialFlat;
    if(kss_ieq(name, "Glass"))
        return MaterialGlass;
    return -999999;
}

static bool
kss_read_hex_color(KssParser *p, uint32_t *out)
{
    uint32_t value = 0;
    int digits = 0;

    kss_skip_ws(p);
    if(*p->cursor != '#')
        return false;
    p->cursor++;
    while(isxdigit((unsigned char)*p->cursor)) {
        int digit;
        char c = *p->cursor++;
        if(c >= '0' && c <= '9')
            digit = c - '0';
        else if(c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            digit = c - 'A' + 10;
        value = (value << 4) | (uint32_t)digit;
        digits++;
    }
    if(digits == 6)
        value = (value << 8) | 0xffu;
    else if(digits != 8)
        return false;
    *out = value;
    return true;
}

static bool
kss_read_number(KssParser *p, float *out)
{
    char *end = NULL;

    kss_skip_ws(p);
    *out = strtof(p->cursor, &end);
    if(end == p->cursor)
        return false;
    p->cursor = end;
    return true;
}

static bool
kss_read_duration(KssParser *p, float *out)
{
    char unit[8];
    const char *save;
    float value;

    if(!kss_read_number(p, &value))
        return false;
    save = p->cursor;
    if(kss_read_ident(p, unit, sizeof(unit))) {
        if(kss_ieq(unit, "ms")) {
            *out = value;
            return true;
        }
        if(kss_ieq(unit, "s")) {
            *out = value * 1000.0f;
            return true;
        }
        p->cursor = save;
        return false;
    }
    *out = value;
    return true;
}

static void
kss_copy_id(char *dest, size_t dest_size, const char *src)
{
    size_t i;

    if(dest_size == 0)
        return;
    for(i = 0; src[i] != '\0' && i + 1 < dest_size; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

static bool
kss_find_color_token(KssParser *p, const char *name, uint32_t *out)
{
    for(int i = p->token_count - 1; i >= 0; i--)
        if(p->tokens[i].kind == KSS_TOKEN_COLOR &&
           strcmp(p->tokens[i].name, name) == 0) {
            *out = p->tokens[i].color;
            return true;
        }
    return false;
}

static bool
kss_find_length_token(KssParser *p, const char *name, float *out)
{
    for(int i = p->token_count - 1; i >= 0; i--)
        if((p->tokens[i].kind == KSS_TOKEN_LENGTH ||
            p->tokens[i].kind == KSS_TOKEN_DURATION) &&
           strcmp(p->tokens[i].name, name) == 0) {
            *out = p->tokens[i].number;
            return true;
        }
    return false;
}

static bool
kss_find_material_token(KssParser *p, const char *name, int *out)
{
    for(int i = p->token_count - 1; i >= 0; i--)
        if(p->tokens[i].kind == KSS_TOKEN_MATERIAL &&
           strcmp(p->tokens[i].name, name) == 0) {
            *out = p->tokens[i].material;
            return true;
        }
    return false;
}

static bool
kss_add_token(KssParser *p, KssToken token)
{
    if(p->token_count >= KSS_TOKEN_MAX)
        return kss_fail(p, "style token capacity exceeded");
    p->tokens[p->token_count++] = token;
    return true;
}

static bool
kss_read_color_value(KssParser *p, uint32_t *out)
{
    char ident[64];
    const char *save;

    if(kss_read_hex_color(p, out))
        return true;
    save = p->cursor;
    if(kss_read_ident(p, ident, sizeof(ident)) &&
       kss_find_color_token(p, ident, out))
        return true;
    p->cursor = save;
    return false;
}

static bool
kss_read_number_value(KssParser *p, float *out)
{
    char ident[64];
    const char *save = p->cursor;

    if(kss_read_number(p, out))
        return true;
    p->cursor = save;
    if(kss_read_ident(p, ident, sizeof(ident)) &&
       kss_find_length_token(p, ident, out))
        return true;
    p->cursor = save;
    return false;
}

static bool
kss_read_material_value(KssParser *p, int *out)
{
    char ident[64];
    int mapped;
    const char *save = p->cursor;

    if(!kss_read_ident(p, ident, sizeof(ident)))
        return false;
    mapped = kss_material(ident);
    if(mapped != -999999) {
        *out = mapped;
        return true;
    }
    if(kss_find_material_token(p, ident, out))
        return true;
    p->cursor = save;
    return false;
}

static bool
kss_parse_token_group(KssParser *p)
{
    char group[32];
    KssTokenKind kind;

    if(!kss_read_ident(p, group, sizeof(group)))
        return kss_fail(p, "expected token group");
    if(kss_ieq(group, "color"))
        kind = KSS_TOKEN_COLOR;
    else if(kss_ieq(group, "length") || kss_ieq(group, "number"))
        kind = KSS_TOKEN_LENGTH;
    else if(kss_ieq(group, "duration"))
        kind = KSS_TOKEN_DURATION;
    else if(kss_ieq(group, "material"))
        kind = KSS_TOKEN_MATERIAL;
    else
        return kss_fail(p, "unknown token group '%s'", group);
    if(!kss_expect(p, '{'))
        return kss_fail(p, "expected '{' after token group");

    for(;;) {
        KssToken token = {0};
        kss_skip_ws(p);
        if(*p->cursor == '}') {
            p->cursor++;
            return true;
        }
        if(*p->cursor == '\0')
            return kss_fail(p, "unterminated token group");
        token.kind = kind;
        if(!kss_read_ident(p, token.name, sizeof(token.name)))
            return kss_fail(p, "expected token name");
        if(!kss_expect(p, ':'))
            return kss_fail(p, "expected ':' after token name");
        if(kind == KSS_TOKEN_COLOR) {
            if(!kss_read_hex_color(p, &token.color))
                return kss_fail(p, "expected token color");
        } else if(kind == KSS_TOKEN_LENGTH) {
            if(!kss_read_number(p, &token.number))
                return kss_fail(p, "expected token number");
        } else if(kind == KSS_TOKEN_DURATION) {
            if(!kss_read_duration(p, &token.number))
                return kss_fail(p, "expected token duration");
        } else if(!kss_read_material_value(p, &token.material)) {
            return kss_fail(p, "expected token material");
        }
        if(!kss_expect(p, ';'))
            return kss_fail(p, "expected ';'");
        if(!kss_add_token(p, token))
            return false;
    }
}

static bool
kss_parse_tokens(KssParser *p)
{
    if(!kss_expect(p, '{'))
        return kss_fail(p, "expected '{' after tokens");
    for(;;) {
        kss_skip_ws(p);
        if(*p->cursor == '}') {
            p->cursor++;
            return true;
        }
        if(*p->cursor == '\0')
            return kss_fail(p, "unterminated tokens block");
        if(!kss_parse_token_group(p))
            return false;
    }
}

static bool
kss_parse_directive(KssParser *p)
{
    char keyword[32];
    char value[64];

    p->cursor++;
    if(!kss_read_ident(p, keyword, sizeof(keyword)))
        return kss_fail(p, "expected directive name");
    if(kss_ieq(keyword, "pack")) {
        if(!kss_read_ident(p, value, sizeof(value)))
            return kss_fail(p, "expected pack id");
        kss_copy_id(p->pack_id, sizeof(p->pack_id), value);
        return kss_expect(p, ';') ? true : kss_fail(p, "expected ';'");
    }
    if(kss_ieq(keyword, "layer")) {
        if(!kss_read_ident(p, value, sizeof(value)))
            return kss_fail(p, "expected layer name");
        if(kss_ieq(value, "reset") || kss_ieq(value, "base") ||
           kss_ieq(value, "defaults"))
            p->layer = 0;
        else if(kss_ieq(value, "components") || kss_ieq(value, "widgets"))
            p->layer = 1;
        else if(kss_ieq(value, "app"))
            p->layer = 2;
        else if(kss_ieq(value, "overrides"))
            p->layer = 3;
        else
            return kss_fail(p, "unknown layer '%s'", value);
        return kss_expect(p, ';') ? true : kss_fail(p, "expected ';'");
    }
    return kss_fail(p, "unknown directive '@%s'", keyword);
}

static bool
kss_apply_attr(KssParser *p, StyleSelector *selector)
{
    char name[32];
    char value[32];
    int mapped;

    if(!kss_read_ident(p, name, sizeof(name)))
        return kss_fail(p, "expected selector attribute");
    if(!kss_expect(p, '='))
        return kss_fail(p, "expected '=' in selector attribute");
    if(!kss_read_ident(p, value, sizeof(value)))
        return kss_fail(p, "expected selector attribute value");
    if(!kss_expect(p, ']'))
        return kss_fail(p, "expected ']'");

    if(kss_ieq(name, "tone")) {
        mapped = kss_tone(value);
        if(mapped == -999999)
            return kss_fail(p, "unknown tone '%s'", value);
        selector->tone = mapped;
        return true;
    }
    if(kss_ieq(name, "emphasis")) {
        mapped = kss_emphasis(value);
        if(mapped == -999999)
            return kss_fail(p, "unknown emphasis '%s'", value);
        selector->emphasis = mapped;
        return true;
    }
    if(kss_ieq(name, "size")) {
        mapped = kss_size(value);
        if(mapped == -999999)
            return kss_fail(p, "unknown size '%s'", value);
        selector->size = mapped;
        return true;
    }
    if(kss_ieq(name, "state")) {
        mapped = kss_state(value);
        if(mapped == -999999)
            return kss_fail(p, "unknown state '%s'", value);
        selector->state = mapped;
        return true;
    }
    if(kss_ieq(name, "role")) {
        mapped = kss_role(value);
        if(mapped == -999999)
            return kss_fail(p, "unknown role '%s'", value);
        selector->role = mapped;
        return true;
    }
    if(kss_ieq(name, "class")) {
        selector->class_name = StyleClassId(value);
        return true;
    }
    return kss_fail(p, "unknown selector attribute '%s'", name);
}

static bool
kss_parse_selector(KssParser *p, StyleRule *rule)
{
    char kind[48];
    int mapped;

    rule->selector = StyleDefaultSelector();
    rule->state = StyleStateAny();
    if(!kss_read_selector_ident(p, kind, sizeof(kind)))
        return kss_fail(p, "expected style selector");
    mapped = kss_style_kind(kind);
    if(mapped == -999999)
        return kss_fail(p, "unknown style selector '%s'", kind);
    rule->selector.kind = mapped;

    for(;;) {
        char state[32];
        kss_skip_ws(p);
        if(*p->cursor == '[') {
            p->cursor++;
            if(!kss_apply_attr(p, &rule->selector))
                return false;
            continue;
        }
        if(*p->cursor == '.') {
            char class_name[48];
            p->cursor++;
            if(!kss_read_selector_ident(p, class_name, sizeof(class_name)))
                return kss_fail(p, "expected class name after '.'");
            rule->selector.class_name = StyleClassId(class_name);
            continue;
        }
        if(*p->cursor == ':') {
            p->cursor++;
            if(!kss_read_ident(p, state, sizeof(state)))
                return kss_fail(p, "expected state after ':'");
            mapped = kss_state(state);
            if(mapped == -999999)
                return kss_fail(p, "unknown state '%s'", state);
            rule->state = mapped;
            continue;
        }
        break;
    }
    return true;
}

static bool
kss_parse_property(KssParser *p, StyleData *style)
{
    char name[48];
    uint32_t color;
    float number;
    char ident[48];
    String text;

    if(!kss_read_ident(p, name, sizeof(name)))
        return kss_fail(p, "expected property name");
    if(!kss_expect(p, ':'))
        return kss_fail(p, "expected ':' after property");

    if(kss_ieq(name, "background")) {
        if(!kss_read_color_value(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBackground;
        style->background = color;
    } else if(kss_ieq(name, "foreground")) {
        if(!kss_read_color_value(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleForeground;
        style->foreground = color;
    } else if(kss_ieq(name, "border")) {
        if(!kss_read_color_value(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBorder;
        style->border = color;
    } else if(kss_ieq(name, "focus")) {
        if(!kss_read_color_value(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleFocus;
        style->focus = color;
    } else if(kss_ieq(name, "background-end")) {
        if(!kss_read_color_value(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBackgroundEnd;
        style->background_end = color;
    } else if(kss_ieq(name, "radius")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleRadius;
        style->radius = number;
    } else if(kss_ieq(name, "border-width")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleBorderWidth;
        style->border_width = number;
    } else if(kss_ieq(name, "opacity")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleOpacity;
        style->opacity = number;
    } else if(kss_ieq(name, "padding-x")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StylePaddingX;
        style->padding_x = number;
    } else if(kss_ieq(name, "padding-y")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StylePaddingY;
        style->padding_y = number;
    } else if(kss_ieq(name, "gap")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleGap;
        style->gap = number;
    } else if(kss_ieq(name, "font-size")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleFontSize;
        style->font_size = number;
    } else if(kss_ieq(name, "icon-size")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleIconSize;
        style->icon_size = number;
    } else if(kss_ieq(name, "offset-x")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleContentOffset;
        style->offset_x = number;
    } else if(kss_ieq(name, "offset-y")) {
        if(!kss_read_number_value(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleContentOffset;
        style->offset_y = number;
    } else if(kss_ieq(name, "material")) {
        int mapped;
        (void)ident;
        if(!kss_read_material_value(p, &mapped))
            return kss_fail(p, "expected material");
        style->fields |= (uint32_t)StyleMaterial;
        style->material = mapped;
    } else if(kss_ieq(name, "typeface")) {
        if(!kss_read_ident_view(p, &text))
            return kss_fail(p, "expected typeface name");
        style->fields |= (uint32_t)StyleTypeface;
        style->typeface = text;
    } else {
        return kss_fail(p, "unknown property '%s'", name);
    }

    return kss_expect(p, ';') ? true : kss_fail(p, "expected ';'");
}

static bool
kss_parse_rule(KssParser *p)
{
    StyleRule rule = {0};

    if(p->rule_count >= p->rule_capacity)
        return kss_fail(p, "style rule capacity exceeded");
    if(!kss_parse_selector(p, &rule))
        return false;
    if(!kss_expect(p, '{'))
        return kss_fail(p, "expected '{'");

    rule.layer = p->layer;
    rule.order = p->rule_count;
    for(;;) {
        kss_skip_ws(p);
        if(*p->cursor == '\0')
            return kss_fail(p, "unterminated style rule");
        if(*p->cursor == '}') {
            p->cursor++;
            break;
        }
        if(!kss_parse_property(p, &rule.style))
            return false;
    }

    p->rules[p->rule_count++] = rule;
    return true;
}

bool
kss_parse_string(const char *source, StyleRule *rules, int rule_capacity,
                 KssParseResult *result, char *diagnostic,
                 size_t diagnostic_size)
{
    KssParser parser = {0};

    if(diagnostic != NULL && diagnostic_size > 0)
        diagnostic[0] = '\0';
    if(source == NULL || rules == NULL || rule_capacity < 0)
        return false;

    parser.source = source;
    parser.cursor = source;
    parser.rules = rules;
    parser.rule_capacity = rule_capacity;
    parser.diagnostic = diagnostic;
    parser.diagnostic_size = diagnostic_size;

    while(true) {
        const char *save;
        char keyword[32];

        kss_skip_ws(&parser);
        if(*parser.cursor == '\0')
            break;
        if(*parser.cursor == '@') {
            if(!kss_parse_directive(&parser))
                return false;
            continue;
        }
        save = parser.cursor;
        if(kss_read_ident(&parser, keyword, sizeof(keyword)) &&
           kss_ieq(keyword, "tokens")) {
            if(!kss_parse_tokens(&parser))
                return false;
            continue;
        }
        parser.cursor = save;
        if(!kss_parse_rule(&parser)) {
            return false;
        }
    }

    if(result != NULL) {
        result->rule_count = parser.rule_count;
        kss_copy_id(result->pack_id, sizeof(result->pack_id), parser.pack_id);
    }
    return true;
}
