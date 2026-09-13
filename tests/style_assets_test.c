#include "embedded_assets.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void
assert_style_asset(const char *path, const char *pack)
{
    const EmbeddedAsset *asset = GetEmbeddedAsset(path);
    char *text;

    assert(asset != NULL);
    assert(strcmp(asset->mime, "text/plain") == 0);
    text = LoadEmbeddedAssetText(path);
    assert(text != NULL);
    assert(strstr(text, pack) != NULL);
    free(text);
}

int
main(void)
{
    assert_style_asset("styles/kryon/material.kss", "@pack material");
    assert_style_asset("./styles/kryon/tk.kss", "@pack tk");
    assert_style_asset("styles/kryon/vanilla.kss", "@pack vanilla");
    assert_style_asset("styles/kryon/glow.kss", "@pack glow");
    assert_style_asset("/styles/kryon/lightfield.kss",
                       "@pack lightfield");
    return 0;
}
