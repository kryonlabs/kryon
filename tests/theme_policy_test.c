#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
#include "runtime/theme.hpp"
#else
#include "runtime/theme.h"
#endif

int
main(int argc, char **argv)
{
    char line[1024];
    int rows = 0;
    FILE *fixture;

    assert(argc == 2);
    fixture = fopen(argv[1], "r");
    assert(fixture != NULL);
    while(fgets(line, sizeof(line), fixture) != NULL) {
        unsigned int values[21];
        char *cursor = line;
        Scheme scheme;

        if(line[0] == '#' || line[0] == '\n')
            continue;
        for(int i = 0; i < 21; i++) {
            char *end;
            values[i] = (unsigned int)strtoul(cursor, &end, 16);
            assert(end != cursor);
            cursor = end;
        }
        scheme = SchemeFor(values[0], values[1], values[2], values[3],
                           values[4], values[5] != 0, (uint8_t)values[6]);
        unsigned int actual[] = {
            scheme.primary, scheme.on_primary, scheme.secondary,
            scheme.on_secondary, scheme.surface, scheme.on_surface,
            scheme.surface_container, scheme.surface_variant,
            scheme.on_surface_variant, scheme.outline, scheme.error,
            scheme.on_error, scheme.disabled_container, scheme.disabled_content
        };
        for(int i = 0; i < 14; i++) {
            if(actual[i] != values[i + 7]) {
                fprintf(stderr, "scheme row %d role %d: %08x != %08x\n",
                        rows + 1, i, actual[i], values[i + 7]);
                abort();
            }
        }
        rows++;
    }
    fclose(fixture);
    assert(rows == 2);
    assert(ToneFor(0x12345678, 0, 0, false) == 0x12345678);
    assert(ToneFor(0x12345678, INT32_MIN, 0, false) == 0xffffff78);
    assert(ToneFor(0x12345678, INT32_MAX, 0, false) == 0x00000078);
    assert(ToneFor(0x12345678, 0, INT32_MIN, true) == 0x00000078);
    assert(ToneFor(0x12345678, 0, INT32_MAX, true) == 0xffffff78);
    puts("theme policy: shared scheme fixtures and tone limits passed");
    return 0;
}
