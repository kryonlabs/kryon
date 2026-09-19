#include "runtime/kss_formatter.h"
#include "kss_parser.h"

#include "runtime/kss_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Thin host shim over the shared KSS runtime module (runtime/kss_parser.kry).
 * This file owns native string views, import module registration, and the
 * public parse entry points; the grammar itself exists once in .kry. */

#define KSS_MODULE_MAX 32

typedef struct KssModule {
    char id[64];
    char *source;
} KssModule;

static KssModule kss_modules[KSS_MODULE_MAX];
static int kss_module_count;
static int kss_parse_invocation_count;

String
StringSlice(String source, int32_t start, int32_t length)
{
    if(start < 0 || length < 0 ||
       (size_t)start + (size_t)length > source.length)
        return StringView(NULL, 0);
    return StringView(source.data + start, (size_t)length);
}

bool
RegisterStyleModule(const char *id, const char *source)
{
    KssModule *slot;
    size_t length;

    if(id == NULL || source == NULL || id[0] == '\0')
        return false;
    slot = NULL;
    for(int i = 0; i < kss_module_count; i++)
        if(strcmp(kss_modules[i].id, id) == 0)
            slot = &kss_modules[i];
    if(slot == NULL) {
        if(kss_module_count >= KSS_MODULE_MAX)
            return false;
        slot = &kss_modules[kss_module_count++];
        snprintf(slot->id, sizeof(slot->id), "%s", id);
    } else {
        free(slot->source);
    }
    length = strlen(source);
    slot->source = (char *)malloc(length + 1);
    if(slot->source == NULL)
        return false;
    memcpy(slot->source, source, length + 1);
    return true;
}

void
ClearStyleModules(void)
{
    for(int i = 0; i < kss_module_count; i++)
        free(kss_modules[i].source);
    memset(kss_modules, 0, sizeof(kss_modules));
    kss_module_count = 0;
}

int
GetStyleModuleCount(void)
{
    return kss_module_count;
}

static const char *
kss_module_source(const KssName *name)
{
    char id[64];
    int length = name->length;

    if(length <= 0 || (size_t)length >= sizeof(id))
        return NULL;
    memcpy(id, name->bytes, (size_t)length);
    id[length] = '\0';
    for(int i = 0; i < kss_module_count; i++)
        if(strcmp(kss_modules[i].id, id) == 0)
            return kss_modules[i].source;
    return NULL;
}


static void
kss_copy_file_name(char *out, size_t out_size, const KssSourceFile *file)
{
    int length;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(file == NULL)
        return;
    length = file->length;
    if(length < 0)
        length = 0;
    if((size_t)length >= out_size)
        length = (int)out_size - 1;
    memcpy(out, file->name, (size_t)length);
    out[length] = '\0';
}

static void
kss_copy_rule_source(KssRuleSource *source, const KssParser *parser,
                     const KssOrigin *origin, const KssRuleSpan *span)
{
    int file_index;

    if(source == NULL || parser == NULL || origin == NULL || span == NULL)
        return;
    memset(source, 0, sizeof(*source));
    file_index = span->file;
    source->file_index = file_index;
    source->line = origin->line;
    source->column = origin->column;
    source->selector_start = span->selector_start;
    source->selector_length = span->selector_length;
    source->body_start = span->body_start;
    source->body_length = span->body_length;
    source->group = span->group;
    source->end = span->end;
    if(file_index >= 0 && file_index < parser->file_count)
        kss_copy_file_name(source->file, sizeof(source->file),
                           &parser->files[file_index]);
}

static void
kss_copy_diagnostic(char *diagnostic, size_t diagnostic_size,
                    const KssParser *parser)
{
    int length;

    if(diagnostic == NULL || diagnostic_size == 0)
        return;
    length = parser->diagnostic_length;
    if(length < 0)
        length = 0;
    if((size_t)length >= diagnostic_size)
        length = (int)diagnostic_size - 1;
    memcpy(diagnostic, parser->diagnostic, (size_t)length);
    diagnostic[length] = '\0';
}

