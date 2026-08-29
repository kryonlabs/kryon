#ifndef K2JS_LOWER_H
#define K2JS_LOWER_H

#include "kir.h"

/*
 * k2js_lower - Kir -> JavaScript backend.
 *
 * Emits one ESM .js file per module. Generated code imports the small
 * web/kryon-runtime.js recorder/runtime and exports createState(), app
 * metadata, all non-extern functions, frame(), and optionally main().
 */
int k2js_lower(const KirProgram *const *progs, int prog_count,
               const char *root, const char *out_dir,
               const char *runtime_import, int no_main);

#endif
