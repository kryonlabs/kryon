#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d /tmp/kryon-raylib-precision.XXXXXX)
cp "$root/vendor/raylib/src/rlgl.h" "$work/rlgl.h"
sh "$root/scripts/prepare-raylib-source.sh" "$work"
cp "$work/rlgl.h" "$work/prepared.h"
sh "$root/scripts/prepare-raylib-source.sh" "$work"
cmp "$work/prepared.h" "$work/rlgl.h"
perl -0ne '
    /const char \*defaultVShaderCode =(.*?)const char \*defaultFShaderCode =(.*)/s
        or die "default shader declarations missing\n";
    my ($vertex, $fragment) = ($1, $2);
    my $positions = () = $vertex =~ /precision highp float/g;
    my $interfaces = () = $vertex =~ /(?:out|varying) mediump vec[24] frag/g;
    die "GLES vertex transforms are not high precision\n" unless $positions == 2;
    die "GLES interpolator precision changed\n" unless $interfaces == 4;
    die "fragment shader unexpectedly requires high precision\n"
        if $fragment =~ /precision highp float/;
' "$work/rlgl.h"
echo "Raylib GLES vertex precision preparation is idempotent"
