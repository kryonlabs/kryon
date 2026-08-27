#ifndef KRYON_SETTINGS_H
#define KRYON_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryIntSetting {
    const char *key;
    int *value;
    int default_value;
    int min_value;
    int max_value;
} KryIntSetting;

typedef struct KryBoolSetting {
    const char *key;
    int *value;
    int default_value;
} KryBoolSetting;

int KryClampInt(int value, int min_value, int max_value);
int KryNormalizeIntSetting(KryIntSetting setting);
int KryNormalizeBoolSetting(KryBoolSetting setting);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_SETTINGS_H */
