#include "kryon_test.h"

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

    return KryTFind(selector, &node);
}

int
KryTType(const char *text)
{
    kryt_copy(g_kryt_last_text, sizeof(g_kryt_last_text), text);
    return text != NULL;
}

int
KryTKey(const char *key)
{
    kryt_copy(g_kryt_last_key, sizeof(g_kryt_last_key), key);
    return key != NULL && key[0] != '\0';
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
