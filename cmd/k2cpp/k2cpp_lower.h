#ifndef K2CPP_LOWER_H
#define K2CPP_LOWER_H

#include "kir.h"

/* Cross-module symbol table: for each parsed module (by its slash-path),
 * every function's .kry name -> full C name. Built by main after parsing all
 * inputs; used to resolve alias-qualified calls (start.draw_start_page(...)
 * -> ide_start_page_draw_start_page_kry_draw(...)). */
typedef struct K2cppModuleSyms {
    char module_stem[KIR_PATH_MAX];  /* source path minus .kry */
        char module_slash[KIR_PATH_MAX];
    struct {
        char kry[KIR_NAME_MAX];
        char c[KIR_NAME_MAX * 3];
    } fns[256];
    int fn_count;
} K2cppModuleSyms;

/* Lower a KirProgram to C++ source (.cpp/.hpp). restab/restab_count resolve
 * cross-module calls; pass NULL/0 to resolve same-module only. */
void k2cpp_lower(const KirProgram *program, const char *root,
               const char *out_dir, const K2cppModuleSyms *restab,
               int restab_count);

/* Build the symbol table entry for one program into out. */
void k2cpp_build_syms(const KirProgram *program, K2cppModuleSyms *out);

/* Full C name for a function (module prefix, colon naming, _kry_draw). */
void k2cpp_function_c_name(const KirModule *m, const KirFunction *fn,
                         char *dst, size_t dst_size);

/* k2cpp_project.c — kryon_project.h/.c (app-host ABI) after all files lower. */
void k2cpp_write_project(KirProgram *const *progs, int prog_count,
                       const char *root, const char *out_dir, int no_main);

#endif
