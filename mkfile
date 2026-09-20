< /$objtype/mkfile

# Native Plan 9 build of Kryon with the libdraw backend.
#
# Builds libkryon.a with the native compiler (8c on 386). The plan9 shim
# headers in src/platform/plan9/include stand in for the hosted-OS headers
# Kryon's portable sources include; KRYON_PLATFORM_PLAN9 selects the native
# branches in the few files that touch OS services.
#
# Sources: the portable core, the libdraw backend, the full UI toolkit,
# the kry_std modules that do not need hosted OS services, and the
# generated runtime modules (widget policy, style sheets) that k2c emits
# as 8c-safe C.
# Excluded on purpose: canvas/KRB backends, raylib audio, dylib/process/http
# surfaces, sync, notifications, desktop integration, file dialogs, preview
# hosts, and runtime asset downloads.
#
# build/plan9 must be prepared on the host first (make kry-c-plan9):
# the generated runtime sources, the embedded asset table, and the
# generated-c-files.txt list the rules below consume.
#
# After `mk install`, link with -lkryon (or /$objtype/lib/libkryon.a) and
# compile app sources with the same include flags shown below (mk/plan9-app.mk
# carries them for applications).

LIB=/$objtype/lib/libkryon.a

ROOT=/sys/src/kryon
SHIM=$ROOT/src/platform/plan9/include
RAYEXT=$ROOT/vendor/raylib/src/external
GEN=$ROOT/build/plan9
genlist=$GEN/generated-c-files.txt

CPPFLAGS=-I$SHIM -I$ROOT/include -I$ROOT/src -I$ROOT/src/ui -I$GEN -I$GEN/generated -I$GEN/generated/src -I$GEN/generated/runtime -I$ROOT/vendor/utf8proc -DUTF8PROC_STATIC -I$RAYEXT \
	-DKRYON_BACKEND_LIBDRAW -DKRYON_PLATFORM_PLAN9 -DKRYON_NATIVE_PLAN9 \
	-DKRYON_EMBEDDED_ONLY=0

CFLAGS=-FTVw

OFILES=\
	src/backend/kry_backend.$O\
	src/backend/kry_event_wait.$O\
	src/backend/kry_image_decode.$O\
	src/backend/kry_input.$O\
	src/backend/kry_instance.$O\
	src/backend/kry_screenshot.$O\
	src/backend/kry_surface_math.$O\
	src/backend/kry_image.$O\
	src/backend/kry_sw.$O\
	src/backend/kry_sw_png.$O\
	src/backend/libdraw_audio.$O\
	src/backend/libdraw_backend.$O\
	src/backend/libdraw_font.$O\
	src/core/app_host.$O\
	src/core/app_runtime.$O\
	src/core/app_shell.$O\
	src/core/app_storage.$O\
	src/core/automation.$O\
	src/core/device_preferences.$O\
	src/core/embedded_assets.$O\
	src/core/kry_capabilities.$O\
	src/core/kryon_abi.$O\
	src/core/kryon_frame.$O\
	src/core/kryon_frame_pacing.$O\
	src/core/kryon_mem.$O\
	src/core/kryon_node.$O\
	src/core/locale.$O\
	src/core/theme.$O\
	src/core/theme_meta.$O\
	src/kry_std/audio_library.$O\
	src/kry_std/kry_xml.$O\
	src/sync/sync_crypto.$O\
	src/ui/ui_image_cache.$O\
	src/ui/ui_paint.$O\
	src/ui/ui_surface_cache.$O\
	src/ui/ui_text.$O\
	src/ui/ui_text_backend.$O\
	src/ui/ui_window.$O\
	src/kry_std/kry_archive.$O\
	src/kry_std/kry_filesystem.$O\
	src/kry_std/kry_json.$O\
	src/kry_std/kry_sha256.$O\
	src/markdown.$O\
	src/platform/kry_activity_monitor.$O\
	src/platform/android_host.$O\
	src/platform/plan9/plan9_notification.$O\
	src/platform/plan9/plan9_os.$O\
	src/platform/plan9/plan9_runtime_stubs.$O\
	src/platform/plan9/plan9_ui_globals.$O\
	src/platform/system_theme/system_theme.$O\
	$embedobj\
	$iconobj\
	$genobj

