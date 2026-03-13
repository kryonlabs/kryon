# TaijiOS Toolkit (TK) - Plan 9 mk build file
# Following mu/mkfile pattern

<../mkfile.common

# lib9 integration - sys toolchain builds lib9
SYS=../sys
LIB9=$SYS/src/lib/9
LIB9_INCLUDE=$SYS/include
LIB9_LIB=$ROOT/amd64/lib/lib9.a

# Directories
INCLUDE=include
LIB=lib
BUILD=build
BIN=bin

# Mu graphics library (formerly marrow)
MU=../mu
MU_INCLUDE=$MU/include

# Library modules (following mu's pattern)
TK_WIDGET=widget
TK_WINDOW=window
TK_RENDER=render render/primitives render/widgets render/events render/screen
TK_EVENTS=events
TK_LAYOUT=layout
TK_UTIL=util/color util/geom
TK_WM=wm parser namespace tree ops ctl vdev state target

# All library objects
TK_ALL=$TK_WIDGET $TK_WINDOW $TK_RENDER $TK_EVENTS $TK_LAYOUT $TK_UTIL $TK_WM

# Targets
LIB_TARGET=$BUILD/libtk.a

# Command-line tool
CMD_TARGET=$BIN/tjk
CMD_SRC=cmd/tjk
CMD=main commands yaml

# Default target
all:V: setup $LIB_TARGET $CMD_TARGET

# Create directories
setup:V:
	mkdir -p $BUILD/lib $BUILD/render $BUILD/util $BUILD/cmd $BIN

# Library
$LIB_TARGET: ${TK_ALL:%=$BUILD/%.$O} $LIB9_LIB
	ar rvc $target $prereq

# CLI tool
$CMD_TARGET: ${CMD:%=$BUILD/cmd/%.$O} $BUILD/target.$O $LIB_TARGET
	$LD -Wall -g -I$INCLUDE -I$CMD_SRC \
		${CMD:%=$BUILD/cmd/%.$O} $BUILD/target.$O \
		-L$BUILD -ltk -l9 -o $target $LDFLAGS
	chmod +x $target

# Compile rules for render subdirectory
$BUILD/render/%.$O: $LIB/render/%.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

# Compile rules for util subdirectory
$BUILD/util/%.$O: $LIB/util/%.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

# Compile rules for top-level library files
$BUILD/widget.$O: $LIB/widget.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/window.$O: $LIB/window.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/render.$O: $LIB/render.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/events.$O: $LIB/events.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/layout.$O: $LIB/layout.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/namespace.$O: $LIB/namespace.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/tree.$O: $LIB/tree.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/ops.$O: $LIB/ops.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/vdev.$O: $LIB/vdev.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/ctl.$O: $LIB/ctl.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/wm.$O: $LIB/wm.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/parser.$O: $LIB/parser.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/state.$O: $LIB/state.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

$BUILD/target.$O: $LIB/target.c
	$CC $CFLAGS -I$INCLUDE -I$LIB -I$MU_INCLUDE -I$LIB9_INCLUDE -c $prereq -o $target

# Compile rules for CLI tool
$BUILD/cmd/%.$O: $CMD_SRC/%.c
	$CC $CFLAGS -I$INCLUDE -I$CMD_SRC -I$LIB -I$LIB9_INCLUDE -c $prereq -o $target

# Clean
clean:V:
	rm -rf $BUILD $BIN
