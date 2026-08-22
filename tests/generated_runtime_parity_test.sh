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
tests/parity/long_text.kry
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

require (
	github.com/waozixyz/kryon/go/kryon v0.0.0
	golang.org/x/image v0.45.0
	golang.org/x/sys v0.47.0
	golang.org/x/text v0.41.0
)
replace github.com/waozixyz/kryon/go/kryon => $root/go/kryon
EOF
cp "$root/go/kryon/go.sum" "$work/go-run/go.sum"
cat > "$work/go-run/main.go" <<'EOF'
package main

import (
	"encoding/json"
	"fmt"
	"image/color"
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
	LongFirstLen       int    `json:"long_first_len"`
	LongFirstCursor    int32  `json:"long_first_cursor"`
	LongFirstHash      uint64 `json:"long_first_hash"`
	LongSecondLen      int    `json:"long_second_len"`
	LongSecondCursor   int32  `json:"long_second_cursor"`
	LongSecondHash     uint64 `json:"long_second_hash"`
	Clipboard          string `json:"clipboard"`
}

var host *kryon.Host

func drawForm() {
	host.Draw(func() {
		kryon.BeginFrame()
		GeneratedForm_FormFrame(GeneratedFormStateValue)
		kryon.EndFrame()
	})
}

func drawFields() {
	host.Draw(func() {
		kryon.BeginFrame()
		Fields_FieldsFrame(FieldsStateValue)
		kryon.EndFrame()
	})
}

func drawFocus() {
	host.Draw(func() {
		kryon.BeginFrame()
		Focus_FocusFrame(FocusStateValue)
		kryon.EndFrame()
	})
}

func drawButtons() {
	host.Draw(func() {
		kryon.BeginFrame()
		ButtonsLayout_ButtonsFrame(ButtonsLayoutStateValue)
		kryon.EndFrame()
	})
}

func drawLongText() {
	host.Draw(func() {
		kryon.BeginFrame()
		LongText_LongTextFrame(LongTextStateValue)
		kryon.EndFrame()
	})
}

func requireFrameOps(label string, requirements map[kryon.FrameOpKind]int) {
	ops := host.FrameOps()
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

func requireRenderedFrame(label string, minChangedPixels int) {
	img := host.Render()
	bounds := img.Bounds()
	if bounds.Dx() != 640 || bounds.Dy() != 480 {
		panic(fmt.Sprintf("%s: rendered frame size = %dx%d, want 640x480", label, bounds.Dx(), bounds.Dy()))
	}
	background := color.RGBA{R: kryon.RAYWHITE.R, G: kryon.RAYWHITE.G, B: kryon.RAYWHITE.B, A: kryon.RAYWHITE.A}
	changed := 0
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			if img.RGBAAt(x, y) != background {
				changed++
			}
		}
	}
	if changed < minChangedPixels {
		panic(fmt.Sprintf("%s: generated Go rendered only %d changed pixels, want at least %d", label, changed, minChangedPixels))
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

func text4096(buf [4096]byte) string {
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i])
		}
	}
	return string(buf[:])
}

func checksum(text string) uint64 {
	var hash uint64 = 1469598103934665603
	for i := 0; i < len(text); i++ {
		hash ^= uint64(text[i])
		hash *= 1099511628211
	}
	return hash
}