int
KssParseInvocationCount(void)
{
    return kss_parse_invocation_count;
}

void
KssResetParseInvocationCount(void)
{
    kss_parse_invocation_count = 0;
}

static bool
kss_collect(const char *source, const StyleColorToken *colors, int color_count,
            const char *variant, const char *theme, StyleRule *rules,
            KssRuleSource *sources, int rule_capacity, KssParseResult *result,
            char *diagnostic, size_t diagnostic_size)
{
    KssEnvironment environment = KssDefaultEnvironment();
    String empty = StringView(NULL, 0);
    environment = KssEnvironmentWithNames(environment,
        StringView(theme, theme != NULL ? strlen(theme) : 0),
        empty, empty, empty, empty,
        StringView(variant, variant != NULL ? strlen(variant) : 0));
    KssParser parser = KssBegin(StringView(source, strlen(source)),
                                StringView(NULL, 0),
                                environment);

    int rule_count = 0;

    kss_parse_invocation_count++;
    if(diagnostic != NULL && diagnostic_size > 0)
        diagnostic[0] = '\0';
    for(int i = 0; i < color_count; i++)
        parser = KssAddColorOverride(parser,
                                     StringView(colors[i].name,
                                                strlen(colors[i].name)),
                                     colors[i].color);
    for(;;) {
        if(parser.status == KssStatusRule) {
            if(rule_count >= rule_capacity) {
                if(diagnostic != NULL && diagnostic_size > 0)
                    snprintf(diagnostic, diagnostic_size,
                             "style rule capacity exceeded");
                return false;
            }
            rules[rule_count] = parser.rule;
            if(sources != NULL)
                kss_copy_rule_source(&sources[rule_count], &parser,
                                     &parser.origin, &parser.rule_span);
            rule_count++;
            parser.status = KssStatusContinue;
            continue;
        }
        if(parser.status != KssStatusContinue)
            break;
        parser = KssStep(parser);
        if(parser.status == KssStatusNeedImport) {
            const char *module = kss_module_source(&parser.pending_import);
            if(module != NULL) {
                char id[96];
                int length = parser.pending_import.length;
                if(length > 95)
                    length = 95;
                memcpy(id, parser.pending_import.bytes, (size_t)length);
                id[length] = '\0';
                parser = KssProvideImport(parser,
                                          StringView(module, strlen(module)),
                                          StringView(id, (size_t)length));
            } else {
                parser = KssFailImport(parser);
            }
        }
    }
    if(parser.status != KssStatusDone) {
        kss_copy_diagnostic(diagnostic, diagnostic_size, &parser);
        return false;
    }
    if(result != NULL) {
        int length = parser.pack.length;
        result->rule_count = rule_count;
        result->variant_count = parser.variant_count;
        for(int i = 0; i < parser.variant_count && i < KSS_VARIANT_MAX; i++) {
            int name_length = parser.variants[i].name.length;
            int label_length = parser.variants[i].label.length;
            if(name_length < 0)
                name_length = 0;
            if((size_t)name_length >= sizeof(result->variants[i].name))
                name_length = (int)sizeof(result->variants[i].name) - 1;
            if(label_length < 0)
                label_length = 0;
            if((size_t)label_length >= sizeof(result->variants[i].label))
                label_length = (int)sizeof(result->variants[i].label) - 1;
            memcpy(result->variants[i].name, parser.variants[i].name.bytes,
                   (size_t)name_length);
            result->variants[i].name[name_length] = '\0';
            memcpy(result->variants[i].label, parser.variants[i].label.bytes,
                   (size_t)label_length);
            result->variants[i].label[label_length] = '\0';
        }
        if(length < 0)
            length = 0;
        if((size_t)length >= sizeof(result->pack_id))
            length = (int)sizeof(result->pack_id) - 1;
        memcpy(result->pack_id, parser.pack.bytes, (size_t)length);
        result->pack_id[length] = '\0';
    }
    return true;
}

