#!/bin/sh
set -eu

root=$(cd "${1:-.}" && pwd)
build_arg=${2:-build/linux-x86_64}
case "$build_arg" in
    /*) build=$build_arg ;;
    *) build=$root/$build_arg ;;
esac
cc=${3:-${CC:-cc}}
cppflags=${4:-${CPPFLAGS:-}}
cflags=${5:-${CFLAGS:-}}
link_flags=${6:-}

work=${TMPDIR:-/tmp}/kryon-generated-runtime-parity.$$
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$work/go" "$work/c" "$work/go-run" "$work/bin"

fixture=tests/parity/generated_form.kry
"$build/bin/k2g" --pkg main --no-main --root "$root" -o "$work/go" "$root/$fixture"
"$build/bin/k2c" --root "$root" -o "$work/c" "$root/$fixture"
sh "$root/tests/check_clean_generated_output.sh" "$work/go"
sh "$root/tests/check_clean_generated_output.sh" "$work/c"

cp "$work/go/generated_form.go" "$work/go-run/generated_form.go"
cat > "$work/go-run/go.mod" <<EOF
module kryon-generated-runtime-parity

go 1.25.0

require github.com/waozixyz/kryon/go/kryon v0.0.0
replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon
EOF
cat > "$work/go-run/main.go" <<'EOF'
package main

import (
	"encoding/json"
	"os"

	kryon "github.com/waozixyz/kryon/go/kryon"
)

type inputDriver interface {
	QueueText(string)
	QueueKey(int32)
	QueueShortcut(int32)
	SetClipboardText(string)
	ClipboardText() string
	SetSelection(int32, int32, int32)
	SetFocus(int32)
}

type snapshot struct {
	First          string `json:"first"`
	FirstCursor    int32  `json:"first_cursor"`
	Second         string `json:"second"`
	SecondCursor   int32  `json:"second_cursor"`
	Password       string `json:"password"`
	PasswordCursor int32  `json:"password_cursor"`
	Notes          string `json:"notes"`
	NotesCursor    int32  `json:"notes_cursor"`
	Action         int32  `json:"action"`
	Clipboard      string `json:"clipboard"`
}

func draw() {
	kryon.BeginFrame()
	GeneratedForm_Main(GeneratedFormStateValue)
	kryon.EndFrame()
}

func text64(buf [64]byte) string {
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i])
		}
	}
	return string(buf[:])
}

func text128(buf [128]byte) string {
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i])
		}
	}
	return string(buf[:])
}

func main() {
	rt := kryon.Open(kryon.AppConfig{Width: 640, Height: 480, FPS: 60})
	driver := rt.(inputDriver)
	st := GeneratedFormStateValue

	draw()
	driver.SetFocus(101)
	draw()
	driver.QueueKey(kryon.KeyLeft)
	draw()
	driver.QueueText("é")
	draw()
	driver.QueueKey(kryon.KeyBackspace)
	draw()

	driver.SetFocus(102)
	draw()
	driver.SetSelection(102, 0, 4)
	driver.QueueText("acct")
	draw()

	driver.SetFocus(101)
	draw()
	driver.QueueKey(kryon.KeyTab)
	draw()
	driver.QueueText("Z")
	draw()

	driver.SetClipboardText("old")
	driver.SetFocus(103)
	draw()
	driver.SetSelection(103, 0, 6)
	driver.QueueShortcut(kryon.KeyC)
	draw()

	out := snapshot{
		First:          text64(st.First),
		FirstCursor:    st.FirstCursor,
		Second:         text64(st.Second),
		SecondCursor:   st.SecondCursor,
		Password:       text64(st.Password),
		PasswordCursor: st.PasswordCursor,
		Notes:          text128(st.Notes),
		NotesCursor:    st.NotesCursor,
		Action:         st.Action,
		Clipboard:      driver.ClipboardText(),
	}
	if err := json.NewEncoder(os.Stdout).Encode(out); err != nil {
		panic(err)
	}
}
EOF

(cd "$work/go-run" && GOCACHE=${GOCACHE:-$work/go-cache} go run . > "$work/go.json")

cat > "$work/c_runner.c" <<EOF
#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <string.h>

#include "$work/c/tests/parity/generated_form.c"

static void drain_events(void)
{
    UIEvent event;
    while(NextUIEvent(&event)) {}
}

static void draw_frame(void)
{
    BeginUIFocus();
    main_kry_draw();
    EndUIFocus();
    drain_events();
}

int main(void)
{
    KryonInjectReset();

    draw_frame();
    SetUIFocus(101);
    draw_frame();
    KryonInjectKeyTap(KEY_LEFT);
    KryonInjectPump();
    draw_frame();
    KryonInjectText("é");
    KryonInjectPump();
    draw_frame();
    KryonInjectKeyTap(KEY_BACKSPACE);
    KryonInjectPump();
    draw_frame();

    SetUIFocus(102);
    draw_frame();
    SetSelection(102, 0, 4);
    KryonInjectText("acct");
    KryonInjectPump();
    draw_frame();

    SetUIFocus(101);
    draw_frame();
    KryonInjectKeyTap(KEY_TAB);
    KryonInjectPump();
    draw_frame();
    KryonInjectText("Z");
    KryonInjectPump();
    draw_frame();

    SetUIClipboardTextValue("old");
    SetUIFocus(103);
    draw_frame();
    SetSelection(103, 0, 6);
    KryonInjectKey(KEY_LEFT_CONTROL, 1);
    KryonInjectKeyTap(KEY_C);
    KryonInjectPump();
    draw_frame();
    KryonInjectKey(KEY_LEFT_CONTROL, 0);
    KryonInjectPump();

    printf("{\"first\":\"%s\",\"first_cursor\":%d,\"second\":\"%s\",\"second_cursor\":%d,\"password\":\"%s\",\"password_cursor\":%d,\"notes\":\"%s\",\"notes_cursor\":%d,\"action\":%d,\"clipboard\":\"%s\"}\n",
        first, first_cursor, second, second_cursor, password, password_cursor,
        notes, notes_cursor, action, GetUIClipboardTextValue());
    return 0;
}
EOF

# shellcheck disable=SC2086
$cc $cppflags $cflags -I"$root/include" -I"$work/c" "$work/c_runner.c" \
    $link_flags -o "$work/bin/c_runner"
"$work/bin/c_runner" > "$work/c.json"

if ! diff -u "$work/go.json" "$work/c.json"; then
    echo "generated Go/C runtime parity mismatch" >&2
    exit 1
fi

printf '%s\n' '{"generated_runtime_parity":"ok","fixture":"tests/parity/generated_form.kry"}'
