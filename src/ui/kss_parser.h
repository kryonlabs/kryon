#ifndef KRYON_KSS_PARSER_H
#define KRYON_KSS_PARSER_H

#include "ui_style_sheet.h"

#include <stdbool.h>
#include <stddef.h>

#define KSS_VARIANT_MAX 8

typedef struct KssVariantInfo {
    char name[64];
    char label[64];
} KssVariantInfo;

typedef struct KssParseResult {
    char pack_id[64];
    int rule_count;
    KssVariantInfo variants[KSS_VARIANT_MAX];
    int variant_count;
} KssParseResult;

typedef struct KssRuleSource {
    char file[96];
    int file_index;
    int line;
    int column;
    int selector_start;
    int selector_length;
    int body_start;
    int body_length;
    int group;
    int end;
} KssRuleSource;

/* Importable in-memory KSS sources for '@import <id>;'. */
bool RegisterStyleModule(const char *id, const char *source);
void ClearStyleModules(void);
int GetStyleModuleCount(void);
int KssParseInvocationCount(void);
void KssResetParseInvocationCount(void);

bool kss_parse_variant(const char *source, const StyleColorToken *colors,
                       int color_count, StyleRule *rules, int rule_capacity,
                       KssParseResult *result, char *diagnostic,
                       size_t diagnostic_size);

bool kss_parse_string(const char *source, StyleRule *rules, int rule_capacity,
                      KssParseResult *result, char *diagnostic,
                      size_t diagnostic_size);

/* Parse and retain inspector source provenance for each rule. `sources` is
 * parallel to `rules` and receives file name, line/column, and selector/body
 * spans for every emitted rule. */
bool kss_parse_trace_string(const char *source, StyleRule *rules,
                            KssRuleSource *sources, int rule_capacity,
                            KssParseResult *result, char *diagnostic,
                            size_t diagnostic_size);

/* Format KSS source through the shared formatter: stable re-layout with
 * verbatim construct text and preserved comments. Returns the formatted
 * length (out holds a NUL-terminated string), or -1 with diagnostic set. */
int kss_format_string(const char *source, char *out, size_t out_size,
                      char *diagnostic, size_t diagnostic_size);

/* Parse with a declared pack variant active ('@variant name ...' blocks yield
 * rules and overlays); variant may be NULL or empty for the base sheet. */
bool kss_parse_with_variant(const char *source, const char *variant,
                            StyleRule *rules, int rule_capacity,
                            KssParseResult *result, char *diagnostic,
                            size_t diagnostic_size);

/* Parse with a variant and a theme ("light", "dark", or empty) applied to
 * the shared parse environment. */
bool kss_parse_with_environment(const char *source, const char *variant,
                                const char *theme, StyleRule *rules,
                                int rule_capacity, KssParseResult *result,
                                char *diagnostic, size_t diagnostic_size);

#endif /* KRYON_KSS_PARSER_H */
