#ifndef KRYON_KSS_PARSER_H
#define KRYON_KSS_PARSER_H

#include "ui_style_sheet.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct KssParseResult {
    char pack_id[64];
    int rule_count;
} KssParseResult;

bool kss_parse_string(const char *source, StyleRule *rules, int rule_capacity,
                      KssParseResult *result, char *diagnostic,
                      size_t diagnostic_size);

#endif /* KRYON_KSS_PARSER_H */
