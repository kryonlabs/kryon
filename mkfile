< /$objtype/mkfile

# Native Plan 9 archive for the Ziran-authored Kryon UI library. The C
# files are generated on a hosted build with `make plan9-c`; this mkfile
# only compiles that checked output with the guest compiler.

LIB=/$objtype/lib/libkryon.a
ROOT=/sys/src/kryon
GEN=$ROOT/build/plan9
genlist=$GEN/generated-c-files.txt

CPPFLAGS=-I$GEN
CFLAGS=-FTVw

gensrc=`{cat $genlist}
genobj=${gensrc:%.c=%.$O}

all:V: check $LIB
install:V: check $LIB

check:V:
	if(! test -f $genlist){
		echo 'missing '^$genlist^'; run make plan9-c on the host first' >[1]2]
		exit missing
	}
	exit 0

$LIB:V: $genobj
	rm -f $LIB
	ar vq $LIB $genobj
	ar vu $LIB

clean:V:
	rm -f $GEN/*.$O $GEN/*.i $LIB

$GEN/%.8: $GEN/%.c
	cpp -+ $CPPFLAGS $prereq > $GEN/$stem.i && $CC $CFLAGS -o $target -c $GEN/$stem.i && rm -f $GEN/$stem.i
