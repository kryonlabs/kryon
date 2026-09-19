/*
 * kir.h - Kryon intermediate representation.
 *
 * KIR is the shared compiler representation between .kry source and the C/KRB
 * backends. This module owns the tree shape, explicit allocation, source spans,
 * and deterministic dumps used by tools and tests.
 */
#ifndef KRYON_KIR_H
#define KRYON_KIR_H

#include <stdio.h>

enum {
    KIR_PATH_MAX = 1024,
    KIR_NAME_MAX = 128,
    KIR_TEXT_MAX = 4096
};

typedef enum KirImportKind {
    KIR_IMPORT_HEADER = 1,
    KIR_IMPORT_MODULE,
    KIR_IMPORT_EXTERN,
    KIR_IMPORT_INTRINSIC,
    KIR_IMPORT_CAPABILITY,
    KIR_IMPORT_HOST
} KirImportKind;

typedef enum KirExternKind {
    KIR_EXTERN_NONE = 0,
    KIR_EXTERN_HOST,
    KIR_EXTERN_GO,
    KIR_EXTERN_C
} KirExternKind;

typedef enum KirStmtKind {
    KIR_STMT_UNKNOWN = 0,
    KIR_STMT_BLOCK_OPEN,
    KIR_STMT_BLOCK_CLOSE,
    KIR_STMT_DECL,
    KIR_STMT_ASSIGN,
    KIR_STMT_EXPR,
    KIR_STMT_IF,
    KIR_STMT_WHILE,
    KIR_STMT_FOR,
    KIR_STMT_SWITCH,
    KIR_STMT_CASE,
    KIR_STMT_RETURN,
    KIR_STMT_BREAK,
    KIR_STMT_CONTINUE,
    KIR_STMT_GOTO,
    KIR_STMT_LABEL,
    KIR_STMT_DEFER,
    KIR_STMT_UNUSED,
    KIR_STMT_RAW,
    KIR_STMT_WIDGET
} KirStmtKind;

typedef enum KirExprKind {
    KIR_EXPR_UNKNOWN = 0,
    KIR_EXPR_IDENT,
    KIR_EXPR_INT,
    KIR_EXPR_FLOAT,
    KIR_EXPR_STRING,
    KIR_EXPR_CALL,
    KIR_EXPR_BINARY,
    KIR_EXPR_UNARY,
    KIR_EXPR_MEMBER,
    KIR_EXPR_POINTER_MEMBER,
    KIR_EXPR_INDEX,
    KIR_EXPR_CAST,
    KIR_EXPR_COMPOUND,
    KIR_EXPR_SIZEOF,
    KIR_EXPR_CHAR,
    KIR_EXPR_CONDITIONAL,
    KIR_EXPR_POSTFIX,
    KIR_EXPR_FIELD_INIT,
    KIR_EXPR_SLICE
} KirExprKind;

typedef struct KirSourceSpan {
    char path[KIR_PATH_MAX];
    int line;
    int column;
    int end_line;
    int end_column;
} KirSourceSpan;

