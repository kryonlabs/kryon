#include <stdio.h>
#include <stdlib.h>
#include "../third_party/tomlc99/toml.h"

int main() {
    FILE* fp = fopen("/home/wao/.kryon/plugins/syntax/plugin.toml", "r");
    if (!fp) {
        printf("Cannot open file\n");
        return 1;
    }

    char errbuf[200] = {0};
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!conf) {
        printf("Parse error: %s\n", errbuf);
        return 1;
    }

    printf("Parse successful!\n");
    
    toml_table_t* plugin = toml_table_in(conf, "plugin");
    if (plugin) {
        toml_datum_t name = toml_string_in(plugin, "name");
        if (name.ok) {
            printf("Plugin name: %s\n", name.u.s);
            free(name.u.s);
        }
    }

    toml_free(conf);
    return 0;
}
