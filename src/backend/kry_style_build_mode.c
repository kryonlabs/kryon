#include <stdbool.h>

#ifdef KRYON_USE_RELEASE_STYLE_TABLES
bool RegisterCompiledBuiltInStylePacks(void);
#endif

int
kry_builtin_style_tables_enabled(void)
{
#ifdef KRYON_USE_RELEASE_STYLE_TABLES
    return 1;
#else
    return 0;
#endif
}

int
kry_register_compiled_builtin_style_packs(void)
{
#ifdef KRYON_USE_RELEASE_STYLE_TABLES
    return RegisterCompiledBuiltInStylePacks();
#else
    return 0;
#endif
}
