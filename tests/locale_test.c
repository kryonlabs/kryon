#define _POSIX_C_SOURCE 200809L

#include "locale.h"
#include "embedded_assets.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const EmbeddedAsset embedded_assets[] = {{0}};
const unsigned int embedded_asset_count = 0;

bool
FileExists(const char *fileName)
{
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 && S_ISREG(st.st_mode);
}

char *
LoadFileText(const char *fileName)
{
    FILE *f;
    long size;
    char *text;

    if(fileName == NULL)
        return NULL;
    f = fopen(fileName, "rb");
    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if(size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return NULL;
    }
    text[size] = '\0';
    fclose(f);
    return text;
}

void
UnloadFileText(char *text)
{
    free(text);
}

char *
LoadEmbeddedAssetText(const char *path)
{
    (void)path;
    return NULL;
}

const EmbeddedAsset *
GetEmbeddedAsset(const char *path)
{
    (void)path;
    return NULL;
}

const char *
GetEmbeddedAssetExtension(const char *path)
{
    (void)path;
    return "";
}

void
TraceLog(int logLevel, const char *text, ...)
{
    (void)logLevel;
    (void)text;
}

static int failures;

static void
write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if(f == NULL) {
        fprintf(stderr, "FAIL: open %s\n", path);
        exit(1);
    }
    if(fwrite(text, 1, strlen(text), f) != strlen(text)) {
        fprintf(stderr, "FAIL: write %s\n", path);
        fclose(f);
        exit(1);
    }
    fclose(f);
}

static void
check_locale(const char *preferences, const char *want)
{
    const char *got;

    setenv("KRYON_TEST_LOCALE", preferences, 1);
    got = GetSystemLocaleCode();
    if(strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL: %s -> %s, want %s\n", preferences, got, want);
        failures++;
    }
}

int
main(void)
{
    char tmp[] = "/tmp/kryon-locale-test.XXXXXX";
    char *dir = mkdtemp(tmp);

    if(dir == NULL) {
        fprintf(stderr, "FAIL: mkdtemp\n");
        return 1;
    }
    if(chdir(dir) != 0 || mkdir("locales", 0700) != 0) {
        fprintf(stderr, "FAIL: create locale fixture\n");
        return 1;
    }

    write_text("locales/index.txt",
               "en|English\n"
               "es|Español\n"
               "pt|Português\n"
               "zh|中文\n"
               "fr|Français\n"
               "de|Deutsch\n");
    write_text("locales/en.txt", "[app_title]\nKryon\n---\n");

    check_locale("pt-BR", "pt");
    check_locale("xx-ZZ,pt_BR.UTF-8", "pt");
    check_locale("zh-Hans-CN", "zh");
    check_locale("C,POSIX,es-419", "es");
    check_locale("EN_us.UTF-8", "en");
    check_locale("de_DE@euro", "de");
    check_locale("xx-ZZ", "en");

    unsetenv("KRYON_TEST_LOCALE");
    if(failures != 0)
        return 1;

    printf("locale tests passed\n");
    return 0;
}
