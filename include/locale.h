#ifndef LOCALE_H
#define LOCALE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LocaleEntry {
    char *key;
    char *value;
} LocaleEntry;

typedef struct LocaleLanguage {
    char *code;
    char *label;
} LocaleLanguage;

void InitLocale(void);
int SetLocale(const char *code);
const char *GetLocaleText(const char *key);
void FormatLocaleText(char *dst, size_t dst_size, const char *key, ...);

int GetLocaleCount(void);
const char *GetLocaleCode(int index);
const char *GetLocaleLabel(int index);
int GetLocaleIndex(const char *code);
const char *GetCurrentLocaleCode(void);
int GetCurrentLocaleIndex(void);

#ifdef __cplusplus
}
#endif

#endif // LOCALE_H
