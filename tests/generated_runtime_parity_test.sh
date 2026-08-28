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
tests/parity/basic_controls.kry
tests/parity/list_box.kry
tests/parity/progress.kry
tests/parity/table_view.kry
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
	ControlsSlider     int32  `json:"controls_slider"`
	ControlsToggle     int32  `json:"controls_toggle"`
	ControlsCheckbox   int32  `json:"controls_checkbox"`
	ControlsSelected   int32  `json:"controls_selected"`
	ListBoxSelected    int32  `json:"list_box_selected"`
	ListBoxScroll      int32  `json:"list_box_scroll"`
	TableSelectedRow   int32  `json:"table_selected_row"`
	TableSelectedCol   int32  `json:"table_selected_column"`
	TableActivatedRow  int32  `json:"table_activated_row"`
	TableActivatedCol  int32  `json:"table_activated_column"`
	TableSortColumn    int32  `json:"table_sort_column"`
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

func drawControls() {
	host.Draw(func() {
		kryon.BeginFrame()
		BasicControls_ControlsFrame(BasicControlsStateValue)
		kryon.EndFrame()
	})
}

func drawListBox() {
	host.Draw(func() {
		kryon.BeginFrame()
		ListBox_ListBoxFrame(ListBoxStateValue)
		kryon.EndFrame()
	})
}

func drawProgress() {
	host.Draw(func() {
		kryon.BeginFrame()
		Progress_ProgressFrame(ProgressStateValue)
		kryon.EndFrame()
	})
}

func drawTableView() {
	host.Draw(func() {
		kryon.BeginFrame()
		TableView_TableFrame(TableViewStateValue)
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
	controls := BasicControlsStateValue
	listBox := ListBoxStateValue
	table := TableViewStateValue

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
	focusAfterFocus := driver.Focus()

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

	drawControls()
	requireFrameOps("controls", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect:   4,
		kryon.FrameOpText:   6,
		kryon.FrameOpButton: 3,
	})
	requireRenderedFrame("controls", 1200)
	driver.QueueTap(146, 48)
	drawControls()
	driver.QueueTap(30, 92)
	drawControls()
	driver.QueueTap(30, 138)
	drawControls()
	driver.QueueTap(30, 180)
	drawControls()
	driver.QueueTap(30, 247)
	drawControls()
	drawControls()
	if controls.SliderValue != 70 || controls.ToggleValue != 1 || controls.CheckboxValue != 1 || controls.Selected != 1 {
		panic(fmt.Sprintf("controls: got slider=%d toggle=%d checkbox=%d selected=%d, want 70,1,1,1",
			controls.SliderValue, controls.ToggleValue, controls.CheckboxValue, controls.Selected))
	}

	drawListBox()
	requireFrameOps("list_box", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect: 1,
		kryon.FrameOpText: 4,
	})
	requireRenderedFrame("list_box", 1000)
	driver.QueueTap(36, 78)
	drawListBox()
	if listBox.ListSelected != 2 || listBox.ListScroll != 0 {
		panic(fmt.Sprintf("list_box: got selected=%d scroll=%d, want 2,0",
			listBox.ListSelected, listBox.ListScroll))
	}

	drawProgress()
	requireFrameOps("progress", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect: 2,
		kryon.FrameOpText: 1,
	})
	requireRenderedFrame("progress", 700)

	drawTableView()
	requireFrameOps("table_view", map[kryon.FrameOpKind]int{
		kryon.FrameOpTable: 1,
		kryon.FrameOpText:  9,
		kryon.FrameOpRect:  1,
	})
	requireRenderedFrame("table_view", 1200)
	driver.QueueTap(116, 62)
	drawTableView()
	driver.QueueTap(116, 62)
	drawTableView()
	tableActivatedRow := table.ActivatedRow
	tableActivatedCol := table.ActivatedColumn
	driver.QueueTap(260, 30)
	drawTableView()
	if table.SelectedRow != -1 || table.SelectedColumn != 2 ||
		tableActivatedRow != 0 || tableActivatedCol != 1 || table.SortColumn != 2 {
		panic(fmt.Sprintf("table_view: got selected=(%d,%d) activated=(%d,%d) sort=%d, want (-1,2),(0,1),2",
			table.SelectedRow, table.SelectedColumn, tableActivatedRow, tableActivatedCol, table.SortColumn))
	}

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
		FocusID:            focusAfterFocus,
		ButtonsAction:      buttons.ButtonsAction,
		LongFirstLen:       len(longFirst),
		LongFirstCursor:    longText.LongFirstCursor,
		LongFirstHash:      checksum(longFirst),
		LongSecondLen:      len(longSecond),
		LongSecondCursor:   longText.LongSecondCursor,
		LongSecondHash:     checksum(longSecond),
		ControlsSlider:     controls.SliderValue,
		ControlsToggle:     controls.ToggleValue,
		ControlsCheckbox:   controls.CheckboxValue,
		ControlsSelected:   controls.Selected,
		ListBoxSelected:    listBox.ListSelected,
		ListBoxScroll:      listBox.ListScroll,
		TableSelectedRow:   table.SelectedRow,
		TableSelectedCol:   table.SelectedColumn,
		TableActivatedRow:  tableActivatedRow,
		TableActivatedCol:  tableActivatedCol,
		TableSortColumn:    table.SortColumn,
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
#include "$work/c/tests/parity/basic_controls.c"
#include "$work/c/tests/parity/list_box.c"
#include "$work/c/tests/parity/progress.c"
#include "$work/c/tests/parity/table_view.c"

