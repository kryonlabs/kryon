#include "kir_emit.h"
#include "kir_check.h"
#include "kir_text.h"
#include "kir_expr.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int
format(char *out, size_t size, const char *format_string, ...)
{
    va_list ap;
    va_start(ap,format_string);
    int n=vsnprintf(out,size,format_string,ap);
    va_end(ap);
    if(n<0 || (size_t)n>=size) {
        fprintf(stderr,"generated expression exceeds output limit\n"); exit(1);
    }
    return n;
}

static const char *
canonical(const char *type)
{
    const char *result = KirScalarType(type);
    if(!strcmp(type, "integer")) return "i32";
    if(!strcmp(type, "real")) return "f64";
    return *result ? result : type;
}

const char *
KirTargetType(const char *type, KirTarget target)
{
    static const struct { const char *type, *c, *go; } map[] = {
        {"i8", "int8_t", "int8"}, {"i16", "int16_t", "int16"},
        {"i32", "int32_t", "int32"}, {"i64", "int64_t", "int64"},
        {"u8", "uint8_t", "uint8"}, {"u16", "uint16_t", "uint16"},
        {"u32", "uint32_t", "uint32"}, {"u64", "uint64_t", "uint64"},
        {"f32", "float", "float32"}, {"f64", "double", "float64"},
        {"bool", "bool", "bool"}, {"void", "void", ""},
        {"string", "String", "string"}, {NULL, NULL, NULL}
    };
    type = canonical(type);
    for(int i = 0; map[i].type; i++)
        if(!strcmp(type, map[i].type)) return target == KIR_GO ? map[i].go : map[i].c;
    return NULL;
}

static int width(const char *type) { return (*type == 'i' || *type == 'u') ? atoi(type + 1) : 0; }
static int signed_type(const char *type) { return *type == 'i'; }

static int
enum_type(const KirModule *module, const char *type)
{
    const KirType *declared = KirFindType(module, type, NULL);
    return declared != NULL && declared->is_enum;
}

static int
record_type(const KirModule *module, const char *type)
{
    const KirType *declared = KirFindType(module, type, NULL);
    return declared != NULL && !declared->is_enum;
}

void
KirEmitStringType(FILE *out)
{
    fputs("\n#ifndef KIR_STRING_VALUE_DEFINED\n#define KIR_STRING_VALUE_DEFINED\n"
          "#include <stddef.h>\n#include <string.h>\n"
          "/* Immutable borrowed UTF-8 bytes; length preserves embedded nulls. */\n"
          "typedef struct String { const char *data; size_t length; } String;\n"
          "static inline String StringView(const char *data, size_t length) {\n"
          "    String value = {data, length};\n    return value;\n}\n"
          "static inline bool StringEqual(String a, String b) {\n"
          "    return a.length == b.length && (a.length == 0 || memcmp(a.data, b.data, a.length) == 0);\n"
          "}\n#endif\n", out);
}

static const char *
zero_value(const char *type, KirTarget target)
{
    if(!strcmp(canonical(type), "string"))
        return target == KIR_C || target == KIR_CPP ? "StringView(NULL, 0)" : "\"\"";
    if(!strcmp(canonical(type), "bool")) return "false";
    return target == KIR_JS && width(canonical(type)) > 32 ? "0n" : "0";
}

typedef struct TypePath {
    const KirType *record;
    const struct TypePath *parent;
} TypePath;

static int
portable_type_path(const KirModule *module, const char *type, const TypePath *path)
{
    const KirModule *owner = NULL;
    const KirType *record;
    size_t offset = 0;
    KirTypeField field;
    int fields = 0;
    int status;

    if(KirTargetType(type, KIR_C)) return 1;
    record = KirFindType(module, type, &owner);
    if(record == NULL)
        return 0;
    if(record->is_enum)
        return 1;
    for(const TypePath *ancestor = path; ancestor; ancestor = ancestor->parent) {
        if(ancestor->record == record)
            return 0;
    }
    TypePath current = {record, path};
    while((status = KirTypeNextField(record, &offset, &field)) == 1) {
        if(!portable_type_path(owner, field.type, &current) || !strcmp(field.type, "void"))
            return 0;
        fields++;
    }
    return status == 0 && fields > 0;
}

static int
portable_type(const KirModule *module, const char *type)
{
    return portable_type_path(module, type, NULL);
}

static int
supported_expression(const KirModule *module, const KirFunction *fn, int index)
{
    const KirExpr *e;
    if(index < 0) return 1;
    e = &fn->exprs[index];
    if(!portable_type(module, e->type)) return 0;
    switch(e->kind) {
    case KIR_EXPR_COMPOUND:
        if(!record_type(module, e->name)) return 0;
        break;
    case KIR_EXPR_FIELD_INIT: break;
    case KIR_EXPR_INT: case KIR_EXPR_FLOAT: case KIR_EXPR_IDENT: case KIR_EXPR_STRING: break;
    case KIR_EXPR_MEMBER: break;
    case KIR_EXPR_BINARY: case KIR_EXPR_CONDITIONAL: break;
    case KIR_EXPR_UNARY:
        if(!strcmp(e->op, "&") || !strcmp(e->op, "*")) return 0;
        break;
    case KIR_EXPR_POSTFIX: break;
    case KIR_EXPR_CAST:
        if(!KirTargetType(e->name, KIR_C) && !enum_type(module, e->name)) return 0;
        break;
    case KIR_EXPR_CALL: if(!e->name[0]) return 0; break;
    default: return 0;
    }
    if(!supported_expression(module, fn, e->left) || !supported_expression(module, fn, e->right) ||
       !supported_expression(module, fn, e->third)) return 0;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(!supported_expression(module, fn, child)) return 0;
    return 1;
}

