/*
 * kir_parse.c - shared Kir frontend: parse .kry source into a KirProgram.
 * Linked by k2kir (dump tool), k2c (C backend), and k2b (krb backend).
 */
#include "kir.h"
#include "kir_parse.h"
#include "kir_text.h"
#include "kir_cleanup.h"
#include "kir_expr.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    K2KIR_PATH_MAX = 1024,
    K2KIR_LINE_MAX = 1024
};

static void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "k2kir: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t' ||
            s[n] == '(' || s[n] == '"' || s[n] == '{' ||
            s[n] == ':');   /* 'default:' — label-style case keyword */
}

static int
is_identifier_text(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if(cursor == NULL || (!isalpha(*cursor) && *cursor != '_'))
        return 0;
    cursor++;
    while(isalnum(*cursor) || *cursor == '_')
        cursor++;
    return *cursor == '\0';
}

static int
source_column_for_trimmed(const char *line, const char *trimmed)
{
    if(line == NULL || trimmed == NULL || trimmed < line)
        return 1;
    return (int)(trimmed - line) + 1;
}

static int
parse_symbol_before_colons(const char *s, char *out, size_t out_size)
{
    const char *p;
    const char *q;
    size_t n = 0;

    out[0] = '\0';
    p = strstr(s, "::");
    if(p == NULL)
        return 0;
    q = s;
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
          n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    while(q < p && (*q == ' ' || *q == '\t'))
        q++;
    return out[0] != '\0' && q == p;
}

static const char *
relative_path(const char *root, const char *path)
{
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && (path[n] == '/' || path[n] == '\0')) {
        if(path[n] == '/')
            return path + n + 1;
        return path + n;
    }
    return path;
}

static int
parse_quoted(const char *s, char *out, size_t out_size)
{
    const char *q;
    size_t n = 0;

    q = strchr(s, '"');
    if(q == NULL)
        return 0;
    q++;
    while(*q != '\0' && *q != '"' && n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    return *q == '"';
}

static int
parse_route_header(const char *s, char *out, size_t out_size)
{
    const char *p;
    size_t n = 0;

    if(!starts_word(s, "route"))
        return 0;
    p = s + 5;
    while(*p == ' ' || *p == '\t')
        p++;
    while((isalnum((unsigned char)*p) || *p == '_') && n + 1 < out_size)
        out[n++] = *p++;
    out[n] = '\0';
    while(*p == ' ' || *p == '\t')
        p++;
    return out[0] != '\0' && strchr(p, '{') != NULL;
}

static int
parse_angled(const char *s, char *out, size_t out_size)
{
    const char *q;
    size_t n = 0;

    q = strchr(s, '<');
    if(q == NULL)
        return 0;
    q++;
    while(*q != '\0' && *q != '>' && n + 1 < out_size)
        out[n++] = *q++;
    out[n] = '\0';
    return *q == '>';
}

static int
is_c_ident(const char *s)
{
    if(s == NULL || s[0] == '\0')
        return 0;
    if(!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for(const char *p = s + 1; *p != '\0'; p++) {
        if(!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }
    return 1;
}

static KirExternKind
classify_extern_target(const char *target, char *symbol, size_t symbol_size,
                       const char *path, int line_no)
{
    const char *dot;
    const char *slash;

    symbol[0] = '\0';
    if(target == NULL || target[0] == '\0')
        return KIR_EXTERN_HOST;
    if(strncmp(target, "c.", 2) == 0) {
        if(!is_c_ident(target + 2))
            die("%s:%d: C extern target must be c.<symbol>", path, line_no);
        snprintf(symbol, symbol_size, "%s", target + 2);
        return KIR_EXTERN_C;
    }
    dot = strrchr(target, '.');
    slash = strrchr(target, '/');
    if(dot != NULL && slash != NULL && slash < dot)
        return KIR_EXTERN_GO;
    return KIR_EXTERN_HOST;
}

/* Net block braces: only '{'/'}' at paren/bracket depth 0 open/close
 * blocks. Braces inside parens (compound literals like (Props){...}) are
 * expression braces, not blocks. */
static int
net_block_braces(const char *s)
{
    int pd = 0;
    int in_s = 0;
    int in_c = 0;
    int delta = 0;

    for(const char *p = s; *p != '\0'; p++) {
        if(in_s) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_s = 0;
        } else if(in_c) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_c = 0;
        } else if(*p == '"') {
            in_s = 1;
        } else if(*p == '\'') {
            in_c = 1;
        } else if(*p == '(' || *p == '[') {
            pd++;
        } else if(*p == ')' || *p == ']') {
            if(pd > 0)
                pd--;
        } else if(pd == 0) {
            if(*p == '{')
                delta++;
            else if(*p == '}')
                delta--;
        }
    }
    return delta;
}

static KirStmtKind
classify_stmt(const char *s)
{
    if(s[0] == '}')
        return KIR_STMT_BLOCK_CLOSE;
    if(strcmp(s, "{") == 0)
        return KIR_STMT_BLOCK_OPEN;
    if(starts_word(s, "if") || starts_word(s, "else"))
        return KIR_STMT_IF;
    if(starts_word(s, "guard"))
        return KIR_STMT_IF;   /* 'guard cond' lowers to if(cond) return */
    if(starts_word(s, "while"))
        return KIR_STMT_WHILE;
    if(starts_word(s, "for"))
        return KIR_STMT_FOR;
    if(starts_word(s, "switch"))
        return KIR_STMT_SWITCH;
    if(starts_word(s, "case") || starts_word(s, "default"))
        return KIR_STMT_CASE;
    if(strcmp(s, "default:") == 0)
        return KIR_STMT_CASE;
    if(starts_word(s, "return"))
        return KIR_STMT_RETURN;
    if(strcmp(s, "break") == 0 || strcmp(s, "break;") == 0)
        return KIR_STMT_BREAK;
    if(strcmp(s, "continue") == 0 || strcmp(s, "continue;") == 0)
        return KIR_STMT_CONTINUE;
    if(starts_word(s, "goto"))
        return KIR_STMT_GOTO;
    /* goto label: a bare 'name:' / 'name: ;' — a following type marks a
     * declaration ('scalar: int = 5'), not a label */
    {
        size_t n = 0;

        while(isalnum((unsigned char)s[n]) || s[n] == '_')
            n++;
        if(n > 0 && s[n] == ':') {
            const char *rest = s + n + 1;

            while(*rest == ' ' || *rest == '\t')
                rest++;
            if(*rest == '\0' || strcmp(rest, ";") == 0)
                return KIR_STMT_LABEL;
        }
    }
    if(starts_word(s, "defer"))
        return KIR_STMT_DEFER;
    if(starts_word(s, "unused"))
        return KIR_STMT_UNUSED;
    if(strstr(s, ":=") != NULL)
        return KIR_STMT_DECL;   /* ':=' wins over the raw 'c' prefix (a
                                   variable may be named 'c') */
    if(starts_word(s, "c") && s[1] != ':') {
        /* 'c <raw C line>' glue — but 'c = ...' / 'c += ...' is an
         * assignment to a local named c, which needs the normal statement
         * path (raw lines emit without a trailing semicolon). 'c:' is a
         * typed decl of a variable named c. */
        const char *rest = s + 1;

        while(*rest == ' ' || *rest == '\t')
            rest++;
        if(*rest != '=' && strncmp(rest, "+=", 2) != 0 &&
           strncmp(rest, "-=", 2) != 0 && strncmp(rest, "*=", 2) != 0 &&
           strncmp(rest, "/=", 2) != 0 && strncmp(rest, "%=", 2) != 0 &&
           strncmp(rest, "&=", 2) != 0 && strncmp(rest, "|=", 2) != 0 &&
           strncmp(rest, "^=", 2) != 0 && strncmp(rest, "<<=", 3) != 0 &&
           strncmp(rest, ">>=", 3) != 0)
            return KIR_STMT_RAW;
    }
    if(strstr(s, "::") != NULL)
        return KIR_STMT_RAW;   /* nested '::' definitions stay raw */
    if(strstr(s, ": ") != NULL || strstr(s, ": [") != NULL) {
        /* typed decl only when an identifier precedes the colon */
        const char *c2 = strstr(s, ": ");

        if(c2 == NULL)
            c2 = strstr(s, ": [");
        if(c2 != NULL && c2 > s &&
           (isalpha((unsigned char)s[0]) || s[0] == '_')) {
            int ident_only = 1;

            for(const char *q = s; q < c2; q++)
                if(!(isalnum((unsigned char)*q) || *q == '_'))
                    ident_only = 0;
            if(ident_only)
                return KIR_STMT_DECL;  /* 'x: T' / 'x: [N] T' */
        }
    }
    /* C-style locals remain accepted at the language boundary. Lowerers
     * already treat declaration statements without ':' as an opaque typed
     * declaration and add the target terminator. */
    {
        static const char *const types[] = {
            "int ", "unsigned ", "long ", "float ", "double ",
            "char ", "bool ", "const ", "struct ", NULL
        };
        int i;

        for(i = 0; types[i] != NULL; i++)
            if(strncmp(s, types[i], strlen(types[i])) == 0)
                return KIR_STMT_DECL;
    }
    /* An '=' inside a call's compound literal is a designated initializer,
     * not an assignment statement (TextField((Props){.text = value})). */
    {
        int depth = 0;
        int quote = 0;
        const char *p;

        for(p = s; *p != '\0'; p++) {
            if(quote) {
                if(*p == '\\' && p[1] != '\0')
                    p++;
                else if(*p == quote)
                    quote = 0;
                continue;
            }
            if(*p == '"' || *p == '\'')
                quote = *p;
            else if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}') {
                if(depth > 0)
                    depth--;
            } else if(*p == '=' && depth == 0)
                return KIR_STMT_ASSIGN;
        }
    }
    if(strchr(s, '(') != NULL || strchr(s, '+') != NULL ||
       strchr(s, '-') != NULL)
        return KIR_STMT_EXPR;
    return KIR_STMT_UNKNOWN;
}

static int
parse_widget_statement(const char *text, char *name, size_t name_size,
                       char *args, size_t args_size)
{
    static const char *const widgets[] = {
        "AppBackground", "Background", "Text", "Paragraph",
        "Box", "Line", "Bevel", "Icon", "Image", "Button", "Card", "Selectable",
        "Bullet", "Separator",
        "Link", "TextField", "TextArea", "Dropdown", "SegmentedControl",
        "Slider", "Menu",
        "Toggle", "Checkbox", "Radio", "Progress", "Plot",
        "Drag", "Input", "Spinbox",
        "DragDrop",
        "Screen", "Column", "Row", "Stack", "End",
        "Modal", "TitleBar", "TabBar",
        "NavigationBar",
        "Toolbar", "Toast", "Fieldset",
		"PanedView", "Collapsible", "ListBox", "TreeView", "TableView",
		"ColorPicker", "CanvasGrid"
    };
    const char *p = text;
    const char *open;
    const char *close;
    size_t length;
    size_t i;
    int known = 0;
    int depth = 0;
    int in_string = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    open = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    length = (size_t)(p - open);
    if(length == 0 || length >= name_size)
        return 0;
    memcpy(name, open, length);
    name[length] = '\0';
    for(i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++)
        if(strcmp(name, widgets[i]) == 0) {
            known = 1;
            break;
        }
    if(!known)
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '(')
        return 0;
    open = p++;
    close = NULL;
    depth = 1;
    while(*p != '\0') {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '(') {
            depth++;
        } else if(*p == ')' && --depth == 0) {
            close = p;
            break;
        }
        p++;
    }
    if(close == NULL || (size_t)(close - open) >= args_size)
        return 0;
    p = close + 1;
    while(*p == ' ' || *p == '\t' || *p == ';')
        p++;
    if(*p != '\0')
        return 0;
    memcpy(args, open + 1, (size_t)(close - open - 1));
    args[close - open - 1] = '\0';
    return 1;
}

typedef struct UiBlock {
    char widget[KIR_NAME_MAX];
    char name[KIR_NAME_MAX];
    char path[KIR_TEXT_MAX];
    char parent_path[KIR_TEXT_MAX];
    KirSourceSpan span;
    char props[KIR_TEXT_MAX];
    char dom_tag[KIR_NAME_MAX];
    char dom_ref[KIR_NAME_MAX];
    char dom_id[KIR_NAME_MAX];
    char dom_name_attr[KIR_NAME_MAX];
    char dom_value_attr[KIR_TEXT_MAX];
    char dom_class[KIR_TEXT_MAX];
    char dom_title[KIR_TEXT_MAX];
    char dom_href[KIR_TEXT_MAX];
    char dom_target[KIR_NAME_MAX];
    char dom_rel[KIR_TEXT_MAX];
    char dom_for_attr[KIR_NAME_MAX];
    char dom_part[KIR_TEXT_MAX];
    char dom_slot[KIR_NAME_MAX];
    char dom_data_attrs[KIR_TEXT_MAX];
    char dom_extra_attrs[KIR_TEXT_MAX];
    char dom_placeholder[KIR_TEXT_MAX];
    char dom_input_type[KIR_NAME_MAX];
    char dom_form_attr[KIR_NAME_MAX];
    char dom_form_action[KIR_TEXT_MAX];
    char dom_form_method[KIR_NAME_MAX];
    char dom_form_enctype[KIR_NAME_MAX];
    char dom_autocomplete[KIR_NAME_MAX];
    char dom_hidden[KIR_NAME_MAX];
    char dom_draggable[KIR_NAME_MAX];
    char dom_spellcheck[KIR_NAME_MAX];
    char dom_contenteditable[KIR_NAME_MAX];
    char dom_autofocus[KIR_NAME_MAX];
    char dom_inert[KIR_NAME_MAX];
    char dom_autocapitalize[KIR_NAME_MAX];
    char dom_enterkeyhint[KIR_NAME_MAX];
    char dom_download[KIR_TEXT_MAX];
    char dom_formnovalidate[KIR_NAME_MAX];
    char dom_novalidate[KIR_NAME_MAX];
    char dom_popover[KIR_NAME_MAX];
    char dom_popover_target[KIR_NAME_MAX];
    char dom_popover_target_action[KIR_NAME_MAX];
    char dom_readonly[KIR_NAME_MAX];
    char dom_required[KIR_NAME_MAX];
    char dom_min[KIR_NAME_MAX];
    char dom_max[KIR_NAME_MAX];
    char dom_step[KIR_NAME_MAX];
    char dom_minlength[KIR_NAME_MAX];
    char dom_maxlength[KIR_NAME_MAX];
    char dom_pattern[KIR_TEXT_MAX];
    char dom_accept[KIR_TEXT_MAX];
    char dom_multiple[KIR_NAME_MAX];
    char dom_inputmode[KIR_NAME_MAX];
    char dom_headers[KIR_TEXT_MAX];
    char dom_scope[KIR_NAME_MAX];
    char dom_colspan[KIR_NAME_MAX];
    char dom_rowspan[KIR_NAME_MAX];
    char dom_tab_index[KIR_NAME_MAX];
    char dom_role[KIR_NAME_MAX];
    char dom_aria_label[KIR_TEXT_MAX];
    char dom_aria_description[KIR_TEXT_MAX];
    char dom_aria_describedby[KIR_TEXT_MAX];
    char dom_aria_labelledby[KIR_TEXT_MAX];
    char dom_aria_activedescendant[KIR_TEXT_MAX];
    char dom_aria_controls[KIR_TEXT_MAX];
    char dom_aria_owns[KIR_TEXT_MAX];
    char dom_aria_sort[KIR_NAME_MAX];
    char dom_aria_orientation[KIR_NAME_MAX];
    char dom_aria_level[KIR_NAME_MAX];
    char dom_aria_posinset[KIR_NAME_MAX];
    char dom_aria_setsize[KIR_NAME_MAX];
    char dom_aria_haspopup[KIR_NAME_MAX];
    char dom_aria_multiselectable[KIR_NAME_MAX];
    char dom_aria_rowindex[KIR_NAME_MAX];
    char dom_aria_colindex[KIR_NAME_MAX];
    char dom_aria_rowcount[KIR_NAME_MAX];
    char dom_aria_colcount[KIR_NAME_MAX];
    char dom_aria_live[KIR_NAME_MAX];
    char dom_aria_attrs[KIR_TEXT_MAX];
    char dom_on_click[KIR_NAME_MAX];
    char dom_on_input[KIR_NAME_MAX];
    char dom_on_before_input[KIR_NAME_MAX];
    char dom_on_change[KIR_NAME_MAX];
    char dom_on_select[KIR_NAME_MAX];
    char dom_on_key[KIR_NAME_MAX];
    char dom_on_invalid[KIR_NAME_MAX];
    char dom_on_submit[KIR_NAME_MAX];
    char dom_on_reset[KIR_NAME_MAX];
    char dom_on_toggle[KIR_NAME_MAX];
    char dom_on_close[KIR_NAME_MAX];
    char dom_on_cancel[KIR_NAME_MAX];
    char dom_on_focus[KIR_NAME_MAX];
    char dom_on_blur[KIR_NAME_MAX];
    char dom_on_scroll[KIR_NAME_MAX];
    char dom_on_mouse_enter[KIR_NAME_MAX];
    char dom_on_mouse_leave[KIR_NAME_MAX];
    char dom_on_mouse_move[KIR_NAME_MAX];
    char dom_on_mouse_down[KIR_NAME_MAX];
    char dom_on_mouse_up[KIR_NAME_MAX];
    char dom_on_wheel[KIR_NAME_MAX];
    char dom_on_drag_start[KIR_NAME_MAX];
    char dom_on_drag_end[KIR_NAME_MAX];
    char dom_on_drag_over[KIR_NAME_MAX];
    char dom_on_drop[KIR_NAME_MAX];
    char dom_on_copy[KIR_NAME_MAX];
    char dom_on_cut[KIR_NAME_MAX];
    char dom_on_paste[KIR_NAME_MAX];
    int anonymous_widget_count;
    int close_depth;
    int statement_index;
    int opened;
    int emits_end;
    int prop_count;
    int has_key;
    char *scope_args[3];
    unsigned scope_fields;
} UiBlock;

typedef struct SlotParseFrame {
    int function_index;
    int depth;
    int root_anonymous_count;
    int block_count;
    UiBlock *blocks;
    int body_count;
    int body_depth[8];
} SlotParseFrame;

static int
is_layout_widget(const char *name)
{
    return strcmp(name, "Screen") == 0 || strcmp(name, "Column") == 0 ||
           strcmp(name, "Row") == 0 || strcmp(name, "Stack") == 0 ||
           strcmp(name, "Button") == 0 || strcmp(name, "Card") == 0;
}

