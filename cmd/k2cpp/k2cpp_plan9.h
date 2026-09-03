#ifndef K2CPP_PLAN9_H
#define K2CPP_PLAN9_H

/* Post-lowering pass that makes k2cpp output safe for the native Plan 9
 * compilers (8c has no __auto_type, no multi-designator compound
 * literals, and no declarations inside for()). Only runs under --plan9;
 * the default output is byte-identical to before.
 *
 * The pass is purely syntactic: types are recovered from cast-literal
 * expressions or from a return-type map built by scanning the project's
 * headers (--include-dir), the same way the kryon surface declares
 * single-line prototypes. */

/* Add a directory scanned (recursively, .h files) for prototypes when
 * resolving __auto_type initializers. Call before k2cpp_lower. */
void k2cpp_plan9_add_include_dir(const char *dir);

/* Enable --plan9 output post-processing (off by default; the default
 * output is unchanged). */
void k2cpp_plan9_set_enabled(int enabled);
int k2cpp_plan9_enabled(void);

/* Rewrite one generated C source for 8c. Returns a malloc'd buffer; the
 * caller frees. Returns NULL on out-of-memory (caller keeps original). */
char *k2cpp_plan9_rewrite(const char *text);

/* Rewrite a generated file in place when the plan9 pass is enabled
 * (weak-declaration exposure for project files, then the generic pass).
 * Returns 0 on success. */
int k2cpp_plan9_rewrite_file(const char *path, int is_project);

/* Number of __auto_type declarations left unresolved by the last
 * k2cpp_plan9_rewrite call (reported once per run by main). */
int k2cpp_plan9_unresolved(void);

/* Drop the __GNUC__ guard around the weak app-lifecycle declarations in
 * generated project files: the native cpp does not define __GNUC__, so
 * the guarded block would compile out and leave the calls without
 * prototypes. */
char *k2cpp_plan9_rewrite_project(const char *text);

#endif
