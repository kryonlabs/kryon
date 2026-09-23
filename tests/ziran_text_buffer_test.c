#include "ziran_host.h"

#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
    assert(instance != NULL);
    for (int phase = 0; phase < 6; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "TextBuffer phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