static const char *
ui_block_prop_type(const char *widget)
{
    /* A lexical scope, not a runtime props type or another widget API. */
    if(strcmp(widget, "Disabled") == 0 || strcmp(widget, "Scroll") == 0 ||
       strcmp(widget, "TableCell") == 0 || strcmp(widget, "Canvas") == 0)
        return "";
    if(strcmp(widget, "Popup") == 0)
        return "PopupProps";
    if(strcmp(widget, "Text") == 0)
        return "TextProps";
    if(strcmp(widget, "Row") == 0)
        return "RowProps";
    if(strcmp(widget, "Screen") == 0 || strcmp(widget, "Column") == 0 ||
       strcmp(widget, "Stack") == 0)
        return "ColumnProps";
    if(strcmp(widget, "Button") == 0)
        return "ButtonProps";
    if(strcmp(widget, "Card") == 0)
        return "CardProps";
    if(strcmp(widget, "TextField") == 0)
        return "TextFieldProps";
    if(strcmp(widget, "TextArea") == 0)
        return "TextAreaProps";
    if(strcmp(widget, "Image") == 0)
        return "ImageProps";
    if(strcmp(widget, "Radio") == 0)
        return "RadioProps";
    if(strcmp(widget, "Progress") == 0)
        return "ProgressProps";
    if(strcmp(widget, "ColorPicker") == 0)
        return "ColorPickerProps";
    if(strcmp(widget, "Separator") == 0)
        return "SeparatorProps";
    if(strcmp(widget, "Spinbox") == 0)
        return "SpinboxProps";
    if(strcmp(widget, "Dropdown") == 0)
        return "DropdownProps";
    if(strcmp(widget, "SegmentedControl") == 0)
        return "SegmentedControlProps";
    if(strcmp(widget, "Fieldset") == 0)
        return "FieldsetProps";
    if(strcmp(widget, "PanedView") == 0)
        return "PanedViewProps";
    if(strcmp(widget, "Collapsible") == 0)
        return "CollapsibleProps";
    if(strcmp(widget, "ListBox") == 0)
        return "ListBoxProps";
    if(strcmp(widget, "TableView") == 0)
        return "TableViewProps";
    if(strcmp(widget, "NavigationBar") == 0)
        return "NavigationBarProps";
    if(strcmp(widget, "Toolbar") == 0)
        return "ToolbarProps";
    if(strcmp(widget, "TabBar") == 0)
        return "TabBarProps";
    if(strcmp(widget, "Page") == 0)
        return "PageProps";
    if(strcmp(widget, "Section") == 0)
        return "SectionProps";
    if(strcmp(widget, "Heading") == 0)
        return "HeadingProps";
    if(strcmp(widget, "ParagraphText") == 0)
        return "ParagraphTextProps";
    if(strcmp(widget, "Link") == 0)
        return "LinkProps";
    if(strcmp(widget, "Flow") == 0)
        return "FlowProps";
    if(strcmp(widget, "Grid") == 0)
        return "GridProps";
    return NULL;
}

static int
parse_ui_block_header(const char *text, char *widget, size_t widget_size,
                      char *name, size_t name_size)
{
    const char *p = text;
    const char *start;
    size_t n = 0;

    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    while(isalnum((unsigned char)*p) || *p == '_')
        p++;
    n = (size_t)(p - start);
    if(n == 0 || n >= widget_size)
        return 0;
    memcpy(widget, start, n);
    widget[n] = '\0';
    /* Switch labels share the colon-and-brace shape of a widget block. */
    if(strcmp(widget, "case") == 0 || strcmp(widget, "default") == 0)
        return 0;
    int known_widget = ui_block_prop_type(widget) != NULL;
    while(*p == ' ' || *p == '\t')
        p++;
    start = p;
    n = 0;
    while(isalnum((unsigned char)*p) || *p == '_') {
        if(n + 1 < name_size)
            name[n++] = *p;
        p++;
    }
    name[n] = '\0';
    while(*p == ' ' || *p == '\t')
        p++;
    int has_colon = *p == ':';
    if(has_colon)
        p++;
    while(*p == ' ' || *p == '\t')
        p++;
    if(p[0] != '{' || p[1] != '\0')
        return 0;
    if(!known_widget && !has_colon)
        return 0;
    return name[0] != '\0' || !is_layout_widget(widget) ||
           strcmp(widget, "Button") == 0;
}

static int
parse_ui_prop_line(char *text, char *field, size_t field_size,
                   char *value, size_t value_size)
{
    char *eq;
    char *name;
    char *expr;
    size_t n;

    eq = strchr(text, '=');
    /* Only the property separator must be assignment. Comparisons in its
     * value are valid expressions (for example, when = count >= limit). */
    if(eq == NULL || eq[1] == '=')
        return 0;
    *eq = '\0';
    name = kir_trim(text);
    expr = kir_trim(eq + 1);
    if(name[0] == '\0' || expr[0] == '\0')
        return 0;
    for(const char *p = name; *p != '\0'; p++)
        if(!isalnum((unsigned char)*p) && *p != '_')
            return 0;
    n = strlen(expr);
    if(n > 0 && expr[n - 1] == ';') {
        expr[n - 1] = '\0';
        expr = kir_trim(expr);
    }
    snprintf(field, field_size, "%s", name);
    snprintf(value, value_size, "%s", expr);
    return 1;
}

static void
ui_block_append_prop(UiBlock *block, const char *field, const char *value,
                     KirSourceSpan span)
{
    size_t used = strlen(block->props);
    int length = snprintf(block->props + used, sizeof(block->props) - used,
                          ".%s = %s, ", field, value);
    if(length < 0 || (size_t)length >= sizeof(block->props) - used)
        die("%s:%d: widget properties exceed the declaration size limit: %s",
            span.path, span.line, block->widget);
    block->prop_count++;
    if(strcmp(field, "key") == 0 || strcmp(field, "Key") == 0)
        block->has_key = 1;
}

