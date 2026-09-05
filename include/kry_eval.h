#ifndef KRY_EVAL_H
#define KRY_EVAL_H

/*
 * kry_eval.h: spreadsheet-style evaluation primitives shared by
 * formula engines — gnumeric-compatible cell addressing and the
 * ulp-grace rounding family (fake floor/round/trunc/ceil and the
 * nextafter-style epsilon nudges).
 *
 * Plain C99 on libm only.
 */

/* Column index to A1 letters: 0 -> "A", 25 -> "Z", 26 -> "AA".  Writes a
 * NUL-terminated string to out (at least 8 bytes) and returns its length. */
int  eval_col_name (int col, char *out);

/* Whether a sheet name needs 'quoting' in a reference. */
int  eval_sheet_name_needs_quote (const char *name);

/* Emit the sheet prefix including the trailing separator: name! or
 * 'name'! (empty name yields just "!").  Returns bytes written. */
int  eval_sheet_prefix (const char *name, char *out);

/* The gnumeric-compatible ulp-grace rounding family: these treat values
 * within one ulp of an integer as that integer. */
double eval_fake_floor (double x);
double eval_fake_round (double x);
double eval_fake_trunc (double x);
double eval_fake_ceil  (double x);

/* One-ulp nudges away from/toward zero (nextafter equivalents built on
 * frexp/ldexp so no C99 nextafter dependency is needed). */
double eval_add_epsilon (double x);
double eval_sub_epsilon (double x);

#endif /* KRY_EVAL_H */
