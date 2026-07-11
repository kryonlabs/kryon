#!/bin/sh
set -eu

src_dir=${1:-}
if [ -z "$src_dir" ] || [ ! -d "$src_dir" ]; then
    echo "usage: $0 /path/to/raylib/src" >&2
    exit 1
fi

sdl_core="$src_dir/platforms/rcore_desktop_sdl.c"
audio="$src_dir/raudio.c"
rgfw_core="$src_dir/platforms/rcore_desktop_rgfw.c"

if [ -f "$sdl_core" ] && ! grep -q 'Flint: avoid X11 Font typedef collision' "$sdl_core"; then
    perl -0pi -e 's@(#elif defined\(USING_SDL2_PROJECT\)\n\s*#include "SDL2/SDL\.h"\n)(\s*#include "SDL2/SDL_syswm\.h"\s*// Required to get window handlers)@$1    // Flint: avoid X11 Font typedef collision when SDL_syswm.h pulls X11 headers.\n    #if defined(__unix__) \&\& !defined(__APPLE__)\n        #define Font X11Font\n    #endif\n$2\n    #if defined(__unix__) \&\& !defined(__APPLE__)\n        #undef Font\n    #endif@' "$sdl_core"
fi

if [ -f "$audio" ] && ! grep -q 'AUDIO_DEVICE_PERIODS' "$audio"; then
    perl -0pi -e 's@(#ifndef AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES\n\s*#define AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES 0[^\n]*\n#endif\n)@$1#ifndef AUDIO_DEVICE_PERIODS\n    #define AUDIO_DEVICE_PERIODS 0    // Device buffer period count. 0 uses miniaudio default\n#endif\n@' "$audio"
    perl -0pi -e 's@(config\.periodSizeInFrames = AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES;\n)@$1    config.periods = AUDIO_DEVICE_PERIODS;\n@' "$audio"
fi

if [ -f "$audio" ] && ! grep -q 'Flint: restore backend PlaySound rename after Win32 headers' "$audio"; then
    perl -0pi -e 's@#undef PlaySound\s*// Win32 API: windows\.h > mmsystem\.h defines PlaySound macro@#if defined(PlaySound)\n#undef PlaySound\n#endif\n#if defined(FLINT_RAYLIB_BACKEND_RENAME_H)\n#define PlaySound FlintRaylibBackend_PlaySound\n#endif                         // Flint: restore backend PlaySound rename after Win32 headers@' "$audio"
fi

if [ -f "$rgfw_core" ] && ! grep -q "Flint: restore backend renames after Win32 header workaround" "$rgfw_core"; then
    perl -i -pe 'if (/#undef Rectangle/ && ++$seen == 2) { $_ .= "\n    #if defined(FLINT_RAYLIB_BACKEND_RENAME_H)\n        #define CloseWindow FlintRaylibBackend_CloseWindow\n        #define ShowCursor FlintRaylibBackend_ShowCursor\n    #endif                         // Flint: restore backend renames after Win32 header workaround\n"; }' "$rgfw_core"
fi