int
KirCanEmitBody(const KirModule *module, const KirFunction *fn)
{
    char params[64][KIR_TEXT_MAX];
    int count;
    /* A widget declaration is a typed function. Its annotation must not
     * bypass shared semantics when every operation is portable. Host calls
     * and unsupported composition still fail the normal eligibility checks. */
    if(!fn->checked || fn->is_extern || !portable_type(module, fn->return_type)) return 0;
    count = *kir_skip_ws(fn->args) ? kir_split_top(fn->args, params[0], 64, sizeof(params[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(params[i], ':');
        if(!colon || !portable_type(module, kir_skip_ws(colon + 1))) return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++) {
        const KirStmt *st = &fn->stmts[i];
        switch(st->kind) {
        case KIR_STMT_DECL: if(!portable_type(module, st->type)) return 0; break;
        case KIR_STMT_ASSIGN:
            if(st->lhs_root < 0 || (fn->exprs[st->lhs_root].kind != KIR_EXPR_IDENT &&
                fn->exprs[st->lhs_root].kind != KIR_EXPR_MEMBER)) return 0;
            break;
        case KIR_STMT_IF: if(!strncmp(st->text, "guard", 5)) return 0; break;
        case KIR_STMT_BLOCK_OPEN: case KIR_STMT_BLOCK_CLOSE:
        case KIR_STMT_WHILE: case KIR_STMT_RETURN: case KIR_STMT_BREAK:
        case KIR_STMT_CONTINUE: case KIR_STMT_UNUSED: case KIR_STMT_EXPR: break;
        default: return 0;
        }
        if(!supported_expression(module, fn, st->expr_root) || !supported_expression(module, fn, st->lhs_root)) return 0;
    }
    return 1;
}

static void
number_prefix(const KirModule *module, char *out, size_t size)
{
    uint32_t hash = 2166136261u;
    for(const unsigned char *p = (const unsigned char *)module->source_path; *p; p++)
        hash = (hash ^ *p) * 16777619u;
    format(out, size, "number_%08x", hash);
}

void
KirEmitNumbers(FILE *out, const KirModule *module, KirTarget target)
{
    char p[64];
    int used = 0;
    for(int i = 0; i < module->function_count; i++) used |= KirCanEmitBody(module, &module->functions[i]);
    if(!used) return;
    if(target == KIR_C || target == KIR_CPP) {
        int instances = 0;
        for(int i = 0; i < module->function_count; i++) {
            const KirFunction *fn = &module->functions[i];
            for(int j = 0; j < fn->stmt_count; j++)
                instances |= fn->stmts[j].is_instance;
        }
        if(instances)
            fputs("#include \"ui_instance.h\"\n", out);
    }
    number_prefix(module, p, sizeof(p));
    if(target == KIR_C || target == KIR_CPP) {
        fprintf(out, "#include <stdint.h>\n#include <stdbool.h>\n#include <stdlib.h>\n\n");
        fprintf(out,
            "static inline int64_t %s_signed(uint64_t x, int w) {\n"
            "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
            "    x &= mask;\n"
            "    return x <= (mask >> 1) ? (int64_t)x : -1 - (int64_t)(mask - x);\n}\n", p);
        fprintf(out,
            "static inline uint64_t %s_bits(uint64_t a, uint64_t b, int w, int sign, int op) {\n"
            "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
            "    uint64_t shift = b; a &= mask; b &= mask;\n"
            "    switch(op) {\n"
            "    case 0: return a; case 1: return (a + b) & mask;\n"
            "    case 2: return (a - b) & mask; case 3: return (a * b) & mask;\n"
            "    case 4: case 5:\n"
            "        if(!b) abort();\n"
            "        if(sign) { int64_t x = %s_signed(a,w), y = %s_signed(b,w);\n"
            "            if(x == INT64_MIN && y == -1) return op == 4 ? a : 0;\n"
            "            return (uint64_t)(op == 4 ? x / y : x %% y) & mask; }\n"
            "        return op == 4 ? a / b : a %% b;\n"
            "    case 6: case 7:\n"
            "        if(shift >= (uint64_t)w) abort();\n"
            "        if(op == 6) return (a << shift) & mask;\n"
            "        if(!shift) return a;\n"
            "        return (a >> shift) | ((sign && (a & (UINT64_C(1) << (w-1)))) ? mask ^ (mask >> shift) : 0);\n"
            "    case 8: return a & b; case 9: return a | b; case 10: return a ^ b;\n"
            "    default: abort(); }\n}\n\n", p, p, p);
        fprintf(out,
            "static inline uint64_t %s_float(double x, int w, int sign) {\n"
            "    double bound = 1; for(int i = 0; i < w-sign; i++) bound *= 2;\n"
            "    if(!(x >= (sign ? -bound : 0) && x < bound)) abort();\n"
            "    return sign ? (uint64_t)(int64_t)x : (uint64_t)x;\n}\n", p);
    } else if(target == KIR_GO) {
        fprintf(out,
            "func %s_float(x float64, w uint, sign bool) uint64 {\n"
            "    bits := w; if sign { bits-- }; bound := float64(1); for i := uint(0); i < bits; i++ { bound *= 2 }; lower := float64(0); if sign { lower = -bound }\n"
            "    if !(x >= lower && x < bound) { panic(\"float conversion out of range\") }; if sign { return uint64(int64(x)) }; return uint64(x)\n}\n", p);
        fprintf(out,
            "func %s_bits(a, b uint64, w uint, sign bool, op int) uint64 {\n"
            "    mask := ^uint64(0); if w < 64 { mask = (uint64(1) << w) - 1 }; shift := b; a &= mask; b &= mask\n"
            "    switch op {\n"
            "    case 0: return a\n    case 1: return (a+b)&mask\n    case 2: return (a-b)&mask\n    case 3: return (a*b)&mask\n"
            "    case 4,5:\n        if b == 0 { panic(\"integer division by zero\") }\n"
            "        if sign { x := int64(a << (64-w)) >> (64-w); y := int64(b << (64-w)) >> (64-w); if op == 4 { return uint64(x/y)&mask }; return uint64(x%%y)&mask }; if op == 4 { return a/b }; return a%%b\n"
            "    case 6,7:\n        if shift >= uint64(w) { panic(\"invalid shift count\") }; if op == 6 { return (a<<shift)&mask }; if shift == 0 { return a }; result := a>>shift; if sign && (a & (uint64(1)<<(w-1))) != 0 { result |= mask ^ (mask>>shift) }; return result\n"
            "    case 8: return a&b\n    case 9: return a|b\n    case 10: return a^b\n    }; panic(\"invalid numeric operation\")\n}\n\n", p);
    } else {
        fprintf(out,
            "function %s_float(x, w, sign) {\n"
            "  const bound = 2 ** (w - (sign ? 1 : 0));\n"
            "  if(!(x >= (sign ? -bound : 0) && x < bound)) throw new RangeError('float conversion out of range');\n"
            "  return BigInt(Math.trunc(x));\n}\n", p);
        fprintf(out,
            "function %s_value(x, w, sign) {\n"
            "  if (typeof x === 'number' && !Number.isSafeInteger(x)) throw new RangeError('integer argument requires an exact value');\n"
            "  x = sign ? BigInt.asIntN(w, BigInt(x)) : BigInt.asUintN(w, BigInt(x));\n"
            "  return w > 32 ? x : Number(x);\n}\n"
            "function %s_bool(x) { if(typeof x !== 'boolean') throw new TypeError('boolean value required'); return x; }\n"
            "function %s_bits(a, b, w, sign, op) {\n"
            "  const shift = BigInt(b); a = BigInt.asUintN(w, BigInt(a)); b = BigInt.asUintN(w, BigInt(b));\n"
            "  let result;\n"
            "  switch (op) {\n"
            "  case 0: result=a; break; case 1: result=a+b; break; case 2: result=a-b; break; case 3: result=a*b; break;\n"
            "  case 4: case 5:\n"
            "    if(b === 0n) throw new RangeError('integer division by zero');\n"
            "    if(sign) { a=BigInt.asIntN(w,a); b=BigInt.asIntN(w,b); }\n"
            "    result=op === 4 ? a/b : a%%b; break;\n"
            "  case 6: case 7:\n"
            "    if(shift < 0n || shift >= BigInt(w)) throw new RangeError('invalid shift count');\n"
            "    result=op === 6 ? a<<shift : (sign ? BigInt.asIntN(w,a) : a)>>shift; break;\n"
            "  case 8: result=a&b; break; case 9: result=a|b; break; case 10: result=a^b; break;\n"
            "  default: throw new Error('invalid numeric operation');\n"
            "  }\n  return %s_value(result,w,sign);\n}\n\n", p, p, p, p);
    }
}

typedef struct Local {
    char name[KIR_NAME_MAX];
    int depth;
    int is_instance;
} Local;
typedef struct Emitter {
    FILE *out;
    const KirModule *module;
    const KirFunction *fn;
    KirTarget target;
    KirResolveTarget resolve;
    void *context;
    const char *instance_host;
    int indent, serial, depth, local_count;
    Local *locals;
    char numbers[64];
} Emitter;

static void
line(Emitter *e, const char *format, ...)
{
    va_list ap;
    for(int i = 0; i < e->indent; i++) fputs("    ", e->out);
    va_start(ap, format); vfprintf(e->out, format, ap); va_end(ap);
    fputc('\n', e->out);
}

static void
fatal(const KirExpr *expr, const char *message)
{
    fprintf(stderr, "%s:%d:%d: %s: %s\n", expr->span.path, expr->span.line,
            expr->span.column, message, expr->text);
    exit(1);
}

static void
fresh(Emitter *e, char *name)
{
    int collision;
    do {
        format(name, KIR_NAME_MAX, "value_%d", e->serial++);
        collision = strstr(e->fn->args, name) != NULL;
        for(int i = 0; i < e->fn->stmt_count; i++) collision |= strstr(e->fn->stmts[i].text, name) != NULL;
        for(int i = 0; i < e->module->state_count; i++) collision |= !strcmp(e->module->state_fields[i].name, name);
        for(int i = 0; i < e->module->global_count; i++) collision |= !strcmp(e->module->globals[i].name, name);
        for(int i = 0; i < e->module->define_count; i++) collision |= !strcmp(e->module->defines[i].name, name);
        for(int i = 0; i < e->module->function_count; i++) collision |= !strcmp(e->module->functions[i].name, name);
    } while(collision);
}

/* Stream the declared shape instead of expanding a nested record into a
 * bounded expression buffer. Copies retain C/Go's independent value semantics. */
static void
js_record_value(FILE *out, const KirModule *module, const char *type, const char *source)
{
    fputc('{', out);
    const KirModule *owner = NULL;
    const KirType *record = KirFindType(module, type, &owner);
    if(record != NULL) {
        size_t offset = 0;
        int count = 0;
        KirTypeField field;

        while(KirTypeNextField(record, &offset, &field) == 1) {
            const char *field_type = canonical(field.type);
            char path[KIR_TEXT_MAX];
            fprintf(out, "%s%s: ", count++ ? ", " : "", field.name);
            if(source != NULL)
                format(path, sizeof(path), "%s.%s", source, field.name);
            if(record_type(owner, field_type)) {
                js_record_value(out, owner, field_type, source ? path : NULL);
            } else if(source != NULL) {
                fputs(path, out);
            } else {
                fputs(zero_value(field_type, KIR_JS), out);
            }
        }
    }
    fputc('}', out);
}

int
KirEmitJsRecordValue(FILE *out, const KirModule *module, const char *type, const char *source)
{
    if(!record_type(module, type) || !portable_type(module, type))
        return 0;
    if(source != NULL)
        fputs("((record_source) => (", out);
    js_record_value(out, module, type, source ? "record_source" : NULL);
    if(source != NULL)
        fprintf(out, "))(%s)", source);
    return 1;
}

static void
js_record_fields(Emitter *e, const KirModule *module, const char *type, const char *source,
                 char *out, size_t size)
{
    char destination[KIR_NAME_MAX];
    fresh(e, destination);
    for(int i = 0; i < e->indent; i++)
        fputs("    ", e->out);
    fprintf(e->out, "let %s = ", destination);
    KirEmitJsRecordValue(e->out, module, type, source);
    fputs(";\n", e->out);
    kir_copy(out, size, destination);
}

static void
js_record_copy(Emitter *e, const char *type, const char *value,
               char *out, size_t size)
{
    char source[KIR_NAME_MAX];
    fresh(e, source);
    /* Capture once, including when the source is a function call. */
    line(e, "let %s = %s;", source, value);
    js_record_fields(e, e->module, type, source, out, size);
}

static void
declare(Emitter *e, const char *name, const char *type, const char *value)
{
    const char *target_type = KirTargetType(type, e->target);
    if(target_type == NULL) target_type = type;
    if(enum_type(e->module, type)) {
        if(e->target == KIR_GO)
            line(e, "var %s %s = %s(%s)", name, type, type, value);
        else if(e->target == KIR_JS)
            line(e, "let %s = %s_value(%s,32,true);", name, e->numbers, value);
        else
            line(e, "%s %s = (%s)(%s);", type, name, type, value);
        return;
    }
    if(e->target == KIR_GO) line(e, "var %s %s = %s", name, target_type, value);
    else if(e->target == KIR_JS && width(canonical(type)))
        line(e,"let %s = %s_value(%s,%d,%s);",name,e->numbers,value,width(canonical(type)),signed_type(canonical(type))?"true":"false");
    else if(e->target == KIR_JS && !strcmp(canonical(type),"bool"))
        line(e,"let %s = %s_bool(%s);",name,e->numbers,value);
    else if(e->target == KIR_JS && !KirTargetType(type, KIR_C)) {
        char copy[KIR_TEXT_MAX];
        js_record_copy(e, type, value, copy, sizeof(copy));
        line(e, "let %s = %s;", name, copy);
    }
    else if(e->target == KIR_JS) line(e, "let %s = %s;", name, value);
    else line(e, "%s %s = %s;", target_type, name, value);
}

static void
resolve(Emitter *e, const char *name, char *out, size_t size)
{
    for(int i = e->local_count - 1; i >= 0; i--)
        if(!strcmp(e->locals[i].name, name)) {
            if(e->locals[i].is_instance)
                format(out, size, e->target == KIR_JS ? "%s.value" : "(*%s)", name);
            else
                kir_copy(out, size, name);
            return;
        }
    e->resolve(e->context, name, out, size);
}

static int
operation(const char *op)
{
    static const char *const ops[] = {"", "+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"};
    for(int i = 1; i <= 10; i++) if(!strcmp(op,ops[i])) return i;
    return 0;
}

static void
number(Emitter *e, const char *type, const char *a, const char *b, int op, char *out, size_t size)
{
    char bits[KIR_TEXT_MAX];
    int w = width(type), sign = signed_type(type);
    if(e->target == KIR_GO) {
        format(bits,sizeof(bits),"%s_bits(uint64(%s),uint64(%s),%d,%s,%d)",e->numbers,a,b,w,sign?"true":"false",op);
        format(out,size,"%s(%s)",KirTargetType(type,e->target),bits);
    } else if(e->target == KIR_JS) {
        format(out,size,"%s_bits(%s,%s,%d,%s,%d)",e->numbers,a,b,w,sign?"true":"false",op);
    } else {
        format(bits,sizeof(bits),"%s_bits((uint64_t)(%s),(uint64_t)(%s),%d,%d,%d)",e->numbers,a,b,w,sign,op);
        if(sign) format(out,size,"(%s)%s_signed(%s,%d)",KirTargetType(type,e->target),e->numbers,bits,w);
        else format(out,size,"(%s)(%s)",KirTargetType(type,e->target),bits);
    }
}

static void emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size);
static void zero_record(Emitter *e, const char *type, char *out, size_t size);

static void
emit_destination(Emitter *e, int index, char *out, size_t size)
{
    const KirExpr *expr = &e->fn->exprs[index];
    if(expr->kind == KIR_EXPR_IDENT) {
        resolve(e, expr->name, out, size);
        return;
    }
    if(expr->kind == KIR_EXPR_MEMBER) {
        char base[KIR_TEXT_MAX], field[KIR_NAME_MAX];
        emit_destination(e, expr->left, base, sizeof(base));
        if(e->target == KIR_GO) kir_go_field_ident(expr->name, field, sizeof(field));
        else kir_copy(field, sizeof(field), expr->name);
        format(out, size, "%s.%s", base, field);
        return;
    }
    fatal(expr, "unsupported assignment destination");
}

static void
literal(Emitter *e, const KirExpr *expr, const char *type, int negative, char *out, size_t size)
{
    char raw[KIR_TEXT_MAX], *end;
    size_t length = 0;
    unsigned long long value;
    for(const char *p = expr->text; *p; p++) if(*p != '_') raw[length++] = *p;
    raw[length] = 0;
    errno = 0;
    value = strtoull(raw,&end,0);
    if(end == raw || errno == ERANGE) fatal(expr,"integer literal is out of range");
    if(*end && strcmp(end,"u") && strcmp(end,"U") && strcmp(end,"l") && strcmp(end,"L") &&
       strcmp(end,"ll") && strcmp(end,"LL") && strcmp(end,"ull") && strcmp(end,"ULL")) fatal(expr,"invalid integer literal");
    int w = width(type);
    if(w) {
        uint64_t max = w == 64 ? UINT64_MAX : (UINT64_C(1)<<w)-1;
        if(signed_type(type)) max = (max>>1) + (negative ? 1 : 0);
        if(value > max || (negative && !signed_type(type) && value != 0)) fatal(expr,"integer literal does not fit its type");
    }
    if(e->target == KIR_JS) format(out,size,"%s%llu%s",negative?"-":"",value,w>32?"n":"");
    else if(e->target == KIR_GO) format(out,size,"%s%llu",negative?"-":"",value);
    else if(negative && value == (UINT64_C(1)<<63)) format(out,size,"(-INT64_C(9223372036854775807)-1)");
    else format(out,size,"%s%llu%s",negative?"-":"",value,w>32?(signed_type(type)?"LL":"ULL"):"");
}

static void
string_literal(const KirExpr *expr, KirTarget target, char *out, size_t size)
{
    size_t used = 0;
    int remaining = 0;
    unsigned int scalar = 0, minimum = 0;
    out[used++] = '"';
    const unsigned char *cursor = (const unsigned char *)expr->text + 1;
    while(*cursor && *cursor != '"') {
        unsigned int value = *cursor++;
        int unicode_escape = 0;
        if(value == '\\') {
            value = *cursor++;
            const char *escapes = "0abfnrtv\\\"";
            const unsigned char values[] = {0, 7, 8, 12, 10, 13, 9, 11, '\\', '"'};
            const char *found = value ? strchr(escapes, (int)value) : NULL;
            if(found) {
                value = values[found - escapes];
            } else if(value == 'x' || value == 'u' || value == 'U') {
                unicode_escape = value != 'x';
                int digits = value == 'x' ? 2 : value == 'u' ? 4 : 8;
                value = 0;
                for(int index = 0; index < digits; index++) {
                    int digit = *cursor++;
                    if(digit >= '0' && digit <= '9') digit -= '0';
                    else if(digit >= 'a' && digit <= 'f') digit -= 'a' - 10;
                    else if(digit >= 'A' && digit <= 'F') digit -= 'A' - 10;
                    else fatal(expr, "invalid string escape");
                    value = value * 16 + (unsigned int)digit;
                }
                if(value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                    fatal(expr, "string escape is not a Unicode scalar value");
            } else {
                fatal(expr, "unknown string escape");
            }
        }
        unsigned char bytes[4];
        int count = 1;
        bytes[0] = (unsigned char)value;
        /* Raw source bytes are already UTF-8. Escaped scalar values above
         * ASCII are encoded explicitly, independently of the C compiler. */
        if(unicode_escape && value >= 128) {
            if(value < 0x800) {
                bytes[0] = 0xc0 | (value >> 6);
                bytes[1] = 0x80 | (value & 63);
                count = 2;
            } else if(value < 0x10000) {
                bytes[0] = 0xe0 | (value >> 12);
                bytes[1] = 0x80 | ((value >> 6) & 63);
                bytes[2] = 0x80 | (value & 63);
                count = 3;
            } else {
                bytes[0] = 0xf0 | (value >> 18);
                bytes[1] = 0x80 | ((value >> 12) & 63);
                bytes[2] = 0x80 | ((value >> 6) & 63);
                bytes[3] = 0x80 | (value & 63);
                count = 4;
            }
        }
        for(int index = 0; index < count; index++) {
            unsigned char byte = bytes[index];
            if(remaining) {
                if((byte & 0xc0) != 0x80) fatal(expr, "string is not valid UTF-8");
                scalar = (scalar << 6) | (byte & 63);
                remaining--;
                if(!remaining && (scalar < minimum || scalar > 0x10ffff ||
                    (scalar >= 0xd800 && scalar <= 0xdfff)))
                    fatal(expr, "string is not valid UTF-8");
            } else if(byte >= 128) {
                if(byte >= 0xc2 && byte <= 0xdf) {
                    remaining = 1;
                    scalar = byte & 31;
                    minimum = 0x80;
                } else if(byte >= 0xe0 && byte <= 0xef) {
                    remaining = 2;
                    scalar = byte & 15;
                    minimum = 0x800;
                } else if(byte >= 0xf0 && byte <= 0xf4) {
                    remaining = 3;
                    scalar = byte & 7;
                    minimum = 0x10000;
                } else {
                    fatal(expr, "string is not valid UTF-8");
                }
            }
            if(used + 8 >= size) fatal(expr, "string literal exceeds output limit");
            if(target == KIR_C || target == KIR_CPP)
                used += (size_t)format(out + used, size - used, "\\%03o", byte);
            else if(byte < 32 || byte == '"' || byte == '\\' || byte == 127)
                used += (size_t)format(out + used, size - used, "\\u%04x", byte);
            else
                out[used++] = (char)byte;
        }
    }
    if(*cursor != '"') fatal(expr, "unterminated string literal");
    if(remaining) fatal(expr, "string is not valid UTF-8");
    out[used++] = '"';
    out[used] = 0;
}

int
KirScalarLiteral(const char *type, const char *text, KirTarget target,
                  KirSourceSpan span, char *out, size_t size)
{
    KirFunction fn={0};Emitter e={0};int ok=0;
    type=canonical(type);e.target=target;
    if(!*type)return 0;
    if(!strcmp(type,"bool")) {
        if(!strcmp(text,"true") || !strcmp(text,"false")) {kir_copy(out,size,text);return 1;}
        return 0;
    }
    int index=KirParseExpr(&fn,NULL,text,span);
    if(index>=0) {
        KirExpr *expr=&fn.exprs[index];
        if(expr->kind == KIR_EXPR_STRING && !strcmp(type, "string")) {
            char value[KIR_TEXT_MAX];
            string_literal(expr, target, value, sizeof(value));
            if(target == KIR_C || target == KIR_CPP)
                format(out, size, "{%s, sizeof(%s) - 1}", value, value);
            else
                kir_copy(out, size, value);
            ok = 1;
        } else if(expr->kind==KIR_EXPR_INT && width(type)) {
            literal(&e,expr,type,0,out,size);ok=1;
        } else if(expr->kind==KIR_EXPR_UNARY && !strcmp(expr->op,"-") &&
                  fn.exprs[expr->right].kind==KIR_EXPR_INT && width(type)) {
            literal(&e,&fn.exprs[expr->right],type,1,out,size);ok=1;
        } else if((expr->kind==KIR_EXPR_FLOAT || expr->kind==KIR_EXPR_INT) && type[0]=='f') {
            kir_copy(out,size,text);size_t n=strlen(out);
            if(n && (out[n-1]=='f'||out[n-1]=='F'))out[n-1]=0;
            ok=1;
        }
    }
    free(fn.exprs);return ok;
}

static void
emit_call(Emitter *e, const KirExpr *expr, char *out, size_t size)
{
    char text[KIR_TEXT_MAX];
    int count=0;
    size_t n=(size_t)format(text,sizeof(text),"%s(",expr->name);
    for(int child=expr->first_child;child>=0;child=e->fn->exprs[child].next_sibling) {
        char argument[KIR_TEXT_MAX];
        emit_expr(e,child,e->fn->exprs[child].type,argument,sizeof(argument));
        /* emit_expr already captures each result before the next argument.
         * A second capture only copies the same scalar or owned record. */
        n+=(size_t)format(text+n,sizeof(text)-n,"%s%s",count?",":"",argument);
        count++;
    }
    format(text+n,sizeof(text)-n,")");
    e->resolve(e->context,text,out,size);
}

static int
member_path(const KirFunction *fn, int index)
{
    const KirExpr *expr = &fn->exprs[index];
    if(expr->kind == KIR_EXPR_IDENT)
        return 1;
    return expr->kind == KIR_EXPR_MEMBER && member_path(fn, expr->left);
}

static void
emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size)
{
    const KirExpr *expr=&e->fn->exprs[index];
    const char *type=canonical(expr->type);
    char a[KIR_TEXT_MAX],b[KIR_TEXT_MAX],result[KIR_TEXT_MAX],temp[KIR_NAME_MAX];
    if(!strcmp(expr->type,"integer") || !strcmp(expr->type,"real")) {
        const char *want=canonical(expected); if(*want && strcmp(want,"bool") && strcmp(want,"void")) type=want;
    }
    switch(expr->kind) {
    case KIR_EXPR_COMPOUND: {
        zero_record(e, type, a, sizeof(a));
        fresh(e, temp);
        declare(e, temp, type, a);
        for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
            const KirExpr *field = &e->fn->exprs[child];
            char field_name[KIR_NAME_MAX];
            if(e->target == KIR_GO)
                kir_go_field_ident(field->name, field_name, sizeof(field_name));
            else
                kir_copy(field_name, sizeof(field_name), field->name);
            emit_expr(e, field->right, field->type, a, sizeof(a));
            line(e, "%s.%s = %s%s", temp, field_name, a, e->target == KIR_GO ? "" : ";");
        }
        kir_copy(out, size, temp);
        return;
    }
    case KIR_EXPR_MEMBER: {
        char field[KIR_NAME_MAX];
        /* Reading a field needs a snapshot of that field, not a copy of every
         * enclosing record. Calls and other computed bases still evaluate once. */
        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, a, sizeof(a));
        else
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, a, sizeof(a));
        if(e->target == KIR_GO) kir_go_field_ident(expr->name, field, sizeof(field));
        else kir_copy(field, sizeof(field), expr->name);
        format(result, sizeof(result), "%s.%s", a, field);
        break;
    }
    case KIR_EXPR_IDENT:
        resolve(e, expr->name, result, sizeof(result));
        if(e->target == KIR_GO &&
           KirFindRuntimeEnumMember(expr->name) != NULL) {
            const char *target = KirTargetType(type, KIR_GO);
            kir_copy(a, sizeof(a), result);
            format(result, sizeof(result), "%s(%s)", target != NULL ? target : type, a);
        }
        break;
    case KIR_EXPR_STRING:
        string_literal(expr, e->target, a, sizeof(a));
        if(e->target == KIR_C || e->target == KIR_CPP)
            format(result, sizeof(result), "StringView(%s, sizeof(%s) - 1)", a, a);
        else
            kir_copy(result, sizeof(result), a);
        break;
    case KIR_EXPR_INT: literal(e,expr,type,0,result,sizeof(result));break;
    case KIR_EXPR_FLOAT: {
        kir_copy(result,sizeof(result),expr->text);size_t n=strlen(result);
        if(n && (result[n-1]=='f' || result[n-1]=='F')) result[n-1]=0;
        break;
    }
    case KIR_EXPR_CALL:
        emit_call(e,expr,result,sizeof(result));
        if(!strcmp(type,"void")) {line(e,"%s%s",result,e->target==KIR_GO?"":";");out[0]=0;return;}
        break;
    case KIR_EXPR_CONDITIONAL:
        emit_expr(e,expr->left,"bool",a,sizeof(a));fresh(e,temp);
        if(record_type(e->module, type))
            zero_record(e, type, b, sizeof(b));
        else
            kir_copy(b, sizeof(b), zero_value(type, e->target));
        declare(e,temp,type,b);
        line(e,e->target==KIR_GO?"if %s {":"if (%s) {",a);e->indent++;
        emit_expr(e,expr->right,type,b,sizeof(b));line(e,"%s = %s%s",temp,b,e->target==KIR_GO?"":";");e->indent--;
        line(e,"} else {");e->indent++;emit_expr(e,expr->third,type,b,sizeof(b));
        line(e,"%s = %s%s",temp,b,e->target==KIR_GO?"":";");e->indent--;line(e,"}");kir_copy(out,size,temp);return;
    case KIR_EXPR_BINARY: {
        const char *operand_type=type;
        if(!strcmp(type,"bool")) {
            operand_type=canonical(e->fn->exprs[expr->left].type);
            if(!strcmp(e->fn->exprs[expr->left].type,"integer") || !strcmp(e->fn->exprs[expr->left].type,"real")) operand_type=canonical(e->fn->exprs[expr->right].type);
        }
        emit_expr(e,expr->left,operand_type,a,sizeof(a));
        if(!strcmp(expr->op,"&&") || !strcmp(expr->op,"||")) {
            fresh(e,temp);declare(e,temp,"bool",a);
            line(e,e->target==KIR_GO?"if %s%s {":"if (%s%s) {",!strcmp(expr->op,"||")?"!":"",temp);e->indent++;
            emit_expr(e,expr->right,"bool",b,sizeof(b));line(e,"%s = %s%s",temp,b,e->target==KIR_GO?"":";");
            e->indent--;line(e,"}");kir_copy(out,size,temp);return;
        }
        emit_expr(e,expr->right,(!strcmp(expr->op,"<<")||!strcmp(expr->op,">>"))?"i32":operand_type,b,sizeof(b));
        if(!strcmp(operand_type, "string") && (e->target == KIR_C || e->target == KIR_CPP))
            format(result, sizeof(result), "%sStringEqual(%s, %s)", !strcmp(expr->op, "!=") ? "!" : "", a, b);
        else if(width(type) && operation(expr->op)) number(e,type,a,b,operation(expr->op),result,sizeof(result));
        else format(result,sizeof(result),"%s %s %s",a,expr->op,b);
        break;
    }
    case KIR_EXPR_UNARY:
        if(!strcmp(expr->op,"-") && e->fn->exprs[expr->right].kind==KIR_EXPR_INT) {
            literal(e,&e->fn->exprs[expr->right],type,1,result,sizeof(result));break;
        }
        if(!strcmp(expr->op,"++") || !strcmp(expr->op,"--")) {
            emit_destination(e, expr->right, a, sizeof(a));
            if(width(type)) number(e,type,a,"1",expr->op[0]=='+'?1:2,result,sizeof(result));
            else format(result,sizeof(result),e->target==KIR_JS&&!strcmp(type,"f32")?"Math.fround(%s %c 1)":"(%s %c 1)",a,expr->op[0]);
            line(e,"%s = %s%s",a,result,e->target==KIR_GO?"":";");
            kir_copy(result,sizeof(result),a);break;
        }
        emit_expr(e,expr->right,type,a,sizeof(a));
        if(width(type) && !strcmp(expr->op,"-")) number(e,type,"0",a,2,result,sizeof(result));
        else if(width(type) && !strcmp(expr->op,"~")) {
            number(e,type,a,e->target==KIR_GO?"^uint64(0)":e->target==KIR_JS?"-1n":"UINT64_MAX",10,result,sizeof(result));
        } else format(result,sizeof(result),"%s%s",expr->op,a);
        break;
    case KIR_EXPR_POSTFIX:
        emit_destination(e, expr->left, a, sizeof(a));fresh(e,temp);declare(e,temp,type,a);
        if(width(type)) number(e,type,a,"1",expr->op[0]=='+'?1:2,result,sizeof(result));
        else format(result,sizeof(result),e->target==KIR_JS&&!strcmp(type,"f32")?"Math.fround(%s %c 1)":"(%s %c 1)",a,expr->op[0]);
        line(e,"%s = %s%s",a,result,e->target==KIR_GO?"":";");kir_copy(out,size,temp);return;
    case KIR_EXPR_CAST: {
        const char *declared_type = type;
        if(enum_type(e->module, type))
            type = "i32";
        emit_expr(e,expr->right,e->fn->exprs[expr->right].type,a,sizeof(a));
        if(!strcmp(type, "string")) {
            kir_copy(result, sizeof(result), a);
        } else if(!strcmp(type,"bool")) {
            format(result,sizeof(result),"%s != %s",a,!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")?"false":"0");
        } else if(!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")) {
            fresh(e,temp);declare(e,temp,type,"0");
            line(e,e->target==KIR_GO?"if %s {":"if (%s) {",a);e->indent++;
            line(e,"%s = %s%s",temp,e->target==KIR_JS&&width(type)>32?"1n":"1",e->target==KIR_GO?"":";");
            e->indent--;
            line(e, "}");
            kir_copy(result, sizeof(result), temp);
        } else if(width(type)) {
            if(canonical(e->fn->exprs[expr->right].type)[0]=='f') {
                if(e->target==KIR_GO) format(b,sizeof(b),"%s_float(float64(%s),%d,%s)",e->numbers,a,width(type),signed_type(type)?"true":"false");
                else format(b,sizeof(b),"%s_float(%s,%d,%s)",e->numbers,a,width(type),signed_type(type)?"true":"false");
                number(e,type,b,"0",0,result,sizeof(result));
            } else number(e,type,a,"0",0,result,sizeof(result));
        }
        else if(e->target==KIR_JS) format(result,sizeof(result),"%s(%s)",!strcmp(type,"f32")?"Math.fround":!strcmp(type,"bool")?"Boolean":"Number",a);
        else if(e->target==KIR_GO) format(result,sizeof(result),"%s(%s)",KirTargetType(type,e->target),a);
        else format(result,sizeof(result),"(%s)(%s)",KirTargetType(type,e->target),a);
        type = declared_type;
        break;
    }
    default: fatal(expr,"unsupported structured expression");
    }
    if(e->target==KIR_JS && !strcmp(type,"f32")) {kir_copy(a,sizeof(a),result);format(result,sizeof(result),"Math.fround(%s)",a);}
    fresh(e,temp);declare(e,temp,type,result);kir_copy(out,size,temp);
}