static int
ui_block_set_web_prop(UiBlock *block, const char *field, const char *value)
{
    if(strcmp(field, "dom") == 0 || strcmp(field, "dom_tag") == 0 ||
       strcmp(field, "html_tag") == 0 || strcmp(field, "tag") == 0) {
        snprintf(block->dom_tag, sizeof(block->dom_tag), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_ref") == 0 || strcmp(field, "web_ref") == 0 ||
       strcmp(field, "kry_ref") == 0) {
        snprintf(block->dom_ref, sizeof(block->dom_ref), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_id") == 0 || strcmp(field, "html_id") == 0) {
        snprintf(block->dom_id, sizeof(block->dom_id), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_name") == 0 || strcmp(field, "html_name") == 0 ||
       strcmp(field, "name_attr") == 0) {
        snprintf(block->dom_name_attr, sizeof(block->dom_name_attr), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "dom_value") == 0 || strcmp(field, "html_value") == 0 ||
       strcmp(field, "value_attr") == 0) {
        snprintf(block->dom_value_attr, sizeof(block->dom_value_attr), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "class") == 0 || strcmp(field, "classes") == 0 ||
       strcmp(field, "class_name") == 0) {
        snprintf(block->dom_class, sizeof(block->dom_class), "%s", value);
        return 1;
    }
    if(strcmp(field, "title") == 0 || strcmp(field, "dom_title") == 0 ||
       strcmp(field, "html_title") == 0) {
        snprintf(block->dom_title, sizeof(block->dom_title), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_href") == 0 || strcmp(field, "html_href") == 0) {
        snprintf(block->dom_href, sizeof(block->dom_href), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_target") == 0 || strcmp(field, "html_target") == 0) {
        snprintf(block->dom_target, sizeof(block->dom_target), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_rel") == 0 || strcmp(field, "html_rel") == 0) {
        snprintf(block->dom_rel, sizeof(block->dom_rel), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_for") == 0 || strcmp(field, "html_for") == 0 ||
       strcmp(field, "for_attr") == 0) {
        snprintf(block->dom_for_attr, sizeof(block->dom_for_attr), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "part") == 0 || strcmp(field, "dom_part") == 0 ||
       strcmp(field, "html_part") == 0) {
        snprintf(block->dom_part, sizeof(block->dom_part), "%s", value);
        return 1;
    }
    if(strcmp(field, "slot") == 0 || strcmp(field, "dom_slot") == 0 ||
       strcmp(field, "html_slot") == 0) {
        snprintf(block->dom_slot, sizeof(block->dom_slot), "%s", value);
        return 1;
    }
    if(strncmp(field, "data_", 5) == 0 ||
       strncmp(field, "dom_data_", 9) == 0 ||
       strncmp(field, "html_data_", 10) == 0) {
        const char *name = field[0] == 'd' && field[3] == '_' ? field + 9 :
                           field[0] == 'h' ? field + 10 : field + 5;
        char attr[KIR_NAME_MAX];
        size_t used = strlen(block->dom_data_attrs);
        size_t length = 0;

        if(name[0] == '\0')
            return 0;
        for(const char *p = name; *p != '\0'; p++) {
            if(!isalnum((unsigned char)*p) && *p != '_')
                return 0;
            if(length + 1 < sizeof(attr))
                attr[length++] = *p == '_' ? '-' : (char)tolower((unsigned char)*p);
        }
        attr[length] = '\0';
        int written = snprintf(block->dom_data_attrs + used,
                               sizeof(block->dom_data_attrs) - used,
                               "%s\t%s\n", attr, value);
        if(written < 0 || (size_t)written >= sizeof(block->dom_data_attrs) - used)
            return 0;
        return 1;
    }
    if(strncmp(field, "attr_", 5) == 0 ||
       strncmp(field, "dom_attr_", 9) == 0 ||
       strncmp(field, "html_attr_", 10) == 0) {
        const char *name = field[0] == 'd' ? field + 9 :
                           field[0] == 'h' ? field + 10 : field + 5;
        char attr[KIR_NAME_MAX];
        size_t used = strlen(block->dom_extra_attrs);
        size_t length = 0;

        if(name[0] == '\0')
            return 0;
        for(const char *p = name; *p != '\0'; p++) {
            if(!isalnum((unsigned char)*p) && *p != '_' && *p != '-')
                return 0;
            if(length + 1 < sizeof(attr))
                attr[length++] = *p == '_' ? '-' : (char)tolower((unsigned char)*p);
        }
        attr[length] = '\0';
        int written = snprintf(block->dom_extra_attrs + used,
                               sizeof(block->dom_extra_attrs) - used,
                               "%s\t%s\n", attr, value);
        if(written < 0 ||
           (size_t)written >= sizeof(block->dom_extra_attrs) - used)
            return 0;
        return 1;
    }
    if(strcmp(field, "placeholder") == 0 ||
       strcmp(field, "dom_placeholder") == 0) {
        snprintf(block->dom_placeholder, sizeof(block->dom_placeholder), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "input_type") == 0 || strcmp(field, "dom_type") == 0 ||
       strcmp(field, "html_type") == 0 || strcmp(field, "dom_input_type") == 0) {
        snprintf(block->dom_input_type, sizeof(block->dom_input_type), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "form") == 0 || strcmp(field, "dom_form") == 0 ||
       strcmp(field, "html_form") == 0) {
        snprintf(block->dom_form_attr, sizeof(block->dom_form_attr), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "dom_action") == 0 || strcmp(field, "html_action") == 0 ||
       strcmp(field, "form_action") == 0) {
        snprintf(block->dom_form_action, sizeof(block->dom_form_action), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "dom_method") == 0 || strcmp(field, "html_method") == 0 ||
       strcmp(field, "form_method") == 0) {
        snprintf(block->dom_form_method, sizeof(block->dom_form_method), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "dom_enctype") == 0 ||
       strcmp(field, "html_enctype") == 0 ||
       strcmp(field, "form_enctype") == 0) {
        snprintf(block->dom_form_enctype, sizeof(block->dom_form_enctype),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "autocomplete") == 0 ||
       strcmp(field, "dom_autocomplete") == 0 ||
       strcmp(field, "html_autocomplete") == 0) {
        snprintf(block->dom_autocomplete, sizeof(block->dom_autocomplete),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "hidden") == 0 || strcmp(field, "dom_hidden") == 0 ||
       strcmp(field, "html_hidden") == 0) {
        snprintf(block->dom_hidden, sizeof(block->dom_hidden), "%s", value);
        return 1;
    }
    if(strcmp(field, "draggable") == 0 ||
       strcmp(field, "dom_draggable") == 0 ||
       strcmp(field, "html_draggable") == 0) {
        snprintf(block->dom_draggable, sizeof(block->dom_draggable), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "spellcheck") == 0 ||
       strcmp(field, "spell_check") == 0 ||
       strcmp(field, "dom_spellcheck") == 0 ||
       strcmp(field, "html_spellcheck") == 0) {
        snprintf(block->dom_spellcheck, sizeof(block->dom_spellcheck), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "contenteditable") == 0 ||
       strcmp(field, "content_editable") == 0 ||
       strcmp(field, "dom_contenteditable") == 0 ||
       strcmp(field, "html_contenteditable") == 0) {
        snprintf(block->dom_contenteditable,
                 sizeof(block->dom_contenteditable), "%s", value);
        return 1;
    }
    if(strcmp(field, "autofocus") == 0 ||
       strcmp(field, "auto_focus") == 0 ||
       strcmp(field, "dom_autofocus") == 0 ||
       strcmp(field, "html_autofocus") == 0) {
        snprintf(block->dom_autofocus, sizeof(block->dom_autofocus), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "inert") == 0 || strcmp(field, "dom_inert") == 0 ||
       strcmp(field, "html_inert") == 0) {
        snprintf(block->dom_inert, sizeof(block->dom_inert), "%s", value);
        return 1;
    }
    if(strcmp(field, "autocapitalize") == 0 ||
       strcmp(field, "auto_capitalize") == 0 ||
       strcmp(field, "dom_autocapitalize") == 0 ||
       strcmp(field, "html_autocapitalize") == 0) {
        snprintf(block->dom_autocapitalize,
                 sizeof(block->dom_autocapitalize), "%s", value);
        return 1;
    }
    if(strcmp(field, "enterkeyhint") == 0 ||
       strcmp(field, "enter_key_hint") == 0 ||
       strcmp(field, "dom_enterkeyhint") == 0 ||
       strcmp(field, "html_enterkeyhint") == 0) {
        snprintf(block->dom_enterkeyhint,
                 sizeof(block->dom_enterkeyhint), "%s", value);
        return 1;
    }
    if(strcmp(field, "download") == 0 ||
       strcmp(field, "dom_download") == 0 ||
       strcmp(field, "html_download") == 0) {
        snprintf(block->dom_download, sizeof(block->dom_download), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "form_no_validate") == 0 ||
       strcmp(field, "formnovalidate") == 0 ||
       strcmp(field, "dom_formnovalidate") == 0 ||
       strcmp(field, "html_formnovalidate") == 0) {
        snprintf(block->dom_formnovalidate,
                 sizeof(block->dom_formnovalidate), "%s", value);
        return 1;
    }
    if(strcmp(field, "no_validate") == 0 || strcmp(field, "novalidate") == 0 ||
       strcmp(field, "dom_novalidate") == 0 ||
       strcmp(field, "html_novalidate") == 0) {
        snprintf(block->dom_novalidate, sizeof(block->dom_novalidate), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "popover") == 0 || strcmp(field, "dom_popover") == 0 ||
       strcmp(field, "html_popover") == 0) {
        snprintf(block->dom_popover, sizeof(block->dom_popover), "%s", value);
        return 1;
    }
    if(strcmp(field, "popover_target") == 0 ||
       strcmp(field, "popovertarget") == 0 ||
       strcmp(field, "dom_popover_target") == 0 ||
       strcmp(field, "html_popover_target") == 0) {
        snprintf(block->dom_popover_target,
                 sizeof(block->dom_popover_target), "%s", value);
        return 1;
    }
    if(strcmp(field, "popover_target_action") == 0 ||
       strcmp(field, "popovertargetaction") == 0 ||
       strcmp(field, "dom_popover_target_action") == 0 ||
       strcmp(field, "html_popover_target_action") == 0) {
        snprintf(block->dom_popover_target_action,
                 sizeof(block->dom_popover_target_action), "%s", value);
        return 1;
    }
    if(strcmp(field, "readonly") == 0 || strcmp(field, "read_only") == 0 ||
       strcmp(field, "dom_readonly") == 0 || strcmp(field, "html_readonly") == 0) {
        snprintf(block->dom_readonly, sizeof(block->dom_readonly), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "required") == 0 || strcmp(field, "dom_required") == 0 ||
       strcmp(field, "html_required") == 0) {
        snprintf(block->dom_required, sizeof(block->dom_required), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "dom_min") == 0 || strcmp(field, "html_min") == 0 ||
       strcmp(field, "form_min") == 0) {
        snprintf(block->dom_min, sizeof(block->dom_min), "%s", value);
        return 1;
    }
    if(strcmp(field, "dom_max") == 0 || strcmp(field, "html_max") == 0 ||
       strcmp(field, "form_max") == 0) {
        snprintf(block->dom_max, sizeof(block->dom_max), "%s", value);
        return 1;
    }
    if(strcmp(field, "step") == 0 || strcmp(field, "dom_step") == 0 ||
       strcmp(field, "html_step") == 0) {
        snprintf(block->dom_step, sizeof(block->dom_step), "%s", value);
        return 1;
    }
    if(strcmp(field, "min_length") == 0 || strcmp(field, "minlength") == 0 ||
       strcmp(field, "dom_minlength") == 0 || strcmp(field, "html_minlength") == 0) {
        snprintf(block->dom_minlength, sizeof(block->dom_minlength), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "max_length") == 0 || strcmp(field, "maxlength") == 0 ||
       strcmp(field, "dom_maxlength") == 0 || strcmp(field, "html_maxlength") == 0) {
        snprintf(block->dom_maxlength, sizeof(block->dom_maxlength), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "pattern") == 0 || strcmp(field, "dom_pattern") == 0 ||
       strcmp(field, "html_pattern") == 0) {
        snprintf(block->dom_pattern, sizeof(block->dom_pattern), "%s", value);
        return 1;
    }
    if(strcmp(field, "accept") == 0 || strcmp(field, "dom_accept") == 0 ||
       strcmp(field, "html_accept") == 0) {
        snprintf(block->dom_accept, sizeof(block->dom_accept), "%s", value);
        return 1;
    }
    if(strcmp(field, "multiple") == 0 || strcmp(field, "dom_multiple") == 0 ||
       strcmp(field, "html_multiple") == 0) {
        snprintf(block->dom_multiple, sizeof(block->dom_multiple), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "input_mode") == 0 || strcmp(field, "inputmode") == 0 ||
       strcmp(field, "dom_inputmode") == 0 || strcmp(field, "html_inputmode") == 0) {
        snprintf(block->dom_inputmode, sizeof(block->dom_inputmode), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "headers") == 0 || strcmp(field, "dom_headers") == 0 ||
       strcmp(field, "html_headers") == 0) {
        snprintf(block->dom_headers, sizeof(block->dom_headers), "%s", value);
        return 1;
    }
    if(strcmp(field, "scope") == 0 || strcmp(field, "dom_scope") == 0 ||
       strcmp(field, "html_scope") == 0) {
        snprintf(block->dom_scope, sizeof(block->dom_scope), "%s", value);
        return 1;
    }
    if(strcmp(field, "colspan") == 0 || strcmp(field, "col_span") == 0 ||
       strcmp(field, "dom_colspan") == 0 || strcmp(field, "html_colspan") == 0) {
        snprintf(block->dom_colspan, sizeof(block->dom_colspan), "%s", value);
        return 1;
    }
    if(strcmp(field, "rowspan") == 0 || strcmp(field, "row_span") == 0 ||
       strcmp(field, "dom_rowspan") == 0 || strcmp(field, "html_rowspan") == 0) {
        snprintf(block->dom_rowspan, sizeof(block->dom_rowspan), "%s", value);
        return 1;
    }
    if(strcmp(field, "tab_index") == 0 || strcmp(field, "tabindex") == 0 ||
       strcmp(field, "dom_tab_index") == 0) {
        snprintf(block->dom_tab_index, sizeof(block->dom_tab_index), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "role") == 0) {
        snprintf(block->dom_role, sizeof(block->dom_role), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_label") == 0 ||
       strcmp(field, "accessible_label") == 0) {
        snprintf(block->dom_aria_label, sizeof(block->dom_aria_label), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "aria_description") == 0 ||
       strcmp(field, "accessible_description") == 0) {
        snprintf(block->dom_aria_description,
                 sizeof(block->dom_aria_description), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_describedby") == 0 ||
       strcmp(field, "aria_described_by") == 0) {
        snprintf(block->dom_aria_describedby,
                 sizeof(block->dom_aria_describedby), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_labelledby") == 0 ||
       strcmp(field, "aria_labelled_by") == 0) {
        snprintf(block->dom_aria_labelledby,
                 sizeof(block->dom_aria_labelledby), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_activedescendant") == 0 ||
       strcmp(field, "aria_active_descendant") == 0) {
        snprintf(block->dom_aria_activedescendant,
                 sizeof(block->dom_aria_activedescendant), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_controls") == 0) {
        snprintf(block->dom_aria_controls, sizeof(block->dom_aria_controls),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_owns") == 0 || strcmp(field, "aria_own") == 0) {
        snprintf(block->dom_aria_owns, sizeof(block->dom_aria_owns), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "aria_sort") == 0 || strcmp(field, "aria_sorted") == 0) {
        snprintf(block->dom_aria_sort, sizeof(block->dom_aria_sort), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "aria_orientation") == 0) {
        snprintf(block->dom_aria_orientation,
                 sizeof(block->dom_aria_orientation), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_level") == 0) {
        snprintf(block->dom_aria_level, sizeof(block->dom_aria_level), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "aria_posinset") == 0 ||
       strcmp(field, "aria_pos_in_set") == 0) {
        snprintf(block->dom_aria_posinset,
                 sizeof(block->dom_aria_posinset), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_setsize") == 0 ||
       strcmp(field, "aria_set_size") == 0) {
        snprintf(block->dom_aria_setsize, sizeof(block->dom_aria_setsize),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_haspopup") == 0 ||
       strcmp(field, "aria_has_popup") == 0) {
        snprintf(block->dom_aria_haspopup,
                 sizeof(block->dom_aria_haspopup), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_multiselectable") == 0 ||
       strcmp(field, "aria_multi_selectable") == 0) {
        snprintf(block->dom_aria_multiselectable,
                 sizeof(block->dom_aria_multiselectable), "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_rowindex") == 0 ||
       strcmp(field, "aria_row_index") == 0) {
        snprintf(block->dom_aria_rowindex, sizeof(block->dom_aria_rowindex),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_colindex") == 0 ||
       strcmp(field, "aria_col_index") == 0) {
        snprintf(block->dom_aria_colindex, sizeof(block->dom_aria_colindex),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_rowcount") == 0 ||
       strcmp(field, "aria_row_count") == 0) {
        snprintf(block->dom_aria_rowcount, sizeof(block->dom_aria_rowcount),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_colcount") == 0 ||
       strcmp(field, "aria_col_count") == 0) {
        snprintf(block->dom_aria_colcount, sizeof(block->dom_aria_colcount),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "aria_live") == 0 || strcmp(field, "live") == 0) {
        snprintf(block->dom_aria_live, sizeof(block->dom_aria_live), "%s",
                 value);
        return 1;
    }
    if(strncmp(field, "aria_", 5) == 0 ||
       strncmp(field, "dom_aria_", 9) == 0 ||
       strncmp(field, "html_aria_", 10) == 0) {
        const char *name = field[0] == 'd' && field[3] == '_' ? field + 9 :
                           field[0] == 'h' ? field + 10 : field + 5;
        char attr[KIR_NAME_MAX];
        size_t used = strlen(block->dom_aria_attrs);
        size_t length = 0;

        if(name[0] == '\0')
            return 0;
        for(const char *p = name; *p != '\0'; p++) {
            if(!isalnum((unsigned char)*p) && *p != '_')
                return 0;
            if(length + 1 < sizeof(attr))
                attr[length++] = *p == '_' ? '-' : (char)tolower((unsigned char)*p);
        }
        attr[length] = '\0';
        int written = snprintf(block->dom_aria_attrs + used,
                               sizeof(block->dom_aria_attrs) - used,
                               "%s\t%s\n", attr, value);
        if(written < 0 || (size_t)written >= sizeof(block->dom_aria_attrs) - used)
            return 0;
        return 1;
    }
    if(strcmp(field, "on_click") == 0) {
        snprintf(block->dom_on_click, sizeof(block->dom_on_click), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_input") == 0) {
        snprintf(block->dom_on_input, sizeof(block->dom_on_input), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_before_input") == 0 ||
       strcmp(field, "on_beforeinput") == 0) {
        snprintf(block->dom_on_before_input,
                 sizeof(block->dom_on_before_input), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_change") == 0) {
        snprintf(block->dom_on_change, sizeof(block->dom_on_change), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_select") == 0) {
        snprintf(block->dom_on_select, sizeof(block->dom_on_select), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_key") == 0 || strcmp(field, "on_key_down") == 0) {
        snprintf(block->dom_on_key, sizeof(block->dom_on_key), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_invalid") == 0) {
        snprintf(block->dom_on_invalid, sizeof(block->dom_on_invalid), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_submit") == 0) {
        snprintf(block->dom_on_submit, sizeof(block->dom_on_submit), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_reset") == 0) {
        snprintf(block->dom_on_reset, sizeof(block->dom_on_reset), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_toggle") == 0) {
        snprintf(block->dom_on_toggle, sizeof(block->dom_on_toggle), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_close") == 0) {
        snprintf(block->dom_on_close, sizeof(block->dom_on_close), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_cancel") == 0) {
        snprintf(block->dom_on_cancel, sizeof(block->dom_on_cancel), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_focus") == 0) {
        snprintf(block->dom_on_focus, sizeof(block->dom_on_focus), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_blur") == 0) {
        snprintf(block->dom_on_blur, sizeof(block->dom_on_blur), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_scroll") == 0) {
        snprintf(block->dom_on_scroll, sizeof(block->dom_on_scroll), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_mouse_enter") == 0 ||
       strcmp(field, "on_pointer_enter") == 0) {
        snprintf(block->dom_on_mouse_enter, sizeof(block->dom_on_mouse_enter),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_mouse_leave") == 0 ||
       strcmp(field, "on_pointer_leave") == 0) {
        snprintf(block->dom_on_mouse_leave, sizeof(block->dom_on_mouse_leave),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_mouse_move") == 0 ||
       strcmp(field, "on_pointer_move") == 0) {
        snprintf(block->dom_on_mouse_move, sizeof(block->dom_on_mouse_move),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_mouse_down") == 0 ||
       strcmp(field, "on_pointer_down") == 0) {
        snprintf(block->dom_on_mouse_down, sizeof(block->dom_on_mouse_down),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_mouse_up") == 0 ||
       strcmp(field, "on_pointer_up") == 0) {
        snprintf(block->dom_on_mouse_up, sizeof(block->dom_on_mouse_up), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_wheel") == 0) {
        snprintf(block->dom_on_wheel, sizeof(block->dom_on_wheel), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_drag_start") == 0 ||
       strcmp(field, "on_dragstart") == 0) {
        snprintf(block->dom_on_drag_start, sizeof(block->dom_on_drag_start),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_drag_end") == 0 ||
       strcmp(field, "on_dragend") == 0) {
        snprintf(block->dom_on_drag_end, sizeof(block->dom_on_drag_end), "%s",
                 value);
        return 1;
    }
    if(strcmp(field, "on_drag_over") == 0 ||
       strcmp(field, "on_dragover") == 0) {
        snprintf(block->dom_on_drag_over, sizeof(block->dom_on_drag_over),
                 "%s", value);
        return 1;
    }
    if(strcmp(field, "on_drop") == 0) {
        snprintf(block->dom_on_drop, sizeof(block->dom_on_drop), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_copy") == 0) {
        snprintf(block->dom_on_copy, sizeof(block->dom_on_copy), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_cut") == 0) {
        snprintf(block->dom_on_cut, sizeof(block->dom_on_cut), "%s", value);
        return 1;
    }
    if(strcmp(field, "on_paste") == 0) {
        snprintf(block->dom_on_paste, sizeof(block->dom_on_paste), "%s",
                 value);
        return 1;
    }
    return 0;
}

static void
ui_block_apply_web_metadata(KirStmt *statement, const UiBlock *block)
{
    if(statement == NULL || block == NULL)
        return;
    snprintf(statement->node_name, sizeof(statement->node_name), "%s",
             block->name);
    snprintf(statement->node_key, sizeof(statement->node_key), "%s",
             block->name[0] != '\0' ? block->name : block->path);
    snprintf(statement->node_path, sizeof(statement->node_path), "%s",
             block->path);
    snprintf(statement->node_parent_path, sizeof(statement->node_parent_path),
             "%s", block->parent_path);
    snprintf(statement->dom_tag, sizeof(statement->dom_tag), "%s",
             block->dom_tag);
    snprintf(statement->dom_ref, sizeof(statement->dom_ref), "%s",
             block->dom_ref);
    snprintf(statement->dom_id, sizeof(statement->dom_id), "%s",
             block->dom_id);
    snprintf(statement->dom_name_attr, sizeof(statement->dom_name_attr), "%s",
             block->dom_name_attr);
    snprintf(statement->dom_value_attr, sizeof(statement->dom_value_attr), "%s",
             block->dom_value_attr);
    snprintf(statement->dom_class, sizeof(statement->dom_class), "%s",
             block->dom_class);
    snprintf(statement->dom_title, sizeof(statement->dom_title), "%s",
             block->dom_title);
    snprintf(statement->dom_href, sizeof(statement->dom_href), "%s",
             block->dom_href);
    snprintf(statement->dom_target, sizeof(statement->dom_target), "%s",
             block->dom_target);
    snprintf(statement->dom_rel, sizeof(statement->dom_rel), "%s",
             block->dom_rel);
    snprintf(statement->dom_for_attr, sizeof(statement->dom_for_attr), "%s",
             block->dom_for_attr);
    snprintf(statement->dom_part, sizeof(statement->dom_part), "%s",
             block->dom_part);
    snprintf(statement->dom_slot, sizeof(statement->dom_slot), "%s",
             block->dom_slot);
    snprintf(statement->dom_data_attrs, sizeof(statement->dom_data_attrs),
             "%s", block->dom_data_attrs);
    snprintf(statement->dom_extra_attrs, sizeof(statement->dom_extra_attrs),
             "%s", block->dom_extra_attrs);
    snprintf(statement->dom_placeholder, sizeof(statement->dom_placeholder),
             "%s", block->dom_placeholder);
    snprintf(statement->dom_input_type, sizeof(statement->dom_input_type), "%s",
             block->dom_input_type);
    snprintf(statement->dom_form_attr, sizeof(statement->dom_form_attr), "%s",
             block->dom_form_attr);
    snprintf(statement->dom_form_action, sizeof(statement->dom_form_action),
             "%s", block->dom_form_action);
    snprintf(statement->dom_form_method, sizeof(statement->dom_form_method),
             "%s", block->dom_form_method);
    snprintf(statement->dom_form_enctype, sizeof(statement->dom_form_enctype),
             "%s", block->dom_form_enctype);
    snprintf(statement->dom_autocomplete, sizeof(statement->dom_autocomplete),
             "%s", block->dom_autocomplete);
    snprintf(statement->dom_hidden, sizeof(statement->dom_hidden), "%s",
             block->dom_hidden);
    snprintf(statement->dom_draggable, sizeof(statement->dom_draggable), "%s",
             block->dom_draggable);
    snprintf(statement->dom_spellcheck, sizeof(statement->dom_spellcheck),
             "%s", block->dom_spellcheck);
    snprintf(statement->dom_contenteditable,
             sizeof(statement->dom_contenteditable), "%s",
             block->dom_contenteditable);
    snprintf(statement->dom_autofocus, sizeof(statement->dom_autofocus), "%s",
             block->dom_autofocus);
    snprintf(statement->dom_inert, sizeof(statement->dom_inert), "%s",
             block->dom_inert);
    snprintf(statement->dom_autocapitalize,
             sizeof(statement->dom_autocapitalize), "%s",
             block->dom_autocapitalize);
    snprintf(statement->dom_enterkeyhint,
             sizeof(statement->dom_enterkeyhint), "%s",
             block->dom_enterkeyhint);
    snprintf(statement->dom_download, sizeof(statement->dom_download), "%s",
             block->dom_download);
    snprintf(statement->dom_formnovalidate,
             sizeof(statement->dom_formnovalidate), "%s",
             block->dom_formnovalidate);
    snprintf(statement->dom_novalidate, sizeof(statement->dom_novalidate),
             "%s", block->dom_novalidate);
    snprintf(statement->dom_popover, sizeof(statement->dom_popover), "%s",
             block->dom_popover);
    snprintf(statement->dom_popover_target,
             sizeof(statement->dom_popover_target), "%s",
             block->dom_popover_target);
    snprintf(statement->dom_popover_target_action,
             sizeof(statement->dom_popover_target_action), "%s",
             block->dom_popover_target_action);
    snprintf(statement->dom_readonly, sizeof(statement->dom_readonly), "%s",
             block->dom_readonly);
    snprintf(statement->dom_required, sizeof(statement->dom_required), "%s",
             block->dom_required);
    snprintf(statement->dom_min, sizeof(statement->dom_min), "%s",
             block->dom_min);
    snprintf(statement->dom_max, sizeof(statement->dom_max), "%s",
             block->dom_max);
    snprintf(statement->dom_step, sizeof(statement->dom_step), "%s",
             block->dom_step);
    snprintf(statement->dom_minlength, sizeof(statement->dom_minlength), "%s",
             block->dom_minlength);
    snprintf(statement->dom_maxlength, sizeof(statement->dom_maxlength), "%s",
             block->dom_maxlength);
    snprintf(statement->dom_pattern, sizeof(statement->dom_pattern), "%s",
             block->dom_pattern);
    snprintf(statement->dom_accept, sizeof(statement->dom_accept), "%s",
             block->dom_accept);
    snprintf(statement->dom_multiple, sizeof(statement->dom_multiple), "%s",
             block->dom_multiple);
    snprintf(statement->dom_inputmode, sizeof(statement->dom_inputmode), "%s",
             block->dom_inputmode);
    snprintf(statement->dom_headers, sizeof(statement->dom_headers), "%s",
             block->dom_headers);
    snprintf(statement->dom_scope, sizeof(statement->dom_scope), "%s",
             block->dom_scope);
    snprintf(statement->dom_colspan, sizeof(statement->dom_colspan), "%s",
             block->dom_colspan);
    snprintf(statement->dom_rowspan, sizeof(statement->dom_rowspan), "%s",
             block->dom_rowspan);
    snprintf(statement->dom_tab_index, sizeof(statement->dom_tab_index), "%s",
             block->dom_tab_index);
    snprintf(statement->dom_role, sizeof(statement->dom_role), "%s",
             block->dom_role);
    snprintf(statement->dom_aria_label, sizeof(statement->dom_aria_label), "%s",
             block->dom_aria_label);
    snprintf(statement->dom_aria_description,
             sizeof(statement->dom_aria_description), "%s",
             block->dom_aria_description);
    snprintf(statement->dom_aria_describedby,
             sizeof(statement->dom_aria_describedby), "%s",
             block->dom_aria_describedby);
    snprintf(statement->dom_aria_labelledby,
             sizeof(statement->dom_aria_labelledby), "%s",
             block->dom_aria_labelledby);
    snprintf(statement->dom_aria_activedescendant,
             sizeof(statement->dom_aria_activedescendant), "%s",
             block->dom_aria_activedescendant);
    snprintf(statement->dom_aria_controls, sizeof(statement->dom_aria_controls),
             "%s", block->dom_aria_controls);
    snprintf(statement->dom_aria_owns, sizeof(statement->dom_aria_owns), "%s",
             block->dom_aria_owns);
    snprintf(statement->dom_aria_sort, sizeof(statement->dom_aria_sort), "%s",
             block->dom_aria_sort);
    snprintf(statement->dom_aria_orientation,
             sizeof(statement->dom_aria_orientation), "%s",
             block->dom_aria_orientation);
    snprintf(statement->dom_aria_level, sizeof(statement->dom_aria_level),
             "%s", block->dom_aria_level);
    snprintf(statement->dom_aria_posinset,
             sizeof(statement->dom_aria_posinset), "%s",
             block->dom_aria_posinset);
    snprintf(statement->dom_aria_setsize, sizeof(statement->dom_aria_setsize),
             "%s", block->dom_aria_setsize);
    snprintf(statement->dom_aria_haspopup,
             sizeof(statement->dom_aria_haspopup), "%s",
             block->dom_aria_haspopup);
    snprintf(statement->dom_aria_multiselectable,
             sizeof(statement->dom_aria_multiselectable), "%s",
             block->dom_aria_multiselectable);
    snprintf(statement->dom_aria_rowindex,
             sizeof(statement->dom_aria_rowindex), "%s",
             block->dom_aria_rowindex);
    snprintf(statement->dom_aria_colindex,
             sizeof(statement->dom_aria_colindex), "%s",
             block->dom_aria_colindex);
    snprintf(statement->dom_aria_rowcount,
             sizeof(statement->dom_aria_rowcount), "%s",
             block->dom_aria_rowcount);
    snprintf(statement->dom_aria_colcount,
             sizeof(statement->dom_aria_colcount), "%s",
             block->dom_aria_colcount);
    snprintf(statement->dom_aria_live, sizeof(statement->dom_aria_live), "%s",
             block->dom_aria_live);
    snprintf(statement->dom_aria_attrs, sizeof(statement->dom_aria_attrs),
             "%s", block->dom_aria_attrs);
    snprintf(statement->dom_on_click, sizeof(statement->dom_on_click), "%s",
             block->dom_on_click);
    snprintf(statement->dom_on_input, sizeof(statement->dom_on_input), "%s",
             block->dom_on_input);
    snprintf(statement->dom_on_before_input,
             sizeof(statement->dom_on_before_input), "%s",
             block->dom_on_before_input);
    snprintf(statement->dom_on_change, sizeof(statement->dom_on_change), "%s",
             block->dom_on_change);
    snprintf(statement->dom_on_select, sizeof(statement->dom_on_select), "%s",
             block->dom_on_select);
    snprintf(statement->dom_on_key, sizeof(statement->dom_on_key), "%s",
             block->dom_on_key);
    snprintf(statement->dom_on_invalid, sizeof(statement->dom_on_invalid),
             "%s", block->dom_on_invalid);
    snprintf(statement->dom_on_submit, sizeof(statement->dom_on_submit), "%s",
             block->dom_on_submit);
    snprintf(statement->dom_on_reset, sizeof(statement->dom_on_reset), "%s",
             block->dom_on_reset);
    snprintf(statement->dom_on_toggle, sizeof(statement->dom_on_toggle), "%s",
             block->dom_on_toggle);
    snprintf(statement->dom_on_close, sizeof(statement->dom_on_close), "%s",
             block->dom_on_close);
    snprintf(statement->dom_on_cancel, sizeof(statement->dom_on_cancel), "%s",
             block->dom_on_cancel);
    snprintf(statement->dom_on_focus, sizeof(statement->dom_on_focus), "%s",
             block->dom_on_focus);
    snprintf(statement->dom_on_blur, sizeof(statement->dom_on_blur), "%s",
             block->dom_on_blur);
    snprintf(statement->dom_on_scroll, sizeof(statement->dom_on_scroll), "%s",
             block->dom_on_scroll);
    snprintf(statement->dom_on_mouse_enter,
             sizeof(statement->dom_on_mouse_enter), "%s",
             block->dom_on_mouse_enter);
    snprintf(statement->dom_on_mouse_leave,
             sizeof(statement->dom_on_mouse_leave), "%s",
             block->dom_on_mouse_leave);
    snprintf(statement->dom_on_mouse_move,
             sizeof(statement->dom_on_mouse_move), "%s",
             block->dom_on_mouse_move);
    snprintf(statement->dom_on_mouse_down, sizeof(statement->dom_on_mouse_down),
             "%s", block->dom_on_mouse_down);
    snprintf(statement->dom_on_mouse_up, sizeof(statement->dom_on_mouse_up),
             "%s", block->dom_on_mouse_up);
    snprintf(statement->dom_on_wheel, sizeof(statement->dom_on_wheel), "%s",
             block->dom_on_wheel);
    snprintf(statement->dom_on_drag_start,
             sizeof(statement->dom_on_drag_start), "%s",
             block->dom_on_drag_start);
    snprintf(statement->dom_on_drag_end, sizeof(statement->dom_on_drag_end),
             "%s", block->dom_on_drag_end);
    snprintf(statement->dom_on_drag_over, sizeof(statement->dom_on_drag_over),
             "%s", block->dom_on_drag_over);
    snprintf(statement->dom_on_drop, sizeof(statement->dom_on_drop), "%s",
             block->dom_on_drop);
    snprintf(statement->dom_on_copy, sizeof(statement->dom_on_copy), "%s",
             block->dom_on_copy);
    snprintf(statement->dom_on_cut, sizeof(statement->dom_on_cut), "%s",
             block->dom_on_cut);
    snprintf(statement->dom_on_paste, sizeof(statement->dom_on_paste), "%s",
             block->dom_on_paste);
}

static void
ui_stmt_apply_source_metadata(KirStmt *statement, const KirFunction *fn,
                              UiBlock *parent, int *root_anonymous_count,
                              const char *widget, KirSourceSpan span)
{
    const char *parent_path;
    int ordinal = 1;

    if(statement == NULL)
        return;
    if(statement->node_parent_path[0] == '\0' && parent != NULL)
        snprintf(statement->node_parent_path,
                 sizeof(statement->node_parent_path), "%s",
                 parent->path);
    if(statement->node_path[0] != '\0')
        return;
    parent_path = statement->node_parent_path[0] != '\0'
                    ? statement->node_parent_path
                    : (fn != NULL ? fn->name : "ui");
    if(parent != NULL)
        ordinal = ++parent->anonymous_widget_count;
    else if(root_anonymous_count != NULL)
        ordinal = ++*root_anonymous_count;
    if(ordinal <= 1)
        snprintf(statement->node_path, sizeof(statement->node_path),
                 "%.3000s/%.700s@%d", parent_path, widget, span.line);
    else
        snprintf(statement->node_path, sizeof(statement->node_path),
                 "%.3000s/%.700s@%d-%d", parent_path, widget, span.line,
                 ordinal);
    if(statement->node_key[0] == '\0')
        snprintf(statement->node_key, sizeof(statement->node_key), "%s",
                 statement->node_path);
}

static void
ui_stmt_apply_expression_widget_metadata(KirStmt *statement,
                                         const KirFunction *fn,
                                         UiBlock *parent,
                                         int *root_anonymous_count,
                                         const char *raw,
                                         KirStmtKind kind,
                                         KirSourceSpan span)
{
    char expr[KIR_TEXT_MAX];
    char widget[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    char *eq;

    if(statement == NULL)
        return;
    snprintf(expr, sizeof(expr), "%s", raw);
    kir_trim_in_place(expr);
    if(kind == KIR_STMT_RETURN && starts_word(expr, "return")) {
        memmove(expr, expr + 6, strlen(expr + 6) + 1);
        kir_trim_in_place(expr);
    } else if(kind == KIR_STMT_IF) {
        kir_strip_block_brace(expr);
        if(starts_word(expr, "else if")) {
            memmove(expr, expr + 7, strlen(expr + 7) + 1);
            kir_trim_in_place(expr);
        } else if(starts_word(expr, "if")) {
            memmove(expr, expr + 2, strlen(expr + 2) + 1);
            kir_trim_in_place(expr);
        } else {
            return;
        }
    } else if(kind == KIR_STMT_DECL || kind == KIR_STMT_ASSIGN) {
        eq = strchr(expr, '=');
        if(eq == NULL || eq[1] == '=')
            return;
        memmove(expr, eq + 1, strlen(eq + 1) + 1);
        kir_trim_in_place(expr);
    } else {
        return;
    }
    if(parse_widget_statement(expr, widget, sizeof(widget), args, sizeof(args)))
        ui_stmt_apply_source_metadata(statement, fn, parent,
                                      root_anonymous_count, widget, span);
}

static void
ui_block_format(char *destination, size_t capacity, KirSourceSpan span,
                const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(destination, capacity, format, arguments);
    va_end(arguments);
    if(length < 0 || (size_t)length >= capacity)
        die("%s:%d: widget properties exceed the lowering size limit",
            span.path, span.line);
}

static void
ui_block_open(KirFunction *fn, UiBlock *block, KirSourceSpan span, int closing)
{
    char call[KIR_TEXT_MAX];
    char args[KIR_TEXT_MAX];
    const char *prop_type;
    KirSourceSpan source_span = block != NULL && block->span.path[0] != '\0'
                                  ? block->span
                                  : span;

    if(block == NULL || block->opened)
        return;
    if(strcmp(block->widget, "Scroll") == 0) {
        char bounds[KIR_TEXT_MAX];
        const char *height = (block->scope_fields & 2) ? block->scope_args[1] : "0";
        const char *offset = (block->scope_fields & 4) ? block->scope_args[2] : "nil";
        if(!(block->scope_fields & 1))
            die("%s:%d: Scroll requires 'bounds'", span.path, span.line);
        const char *source_bounds = kir_skip_ws(block->scope_args[0]);
        int n = snprintf(bounds, sizeof(bounds), "%s%s",
                         *source_bounds == '{' ? "(Rectangle)" : "", source_bounds);
        if(n < 0 || (size_t)n >= sizeof(bounds))
            die("%s:%d: Scroll bounds expression is too long", span.path, span.line);
        n = snprintf(call, sizeof(call), "%s%sBeginScroll(%s, %s, %s)",
                     block->name, block->name[0] ? ": Rectangle = " : "",
                     bounds, height, offset);
        if(n < 0 || (size_t)n >= sizeof(call))
            die("%s:%d: Scroll arguments are too long", span.path, span.line);
        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_OPEN, "{", "", source_span);
        KirFunctionAddStmt(fn, block->name[0] ? KIR_STMT_DECL : KIR_STMT_EXPR,
                           call, "", source_span);
        KirFunctionAddStmt(fn, KIR_STMT_DEFER, "defer EndScroll()", "",
                           source_span);
        block->opened = 1;
        return;
    }
    if(strcmp(block->widget, "TableCell") == 0) {
        const char *table = (block->scope_fields & 1) ? block->scope_args[0] : NULL;
        const char *row = (block->scope_fields & 2) ? block->scope_args[1] : NULL;
        const char *column = (block->scope_fields & 4) ? block->scope_args[2] : NULL;

        if(block->name[0] == '\0')
            die("%s:%d: TableCell requires a rectangle binding name", span.path, span.line);
        if(table == NULL || row == NULL || column == NULL)
            die("%s:%d: TableCell requires table, row and column", span.path, span.line);
        ui_block_format(call, sizeof(call), span,
                        "%s: Rectangle = BeginTableCell(%s, %s, %s)",
                        block->name, table, row, column);
        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_OPEN, "{", "", source_span);
        KirFunctionAddStmt(fn, KIR_STMT_DECL, call, "", source_span);
        KirFunctionAddStmt(fn, KIR_STMT_DEFER, "defer EndTableCell()", "",
                           source_span);
        block->opened = 1;
        return;
    }
    if(strcmp(block->widget, "Canvas") == 0) {
        char spec_name[KIR_NAME_MAX + 16];

        if(block->name[0] == '\0')
            die("%s:%d: Canvas requires a result binding name", span.path, span.line);
        if(!(block->scope_fields & 1))
            die("%s:%d: Canvas requires 'bounds'", span.path, span.line);
        ui_block_format(spec_name, sizeof(spec_name), span, "%s_spec", block->name);
        ui_block_format(args, sizeof(args), span, "(Canvas){%s}", block->props);
        ui_block_format(call, sizeof(call), span, "%s: Canvas = %s",
                        spec_name, args);
        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_OPEN, "{", "", source_span);
        KirFunctionAddStmt(fn, KIR_STMT_DECL, call, "", source_span);
        ui_block_format(call, sizeof(call), span,
                        "%s: CanvasResult = BeginCanvas(%s)",
                        block->name, spec_name);
        KirFunctionAddStmt(fn, KIR_STMT_DECL, call, "", source_span);
        ui_block_format(call, sizeof(call), span, "defer EndCanvas(%s)",
                        spec_name);
        KirFunctionAddStmt(fn, KIR_STMT_DEFER, call, "", source_span);
        block->opened = 1;
        return;
    }
    if(strcmp(block->widget, "Disabled") == 0) {
        const char *condition = block->prop_count ? block->props : "true";
        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_OPEN, "{", "", source_span);
        ui_block_format(call, sizeof(call), span, "BeginDisabled(%s)", condition);
        /* Scope conditions are boolean expressions, not legacy integer UI
         * widget arguments. Keep the ordinary typed call in the shared IR. */
        KirFunctionAddStmt(fn, KIR_STMT_EXPR, call, "", source_span);
        KirFunctionAddStmt(fn, KIR_STMT_DEFER, "defer EndDisabled()", "",
                           source_span);
        block->opened = 1;
        return;
    }
    if(strcmp(block->widget, "Popup") == 0) {
        ui_block_format(args, sizeof(args), span, "(PopupProps){%s}", block->props);
        ui_block_format(call, sizeof(call), span, "if Begin%s(%s) {",
                        block->widget, args);
        KirFunctionAddStmt(fn, KIR_STMT_IF, call, "", source_span);
        snprintf(call,sizeof(call),"defer End%s()",block->widget);
        KirFunctionAddStmt(fn, KIR_STMT_DEFER, call, "", source_span);
        block->opened = 1;
        return;
    }
    if(strcmp(block->widget, "Button") == 0 ||
       strcmp(block->widget, "Card") == 0) {
        int is_card = strcmp(block->widget, "Card") == 0;
        const char *constructor = closing
            ? block->widget
            : (is_card ? "BeginCard" : "BeginButton");
        const char *props_type = is_card ? "CardProps" : "ButtonProps";
        ui_block_format(args, sizeof(args), span, "(%s){%s}", props_type, block->props);
        ui_block_format(call, sizeof(call), span, "%s(%s)", constructor, args);
        KirStmt *statement = KirFunctionAddWidget(fn, constructor, args, call,
                                                  source_span);
        if(statement == NULL)
            die("out of memory parsing widget block");
        block->statement_index = (int)(statement - fn->stmts);
        ui_block_apply_web_metadata(statement, block);
        if(closing) {
            statement->declared_widget = 1;
            statement->widget_fallback = 1;
        }
        block->emits_end = !closing;
        block->opened = 1;
        return;
    }
    prop_type = ui_block_prop_type(block->widget);
    if(prop_type == NULL) {
        if(!closing)
            die("%s:%d: declared widget blocks do not yet accept child content: %s",
                span.path, span.line, block->widget);
        KirStmt *statement = KirFunctionAddWidget(fn, block->widget,
                                                  block->props, "",
                                                  source_span);
        if(statement == NULL)
            die("out of memory parsing declared widget");
        block->statement_index = (int)(statement - fn->stmts);
        ui_block_apply_web_metadata(statement, block);
        statement->declared_widget = 1;
        block->opened = 1;
        return;
    }
    if(!block->emits_end || block->has_key)
        ui_block_format(args, sizeof(args), span, "(%s){%s}", prop_type, block->props);
    else
        ui_block_format(args, sizeof(args), span,
                        "(%s){%s.key = Key(\"%s\")}",
                        prop_type, block->props, block->path);
    ui_block_format(call, sizeof(call), span, "%s(%s)", block->widget, args);
    KirStmt *statement = KirFunctionAddWidget(fn, block->widget, args, call,
                                              source_span);
    if(statement == NULL)
        die("out of memory parsing widget block");
    block->statement_index = (int)(statement - fn->stmts);
    ui_block_apply_web_metadata(statement, block);
    if(closing && !block->emits_end) {
        statement->declared_widget = 1;
        statement->widget_fallback = 1;
    }
    block->opened = 1;
}

static KirSourceSpan
ui_block_close_span(const UiBlock *block, const char *path, int line_no,
                    const char *line)
{
    KirSourceSpan start = block != NULL && block->span.path[0] != '\0'
                            ? block->span
                            : KirSpan(path, line_no, 1);
    int end_column = (int)strlen(line) + 1;

    return KirSpanEnd(start.path, start.line, start.column, line_no,
                      end_column > 0 ? end_column : 1);
}

static int
split_oneline_block(const char *t, char *head, size_t hsz,
                    char *body, size_t bsz)
{
    static const char *kws[] = { "if", "else", "while", "for", "switch",
                                 "case", "default", "guard", "do" };
    size_t n = strlen(t);
    size_t brace_pos = 0;
    int depth = 0;
    int in_str = 0;
    int in_chr = 0;
    char w0[16];
    size_t wl = 0;
    size_t i;

    if(n < 8 || t[n - 1] != '}')
        return 0;
    for(i = 0; t[i] != '\0' && (isalnum((unsigned char)t[i]) || t[i] == '_') &&
        wl + 1 < sizeof(w0); i++)
        w0[wl++] = t[i];
    w0[wl] = '\0';
    {
        int is_kw = 0;

        for(size_t k = 0; k < sizeof(kws) / sizeof(kws[0]); k++)
            if(strcmp(w0, kws[k]) == 0)
                is_kw = 1;
        if(!is_kw)
            return 0;
    }
    for(i = 0; i < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '(' || ch == '[') {
            depth++;
        } else if(ch == ')' || ch == ']') {
            depth--;
        } else if(ch == '{' && depth == 0) {
            /* first top-level '{' preceded by a space opens the block */
            if(i > 0 && t[i - 1] == ' ' && i + 2 < n && t[i + 1] == ' ' &&
                t[i + 2] != '}') {
                brace_pos = i;
                break;
            }
            return 0;   /* '{' used as expression on a control line */
        }
    }
    if(brace_pos == 0)
        return 0;
    /* no other top-level brace may appear before the trailing closer */
    depth = 0;
    in_str = in_chr = 0;
    for(i = brace_pos + 1; i + 1 < n; i++) {
        char ch = t[i];

        if(in_str) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '"')
                in_str = 0;
        } else if(in_chr) {
            if(ch == '\\' && i + 1 < n)
                i++;
            else if(ch == '\'')
                in_chr = 0;
        } else if(ch == '"') {
            in_str = 1;
        } else if(ch == '\'') {
            in_chr = 1;
        } else if(ch == '(' || ch == '[') {
            depth++;
        } else if(ch == ')' || ch == ']') {
            depth--;
        } else if((ch == '{' || ch == '}') && depth == 0) {
            return 0;
        }
    }
    snprintf(head, hsz, "%.*s", (int)(brace_pos + 1), t);
    snprintf(body, bsz, "%.*s", (int)(n - brace_pos - 3), t + brace_pos + 2);
    return 1;
}

static void
parse_state_field(KirModule *module, const char *path, int line_no, char *line)
{
    char *colon;
    char *eq;
    char *name;
    char *type;
    char *init;

    colon = strchr(line, ':');
    if(colon == NULL)
        return;
    *colon = '\0';
    name = kir_trim(line);
    type = kir_trim(colon + 1);
    init = "";
    eq = strchr(type, '=');
    if(eq != NULL) {
        *eq = '\0';
        init = kir_trim(eq + 1);
    }
    KirModuleAddStateField(module, name, kir_trim(type), init,
                           KirSpan(path, line_no, 1));
}

static void
parse_function_header(char *name, size_t name_size, char *args,
                      size_t args_size, char *ret, size_t ret_size,
                      const char *line)
{
    const char *p;
    const char *q;
    size_t n = 0;

    name[0] = '\0';
    args[0] = '\0';
    snprintf(ret, ret_size, "void");
    p = strstr(line, "::");
    if(p != NULL) {
        q = line;
        while(q < p && (*q == ' ' || *q == '\t'))
            q++;
        while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
              n + 1 < name_size)
            name[n++] = *q++;
        name[n] = '\0';
    }
    p = strchr(line, '(');
    q = p == NULL ? NULL : strchr(p, ')');
    if(p != NULL && q != NULL && q > p) {
        n = (size_t)(q - p - 1);
        if(n >= args_size)
            n = args_size - 1;
        memcpy(args, p + 1, n);
        args[n] = '\0';
        /* Return type: after the closing ')', an optional '-> T' before any
         * trailing directive (#extern / #global / ...). */
        q++;
        while(*q == ' ' || *q == '\t')
            q++;
        if(q[0] == '-' && q[1] == '>') {
            q += 2;
            while(*q == ' ' || *q == '\t')
                q++;
            n = 0;
            while(*q != '\0' && *q != '#' && *q != '{' && n + 1 < ret_size)
                ret[n++] = *q++;
            while(n > 0 && (ret[n - 1] == ' ' || ret[n - 1] == '\t'))
                n--;
            ret[n] = '\0';
        }
    }
}

static int
parse_import_line(KirModule *module, const char *path, int line_no,
                  const char *line)
{
    const char *directive;
    char target[K2KIR_PATH_MAX];
    char name[KIR_NAME_MAX];
    KirImportKind kind;
    int quoted;

    directive = strstr(line, "#import");
    if(directive == NULL)
        return 0;
    target[0] = '\0';
    name[0] = '\0';
    quoted = parse_quoted(directive, target, sizeof(target));
    if(!quoted && !parse_angled(directive, target, sizeof(target)))
        return 0;
    if(parse_symbol_before_colons(line, name, sizeof(name)))
        kind = KIR_IMPORT_MODULE;
    else {
        kir_copy(name, sizeof(name), target);
        kind = KIR_IMPORT_HEADER;
    }
    /* Signature records the bracket style so backends can keep angled
     * includes angled ("<") instead of quoted. required=0 marks '#private'
     * (include in the .c only, not the header). */
    KirModuleAddImport(module, kind, name, target, quoted ? "" : "<",
                       strstr(line, "#private") == NULL,
                       KirSpan(path, line_no, 1));
    return 1;
}

static int
parse_style_alias(const char *s, char *out, size_t out_size)
{
    const char *p = strstr(s, " as ");
    size_t n = 0;

    out[0] = '\0';
    if(p == NULL)
        return 1;
    p += 4;
    while(*p == ' ' || *p == '\t')
        p++;
    if(!isalpha((unsigned char)*p) && *p != '_')
        return 0;
    while((isalnum((unsigned char)*p) || *p == '_') && n + 1 < out_size)
        out[n++] = *p++;
    out[n] = '\0';
    while(*p == ' ' || *p == '\t' || *p == ';')
        p++;
    return out[0] != '\0' && *p == '\0';
}

static int
parse_style_line(KirModule *module, const char *path, int line_no,
                 const char *line)
{
    const char *directive;
    char target[K2KIR_PATH_MAX];
    char alias[KIR_NAME_MAX];
    int quoted;
    KirStyleImport *imp;

    directive = strstr(line, "#style");
    if(directive == NULL)
        return 0;
    target[0] = '\0';
    alias[0] = '\0';
    quoted = parse_quoted(directive, target, sizeof(target));
    if(!quoted && !parse_angled(directive, target, sizeof(target)))
        die("%s:%d: #style requires \"file.kss\" or <builtin.pack>",
            path, line_no);
    if(!parse_style_alias(directive, alias, sizeof(alias)))
        die("%s:%d: #style alias must be `as name`", path, line_no);
    imp = KirModuleAddStyleImport(module,
        quoted ? KIR_STYLE_IMPORT_FILE : KIR_STYLE_IMPORT_BUILTIN,
        target, alias, KirSpan(path, line_no, 1));
    if(imp == NULL)
        die("%s:%d: out of memory while recording #style", path, line_no);
    return 1;
}

static int
parse_extern_line(KirModule *module, const char *path, int line_no,
                  const char *line)
{
    char name[KIR_NAME_MAX];
    char target[KIR_PATH_MAX];
    char symbol[KIR_NAME_MAX];
    KirImport *imp;
    KirExternKind extern_kind;
    const char *intrinsic = strstr(line, "#intrinsic");

    if(intrinsic == NULL && strstr(line, "#extern") == NULL)
        return 0;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    const char *declaration = kir_skip_ws(strstr(line, "::") + 2);
    if(starts_word(declaration, "struct") || starts_word(declaration, "enum"))
        return 0;
    target[0] = '\0';
    symbol[0] = '\0';
    if(intrinsic != NULL) {
        /* 'name :: (args) -> int #intrinsic "web"' — lowered by k2c to a
         * static EM_ASM wrapper on web builds. Only the two known web
         * intrinsics exist. */
        const char *b = intrinsic + strlen("#intrinsic");
        char backend[KIR_NAME_MAX];
        char ret[KIR_NAME_MAX] = "";
        const char *arrow = strstr(line, "->");

        while(*b == ' ' || *b == '\t')
            b++;
        if(*b == '"') {
            size_t n = 0;

            b++;
            while(*b != '\0' && *b != '"' && n + 1 < sizeof(backend))
                backend[n++] = *b++;
            backend[n] = '\0';
        } else {
            snprintf(backend, sizeof(backend), "%s", b);
        }
        if(strcmp(backend, "web") != 0)
            die("%s:%d: unknown intrinsic backend '%s'", path, line_no,
                backend);
        if(arrow != NULL && arrow < intrinsic) {
            size_t n = 0;
            const char *r = arrow + 2;

            while(r < intrinsic && n + 1 < sizeof(ret)) {
                if(*r != ' ' && *r != '\t')
                    ret[n++] = *r;
                r++;
            }
            ret[n] = '\0';
        }
        if(strcmp(ret, "int") != 0)
            die("%s:%d: web intrinsic '%s' must return int", path, line_no,
                name);
        if(strcmp(name, "web_download_file") != 0 &&
            strcmp(name, "web_context_click_in_bounds") != 0)
            die("%s:%d: unknown web intrinsic '%s'", path, line_no, name);
    } else {
        const char *dir = strstr(line, "#extern");

        if(dir != NULL)
            parse_quoted(dir + 7, target, sizeof(target));
    }
    extern_kind = intrinsic != NULL
                      ? KIR_EXTERN_NONE
                      : classify_extern_target(target, symbol, sizeof(symbol),
                                               path, line_no);
    imp = KirModuleAddImport(module,
                             intrinsic != NULL ? KIR_IMPORT_INTRINSIC
                                               : KIR_IMPORT_EXTERN,
                             name, target[0] ? target : name, line, 1,
                             KirSpan(path, line_no, 1));
    if(imp != NULL) {
        char parsed_name[KIR_NAME_MAX];
        parse_function_header(parsed_name, sizeof(parsed_name), imp->args,
                              sizeof(imp->args), imp->return_type,
                              sizeof(imp->return_type), line);
        imp->extern_kind = extern_kind;
        snprintf(imp->extern_symbol, sizeof(imp->extern_symbol), "%s",
                 symbol);
    }
    return 1;
}

static int
paren_params_are_typed(const char *body)
{
    const char *inner = NULL;
    const char *end = NULL;
    const char *p;
    size_t depth = 0;

    /* Find the top-level parameter list and its matching close. */
    for(p = body; *p != '\0'; p++) {
        if(*p == '(') {
            if(depth == 0)
                inner = p + 1;
            depth++;
        } else if(*p == ')') {
            depth--;
            if(depth == 0) {
                end = p;
                break;
            }
        }
    }
    if(inner == NULL || end == NULL)
        return 0;
    while(inner < end && (*inner == ' ' || *inner == '\t'))
        inner++;
    if(inner >= end)
        return 1; /* () — empty parameter list */
    /* Jai-style rule: a binding like 'X :: (expr)' stays a constant unless
     * the parenthesized text is a typed parameter list. Parameter lists
     * name their arguments ('name: Type'); bare expressions — numbers,
     * arithmetic, ternaries, literals — do not. */
    if(*inner == '"' || *inner == '\'' || isdigit((unsigned char)*inner))
        return 0;
    if(strchr(inner, '?') != NULL)
        return 0;
    return strchr(inner, ':') != NULL;
}

static int
looks_like_function_header(const char *line)
{
    char tmp[K2KIR_LINE_MAX];
    char *p;
    char *body;

    p = strstr(line, "::");
    if(p == NULL)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", p + 2);
    body = kir_trim(tmp);
    if(starts_word(body, "#import") || starts_word(body, "#defined") ||
       starts_word(body, "#define") || starts_word(body, "struct") ||
       starts_word(body, "enum"))
        return 0;
    if(strstr(body, "#type") != NULL)
        return 0;
    if(body[0] != '(')
        return 0;
    /* A body or return type makes it a procedure regardless of params. */
    if(strchr(body, '{') != NULL || strstr(body, "->") != NULL)
        return 1;
    return paren_params_are_typed(body);
}


/* Inline bodies retain ordinary function parameter syntax, with an explicit
 * #slot marker separating them from record and array initializers. */
static int
parse_slot_header(const char *text, char *binding, size_t binding_size,
                   char *arguments, size_t arguments_size)
{
    const char *equals = strchr(text, '=');
    const char *annotation = strstr(text, "#slot");
    if(equals == NULL || annotation == NULL || equals >= annotation ||
       strcmp(kir_skip_ws(annotation + 5), "{") != 0 ||
       *kir_skip_ws(equals + 1) != '(')
        return 0;
    size_t length = (size_t)(equals - text);
    if(length >= binding_size)
        return 0;
    memcpy(binding, text, length);
    binding[length] = '\0';
    kir_trim_in_place(binding);
    char header[KIR_TEXT_MAX], name[KIR_NAME_MAX], result[KIR_NAME_MAX];
    int written = snprintf(header, sizeof(header), "slot_body :: %s", equals + 1);
    if(written < 0 || (size_t)written >= sizeof(header))
        return 0;
    parse_function_header(name, sizeof(name), arguments, arguments_size,
                          result, sizeof(result), header);
    return name[0] && !strcmp(result, "void");
}

/* ---- compile-time conditionals ------------------------------------------
 * '#if COND { ... } #else { ... }' regions use one model: top-level
 * captures inside a region are stamped with the expanded C preprocessor
 * condition and the emitter wraps each item in '#if cond / #endif'; the
 * condition's 'Name' constants ('WEB :: #defined(PLATFORM_WEB)') expand to
 * their expressions. Body-level regions lower to raw #if/#elif/#else/#endif
 * statements whose braces are consumed here. */

typedef struct {
    char names[16][KIR_NAME_MAX];
    char exprs[16][KIR_TEXT_MAX];
    int count;
} KirConsts;

typedef struct {
    char cond[KIR_TEXT_MAX];      /* active branch condition (C form) */
    char excluded[KIR_TEXT_MAX];  /* conditions handled by earlier branches */
    int braces;                   /* net '{' until the region's closing '}' */
} KirCondFrame;

static int
line_is_hash_else(const char *line)
{
    return strcmp(line, "} #else {") == 0 || strcmp(line, "#else {") == 0;
}

/* '#if COND {' / '#else_if COND {' (with optional leading '}'): strips the
 * trailing '{' — region braces are consumed, never emitted. Returns 1 for
 * '#if', 2 for '#else_if', 0 otherwise; *condition points into line. */
static int
parse_cond_start(char *line, char **condition)
{
    char *q = NULL;
    int kind = 0;
    size_t n;

    if(strncmp(line, "#if", 3) == 0 &&
       (line[3] == '\0' || isspace((unsigned char)line[3]))) {
        q = line + 3;
        kind = 1;
    } else if(strncmp(line, "#else_if", 8) == 0 &&
              (line[8] == '\0' || isspace((unsigned char)line[8]))) {
        q = line + 8;
        kind = 2;
    } else if(strncmp(line, "} #else_if", 10) == 0 &&
              (line[10] == '\0' || isspace((unsigned char)line[10]))) {
        q = line + 10;
        kind = 2;
    } else {
        return 0;
    }
    q = kir_trim(q);
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        return 0;
    q[n - 1] = '\0';
    q = kir_trim(q);
    if(q[0] == '\0')
        return 0;
    *condition = q;
    return kind;
}

static void
expand_compile_expr_depth(char *dst, size_t dst_size, const KirConsts *consts,
                          const char *src, int depth)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;
    int i;

    if(dst_size == 0)
        return;
    if(depth > 16) {
        dst[0] = '\0';
        return;
    }
    for(const char *p = src; p != NULL && *p != '\0' && n + 1 < dst_size;) {
        if(in_string) {
            dst[n++] = *p;
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            p++;
            continue;
        }
        if(*p == '"') {
            in_string = 1;
            dst[n++] = *p++;
            continue;
        }
        if(*p == '#' && strncmp(p, "#defined", 8) == 0) {
            const char *word_end = p + 8;

            if(*word_end == '\0' || *word_end == '(' ||
               isspace((unsigned char)*word_end)) {
                if(n + 7 >= dst_size)
                    break;
                memcpy(dst + n, "defined", 7);
                n += 7;
                p += 8;
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            char ident[KIR_NAME_MAX];
            size_t il = 0;
            int found = 0;

            while(isalnum((unsigned char)*p) || *p == '_') {
                if(il + 1 < sizeof(ident))
                    ident[il++] = *p;
                p++;
            }
            ident[il] = '\0';
            for(i = 0; i < consts->count; i++) {
                if(strcmp(consts->names[i], ident) == 0) {
                    char expanded[KIR_TEXT_MAX];
                    int written;

                    expand_compile_expr_depth(expanded, sizeof(expanded),
                                              consts, consts->exprs[i],
                                              depth + 1);
                    written = snprintf(dst + n, dst_size - n, "(%s)", expanded);
                    if(written < 0)
                        written = 0;
                    if((size_t)written >= dst_size - n)
                        n = dst_size - 1;
                    else
                        n += (size_t)written;
                    found = 1;
                    break;
                }
            }
            if(!found) {
                if(n + il >= dst_size)
                    break;
                memcpy(dst + n, ident, il);
                n += il;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

static void
expand_compile_expr(char *dst, size_t dst_size, const KirConsts *consts,
                    const char *src)
{
    expand_compile_expr_depth(dst, dst_size, consts, src, 0);
}

static char *
find_top_comma(char *s)
{
    int depth = 0;
    int in_string = 0;
    int in_char = 0;

    for(char *p = s; *p != '\0'; p++) {
        if(in_string) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '"')
                in_string = 0;
        } else if(in_char) {
            if(*p == '\\' && p[1] != '\0')
                p++;
            else if(*p == '\'')
                in_char = 0;
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '\'') {
            in_char = 1;
        } else if(*p == '(' || *p == '[' || *p == '{') {
            depth++;
        } else if(*p == ')' || *p == ']' || *p == '}') {
            if(depth > 0)
                depth--;
        } else if(*p == ',' && depth == 0) {
            return p;
        }
    }
    return NULL;
}

typedef struct KirEval {
    const char *p;
    int known;
    long value;
} KirEval;

static void
eval_skip(KirEval *ev)
{
    while(*ev->p == ' ' || *ev->p == '\t')
        ev->p++;
}

static long eval_or(KirEval *ev);

static long
eval_primary(KirEval *ev)
{
    char *end;
    long value;

    eval_skip(ev);
    if(*ev->p == '(') {
        ev->p++;
        value = eval_or(ev);
        eval_skip(ev);
        if(*ev->p == ')')
            ev->p++;
        else
            ev->known = 0;
        return value;
    }
    if(isdigit((unsigned char)*ev->p)) {
        value = strtol(ev->p, &end, 0);
        if(end == ev->p) {
            ev->known = 0;
            return 0;
        }
        ev->p = end;
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;   /* integer suffixes: U, L, UL */
        return value;
    }
    if(isalpha((unsigned char)*ev->p) || *ev->p == '_') {
        while(isalnum((unsigned char)*ev->p) || *ev->p == '_')
            ev->p++;
        ev->known = 0;
        return 0;
    }
    ev->known = 0;
    return 0;
}

static long
eval_unary(KirEval *ev)
{
    eval_skip(ev);
    if(*ev->p == '!') {
        long v;

        ev->p++;
        v = eval_unary(ev);
        return ev->known ? !v : 0;
    }
    if(*ev->p == '-') {
        ev->p++;
        return -eval_unary(ev);
    }
    if(*ev->p == '+') {
        ev->p++;
        return eval_unary(ev);
    }
    return eval_primary(ev);
}

static long
eval_mul(KirEval *ev)
{
    long left = eval_unary(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '*' && *ev->p != '/' && *ev->p != '%')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_unary(ev);
        if(!left_known || !ev->known || (right == 0 && op != '*')) {
            ev->known = 0;
            left = 0;
        } else if(op == '*') {
            left *= right;
        } else if(op == '/') {
            left /= right;
        } else {
            left %= right;
        }
    }
}

static long
eval_add(KirEval *ev)
{
    long left = eval_mul(ev);

    while(1) {
        char op;
        long right;
        int left_known;

        eval_skip(ev);
        if(*ev->p != '+' && *ev->p != '-')
            return left;
        op = *ev->p++;
        left_known = ev->known;
        right = eval_mul(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op == '+') {
            left += right;
        } else {
            left -= right;
        }
    }
}

static long
eval_rel(KirEval *ev)
{
    long left = eval_add(ev);

    while(1) {
        const char *op = NULL;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "<=", 2) == 0 || strncmp(ev->p, ">=", 2) == 0)
            op = ev->p, ev->p += 2;
        else if(*ev->p == '<' || *ev->p == '>')
            op = ev->p, ev->p++;
        else
            return left;
        left_known = ev->known;
        right = eval_add(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else if(op[0] == '<' && op[1] == '=') {
            left = left <= right;
        } else if(op[0] == '>' && op[1] == '=') {
            left = left >= right;
        } else if(op[0] == '<') {
            left = left < right;
        } else {
            left = left > right;
        }
    }
}

static long
eval_eq(KirEval *ev)
{
    long left = eval_rel(ev);

    while(1) {
        int neq = 0;
        long right;
        int left_known;

        eval_skip(ev);
        if(strncmp(ev->p, "==", 2) == 0) {
            ev->p += 2;
        } else if(strncmp(ev->p, "!=", 2) == 0) {
            ev->p += 2;
            neq = 1;
        } else {
            return left;
        }
        left_known = ev->known;
        right = eval_rel(ev);
        if(!left_known || !ev->known) {
            ev->known = 0;
            left = 0;
        } else {
            left = neq ? left != right : left == right;
        }
    }
}

static long
eval_and(KirEval *ev)
{
    long left = eval_eq(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "&&", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_eq(ev);
        if(left_known && !left_value) {
            ev->known = 1;
            left = 0;
        } else if(left_known && ev->known) {
            left = left_value && right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static long
eval_or(KirEval *ev)
{
    long left = eval_and(ev);

    while(1) {
        long right;
        int left_known;
        long left_value;

        eval_skip(ev);
        if(strncmp(ev->p, "||", 2) != 0)
            return left;
        ev->p += 2;
        left_known = ev->known;
        left_value = left;
        right = eval_and(ev);
        if(left_known && left_value) {
            ev->known = 1;
            left = 1;
        } else if(left_known && ev->known) {
            left = left_value || right;
        } else {
            ev->known = 0;
            left = 0;
        }
    }
}

static int
eval_const_condition(const char *src, long *value)
{
    KirEval ev;

    ev.p = src;
    ev.known = 1;
    ev.value = eval_or(&ev);
    eval_skip(&ev);
    if(*ev.p != '\0')
        ev.known = 0;
    if(value != NULL)
        *value = ev.value;
    return ev.known;
}

static int
parse_compile_check(KirModule *module, const char *path, int line_no,
                    char *line, const KirConsts *consts, const char *guard)
{
    char cond[KIR_TEXT_MAX];
    char msg[KIR_TEXT_MAX];
    KirAssert *a;

    if(strncmp(line, "#assert", 7) == 0 &&
       (line[7] == '\0' || isspace((unsigned char)line[7]))) {
        char *body = kir_trim(line + 7);
        char *comma;

        if(body[0] == '\0')
            die("%s:%d: #assert needs a condition", path, line_no);
        comma = find_top_comma(body);
        if(comma != NULL) {
            *comma = '\0';
            snprintf(msg, sizeof(msg), "%s", kir_trim(comma + 1));
            if(msg[0] == '\0')
                snprintf(msg, sizeof(msg), "\"Kry #assert failed\"");
        } else {
            snprintf(msg, sizeof(msg), "\"Kry #assert failed\"");
        }
        expand_compile_expr(cond, sizeof(cond), consts, kir_trim(body));
        {
            long value = 0;
            int known = eval_const_condition(cond, &value);

            if(guard[0] == '\0' && known && !value)
                die("%s:%d: #assert failed: %s", path, line_no, msg);
            a = KirModuleAddAssert(module, cond, msg, KirSpan(path, line_no, 1));
            if(a != NULL) {
                a->known = known;
                a->value = value != 0;
                snprintf(a->guard, sizeof(a->guard), "%s", guard);
            }
        }
        return 1;
    }
    if(strncmp(line, "#error", 6) == 0 &&
       (line[6] == '\0' || isspace((unsigned char)line[6]))) {
        char *body = kir_trim(line + 6);

        if(body[0] == '\0')
            die("%s:%d: #error needs a message", path, line_no);
        a = KirModuleAddAssert(module, "0", body, KirSpan(path, line_no, 1));
        if(a != NULL) {
            a->known = 1;
            a->value = 0;
            snprintf(a->guard, sizeof(a->guard), "%s", guard);
        }
        return 1;
    }
    return 0;
}

static void
text_append(char *dst, size_t dst_size, const char *part)
{
    size_t used;
    size_t remaining;

    if(dst == NULL || dst_size == 0 || part == NULL)
        return;
    used = strlen(dst);
    if(used >= dst_size - 1)
        return;
    remaining = dst_size - used - 1;
    if(strlen(part) < remaining)
        remaining = strlen(part);
    memcpy(dst + used, part, remaining);
    dst[used + remaining] = '\0';
}

static void
format_else_if_guard(char *dst, size_t dst_size, const char *excluded,
                     const char *expanded)
{
    kir_copy(dst, dst_size, "!(");
    text_append(dst, dst_size, excluded);
    text_append(dst, dst_size, ") && (");
    text_append(dst, dst_size, expanded);
    text_append(dst, dst_size, ")");
}

static void
format_else_guard(char *dst, size_t dst_size, const char *excluded)
{
    kir_copy(dst, dst_size, "!(");
    text_append(dst, dst_size, excluded);
    text_append(dst, dst_size, ")");
}

static void
format_excluded_guard(char *dst, size_t dst_size, const char *previous,
                      const char *expanded)
{
    kir_copy(dst, dst_size, "(");
    text_append(dst, dst_size, previous);
    text_append(dst, dst_size, ") || (");
    text_append(dst, dst_size, expanded);
    text_append(dst, dst_size, ")");
}

static void
format_preprocessor_cond(char *dst, size_t dst_size, const char *directive,
                         const char *expanded)
{
    kir_copy(dst, dst_size, directive);
    text_append(dst, dst_size, " ");
    text_append(dst, dst_size, expanded);
}

static void
combine_active_guard(char *dst, size_t dst_size, const KirCondFrame *frames,
                     int count)
{
    int i;

    dst[0] = '\0';
    for(i = 0; i < count; i++) {
        if(dst[0] == '\0')
            snprintf(dst, dst_size, "%s", frames[i].cond);
        else
            snprintf(dst + strlen(dst), dst_size - strlen(dst), " && %s",
                     frames[i].cond);
    }
}

/* Returns 1 when the line is consumed by top-level conditional handling
 * ('#if'/'#else'/'#else_if' open or retarget a frame; the matching '}'
 * pops one). A plain line inside a region only settles the frame's brace
 * count and returns 0, so normal captures proceed — stamped with the
 * active guard by the caller. */
static int
cond_top_step(char *line, KirCondFrame *frames, int *count, char *guard,
              size_t guard_size, const KirConsts *consts, const char *path,
              int line_no)
{
    char *cnd = NULL;
    int ck = parse_cond_start(line, &cnd);
    KirCondFrame *fr;

    if(ck == 1) {
        char expanded[KIR_TEXT_MAX];

        if(*count >= 8)
            die("%s:%d: too many nested #if blocks", path, line_no);
        expand_compile_expr(expanded, sizeof(expanded), consts, cnd);
        fr = &frames[(*count)++];
        kir_copy(fr->cond, sizeof(fr->cond), expanded);
        kir_copy(fr->excluded, sizeof(fr->excluded), expanded);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    if(*count <= 0)
        return 0;
    fr = &frames[*count - 1];
    if(ck == 2) {
        char expanded[KIR_TEXT_MAX];
        char next[KIR_TEXT_MAX * 2];

        expand_compile_expr(expanded, sizeof(expanded), consts, cnd);
        format_else_if_guard(fr->cond, sizeof(fr->cond), fr->excluded,
                             expanded);
        format_excluded_guard(next, sizeof(next), fr->excluded, expanded);
        kir_copy(fr->excluded, sizeof(fr->excluded), next);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    if(line_is_hash_else(line)) {
        format_else_guard(fr->cond, sizeof(fr->cond), fr->excluded);
        fr->braces = 1;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    fr->braces += net_block_braces(line);
    if(fr->braces <= 0) {
        (*count)--;
        combine_active_guard(guard, guard_size, frames, *count);
        return 1;
    }
    return 0;
}

/* A sub-mode (state/app/type/enum/function) consumed exactly one net '{'
 * from the enclosing region — settle the frame count. */
static void
cond_frame_settle(KirCondFrame *frames, int count)
{
    if(count > 0)
        frames[count - 1].braces--;
}

/* Strip C-style block comments in place, preserving newlines so line
 * numbers stay honest. *in_comment carries the state across lines (a
 * comment opened on one line keeps stripping on the next). String and
 * char literals are respected, and reset at each newline since Kry
 * literals never span lines. Without this, a comment close at end of
 * line trips the trailing-slash continuation rule and glues the comment
 * onto the next function header, silently dropping the function. */
static void
strip_block_comments(char *s, int *in_comment)
{
    char *w = s;
    char *r = s;
    int in_str = 0;
    int in_chr = 0;

    while(*r != '\0') {
        if(*in_comment) {
            while(*r != '\0' && !(*r == '*' && r[1] == '/')) {
                if(*r == '\n')
                    *w++ = '\n';
                r++;
            }
            if(*r != '\0') {
                r += 2;
                *in_comment = 0;
                *w++ = ' ';   /* keep tokens on either side apart */
            }
            continue;
        }
        if(in_str || in_chr) {
            if(*r == '\\' && r[1] != '\0') {
                *w++ = *r++;
                *w++ = *r++;
                continue;
            }
            if((in_str && *r == '"') || (in_chr && *r == '\''))
                in_str = in_chr = 0;
            else if(*r == '\n')
                in_str = in_chr = 0;
            *w++ = *r++;
        } else if(*r == '"') {
            in_str = 1;
            *w++ = *r++;
        } else if(*r == '\'') {
            in_chr = 1;
            *w++ = *r++;
        } else if(*r == '/' && r[1] == '*') {
            *in_comment = 1;
            r += 2;
        } else if(*r == '\n') {
            *w++ = *r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/* File and embedded declarations share the complete frontend. */
static char *
read_source_line(char *line, size_t size, FILE *file, const char **source)
{
    if(file != NULL)
        return fgets(line, (int)size, file);
    if(**source == '\0')
        return NULL;
    size_t length = 0;
    while(length + 1 < size && **source != '\0') {
        char next = *(*source)++;
        line[length++] = next;
        if(next == '\n')
            break;
    }
    line[length] = '\0';
    return line;
}

static KirProgram *
parse_source(const char *path, const char *root, FILE *in, const char *source)
{
    KirProgram *program;
    KirModule *module;
    KirFunction *fn = NULL;
    KirRoute *route = NULL;
    char line[K2KIR_LINE_MAX];
    char module_name[KIR_NAME_MAX] = "main";
    char rel[K2KIR_PATH_MAX];
    int line_no = 0;
    enum { TOP, APP, STATE, ROUTE, TYPE, ENUM, FUNCTION } mode = TOP;
    int enum_return = TOP;
    int depth = 0;
    char pending[K2KIR_LINE_MAX * 4];
    pending[0] = '\0';
    char lookahead[K2KIR_LINE_MAX];
    int have_look = 0;
    /* One-line control blocks ('if cond { body }') are split into header /
     * body / '}' logical lines; the body and closer re-enter the main loop
     * through this FIFO so they flow through the normal join machinery. */
    char onelineq[16][K2KIR_LINE_MAX * 2];
    int onelineq_count = 0;
    int from_queue = 0;
    int pending_len = 0;
    int pending_start_column = 1;
    int paren_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int expr_brace = 0;
    KirCondFrame tframes[8];
    int tframe_count = 0;
    KirConsts consts;
    char cur_guard[KIR_TEXT_MAX];
    int body_mdepth[8];
    int body_mcount = 0;
    int in_block_comment = 0;
    enum { UI_BLOCK_CAP = 64 };
    UiBlock *ui_blocks = calloc(UI_BLOCK_CAP, sizeof(*ui_blocks));
    int ui_block_count = 0;
    int root_anonymous_widget_count = 0;
    SlotParseFrame slot_frames[64];
    int slot_frame_count = 0;

    if(ui_blocks == NULL)
        die("out of memory");
    memset(&consts, 0, sizeof(consts));
    cur_guard[0] = '\0';
    snprintf(rel, sizeof(rel), "%s", relative_path(root, path));
    program = KirProgramNew();
    if(program == NULL)
        die("out of memory");

    module = KirProgramAddModule(program, module_name, rel, KirSpan(rel, 1, 1));
    if(module == NULL)
        die("out of memory");

    while(have_look || onelineq_count > 0 || read_source_line(line, sizeof(line), in, &source) != NULL) {
        char raw[K2KIR_LINE_MAX];
        char *t;

        /* Queued one-liner parts outrank the stashed lookahead: they belong
         * before the next source line, and have_look persists until the
         * queue drains. */
        from_queue = onelineq_count > 0;
        if(onelineq_count > 0) {
            kir_copy(line, sizeof(line), onelineq[0]);
            memmove(onelineq[0], onelineq[1],
                    sizeof(onelineq[0]) * (size_t)(onelineq_count - 1));
            onelineq_count--;
        } else if(have_look) {
            snprintf(line, sizeof(line), "%s", lookahead);
            have_look = 0;
        }

        line_no++;
        strip_block_comments(line, &in_block_comment);
        snprintf(raw, sizeof(raw), "%s", line);
        {
            char *trimmed = kir_trim(raw);
            int trimmed_column = from_queue ? 1 :
                                 source_column_for_trimmed(raw, trimmed);

            if(trimmed[0] == '\0' || strncmp(trimmed, "//", 2) == 0) {
                if(pending_len == 0)
                    continue;
                continue;
            }
            if(pending_len == 0)
                pending_start_column = trimmed_column;
            if(pending_len > 0 && pending_len + 2 < (int)sizeof(pending)) {
                pending[pending_len++] = ' ';
                pending[pending_len] = '\0';
            }
            strncat(pending, trimmed, sizeof(pending) - pending_len - 1);
            pending_len = (int)strlen(pending);
            pending_len = (int)strlen(pending);
            /* Decide whether braces at paren-depth 0 on this logical line are
             * block braces (control/headers open scopes) or expression braces
             * (compound literals / initializers continue the statement). */
            {
                int header_line = 0;
                char w0[16];
                size_t wl = 0;

                for(const char *w = pending;
                    *w != '\0' && (isalnum((unsigned char)*w) || *w == '_') &&
                    wl + 1 < sizeof(w0); w++)
                    w0[wl++] = *w;
                w0[wl] = '\0';
                /* Keyword headers must be followed by ' ', '(' or '{':
                 * 'app->x = ...' / 'state.x' are member statements, not
                 * block headers (their compound-literal braces are
                 * expression braces). */
                {
                    char nc = pending[wl];
                    char uiw[KIR_NAME_MAX];
                    char uin[KIR_NAME_MAX];
                    char slot_binding[KIR_TEXT_MAX], slot_arguments[KIR_TEXT_MAX];

                    header_line =
                        pending[0] == '#' ||
                        strcmp(pending, "{") == 0 ||   /* bare scope-open */
                        parse_slot_header(pending, slot_binding, sizeof(slot_binding),
                                          slot_arguments, sizeof(slot_arguments)) ||
                        parse_ui_block_header(pending, uiw, sizeof(uiw),
                                              uin, sizeof(uin)) ||
                        /* 'name :: Type = {' carries an initializer, not a
                         * body: its braces are expression braces so the
                         * logical line continues until they balance. Header
                         * forms ('name :: struct {', 'f :: (args) {', typedefs,
                         * externs) never contain ' = '. */
                        (strstr(pending, " :: ") != NULL &&
                         strstr(pending, " = ") == NULL) ||
                        (nc != '\0' && nc != '-' && nc != '.' &&
                         (strchr(" ({", nc) != NULL || nc == ':') &&
                         (strcmp(w0, "if") == 0 ||
                          strcmp(w0, "else") == 0 ||
                          strcmp(w0, "while") == 0 ||
                          strcmp(w0, "for") == 0 ||
                          strcmp(w0, "switch") == 0 ||
                          strcmp(w0, "do") == 0 ||
                          strcmp(w0, "case") == 0 ||
                          strcmp(w0, "default") == 0 ||
                          strcmp(w0, "struct") == 0 ||
                          strcmp(w0, "enum") == 0 ||
                          strcmp(w0, "state") == 0 ||
                          strcmp(w0, "route") == 0 ||
                          strcmp(w0, "app") == 0));
                }
                /* K&R "} else {" / "} else if (...) {": the leading '}' closes
                 * the if-body and the trailing '{' re-opens the else-body, so
                 * both braces are block braces even though the leading word
                 * extraction above saw only '}'. The same holds for chained
                 * regions: "} #else_if COND {" / "} #else {" / "} #if COND {"
                 * (parse_cond_start accepts the optional leading '}'), whose
                 * braces belong to the region, not the statement. */
                if(!header_line && pending[0] == '}') {
                    const char *eq = pending + 1;

                    while(*eq == ' ' || *eq == '\t')
                        eq++;
                    if(starts_word(eq, "else") ||
                       strncmp(eq, "#else", 5) == 0 ||
                       starts_word(eq, "#if ") ||
                       starts_word(eq, "#elif "))
                        header_line = 1;
                }
                {
                int in_chr = 0;

                for(const char *p = trimmed; *p != '\0'; p++) {
                    if(in_string) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '"')
                            in_string = 0;
                    } else if(in_chr) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '\'')
                            in_chr = 0;
                    } else if(*p == '"') {
                        in_string = 1;
                    } else if(*p == '\'') {
                        in_chr = 1;
                    } else if(*p == '(') {
                        paren_depth++;
                    } else if(*p == ')') {
                        paren_depth--;
                    } else if(*p == '[') {
                        bracket_depth++;
                    } else if(*p == ']') {
                        bracket_depth--;
                    } else if(paren_depth == 0 && bracket_depth == 0) {
                        if(*p == '{') {
                            if(!header_line)
                                expr_brace++;
                        } else if(*p == '}') {
                            if(!header_line && expr_brace > 0)
                                expr_brace--;
                        }
                    }
                }
                }
            }
            if(paren_depth > 0 || bracket_depth > 0 || in_string ||
               expr_brace > 0)
                continue;
            /* Continuation: a line ending in a binary operator or comma
             * continues onto the next. Exclude ++/-- (they end statements). */
            {
                size_t pl = (size_t)pending_len;
                char last;
                char prev;

                while(pl > 0 && (pending[pl - 1] == ' ' ||
                                 pending[pl - 1] == '\t'))
                    pl--;
                last = pl > 0 ? pending[pl - 1] : '\0';
                prev = pl > 1 ? pending[pl - 2] : '\0';
                /* Continuation operators: ',','=','%','/' always; '+','-','*',
                 * '<','>' only in binary position (prev is space — excludes
                 * 'char*','x++' handled below,'<stdlib.h>'); '&','|' when
                 * doubled ('&&','||') or space-preceded; ':' only with an
                 * open ternary ('?' pending) — 'case 1:' and goto labels
                 * ('fail:') end their statement. */
                if(last == ',' || last == '=' || last == '%' ||
                   last == '?' ||
                   (last == ':' && prev != ':' &&
                    strchr(pending, '?') != NULL) ||
                   (last == '/' && prev != '>'))
                    continue;
                if((last == '+' || last == '-' || last == '*' ||
                    last == '<' || last == '>') &&
                   (prev == ' ' || prev == '\t') &&
                   !(prev == last))
                    continue;
                if((last == '&' || last == '|') &&
                   (prev == last || prev == ' ' || prev == '\t'))
                    continue;
                /* Look ahead: a next line starting with a continuation
                 * token ('?' / ':' ternary branches, '.', ',', leading
                 * binary operators) continues this statement. Skipped for queued one-liner
                 * parts and while a stash is pending: their "next line" is
                 * not the next physical source line, and reading ahead here
                 * would overwrite/lose the stashed one. */
                if(!from_queue && !have_look) {
                    char la[K2KIR_LINE_MAX];
                    int pend_str;

                    /* C adjacent-literal concatenation: a statement whose
                     * last token closes a string ("...") continues when the
                     * next line opens a new literal ("...") — otherwise each
                     * fragment becomes its own orphan expression statement. */
                    {
                        int pl2 = (int)strlen(pending);

                        while(pl2 > 0 && (pending[pl2 - 1] == ' ' ||
                                          pending[pl2 - 1] == '\t'))
                            pl2--;
                        pend_str = pl2 > 0 && pending[pl2 - 1] == '"';
                    }

                    /* Keep consuming lookahead lines while they continue this
                     * statement; the first non-continuation line is stashed
                     * for the next iteration (appending it blindly here is
                     * how block-closing '}'s used to get swallowed). */
                    while(read_source_line(la, sizeof(la), in, &source) != NULL) {
                        const char *lt;
                        int cont;

                        strip_block_comments(la, &in_block_comment);
                        /* trim in place: the lookahead is appended verbatim,
                         * and a raw fgets line would carry its '\n' into the
                         * joined statement text. */
                        lt = kir_trim(la);
                        if(lt[0] == '\0') {
                            line_no++;   /* blank lookaheads still count */
                            continue;
                        }
                        cont =
                            *lt == '?' || (*lt == ':' && lt[1] != ':') ||
                            *lt == '.' || *lt == ',' || *lt == '+' ||
                            *lt == '/' || *lt == '%' ||
                            (*lt == '-' && lt[1] != '>') ||
                            ((*lt == '&' && lt[1] == '&') ||
                             (*lt == '|' && lt[1] == '|') ||
                             (*lt == '=' && lt[1] == '=') ||
                             (*lt == '!' && lt[1] == '=') ||
                             (*lt == '<' && lt[1] == '=') ||
                             (*lt == '>' && lt[1] == '=')) ||
                            (*lt == '"' && pend_str);
                        if(!cont) {
                            snprintf(lookahead, sizeof(lookahead), "%s", la);
                            have_look = 1;
                            break;
                        }
                        if(pending_len > 0 &&
                           pending_len + 2 < (int)sizeof(pending)) {
                            pending[pending_len++] = ' ';
                            pending[pending_len] = '\0';
                        }
                        strncat(pending, lt,
                                sizeof(pending) - pending_len - 1);
                        pending_len = (int)strlen(pending);
                        line_no++;
                        /* the joined statement now ends with whatever this
                         * fragment ended with */
                        {
                            int pl2 = pending_len;

                            while(pl2 > 0 && (pending[pl2 - 1] == ' ' ||
                                              pending[pl2 - 1] == '\t'))
                                pl2--;
                            pend_str = pl2 > 0 && pending[pl2 - 1] == '"';
                        }
                    }
                }
            }
        }
        t = pending;
        {
            static char logical[K2KIR_LINE_MAX * 4];

            snprintf(logical, sizeof(logical), "%s", pending);
            t = logical;
            /* One-line control block: keep the header as this logical line
             * and queue the body + closer for the next iterations (nested
             * one-liners split again when their body is finalized). */
            if(mode != TOP) {
                char head[K2KIR_LINE_MAX * 2];
                char body[K2KIR_LINE_MAX * 2];

                if(split_oneline_block(t, head, sizeof(head),
                                       body, sizeof(body))) {
                    if(onelineq_count + 2 <=
                       (int)(sizeof(onelineq) / sizeof(onelineq[0]))) {
                        snprintf(onelineq[onelineq_count++],
                                 sizeof(onelineq[0]), "%s", body);
                        snprintf(onelineq[onelineq_count++],
                                 sizeof(onelineq[0]), "}");
                        snprintf(logical, sizeof(logical), "%s", head);
                        t = logical;
                    }
                }
            }
        }
        pending[0] = '\0';
        pending_len = 0;
        if(mode == TOP &&
           cond_top_step(t, tframes, &tframe_count, cur_guard,
                         sizeof(cur_guard), &consts, rel, line_no)) {
            continue;
        } else if(mode == TOP &&
                  parse_compile_check(module, rel, line_no, t, &consts,
                                      cur_guard)) {
            continue;
        } else if(mode == TOP && t[0] == '#' &&
                  strncmp(t, "#if", 3) != 0 && strncmp(t, "#else", 5) != 0 &&
                  strncmp(t, "#endif", 6) != 0 &&
                  strncmp(t, "#defined", 8) != 0 &&
                  strncmp(t, "#enum", 5) != 0 &&
                  strncmp(t, "#module", 7) != 0 &&
                  strncmp(t, "#import", 7) != 0 &&
                  strncmp(t, "#style", 6) != 0) {
            /* plain # comment at top level — never a header; real
             * directives (#module/#import/#if...) fall through below */
        } else if(mode == TOP && strncmp(t, "#module", 7) == 0) {
            if(parse_quoted(t, module_name, sizeof(module_name)))
                snprintf(module->name, sizeof(module->name), "%s", module_name);
        } else if(mode == TOP && parse_style_line(module, rel, line_no, t)) {
            if(module->style_import_count > 0)
                snprintf(module->style_imports[module->style_import_count - 1].guard,
                         sizeof(module->style_imports[0].guard), "%s",
                         cur_guard);
            continue;
        } else if(mode == TOP &&
                  (parse_import_line(module, rel, line_no, t) ||
                   parse_extern_line(module, rel, line_no, t))) {
            if(module->import_count > 0)
                snprintf(module->imports[module->import_count - 1].guard,
                         sizeof(module->imports[0].guard), "%s", cur_guard);
            continue;
        } else if(mode == TOP && starts_word(t, "state") && strchr(t, '{') != NULL) {
            mode = STATE;
        } else if(mode == STATE) {
            if(t[0] == '}') {
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else {
                parse_state_field(module, rel, line_no, t);
                if(module->state_count > 0)
                    snprintf(module->state_fields[module->state_count - 1].guard,
                             sizeof(module->state_fields[0].guard), "%s",
                             cur_guard);
            }
        } else if(mode == TOP && starts_word(t, "route") &&
                  strchr(t, '{') != NULL) {
            char route_id[KIR_NAME_MAX];

            if(!parse_route_header(t, route_id, sizeof(route_id)))
                die("%s:%d: route block must be `route name {`", rel, line_no);
            route = KirModuleAddRoute(module, route_id, KirSpan(rel, line_no, 1));
            if(route != NULL)
                snprintf(route->guard, sizeof(route->guard), "%s", cur_guard);
            mode = ROUTE;
        } else if(mode == ROUTE) {
            if(t[0] == '}') {
                route = NULL;
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(route != NULL && starts_word(t, "title")) {
                parse_quoted(t, route->title, sizeof(route->title));
            } else if(route != NULL && starts_word(t, "group")) {
                parse_quoted(t, route->group, sizeof(route->group));
            } else if(route != NULL && starts_word(t, "page")) {
                char page[KIR_NAME_MAX];

                page[0] = '\0';
                sscanf(t, "page %127s", page);
                if(page[0] != '\0')
                    snprintf(route->page, sizeof(route->page), "%s", page);
            } else if(route != NULL && starts_word(t, "path")) {
                parse_quoted(t, route->path, sizeof(route->path));
            }
        } else if(mode == TOP && starts_word(t, "app") &&
                  strchr(t, '{') != NULL) {
            parse_quoted(t, module->app.title, sizeof(module->app.title));
            module->app.has_app = 1;
            module->app.width = 800;
            module->app.height = 600;
            module->app.fps = 60;
            mode = APP;
        } else if(mode == APP) {
            if(t[0] == '}') {
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(starts_word(t, "size")) {
                sscanf(t, "size %d %d",
                       &module->app.width, &module->app.height);
            } else if(starts_word(t, "fps")) {
                module->app.fps = atoi(t + 3);
            } else if(starts_word(t, "theme")) {
                char m2[32] = "";

                sscanf(t, "theme %127s %31s", module->app.theme, m2);
                module->app.dark_mode = strcmp(m2, "dark") == 0;
            } else if(starts_word(t, "font") && strstr(t, "examples")) {
                module->app.font_examples = 1;
            } else if(starts_word(t, "frame")) {
                die("%s:%d: app frame property is not supported; declare a #ui function",
                    rel, line_no);
            } else if(starts_word(t, "init")) {
                sscanf(t, "init %127s", module->app.init);
            } else if(starts_word(t, "scene")) {
                sscanf(t, "scene %127s", module->app.scene);
            } else if(starts_word(t, "shutdown")) {
                sscanf(t, "shutdown %127s", module->app.shutdown);
            }
        } else if(mode == TOP &&
                  (starts_word(t, "screen") || starts_word(t, "preview") ||
                   starts_word(t, "page") || starts_word(t, "frame") ||
                   starts_word(t, "fn"))) {
            die("%s:%d: declare UI with Name :: (...) #ui", rel, line_no);
        } else if(mode == TOP && looks_like_function_header(t)) {
            char name[KIR_NAME_MAX];
            char args[KIR_TEXT_MAX];
            char ret[KIR_NAME_MAX];
            int is_extern = strstr(t, "#extern") != NULL;
            int is_ui = strstr(t, "#ui") != NULL;
            int has_body = strchr(t, '{') != NULL;

            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            if(strstr(t, "#slot") != NULL) {
                if(has_body || is_extern || is_ui || strcmp(ret, "void") != 0)
                    die("%s:%d: slot declarations require a bodyless void signature", rel, line_no);
                KirType *slot = KirModuleAddType(module, name, KirSpan(rel, line_no, 1));
                slot->is_slot = 1;
                kir_copy(slot->body, sizeof(slot->body), args);
                kir_copy(slot->guard, sizeof(slot->guard), cur_guard);
            } else if(name[0] != '\0') {
                fn = KirModuleAddFunction(module, name, args, ret, 0,
                                          KirSpan(rel, line_no, 1));
                snprintf(fn->guard, sizeof(fn->guard), "%s", cur_guard);
                fn->is_extern = is_extern;
                /* '#extern "pkg.Fn"' — keep the quoted host symbol so the
                 * Go backend can bridge the call instead of emitting C. */
                if(is_extern) {
                    const char *dir = strstr(t, "#extern");

                    if(dir != NULL) {
                        const char *q = strchr(dir + 7, '"');

                        if(q != NULL) {
                            size_t n = 0;
                            const char *r = q + 1;

                            while(*r != '\0' && *r != '"' &&
                                  n + 1 < sizeof(fn->extern_target))
                                fn->extern_target[n++] = *r++;
                            fn->extern_target[n] = '\0';
                        }
                    }
                    fn->extern_kind = classify_extern_target(
                        fn->extern_target, fn->extern_symbol,
                        sizeof(fn->extern_symbol), rel, line_no);
                }
                fn->is_colon = strstr(t, "::") != NULL;
                fn->is_ui = is_ui;
                /* '#export' on a colon function keeps the plain Kry name as
                 * the C symbol so handwritten C and JNI entry points can call
                 * it directly. */
                fn->exported = fn->is_colon && strstr(t, "#export") != NULL;
                /* Public functions are emitted in headers; #ui functions are
                 * also project routes. */
                fn->is_public = !is_extern &&
                                strstr(t, "#private") == NULL &&
                                (is_ui || fn->is_colon);
                if(has_body && !is_extern) {
                    mode = FUNCTION;
                    depth = 1;
                    ui_block_count = 0;
                    root_anonymous_widget_count = 0;
                } else {
                    /* extern / body-less prototype: no body follows */
                    fn = NULL;
                }
            }
        } else if(mode == TOP && strncmp(t, "static ", 7) == 0 &&
                  strchr(t, ':') != NULL) {
            /* 'static name: T = init' — an internal-linkage global
             * (multi-line initializers arrive joined). */
            const char *rest = t + 7;
            const char *colon = strchr(rest, ':');
            const char *eq = strstr(rest, " = ");
            char gname[KIR_NAME_MAX];
            char gtype[KIR_TEXT_MAX];
            size_t nn = 0;

            while(rest < colon && (isalnum((unsigned char)*rest) ||
                   *rest == '_') && nn + 1 < sizeof(gname))
                gname[nn++] = *rest++;
            gname[nn] = '\0';
            nn = 0;
            {
                const char *ty = colon + 1;
                const char *end = eq != NULL ? eq : ty + strlen(ty);

                while(*ty == ' ' || *ty == '\t')
                    ty++;
                while(end > ty && (end[-1] == ' ' || end[-1] == '\t'))
                    end--;
                if((size_t)(end - ty) >= sizeof(gtype))
                    end = ty + sizeof(gtype) - 1;
                memcpy(gtype, ty, (size_t)(end - ty));
                gtype[end - ty] = '\0';
            }
            if(eq != NULL)
                KirModuleAddStatic(module, gname, gtype, eq + 3,
                                   KirSpan(rel, line_no, 1));
            else
                KirModuleAddStatic(module, gname, gtype, "",
                                   KirSpan(rel, line_no, 1));
            if(module->global_count > 0)
                snprintf(module->globals[module->global_count - 1].guard,
                         sizeof(module->globals[0].guard), "%s", cur_guard);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#global") != NULL) {
            /* name :: Type #global — a module-level global variable.
             * 'name :: Type = init #global' carries the initializer between
             * ' = ' and the trailing directives; both type and init end
             * there, not at '#global'. */
            char gname[KIR_NAME_MAX];
            char gtype[KIR_TEXT_MAX];
            char ginit[KIR_TEXT_MAX];
            const char *colon = strstr(t, "::");
            const char *ty = colon + 2;
            const char *hash = strstr(t, "#global");
            const char *eq = strstr(t, " = ");
            const char *tyend = (eq != NULL && eq < hash) ? eq : hash;
            size_t nn = 0;

            while(t < colon && (isalnum((unsigned char)*t) || *t == '_') &&
                   nn + 1 < sizeof(gname))
                gname[nn++] = *t++;
            gname[nn] = '\0';
            while(*ty == ' ' || *ty == '\t')
                ty++;
            nn = 0;
            while(ty < tyend && nn + 1 < sizeof(gtype))
                gtype[nn++] = *ty++;
            while(nn > 0 && (gtype[nn - 1] == ' ' || gtype[nn - 1] == '\t'))
                nn--;
            gtype[nn] = '\0';
            nn = 0;
            if(eq != NULL && eq < hash) {
                const char *ib = eq + 3;
                const char *ie = hash;

                while(ib < ie && nn + 1 < sizeof(ginit))
                    ginit[nn++] = *ib++;
                while(nn > 0 && (ginit[nn - 1] == ' ' ||
                                 ginit[nn - 1] == '\t'))
                    nn--;
            }
            ginit[nn] = '\0';
            if(strstr(t, "#private") != NULL)
                KirModuleAddStatic(module, gname, gtype, ginit,
                                   KirSpan(rel, line_no, 1));
            else
                KirModuleAddGlobal(module, gname, gtype, ginit,
                                   KirSpan(rel, line_no, 1));
            if(module->global_count > 0)
                snprintf(module->globals[module->global_count - 1].guard,
                         sizeof(module->globals[0].guard), "%s", cur_guard);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#type") != NULL) {
            /* 'Name :: C-type #type' — a typedef. Build the C declarator:
             * function-pointer types insert the name after '(*'; others
             * append ' NAME'. Must precede the struct catch-all below. */
            KirType *tty;
            const char *colons0 = strstr(t, "::");
            const char *tybegin = colons0 + 2;
            const char *hash = strstr(t, "#type");
            char tname[KIR_NAME_MAX];
            size_t tn = 0;
            const char *q = t;

            while(q < colons0 && (isalnum((unsigned char)*q) || *q == '_') &&
                  tn + 1 < sizeof(tname))
                tname[tn++] = *q++;
            tname[tn] = '\0';
            tty = KirModuleAddType(module, "#typedef",
                                   KirSpan(rel, line_no, 1));
            if(tty != NULL)
                snprintf(tty->guard, sizeof(tty->guard), "%s", cur_guard);
            if(tty != NULL && tname[0] != '\0') {
                char tytext[KIR_TEXT_MAX];
                size_t tl;
                const char *lp;

                while(tybegin < hash && (*tybegin == ' ' || *tybegin == '\t'))
                    tybegin++;
                tl = (size_t)(hash - tybegin);
                while(tl > 0 && (tybegin[tl - 1] == ' ' || tybegin[tl - 1] == '\t'))
                    tl--;
                if(tl >= sizeof(tytext))
                    tl = sizeof(tytext) - 1;
                memcpy(tytext, tybegin, tl);
                tytext[tl] = '\0';
                lp = strstr(tytext, "(*");
                if(lp != NULL) {
                    size_t off = (size_t)(lp - tytext) + 2;

                    snprintf(tty->body, sizeof(tty->body), "%.*s%s%s",
                             (int)off, tytext, tname, tytext + off);
                } else {
                    snprintf(tty->body, sizeof(tty->body), "%s %s",
                             tytext, tname);
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strchr(t, '{') == NULL &&
                  !looks_like_function_header(t)) {
            /* 'Name :: expr' is the single Kry constant declaration. Ordinary
             * constants are also emitted into generated interfaces so public
             * types can use them in array bounds. Platform predicates remain
             * frontend-only because #defined is not a C expression. */
            const char *colons = strstr(t, "::");
            const char *expr = colons + 2;
            char cname[KIR_NAME_MAX];
            size_t cn = 0;
            const char *q = t;

            while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                  cn + 1 < sizeof(cname))
                cname[cn++] = *q++;
            cname[cn] = '\0';
            while(*expr == ' ' || *expr == '\t')
                expr++;
            if(cname[0] != '\0' && *expr != '\0') {
                if(starts_word(expr, "#define"))
                    die("%s:%d: use '%s :: value'; #define is not Kry syntax",
                        rel, line_no, cname);
                else {
                    char run_value[KIR_TEXT_MAX];
                    KirDefine *def;

                    if(consts.count >= 16)
                        die("%s:%d: too many compile-time constants",
                            rel, line_no);
                    if(starts_word(expr, "#run")) {
                        char expanded[KIR_TEXT_MAX];
                        long value = 0;

                        expand_compile_expr(expanded, sizeof(expanded),
                                            &consts, kir_trim((char *)(expr + 4)));
                        if(!eval_const_condition(expanded, &value))
                            die("%s:%d: #run expression is not a constant: %s",
                                rel, line_no, expanded);
                        snprintf(run_value, sizeof(run_value), "%ld", value);
                        expr = run_value;
                    }
                    if(!starts_word(expr, "#defined") &&
                       !is_identifier_text(expr)) {
                        def = KirModuleAddDefine(module, cname, expr,
                                                 KirSpan(rel, line_no, 1));
                        if(def != NULL)
                            snprintf(def->guard, sizeof(def->guard), "%s",
                                     cur_guard);
                    }
                    snprintf(consts.names[consts.count],
                             sizeof(consts.names[0]), "%s", cname);
                    snprintf(consts.exprs[consts.count],
                             sizeof(consts.exprs[0]), "%s", expr);
                    consts.count++;
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL) {
            /* Name :: struct { ... } | Name :: enum { ... } — capture the
             * type body verbatim (enums emit as typedef enum). */
            const char *colons = strstr(t, "::");
            const char *after = colons + 2;
            char tname[KIR_NAME_MAX];
            size_t tn = 0;

            while(after < after + strlen(after) &&
                  (*after == ' ' || *after == '\t'))
                after++;
            if((strncmp(after, "struct", 6) == 0 &&
                (after[6] == '\0' || after[6] == ' ' || after[6] == '{')) ||
               (strncmp(after, "enum", 4) == 0 &&
                (after[4] == '\0' || after[4] == ' ' || after[4] == '{'))) {
                const char *q = t;
                KirType *ty;

                while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                      tn + 1 < sizeof(tname))
                    tname[tn++] = *q++;
                tname[tn] = '\0';
                ty = KirModuleAddType(module, tname,
                                      KirSpan(rel, line_no, 1));
                if(ty != NULL) {
                    ty->is_enum = strncmp(after, "enum", 4) == 0;
                    ty->is_extern = strstr(after, "#extern") != NULL;
                    if(ty->is_enum && ty->is_extern)
                        die("%s:%d: #extern type contracts require a struct", rel, line_no);
                    snprintf(ty->guard, sizeof(ty->guard), "%s", cur_guard);
                    mode = TYPE;
                }
                fn = NULL;
            }
        } else if((mode == TOP || mode == TYPE) &&
                  strncmp(t, "#enum", 5) == 0) {
            /* #enum { ... } — capture the constants as a type body. */
            KirType *ety = KirModuleAddType(module, "#enum",
                                            KirSpan(rel, line_no, 1));

            if(ety != NULL)
                snprintf(ety->guard, sizeof(ety->guard), "%s", cur_guard);
            (void)ety;
            enum_return = mode;
            if(strchr(t, '}') == NULL)
                mode = ENUM;
        } else if(mode == ENUM) {
            size_t tl = strlen(t);

            if(t[0] == '}') {
                mode = enum_return;
                cond_frame_settle(tframes, tframe_count);
            } else if(tl > 0 && t[tl - 1] == '}') {
                /* joined constants + closing brace on one line */
                KirType *ety = &module->types[module->type_count - 1];
                size_t used = strlen(ety->body);

                snprintf(ety->body + used, sizeof(ety->body) - used,
                         "%.*s\n", (int)(tl - 1), t);
                mode = enum_return;
                cond_frame_settle(tframes, tframe_count);
            } else {
                KirType *ety = &module->types[module->type_count - 1];
                size_t used = strlen(ety->body);

                snprintf(ety->body + used, sizeof(ety->body) - used,
                         "%s\n", t);
            }
        } else if(mode == TYPE) {
            if(t[0] == '}') {
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(t[0] == '#') {
                /* comment inside a struct body — skip */
            } else {
                KirType *ty = &module->types[module->type_count - 1];
                size_t used = strlen(ty->body);

                snprintf(ty->body + used, sizeof(ty->body) - used, "%s\n", t);
            }
        } else if(mode == FUNCTION) {
            char *bcnd = NULL;
            int bck = parse_cond_start(t, &bcnd);

            if(bck != 0 || line_is_hash_else(t)) {
                /* body-level '#if COND {' — the braces are consumed here;
                 * the region lowers to raw #if/#elif/#else/#endif lines.
                 * (bck was computed once above: parse_cond_start strips the
                 * trailing '{' in place, so re-parsing would misfire.) */
                char raw[KIR_TEXT_MAX];
                char expanded[KIR_TEXT_MAX];

                if(line_is_hash_else(t)) {
                    snprintf(raw, sizeof(raw), "#else");
                } else if(bck == 1) {
                    if(body_mcount >= 8)
                        die("%s:%d: too many nested #if blocks", rel, line_no);
                    body_mdepth[body_mcount++] = depth;
                    expand_compile_expr(expanded, sizeof(expanded), &consts,
                                        bcnd);
                    format_preprocessor_cond(raw, sizeof(raw), "#if",
                                             expanded);
                } else {
                    expand_compile_expr(expanded, sizeof(expanded), &consts,
                                        bcnd);
                    format_preprocessor_cond(raw, sizeof(raw), "#elif",
                                             expanded);
                }
                KirFunctionAddStmt(fn, KIR_STMT_RAW, raw, "",
                                   KirSpan(rel, line_no, 1));
            } else if(t[0] == '}' && body_mcount > 0 &&
                      depth == body_mdepth[body_mcount - 1]) {
                /* this '}' closes a body-level '#if' region, not a block */
                body_mcount--;
                KirFunctionAddStmt(fn, KIR_STMT_RAW, "#endif", "",
                                   KirSpan(rel, line_no, 1));
            } else if(t[0] == '#') {
                /* comment inside a body — skip (directives are top-level) */
            } else if(t[0] == '}' && slot_frame_count > 0 && depth == 1 && ui_block_count == 0) {
                if(*kir_skip_ws(t + 1))
                    die("%s:%d: slot body closing brace must be on its own line", rel, line_no);
                SlotParseFrame *frame = &slot_frames[--slot_frame_count];
                fn = &module->functions[frame->function_index];
                depth = frame->depth;
                root_anonymous_widget_count = frame->root_anonymous_count;
                ui_block_count = frame->block_count;
                if(ui_block_count)
                    memcpy(ui_blocks, frame->blocks, (size_t)ui_block_count * sizeof(*ui_blocks));
                free(frame->blocks);
                frame->blocks = NULL;
                body_mcount = frame->body_count;
                memcpy(body_mdepth, frame->body_depth, sizeof(body_mdepth));
            } else if(t[0] == '}' && ui_block_count > 0 &&
                      depth == ui_blocks[ui_block_count - 1].close_depth) {
                UiBlock *block = &ui_blocks[ui_block_count - 1];
                KirSourceSpan block_span = ui_block_close_span(block, rel,
                                                               line_no, t);

                ui_block_open(fn, block, block_span, 1);
                if(block->statement_index >= 0 &&
                   block->statement_index < fn->stmt_count)
                    fn->stmts[block->statement_index].span = block_span;
                if(strcmp(block->widget, "Disabled") == 0 ||
                   strcmp(block->widget, "Scroll") == 0 ||
                   strcmp(block->widget, "TableCell") == 0 ||
                   strcmp(block->widget, "Canvas") == 0 ||
                   strcmp(block->widget, "Popup") == 0)
                    KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, "}", "",
                                       KirSpan(rel, line_no, 1));
                else if(block->emits_end)
                    KirFunctionAddWidget(fn, "End", "", "End()",
                                         KirSpan(rel, line_no, 1));
                for(int field = 0; field < 3; field++)
                    free(block->scope_args[field]);
                ui_block_count--;
                if(depth > 0)
                    depth--;
            } else if(t[0] == '}') {
                /* K&R "} else {" / "} else if (...) {": the brace closes the
                 * if-body and the else re-opens a new one, so depth is net
                 * unchanged. Recorded as BLOCK_CLOSE plus an IF whose text
                 * starts with "else" — the k2c lowering emits the "} else"
                 * itself and suppresses the duplicate close. */
                const char *eq = t + 1;

                while(*eq == ' ' || *eq == '\t')
                    eq++;
                if(depth > 1 && starts_word(eq, "else")) {
                    KirSourceSpan span = KirSpan(rel, line_no,
                                                 pending_start_column +
                                                 (int)(eq - t));
                    KirStmt *st;

                    KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, "}", "",
                                       KirSpan(rel, line_no, 1));
                    st = KirFunctionAddStmt(fn, KIR_STMT_IF, eq, "", span);
                    ui_stmt_apply_expression_widget_metadata(st, fn,
                        ui_block_count > 0 ? &ui_blocks[ui_block_count - 1] : NULL,
                        &root_anonymous_widget_count,
                        eq, KIR_STMT_IF, span);
                } else {
                    if(depth > 0)
                        depth--;
                    if(depth == 0) {
                        mode = TOP;
                        cond_frame_settle(tframes, tframe_count);
                        fn = NULL;
                        ui_block_count = 0;
                    } else {
                        KirFunctionAddStmt(fn, KIR_STMT_BLOCK_CLOSE, t, "",
                                           KirSpan(rel, line_no, 1));
                    }
                }
            } else {
                KirStmtKind kind = classify_stmt(t);
                char widget[KIR_NAME_MAX] = "";
                char widget_args[KIR_TEXT_MAX] = "";
                int brace_delta = net_block_braces(t);
                char block_widget[KIR_NAME_MAX];
                char block_name[KIR_NAME_MAX];
                char prop_field[KIR_NAME_MAX];
                char prop_value[KIR_TEXT_MAX];
                char prop_line[KIR_TEXT_MAX];

                char slot_binding[KIR_TEXT_MAX], slot_arguments[KIR_TEXT_MAX];
                if(parse_slot_header(t, slot_binding, sizeof(slot_binding),
                                     slot_arguments, sizeof(slot_arguments))) {
                    if(slot_frame_count == 64)
                        die("%s:%d: too many nested slot bodies", rel, line_no);
                    SlotParseFrame *frame = &slot_frames[slot_frame_count++];
                    frame->function_index = (int)(fn - module->functions);
                    frame->depth = depth;
                    frame->root_anonymous_count = root_anonymous_widget_count;
                    frame->body_count = body_mcount;
                    memcpy(frame->body_depth, body_mdepth, sizeof(body_mdepth));
                    char name[KIR_NAME_MAX];
                    snprintf(name, sizeof(name), "slot_body_%d_%d", module->function_count, line_no);
                    KirSourceSpan span = KirSpan(rel, line_no, 1);
                    if(ui_block_count > 0 && !ui_blocks[ui_block_count - 1].opened &&
                       depth == ui_blocks[ui_block_count - 1].close_depth &&
                       is_identifier_text(slot_binding)) {
                        ui_block_append_prop(&ui_blocks[ui_block_count - 1], slot_binding, name, span);
                    } else {
                        char initializer[KIR_TEXT_MAX];
                        ui_block_format(initializer, sizeof(initializer), span, "%s = %s", slot_binding, name);
                        KirFunctionAddStmt(fn, strchr(slot_binding, ':') ? KIR_STMT_DECL : KIR_STMT_ASSIGN,
                                           initializer, "", span);
                    }
                    frame->block_count = ui_block_count;
                    frame->blocks = ui_block_count ? malloc((size_t)ui_block_count * sizeof(*ui_blocks)) : NULL;
                    if(ui_block_count && frame->blocks == NULL)
                        die("out of memory parsing slot body");
                    if(ui_block_count)
                        memcpy(frame->blocks, ui_blocks, (size_t)ui_block_count * sizeof(*ui_blocks));
                    fn = KirModuleAddFunction(module, name, slot_arguments, "void", 0, span);
                    fn->is_closure = 1;
                    kir_copy(fn->guard, sizeof(fn->guard), cur_guard);
                    depth = 1;
                    root_anonymous_widget_count = 0;
                    ui_block_count = body_mcount = 0;
                    continue;
                }
                if(parse_ui_block_header(t, block_widget,
                                         sizeof(block_widget),
                                         block_name, sizeof(block_name))) {
                    UiBlock *block;

                    if(ui_block_count > 0)
                        ui_block_open(fn, &ui_blocks[ui_block_count - 1],
                                      KirSpan(rel, line_no,
                                              pending_start_column), 0);
                    if(ui_block_count >= UI_BLOCK_CAP)
                        die("%s:%d: too many nested UI blocks", rel, line_no);
                    block = &ui_blocks[ui_block_count++];
                    memset(block, 0, sizeof(*block));
                    block->statement_index = -1;
                    block->span = KirSpanEnd(rel, line_no,
                                             pending_start_column, line_no,
                                             pending_start_column +
                                             (int)strlen(t));
                    snprintf(block->widget, sizeof(block->widget), "%s",
                             block_widget);
                    snprintf(block->name, sizeof(block->name), "%s",
                             block_name);
                    block->emits_end = is_layout_widget(block_widget);
                    if(ui_block_count > 1)
                    {
                        char parent_path[KIR_TEXT_MAX];

                        snprintf(parent_path, sizeof(parent_path), "%s",
                                 ui_blocks[ui_block_count - 2].path);
                        snprintf(block->parent_path, sizeof(block->parent_path),
                                 "%s", parent_path);
                        snprintf(block->path, sizeof(block->path), "%.3000s/%.900s",
                                 parent_path,
                                 block_name[0] != '\0' ? block_name : block_widget);
                    }
                    else {
                        block->parent_path[0] = '\0';
                        snprintf(block->path, sizeof(block->path), "%.3000s/%.900s",
                                 fn != NULL ? fn->name : "ui",
                                 block_name[0] != '\0' ? block_name : block_widget);
                    }
                    block->close_depth = depth + 1;
                    depth++;
                    continue;
                }

                kir_copy(prop_line, sizeof(prop_line), t);
                if(ui_block_count > 0 &&
                   !ui_blocks[ui_block_count - 1].opened &&
                   depth == ui_blocks[ui_block_count - 1].close_depth &&
                   parse_ui_prop_line(prop_line, prop_field,
                                      sizeof(prop_field), prop_value,
                                      sizeof(prop_value))) {
                    UiBlock *block = &ui_blocks[ui_block_count - 1];
                    if(strcmp(block->widget, "Scroll") == 0 ||
                       strcmp(block->widget, "TableCell") == 0 ||
                       strcmp(block->widget, "Canvas") == 0) {
                        int field;

                        if(strcmp(block->widget, "Scroll") == 0)
                            field = strcmp(prop_field, "bounds") == 0 ? 0 :
                                    strcmp(prop_field, "content_height") == 0 ? 1 :
                                    strcmp(prop_field, "scroll_offset") == 0 ? 2 : -1;
                        else if(strcmp(block->widget, "TableCell") == 0)
                            field = strcmp(prop_field, "table") == 0 ? 0 :
                                    strcmp(prop_field, "row") == 0 ? 1 :
                                    strcmp(prop_field, "column") == 0 ? 2 : -1;
                        else
                            field = strcmp(prop_field, "bounds") == 0 ? 0 :
                                    strcmp(prop_field, "scroll_x") == 0 ? 1 :
                                    strcmp(prop_field, "scroll_y") == 0 ? 2 :
                                    strcmp(prop_field, "zoom") == 0 ? 3 : -1;
                        if(field < 0)
                            die("%s:%d: %s accepts only %s", rel, line_no,
                                block->widget,
                                strcmp(block->widget, "Scroll") == 0
                                    ? "bounds, content_height and scroll_offset"
                                    : strcmp(block->widget, "TableCell") == 0
                                          ? "table, row and column"
                                          : "bounds, scroll_x, scroll_y and zoom");
                        if(block->scope_fields & (1u << field))
                            die("%s:%d: duplicate %s '%s' property",
                                rel, line_no, block->widget, prop_field);
                        if(strcmp(block->widget, "Canvas") == 0)
                            ui_block_append_prop(block, prop_field, prop_value,
                                                 KirSpan(rel, line_no, 1));
                        else {
                            block->scope_args[field] = malloc(strlen(prop_value) + 1);
                            if(block->scope_args[field] == NULL)
                                die("out of memory parsing %s scope", block->widget);
                            strcpy(block->scope_args[field], prop_value);
                        }
                        block->scope_fields |= 1u << field;
                    } else if(strcmp(block->widget, "Disabled") == 0) {
                        if(strcmp(prop_field, "when") != 0)
                            die("%s:%d: Disabled only accepts the 'when' property", rel, line_no);
                        if(block->prop_count != 0)
                            die("%s:%d: duplicate Disabled 'when' property", rel, line_no);
                        kir_copy(block->props, sizeof(block->props), prop_value);
                        block->prop_count = 1;
                    } else if(ui_block_set_web_prop(block, prop_field,
                                                    prop_value)) {
                        /* Source-level web facts travel in KIR metadata.
                         * They are not fields on the native widget props. */
                    } else {
                        ui_block_append_prop(block, prop_field, prop_value,
                                             KirSpan(rel, line_no, 1));
                    }
                    continue;
                }

                if(ui_block_count > 0 &&
                   !ui_blocks[ui_block_count - 1].opened &&
                   depth == ui_blocks[ui_block_count - 1].close_depth)
                    ui_block_open(fn, &ui_blocks[ui_block_count - 1],
                                  KirSpan(rel, line_no, pending_start_column),
                                  0);

                if(kind == KIR_STMT_EXPR &&
                   parse_widget_statement(t, widget, sizeof(widget),
                                          widget_args,
                                          sizeof(widget_args)))
                    kind = KIR_STMT_WIDGET;
                if(kind == KIR_STMT_WIDGET) {
                    KirStmt *st;
                    KirSourceSpan span = KirSpanEnd(rel, line_no,
                                                    pending_start_column,
                                                    line_no,
                                                    pending_start_column +
                                                    (int)strlen(t));

                    st = KirFunctionAddWidget(fn, widget, widget_args, t,
                                              span);
                    ui_stmt_apply_source_metadata(st, fn,
                        ui_block_count > 0 ? &ui_blocks[ui_block_count - 1] : NULL,
                        &root_anonymous_widget_count,
                        widget, span);
                } else {
                    KirStmt *st;
                    KirSourceSpan span = KirSpanEnd(rel, line_no,
                                                    pending_start_column,
                                                    line_no,
                                                    pending_start_column +
                                                    (int)strlen(t));

                    st = KirFunctionAddStmt(fn, kind, t, widget, span);
                    if(kind == KIR_STMT_RETURN || kind == KIR_STMT_DECL ||
                       kind == KIR_STMT_ASSIGN || kind == KIR_STMT_IF)
                        ui_stmt_apply_expression_widget_metadata(st, fn,
                            ui_block_count > 0 ? &ui_blocks[ui_block_count - 1] : NULL,
                            &root_anonymous_widget_count,
                            t, kind, span);
                }
                depth += brace_delta;
                if(depth < 0)
                    depth = 0;
            }
        }
    }
    if(slot_frame_count)
        die("%s:%d: unterminated slot body", rel, line_no);
    if(in != NULL)
        fclose(in);
    for(int mi = 0; mi < program->module_count; mi++)
        for(int fi = 0; fi < program->modules[mi].function_count; fi++) {
            if(!KirLowerCleanup(&program->modules[mi].functions[fi])) {
                KirProgramFree(program);
                free(ui_blocks);
                return NULL;
            }
            KirStructureFunction(&program->modules[mi].functions[fi], &program->modules[mi]);
        }
    free(ui_blocks);
    return program;
}

KirProgram *
kir_parse_file(const char *path, const char *root)
{
    FILE *in = fopen(path, "rb");
    if(in == NULL)
        die("%s: open failed: %s", path, strerror(errno));
    return parse_source(path, root, in, NULL);
}

KirProgram *
kir_parse_source(const char *path, const char *source)
{
    return parse_source(path, ".", NULL, source);
}
