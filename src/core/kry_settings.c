#include "kry_settings.h"

int
KryClampInt(int value, int min_value, int max_value)
{
    if(max_value < min_value) {
        int tmp = min_value;
        min_value = max_value;
        max_value = tmp;
    }
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

int
KryNormalizeIntSetting(KryIntSetting setting)
{
    int value;

    if(setting.value == 0)
        return KryClampInt(setting.default_value, setting.min_value,
                           setting.max_value);
    value = *setting.value;
    if(setting.max_value != setting.min_value)
        value = KryClampInt(value, setting.min_value, setting.max_value);
    *setting.value = value;
    return value;
}

int
KryNormalizeBoolSetting(KryBoolSetting setting)
{
    int value;

    if(setting.value == 0)
        return setting.default_value != 0;
    value = *setting.value != 0;
    *setting.value = value;
    return value;
}
