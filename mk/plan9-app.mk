# Shared native Plan 9 build rules for Kryon applications.
#
# The including mkfile defines, before the include:
#
#	TARG		binary name
#	ROOT		the app's /sys/src path (its own source root)
#	gensrc		generated sources, usually `{cat $list}; filter here if
#			some generated files are replaced by stubs
#	appsrc		hand-written app sources
#	hostsrc		generated host-side sources (embedded assets, ...)
#	APPCPPFLAGS	extra -I/-D flags beyond the Kryon library set
#	LDLIBS		extra libraries beyond -lkryon -ldraw -lmemdraw -lthread
#
# build/plan9 must be prepared on the host first (k2c --plan9 plus the
# embedded asset table and generated-c-files.txt).

KRYON=/sys/src/kryon
BIN=/$objtype/bin
OUT=$O.out

obj=$ROOT/build/plan9/obj
list=$ROOT/build/plan9/generated-c-files.txt

CPPFLAGS=-I$KRYON/src/platform/plan9/include -I$KRYON/include -I$KRYON/src -I$KRYON/src/ui $APPCPPFLAGS \
	-DKRYON_BACKEND_LIBDRAW=1 -DKRYON_PLATFORM_PLAN9=1 -DKRYON_NATIVE_PLAN9=1 -DUI_EMBEDDED_ONLY=1

CFLAGS=-FTVw

allsrc=`{echo $gensrc $appsrc $hostsrc | tr ' ' '\12' | grep -v '^$'}
OFILES=`{echo $allsrc | tr ' ' '\12' | sed -e 's@\.c$@.8@' -e 's@^@'$obj'/@'}

check:V:
	if(! test -f $list) {
		echo 'missing '^$list^'; prepare build/plan9 on the host first' >[1=2]
		exit missing
	}
	exit 0

all:V: check $OUT

install:V: check $BIN/$TARG

$BIN/$TARG: $OUT
	cp $OUT $BIN/$TARG

$OUT: $OFILES /$objtype/lib/libkryon.a
	$LD -o $target $prereq -lkryon -ldraw -lmemdraw -lthread $LDLIBS

$obj/%.8: %.c
	mkdir -p `{echo $target | sed 's@/[^/]*$@@'} && cpp -+ $CPPFLAGS $prereq > $obj/$stem.i && $CC $CFLAGS -o $target -c $obj/$stem.i && rm -f $obj/$stem.i

clean:V:
	rm -rf $obj [$OS].out
