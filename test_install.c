#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* name;
} InstallConfig;

typedef struct {
    InstallConfig* install;
} KryonConfig;

InstallConfig* get_default() {
    InstallConfig* install = calloc(1, sizeof(InstallConfig));
    install->name = strdup("test");
    return install;
}

void config_free(KryonConfig* config) {
    if (!config) return;
    if (config->install) {
        free(config->install->name);
        free(config->install);
    }
    free(config);
}

int main() {
    KryonConfig* config = calloc(1, sizeof(KryonConfig));
    config->install = NULL;  // Simulating no install section

    InstallConfig* install = config->install;
    int install_is_default = 0;
    if (!install) {
        install = get_default();
        install_is_default = 1;
    }

    printf("install = %p, config->install = %p\n", (void*)install, (void*)config->install);
    printf("install_is_default = %d\n", install_is_default);

    // Cleanup
    if (install_is_default) {
        free(install->name);
        free(install);
    }
    config_free(config);

    printf("Success - no double free\n");
    return 0;
}
