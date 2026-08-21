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
if [ "${KEEP_GENERATED_RUNTIME_PARITY_WORK:-0}" = 1 ]; then
    echo "keeping generated runtime parity work dir: $work" >&2
else
    trap 'rm -rf "$work"' EXIT INT TERM
fi

fixtures="
tests/parity/generated_form.kry
tests/parity/fields.kry
tests/parity/focus.kry
tests/parity/buttons_layout.kry
"
fixture_args=
for fixture in $fixtures; do
    fixture_args="$fixture_args $root/$fixture"
done

mkdir -p "$work/go" "$work/c" "$work/go-run" "$work/bin"

# shellcheck disable=SC2086
"$build/bin/k2g" --pkg main --no-main --root "$root" -o "$work/go" $fixture_args
# shellcheck disable=SC2086
"$build/bin/k2c" --root "$root" -o "$work/c" $fixture_args
sh "$root/tests/check_clean_generated_output.sh" "$work/go"
sh "$root/tests/check_clean_generated_output.sh" "$work/c"

cp "$work/go"/*.go "$work/go-run"/
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
	"fmt"
	"os"

	kryon "github.com/waozixyz/kryon/go/kryon"
)

type inputDriver interface {
	QueueText(string)
	QueueKey(int32)
	QueueShiftKey(int32)
	QueueShortcut(int32)
	QueueTap(float32, float32)
	SetClipboardText(string)
	ClipboardText() string
	SetSelection(int32, int32, int32)
	SetFocus(int32)
	Focus() int32
}

type snapshot struct {
	FormFirst          string `json:"form_first"`
	FormFirstCursor    int32  `json:"form_first_cursor"`
	FormSecond         string `json:"form_second"`
	FormSecondCursor   int32  `json:"form_second_cursor"`
	FormPassword       string `json:"form_password"`
	FormPasswordCursor int32  `json:"form_password_cursor"`
	FormNotes          string `json:"form_notes"`
	FormNotesCursor    int32  `json:"form_notes_cursor"`
	FormAction         int32  `json:"form_action"`
	FieldsTitle        string `json:"fields_title"`
	FieldsTitleCursor  int32  `json:"fields_title_cursor"`
	FieldsBody         string `json:"fields_body"`
	FieldsBodyCursor   int32  `json:"fields_body_cursor"`
	FocusOne           string `json:"focus_one"`
	FocusTwo           string `json:"focus_two"`
	FocusThree         string `json:"focus_three"`
	FocusID            int32  `json:"focus_id"`
	ButtonsAction      int32  `json:"buttons_action"`
	Clipboard          string `json:"clipboard"`
}

func drawForm() {
	kryon.BeginFrame()
	GeneratedForm_FormFrame(GeneratedFormStateValue)
	kryon.EndFrame()
}

func drawFields() {
	kryon.BeginFrame()
	Fields_FieldsFrame(FieldsStateValue)
	kryon.EndFrame()
}

func drawFocus() {
	kryon.BeginFrame()
	Focus_FocusFrame(FocusStateValue)
	kryon.EndFrame()
}

func drawButtons() {
	kryon.BeginFrame()
	ButtonsLayout_ButtonsFrame(ButtonsLayoutStateValue)
	kryon.EndFrame()
}

func requireFrameOps(label string, requirements map[kryon.FrameOpKind]int) {
	ops := kryon.FrameOps()
	if len(ops) == 0 {
		panic(label + ": generated Go produced no frame operations")
	}
	counts := map[kryon.FrameOpKind]int{}
	for _, op := range ops {
		counts[op.Kind]++
		if op.Secure && op.Text == "secret" {
			panic(label + ": secure text field leaked plaintext frame operation")
		}
	}
	for kind, min := range requirements {
		if counts[kind] < min {
			panic(fmt.Sprintf("%s: expected at least %d %s frame operations, got %d", label, min, kind, counts[kind]))
		}
	}
}

func text32(buf [32]byte) string {
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i])
		}
	}
	return string(buf[:])
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

	form := GeneratedFormStateValue
	fields := FieldsStateValue
	focus := FocusStateValue
	buttons := ButtonsLayoutStateValue

	drawForm()
	requireFrameOps("form", map[kryon.FrameOpKind]int{
		kryon.FrameOpColumn:    1,
		kryon.FrameOpRow:       1,
		kryon.FrameOpText:      1,
		kryon.FrameOpTextField: 3,
		kryon.FrameOpTextArea:  1,
		kryon.FrameOpButton:    2,
	})
	driver.SetFocus(101)
	drawForm()
	driver.QueueKey(kryon.KeyLeft)
	drawForm()
	driver.QueueText("é")
	drawForm()
	driver.QueueKey(kryon.KeyBackspace)
	drawForm()

	driver.SetFocus(102)
	drawForm()
	driver.SetSelection(102, 0, 4)
	driver.QueueText("acct")
	drawForm()

	driver.SetFocus(101)
	drawForm()
	driver.QueueKey(kryon.KeyTab)
	drawForm()
	driver.QueueText("Z")
	drawForm()

	driver.SetClipboardText("old")
	driver.SetFocus(103)
	drawForm()
	driver.SetSelection(103, 0, 6)
	driver.QueueShortcut(kryon.KeyC)
	drawForm()

	drawFields()
	requireFrameOps("fields", map[kryon.FrameOpKind]int{
		kryon.FrameOpTextField: 1,
		kryon.FrameOpTextArea:  1,
	})
	driver.QueueTap(30, 30)
	drawFields()
	driver.QueueKey(kryon.KeyLeft)
	drawFields()
	driver.QueueText("!")
	drawFields()
	driver.QueueTap(30, 86)
	drawFields()
	driver.QueueText(" body")
	drawFields()

	drawFocus()
	driver.QueueTap(30, 75)
	drawFocus()
	driver.QueueText("Z")
	drawFocus()
	driver.QueueShiftKey(kryon.KeyTab)
	drawFocus()
	driver.QueueText("A")
	drawFocus()

	drawButtons()
	requireFrameOps("buttons", map[kryon.FrameOpKind]int{
		kryon.FrameOpColumn: 1,
		kryon.FrameOpRow:    1,
		kryon.FrameOpText:   1,
		kryon.FrameOpButton: 2,
	})
	driver.QueueTap(30, 130)
	drawButtons()
	driver.QueueTap(130, 130)
	drawButtons()

	out := snapshot{
		FormFirst:          text64(form.First),
		FormFirstCursor:    form.FirstCursor,
		FormSecond:         text64(form.Second),
		FormSecondCursor:   form.SecondCursor,
		FormPassword:       text64(form.Password),
		FormPasswordCursor: form.PasswordCursor,
		FormNotes:          text128(form.Notes),
		FormNotesCursor:    form.NotesCursor,
		FormAction:         form.FormAction,
		FieldsTitle:        text64(fields.Title),
		FieldsTitleCursor:  fields.TitleCursor,
		FieldsBody:         text128(fields.Body),
		FieldsBodyCursor:   fields.BodyCursor,
		FocusOne:           text32(focus.One),
		FocusTwo:           text32(focus.Two),
		FocusThree:         text32(focus.Three),
		FocusID:            driver.Focus(),
		ButtonsAction:      buttons.ButtonsAction,
		Clipboard:          driver.ClipboardText(),
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
#include "$work/c/tests/parity/fields.c"
#include "$work/c/tests/parity/focus.c"
#include "$work/c/tests/parity/buttons_layout.c"

static void drain_events(void)
{
    UIEvent event;
    while(NextUIEvent(&event)) {}
}

static void draw_form(void)
{
    BeginUIFocus();
    form_frame_kry_draw();
    EndUIFocus();
    drain_events();
}

static void draw_fields(void)
{
    BeginUIFocus();
    fields_frame_kry_draw();
    EndUIFocus();
    drain_events();
}

static void draw_focus(void)
{
    BeginUIFocus();
    focus_frame_kry_draw();
    EndUIFocus();
    drain_events();
}

static void draw_buttons(void)
{
    BeginUIFocus();
    buttons_frame_kry_draw();
    EndUIFocus();
    drain_events();
}

int main(void)
{
    KryonInjectReset();

    draw_form();
    SetUIFocus(101);
    draw_form();
    KryonInjectKeyTap(KEY_LEFT);
    KryonInjectPump();
    draw_form();
    KryonInjectText("é");
    KryonInjectPump();
    draw_form();
    KryonInjectKeyTap(KEY_BACKSPACE);
    KryonInjectPump();
    draw_form();

    SetUIFocus(102);
    draw_form();
    SetSelection(102, 0, 4);
    KryonInjectText("acct");
    KryonInjectPump();
    draw_form();

    SetUIFocus(101);
    draw_form();
    KryonInjectKeyTap(KEY_TAB);
    KryonInjectPump();
    draw_form();
    KryonInjectText("Z");
    KryonInjectPump();
    draw_form();

    SetUIClipboardTextValue("old");
    SetUIFocus(103);
    draw_form();
    SetSelection(103, 0, 6);
    KryonInjectKey(KEY_LEFT_CONTROL, 1);
    KryonInjectKeyTap(KEY_C);
    KryonInjectPump();
    draw_form();
    KryonInjectKey(KEY_LEFT_CONTROL, 0);
    KryonInjectPump();

    draw_fields();
    KryonInjectTap(30, 30);
    KryonInjectPump();
    draw_fields();
    KryonInjectKeyTap(KEY_LEFT);
    KryonInjectPump();
    draw_fields();
    KryonInjectText("!");
    KryonInjectPump();
    draw_fields();
    KryonInjectTap(30, 86);
    KryonInjectPump();
    draw_fields();
    KryonInjectText(" body");
    KryonInjectPump();
    draw_fields();

    draw_focus();
    KryonInjectTap(30, 75);
    KryonInjectPump();
    draw_focus();
    KryonInjectText("Z");
    KryonInjectPump();
    draw_focus();
    KryonInjectKey(KEY_LEFT_SHIFT, 1);
    KryonInjectKeyTap(KEY_TAB);
    KryonInjectPump();
    draw_focus();
    KryonInjectKey(KEY_LEFT_SHIFT, 0);
    KryonInjectPump();
    KryonInjectText("A");
    KryonInjectPump();
    draw_focus();

    draw_buttons();
    KryonInjectTap(30, 130);
    KryonInjectPump();
    draw_buttons();
    KryonInjectPump();
    KryonInjectTap(130, 130);
    KryonInjectPump();
    draw_buttons();

    printf("{\"form_first\":\"%s\",\"form_first_cursor\":%d,\"form_second\":\"%s\",\"form_second_cursor\":%d,\"form_password\":\"%s\",\"form_password_cursor\":%d,\"form_notes\":\"%s\",\"form_notes_cursor\":%d,\"form_action\":%d,\"fields_title\":\"%s\",\"fields_title_cursor\":%d,\"fields_body\":\"%s\",\"fields_body_cursor\":%d,\"focus_one\":\"%s\",\"focus_two\":\"%s\",\"focus_three\":\"%s\",\"focus_id\":%d,\"buttons_action\":%d,\"clipboard\":\"%s\"}\n",
        first, first_cursor, second, second_cursor, password, password_cursor,
        notes, notes_cursor, form_action, title, title_cursor, body,
        body_cursor, one, two, three, GetUIFocus(), buttons_action,
        GetUIClipboardTextValue());
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

printf '%s\n' '{"generated_runtime_parity":"ok","fixtures":["tests/parity/generated_form.kry","tests/parity/fields.kry","tests/parity/focus.kry","tests/parity/buttons_layout.kry"]}'
