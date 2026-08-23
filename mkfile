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

SHIM=src/platform/plan9/include
RAYEXT=vendor/raylib/src/external

CFLAGS=-FTVw -I$SHIM -Iinclude -Isrc -I$RAYEXT \
	-DKRYON_BACKEND_LIBDRAW -DKRYON_PLATFORM_PLAN9 -DKRYON_NATIVE_PLAN9 \
	-DUI_EMBEDDED_ONLY=0 -c

OFILES=\
	backend/kry_backend.$O\
	backend/kry_event_wait.$O\
	backend/kry_image_decode.$O\
	backend/kry_input.$O\
	backend/kry_screenshot.$O\
	backend/kry_surface_math.$O\
	backend/kry_sw.$O\
	backend/kry_sw_png.$O\
	backend/libdraw_audio.$O\
	backend/libdraw_backend.$O\
	backend/libdraw_font.$O\
	core/app_host.$O\
	core/app_runtime.$O\
	core/app_shell.$O\
	core/device_preferences.$O\
	core/embedded_assets.$O\
	core/kryon_abi.$O\
	core/kryon_frame.$O\
	core/kryon_mem.$O\
	core/kryon_node.$O\
	core/locale.$O\
	core/theme.$O\
	core/theme_meta.$O\
	kry_std/audio_library.$O\
	kry_std/kry_archive.$O\
	kry_std/kry_filesystem.$O\
	kry_std/kry_json.$O\
	kry_std/kry_sha256.$O\
	markdown.$O\
	platform/plan9/plan9_os.$O\
	platform/system_theme/system_theme.$O\
	scene/kry_animation.$O\
	scene/kry_signal.$O\
	scene/node_2d.$O\
	scene/node_animated_sprite2d.$O\
	scene/node_animation_player.$O\
	scene/node_area2d.$O\
	scene/node_audio_source.$O\
	scene/node_body2d.$O\
	scene/node_camera2d.$O\
	scene/node_collision_shape2d.$O\
	scene/node_sprite2d.$O\
	scene/node_tilemap.$O\
	scene/physics_world.$O\
	scene/scene_builtins.$O\
	scene/scene_inspect.$O\
	scene/scene_property.$O\
	scene/scene_tree.$O\
	ui/bottom_nav.$O\
	ui/button.$O\
	ui/dropdown.$O\
	ui/guide.$O\
	ui/icon_controls.$O\
	ui/kryon_test.$O\
	ui/modal.$O\
	ui/profile_header.$O\
	ui/reorder.$O\
	ui/rows.$O\
	ui/scroll.$O\
	ui/tab_bar.$O\
	ui/terminal_pane.$O\
	ui/terminal_pane_clipboard.$O\
	ui/terminal_pane_csi.$O\
	ui/terminal_pane_dcs.$O\
	ui/terminal_pane_keys.$O\
	ui/terminal_pane_modes.$O\
	ui/terminal_pane_mouse.$O\
	ui/terminal_pane_osc.$O\
	ui/terminal_pane_profile.$O\
	ui/terminal_pane_reflow.$O\
	ui/terminal_pane_render.$O\
	ui/terminal_pane_selection.$O\
	ui/terminal_pane_session.$O\
	ui/terminal_pane_sgr.$O\
	ui/terminal_pane_sixel.$O\
	ui/terminal_pane_text.$O\
	ui/theme_picker.$O\
	ui/toast.$O\
	ui/toolbar.$O\
	ui/top_nav.$O\
	ui/tutorial.$O\
	ui/ui.$O\
	ui/ui_clipboard.$O\
	ui/ui_clip.$O\
	ui/ui_color.$O\
	ui/ui_dpi.$O\
	ui/ui_icon_assets.$O\
	ui/ui_icon_names.$O\
	ui/ui_icons.$O\
	ui/ui_inspect.$O\
	ui/ui_layout.$O\
	ui/ui_node_registry.$O\
	ui/ui_picture_cache.$O\
	ui/ui_scaling.$O\
	ui/ui_slider.$O\
	ui/ui_style.$O\
	ui/ui_text.$O\
	ui/ui_text_backend.$O\
	ui/ui_text_edit.$O\
	ui/ui_text_layout.$O\
	ui/ui_titlebar.$O\
	ui/ui_tk.$O\
	ui/ui_transition.$O\
	ui/ui_tree.$O\
	ui/ui_window.$O\

CLEANFILES=backend/*.$O core/*.$O kry_std/*.$O platform/plan9/*.$O \
	platform/system_theme/*.$O scene/*.$O ui/*.$O *.$O

< /sys/src/cmd/mksyslib