bool
kss_parse_variant(const char *source, const StyleColorToken *colors,
                  int color_count, StyleRule *rules, int rule_capacity,
                  KssParseResult *result, char *diagnostic,
                  size_t diagnostic_size)
{
    if(source == NULL || rules == NULL || rule_capacity < 0)
        return false;
    if(color_count < 0 || (color_count > 0 && colors == NULL))
        return false;
    return kss_collect(source, colors, color_count, NULL, NULL, rules, NULL,
                       rule_capacity, result, diagnostic, diagnostic_size);
}

bool
kss_parse_string(const char *source, StyleRule *rules, int rule_capacity,
                 KssParseResult *result, char *diagnostic,
                 size_t diagnostic_size)
{
    return kss_parse_variant(source, NULL, 0, rules, rule_capacity, result,
                             diagnostic, diagnostic_size);
}

bool
kss_parse_trace_string(const char *source, StyleRule *rules,
                       KssRuleSource *sources, int rule_capacity,
                       KssParseResult *result, char *diagnostic,
                       size_t diagnostic_size)
{
    if(source == NULL || rules == NULL || sources == NULL || rule_capacity < 0)
        return false;
    return kss_collect(source, NULL, 0, NULL, NULL, rules, sources,
                       rule_capacity, result, diagnostic, diagnostic_size);
}

bool
kss_parse_with_variant(const char *source, const char *variant,
                        StyleRule *rules, int rule_capacity,
                        KssParseResult *result, char *diagnostic,
                        size_t diagnostic_size)
{
    return kss_parse_with_environment(source, variant, NULL, rules,
                                     rule_capacity, result, diagnostic,
                                     diagnostic_size);
}

bool
kss_parse_with_environment(const char *source, const char *variant,
                            const char *theme, StyleRule *rules,
                            int rule_capacity, KssParseResult *result,
                            char *diagnostic, size_t diagnostic_size)
{
    if(source == NULL || rules == NULL || rule_capacity < 0)
        return false;
    return kss_collect(source, NULL, 0, variant, theme, rules, NULL,
                       rule_capacity, result, diagnostic, diagnostic_size);
}

int
kss_format_string(const char *source, char *out, size_t out_size,
                  char *diagnostic, size_t diagnostic_size)
{
    KssFormatResult result;
    size_t length = 0;

    if(source == NULL || out == NULL || out_size == 0)
        return -1;
    if(diagnostic != NULL && diagnostic_size > 0)
        diagnostic[0] = '\0';
    result = KssFormat(StringView(source, strlen(source)));
    if(!result.ok) {
        int count = result.diagnostic_length;
        if(diagnostic != NULL && diagnostic_size > 0) {
            if(count >= (int)diagnostic_size)
                count = (int)diagnostic_size - 1;
            if(count < 0)
                count = 0;
            memcpy(diagnostic, result.diagnostic, (size_t)count);
            diagnostic[count] = '\0';
        }
        return -1;
    }
    for(int i = 0; i < result.count; i++) {
        const KssFormatSegment *segment = &result.segments[i];
        size_t need;

        if(segment->kind == 0)
            need = (size_t)segment->length;
        else if(segment->kind == 1)
            need = 1;
        else
            need = 1 + (size_t)segment->length;
        if(length + need + 1 > out_size)
            return -1;
        if(segment->kind == 0) {
            memcpy(out + length, source + segment->start, (size_t)segment->length);
            length += (size_t)segment->length;
        } else if(segment->kind == 1) {
            out[length++] = (char)segment->atom;
        } else {
            out[length++] = '\n';
            for(int space = 0; space < segment->length; space++)
                out[length++] = ' ';
        }
    }
    out[length] = '\0';
    return (int)length;
}
