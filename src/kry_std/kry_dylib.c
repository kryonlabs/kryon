/*
 * kry_dylib.c - Kry standard library: dynamic library loading.
 *
 * Wraps dlopen/dlsym/dlclose (the IDE's editor_load_host/unload_host path) so
 * a .kry program can load a built app host and resolve its entry symbols
 * (CreateAppHost/DestroyAppHost) for live preview.
 */
#include "kry_dylib.h"

#include <stddef.h>

#if !defined(_WIN32)
#include <dlfcn.h>

void *
kry_dylib_load(const char *path)
{
    if(path == NULL)
        return NULL;
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *
kry_dylib_sym(void *handle, const char *name)
{
    if(handle == NULL || name == NULL)
        return NULL;
    return dlsym(handle, name);
}

void
kry_dylib_close(void *handle)
{
    if(handle != NULL)
        dlclose(handle);
}

const char *
kry_dylib_error(void)
{
    return dlerror();
}

#else  /* _WIN32 */

#include <windows.h>

void *
kry_dylib_load(const char *path)
{
    if(path == NULL)
        return NULL;
    return (void *)LoadLibraryA(path);
}

void *
kry_dylib_sym(void *handle, const char *name)
{
    if(handle == NULL || name == NULL)
        return NULL;
    return (void *)GetProcAddress((HMODULE)handle, name);
}

void
kry_dylib_close(void *handle)
{
    if(handle != NULL)
        FreeLibrary((HMODULE)handle);
}

const char *
kry_dylib_error(void)
{
    return NULL;
}

#endif /* _WIN32 */
