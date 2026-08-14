#ifndef K2C_LOWER_H
#define K2C_LOWER_H

struct KirProgram;

/* Lower a KirProgram to C source (.c/.h). This is the Kir→C backend,
 * gated by the --kir flag. Not yet byte-identical to the legacy kc path. */
void k2c_lower(const struct KirProgram *program, const char *root,
               const char *out_dir);

#endif
