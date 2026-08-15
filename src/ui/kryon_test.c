#include "kryon_test.h"
#include "kry_inject.h"
#include "kry_sfs.h"

#include <stdio.h>
#include <string.h>

static char g_kryt_last_text[1024];
static char g_kryt_last_key[64];
static char g_kryt_last_shot[256];

static void
kryt_copy(char *dst, size_t dst_size, const char *src)
{
    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

int
KryTFind(const char *selector, UIInspectNode *node)
{
    return UIInspectFindNode(selector, node);
}

int
KryTTap(const char *selector)
{
    UIInspectNode node;

    if(!KryTFind(selector, &node))
        return 0;
    KryonInjectTap(node.bounds.x + node.bounds.width / 2.0f,
                   node.bounds.y + node.bounds.height / 2.0f);
    return 1;
}

int
KryTType(const char *text)
{
    kryt_copy(g_kryt_last_text, sizeof(g_kryt_last_text), text);
    if(text == NULL || text[0] == '\0')
        return 0;
    KryonInjectText(text);
    return 1;
}

int
KryTKey(const char *key)
{
    char name[96];
    char path[160];

    kryt_copy(g_kryt_last_key, sizeof(g_kryt_last_key), key);
    if(key == NULL || key[0] == '\0')
        return 0;
    /* accept "KEY_ENTER" and "ENTER" alike; resolved through the SFS so
     * key names live in one place */
    if(strncmp(key, "KEY_", 4) == 0)
        snprintf(name, sizeof(name), "%s", key);
    else
        snprintf(name, sizeof(name), "KEY_%s", key);
    snprintf(path, sizeof(path), "/input/keys/%s", name);
    return KrySfsWrite(path, "tap") == 1;
}

int
KryTSee(const char *text)
{
    UIInspectNode node;
    char selector[96];

    if(text == NULL || text[0] == '\0')
        return 0;
    snprintf(selector, sizeof(selector), "text=%s", text);
    return KryTFind(selector, &node);
}

int
KryTShot(const char *name)
{
    kryt_copy(g_kryt_last_shot, sizeof(g_kryt_last_shot), name);
    return name != NULL && name[0] != '\0';
}

const char *
KryTLastText(void)
{
    return g_kryt_last_text;
}

const char *
KryTLastKey(void)
{
    return g_kryt_last_key;
}

const char *
KryTLastShot(void)
{
    return g_kryt_last_shot;
}
