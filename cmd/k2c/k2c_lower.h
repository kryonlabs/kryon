#ifndef K2C_LOWER_H
#define K2C_LOWER_H

#include "kir.h"

/* Cross-module symbol table: for each parsed module (by its slash-path),
 * every function's .kry name -> full C name. Built by main after parsing all
 * inputs; used to resolve alias-qualified calls (start.draw_start_page(...)
 * -> ide_start_page_draw_start_page_kry_draw(...)). */
typedef struct K2cModuleSyms {
    char module_slash[KIR_PATH_MAX];
    struct {
        char kry[KIR_NAME_MAX];
        char c[KIR_NAME_MAX * 3];
    } fns[256];
    int fn_count;
} K2cModuleSyms;

/* Lower a KirProgram to C source (.c/.h). restab/restab_count resolve
 * cross-module calls; pass NULL/0 to resolve same-module only. */
void k2c_lower(const KirProgram *program, const char *root,
               const char *out_dir, const K2cModuleSyms *restab,
               int restab_count);

/* Build the symbol table entry for one program into out. */
void k2c_build_syms(const KirProgram *program, K2cModuleSyms *out);

#endif