gensrc=`{cat $genlist}
genobj=${gensrc:%.c=%.$O}
embedobj=$GEN/embedded_asset_data.$O
iconobj=$GEN/ui_icon_assets.$O $GEN/ui_icon_names.$O

CLEANFILES=src/backend/*.$O src/core/*.$O src/kry_std/*.$O src/sync/*.$O src/platform/*/*.$O \
	src/platform/*.$O src/ui/*.$O *.$O src/*/*.i src/*.i \
	$GEN/generated/runtime/*.$O $GEN/generated/src/ui/*.$O $GEN/*.$O

all:V: check $LIB

check:V:
	if(! test -f $genlist){
		echo 'missing '^$genlist^'; run make kry-c-plan9 on the host first' >[1=2]
		exit missing
	}
	exit 0

install:V: check $LIB

$LIB:V: $OFILES
	ar vu $LIB $newprereq

&:n: &.$O
	ar vu $LIB $stem.$O

clean:V:
	rm -f $CLEANFILES

nuke:V: clean
	rm -f $LIB

# The native Plan 9 compilers carry no expression preprocessor (no #if,
# #elif, defined(), ||), so every source is first run through the system
# cpp - which supports the full directive set - and the fully preprocessed
# translation unit is compiled from its own directory so the object lands
# where the archive step expects it. This keeps the Kryon sources
# untouched: the same files build on hosted platforms with their native
# toolchains. Keep these rules free of Unix helper commands; rsc Plan 9
# does not ship dirname(1) or basename(1).
src/backend/%.$O: src/backend/%.c
	cd src/backend && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/core/%.$O: src/core/%.c
	cd src/core && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/kry_std/%.$O: src/kry_std/%.c
	cd src/kry_std && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/sync/%.$O: src/sync/%.c
	cd src/sync && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i
src/platform/plan9/%.$O: src/platform/plan9/%.c
	cd src/platform/plan9 && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/platform/system_theme/%.$O: src/platform/system_theme/%.c
	cd src/platform/system_theme && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/ui/%.$O: src/ui/%.c
	cd src/ui && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

src/markdown.$O: src/markdown.c
	cd src && cpp -+ $CPPFLAGS markdown.c > markdown.i && $CC $CFLAGS -c markdown.i && mv markdown.i.$O markdown.$O && rm -f markdown.i

$GEN/generated/runtime/%.$O: $GEN/generated/runtime/%.c
	cd $GEN/generated/runtime && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

$GEN/generated/src/ui/%.$O: $GEN/generated/src/ui/%.c
	cd $GEN/generated/src/ui && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

$GEN/embedded_asset_data.$O: $GEN/embedded_asset_data.c
	cd $GEN && cpp -+ $CPPFLAGS embedded_asset_data.c > embedded_asset_data.i && $CC $CFLAGS -c embedded_asset_data.i && mv embedded_asset_data.i.$O embedded_asset_data.$O && rm -f embedded_asset_data.i

$GEN/ui_icon_assets.$O: $GEN/ui_icon_assets.c
	cd $GEN && cpp -+ $CPPFLAGS ui_icon_assets.c > ui_icon_assets.i && $CC $CFLAGS -c ui_icon_assets.i && mv ui_icon_assets.i.$O ui_icon_assets.$O && rm -f ui_icon_assets.i

$GEN/ui_icon_names.$O: $GEN/ui_icon_names.c
	cd $GEN && cpp -+ $CPPFLAGS ui_icon_names.c > ui_icon_names.i && $CC $CFLAGS -c ui_icon_names.i && mv ui_icon_names.i.$O ui_icon_names.$O && rm -f ui_icon_names.i

src/platform/kry_activity_monitor.$O: src/platform/kry_activity_monitor.c
	cd src/platform && cpp -+ $CPPFLAGS kry_activity_monitor.c > kry_activity_monitor.i && $CC $CFLAGS -c kry_activity_monitor.i && mv kry_activity_monitor.i.$O kry_activity_monitor.$O && rm -f kry_activity_monitor.i

src/platform/android_host.$O: src/platform/android_host.c
	cd src/platform && cpp -+ $CPPFLAGS android_host.c > android_host.i && $CC $CFLAGS -c android_host.i && mv android_host.i.$O android_host.$O && rm -f android_host.i