static int
block_end(const KirFunction *fn,int begin,int end)
{
    int depth=1;
    for(int i=begin+1;i<end;i++) {
        KirStmtKind k=fn->stmts[i].kind;
        if(k==KIR_STMT_IF||k==KIR_STMT_WHILE||k==KIR_STMT_BLOCK_OPEN)depth++;
        if(k==KIR_STMT_BLOCK_CLOSE && !--depth)return i;
    }
    return end;
}

static void emit_sequence(Emitter *e,int begin,int end);

static void
zero_record(Emitter *e, const char *type, char *out, size_t size)
{
    if(e->target == KIR_GO) {
        format(out, size, "%s{}", type);
        return;
    }
    if(e->target != KIR_JS) {
        kir_copy(out, size, e->target == KIR_CPP ? "{}" : "{0}");
        return;
    }
    js_record_fields(e, e->module, type, NULL, out, size);
}

static int
emit_if(Emitter *e,int i,int end)
{
    char cond[KIR_TEXT_MAX];
    int close=block_end(e->fn,i,end);
    emit_expr(e,e->fn->stmts[i].expr_root,"bool",cond,sizeof(cond));
    line(e,e->target==KIR_GO?"if %s {":"if (%s) {",cond);e->indent++;
    emit_sequence(e,i+1,close);e->indent--;
    if(close+1<end && e->fn->stmts[close+1].kind==KIR_STMT_IF && !strncmp(e->fn->stmts[close+1].text,"else",4)) {
        int next=close+1;
        line(e,"} else {");e->indent++;
        if(e->fn->stmts[next].expr_root>=0)close=emit_if(e,next,end);
        else {close=block_end(e->fn,next,end);emit_sequence(e,next+1,close);}
        e->indent--;
    }
    line(e,"}");return close;
}

