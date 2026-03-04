# Kryon Core - Plan 9 mk build file

<../mkfile.common

# Force POSIX toolchain (kryon needs SDL2/POSIX threads)
CC=gcc
LD=gcc
O=o

# Directories
INCLUDE=include
BUILD=build
BIN=bin

# Component source directories
WM_SRC=wm/src
TK_SRC=tk/src
DISPLAY_SRC=display/src
CLI_SRC=cli/src
LIB_SRC=lib/src

# Marrow graphics library
MARROW=../marrow
MARROW_INCLUDE=$MARROW/include

# Toolkit (tk/) - Core UI system
TK=widget window render events layout namespace tree 9p ops \
	devenv vdev devfd devproc devcons ctl wm parser target

TK_RENDER=render/primitives render/widgets render/events render/screen
TK_UTIL=util/color util/geom

TK_ALL=$TK $TK_RENDER $TK_UTIL

# Library (lib/) - 9P client library
LIB=client/p9client client/marrow

# Display client (display/)
DISPLAY=sdl_display eventpoll

# CLI tools (cli/)
CLI=main commands yaml

# Targets
LIB_TARGET=$BUILD/libkryon.a
DISPLAY_TARGET=$BIN/kryon-view
KRYON_WM_TARGET=$BIN/kryon-wm
KRYON_TARGET=$BIN/kryon
SAVE_PPM_TARGET=$BIN/save_ppm

# Default target
all:V: marrow-lib setup $LIB_TARGET $KRYON_WM_TARGET $DISPLAY_TARGET $SAVE_PPM_TARGET

# Ensure marrow is built first
marrow-lib:V:
	(cd ../marrow && mk setup && mk all)

# Create directories
setup:V:
	mkdir -p $BUILD/tk/render $BUILD/tk/util $BUILD/lib/client
	mkdir -p $BUILD/display $BUILD/cli $BUILD/marrow $BUILD/wm $BIN

# Library
$LIB_TARGET: ${TK_ALL:%=$BUILD/tk/%.o} ${LIB:%=$BUILD/lib/%.o}
	ar rvc $target $prereq

# Kryon Window Manager binary
$KRYON_WM_TARGET: $WM_SRC/main.c $LIB_TARGET
	$LD -Wall -g -I$INCLUDE -I$MARROW_INCLUDE -I$TK_SRC -I$LIB_SRC \
		$WM_SRC/main.c $BUILD/lib/client/p9client.$O $BUILD/lib/client/marrow.$O \
		$BUILD/tk/parser.$O $BUILD/tk/target.$O \
		-L$BUILD -L$MARROW/build -lkryon -lmarrow -o $target $LDFLAGS -lm

# Display client binary (requires SDL2)
$DISPLAY_TARGET: $DISPLAY_SRC/main.c $BUILD/display/sdl_display.$O $BUILD/display/eventpoll.$O $LIB_TARGET
	$LD -Wall -g -I$INCLUDE -I$TK_SRC -I$LIB_SRC -I$MARROW_INCLUDE \
		$DISPLAY_SRC/main.c $BUILD/display/sdl_display.$O $BUILD/display/eventpoll.$O \
		-L$BUILD -L$MARROW/build -lkryon -lmarrow -o $target $LDFLAGS -lSDL2

# Kryon CLI binary
$KRYON_TARGET: ${CLI:%=$BUILD/cli/%.o} $BUILD/tk/target.$O
	$LD -Wall -g -I$INCLUDE -I$CLI_SRC -I$TK_SRC \
		${CLI:%=$BUILD/cli/%.o} $BUILD/tk/target.$O -o $target $LDFLAGS
	chmod +x $target

# PPM screenshot tool
$SAVE_PPM_TARGET: tools/save_ppm.c
	gcc -Wall -g $prereq -o $target

# Compile rules for tk subdirectories (must come first - more specific)
$BUILD/tk/render/%.o: $TK_SRC/render/%.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/util/%.o: $TK_SRC/util/%.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

# Compile rules for tk top-level files only (no subdirectories)
# List explicitly to avoid matching subdirectory files
$BUILD/tk/widget.$O: $TK_SRC/widget.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/window.$O: $TK_SRC/window.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/render.$O: $TK_SRC/render.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/events.$O: $TK_SRC/events.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/layout.$O: $TK_SRC/layout.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/namespace.$O: $TK_SRC/namespace.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/tree.$O: $TK_SRC/tree.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/9p.$O: $TK_SRC/9p.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/ops.$O: $TK_SRC/ops.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/devenv.$O: $TK_SRC/devenv.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/vdev.$O: $TK_SRC/vdev.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/devfd.$O: $TK_SRC/devfd.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/devproc.$O: $TK_SRC/devproc.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/devcons.$O: $TK_SRC/devcons.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/ctl.$O: $TK_SRC/ctl.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/wm.$O: $TK_SRC/wm.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/parser.$O: $TK_SRC/parser.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

$BUILD/tk/target.$O: $TK_SRC/target.c
	$CC $CFLAGS -I$INCLUDE -I$TK_SRC -I$MARROW_INCLUDE -c $prereq -o $target

# Compile rules for lib
$BUILD/lib/client/%.o: $LIB_SRC/client/%.c
	$CC $CFLAGS -I$INCLUDE -I$LIB_SRC -c $prereq -o $target

# Compile rules for display
$BUILD/display/%.o: $DISPLAY_SRC/%.c
	$CC $CFLAGS -DHAVE_SDL2 `sdl2-config --cflags` -I$INCLUDE -I$TK_SRC -I$LIB_SRC -c $prereq -o $target

# Compile rules for CLI
$BUILD/cli/%.o: $CLI_SRC/%.c
	$CC $CFLAGS -I$INCLUDE -I$CLI_SRC -I$TK_SRC -c $prereq -o $target

# Clean
clean:V:
	rm -rf $BUILD $BIN
