//go:build linux

package kryon

import (
	"image/png"
	"os"
	"os/exec"
	"strconv"
	"testing"
)

func TestX11LiveTextSelection(t *testing.T) {
	if os.Getenv("KRYON_LIVE_X11_TEST") == "" {
		t.Skip("run tests/native_go_input_test.sh on a private virtual display")
	}
	for _, name := range []string{"IBUS_ADDRESS", "GTK_IM_MODULE", "QT_IM_MODULE", "XMODIFIERS"} {
		t.Setenv(name, "")
	}
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	rt, err := openWindowRuntime(AppConfig{Title: "Kryon native text selection", Width: 520, Height: 180})
	if err != nil {
		t.Fatal(err)
	}
	defer rt.Close()
	r := rt.(*windowRuntime)
	props := TextProps{Text: "Select this text with the mouse", Bounds: NewRectangle(20, 60, 460, 50),
		Font: 24, Wrap: TextWrapNone, Selectable: true}
	draw := func() {
		r.BeginFrame()
		r.ClearBackground(Color{35, 40, 50, 255})
		r.Text(TextProps{Text: "Native Go range selection", Bounds: NewRectangle(20, 15, 460, 35),
			Font: 22})
		r.Text(props)
		r.EndFrame()
		if r.closed {
			t.Fatal("native window closed during presentation")
		}
	}
	input := func(args ...string) {
		if output, err := exec.Command("xdotool", args...).CombinedOutput(); err != nil {
			t.Fatalf("xdotool: %v: %s", err, output)
		}
	}
	draw()
	id := strconv.FormatUint(uint64(r.window.window), 10)
	input("mousemove", "--window", id, "20", "65", "mousedown", "1")
	draw()
	x := 20 + runtimeTextWidthWithFont("Select this text", 24, 0)
	input("mousemove", "--window", id, strconv.Itoa(x), "65")
	draw()
	input("mouseup", "1")
	draw()
	input("key", "ctrl+c")
	draw()
	if got := r.Runtime.(*runtime).ClipboardText(); got != "Select this text" {
		t.Fatalf("native drag/copy = %q", got)
	}
	if path := os.Getenv("KRYON_INPUT_CAPTURE"); path != "" {
		file, err := os.Create(path)
		if err != nil {
			t.Fatal(err)
		}
		err = png.Encode(file, r.frame)
		closeErr := file.Close()
		if err != nil || closeErr != nil {
			t.Fatalf("capture: %v, close: %v", err, closeErr)
		}
	}
}