static void
emit_sequence(Emitter *e,int begin,int end)
{
    int saved=e->local_count;e->depth++;
    for(int i=begin;i<end;i++) {
        const KirStmt *st=&e->fn->stmts[i];
        char value[KIR_TEXT_MAX],lhs[KIR_TEXT_MAX],result[KIR_TEXT_MAX];
        switch(st->kind) {
        case KIR_STMT_DECL:
            if(st->is_instance) {
                const KirExpr *key = &e->fn->exprs[st->expr_root];
                emit_expr(e, st->expr_root, key->kind == KIR_EXPR_INT ? "u64" : "i64",
                          value, sizeof(value));
                if(e->target == KIR_GO && *e->instance_host)
                    line(e, "%s := instanceState[%s](%s, uint64(%s))",
                         st->name, st->type, e->instance_host, value);
                else if(e->target == KIR_GO)
                    line(e, "%s := kryon.InstanceState[%s](uint64(%s))",
                         st->name, st->type, value);
                else if(e->target == KIR_JS) {
                    for(int depth = 0; depth < e->indent; depth++)
                        fputs("    ", e->out);
                    fprintf(e->out, "const %s = kryon.instanceState($rt, \"%s\", %s, () => (",
                            st->name, st->type, value);
                    KirEmitJsRecordValue(e->out, e->module, st->type, NULL);
                    fputs("));\n", e->out);
                } else
                    line(e, "%s *%s = (%s *)InstanceState(\"%s\", (uint64_t)%s, sizeof(%s));",
                         st->type, st->name, st->type, st->type, value, st->type);
            } else if(st->expr_root>=0)emit_expr(e,st->expr_root,st->type,value,sizeof(value));
            else if(record_type(e->module, st->type)) {
                zero_record(e, st->type, value, sizeof(value));
            }
            else kir_copy(value, sizeof(value), zero_value(st->type, e->target));
            if(!st->is_instance)
                declare(e,st->name,st->type,value);
            e->locals[e->local_count].is_instance = st->is_instance;
            kir_copy(e->locals[e->local_count].name,KIR_NAME_MAX,st->name);e->locals[e->local_count++].depth=e->depth;
            break;
        case KIR_STMT_ASSIGN:
            emit_destination(e, st->lhs_root, lhs, sizeof(lhs));
            if(strcmp(st->assignment_op,"=")) {
                char old[KIR_NAME_MAX];fresh(e,old);
                declare(e,old,e->fn->exprs[st->lhs_root].type,lhs);
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                char op[4];kir_copy(op,sizeof(op),st->assignment_op);op[strlen(op)-1]=0;
                const char *type=canonical(e->fn->exprs[st->lhs_root].type);
                if(width(type))number(e,type,old,value,operation(op),result,sizeof(result));
                else format(result,sizeof(result),e->target==KIR_JS&&!strcmp(type,"f32")?"Math.fround(%s %s %s)":"%s %s %s",old,op,value);
            } else {
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                kir_copy(result,sizeof(result),value);
            }
            if(e->target == KIR_JS && record_type(e->module, e->fn->exprs[st->lhs_root].type)) {
                char copy[KIR_TEXT_MAX];
                js_record_copy(e, e->fn->exprs[st->lhs_root].type, result, copy, sizeof(copy));
                line(e, "%s = %s;", lhs, copy);
            }
            else
                line(e,"%s = %s%s",lhs,result,e->target==KIR_GO?"":";");
            break;
        case KIR_STMT_RETURN:
            if(st->expr_root>=0) {emit_expr(e,st->expr_root,e->fn->return_type,value,sizeof(value));line(e,"return %s%s",value,e->target==KIR_GO?"":";");}
            else line(e,e->target==KIR_GO?"return":"return;");
            e->local_count=saved;e->depth--;return;
        case KIR_STMT_IF:i=emit_if(e,i,end);break;
        case KIR_STMT_WHILE: {
            int close=block_end(e->fn,i,end);
            line(e,e->target==KIR_GO?"for {":"while (true) {");e->indent++;
            emit_expr(e,st->expr_root,"bool",value,sizeof(value));
            line(e,e->target==KIR_GO?"if !%s { break }":"if (!%s) { break; }",value);
            emit_sequence(e,i+1,close);e->indent--;line(e,"}");i=close;break;
        }
        case KIR_STMT_BLOCK_OPEN: {
            int close=block_end(e->fn,i,end);line(e,"{");e->indent++;emit_sequence(e,i+1,close);e->indent--;line(e,"}");i=close;break;
        }
        case KIR_STMT_BREAK:case KIR_STMT_CONTINUE:
            line(e,"%s%s",st->kind==KIR_STMT_BREAK?"break":"continue",e->target==KIR_GO?"":";");
            e->local_count=saved;e->depth--;return;
        case KIR_STMT_EXPR:case KIR_STMT_UNUSED:
            if(st->expr_root>=0) {
                emit_expr(e,st->expr_root,e->fn->exprs[st->expr_root].type,value,sizeof(value));
                if(*value)line(e,e->target==KIR_GO?"_ = %s":e->target==KIR_JS?"void %s;":"(void)%s;",value);
            }break;
        default:break;
        }
    }
    e->local_count=saved;e->depth--;
}

