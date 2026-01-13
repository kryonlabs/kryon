#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef char* (*CSSGenFunc)(const char* theme);

int main() {
    void* handle = dlopen("/home/wao/.local/share/kryon/plugins/syntax/libkryon_syntax.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load: %s\n", dlerror());
        return 1;
    }

    CSSGenFunc css_gen = dlsym(handle, "syntax_web_css_generator");
    if (!css_gen) {
        fprintf(stderr, "Failed to find symbol: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    char* css = css_gen("dark");
    if (css) {
        printf("CSS returned (length %zu):\n%s\n", strlen(css), css);
        free(css);
    } else {
        printf("No CSS returned\n");
    }

    dlclose(handle);
    return 0;
}