static void drain_events(void)
{
    UIEvent event;
    while(NextUIEvent(&event)) {}
}

static void draw_ui(void (*fn)(void))
{
    BeginUIFrame(640, 480, 1.0f);
    BeginUI(Key("generated-runtime-parity"));
    fn();
    EndUI();
    EndUIFrame();
    drain_events();
}

static void draw_form(void)
{
    draw_ui(form_frame);
}

static void draw_fields(void)
{
    draw_ui(fields_frame);
}

static void draw_focus(void)
{
    draw_ui(focus_frame);
}

static void draw_buttons(void)
{
    draw_ui(buttons_frame);
}

static void draw_long_text(void)
{
    draw_ui(long_text_frame);
}

static void draw_controls(void)
{
    draw_ui(controls_frame);
}

static void draw_list_box(void)
{
    draw_ui(list_box_frame);
}

static void draw_progress(void)
{
    draw_ui(progress_frame);
}

static void draw_table_view(void)
{
    draw_ui(table_frame);
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
    InjectReset();

    draw_form();
    SetUIFocus(101);
    draw_form();
    InjectKeyTap(KEY_LEFT);
    InjectPump();
    draw_form();
    InjectText("é");
    InjectPump();
    draw_form();
    InjectKeyTap(KEY_BACKSPACE);
    InjectPump();
    draw_form();
    if(strcmp(first, "alpha") != 0) {
        fprintf(stderr, "form: backspace restored first field to '%s', want alpha\n", first);
        return 1;
    }

    SetUIFocus(102);
    draw_form();
    SetSelection(102, 0, 4);
    InjectText("acct");
    InjectPump();
    draw_form();

    SetUIFocus(101);
    draw_form();
    InjectKeyTap(KEY_TAB);
    InjectPump();
    draw_form();
    InjectText("Z");
    InjectPump();
    draw_form();

    SetUIClipboardTextValue("old");
    SetUIFocus(103);
    draw_form();
    SetSelection(103, 0, 6);
    InjectKey(KEY_LEFT_CONTROL, 1);
    InjectKeyTap(KEY_C);
    InjectPump();
    draw_form();
    InjectKey(KEY_LEFT_CONTROL, 0);
    InjectPump();

    draw_fields();
    InjectTap(30, 30);
    InjectPump();
    draw_fields();
    InjectKeyTap(KEY_LEFT);
    InjectPump();
    draw_fields();
    InjectText("!");
    InjectPump();
    draw_fields();
    InjectTap(30, 86);
    InjectPump();
    draw_fields();
    InjectText(" body");
    InjectPump();
    draw_fields();

    draw_focus();
    InjectTap(30, 75);
    InjectPump();
    draw_focus();
    InjectText("Z");
    InjectPump();
    draw_focus();
    InjectKey(KEY_LEFT_SHIFT, 1);
    InjectKeyTap(KEY_TAB);
    InjectPump();
    draw_focus();
    InjectKey(KEY_LEFT_SHIFT, 0);
    InjectPump();
    InjectText("A");
    InjectPump();
    draw_focus();
    int focus_after_focus = GetUIFocus();

    draw_buttons();
    InjectTap(30, 130);
    InjectPump();
    draw_buttons();
    InjectPump();
    draw_buttons();
    InjectTap(130, 130);
    InjectPump();
    draw_buttons();
    InjectPump();
    draw_buttons();

    draw_long_text();
    int long_text_nodes = 0;
    (void)UIGetTreeNodes(&long_text_nodes);
    SetUIFocus(701);
    draw_long_text();
    require_long_text_node_count(long_text_nodes, "focus");
    for(int i = 0; i < 2048; i++) {
        if(i > 0 && i % 256 == 0) {
            InjectKeyTap(KEY_TAB);
            InjectPump();
            draw_long_text();
            require_long_text_node_count(long_text_nodes, "tab");
        }
        InjectText("x");
        InjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "text");
        InjectKeyTap(KEY_LEFT);
        InjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "left");
        InjectKeyTap(KEY_RIGHT);
        InjectPump();
        draw_long_text();
        require_long_text_node_count(long_text_nodes, "right");
    }

    draw_controls();
    InjectTap(146, 48);
    InjectPump();
    draw_controls();
    InjectPump();
    draw_controls();
    InjectTap(30, 92);
    InjectPump();
    draw_controls();
    InjectPump();
    draw_controls();
    InjectTap(30, 138);
    InjectPump();
    draw_controls();
    InjectPump();
    draw_controls();
    InjectTap(30, 180);
    InjectPump();
    draw_controls();
    InjectPump();
    draw_controls();
    InjectTap(30, 247);
    InjectPump();
    draw_controls();
    InjectPump();
    draw_controls();
    draw_controls();
    if(slider_value != 70 || toggle_value != 1 || checkbox_value != 1 || selected != 1) {
        fprintf(stderr,
                "controls: got slider=%d toggle=%d checkbox=%d selected=%d, want 70,1,1,1\n",
                slider_value, toggle_value, checkbox_value, selected);
        return 1;
    }

    draw_list_box();
    InjectTap(36, 78);
    InjectPump();
    draw_list_box();
    InjectPump();
    draw_list_box();
    if(list_selected != 2 || list_scroll != 0) {
        fprintf(stderr,
                "list_box: got selected=%d scroll=%d, want 2,0\n",
                list_selected, list_scroll);
        return 1;
    }

    draw_progress();

    draw_table_view();
    InjectTap(116, 62);
    InjectPump();
    draw_table_view();
    InjectPump();
    draw_table_view();
    InjectTap(116, 62);
    InjectPump();
    draw_table_view();
    InjectPump();
    draw_table_view();
    int table_activated_row = activated_row;
    int table_activated_column = activated_column;
    InjectTap(260, 30);
    InjectPump();
    draw_table_view();
    InjectPump();
    draw_table_view();
    if(selected_row != -1 || selected_column != 2 ||
       table_activated_row != 0 || table_activated_column != 1 ||
       sort_column != 2) {
        fprintf(stderr,
                "table_view: got selected=(%d,%d) activated=(%d,%d) sort=%d, want (-1,2),(0,1),2\n",
                selected_row, selected_column, table_activated_row,
                table_activated_column, sort_column);
        return 1;
    }

    printf("{\"form_first\":\"%s\",\"form_first_cursor\":%d,\"form_second\":\"%s\",\"form_second_cursor\":%d,\"form_password\":\"%s\",\"form_password_cursor\":%d,\"form_notes\":\"%s\",\"form_notes_cursor\":%d,\"form_action\":%d,\"fields_title\":\"%s\",\"fields_title_cursor\":%d,\"fields_body\":\"%s\",\"fields_body_cursor\":%d,\"focus_one\":\"%s\",\"focus_two\":\"%s\",\"focus_three\":\"%s\",\"focus_id\":%d,\"buttons_action\":%d,\"long_first_len\":%d,\"long_first_cursor\":%d,\"long_first_hash\":%llu,\"long_second_len\":%d,\"long_second_cursor\":%d,\"long_second_hash\":%llu,\"controls_slider\":%d,\"controls_toggle\":%d,\"controls_checkbox\":%d,\"controls_selected\":%d,\"list_box_selected\":%d,\"list_box_scroll\":%d,\"table_selected_row\":%d,\"table_selected_column\":%d,\"table_activated_row\":%d,\"table_activated_column\":%d,\"table_sort_column\":%d,\"clipboard\":\"%s\"}\n",
        first, first_cursor, second, second_cursor, password, password_cursor,
        notes, notes_cursor, form_action, title, title_cursor, body,
        body_cursor, one, two, three, focus_after_focus, buttons_action,
        (int)strlen(long_first), long_first_cursor, checksum(long_first),
        (int)strlen(long_second), long_second_cursor, checksum(long_second),
        slider_value, toggle_value, checkbox_value, selected,
        list_selected, list_scroll,
        selected_row, selected_column, table_activated_row,
        table_activated_column, sort_column,
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

printf '%s\n' '{"generated_runtime_parity":"ok","fixtures":["tests/parity/generated_form.kry","tests/parity/fields.kry","tests/parity/focus.kry","tests/parity/buttons_layout.kry","tests/parity/long_text.kry","tests/parity/basic_controls.kry","tests/parity/list_box.kry","tests/parity/progress.kry","tests/parity/table_view.kry"]}'