int
KirEmitBody(FILE *out,const KirModule *module,const KirFunction *fn,KirTarget target,
            KirResolveTarget resolver,void *context,const char *instance_host)
{
    Emitter e={0};char params[64][KIR_TEXT_MAX];int count;
    if(!KirCanEmitBody(module, fn))return 0;
    e.out=out;e.module=module;e.fn=fn;e.target=target;e.resolve=resolver;e.context=context;e.indent=1;
    e.instance_host = instance_host;
    e.locals=calloc((size_t)fn->stmt_count+65,sizeof(*e.locals));
    if(!e.locals) { fprintf(stderr,"out of memory during scalar emission\n"); exit(1); }
    number_prefix(module,e.numbers,sizeof(e.numbers));
    count=*kir_skip_ws(fn->args)?kir_split_top(fn->args,params[0],64,sizeof(params[0])):0;
    for(int i=0;i<count;i++) {
        char *colon=strchr(params[i],':');*colon++=0;kir_trim_in_place(params[i]);
        kir_copy(e.locals[e.local_count++].name,KIR_NAME_MAX,params[i]);
        const char *type=canonical(kir_skip_ws(colon));
        if(target == KIR_JS && enum_type(module, type))
            line(&e, "%s = %s_value(%s,32,true);", params[i], e.numbers, params[i]);
        if(target==KIR_JS && width(type))line(&e,"%s = %s_value(%s,%d,%s);",params[i],e.numbers,params[i],width(type),signed_type(type)?"true":"false");
        if(target==KIR_JS && !strcmp(type,"bool"))line(&e,"%s = %s_bool(%s);",params[i],e.numbers,params[i]);
        if(target==KIR_JS && !strcmp(type,"f32"))line(&e,"%s = Math.fround(%s);",params[i],params[i]);
        if(target == KIR_JS && record_type(module, type)) {
            char copy[KIR_TEXT_MAX];
            js_record_copy(&e, type, params[i], copy, sizeof(copy));
            line(&e, "%s = %s;", params[i], copy);
        }
    }
    emit_sequence(&e,0,fn->stmt_count);free(e.locals);return 1;
}
