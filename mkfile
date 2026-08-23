< /$objtype/mkfile

# Native Plan 9 build of Kryon with the libdraw backend.
#
# Builds libkryon.a with the native compiler (8c on 386). The plan9 shim
# headers in src/platform/plan9/include stand in for the hosted-OS headers
# Kryon's portable sources include; KRYON_PLATFORM_PLAN9 selects the native
# branches in the few files that touch OS services.
#
# Sources: the portable core, the libdraw backend, the full UI toolkit, the
# scene layer, and the kry_std modules that do not need hosted OS services.
# Excluded on purpose: canvas/KRB backends, raylib audio, dylib/process/http
# surfaces, ksync, notifications, desktop integration, file dialogs, preview
# hosts, and runtime asset downloads.
#
# After `mk install`, link with -lkryon (or /$objtype/lib/libkryon.a) and
# compile app sources with the same include flags shown below.

LIB=/$objtype/lib/libkryon.a

ROOT=/sys/src/kryon
SHIM=$ROOT/src/platform/plan9/include
RAYEXT=$ROOT/vendor/raylib/src/external

CPPFLAGS=-I$SHIM -I$ROOT/include -I$ROOT/src -I$RAYEXT \
	-DKRYON_BACKEND_LIBDRAW -DKRYON_PLATFORM_PLAN9 -DKRYON_NATIVE_PLAN9 \
	-DUI_EMBEDDED_ONLY=0

CFLAGS=-FTVw

OFILES=\
	src/backend/kry_backend.$O\
	src/backend/kry_event_wait.$O\
	src/backend/kry_image_decode.$O\
	src/backend/kry_input.$O\
	src/backend/kry_screenshot.$O\
	src/backend/kry_surface_math.$O\
	src/backend/kry_sw.$O\
	src/backend/kry_sw_png.$O\
	src/backend/libdraw_audio.$O\
	src/backend/libdraw_backend.$O\
	src/backend/libdraw_font.$O\
	src/core/app_host.$O\
	src/core/app_runtime.$O\
	src/core/app_shell.$O\
	src/core/device_preferences.$O\
	src/core/embedded_assets.$O\
	src/core/kryon_abi.$O\
	src/core/kryon_frame.$O\
	src/core/kryon_mem.$O\
	src/core/kryon_node.$O\
	src/core/locale.$O\
	src/core/theme.$O\
	src/core/theme_meta.$O\
	src/kry_std/audio_library.$O\
	src/kry_std/kry_archive.$O\
	src/kry_std/kry_filesystem.$O\
	src/kry_std/kry_json.$O\
	src/kry_std/kry_sha256.$O\
	src/markdown.$O\
	src/platform/plan9/plan9_os.$O\
	src/platform/system_theme/system_theme.$O\
	src/scene/kry_animation.$O\
	src/scene/kry_signal.$O\
	src/scene/node_2d.$O\
	src/scene/node_animated_sprite2d.$O\
	src/scene/node_animation_player.$O\
	src/scene/node_area2d.$O\
	src/scene/node_audio_source.$O\
	src/scene/node_body2d.$O\
	src/scene/node_camera2d.$O\
	src/scene/node_collision_shape2d.$O\
	src/scene/node_sprite2d.$O\
	src/scene/node_tilemap.$O\
	src/scene/physics_world.$O\
	src/scene/scene_builtins.$O\
	src/scene/scene_inspect.$O\
	src/scene/scene_property.$O\
	src/scene/scene_tree.$O\
	src/ui/bottom_nav.$O\
	src/ui/button.$O\
	src/ui/dropdown.$O\
	src/ui/guide.$O\
	src/ui/icon_controls.$O\
	src/ui/kryon_test.$O\
	src/ui/modal.$O\
	src/ui/profile_header.$O\
	src/ui/reorder.$O\
	src/ui/rows.$O\
	src/ui/scroll.$O\
	src/ui/tab_bar.$O\
	src/ui/terminal_pane.$O\
	src/ui/terminal_pane_clipboard.$O\
	src/ui/terminal_pane_csi.$O\
	src/ui/terminal_pane_dcs.$O\
	src/ui/terminal_pane_keys.$O\
	src/ui/terminal_pane_modes.$O\
	src/ui/terminal_pane_mouse.$O\
	src/ui/terminal_pane_osc.$O\
	src/ui/terminal_pane_profile.$O\
	src/ui/terminal_pane_reflow.$O\
	src/ui/terminal_pane_render.$O\
	src/ui/terminal_pane_selection.$O\
	src/ui/terminal_pane_session.$O\
	src/ui/terminal_pane_sgr.$O\
	src/ui/terminal_pane_sixel.$O\
	src/ui/terminal_pane_text.$O\
	src/ui/theme_picker.$O\
	src/ui/toast.$O\
	src/ui/toolbar.$O\
	src/ui/top_nav.$O\
	src/ui/tutorial.$O\
	src/ui/ui.$O\
	src/ui/ui_clipboard.$O\
	src/ui/ui_clip.$O\
	src/ui/ui_color.$O\
	src/ui/ui_dpi.$O\
	src/ui/ui_icon_assets.$O\
	src/ui/ui_icon_names.$O\
	src/ui/ui_icons.$O\
	src/ui/ui_inspect.$O\
	src/ui/ui_layout.$O\
	src/ui/ui_node_registry.$O\
	src/ui/ui_picture_cache.$O\
	src/ui/ui_scaling.$O\
	src/ui/ui_slider.$O\
	src/ui/ui_style.$O\
	src/ui/ui_text.$O\
	src/ui/ui_text_backend.$O\
	src/ui/ui_text_edit.$O\
	src/ui/ui_text_layout.$O\
	src/ui/ui_titlebar.$O\
	src/ui/ui_tk.$O\
	src/ui/ui_transition.$O\
	src/ui/ui_tree.$O\
	src/ui/ui_window.$O\

CLEANFILES=src/backend/*.$O src/core/*.$O src/kry_std/*.$O src/platform/*/*.$O \
	src/scene/*.$O src/ui/*.$O *.$O src/*/*.i

< /sys/src/cmd/mksyslib

# The native Plan 9 compilers carry no expression preprocessor (no #if,
# #elif, defined(), ||), so every source is first run through the system
# cpp - which supports the full directive set - and the fully preprocessed
# translation unit is compiled from its own directory so the object lands
# where the archive step expects it. This keeps the Kryon sources
# untouched: the same files build on hosted platforms with their native
# toolchains.
%.$O: %.c
	d=`{dirname $stem}; b=`{basename $stem}; cd $d && cpp $CPPFLAGS $b.c > $b.i && $CC $CFLAGS -c $b.i && rm -f $b.i
