#include "kss_parser.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct KssParser {
    const char *source;
    const char *cursor;
    StyleRule *rules;
    int rule_capacity;
    int rule_count;
    int layer;
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
    if(kss_ieq(name, "Toggle"))
        return StyleKindToggle();
    if(kss_ieq(name, "Checkbox"))
        return StyleKindCheckbox();
    if(kss_ieq(name, "Radio"))
        return StyleKindRadio();
    if(kss_ieq(name, "Progress"))
        return StyleKindProgress();
    if(kss_ieq(name, "Separator"))
        return StyleKindSeparator();
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
    return kss_fail(p, "unknown selector attribute '%s'", name);
}

static bool
kss_parse_selector(KssParser *p, StyleRule *rule)
{
    char kind[48];
    int mapped;

    rule->selector = StyleDefaultSelector();
    rule->state = StyleStateAny();
    if(!kss_read_ident(p, kind, sizeof(kind)))
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

    if(!kss_read_ident(p, name, sizeof(name)))
        return kss_fail(p, "expected property name");
    if(!kss_expect(p, ':'))
        return kss_fail(p, "expected ':' after property");

    if(kss_ieq(name, "background") || kss_ieq(name, "background-color")) {
        if(!kss_read_hex_color(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBackground;
        style->background = color;
    } else if(kss_ieq(name, "foreground") || kss_ieq(name, "color")) {
        if(!kss_read_hex_color(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleForeground;
        style->foreground = color;
    } else if(kss_ieq(name, "border") || kss_ieq(name, "border-color")) {
        if(!kss_read_hex_color(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBorder;
        style->border = color;
    } else if(kss_ieq(name, "focus") || kss_ieq(name, "focus-color")) {
        if(!kss_read_hex_color(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleFocus;
        style->focus = color;
    } else if(kss_ieq(name, "background-end") ||
              kss_ieq(name, "background_end")) {
        if(!kss_read_hex_color(p, &color))
            return kss_fail(p, "expected hex color");
        style->fields |= (uint32_t)StyleBackgroundEnd;
        style->background_end = color;
    } else if(kss_ieq(name, "radius")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleRadius;
        style->radius = number;
    } else if(kss_ieq(name, "border-width") ||
              kss_ieq(name, "border_width")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleBorderWidth;
        style->border_width = number;
    } else if(kss_ieq(name, "opacity")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleOpacity;
        style->opacity = number;
    } else if(kss_ieq(name, "padding-x") || kss_ieq(name, "padding_x")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StylePaddingX;
        style->padding_x = number;
    } else if(kss_ieq(name, "padding-y") || kss_ieq(name, "padding_y")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StylePaddingY;
        style->padding_y = number;
    } else if(kss_ieq(name, "gap")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleGap;
        style->gap = number;
    } else if(kss_ieq(name, "font-size") || kss_ieq(name, "font_size")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleFontSize;
        style->font_size = number;
    } else if(kss_ieq(name, "icon-size") || kss_ieq(name, "icon_size")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleIconSize;
        style->icon_size = number;
    } else if(kss_ieq(name, "offset-x") || kss_ieq(name, "offset_x")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleContentOffset;
        style->offset_x = number;
    } else if(kss_ieq(name, "offset-y") || kss_ieq(name, "offset_y")) {
        if(!kss_read_number(p, &number))
            return kss_fail(p, "expected number");
        style->fields |= (uint32_t)StyleContentOffset;
        style->offset_y = number;
    } else if(kss_ieq(name, "material")) {
        int mapped;
        if(!kss_read_ident(p, ident, sizeof(ident)))
            return kss_fail(p, "expected material");
        mapped = kss_material(ident);
        if(mapped == -999999)
            return kss_fail(p, "unknown material '%s'", ident);
        style->fields |= (uint32_t)StyleMaterial;
        style->material = mapped;
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
        kss_skip_ws(&parser);
        if(*parser.cursor == '\0')
            break;
        if(*parser.cursor == '@') {
            if(!kss_parse_directive(&parser))
                return false;
        } else if(!kss_parse_rule(&parser)) {
            return false;
        }
    }

    if(result != NULL) {
        result->rule_count = parser.rule_count;
        kss_copy_id(result->pack_id, sizeof(result->pack_id), parser.pack_id);
    }
    return true;
}
