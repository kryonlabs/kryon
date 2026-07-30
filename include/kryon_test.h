#ifndef KRYON_TEST_H
#define KRYON_TEST_H

#include "ui_inspect.h"

int KryTFind(const char *selector, UIInspectNode *node);
int KryTTap(const char *selector);
int KryTType(const char *text);
int KryTKey(const char *key);
int KryTSee(const char *text);
int KryTShot(const char *name);

const char *KryTLastText(void);
const char *KryTLastKey(void);
const char *KryTLastShot(void);

#endif
