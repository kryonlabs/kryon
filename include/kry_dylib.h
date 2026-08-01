/*
 * kry_dylib.h - Kry standard library: dynamic library loading.
 *
 * A thin, IDE-facing wrapper over dlopen/dlsym/dlclose used to load a compiled
 * app host (.so/.dylib/.dll) for live preview. The handle is an opaque pointer;
 * symbol lookup returns a void* the caller casts to the expected function type.
 */
#ifndef KRYON_KRY_DYLIB_H
#define KRYON_KRY_DYLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Load a shared library at `path`. Returns an opaque handle, or NULL on
 * failure (use kry_dylib_error for details). */
void *kry_dylib_load(const char *path);

/* Look up a symbol by name in a loaded library. Returns NULL if not found. */
void *kry_dylib_sym(void *handle, const char *name);

/* Unload a library loaded by kry_dylib_load. Safe to call with NULL. */
void kry_dylib_close(void *handle);

/* Return a human-readable error string describing the most recent load/sym
 * failure, or NULL if none. Not all platforms populate this. */
const char *kry_dylib_error(void);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_DYLIB_H */
