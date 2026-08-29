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

if [ -f "$sdl_core" ] && ! grep -q 'Kryon: avoid X11 Font typedef collision' "$sdl_core"; then
    perl -0pi -e 's@(#elif defined\(USING_SDL2_PROJECT\)\n\s*#include "SDL2/SDL\.h"\n)(\s*#include "SDL2/SDL_syswm\.h"\s*// Required to get window handlers)@$1    // Kryon: avoid X11 Font typedef collision when SDL_syswm.h pulls X11 headers.\n    #if defined(__unix__) \&\& !defined(__APPLE__)\n        #define Font X11Font\n    #endif\n$2\n    #if defined(__unix__) \&\& !defined(__APPLE__)\n        #undef Font\n    #endif@' "$sdl_core"
fi

if [ -f "$sdl_core" ] && ! grep -q 'Kryon: derive SDL2 scale from drawable pixels' "$sdl_core"; then
    perl -0pi -e 's@    // NOTE: SDL_GetWindowDisplayScale not available on SDL2\n    // TODO: Implement the window scale factor calculation manually\n    TRACELOG\(LOG_WARNING, "GetWindowScaleDPI\(\) not implemented on target platform"\);@    // Kryon: derive SDL2 scale from drawable pixels instead of guessing from monitor DPI.\n    int window_width = 0, window_height = 0;\n    int drawable_width = 0, drawable_height = 0;\n    SDL_GetWindowSize(platform.window, \&window_width, \&window_height);\n    SDL_GL_GetDrawableSize(platform.window, \&drawable_width, \&drawable_height);\n    if (window_width > 0 \&\& window_height > 0 \&\&\n        drawable_width > 0 \&\& drawable_height > 0)\n    \{\n        scale.x = (float)drawable_width/(float)window_width;\n        scale.y = (float)drawable_height/(float)window_height;\n    \}@' "$sdl_core"
fi

if [ -f "$sdl_core" ] && ! grep -q 'Kryon: accumulate relative mouse motion' "$sdl_core"; then
    # Cursor-locked mouse must behave like the GLFW cursor-disabled mode
    # apps are written against: an unclamped virtual absolute position that
    # keeps accumulating past the window edges, with per-frame deltas as
    # the sum of that frame motion events. Overwriting the position with
    # each event xrel instead breaks every app that differences
    # GetMousePosition() itself (voxel clients and similar look controls).
    perl -0pi -e 's@                if \(CORE\.Input\.Mouse\.cursorLocked\)\n                \{\n                    CORE\.Input\.Mouse\.currentPosition\.x = \(float\)event\.motion\.xrel;\n                    CORE\.Input\.Mouse\.currentPosition\.y = \(float\)event\.motion\.yrel;\n                    CORE\.Input\.Mouse\.previousPosition = \(Vector2\)\{ 0\.0f, 0\.0f \};\n                \}@                if (CORE.Input.Mouse.cursorLocked)\n                \{\n                    // Kryon: accumulate relative mouse motion into an unclamped\n                    // virtual position (GLFW cursor-disabled parity).\n                    CORE.Input.Mouse.currentPosition.x += (float)event.motion.xrel;\n                    CORE.Input.Mouse.currentPosition.y += (float)event.motion.yrel;\n                \}@' "$sdl_core"
fi

if [ -f "$sdl_core" ] && ! grep -q 'Kryon: preserve the event consumed while waiting' "$sdl_core"; then
    perl -0pi -e 's@        SDL_WaitEvent\(NULL\);\n        CORE\.Time\.previous = GetTime\(\);@        // Kryon: SDL_WaitEvent consumes an event. Push it back so the poll loop\n        // below delivers the key, text, mouse, or window event instead of losing it.\n        SDL_Event waited = \{ 0 \};\n        if (SDL_WaitEvent(\&waited) > 0) SDL_PushEvent(\&waited);\n        CORE.Time.previous = GetTime();@' "$sdl_core"
fi

if [ -f "$audio" ] && ! grep -q 'AUDIO_DEVICE_PERIODS' "$audio"; then
    perl -0pi -e 's@(#ifndef AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES\n\s*#define AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES 0[^\n]*\n#endif\n)@$1#ifndef AUDIO_DEVICE_PERIODS\n    #define AUDIO_DEVICE_PERIODS 0    // Device buffer period count. 0 uses miniaudio default\n#endif\n@' "$audio"
    perl -0pi -e 's@(config\.periodSizeInFrames = AUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES;\n)@$1    config.periods = AUDIO_DEVICE_PERIODS;\n@' "$audio"
fi

if [ -f "$audio" ] && ! grep -q 'Kryon: restore backend PlaySound rename after Win32 headers' "$audio"; then
    perl -0pi -e 's@#undef PlaySound\s*// Win32 API: windows\.h > mmsystem\.h defines PlaySound macro@#if defined(PlaySound)\n#undef PlaySound\n#endif\n#if defined(KRYON_RAYLIB_BACKEND_RENAME_H)\n#define PlaySound KryonRaylibBackend_PlaySound\n#endif                         // Kryon: restore backend PlaySound rename after Win32 headers@' "$audio"
fi

if [ -f "$rgfw_core" ] && ! grep -q "Kryon: restore backend renames after Win32 header workaround" "$rgfw_core"; then
    perl -i -pe 'if (/#undef Rectangle/ && ++$seen == 2) { $_ .= "\n    #if defined(KRYON_RAYLIB_BACKEND_RENAME_H)\n        #define CloseWindow KryonRaylibBackend_CloseWindow\n        #define ShowCursor KryonRaylibBackend_ShowCursor\n    #endif                         // Kryon: restore backend renames after Win32 header workaround\n"; }' "$rgfw_core"
fi