func main() {
	host = kryon.NewHost(kryon.AppConfig{Width: 640, Height: 480, FPS: 60})
	driver := host.Runtime().(inputDriver)

	form := GeneratedFormStateValue
	fields := FieldsStateValue
	focus := FocusStateValue
	buttons := ButtonsLayoutStateValue
	longText := LongTextStateValue

	drawForm()
	requireFrameOps("form", map[kryon.FrameOpKind]int{
		kryon.FrameOpColumn:    1,
		kryon.FrameOpRow:       1,
		kryon.FrameOpText:      1,
		kryon.FrameOpTextField: 3,
		kryon.FrameOpTextArea:  1,
		kryon.FrameOpButton:    2,
	})
	requireRenderedFrame("form", 2500)
	driver.SetFocus(101)
	drawForm()
	driver.QueueKey(kryon.KeyLeft)
	drawForm()
	driver.QueueText("é")
	drawForm()
	driver.QueueKey(kryon.KeyBackspace)
	drawForm()
	if got := text64(form.First); got != "alpha" {
		panic(fmt.Sprintf("form: backspace restored first field to %q, want alpha", got))
	}

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
	requireRenderedFrame("fields", 1200)
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
	requireRenderedFrame("buttons", 1000)
	driver.QueueTap(30, 130)
	drawButtons()
	driver.QueueTap(130, 130)
	drawButtons()

	drawLongText()
	requireFrameOps("long_text", map[kryon.FrameOpKind]int{
		kryon.FrameOpColumn:    1,
		kryon.FrameOpText:      1,
		kryon.FrameOpTextField: 2,
	})
	initialLongOps := len(host.FrameOps())
	driver.SetFocus(701)
	drawLongText()
	if got := len(host.FrameOps()); got != initialLongOps {
		panic(fmt.Sprintf("long_text: frame operation count changed after focus, got %d want %d", got, initialLongOps))
	}
	for i := 0; i < 2048; i++ {
		if i > 0 && i%256 == 0 {
			driver.QueueKey(kryon.KeyTab)
			drawLongText()
			if got := len(host.FrameOps()); got != initialLongOps {
				panic(fmt.Sprintf("long_text: frame operation count changed after tab at %d, got %d want %d", i, got, initialLongOps))
			}
		}
		driver.QueueText("x")
		drawLongText()
		if got := len(host.FrameOps()); got != initialLongOps {
			panic(fmt.Sprintf("long_text: frame operation count changed after text at %d, got %d want %d", i, got, initialLongOps))
		}
		driver.QueueKey(kryon.KeyLeft)
		drawLongText()
		if got := len(host.FrameOps()); got != initialLongOps {
			panic(fmt.Sprintf("long_text: frame operation count changed after left at %d, got %d want %d", i, got, initialLongOps))
		}
		driver.QueueKey(kryon.KeyRight)
		drawLongText()
		if got := len(host.FrameOps()); got != initialLongOps {
			panic(fmt.Sprintf("long_text: frame operation count changed after right at %d, got %d want %d", i, got, initialLongOps))
		}
	}
	longFirst := text4096(longText.LongFirst)
	longSecond := text4096(longText.LongSecond)

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
		LongFirstLen:       len(longFirst),
		LongFirstCursor:    longText.LongFirstCursor,
		LongFirstHash:      checksum(longFirst),
		LongSecondLen:      len(longSecond),
		LongSecondCursor:   longText.LongSecondCursor,
		LongSecondHash:     checksum(longSecond),
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "$work/c/tests/parity/generated_form.c"
#include "$work/c/tests/parity/fields.c"
#include "$work/c/tests/parity/focus.c"
#include "$work/c/tests/parity/buttons_layout.c"
#include "$work/c/tests/parity/long_text.c"

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

static void draw_long_text(void)
{
    BeginUIFocus();
    long_text_frame_kry_draw();
    EndUIFocus();
    drain_events();
}

static unsigned long long checksum(const char *text)
{
    unsigned long long hash = 1469598103934665603ULL;
    while(*text) {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void require_long_text_node_count(int want, const char *label)
{
    int got = 0;
    (void)UIGetTreeNodes(&got);
    if(got != want) {
        fprintf(stderr,
                "long_text: retained node count changed after %s, got %d want %d\n",
                label, got, want);
        exit(1);
    }
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
    if(strcmp(first, "alpha") != 0) {
        fprintf(stderr, "form: backspace restored first field to '%s', want alpha\n", first);
        return 1;
    }

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

    draw_long_text();
    int long_text_nodes = 0;
    (void)UIGetTreeNodes(&long_text_nodes);
    SetUIFocus(701);
    draw_long_text();
    require_long_text_node_count(long_text_nodes, "focus");
    for(int i = 0; i < 2048; i++) {
        if(i > 0 && i % 256 == 0) {
            KryonInjectKeyTap(KEY_TAB);
            KryonInjectPump();
            draw_long_text();
            require_long_text_node_count(long_text_nodes, "tab");
        }
        KryonInjectText("x");
        KryonInjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "text");
        KryonInjectKeyTap(KEY_LEFT);
        KryonInjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "left");
        KryonInjectKeyTap(KEY_RIGHT);
        KryonInjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "right");
    }

    printf("{\"form_first\":\"%s\",\"form_first_cursor\":%d,\"form_second\":\"%s\",\"form_second_cursor\":%d,\"form_password\":\"%s\",\"form_password_cursor\":%d,\"form_notes\":\"%s\",\"form_notes_cursor\":%d,\"form_action\":%d,\"fields_title\":\"%s\",\"fields_title_cursor\":%d,\"fields_body\":\"%s\",\"fields_body_cursor\":%d,\"focus_one\":\"%s\",\"focus_two\":\"%s\",\"focus_three\":\"%s\",\"focus_id\":%d,\"buttons_action\":%d,\"long_first_len\":%d,\"long_first_cursor\":%d,\"long_first_hash\":%llu,\"long_second_len\":%d,\"long_second_cursor\":%d,\"long_second_hash\":%llu,\"clipboard\":\"%s\"}\n",
        first, first_cursor, second, second_cursor, password, password_cursor,
        notes, notes_cursor, form_action, title, title_cursor, body,
        body_cursor, one, two, three, GetUIFocus(), buttons_action,
        (int)strlen(long_first), long_first_cursor, checksum(long_first),
        (int)strlen(long_second), long_second_cursor, checksum(long_second),
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

printf '%s\n' '{"generated_runtime_parity":"ok","fixtures":["tests/parity/generated_form.kry","tests/parity/fields.kry","tests/parity/focus.kry","tests/parity/buttons_layout.kry","tests/parity/long_text.kry"]}'
