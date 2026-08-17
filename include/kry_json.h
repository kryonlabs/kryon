/*
 * kry_json.h - Kry standard library: JSON parsing and emission.
 *
 * A small recursive-descent parser with no dependencies, plus a growable
 * buffer for emitting compact JSON. Values are plain structs the caller
 * walks directly; there is no DOM mutation — build output with KryJsonBuf,
 * read input with kry_json_parse.
 */
#ifndef KRYON_KRY_JSON_H
#define KRYON_KRY_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KRY_JSON_NULL,
    KRY_JSON_BOOL,
    KRY_JSON_NUMBER,
    KRY_JSON_STRING,
    KRY_JSON_ARRAY,
    KRY_JSON_OBJECT,
} KryJsonType;

typedef struct KryJson KryJson;

struct KryJson {
    KryJsonType type;
    char *string;     /* KRY_JSON_STRING: decoded, NUL-terminated */
    double number;    /* KRY_JSON_NUMBER */
    int boolean;      /* KRY_JSON_BOOL */
    KryJson **items;  /* ARRAY elements / OBJECT values (parallel to keys) */
    char **keys;      /* OBJECT member names */
    int count;        /* ARRAY length / OBJECT member count */
};

/* Parse a complete JSON document. Returns NULL on malformed input. Nesting
 * is capped (64) so hostile input cannot exhaust the C stack. */
KryJson *kry_json_parse(const char *text);

/* Free a value and everything under it. NULL is allowed. */
void kry_json_free(KryJson *v);

/* OBJECT lookup by key (first match). Returns NULL if v is not an object or
 * the key is absent. */
KryJson *kry_json_get(const KryJson *v, const char *key);

/* ARRAY/OBJECT element access. Returns NULL when out of range. */
KryJson *kry_json_at(const KryJson *v, int index);

/* Member name for OBJECT index i (NULL otherwise). */
const char *kry_json_key(const KryJson *v, int index);

/* Leaf accessors: return the value, or the zero fallback when the type
 * does not match. */
KryJsonType kry_json_type(const KryJson *v);
const char *kry_json_string(const KryJson *v);   /* NULL unless STRING */
double kry_json_number(const KryJson *v);        /* 0.0 unless NUMBER */
int kry_json_bool(const KryJson *v);             /* 0 unless BOOL */
int kry_json_count(const KryJson *v);            /* 0 unless ARRAY/OBJECT */

/* --- emitter ------------------------------------------------------------ */

typedef struct {
    char *buf;
    unsigned long len;
    unsigned long cap;
} KryJsonBuf;

/* Append raw text (already-valid JSON fragments, brackets, numbers). */
void kry_json_buf_raw(KryJsonBuf *b, const char *text);

/* Append a quoted, escaped JSON string literal (handles ", \, control
 * characters as \uXXXX). */
void kry_json_buf_str(KryJsonBuf *b, const char *text);

/* Append a number without trailing ".0" for integral values. */
void kry_json_buf_num(KryJsonBuf *b, double value);

/* NUL-terminate and return the buffer (owned by the KryJsonBuf; valid until
 * the next append or free). */
const char *kry_json_buf_finish(KryJsonBuf *b);

/* Release the buffer. Safe on a zeroed KryJsonBuf. */
void kry_json_buf_free(KryJsonBuf *b);

#ifdef __cplusplus
}
#endif

#endif
