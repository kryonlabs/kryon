#ifndef KRYON_KIR_TEXT_H
#define KRYON_KIR_TEXT_H

#include <stddef.h>

int kir_is_ident_char(int c);
const char *kir_skip_ws(const char *s);
const char *kir_skip_inline_ws(const char *s);
char *kir_trim(char *s);
char *kir_trim_in_place(char *s);
void kir_strip_block_brace(char *s);
void kir_camel_ident(const char *s, char *dst, size_t dst_size);
int kir_split_top(const char *s, char *parts, int max, size_t part_size);

#endif /* KRYON_KIR_TEXT_H */
