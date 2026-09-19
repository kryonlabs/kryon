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

# Active runtime parity executes generated C and native Go.

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
tests/parity/tree_view.kry
tests/parity/progress.kry
tests/parity/plots.kry
tests/parity/menus.kry
tests/parity/selection_images.kry
tests/parity/table_view.kry
tests/parity/scroll_content.kry
tests/parity/drag_drop.kry
tests/parity/composition.kry
tests/parity/composed_popup.kry
"
fixture_args=
for fixture in $fixtures; do
    fixture_args="$fixture_args $root/$fixture"
done

mkdir -p "$work/go" "$work/c" "$work/go-run" "$work/bin"

# shellcheck disable=SC2086
"$build/bin/k2go" --pkg main --no-main --root "$root" -o "$work/go" $fixture_args
# shellcheck disable=SC2086
"$build/bin/k2c" --root "$root" -o "$work/c" $fixture_args
sh "$root/tests/check_clean_generated_output.sh" "$work/go"
sh "$root/tests/check_clean_generated_output.sh" "$work/c"

cp "$work/go"/*.go "$work/go-run"/
cat > "$work/go-run/go.mod" <<EOF
module kryon-generated-runtime-parity

go 1.25.0

require (
	github.com/clipperhouse/uax29/v2 v2.7.0
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
	"strings"

	kryon "github.com/waozixyz/kryon/go/kryon"
)

type inputDriver interface {
	GetAccessibilitySnapshot() []kryon.AccessibilityNode
	QueueAccessibilityAction(int32, uint64, kryon.AccessibilityAction) bool
	QueueAccessibilityValue(int32, uint64, string) bool
	QueueAccessibilitySelection(int32, uint64, int32, int32) bool
	QueueAccessibilityItem(int32, uint64, int32, bool) bool
	QueueText(string)
	QueueKey(int32)
	QueueShiftKey(int32)
	QueueShortcut(int32)
	QueueTap(float32, float32)
	QueueMouseMove(float32, float32)
	QueueMouseWheel(float32)
	QueueMouseButtonDown(int32, float32, float32)
	QueueMouseButtonUp(int32, float32, float32)
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
	TreeSelected       int32  `json:"tree_selected"`
	TreeScroll         int32  `json:"tree_scroll"`
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

func drawComposition() {
	host.Draw(func() {
		kryon.BeginFrame()
		Composition_CompositionFrame(CompositionStateValue)
		kryon.EndFrame()
	})
}

func drawComposedPopupContent() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedPopupContentFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedPopupTools() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedPopupToolsFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedTooltip() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedTooltipFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedModal() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedModalFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedContext() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedContextFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedBlockedContext() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedModalFrame(ComposedPopupStateValue)
		ComposedPopup_ComposedContextFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedPopupDrag() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedPopupDragFrame(ComposedPopupStateValue)
		kryon.EndFrame()
	})
}

func drawComposedPopupShortcut() {
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedPopupShortcutFrame(ComposedPopupStateValue)
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

func drawMultiSelect() {
	host.Draw(func() {
		kryon.BeginFrame()
		SelectionImages_MultiSelectKeyboardFrame(SelectionImagesStateValue)
		kryon.EndFrame()
	})
}

func drawTabScope() {
	host.Draw(func() {
		kryon.BeginFrame()
		SelectionImages_TabScopeFrame(SelectionImagesStateValue)
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

func drawTreeView() {
	host.Draw(func() {
		kryon.BeginFrame()
		TreeView_TreeFrame(TreeViewStateValue)
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

func drawPlots() {
	host.Draw(func() {
		kryon.BeginFrame()
		Plots_PlotsFrame(PlotsStateValue)
		kryon.EndFrame()
	})
}

func drawMenus() {
	host.Draw(func() {
		kryon.BeginFrame()
		Menus_MenusFrame(MenusStateValue)
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

func requireRenderedFrame(label string, minChangedPixels int, draw func()) {
	// Rendering needs an explicit appearance pack. Unstyled controls no longer
	// invent chrome; keep interaction checks independent of pack geometry.
	if !kryon.RegisterBuiltInStylePacks() || !kryon.SetActiveStylePack("material") {
		panic("render parity: material style pack unavailable")
	}
	draw()
	defer func() {
		kryon.ClearStylePacks()
		draw()
	}()
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
	_ButtonsLayoutRuntime = host.Runtime()
	_ComposedPopupRuntime = host.Runtime()
	_DragDropRuntime = host.Runtime()
	_MenusRuntime = host.Runtime()
	_ScrollContentRuntime = host.Runtime()
	host.Draw(func() {
		kryon.BeginFrame()
		kryon.EndFrame()
	})
    drawDragDrop := func() {
        host.Draw(func() {
            kryon.BeginFrame()
            DragDrop_DragDropFrame(DragDropStateValue)
            kryon.EndFrame()
        })
    }
    driver.QueueMouseButtonDown(kryon.MouseButtonLeft,20,20)
    drawDragDrop()
    DragDropStateValue.DdPayload[0] = 'X'
    driver.QueueMouseButtonUp(kryon.MouseButtonLeft,150,20)
    drawDragDrop()
    if DragDropStateValue.DdInvalidAccepts != 0 || DragDropStateValue.DdAccepted != 8 || string(DragDropStateValue.DdReceived[:4]) != "item" {
        panic("generated drag-drop clipping or copied payload failed")
    }
	drawScroll := func() {
		host.Draw(func() {
			kryon.BeginFrame()
			ScrollContent_ScrollContentFrame(ScrollContentStateValue)
			kryon.EndFrame()
		})
	}
	drawCloseableCollapsible := func() {
		host.Draw(func() {
			kryon.BeginFrame()
			ScrollContent_CloseableCollapsibleFrame(ScrollContentStateValue)
			kryon.EndFrame()
		})
	}
	driver.QueueTap(180, 20)
	drawCloseableCollapsible()
	if ScrollContentStateValue.CloseableVisible || ScrollContentStateValue.CloseableOpen {
		panic("closeable collapsible: close did not hide without opening")
	}
	driver.QueueTap(20, 90)
	drawScroll()
	if ScrollContentStateValue.ScrollingActions != 100 {
		panic("scroll: clipped child activated or parent input was not restored")
	}
	scrollImage := kryon.RenderFrame(220, 160, host.FrameOps())
	if got := color.RGBAModel.Convert(scrollImage.At(20, 75)).(color.RGBA); got != (color.RGBA{245,245,245,255}) {
		panic("scroll: generated child painted below viewport")
	}
	if got := color.RGBAModel.Convert(scrollImage.At(120,55)).(color.RGBA); got != (color.RGBA{245,245,245,255}) {
		panic("scroll: nested content escaped parent viewport")
	}
	if got := color.RGBAModel.Convert(scrollImage.At(65,55)).(color.RGBA); got != (color.RGBA{0,121,241,255}) {
		panic("scroll: nested content did not render inside viewport")
	}
	driver.QueueMouseMove(30, 30)
	driver.QueueMouseWheel(-1)
	drawScroll()
	if ScrollContentStateValue.ScrollingOffset != 42 { panic("scroll: wheel offset") }
	driver.QueueTap(20, 45)
	drawScroll()
	if ScrollContentStateValue.ScrollingActions != 101 { panic("scroll: visible child did not activate") }
	driver.QueueMouseButtonDown(kryon.MouseButtonLeft,105,30)
	drawScroll()
	driver.QueueMouseMove(105,110)
	drawScroll()
	driver.QueueMouseButtonUp(kryon.MouseButtonLeft,105,110)
	drawScroll()
	if ScrollContentStateValue.ScrollingOffset != 140 { panic("scroll: generated thumb drag") }
	driver.QueueTap(120,65)
	drawScroll()
	if ScrollContentStateValue.ScrollingActions != 101 { panic("scroll: nested clipped child activated") }
	driver.QueueTap(70,65)
	drawScroll()
	if ScrollContentStateValue.ScrollingActions != 1101 { panic("scroll: nested visible child did not activate") }
	driver.QueueTap(250,20)
	drawScroll()
	if ScrollContentStateValue.MixedFlags != 4 { panic("scroll: mixed checkbox") }
	driver.QueueTap(250,115)
	drawScroll()
	if ScrollContentStateValue.MixedActions != 0 { panic("scroll: hidden mixed button activated") }
	driver.SetFocus(988)
	driver.QueueText("!")
	drawScroll()
	if text64(ScrollContentStateValue.MixedText) != "item!" { panic("scroll: mixed text editing") }
	driver.QueueMouseMove(250,80)
	driver.QueueMouseWheel(-1)
	drawScroll()
	driver.QueueTap(250,80)
	drawScroll()
	if ScrollContentStateValue.MixedActions != 1 || ScrollContentStateValue.MixedFlags != 4 || ScrollContentStateValue.MixedOffset != 42 {
		panic("scroll: mixed widget state or scrolled button")
	}

	for i, point := range [][2]float32{{450,20},{450,50},{470,50},{490,85},{450,20},{490,85},{450,20}} {
		driver.QueueTap(point[0],point[1])
		drawScroll()
		wantRoot := i != 4 && i != 5
		wantNested := i >= 2
		wantActions := int32(0)
		if i >= 3 { wantActions = 1 }
		if ScrollContentStateValue.BranchOpen != wantRoot || ScrollContentStateValue.NestedOpen != wantNested || ScrollContentStateValue.BranchActions != wantActions {
			panic(fmt.Sprintf("tree composition: step %d",i))
		}
	}

	driver.SetFocus(990)
	for i,key := range []int32{kryon.KeyLeft,kryon.KeyRight,kryon.KeyRight,kryon.KeyEnter,kryon.KeySpace} {
		driver.SetFocus(990)
		driver.QueueKey(key)
		drawScroll()
		if ScrollContentStateValue.BranchOpen != (i == 1 || i == 2 || i == 4) || !ScrollContentStateValue.NestedOpen {
			panic(fmt.Sprintf("tree keyboard: step %d",i))
		}
	}

	driver.QueueKey(kryon.KeyTab)
	drawScroll()
	driver.QueueKey(kryon.KeyLeft)
	drawScroll()
	if !ScrollContentStateValue.BranchOpen || ScrollContentStateValue.NestedOpen { panic("tree keyboard: tab to child") }
	for i,key := range []int32{kryon.KeyUp,kryon.KeyDown,kryon.KeyLeft,kryon.KeyRight,kryon.KeyRight,kryon.KeyDown,kryon.KeyLeft} {
		driver.QueueKey(key)
		drawScroll()
		want := int32(991)
		if i == 0 || i == 2 || i == 6 { want = 990 }
		if i == 5 { want = 995 }
		if driver.Focus() != want || !ScrollContentStateValue.BranchOpen || ScrollContentStateValue.NestedOpen != (i >= 4) {
			panic(fmt.Sprintf("tree directional focus: step %d focus %d",i,driver.Focus()))
		}
	}

	driver.QueueTap(250,190)
	drawScroll()
	driver.QueueMouseMove(250,255)
	driver.QueueMouseWheel(-1)
	drawScroll()
	if ScrollContentStateValue.OverlayBackgroundOffset != 0 { panic("background scroll stole popup wheel") }
	driver.QueueTap(250,255)
	drawScroll()
	if ScrollContentStateValue.OverlaySelected != 1 || ScrollContentStateValue.OverlayActions != 0 { panic("dropdown overlay: selection or background capture") }
	for _, escape := range []bool{true,false} {
		driver.QueueTap(250,190); drawScroll()
		if escape { driver.QueueKey(kryon.KeyEscape) } else { driver.QueueTap(220,280) }
		drawScroll()
		driver.QueueTap(250,220); drawScroll()
		if ScrollContentStateValue.OverlaySelected != 1 { panic("dropdown dismissal: stale popup row selected") }
	}
	if ScrollContentStateValue.OverlayActions != 2 { panic("dropdown dismissal: popup capture remained") }
	for _, key := range []int32{kryon.KeyHome,kryon.KeyEnd} {
		driver.SetFocus(996)
		driver.QueueKey(kryon.KeySpace); drawScroll()
		before := ScrollContentStateValue.OverlaySelected
		driver.QueueKey(key); drawScroll()
		if ScrollContentStateValue.OverlaySelected != before { panic("dropdown navigation committed before Enter") }
		driver.QueueKey(kryon.KeyEnter); drawScroll()
		want := int32(0); if key == kryon.KeyEnd { want = 1 }
		if ScrollContentStateValue.OverlaySelected != want { panic("dropdown keyboard commit failed") }
	}

	driver.SetFocus(24001)
	driver.QueueKey(kryon.KeySpace); drawScroll()
	driver.QueueKey(kryon.KeyEnd); drawScroll()
	if ScrollContentStateValue.LongDropdownSelected != 0 { panic("long dropdown navigation committed early") }
	driver.QueueTap(20,420); drawScroll()
	if ScrollContentStateValue.LongDropdownSelected != 19 { panic("long dropdown flipped viewport did not reveal last row") }
	ScrollContentStateValue.LongDropdownSelected = 0
	driver.SetFocus(24001)
	driver.QueueKey(kryon.KeySpace); drawScroll()
	driver.QueueMouseButtonDown(kryon.MouseButtonLeft,166,50); drawScroll()
	driver.QueueMouseMove(166,425); drawScroll()
	driver.QueueMouseButtonUp(kryon.MouseButtonLeft,20,60); drawScroll()
	if ScrollContentStateValue.LongDropdownSelected != 0 { panic("dropdown scrollbar drag selected a row") }
	driver.QueueTap(20,420); drawScroll()
	if ScrollContentStateValue.LongDropdownSelected != 19 { panic("dropdown scrollbar did not reveal last row") }
	ScrollContentStateValue.EdgeDropdownVisible = true
	driver.SetFocus(24002)
	driver.QueueKey(kryon.KeySpace); drawScroll()
	driver.QueueTap(490,380); drawScroll()
	if ScrollContentStateValue.EdgeDropdownSelected != 1 { panic("edge dropdown shifted popup row did not select") }
	ScrollContentStateValue.EdgeDropdownVisible = false

	driver.QueueTap(450,270); drawScroll()
	if ScrollContentStateValue.RotatedSort != -1 { panic("slanted header empty wedge sorted") }
	driver.QueueTap(590,230); drawScroll()
	if ScrollContentStateValue.RotatedSort != 0 || ScrollContentStateValue.RotatedRow != -1 { panic("rotated header hit geometry") }
	driver.QueueTap(590,290); drawScroll()
	if ScrollContentStateValue.RotatedSort != 1 { panic("slanted header second column hit") }
	driver.QueueTap(450,310); drawScroll()
	if ScrollContentStateValue.RotatedRow != 0 { panic("rotated header body geometry") }
	driver.QueueMouseButtonDown(kryon.MouseButtonLeft,600,230); drawScroll()
	driver.QueueMouseMove(620,230); drawScroll()
	driver.QueueMouseButtonUp(kryon.MouseButtonLeft,620,230); drawScroll()
	if ScrollContentStateValue.RotatedWidths[0] != 110 { panic("slanted separator resize") }

	driver.QueueTap(20,345); drawScroll()
	driver.QueueTap(180,345); drawScroll()
	if ScrollContentStateValue.CustomActions != 1 || ScrollContentStateValue.CustomFlags != 4 || ScrollContentStateValue.CustomSelected != -1 { panic("custom table cell interaction or clipping") }
	driver.QueueTap(120,345); drawScroll()
	if ScrollContentStateValue.CustomActions != 11 { panic("custom table nested row layout") }
	driver.QueueTap(340,310); drawScroll()
	driver.QueueTap(340,330); drawScroll()
	if ScrollContentStateValue.CustomOuterActions != 11 { panic("custom table surrounding layout restoration") }
	driver.QueueTap(20,380); drawScroll()
	if driver.Focus() != 1006 { panic("custom cell editor focus") }
	driver.QueueKey(kryon.KeyEnd); drawScroll()
	driver.QueueText("!"); drawScroll()
	if text64(ScrollContentStateValue.CustomText) != "cell!" { panic("custom cell editing") }
	ScrollContentStateValue.CustomDisabled = 1
	driver.QueueText("X"); drawScroll()
	if text64(ScrollContentStateValue.CustomText) != "cell!" { panic("disabled custom cell edited") }
	ScrollContentStateValue.CustomDisabled = 0
	driver.SetFocus(1006); drawScroll()
	driver.QueueText("?"); drawScroll()
	if text64(ScrollContentStateValue.CustomText) != "cell!?" { panic("custom cell editor re-enable") }

	form := GeneratedFormStateValue
	drawComposedPopupContent()
	if !ComposedPopupStateValue.PopupContentOpen { panic("generated composed popup did not open") }
	driver.QueueTap(30,70); drawComposedPopupContent()
	if ComposedPopupStateValue.PopupContentAction != 1 { panic("ordinary generated popup button did not activate") }
	ComposedPopupStateValue.PopupContentClose = true
	drawComposedPopupContent()
	if ComposedPopupStateValue.PopupContentOpen { panic("generated popup state close did not update caller state") }
	ComposedPopupStateValue.PopupContentClose = false
	host.Draw(func() {
		kryon.BeginFrame()
		ComposedPopup_ComposedPopupEarlyExit(ComposedPopupStateValue, true)
		kryon.EndFrame()
	})
	drawComposedPopupContent()
	driver.QueueTap(180, 70)
	drawComposedPopupTools()
	if ComposedPopupStateValue.PopupAction != 1 {
		panic("ordinary generated popup button did not activate")
	}
	ComposedPopupStateValue.PopupClose = true
	drawComposedPopupTools()
	if ComposedPopupStateValue.PopupOpen {
		panic("generated popup state close did not update caller state")
	}
	ComposedPopupStateValue.PopupClose = false
	ComposedPopupStateValue.PopupOpen = true
	drawComposedPopupTools()
	driver.QueueTap(180, 170)
	drawComposedPopupTools()
	if ComposedPopupStateValue.PopupOpen || ComposedPopupStateValue.PopupBackground != 0 {
		panic("outside popup dismissal leaked into background button")
	}
	driver.QueueMouseMove(30, 25)
	drawComposedTooltip()
	visibleFrames := ComposedPopupStateValue.TooltipFrames
	if visibleFrames != 1 { panic("generated arbitrary tooltip did not open on hover") }
	driver.QueueTap(30, 25)
	drawComposedTooltip()
	if ComposedPopupStateValue.TooltipFrames != visibleFrames+1 || ComposedPopupStateValue.TooltipBackground != 1 {
		panic("generated tooltip captured background input")
	}
	driver.QueueMouseMove(300, 200)
	drawComposedTooltip()
	if ComposedPopupStateValue.TooltipFrames != visibleFrames+1 { panic("generated tooltip remained open outside trigger") }
	drawComposedModal()
	if !ComposedPopupStateValue.ModalOpen || ComposedPopupStateValue.ModalFrames != 1 {
		panic("generated arbitrary modal did not open")
	}
	driver.QueueTap(290, 175)
	drawComposedModal()
	if !ComposedPopupStateValue.ModalOpen || ComposedPopupStateValue.ModalBackground != 0 {
		panic("generated modal dismissed or leaked outside input")
	}
	driver.QueueMouseButtonDown(kryon.MouseButtonRight, 30, 25)
	drawComposedBlockedContext()
	driver.QueueMouseButtonUp(kryon.MouseButtonRight, 30, 25)
	drawComposedBlockedContext()
	if ComposedPopupStateValue.ContextOpen || ComposedPopupStateValue.ContextFrames != 0 {
		panic("generated context popup opened behind a modal")
	}
	driver.QueueKey(kryon.KeyEscape)
	drawComposedModal()
	if ComposedPopupStateValue.ModalOpen { panic("generated modal ignored Escape") }
	driver.QueueMouseButtonDown(kryon.MouseButtonRight, 30, 25)
	drawComposedContext()
	if ComposedPopupStateValue.ContextOpen || ComposedPopupStateValue.ContextFrames != 0 {
		panic("generated context popup opened before right release")
	}
	driver.QueueMouseButtonUp(kryon.MouseButtonRight, 30, 25)
	drawComposedContext()
	if !ComposedPopupStateValue.ContextOpen || ComposedPopupStateValue.ContextFrames != 1 {
		panic("generated context popup did not open on right release")
	}
	driver.QueueTap(120, 80)
	drawComposedContext()
	if ComposedPopupStateValue.ContextOpen || ComposedPopupStateValue.ContextAction != 1 {
		panic("generated context popup child did not activate and close")
	}
	driver.QueueMouseButtonDown(kryon.MouseButtonLeft, 40, 40)
	drawComposedPopupDrag()
	driver.QueueMouseMove(200, 40)
	drawComposedPopupDrag()
	if ComposedPopupStateValue.PopupDragValues[0] != 170 {
		panic("generated popup drag did not retain ownership outside its bounds")
	}
	ComposedPopupStateValue.PopupDragOpen = false
	driver.QueueMouseMove(230, 40)
	drawComposedPopupDrag()
	if ComposedPopupStateValue.PopupDragValues[0] != 170 {
		panic("generated dismissed popup drag leaked into background widget")
	}
	driver.QueueMouseButtonUp(kryon.MouseButtonLeft, 230, 40)
	drawComposedPopupDrag()
	driver.QueueKey(kryon.KeyLeftControl)
	driver.QueueKey(kryon.KeyC)
	drawComposedPopupShortcut()
	if ComposedPopupStateValue.PopupShortcutInside != 1 || ComposedPopupStateValue.PopupShortcutBackground != 0 {
		panic("generated popup shortcut did not route exclusively to its owner")
	}
	ComposedPopupStateValue.PopupShortcutOpen = false
	driver.QueueKey(kryon.KeyLeftControl)
	driver.QueueKey(kryon.KeyC)
	drawComposedPopupShortcut()
	if ComposedPopupStateValue.PopupShortcutInside != 1 || ComposedPopupStateValue.PopupShortcutBackground != 1 {
		panic("generated popup shortcut did not restore background routing")
	}
	ComposedPopupStateValue.PopupShortcutOpen = true
	driver.SetFocus(27072)
	driver.QueueKey(kryon.KeyRight)
	drawComposedPopupShortcut()
	if ComposedPopupStateValue.PopupTreeBackgroundOpen {
		panic("generated background tree handled a popup-owned key")
	}
	driver.SetFocus(27071)
	driver.QueueKey(kryon.KeyRight)
	drawComposedPopupShortcut()
	if !ComposedPopupStateValue.PopupTreeInsideOpen {
		panic("generated popup tree did not handle its owned key")
	}
	ComposedPopupStateValue.PopupShortcutOpen = false
	driver.SetFocus(27072)
	driver.QueueKey(kryon.KeyRight)
	drawComposedPopupShortcut()
	if !ComposedPopupStateValue.PopupTreeBackgroundOpen {
		panic("generated background tree routing was not restored")
	}
	// Native-only composition contract: preedit never mutates committed text.
	driver.SetFocus(26100)
	host.Runtime().SubmitTextComposition(kryon.KRY_TEXT_COMPOSITION_UPDATE, "ni", 2, 0)
	drawComposition()
	if text64(CompositionStateValue.CompositionText) != "base" { panic("preedit mutated generated buffer") }
	host.Runtime().SubmitTextComposition(kryon.KRY_TEXT_COMPOSITION_COMMIT, "日本", 2, 0)
	drawComposition()
	if text64(CompositionStateValue.CompositionText) != "base日本" || CompositionStateValue.CompositionCursor != 10 {
		panic("generated composition commit failed")
	}
	host.Runtime().SubmitTextComposition(kryon.KRY_TEXT_COMPOSITION_UPDATE, "cancel", 6, 0)
	host.Runtime().SubmitTextComposition(kryon.KRY_TEXT_COMPOSITION_CANCEL, "", 0, 0)
	drawComposition()
	if text64(CompositionStateValue.CompositionText) != "base日本" { panic("composition cancellation mutated generated buffer") }
	CompositionStateValue.CompositionReadOnly = true
	for _, id := range []int32{26100, 26101} {
		driver.SetFocus(id)
		driver.QueueShortcut(kryon.KeyA); driver.QueueShortcut(kryon.KeyC)
		driver.QueueShortcut(kryon.KeyX); driver.QueueShortcut(kryon.KeyV)
		driver.QueueKey(kryon.KeyBackspace); driver.QueueKey(kryon.KeyDelete)
		driver.QueueText("blocked")
		host.Runtime().SubmitTextComposition(kryon.KRY_TEXT_COMPOSITION_COMMIT, "blocked", 7, 0)
		drawComposition()
		copied := "base日本"; if id == 26101 { copied = "area" }
		if driver.ClipboardText() != copied { panic("generated read-only copy failed") }
		if text64(CompositionStateValue.CompositionText) != "base日本" || text64(CompositionStateValue.CompositionArea) != "area" {
			panic("generated read-only editor mutated")
		}
	}
	CompositionStateValue.CompositionReadOnly = false
	driver.SetFocus(26102)
	driver.QueueShortcut(kryon.KeyRight)
	drawComposition()
	if CompositionStateValue.CompositionPageCursor != 5 {
		panic("generated TextArea Ctrl+Right did not stop at separator")
	}
	driver.QueueShiftKey(kryon.KeyLeftShift)
	driver.QueueShortcut(kryon.KeyRight)
	drawComposition()
	if CompositionStateValue.CompositionPageCursor != 6 {
		panic("generated TextArea Ctrl+Shift+Right did not extend by word")
	}
	driver.QueueShortcut(kryon.KeyC)
	drawComposition()
	if driver.ClipboardText() != "\n" {
		panic("generated TextArea Ctrl+Shift+Right selection failed")
	}
	driver.QueueKey(kryon.KeyLeft)
	drawComposition()
	driver.QueueKey(kryon.KeyLeft)
	drawComposition()
	if CompositionStateValue.CompositionPageCursor != 4 {
		panic(fmt.Sprintf("generated TextArea word-navigation reset cursor=%d, want 4", CompositionStateValue.CompositionPageCursor))
	}
	driver.QueueShiftKey(kryon.KeyDown)
	drawComposition()
	if CompositionStateValue.CompositionPageCursor != 7 {
		panic("generated TextArea Shift+Down did not move")
	}
	driver.QueueShortcut(kryon.KeyC)
	drawComposition()
	if driver.ClipboardText() != "1\nc" {
		panic("generated TextArea Shift+Down did not select")
	}
	driver.QueueKey(kryon.KeyLeft)
	drawComposition()
	if CompositionStateValue.CompositionPageCursor != 4 {
		panic("generated TextArea Left did not collapse selection")
	}
	driver.QueueKey(kryon.KeyDown); drawComposition()
	if CompositionStateValue.CompositionPageCursor <= 4 { panic("generated TextArea Down did not move") }
	beforePage := CompositionStateValue.CompositionPageCursor
	driver.QueueKey(kryon.KeyPageDown); drawComposition()
	if CompositionStateValue.CompositionPageCursor < beforePage+4 { panic("generated TextArea PageDown did not move a page") }
	beforePage = CompositionStateValue.CompositionPageCursor
	driver.QueueKey(kryon.KeyPageUp); drawComposition()
	if CompositionStateValue.CompositionPageCursor > beforePage-4 { panic("generated TextArea PageUp did not move a page") }
	pageTextBefore := text64(CompositionStateValue.CompositionPageArea)
	driver.QueueKey(kryon.KeyEnter); drawComposition()
	pageTextAfter := text64(CompositionStateValue.CompositionPageArea)
	if len(pageTextAfter) != len(pageTextBefore)+1 || strings.Count(pageTextAfter, "\n") != strings.Count(pageTextBefore, "\n")+1 { panic("generated TextArea Enter did not insert newline") }
	driver.QueueShortcut(kryon.KeyEnd); drawComposition()
	driver.QueueShortcut(kryon.KeyBackspace); drawComposition()
	wordDeletedText := text64(CompositionStateValue.CompositionPageArea)
	if len(wordDeletedText) != len(pageTextAfter)-2 || !strings.HasSuffix(wordDeletedText, "\n") {
		panic("generated TextArea Ctrl+Backspace did not delete final word")
	}
	fields := FieldsStateValue
	focus := FocusStateValue
	buttons := ButtonsLayoutStateValue
	longText := LongTextStateValue
	controls := BasicControlsStateValue
	listBox := ListBoxStateValue
	treeView := TreeViewStateValue
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
	requireRenderedFrame("form", 2500, drawForm)
	driver.SetFocus(101)
	drawForm()
	accessibleFields := 0
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID < 101 || node.FocusID > 104 { continue }
		accessibleFields++
		if node.Role != "textbox" || node.Disabled { panic("generated editor accessibility role/state mismatch") }
		switch node.FocusID {
		case 101:
			if !node.Focused || node.Value != "alpha" { panic("generated editor accessibility focus/value mismatch") }
		case 102:
			if node.Value != "beta" { panic("generated editor accessibility value mismatch") }
		case 103:
			if !node.Secure || node.Value != "" || node.Label != "" { panic("generated editor accessibility leaked password") }
		case 104:
			if !node.Multiline || node.Value != "notes" || node.Label != "Notes" { panic("generated area accessibility mismatch") }
		}
	}
	if accessibleFields != 4 { panic("generated editor accessibility nodes missing") }
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 101 {
			if !driver.QueueAccessibilityValue(101, node.Generation, "Ae\u0301Z") ||
				!driver.QueueAccessibilitySelection(101, node.Generation, 3, 99) {
				panic("generated editor value/selection request rejected")
			}
		}
		if node.FocusID == 104 && !driver.QueueAccessibilityValue(104, node.Generation, "a\r\nb\tc") {
			panic("generated area value request rejected")
		}
	}
	drawForm()
	if text64(form.First) != "Ae\u0301Z" || form.FirstCursor != 5 || text128(form.Notes) != "a\r\nb\tc" {
		panic("generated editor value/selection not delivered")
	}
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 101 {
			if node.SelectionAnchor != 1 || node.SelectionCursor != 5 { panic("generated editor selection split grapheme") }
			if !driver.QueueAccessibilityValue(101, node.Generation, "alpha") { panic("generated editor reset rejected") }
		}
		if node.FocusID == 104 && !driver.QueueAccessibilityValue(104, node.Generation, "notes") {
			panic("generated area reset rejected")
		}
	}
	drawForm()
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 201 && !driver.QueueAccessibilityAction(201, node.Generation, kryon.AccessibilityActionActivate) {
			panic("generated button accessibility action rejected")
		}
	}
	drawForm()
	if form.FormAction != 1 { panic("generated button accessibility action not delivered") }
	drawForm()
	if form.FormAction != 1 { panic("generated button accessibility action replayed") }
	form.FormAction = 0
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 101 && !driver.QueueAccessibilityAction(101, node.Generation, kryon.AccessibilityActionFocus) {
			panic("generated editor accessibility focus rejected")
		}
	}
	drawForm()
	if driver.Focus() != 101 { panic("generated editor accessibility focus not delivered") }
	driver.QueueKey(kryon.KeyLeft)
	drawForm()
	driver.QueueText("é")
	drawForm()
	driver.QueueKey(kryon.KeyBackspace)
	drawForm()
	if got := text64(form.First); got != "alpha" {
		panic(fmt.Sprintf("form: backspace restored first field to %q, want alpha", got))
	}
	for _, cluster := range []string{"e\u0301", "\U0001f469\u200d\U0001f4bb", "\U0001f1f5\U0001f1fe", "\U0001f44d\U0001f3fd", "\u0915\u094d\u0937"} {
		for _, key := range []int32{kryon.KeyDelete, kryon.KeyBackspace} {
			clear(form.First[:])
			copy(form.First[:], "A"+cluster+"Z")
			form.FirstCursor = int32(1 + len(cluster))
			driver.SetSelection(101, form.FirstCursor, form.FirstCursor)
			if key == kryon.KeyDelete {
				driver.QueueKey(kryon.KeyLeft)
				drawForm()
				if form.FirstCursor != 1 { panic("generated TextField left split grapheme") }
			}
			driver.QueueKey(key)
			drawForm()
			if text64(form.First) != "AZ" || form.FirstCursor != 1 { panic("generated TextField deletion split grapheme") }
		}
	}
	clear(form.First[:])
	copy(form.First[:], "alpha")
	form.FirstCursor = 4
	driver.SetSelection(101, 4, 4)

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
	requireRenderedFrame("fields", 1200, drawFields)
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
		kryon.FrameOpButton: 3,
	})
	requireRenderedFrame("buttons", 1000, drawButtons)
	driver.QueueTap(30, 130)
	drawButtons()
	driver.QueueTap(130, 130)
	drawButtons()
	if buttons.ButtonsAction != 10 {
		panic("first widget instance action was not isolated")
	}
	driver.QueueTap(230, 130)
	drawButtons()
	if buttons.ButtonsAction != 110 {
		panic("second widget instance action was not isolated")
	}
	buttons.ButtonsReverse = true
	drawButtons()
	driver.QueueTap(130, 130)
	drawButtons()
	driver.QueueTap(230, 130)
	drawButtons()
	if buttons.ButtonsAction != 220 {
		panic("widget instance actions changed after reordering")
	}
	buttons.ButtonsReverse = false
	drawButtons()
	// Native keyboard parity; preserve the shared C/Go/JS pointer result below.
	buttonPointerAction := buttons.ButtonsAction
	driver.SetFocus(502); driver.QueueKey(kryon.KeyEnter); drawButtons()
	driver.SetFocus(501); driver.QueueKey(kryon.KeySpace); drawButtons()
	driver.QueueKey(kryon.KeyTab); drawButtons()
	if driver.Focus() != 502 { panic("generated button Tab did not skip disabled control") }
	driver.QueueKey(kryon.KeySpace); drawButtons()
	if buttons.ButtonsAction != buttonPointerAction + 20 {
		panic("generated button keyboard activation or disabled gating failed")
	}
	buttons.ButtonsAction = buttonPointerAction

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
		kryon.FrameOpRect:   5,
		kryon.FrameOpText:   10,
		kryon.FrameOpButton: 1,
	})
	requireRenderedFrame("controls", 50, drawControls)
	driver.QueueTap(346, 236)
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
	driver.SetFocus(802)
	driver.QueueKey(kryon.KeySpace)
	drawControls()
	if controls.ToggleValue != 0 {
		panic("controls: generated Toggle rejected keyboard toggle")
	}
	driver.QueueKey(kryon.KeySpace)
	drawControls()
	if controls.ToggleValue != 1 {
		panic("controls: generated Toggle did not restore state")
	}
	driver.SetFocus(802)
	driver.QueueKey(kryon.KeyTab)
	drawControls()
	if driver.Focus() != 803 {
		panic(fmt.Sprintf("controls: generated Toggle Tab focus=%d, want 803", driver.Focus()))
	}
	driver.SetFocus(803)
	driver.QueueKey(kryon.KeySpace)
	drawControls()
	if controls.CheckboxValue != 0 {
		panic("controls: generated Checkbox rejected keyboard toggle")
	}
	driver.QueueKey(kryon.KeySpace)
	drawControls()
	driver.SetFocus(805)
	driver.QueueKey(kryon.KeyEnter)
	drawControls()
	driver.SetFocus(806)
	driver.QueueKey(kryon.KeySpace)
	drawControls()
	driver.SetFocus(807)
	driver.QueueKey(kryon.KeyEnter)
	drawControls()
	if controls.ChoiceSelected != 1 || controls.ChoiceFlags != 4 || controls.ChoiceRadioActions != 1 {
		panic(fmt.Sprintf("controls: generated choice keyboard state=%d/%d/%d, want 1/4/1",
			controls.ChoiceSelected, controls.ChoiceFlags, controls.ChoiceRadioActions))
	}
	controls.ChoiceSelected, controls.ChoiceFlags, controls.ChoiceRadioActions = 0, 0, 0
	accessibleRadio := false
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 807 {
			accessibleRadio = node.Role == "radio" && node.Label == "Radio" &&
				driver.QueueAccessibilityAction(807, node.Generation, kryon.AccessibilityActionActivate)
		}
	}
	if !accessibleRadio { panic("generated radio accessibility missing") }
	drawControls()
	if controls.ChoiceRadioActions != 1 { panic("generated radio accessibility action not delivered") }
	controls.ChoiceRadioActions = 0
	driver.SetFocus(805)
	driver.QueueKey(kryon.KeyTab)
	drawControls()
	if driver.Focus() != 806 {
		panic(fmt.Sprintf("controls: generated choice Tab focus=%d, want 806", driver.Focus()))
	}
	drawMultiSelect()
	driver.SetFocus(957)
	driver.QueueKey(kryon.KeyDown)
	drawMultiSelect()
	if SelectionImagesStateValue.MultiAnchor != 1 || SelectionImagesStateValue.MultiCount != 1 || SelectionImagesStateValue.MultiSelected != [3]int32{0, 1, 0} {
		panic(fmt.Sprintf("multi_select: generated Down state=%v/%d/%d", SelectionImagesStateValue.MultiSelected, SelectionImagesStateValue.MultiCount, SelectionImagesStateValue.MultiAnchor))
	}
	driver.QueueKey(kryon.KeyLeftShift)
	driver.QueueKey(kryon.KeyDown)
	drawMultiSelect()
	if SelectionImagesStateValue.MultiAnchor != 2 || SelectionImagesStateValue.MultiCount != 2 || SelectionImagesStateValue.MultiSelected != [3]int32{0, 1, 1} {
		panic(fmt.Sprintf("multi_select: generated Shift+Down state=%v/%d/%d", SelectionImagesStateValue.MultiSelected, SelectionImagesStateValue.MultiCount, SelectionImagesStateValue.MultiAnchor))
	}
	drawTabScope()
	driver.QueueTap(60, 84)
	drawTabScope()
	if SelectionImagesStateValue.TabFirstActions != 1 || SelectionImagesStateValue.TabSecondActions != 0 {
		panic("tab scope: selected first child did not own input")
	}
	driver.SetFocus(958)
	driver.QueueKey(kryon.KeyRight)
	drawTabScope()
	driver.QueueTap(60, 84)
	drawTabScope()
	if SelectionImagesStateValue.TabScopeSelected != 1 || SelectionImagesStateValue.TabFirstActions != 1 || SelectionImagesStateValue.TabSecondActions != 1 {
		panic("tab scope: selection did not switch arbitrary child content")
	}

	drawListBox()
	requireFrameOps("list_box", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect: 1,
		kryon.FrameOpText: 4,
	})
	listNodes := driver.GetAccessibilitySnapshot()
	listItems := 0
	for _, node := range listNodes {
		if node.Role == "option" {
			listItems++
			if node.Parent == 0 || listNodes[node.Parent-1].FocusID != 801 || node.Key != uint64(11+node.ItemIndex) {
				panic("generated list option hierarchy/key mismatch")
			}
		}
		if node.FocusID == 801 && !driver.QueueAccessibilityItem(801, node.Generation, 3, true) {
			panic("generated list selection rejected")
		}
	}
	if listItems != 4 { panic("generated list options missing") }
	drawListBox()
	if listBox.ListSelected != 3 { panic("generated list selection not delivered") }
	for _, node := range driver.GetAccessibilitySnapshot() {
		if node.FocusID == 801 && !driver.QueueAccessibilityAction(801, node.Generation, kryon.AccessibilityActionClearSelection) {
			panic("generated list clear rejected")
		}
	}
	drawListBox()
	if listBox.ListSelected != -1 { panic("generated list clear not delivered") }
	driver.QueueTap(36, 78)
	drawListBox()
	driver.SetFocus(0); driver.QueueKey(kryon.KeyTab); drawListBox()
	if driver.Focus() != 801 { panic("generated list Tab focus failed") }
	driver.SetFocus(801); driver.QueueKey(kryon.KeyEnd); drawListBox()
	driver.QueueKey(kryon.KeyHome); drawListBox()
	driver.QueueKey(kryon.KeyEnd); drawListBox()
	driver.QueueKey(kryon.KeyUp); drawListBox()
	if listBox.ListSelected != 2 || listBox.ListScroll != 0 {
		panic(fmt.Sprintf("list_box: got selected=%d scroll=%d, want 2,0",
			listBox.ListSelected, listBox.ListScroll))
	}

	driver.SetFocus(940); driver.QueueKey(kryon.KeyDown); drawMenus()
	if MenusStateValue.OpenMenu != 0 { panic("generated menu Down did not open") }
	driver.QueueKey(kryon.KeyEnd); drawMenus()
	driver.QueueKey(kryon.KeyRight); drawMenus()
	driver.QueueKey(kryon.KeyEnter); drawMenus()
	if MenusStateValue.MenuAction != 23 || MenusStateValue.OpenMenu != -1 {
		panic("generated submenu keyboard activation failed")
	}

	drawTreeView()
	driver.QueueTap(36, 84)
	drawTreeView()
	if treeView.TreeSelected != 2 || treeView.TreeScroll != 0 {
		panic(fmt.Sprintf("tree_view: got selected=%d scroll=%d, want 2,0",
			treeView.TreeSelected, treeView.TreeScroll))
	}

	drawProgress()
	drawPlots()
	requireFrameOps("plots", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect: 6,
		kryon.FrameOpLine: 3,
		kryon.FrameOpText: 3,
	})
	driver.SetFocus(920)
	driver.QueueKey(kryon.KeyRight)
	drawPlots()
	if value := PlotsStateValue.PlotsScalarValues[0]; value < 1.0999 || value > 1.1001 {
		panic(fmt.Sprintf("generated float drag keyboard value=%v, want 1.1", value))
	}
	driver.SetFocus(921)
	driver.QueueKey(kryon.KeyRight)
	drawPlots()
	if value := PlotsStateValue.PlotsWholeValues[0]; value != 4 {
		panic(fmt.Sprintf("generated int drag keyboard value=%d, want 4", value))
	}
	driver.SetFocus(938)
	driver.QueueKey(kryon.KeyRight)
	drawPlots()
	if value := PlotsStateValue.PlotsScalarRangeMin; value < 2.0999 || value > 2.1001 {
		panic(fmt.Sprintf("generated float range drag keyboard value=%v, want 2.1", value))
	}
	driver.SetFocus(922)
	driver.QueueKey(kryon.KeyRight)
	drawPlots()
	if value := PlotsStateValue.PlotsSliderValues[0]; value < 0.2599 || value > 0.2601 {
		panic(fmt.Sprintf("generated float slider keyboard value=%v, want 0.26", value))
	}
	driver.SetFocus(925)
	driver.QueueKey(kryon.KeyUp)
	drawPlots()
	if value := PlotsStateValue.PlotsSliderDiscreteValues[0]; value != 3 {
		panic(fmt.Sprintf("generated vertical int slider keyboard value=%d, want 3", value))
	}
	driver.QueueKey(kryon.KeyLeftControl)
	driver.QueueTap(30, 190)
	drawPlots()
	driver.QueueShortcut(kryon.KeyA)
	driver.QueueText("12.5")
	drawPlots()
	if value := PlotsStateValue.PlotsScalarValues[0]; value != 12.5 {
		panic(fmt.Sprintf("generated float drag temporary input=%v, want 12.5", value))
	}
	driver.QueueKey(kryon.KeyEnter)
	drawPlots()
	driver.QueueTap(145, 190)
	drawPlots()
	driver.QueueTap(145, 190)
	drawPlots()
	driver.QueueShortcut(kryon.KeyA)
	driver.QueueText("17")
	drawPlots()
	if value := PlotsStateValue.PlotsWholeValues[0]; value != 17 {
		panic(fmt.Sprintf("generated int drag double-click input=%d, want 17", value))
	}
	driver.QueueKey(kryon.KeyEnter)
	drawPlots()
	driver.QueueKey(kryon.KeyLeftControl)
	driver.QueueTap(330, 260)
	drawPlots()
	driver.QueueShortcut(kryon.KeyA)
	driver.QueueText("19")
	drawPlots()
	if value := PlotsStateValue.PlotsSliderDiscreteValues[0]; value != 19 {
		panic(fmt.Sprintf("generated int slider temporary input=%d, want 19", value))
	}
	driver.SetFocus(0x60000008)
	driver.QueueKey(kryon.KeySpace)
	drawPlots()
	if value := PlotsStateValue.PlotsInputWholeValues[0]; value != 5 {
		panic(fmt.Sprintf("generated input step keyboard value=%d, want 5", value))
	}
	requireFrameOps("progress", map[kryon.FrameOpKind]int{
		kryon.FrameOpRect: 2,
		kryon.FrameOpText: 1,
	})
	requireRenderedFrame("progress", 700, drawProgress)

	drawTableView()
	requireFrameOps("table_view", map[kryon.FrameOpKind]int{
		kryon.FrameOpTable: 1,
		kryon.FrameOpText:  9,
		kryon.FrameOpRect:  1,
	})
	requireRenderedFrame("table_view", 50, drawTableView)
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
	driver.SetFocus(901)
	driver.QueueKey(kryon.KeyTab)
	drawTableView()
	if table.SelectedRow != 1 || table.SelectedColumn != 0 || driver.Focus() != 901 {
		panic(fmt.Sprintf("table_view tab: got selected=(%d,%d) focus=%d, want (1,0),901",
			table.SelectedRow, table.SelectedColumn, driver.Focus()))
	}
	driver.QueueShiftKey(kryon.KeyTab)
	drawTableView()
	if table.SelectedRow != 0 || table.SelectedColumn != 2 || driver.Focus() != 901 {
		panic(fmt.Sprintf("table_view shift-tab: got selected=(%d,%d) focus=%d, want (0,2),901",
			table.SelectedRow, table.SelectedColumn, driver.Focus()))
	}
	driver.QueueKey(kryon.KeyLeft)
	drawTableView()
	if table.SelectedRow != 0 || table.SelectedColumn != 1 {
		panic(fmt.Sprintf("table_view keyboard left: got (%d,%d), want (0,1)",
			table.SelectedRow, table.SelectedColumn))
	}
	driver.QueueKey(kryon.KeyDown)
	driver.QueueKey(kryon.KeyF2)
	drawTableView()
	if table.SelectedRow != 1 || table.SelectedColumn != 1 ||
		table.ActivatedRow != 1 || table.ActivatedColumn != 1 {
		panic(fmt.Sprintf("table_view keyboard activation: got selected=(%d,%d) activated=(%d,%d), want (1,1),(1,1)",
			table.SelectedRow, table.SelectedColumn, table.ActivatedRow, table.ActivatedColumn))
	}
	driver.QueueShortcut(kryon.KeyC)
	drawTableView()
	if driver.ClipboardText() != "Wallet" {
		panic(fmt.Sprintf("table_view cell copy: got %q, want Wallet", driver.ClipboardText()))
	}
	driver.SetClipboardText("generated-paste")
	driver.QueueShortcut(kryon.KeyV)
	drawTableView()
	if table.PastedText != "generated-paste" || table.PastedRow != 1 || table.PastedColumn != 1 {
		panic(fmt.Sprintf("table_view paste: got %q at (%d,%d), want generated-paste at (1,1)",
			table.PastedText, table.PastedRow, table.PastedColumn))
	}
	driver.SetClipboardText("old")
	table.SelectedRow, table.SelectedColumn = -1, 2

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
		TreeSelected:       treeView.TreeSelected,
		TreeScroll:         treeView.TreeScroll,
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

# Let the disposable fixture follow the native runtime's pinned transitive
# dependencies without modifying the repository's module files.
(cd "$work/go-run" && GOWORK=off GOCACHE=${GOCACHE:-$work/go-cache} go run -mod=mod . > "$work/go.json")

cat > "$work/c_runner.c" <<EOF
#include "kryon.h"
#include "ui_internal.h"
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
#include "$work/c/tests/parity/tree_view.c"
#include "$work/c/tests/parity/progress.c"
#include "$work/c/tests/parity/plots.c"
#include "$work/c/tests/parity/menus.c"
#include "$work/c/tests/parity/selection_images.c"
#include "$work/c/tests/parity/table_view.c"
#include "$work/c/tests/parity/scroll_content.c"
#include "$work/c/tests/parity/drag_drop.c"
#include "$work/c/tests/parity/composition.c"
#include "$work/c/tests/parity/composed_popup.c"

static void drain_events(void)
{
    Event event;
    while(NextEvent(&event)) {}
}

static void draw_ui(void (*fn)(void))
{
    BeginInterfaceFrame(640, 480, 1.0f);
    BeginTree(Key("generated-runtime-parity"));
    fn();
    EndTree();
    EndInterfaceFrame();
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

static void draw_composition(void) { draw_ui(composition_frame); }
static void draw_tab_scope(void) { draw_ui(tab_scope_frame); }
static void draw_composed_popup_content(void) { draw_ui(composed_popup_content_frame); }
static void draw_composed_popup_tools(void) { draw_ui(composed_popup_tools_frame); }
static void draw_composed_tooltip(void) { draw_ui(composed_tooltip_frame); }
static void draw_composed_modal(void) { draw_ui(composed_modal_frame); }
static void draw_composed_context(void) { draw_ui(composed_context_frame); }

static void composed_blocked_context_frame(void)
{
    composed_modal_frame();
    composed_context_frame();
}

static void draw_composed_blocked_context(void)
{
    draw_ui(composed_blocked_context_frame);
}

static void draw_composed_popup_drag(void) { draw_ui(composed_popup_drag_frame); }
static void draw_composed_popup_shortcut(void) { draw_ui(composed_popup_shortcut_frame); }

static void draw_long_text(void)
{
    draw_ui(long_text_frame);
}

static void draw_controls(void)
{
    draw_ui(controls_frame);
}

static void draw_multi_select(void)
{
    draw_ui(multi_select_keyboard_frame);
}

static void draw_list_box(void)
{
    draw_ui(list_box_frame);
}

static void draw_tree_view(void)
{
    draw_ui(tree_frame);
}

static void draw_progress(void)
{
    draw_ui(progress_frame);
}

static void draw_plots(void)
{
    draw_ui(plots_frame);
}

static void draw_menus(void)
{
    draw_ui(menus_frame);
}

static void draw_table_view(void)
{
    draw_ui(table_frame);
}

static void draw_closeable_collapsible(void)
{
    draw_ui(closeable_collapsible_frame);
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
    (void)GetTreeNodes(&got);
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
    BeginInterfaceFrame(640, 480, 1.0f);
    EndInterfaceFrame();
    InjectTap(180,20);
    InjectPump(); draw_closeable_collapsible();
    InjectPump(); draw_closeable_collapsible();
    if(closeable_visible || closeable_open) {
        fprintf(stderr,"closeable collapsible: close did not hide without opening\n");
        return 1;
    }
    draw_tab_scope();
    InjectTap(60,84); InjectPump(); draw_tab_scope();
    InjectPump(); draw_tab_scope();
    if(tab_first_actions != 1 || tab_second_actions != 0) {
        fprintf(stderr,"tab scope: selected first child did not own input\n"); return 1;
    }
    SetFocus(958); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_tab_scope();
    InjectTap(60,84); InjectPump(); draw_tab_scope();
    InjectPump(); draw_tab_scope();
    if(tab_scope_selected != 1 || tab_first_actions != 1 ||
       tab_second_actions != 1) {
        fprintf(stderr,"tab scope: selection did not switch arbitrary child content\n"); return 1;
    }
    draw_composed_popup_content();
    if(!popup_content_open) { fprintf(stderr,"generated composed popup did not open\n"); return 1; }
    InjectTap(30,70); InjectPump(); draw_composed_popup_content();
    InjectPump(); draw_composed_popup_content();
    if(popup_content_action != 1) { fprintf(stderr,"ordinary generated popup button did not activate\n"); return 1; }
    popup_content_close = 1; draw_composed_popup_content();
    if(popup_content_open) { fprintf(stderr,"generated popup state close did not update caller state\n"); return 1; }
    popup_content_close = 0;
    BeginInterfaceFrame(640,480,1);
    composed_popup_early_exit(1);
    EndInterfaceFrame();
    draw_composed_popup_content();
    InjectTap(180,70); InjectPump(); draw_composed_popup_tools();
    InjectPump(); draw_composed_popup_tools();
    if(popup_action != 1) { fprintf(stderr,"ordinary generated popup button did not activate\n"); return 1; }
    popup_close = 1; draw_composed_popup_tools();
    if(popup_open) { fprintf(stderr,"generated popup state close did not update caller state\n"); return 1; }
    popup_close = 0; popup_open = 1; draw_composed_popup_tools();
    InjectTap(180,170); InjectPump(); draw_composed_popup_tools();
    InjectPump(); draw_composed_popup_tools();
    if(popup_open || popup_background != 0) {
        fprintf(stderr,"outside popup dismissal leaked into background button\n"); return 1;
    }
    InjectMousePosition(30,25); InjectPump(); draw_composed_tooltip();
    int visible_tooltip_frames = tooltip_frames;
    if(visible_tooltip_frames != 1) {
        fprintf(stderr,"generated arbitrary tooltip did not open on hover\n"); return 1;
    }
    InjectTap(30,25); InjectPump(); draw_composed_tooltip();
    InjectPump(); draw_composed_tooltip();
    if(tooltip_frames <= visible_tooltip_frames || tooltip_background != 1) {
        fprintf(stderr,"generated tooltip captured background input\n"); return 1;
    }
    visible_tooltip_frames = tooltip_frames;
    InjectMousePosition(300,200); InjectPump(); draw_composed_tooltip();
    if(tooltip_frames != visible_tooltip_frames) {
        fprintf(stderr,"generated tooltip remained open outside trigger\n"); return 1;
    }
    draw_composed_modal();
    if(!modal_open || modal_frames != 1) {
        fprintf(stderr,"generated arbitrary modal did not open\n"); return 1;
    }
    InjectTap(290,175); InjectPump(); draw_composed_modal();
    InjectPump(); draw_composed_modal();
    if(!modal_open || modal_background != 0) {
        fprintf(stderr,"generated modal dismissed or leaked outside input\n"); return 1;
    }
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,1);
    InjectPump();
    draw_composed_blocked_context();
    InjectMouseButton(MOUSE_BUTTON_RIGHT,0);
    InjectPump();
    draw_composed_blocked_context();
    if(context_open || context_frames != 0) {
        fprintf(stderr,"generated context popup opened behind a modal\n");
        return 1;
    }
    InjectKeyTap(KEY_ESCAPE); InjectPump(); draw_composed_modal();
    if(modal_open) {
        fprintf(stderr,"generated modal ignored Escape\n"); return 1;
    }
    InjectMousePosition(30,25); InjectMouseButton(MOUSE_BUTTON_RIGHT,1);
    InjectPump(); draw_composed_context();
    if(context_open || context_frames != 0) {
        fprintf(stderr,"generated context popup opened before right release\n"); return 1;
    }
    InjectMousePosition(30,25); InjectMouseButton(MOUSE_BUTTON_RIGHT,0);
    InjectPump(); draw_composed_context();
    if(!context_open || context_frames != 1) {
        fprintf(stderr,"generated context popup did not open on right release\n"); return 1;
    }
    InjectTap(120,80); InjectPump(); draw_composed_context();
    InjectPump(); draw_composed_context();
    if(context_open || context_action != 1) {
        fprintf(stderr,"generated context popup child did not activate and close\n"); return 1;
    }
    InjectMousePosition(40,40); InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump(); draw_composed_popup_drag();
    InjectMousePosition(200,40); InjectPump(); draw_composed_popup_drag();
    if((int)popup_drag_values[0] != 170) {
        fprintf(stderr,"generated popup drag did not retain ownership outside its bounds\n"); return 1;
    }
    popup_drag_open = 0;
    InjectMousePosition(230,40); InjectPump(); draw_composed_popup_drag();
    if((int)popup_drag_values[0] != 170) {
        fprintf(stderr,"generated dismissed popup drag leaked into background widget: %.1f\n",
                popup_drag_values[0]);
        return 1;
    }
    InjectMouseButton(MOUSE_BUTTON_LEFT,0); InjectPump(); draw_composed_popup_drag();
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_C); InjectPump();
    draw_composed_popup_shortcut();
    if(popup_shortcut_inside != 1 || popup_shortcut_background != 0) {
        fprintf(stderr,"generated popup shortcut did not route exclusively to its owner\n");
        return 1;
    }
    popup_shortcut_open = 0;
    InjectPump();
    InjectKeyTap(KEY_C); InjectPump(); draw_composed_popup_shortcut();
    if(popup_shortcut_inside != 1 || popup_shortcut_background != 1) {
        fprintf(stderr,"generated popup shortcut did not restore background routing: inside=%d background=%d\n",
                popup_shortcut_inside,popup_shortcut_background);
        return 1;
    }
    InjectKey(KEY_C,0); InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    popup_shortcut_open = 1;
    SetFocus(27072); InjectKeyTap(KEY_RIGHT); InjectPump();
    draw_composed_popup_shortcut();
    if(popup_tree_background_open) {
        fprintf(stderr,"generated background tree handled a popup-owned key\n"); return 1;
    }
    InjectPump(); SetFocus(27071); InjectKeyTap(KEY_RIGHT); InjectPump();
    draw_composed_popup_shortcut();
    if(!popup_tree_inside_open) {
        fprintf(stderr,"generated popup tree did not handle its owned key\n"); return 1;
    }
    InjectPump(); popup_shortcut_open = 0;
    SetFocus(27072); InjectKeyTap(KEY_RIGHT); InjectPump();
    draw_composed_popup_shortcut();
    if(!popup_tree_background_open) {
        fprintf(stderr,"generated background tree routing was not restored\n"); return 1;
    }
    InjectKey(KEY_RIGHT,0); InjectPump();
    SetFocus(26100);
    SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"ni",2,0);
    draw_composition();
    if(strcmp(composition_text,"base") != 0) {
        fprintf(stderr,"preedit mutated generated buffer\n"); return 1;
    }
    SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,"日本",2,0);
    draw_composition();
    if(strcmp(composition_text,"base日本") != 0 || composition_cursor != 10) {
        fprintf(stderr,"generated composition commit failed\n"); return 1;
    }
    SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"cancel",6,0);
    SubmitTextComposition(KRY_TEXT_COMPOSITION_CANCEL,"",0,0);
    draw_composition();
    if(strcmp(composition_text,"base日本") != 0) {
        fprintf(stderr,"composition cancellation mutated generated buffer\n"); return 1;
    }
    composition_read_only = 1;
    for(int id = 26100; id <= 26101; id++) {
        SetFocus(id);
        InjectKey(KEY_LEFT_CONTROL,1);
        InjectKeyTap(KEY_A); InjectKeyTap(KEY_C); InjectKeyTap(KEY_X); InjectKeyTap(KEY_V);
        InjectKeyTap(KEY_BACKSPACE); InjectKeyTap(KEY_DELETE); InjectText("blocked");
        SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,"blocked",7,0);
        InjectPump(); draw_composition();
        if(strcmp(GetClipboardTextValue(),id == 26100 ? "base日本" : "area") != 0 ||
           strcmp(composition_text,"base日本") != 0 || strcmp(composition_area,"area") != 0) {
            fprintf(stderr,"generated read-only copy or mutation guard failed\n"); return 1;
        }
        InjectReset();
    }
    composition_read_only = 0;
    SetFocus(26102);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_RIGHT);
    InjectPump();
    draw_composition();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    if(composition_page_cursor != 5) {
        fprintf(stderr,"generated TextArea Ctrl+Right did not stop at separator\n");
        return 1;
    }
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKey(KEY_LEFT_SHIFT,1);
    InjectKeyTap(KEY_RIGHT);
    InjectPump();
    draw_composition();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectKey(KEY_LEFT_SHIFT,0);
    InjectPump();
    if(composition_page_cursor != 6) {
        fprintf(stderr,"generated TextArea Ctrl+Shift+Right did not extend by word\n");
        return 1;
    }
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_C);
    InjectPump();
    draw_composition();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    if(strcmp(GetClipboardTextValue(),"\n") != 0) {
        fprintf(stderr,"generated TextArea Ctrl+Shift+Right selection failed\n");
        return 1;
    }
    InjectKeyTap(KEY_LEFT); InjectPump(); draw_composition(); InjectPump();
    InjectKeyTap(KEY_LEFT); InjectPump(); draw_composition();
    if(composition_page_cursor != 4) {
        fprintf(stderr,"generated TextArea word-navigation reset cursor=%d, want 4\n",
                composition_page_cursor);
        return 1;
    }
    InjectKey(KEY_LEFT_SHIFT,1);
    InjectKeyTap(KEY_DOWN);
    InjectPump();
    draw_composition();
    InjectKey(KEY_LEFT_SHIFT,0);
    InjectPump();
    if(composition_page_cursor != 7) {
        fprintf(stderr,"generated TextArea Shift+Down cursor=%d, want 7\n",
                composition_page_cursor);
        return 1;
    }
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_C);
    InjectPump();
    draw_composition();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    if(strcmp(GetClipboardTextValue(),"1\nc") != 0) {
        fprintf(stderr,"generated TextArea Shift+Down did not select\n");
        return 1;
    }
    InjectKeyTap(KEY_LEFT);
    InjectPump();
    draw_composition();
    if(composition_page_cursor != 4) {
        fprintf(stderr,"generated TextArea Left did not collapse selection\n");
        return 1;
    }
    InjectKeyTap(KEY_DOWN); InjectPump(); draw_composition();
    if(composition_page_cursor <= 4) {
        fprintf(stderr,"generated TextArea Down did not move\n"); return 1;
    }
    int composition_before_page = composition_page_cursor;
    InjectKeyTap(KEY_PAGE_DOWN); InjectPump(); draw_composition();
    if(composition_page_cursor < composition_before_page + 4) {
        fprintf(stderr,"generated TextArea PageDown did not move a page\n"); return 1;
    }
    composition_before_page = composition_page_cursor;
    InjectKeyTap(KEY_PAGE_UP); InjectPump(); draw_composition();
    if(composition_page_cursor > composition_before_page - 4) {
        fprintf(stderr,"generated TextArea PageUp did not move a page\n"); return 1;
    }
    int composition_page_length = (int)strlen(composition_page_area);
    int composition_page_lines = TextBufferLineCount(composition_page_area);
    InjectKeyTap(KEY_ENTER); InjectPump(); draw_composition();
    if((int)strlen(composition_page_area) != composition_page_length + 1 ||
       TextBufferLineCount(composition_page_area) != composition_page_lines + 1) {
        fprintf(stderr,"generated TextArea Enter did not insert newline\n"); return 1;
    }
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_END); InjectPump(); draw_composition();
    InjectKeyTap(KEY_BACKSPACE); InjectPump(); draw_composition();
    InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    if((int)strlen(composition_page_area) != composition_page_length - 1 ||
       composition_page_area[strlen(composition_page_area) - 1] != '\n') {
        fprintf(stderr,"generated TextArea Ctrl+Backspace did not delete final word\n");
        return 1;
    }
    InjectMousePosition(20,20);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump(); draw_ui(drag_drop_frame);
    dd_payload[0] = 'X';
    InjectMousePosition(150,20);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump(); draw_ui(drag_drop_frame);
    if(dd_invalid_accepts != 0 || dd_accepted != 8 || strcmp(dd_received,"item") != 0) {
        fprintf(stderr,"generated drag-drop clipping or copied payload failed\n"); return 1;
    }
    InjectReset();
    InjectReset();

    InjectTap(20, 90);
    InjectPump();
    draw_ui(scroll_content_frame);
    InjectPump();
    draw_ui(scroll_content_frame);
    if(scrolling_actions != 100) {
        fprintf(stderr, "scroll: clipped child or parent restoration failed\n");
        return 1;
    }
    InjectMousePosition(30, 30);
    InjectWheel(-1);
    InjectPump();
    draw_ui(scroll_content_frame);
    if(scrolling_offset != 42) {
        fprintf(stderr, "scroll: wheel offset failed\n");
        return 1;
    }
    InjectTap(20, 45);
    InjectPump();
    draw_ui(scroll_content_frame);
    InjectPump();
    draw_ui(scroll_content_frame);
    if(scrolling_actions != 101) {
        fprintf(stderr, "scroll: visible child did not activate\n");
        return 1;
    }
    InjectMousePosition(105,30);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_ui(scroll_content_frame);
    InjectMousePosition(105,110);
    InjectPump();
    draw_ui(scroll_content_frame);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    draw_ui(scroll_content_frame);
    if(scrolling_offset != 140) {
        fprintf(stderr, "scroll: generated thumb drag failed\n");
        return 1;
    }
    for(int nested = 0; nested < 2; nested++) {
        InjectTap(nested ? 70 : 120,65);
        InjectPump();
        draw_ui(scroll_content_frame);
        InjectPump();
        draw_ui(scroll_content_frame);
        if(scrolling_actions != (nested ? 1101 : 101)) {
            fprintf(stderr, "scroll: nested child clipping failed\n");
            return 1;
        }
    }
    InjectTap(250,20);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(mixed_flags != 4) { fprintf(stderr, "scroll: mixed checkbox failed\n"); return 1; }
    InjectTap(250,115);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(mixed_actions != 0) { fprintf(stderr, "scroll: hidden mixed button activated\n"); return 1; }
    SetFocus(988);
    InjectText("!");
    InjectPump(); draw_ui(scroll_content_frame);
    if(strcmp(mixed_text,"item!") != 0) { fprintf(stderr, "scroll: mixed text editing failed\n"); return 1; }
    InjectMousePosition(250,80);
    InjectWheel(-1);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectTap(250,80);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(mixed_actions != 1 || mixed_flags != 4 || mixed_offset != 42) {
        fprintf(stderr, "scroll: mixed widget state or scrolled button failed\n"); return 1;
    }
    InjectReset();

    const int branch_points[][2] = {{450,20},{450,50},{470,50},{490,85},{450,20},{490,85},{450,20}};
    for(int i = 0; i < 7; i++) {
        InjectTap(branch_points[i][0],branch_points[i][1]);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        if(branch_open != (i != 4 && i != 5) || nested_open != (i >= 2) || branch_actions != (i >= 3)) {
            fprintf(stderr,"tree composition: step %d failed\n",i); return 1;
        }
    }
    InjectReset();
    SetFocus(990);
    const int branch_keys[] = {KEY_LEFT,KEY_RIGHT,KEY_RIGHT,KEY_ENTER,KEY_SPACE};
    for(int i = 0; i < 5; i++) {
        SetFocus(990);
        InjectKeyTap(branch_keys[i]);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        if(branch_open != (i == 1 || i == 2 || i == 4) || !nested_open) {
            fprintf(stderr,"tree keyboard: step %d failed\n",i); return 1;
        }
    }
    InjectReset();
    InjectKeyTap(KEY_TAB);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectKeyTap(KEY_LEFT);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(!branch_open || nested_open) { fprintf(stderr,"tree keyboard: tab to child failed\n"); return 1; }
    const int direction_keys[] = {KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_RIGHT,KEY_DOWN,KEY_LEFT};
    for(int i = 0; i < 7; i++) {
        InjectKeyTap(direction_keys[i]);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        int want = (i == 0 || i == 2 || i == 6) ? 990 : i == 5 ? 995 : 991;
        if(GetFocus() != want || !branch_open || nested_open != (i >= 4)) {
            fprintf(stderr,"tree directional focus: step %d focus %d failed\n",i,GetFocus()); return 1;
        }
    }
    InjectReset();
    InjectTap(250,190);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectMousePosition(250,255);
    InjectWheel(-1);
    InjectPump(); draw_ui(scroll_content_frame);
    if(overlay_background_offset != 0) { fprintf(stderr,"background scroll stole popup wheel\n"); return 1; }
    InjectTap(250,255);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(overlay_selected != 1 || overlay_actions != 0) { fprintf(stderr,"dropdown overlay: selection or background capture failed\n"); return 1; }
    for(int escape = 0; escape < 2; escape++) {
        InjectTap(250,190);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        if(escape) InjectKeyTap(KEY_ESCAPE); else InjectTap(220,280);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectTap(250,220);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        InjectPump(); draw_ui(scroll_content_frame);
        if(overlay_selected != 1) { fprintf(stderr,"dropdown dismissal: stale row selected\n"); return 1; }
    }
    if(overlay_actions != 2) { fprintf(stderr,"dropdown dismissal: capture remained\n"); return 1; }
    for(int last = 0; last < 2; last++) {
        SetFocus(996);
        InjectKeyTap(KEY_SPACE);
        for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
        int before = overlay_selected;
        InjectKeyTap(last ? KEY_END : KEY_HOME);
        for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
        if(overlay_selected != before) { fprintf(stderr,"dropdown navigation committed before Enter\n"); return 1; }
        InjectKeyTap(KEY_ENTER);
        for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
        if(overlay_selected != last) { fprintf(stderr,"dropdown keyboard commit failed\n"); return 1; }
    }
    InjectReset();
    SetFocus(24001);
    InjectKeyTap(KEY_SPACE);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    InjectKeyTap(KEY_END);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    if(long_dropdown_selected != 0) { fprintf(stderr,"long dropdown navigation committed early\n"); return 1; }
    InjectTap(20,420);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    if(long_dropdown_selected != 19) { fprintf(stderr,"long dropdown flipped viewport did not reveal last row\n"); return 1; }
    long_dropdown_selected = 0;
    SetFocus(24001);
    InjectKeyTap(KEY_SPACE);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    InjectMousePosition(166,50); InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectMousePosition(166,425);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectMousePosition(20,60); InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    if(long_dropdown_selected != 0) { fprintf(stderr,"dropdown scrollbar drag selected a row\n"); return 1; }
    InjectTap(20,420);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    if(long_dropdown_selected != 19) { fprintf(stderr,"dropdown scrollbar did not reveal last row\n"); return 1; }
    edge_dropdown_visible = 1;
    SetFocus(24002); InjectKeyTap(KEY_SPACE);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    InjectTap(490,380);
    for(int frame = 0; frame < 3; frame++) { InjectPump(); draw_ui(scroll_content_frame); }
    if(edge_dropdown_selected != 1) { fprintf(stderr,"edge dropdown shifted popup row did not select\n"); return 1; }
    edge_dropdown_visible = 0;
    InjectReset();
    InjectTap(450,270);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(rotated_sort != -1) { fprintf(stderr,"slanted header wedge sorted\n"); return 1; }
    InjectTap(590,230);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(rotated_sort != 0 || rotated_row != -1) { fprintf(stderr,"rotated header hit geometry failed\n"); return 1; }
    InjectTap(590,290);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(rotated_sort != 1) { fprintf(stderr,"slanted header second column hit failed\n"); return 1; }
    InjectTap(450,310);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(rotated_row != 0) { fprintf(stderr,"rotated header body geometry failed\n"); return 1; }
    InjectMousePosition(600,230); InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectMousePosition(620,230);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump(); draw_ui(scroll_content_frame);
    if(rotated_widths[0] != 110) { fprintf(stderr,"slanted separator resize failed\n"); return 1; }
    InjectReset();
    InjectTap(20,345);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectTap(180,345);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(custom_actions != 1 || custom_flags != 4 || custom_selected != -1) { fprintf(stderr,"custom table cell interaction or clipping failed: actions=%d flags=%d selected=%d\n",custom_actions,custom_flags,custom_selected); return 1; }
    InjectTap(120,345);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(custom_actions != 11) { fprintf(stderr,"custom table nested row layout failed\n"); return 1; }
    InjectTap(340,310);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectTap(340,330);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(custom_outer_actions != 11) { fprintf(stderr,"custom table surrounding layout restoration failed\n"); return 1; }
    InjectTap(20,380);
    InjectPump(); draw_ui(scroll_content_frame);
    InjectPump(); draw_ui(scroll_content_frame);
    if(GetFocus() != 1006) { fprintf(stderr,"custom cell editor focus failed\n"); return 1; }
    InjectKeyTap(KEY_END); InjectPump(); draw_ui(scroll_content_frame);
    InjectText("!"); InjectPump(); draw_ui(scroll_content_frame);
    if(strcmp(custom_text,"cell!") != 0) { fprintf(stderr,"custom cell editing failed: %s\n",custom_text); return 1; }
    custom_disabled = 1;
    InjectText("X"); InjectPump(); draw_ui(scroll_content_frame);
    if(strcmp(custom_text,"cell!") != 0) { fprintf(stderr,"disabled custom cell edited: %s\n",custom_text); return 1; }
    custom_disabled = 0;
    SetFocus(1006); InjectPump(); draw_ui(scroll_content_frame);
    InjectText("?"); InjectPump(); draw_ui(scroll_content_frame);
    if(strcmp(custom_text,"cell!?") != 0) { fprintf(stderr,"custom cell editor re-enable failed: %s\n",custom_text); return 1; }
    InjectReset();
    draw_form();
    SetFocus(101);
    draw_form();
    AccessibilityNode accessible_nodes[32];
    int accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    int accessible_fields = 0;
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id < 101 || node.focus_id > 104)
            continue;
        accessible_fields++;
        int valid = strcmp(node.role, "textbox") == 0 && !node.disabled;
        if(node.focus_id == 101)
            valid = valid && node.focused && strcmp(node.value, "alpha") == 0;
        else if(node.focus_id == 102)
            valid = valid && strcmp(node.value, "beta") == 0;
        else if(node.focus_id == 103)
            valid = valid && node.secure && node.value[0] == '\0' && node.label[0] == '\0';
        else if(node.focus_id == 104)
            valid = valid && node.multiline && strcmp(node.value, "notes") == 0 && strcmp(node.label, "Notes") == 0;
        if(!valid) {
            fprintf(stderr, "generated editor accessibility mismatch for %d\n", node.focus_id);
            return 1;
        }
    }
    if(accessible_fields != 4) {
        fprintf(stderr, "generated editor accessibility nodes missing\n");
        return 1;
    }
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 101 &&
           (!QueueAccessibilityValue(101, node.generation, "Ae\xcc\x81Z") ||
            !QueueAccessibilitySelection(101, node.generation, 3, 99))) {
            fprintf(stderr, "generated editor value/selection request rejected\n");
            return 1;
        }
        if(node.focus_id == 104 && !QueueAccessibilityValue(104, node.generation, "a\r\nb\tc")) {
            fprintf(stderr, "generated area value request rejected\n");
            return 1;
        }
    }
    draw_form();
    if(strcmp(first, "Ae\xcc\x81Z") != 0 || first_cursor != 5 || strcmp(notes, "a\r\nb\tc") != 0) {
        fprintf(stderr, "generated editor value/selection not delivered\n");
        return 1;
    }
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 101) {
            if(node.selection_anchor != 1 || node.selection_cursor != 5 ||
               !QueueAccessibilityValue(101, node.generation, "alpha")) {
                fprintf(stderr, "generated editor selection or reset failed\n");
                return 1;
            }
        }
        if(node.focus_id == 104 && !QueueAccessibilityValue(104, node.generation, "notes")) {
            fprintf(stderr, "generated area reset rejected\n");
            return 1;
        }
    }
    draw_form();
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 201 && !QueueAccessibilityAction(201, node.generation, AccessibilityActionActivate)) {
            fprintf(stderr, "generated button accessibility action rejected\n");
            return 1;
        }
    }
    draw_form();
    if(form_action != 1) {
        fprintf(stderr, "generated button accessibility action not delivered\n");
        return 1;
    }
    draw_form();
    if(form_action != 1) {
        fprintf(stderr, "generated button accessibility action replayed\n");
        return 1;
    }
    form_action = 0;
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 101 && !QueueAccessibilityAction(101, node.generation, AccessibilityActionFocus)) {
            fprintf(stderr, "generated editor accessibility focus rejected\n");
            return 1;
        }
    }
    draw_form();
    if(!IsFocusActive(101)) {
        fprintf(stderr, "generated editor accessibility focus not delivered\n");
        return 1;
    }
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

    const char *clusters[] = {
        "e\xcc\x81", "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",
        "\xf0\x9f\x87\xb5\xf0\x9f\x87\xbe", "\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd",
        "\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7"
    };
    for(size_t i = 0; i < sizeof(clusters) / sizeof(clusters[0]); i++) {
        for(int backward = 0; backward < 2; backward++) {
            snprintf(first, sizeof(first), "A%sZ", clusters[i]);
            first_cursor = 1 + (int)strlen(clusters[i]);
            SetSelection(101, first_cursor, first_cursor);
            if(!backward) {
                InjectKeyTap(KEY_LEFT);
                InjectPump();
                draw_form();
                if(first_cursor != 1) {
                    fprintf(stderr, "generated TextField left split grapheme\n");
                    return 1;
                }
            }
            InjectKeyTap(backward ? KEY_BACKSPACE : KEY_DELETE);
            InjectPump();
            draw_form();
            if(strcmp(first, "AZ") != 0 || first_cursor != 1) {
                fprintf(stderr, "generated TextField deletion split grapheme\n");
                return 1;
            }
        }
    }
    snprintf(first, sizeof(first), "alpha");
    first_cursor = 4;
    SetSelection(101, 4, 4);

    SetFocus(102);
    draw_form();
    SetSelection(102, 0, 4);
    InjectText("acct");
    InjectPump();
    draw_form();

    SetFocus(101);
    draw_form();
    InjectKeyTap(KEY_TAB);
    InjectPump();
    draw_form();
    InjectText("Z");
    InjectPump();
    draw_form();

    SetClipboardTextValue("old");
    SetFocus(103);
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
    int focus_after_focus = GetFocus();

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

    /* Native keyboard parity; retain the shared pointer-only JSON result. */
    if(buttons_action != 10) {
        fprintf(stderr, "first widget instance action was not isolated\n");
        return 1;
    }
    InjectTap(230, 130);
    InjectPump();
    draw_buttons();
    InjectPump();
    draw_buttons();
    if(buttons_action != 110) {
        fprintf(stderr, "second widget instance action was not isolated\n");
        return 1;
    }
    buttons_reverse = true;
    draw_buttons();
    InjectTap(130, 130);
    InjectPump();
    draw_buttons();
    InjectPump();
    draw_buttons();
    InjectTap(230, 130);
    InjectPump();
    draw_buttons();
    InjectPump();
    draw_buttons();
    if(buttons_action != 220) {
        fprintf(stderr, "widget instance actions changed after reordering\n");
        return 1;
    }
    buttons_reverse = false;
    draw_buttons();
    int button_pointer_action = buttons_action;
    SetFocus(502); InjectKeyTap(KEY_ENTER); InjectPump(); draw_buttons();
    InjectPump(); draw_buttons();
    SetFocus(501); InjectKeyTap(KEY_SPACE); InjectPump(); draw_buttons();
    InjectPump(); draw_buttons();
    InjectKeyTap(KEY_TAB); InjectPump(); draw_buttons();
    InjectPump(); draw_buttons();
    if(GetFocus() != 502) {
        fprintf(stderr,"generated button Tab did not skip disabled control\n");
        return 1;
    }
    InjectKeyTap(KEY_SPACE); InjectPump(); draw_buttons();
    InjectPump(); draw_buttons();
    if(buttons_action != button_pointer_action + 20) {
        fprintf(stderr,"generated button keyboard activation or disabled gating failed\n");
        return 1;
    }
    buttons_action = button_pointer_action;
    draw_long_text();
    int long_text_nodes = 0;
    (void)GetTreeNodes(&long_text_nodes);
    SetFocus(701);
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
    InjectTap(346, 220);
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
    SetFocus(802); InjectKeyTap(KEY_SPACE); InjectPump(); draw_controls();
    if(toggle_value != 0) {
        fprintf(stderr,"controls: generated Toggle rejected keyboard toggle\n");
        return 1;
    }
    InjectPump(); InjectKeyTap(KEY_SPACE); InjectPump(); draw_controls();
    if(toggle_value != 1) {
        fprintf(stderr,"controls: generated Toggle did not restore state\n");
        return 1;
    }
    SetFocus(802); InjectKeyTap(KEY_TAB); InjectPump(); draw_controls();
    if(GetFocus() != 803) {
        fprintf(stderr,"controls: generated Toggle Tab focus=%d, want 803\n",GetFocus());
        return 1;
    }
    SetFocus(803); InjectKeyTap(KEY_SPACE); InjectPump(); draw_controls();
    if(checkbox_value != 0) {
        fprintf(stderr,"controls: generated Checkbox rejected keyboard toggle\n");
        return 1;
    }
    InjectPump(); InjectKeyTap(KEY_SPACE); InjectPump(); draw_controls();
    SetFocus(805); InjectKeyTap(KEY_ENTER); InjectPump(); draw_controls();
    SetFocus(806); InjectKeyTap(KEY_SPACE); InjectPump(); draw_controls();
    SetFocus(807); InjectKeyTap(KEY_ENTER); InjectPump(); draw_controls();
    if(choice_selected != 1 || choice_flags != 4 || choice_radio_actions != 1) {
        fprintf(stderr,"controls: generated choice keyboard state=%d/%d/%d, want 1/4/1\n",
                choice_selected,choice_flags,choice_radio_actions);
        return 1;
    }
    choice_selected = choice_flags = choice_radio_actions = 0;
    int accessible_radio = 0;
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 807)
            accessible_radio = strcmp(node.role, "radio") == 0 && strcmp(node.label, "Radio") == 0 &&
                QueueAccessibilityAction(807, node.generation, AccessibilityActionActivate);
    }
    if(!accessible_radio) {
        fprintf(stderr, "generated radio accessibility missing\n");
        return 1;
    }
    InjectPump(); draw_controls();
    if(choice_radio_actions != 1) {
        fprintf(stderr, "generated radio accessibility action not delivered\n");
        return 1;
    }
    choice_radio_actions = 0;
    SetFocus(805); InjectKeyTap(KEY_TAB); InjectPump(); draw_controls();
    if(GetFocus() != 806) {
        fprintf(stderr,"controls: generated choice Tab focus=%d, want 806\n",GetFocus());
        return 1;
    }
    draw_multi_select();
    SetFocus(957); InjectKeyTap(KEY_DOWN); InjectPump(); draw_multi_select();
    if(multi_anchor != 1 || multi_count != 1 ||
       multi_selected[0] != 0 || multi_selected[1] != 1 || multi_selected[2] != 0) {
        fprintf(stderr,"multi_select: generated Down state=%d%d%d/%d/%d\n",
                multi_selected[0],multi_selected[1],multi_selected[2],multi_count,multi_anchor);
        return 1;
    }
    InjectPump(); InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_DOWN); InjectPump();
    draw_multi_select();
    InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    if(multi_anchor != 2 || multi_count != 2 ||
       multi_selected[0] != 0 || multi_selected[1] != 1 || multi_selected[2] != 1) {
        fprintf(stderr,"multi_select: generated Shift+Down state=%d%d%d/%d/%d\n",
                multi_selected[0],multi_selected[1],multi_selected[2],multi_count,multi_anchor);
        return 1;
    }

    draw_list_box();
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    int list_items = 0;
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(strcmp(node.role, "option") == 0) {
            list_items++;
            if(node.parent == 0 || accessible_nodes[node.parent-1].focus_id != 801 || node.key != (uint64_t)(11+node.item_index)) {
                fprintf(stderr, "generated list option hierarchy/key mismatch\n");
                return 1;
            }
        }
        if(node.focus_id == 801 && !QueueAccessibilityItem(801, node.generation, 3, 1)) {
            fprintf(stderr, "generated list selection rejected\n");
            return 1;
        }
    }
    if(list_items != 4) { fprintf(stderr, "generated list options missing\n"); return 1; }
    InjectPump(); draw_list_box();
    if(list_selected != 3) { fprintf(stderr, "generated list selection not delivered\n"); return 1; }
    accessible_count = GetAccessibilitySnapshot(accessible_nodes, 32);
    for(int i = 0; i < accessible_count && i < 32; i++) {
        AccessibilityNode node = accessible_nodes[i];
        if(node.focus_id == 801 && !QueueAccessibilityAction(801, node.generation, AccessibilityActionClearSelection)) {
            fprintf(stderr, "generated list clear rejected\n");
            return 1;
        }
    }
    InjectPump(); draw_list_box();
    if(list_selected != -1) { fprintf(stderr, "generated list clear not delivered\n"); return 1; }
    InjectTap(36, 78);
    InjectPump();
    draw_list_box();
    SetFocus(0); InjectKeyTap(KEY_TAB); InjectPump(); draw_list_box();
    if(GetFocus() != 801) { fprintf(stderr,"generated list Tab focus failed\n"); return 1; }
    SetFocus(801); InjectKeyTap(KEY_END); InjectPump(); draw_list_box();
    InjectKeyTap(KEY_HOME); InjectPump(); draw_list_box();
    InjectKeyTap(KEY_END); InjectPump(); draw_list_box();
    InjectKeyTap(KEY_UP); InjectPump(); draw_list_box();
    InjectPump();
    draw_list_box();
    if(list_selected != 2 || list_scroll != 0) {
        fprintf(stderr,
                "list_box: got selected=%d scroll=%d, want 2,0\n",
                list_selected, list_scroll);
        return 1;
    }

    SetFocus(940); InjectKeyTap(KEY_DOWN); InjectPump(); draw_menus();
    if(open_menu != 0) { fprintf(stderr,"generated menu Down did not open\n"); return 1; }
    InjectKeyTap(KEY_END); InjectPump(); draw_menus();
    InjectKeyTap(KEY_RIGHT); InjectPump(); draw_menus();
    InjectKeyTap(KEY_ENTER); InjectPump(); draw_menus();
    InjectPump(); draw_menus();
    if(menu_action != 23 || open_menu != -1) {
        fprintf(stderr,"generated submenu keyboard activation failed\n"); return 1;
    }

    draw_tree_view();
    InjectTap(36, 84);
    InjectPump();
    draw_tree_view();
    InjectPump();
    draw_tree_view();
    if(tree_selected != 2 || tree_scroll != 0) {
        fprintf(stderr,
                "tree_view: got selected=%d scroll=%d, want 2,0\n",
                tree_selected, tree_scroll);
        return 1;
    }

    draw_progress();
    draw_plots();
    SetFocus(920); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_plots();
    if(plots_scalar_values[0] < 1.0999f || plots_scalar_values[0] > 1.1001f) {
        fprintf(stderr,"generated float drag keyboard value=%f, want 1.1\n",plots_scalar_values[0]);
        return 1;
    }
    InjectPump(); SetFocus(921); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_plots();
    if(plots_whole_values[0] != 4) {
        fprintf(stderr,"generated int drag keyboard value=%d, want 4\n",plots_whole_values[0]);
        return 1;
    }
    InjectPump(); SetFocus(938); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_plots();
    if(plots_scalar_range_min < 2.0999f || plots_scalar_range_min > 2.1001f) {
        fprintf(stderr,"generated float range drag keyboard value=%f, want 2.1\n",plots_scalar_range_min);
        return 1;
    }
    InjectPump();
    SetFocus(922); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_plots();
    if((int)(plots_slider_values[0]*1000.0f+0.5f) != 260) {
        fprintf(stderr,"generated float slider keyboard value=%f, want 0.26\n",
                plots_slider_values[0]);
        return 1;
    }
    SetFocus(925); InjectKeyTap(KEY_UP); InjectPump(); draw_plots();
    if(plots_slider_discrete_values[0] != 3) {
        fprintf(stderr,"generated vertical int slider keyboard value=%d, want 3\n",
                plots_slider_discrete_values[0]);
        return 1;
    }
    InjectMousePosition(30,190); InjectKey(KEY_LEFT_CONTROL,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1); InjectPump(); draw_plots();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0); InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_A);
    InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,0); InjectText("12.5");
    InjectPump(); draw_plots();
    if(plots_scalar_values[0] != 12.5f) {
        fprintf(stderr,"generated float drag temporary input=%f, want 12.5\n",
                plots_scalar_values[0]);
        return 1;
    }
    InjectKeyTap(KEY_ENTER); InjectPump(); draw_plots();
    InjectMousePosition(145,190);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1); InjectPump(); draw_plots();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0); InjectPump(); draw_plots();
    InjectMousePosition(145,190);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1); InjectPump(); draw_plots();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0); InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_A);
    InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,0); InjectText("17");
    InjectPump(); draw_plots();
    if(plots_whole_values[0] != 17) {
        fprintf(stderr,"generated int drag double-click input=%d, want 17\n",
                plots_whole_values[0]);
        return 1;
    }
    InjectKeyTap(KEY_ENTER); InjectPump(); draw_plots();
    InjectMousePosition(330,260); InjectKey(KEY_LEFT_CONTROL,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1); InjectPump(); draw_plots();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0); InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_A);
    InjectPump(); draw_plots();
    InjectKey(KEY_LEFT_CONTROL,0); InjectText("19");
    InjectPump(); draw_plots();
    if(plots_slider_discrete_values[0] != 19) {
        fprintf(stderr,"generated int slider temporary input=%d, want 19\n",
                plots_slider_discrete_values[0]);
        return 1;
    }
    SetFocus(0x60000008); InjectKeyTap(KEY_SPACE); InjectPump(); draw_plots();
    if(plots_input_whole_values[0] != 5) {
        fprintf(stderr,"generated input step keyboard value=%d, want 5\n",
                plots_input_whole_values[0]);
        return 1;
    }

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

    SetFocus(901);
    InjectKey(KEY_TAB,1); InjectPump(); draw_table_view();
    InjectKey(KEY_TAB,0); InjectPump();
    if(selected_row != 1 || selected_column != 0 || GetFocus() != 901) {
        fprintf(stderr,"table_view tab: got selected=(%d,%d) focus=%d, want (1,0),901\n",
                selected_row,selected_column,GetFocus());
        return 1;
    }
    InjectKey(KEY_LEFT_SHIFT,1); InjectKey(KEY_TAB,1); InjectPump(); draw_table_view();
    InjectKey(KEY_TAB,0); InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    if(selected_row != 0 || selected_column != 2 || GetFocus() != 901) {
        fprintf(stderr,"table_view shift-tab: got selected=(%d,%d) focus=%d, want (0,2),901\n",
                selected_row,selected_column,GetFocus());
        return 1;
    }
    InjectKey(KEY_LEFT,1); InjectPump(); draw_table_view();
    InjectKey(KEY_LEFT,0); InjectPump();
    if(selected_row != 0 || selected_column != 1) {
        fprintf(stderr,"table_view keyboard left: got (%d,%d), want (0,1)\n",
                selected_row,selected_column);
        return 1;
    }
    InjectKey(KEY_DOWN,1); InjectKey(KEY_F2,1); InjectPump(); draw_table_view();
    InjectKey(KEY_DOWN,0); InjectKey(KEY_F2,0); InjectPump();
    if(selected_row != 1 || selected_column != 1 ||
       activated_row != 1 || activated_column != 1) {
        fprintf(stderr,
                "table_view keyboard activation: got selected=(%d,%d) activated=(%d,%d), want (1,1),(1,1)\n",
                selected_row,selected_column,activated_row,activated_column);
        return 1;
    }
    InjectKey(KEY_LEFT_CONTROL,1); InjectKey(KEY_C,1); InjectPump(); draw_table_view();
    InjectKey(KEY_C,0); InjectPump();
    if(strcmp(GetClipboardTextValue(),"Wallet") != 0) {
        fprintf(stderr,"table_view cell copy: got %s, want Wallet\n",
                GetClipboardTextValue());
        return 1;
    }
    SetClipboardTextValue("generated-paste");
    InjectKey(KEY_V,1); InjectPump(); draw_table_view();
    InjectKey(KEY_V,0); InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    if(pasted_text == NULL || strcmp(pasted_text,"generated-paste") != 0 ||
       pasted_row != 1 || pasted_column != 1) {
        fprintf(stderr,"table_view paste: got %s at (%d,%d), want generated-paste at (1,1)\n",
                pasted_text != NULL ? pasted_text : "(null)",pasted_row,pasted_column);
        return 1;
    }
    SetClipboardTextValue("old");
    selected_row = -1;
    selected_column = 2;

    printf("{\"form_first\":\"%s\",\"form_first_cursor\":%d,\"form_second\":\"%s\",\"form_second_cursor\":%d,\"form_password\":\"%s\",\"form_password_cursor\":%d,\"form_notes\":\"%s\",\"form_notes_cursor\":%d,\"form_action\":%d,\"fields_title\":\"%s\",\"fields_title_cursor\":%d,\"fields_body\":\"%s\",\"fields_body_cursor\":%d,\"focus_one\":\"%s\",\"focus_two\":\"%s\",\"focus_three\":\"%s\",\"focus_id\":%d,\"buttons_action\":%d,\"long_first_len\":%d,\"long_first_cursor\":%d,\"long_first_hash\":%llu,\"long_second_len\":%d,\"long_second_cursor\":%d,\"long_second_hash\":%llu,\"controls_slider\":%d,\"controls_toggle\":%d,\"controls_checkbox\":%d,\"controls_selected\":%d,\"list_box_selected\":%d,\"list_box_scroll\":%d,\"tree_selected\":%d,\"tree_scroll\":%d,\"table_selected_row\":%d,\"table_selected_column\":%d,\"table_activated_row\":%d,\"table_activated_column\":%d,\"table_sort_column\":%d,\"clipboard\":\"%s\"}\n",
        first, first_cursor, second, second_cursor, password, password_cursor,
        notes, notes_cursor, form_action, title, title_cursor, body,
        body_cursor, one, two, three, focus_after_focus, buttons_action,
        (int)strlen(long_first), long_first_cursor, checksum(long_first),
        (int)strlen(long_second), long_second_cursor, checksum(long_second),
        slider_value, toggle_value, checkbox_value, selected,
        list_selected, list_scroll, tree_selected, tree_scroll,
        selected_row, selected_column, table_activated_row,
        table_activated_column, sort_column,
        GetClipboardTextValue());
    return 0;
}
EOF

# shellcheck disable=SC2086
$cc $cppflags $cflags -I"$root/include" -I"$root/src/ui" -I"$build/generated/src" -I"$work/c" "$work/c_runner.c" \
    $link_flags -o "$work/bin/c_runner"
"$work/bin/c_runner" > "$work/c.json"

if ! diff -u "$work/go.json" "$work/c.json"; then
    echo "generated Go/C runtime parity mismatch" >&2
    exit 1
fi

printf '%s\n' '{"generated_runtime_parity":"ok","runtimes":["go","c"],"fixture_count":17,"scope":"executed interaction assertions and final state comparison"}'
