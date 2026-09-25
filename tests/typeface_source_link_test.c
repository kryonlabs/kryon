#include "typeface_source_behavior.h"
#include "typeface_source.h"

#include <assert.h>
#include <string.h>

static int data_calls;
static int file_calls;
static int system_calls;
static int use_calls;
static int clear_calls;
static int discard_calls;

bool LoadTypefaceData(String name, String extension, String data,
                      int32_t *seed, int32_t seed_count)
{
    assert(name.length == 2 && memcmp(name.data, "ui", 2) == 0);
    assert(extension.length == 4 && memcmp(extension.data, ".ttf", 4) == 0);
    assert(data.length == 4 && memcmp(data.data, "font", 4) == 0);
    assert(seed == NULL && seed_count == 0);
    data_calls++;
    return true;
}

bool LoadTypefaceFile(String name, String path, int32_t *seed, int32_t seed_count)
{
    assert(name.length == 8 && memcmp(name.data, "fallback", 8) == 0);
    assert(path.length == 9 && memcmp(path.data, "/font.ttf", 9) == 0);
    assert(seed == NULL && seed_count == 0);
    file_calls++;
    return true;
}

bool LoadSystemTypeface(String name)
{
    assert(name.length == 2 && memcmp(name.data, "ui", 2) == 0);
    system_calls++;
    return true;
}

bool SelectTypeface(String name)
{
    assert(name.length == 2 && memcmp(name.data, "ui", 2) == 0);
    use_calls++;
    return true;
}

void ReleaseTypefaces(void) { clear_calls++; }
void ReleaseTypefaceCpu(void) { discard_calls++; }

int main(void)
{
    assert(Answer() == 42);
    assert(data_calls == 1 && file_calls == 1 && system_calls == 1);
    assert(use_calls == 1 && clear_calls == 1 && discard_calls == 1);
    return 0;
}