typedef struct KirStateField {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
    char init[KIR_TEXT_MAX];
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirStateField;

typedef struct KirImport {
    KirImportKind kind;
    KirExternKind extern_kind;
    char name[KIR_NAME_MAX];
    char target[KIR_PATH_MAX];
    char extern_symbol[KIR_NAME_MAX];
    char signature[KIR_TEXT_MAX];
    char args[KIR_TEXT_MAX];       /* parsed extern parameters */
    char return_type[KIR_NAME_MAX];
    int required;
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
    const struct KirModule *resolved_module; /* borrowed from the checked program set */
} KirImport;

typedef enum KirStyleImportKind {
    KIR_STYLE_IMPORT_FILE = 1,
    KIR_STYLE_IMPORT_BUILTIN
} KirStyleImportKind;

typedef struct KirStyleImport {
    KirStyleImportKind kind;
    char alias[KIR_NAME_MAX];
    char target[KIR_PATH_MAX];
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirStyleImport;

typedef struct KirStmt {
    KirStmtKind kind;
    char text[KIR_TEXT_MAX];
    char widget[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    char node_name[KIR_NAME_MAX];       /* source-level UI block identity */
    char node_key[KIR_TEXT_MAX];        /* stable source-level UI key */
    char node_path[KIR_TEXT_MAX];       /* stable source-level UI tree path */
    char node_parent_path[KIR_TEXT_MAX];/* parent UI tree path, empty at root */
    char dom_tag[KIR_NAME_MAX];         /* requested browser element tag */
    char dom_ref[KIR_NAME_MAX];         /* requested Kry DOM object ref */
    char dom_id[KIR_NAME_MAX];          /* requested browser id */
    char dom_name_attr[KIR_NAME_MAX];   /* requested browser name attribute */
    char dom_value_attr[KIR_TEXT_MAX];  /* requested browser value attribute */
    char dom_class[KIR_TEXT_MAX];       /* requested browser/KSS classes */
    char dom_title[KIR_TEXT_MAX];       /* requested browser title */
    char dom_href[KIR_TEXT_MAX];        /* requested browser link target */
    char dom_target[KIR_NAME_MAX];      /* requested browser browsing context */
    char dom_rel[KIR_TEXT_MAX];         /* requested browser link relation */
    char dom_for_attr[KIR_NAME_MAX];    /* requested label/control id */
    char dom_part[KIR_TEXT_MAX];        /* requested element part names */
    char dom_slot[KIR_NAME_MAX];        /* requested shadow slot name */
    char dom_data_attrs[KIR_TEXT_MAX];  /* requested data-* attributes */
    char dom_extra_attrs[KIR_TEXT_MAX]; /* requested arbitrary browser attributes */
    char dom_placeholder[KIR_TEXT_MAX]; /* requested input placeholder */
    char dom_input_type[KIR_NAME_MAX];  /* requested browser input type */
    char dom_form_attr[KIR_NAME_MAX];   /* requested form owner id */
    char dom_form_action[KIR_TEXT_MAX]; /* requested form action URL */
    char dom_form_method[KIR_NAME_MAX]; /* requested form method */
    char dom_form_enctype[KIR_NAME_MAX];/* requested form encoding */
    char dom_autocomplete[KIR_NAME_MAX];/* requested autocomplete policy */
    char dom_hidden[KIR_NAME_MAX];      /* requested hidden expression */
    char dom_draggable[KIR_NAME_MAX];   /* requested draggable policy */
    char dom_spellcheck[KIR_NAME_MAX];  /* requested spellcheck policy */
    char dom_contenteditable[KIR_NAME_MAX]; /* requested editing policy */
    char dom_autofocus[KIR_NAME_MAX];   /* requested autofocus expression */
    char dom_inert[KIR_NAME_MAX];       /* requested inert expression */
    char dom_autocapitalize[KIR_NAME_MAX]; /* requested autocapitalize policy */
    char dom_enterkeyhint[KIR_NAME_MAX];/* requested virtual keyboard enter hint */
    char dom_download[KIR_TEXT_MAX];    /* requested download filename */
    char dom_formnovalidate[KIR_NAME_MAX]; /* requested formnovalidate expression */
    char dom_novalidate[KIR_NAME_MAX];  /* requested novalidate expression */
    char dom_popover[KIR_NAME_MAX];     /* requested popover policy */
    char dom_popover_target[KIR_NAME_MAX]; /* requested popover target id */
    char dom_popover_target_action[KIR_NAME_MAX]; /* requested popover target action */
    char dom_readonly[KIR_NAME_MAX];    /* requested readonly expression */
    char dom_required[KIR_NAME_MAX];    /* requested required expression */
    char dom_min[KIR_NAME_MAX];         /* requested form min expression */
    char dom_max[KIR_NAME_MAX];         /* requested form max expression */
    char dom_step[KIR_NAME_MAX];        /* requested form step expression */
    char dom_minlength[KIR_NAME_MAX];   /* requested minlength expression */
    char dom_maxlength[KIR_NAME_MAX];   /* requested maxlength expression */
    char dom_pattern[KIR_TEXT_MAX];     /* requested form validation pattern */
    char dom_accept[KIR_TEXT_MAX];      /* requested file accept list */
    char dom_multiple[KIR_NAME_MAX];    /* requested multiple expression */
    char dom_inputmode[KIR_NAME_MAX];   /* requested inputmode hint */
    char dom_headers[KIR_TEXT_MAX];     /* requested table header refs */
    char dom_scope[KIR_NAME_MAX];       /* requested table header scope */
    char dom_colspan[KIR_NAME_MAX];     /* requested table colspan expression */
    char dom_rowspan[KIR_NAME_MAX];     /* requested table rowspan expression */
    char dom_tab_index[KIR_NAME_MAX];   /* requested tabindex expression */
    char dom_role[KIR_NAME_MAX];        /* requested accessibility role */
    char dom_aria_label[KIR_TEXT_MAX];  /* requested accessible label */
    char dom_aria_description[KIR_TEXT_MAX]; /* requested accessible description */
    char dom_aria_describedby[KIR_TEXT_MAX]; /* requested accessible relationship */
    char dom_aria_labelledby[KIR_TEXT_MAX]; /* requested labelling relationship */
    char dom_aria_activedescendant[KIR_TEXT_MAX]; /* requested active descendant */
    char dom_aria_controls[KIR_TEXT_MAX];    /* requested controlled element ids */
    char dom_aria_owns[KIR_TEXT_MAX];        /* requested owned element ids */
    char dom_aria_sort[KIR_NAME_MAX];        /* requested table/list sort order */
    char dom_aria_orientation[KIR_NAME_MAX]; /* requested widget orientation */
    char dom_aria_level[KIR_NAME_MAX];       /* requested tree/list level */
    char dom_aria_posinset[KIR_NAME_MAX];    /* requested item position */
    char dom_aria_setsize[KIR_NAME_MAX];     /* requested set size */
    char dom_aria_haspopup[KIR_NAME_MAX];    /* requested popup relationship type */
    char dom_aria_multiselectable[KIR_NAME_MAX]; /* requested multi-select state */
    char dom_aria_rowindex[KIR_NAME_MAX];    /* requested table/grid row index */
    char dom_aria_colindex[KIR_NAME_MAX];    /* requested table/grid column index */
    char dom_aria_rowcount[KIR_NAME_MAX];    /* requested table/grid row count */
    char dom_aria_colcount[KIR_NAME_MAX];    /* requested table/grid column count */
    char dom_aria_live[KIR_NAME_MAX];        /* requested live region policy */
    char dom_aria_attrs[KIR_TEXT_MAX];  /* requested arbitrary aria-* attributes */
    char dom_on_click[KIR_NAME_MAX];    /* logic function bound to click */
    char dom_on_input[KIR_NAME_MAX];    /* logic function bound to input(value) */
    char dom_on_before_input[KIR_NAME_MAX]; /* logic function bound to beforeinput(value) */
    char dom_on_change[KIR_NAME_MAX];   /* logic function bound to change(value) */
    char dom_on_select[KIR_NAME_MAX];   /* logic function bound to select(value) */
    char dom_on_key[KIR_NAME_MAX];      /* logic function bound to keydown(key) */
    char dom_on_invalid[KIR_NAME_MAX];  /* logic function bound to invalid(value) */
    char dom_on_submit[KIR_NAME_MAX];   /* logic function bound to submit */
    char dom_on_reset[KIR_NAME_MAX];    /* logic function bound to reset */
    char dom_on_toggle[KIR_NAME_MAX];   /* logic function bound to toggle */
    char dom_on_close[KIR_NAME_MAX];    /* logic function bound to close */
    char dom_on_cancel[KIR_NAME_MAX];   /* logic function bound to cancel */
    char dom_on_focus[KIR_NAME_MAX];    /* logic function bound to focus */
    char dom_on_blur[KIR_NAME_MAX];     /* logic function bound to blur */
    char dom_on_scroll[KIR_NAME_MAX];   /* logic function bound to scroll(value) */
    char dom_on_mouse_enter[KIR_NAME_MAX]; /* logic function bound to mouseenter */
    char dom_on_mouse_leave[KIR_NAME_MAX]; /* logic function bound to mouseleave */
    char dom_on_mouse_move[KIR_NAME_MAX];  /* logic function bound to mousemove */
    char dom_on_mouse_down[KIR_NAME_MAX];  /* logic function bound to mousedown */
    char dom_on_mouse_up[KIR_NAME_MAX];    /* logic function bound to mouseup */
    char dom_on_wheel[KIR_NAME_MAX];       /* logic function bound to wheel(value) */
    char dom_on_drag_start[KIR_NAME_MAX];  /* logic function bound to dragstart(value) */
    char dom_on_drag_end[KIR_NAME_MAX];    /* logic function bound to dragend(value) */
    char dom_on_drag_over[KIR_NAME_MAX];   /* logic function bound to dragover */
    char dom_on_drop[KIR_NAME_MAX];        /* logic function bound to drop(value) */
    char dom_on_copy[KIR_NAME_MAX];        /* logic function bound to copy(value) */
    char dom_on_cut[KIR_NAME_MAX];         /* logic function bound to cut(value) */
    char dom_on_paste[KIR_NAME_MAX];       /* logic function bound to paste(value) */
    int declared_widget; /* typed #ui block invocation, resolved after imports */
    int widget_fallback; /* leaf block may use host props only if no declaration resolves */
    int is_instance; /* typed record binding retained by its explicit key */
    int expr_root;      /* index into enclosing function exprs, or -1 */
    int lhs_root;       /* structured assignment destination, or -1 */
    char name[KIR_NAME_MAX]; /* declaration binding */
    char type[KIR_NAME_MAX]; /* declared or inferred type */
    char assignment_op[4];
    KirSourceSpan span;
} KirStmt;

typedef struct KirExpr {
    KirExprKind kind;
    int is_function_value; /* declaration bound to an expected slot signature */
    char slot_type[KIR_NAME_MAX]; /* lexical callable signature, empty for ordinary calls */
    char text[KIR_TEXT_MAX];
    char name[KIR_NAME_MAX];
    char op[8];
    int left;
    int right;
    int first_child;
    int next_sibling;
    int third;         /* false conditional arm or slice upper bound, or -1 */
    char type[KIR_NAME_MAX]; /* resolved type; empty means unresolved */
    KirSourceSpan span;
} KirExpr;

typedef struct KirCapture {
    int is_instance;
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
} KirCapture;

typedef struct KirFunction {
    char name[KIR_NAME_MAX];
    char args[KIR_TEXT_MAX];
    char return_type[KIR_NAME_MAX];
    int exported;
    int is_extern;
    KirExternKind extern_kind;
    int is_colon;   /* 'Name :: (...) {' form: C name has no _kry_draw suffix */
    int is_closure; /* inline slot body, emitted at its lexical binding */
    KirCapture *captures;
    int capture_count;
    int is_ui;      /* '#ui' function: declares a retained UI hierarchy */
    int is_public;  /* exported function or project route */
    int checked;    /* shared checker resolved the function without errors */
    int uses_host; /* direct or transitive host services or retained-state access */
    char extern_target[KIR_NAME_MAX];   /* '#extern "pkg.Fn"' quoted symbol */
    char extern_symbol[KIR_NAME_MAX];   /* stripped C symbol for c.* externs */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
    KirStmt *stmts;
    int stmt_count;
    int stmt_cap;
    KirExpr *exprs;
    int expr_count;
    int expr_cap;
} KirFunction;

typedef struct KirGlobal {
    char name[KIR_NAME_MAX];
    char type[KIR_TEXT_MAX];
    char init[KIR_TEXT_MAX];
    int is_static;   /* 'static name: T = init' — internal linkage */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirGlobal;

/* A Kry `Name :: value` constant emitted into generated target interfaces. */
typedef struct KirDefine {
    char name[KIR_NAME_MAX];
    char value[KIR_TEXT_MAX];
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirDefine;

typedef struct KirAssert {
    char condition[KIR_TEXT_MAX]; /* expanded C preprocessor condition */
    char message[KIR_TEXT_MAX];   /* C #error payload */
    int known;                    /* condition resolved by Kry frontend */
    int value;                    /* boolean value when known */
    char guard[KIR_TEXT_MAX];     /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirAssert;

/* A `Name :: struct { fields }` type declaration. body holds the raw field
 * lines (one per line, no braces). */
typedef struct KirType {
    char name[KIR_NAME_MAX];
    char body[KIR_TEXT_MAX * 2];
    int is_slot;   /* named, synchronous child-content signature; body holds parameters */
    int is_enum;   /* 'Name :: enum' — emit typedef enum, not struct */
    int is_extern; /* host-owned C record; native Go emits the declared shape */
    char guard[KIR_TEXT_MAX];   /* enclosing '#if' condition (expanded) */
    KirSourceSpan span;
} KirType;

typedef struct KirTypeField {
    char name[KIR_NAME_MAX];
    char type[KIR_NAME_MAX];
} KirTypeField;

/* Start offset at zero. Returns 1 for a field, 0 at end, -1 for malformed
 * record syntax. Names/types are trimmed without truncating source tokens. */
int KirTypeNextField(const KirType *record, size_t *offset, KirTypeField *field);

/* Parse a fixed-capacity array type text "[N]element". Returns 1 with the
 * element type copied out and the capacity stored, 0 for any other type. */
int KirSliceElementType(const char *type, char *element, size_t element_size);
int KirArrayElementType(const char *type, char *element, size_t element_size,
                        int *capacity);

typedef struct KirAppMeta {
    int has_app;
    char title[KIR_NAME_MAX];
    int width;
    int height;
    int fps;
    char theme[KIR_NAME_MAX];
    int dark_mode;
    int font_examples;
    char frame[KIR_NAME_MAX];
    char init[KIR_NAME_MAX];
    char scene[KIR_NAME_MAX];
    char shutdown[KIR_NAME_MAX];
} KirAppMeta;

typedef struct KirRoute {
    char id[KIR_NAME_MAX];
    char title[KIR_NAME_MAX];
    char group[KIR_NAME_MAX];
    char page[KIR_NAME_MAX];
    char path[KIR_PATH_MAX];
    char guard[KIR_TEXT_MAX];
    KirSourceSpan span;
} KirRoute;

typedef struct KirModule {
    char name[KIR_NAME_MAX];
    char source_path[KIR_PATH_MAX];
    KirSourceSpan span;
    KirAppMeta app;
    KirGlobal *globals;
    int global_count;
    int global_cap;
    KirDefine *defines;
    int define_count;
    int define_cap;
    KirAssert *asserts;
    int assert_count;
    int assert_cap;
    KirType *types;
    int type_count;
    int type_cap;
    KirStateField *state_fields;
    int state_count;
    int state_cap;
    KirImport *imports;
    int import_count;
    int import_cap;
    KirStyleImport *style_imports;
    int style_import_count;
    int style_import_cap;
    KirRoute *routes;
    int route_count;
    int route_cap;
    KirFunction *functions;
    int function_count;
    int function_cap;
} KirModule;

typedef struct KirProgram {
    KirModule *modules;
    int module_count;
    int module_cap;
} KirProgram;

/* Local records and direct unqualified Kry imports; owner supplies field scope. */
/* Local declarations shadow imports. Returns 1 found, 0 absent, -1 ambiguous. */
int KirResolveFunction(const KirModule *module, const char *name,
                       const KirModule **owner, const KirFunction **function);
/* Runtime contracts are parsed from embedded declaration sources. */
const KirType *KirFindRuntimeEnumMember(const char *name);
const KirType *KirFindRuntimeType(const char *name, const KirModule **owner);
const KirType *KirFindType(const KirModule *module, const char *name,
                          const KirModule **owner);
int KirResolveEnumMember(const KirModule *module, const char *name,
                         const KirModule **owner, const KirType **type);

KirProgram *KirProgramNew(void);
void KirProgramFree(KirProgram *program);
void kir_copy(char *dst, size_t dst_size, const char *src);
KirSourceSpan KirSpan(const char *path, int line, int column);
KirSourceSpan KirSpanEnd(const char *path, int line, int column,
                         int end_line, int end_column);
KirModule *KirProgramAddModule(KirProgram *program, const char *name,
                               const char *source_path, KirSourceSpan span);
KirStateField *KirModuleAddStateField(KirModule *module, const char *name,
                                      const char *type, const char *init,
                                      KirSourceSpan span);
KirImport *KirModuleAddImport(KirModule *module, KirImportKind kind,
                              const char *name, const char *target,
                              const char *signature, int required,
                              KirSourceSpan span);
KirStyleImport *KirModuleAddStyleImport(KirModule *module,
                                        KirStyleImportKind kind,
                                        const char *target,
                                        const char *alias,
                                        KirSourceSpan span);
KirFunction *KirModuleAddFunction(KirModule *module, const char *name,
                                  const char *args, const char *return_type,
                                  int exported, KirSourceSpan span);
void KirModuleAddGlobal(KirModule *module, const char *name, const char *type,
                        const char *init, KirSourceSpan span);
void KirModuleAddStatic(KirModule *module, const char *name, const char *type,
                        const char *init, KirSourceSpan span);
KirDefine *KirModuleAddDefine(KirModule *module, const char *name,
                              const char *value, KirSourceSpan span);
KirAssert *KirModuleAddAssert(KirModule *module, const char *condition,
                              const char *message, KirSourceSpan span);
KirType *KirModuleAddType(KirModule *module, const char *name,
                          KirSourceSpan span);
KirRoute *KirModuleAddRoute(KirModule *module, const char *id,
                            KirSourceSpan span);
KirStmt *KirFunctionAddStmt(KirFunction *fn, KirStmtKind kind,
                            const char *text, const char *widget,
                            KirSourceSpan span);
KirStmt *KirFunctionAddWidget(KirFunction *fn, const char *widget,
                              const char *args, const char *text,
                              KirSourceSpan span);
const char *KirImportKindName(KirImportKind kind);
const char *KirStmtKindName(KirStmtKind kind);
const char *KirExprKindName(KirExprKind kind);
KirExpr *KirFunctionAddExpr(KirFunction *fn, KirExprKind kind,
                            const char *text, KirSourceSpan span);
void KirProgramDump(const KirProgram *program, FILE *out);

#endif /* KRYON_KIR_H */
