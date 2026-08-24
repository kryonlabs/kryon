//go:build linux

package kryon

import (
	"bytes"
	"encoding/binary"
	"image"
	"image/color"
	"net"
	"os"
	"path/filepath"
	"testing"
)

func TestOpenFallsBackWithoutDisplay(t *testing.T) {
	t.Setenv("DISPLAY", "")
	defer SetRuntime(nil)

	rt := Open(AppConfig{Width: 123, Height: 77})
	if _, ok := rt.(*runtime); !ok {
		t.Fatalf("Open without DISPLAY returned %T, want headless runtime fallback", rt)
	}
	if got, want := rt.GetScreenWidth(), int32(123); got != want {
		t.Fatalf("fallback width = %d, want %d", got, want)
	}
	if got, want := rt.GetScreenHeight(), int32(77); got != want {
		t.Fatalf("fallback height = %d, want %d", got, want)
	}
}

func TestX11SocketParsesDisplay(t *testing.T) {
	cases := map[string]string{
		":0":        "/tmp/.X11-unix/X0",
		":1.0":      "/tmp/.X11-unix/X1",
		"2":         "/tmp/.X11-unix/X2",
		"bad-value": "/tmp/.X11-unix/X0",
	}
	for display, want := range cases {
		if got := x11Socket(display); got != want {
			t.Fatalf("x11Socket(%q) = %q, want %q", display, got, want)
		}
	}
}

func TestX11KeyTranslation(t *testing.T) {
	if got := specialKey(0xff51); got != KeyLeft {
		t.Fatalf("left keysym = %d, want %d", got, KeyLeft)
	}
	if got := shortcutKey('v'); got != KeyV {
		t.Fatalf("shortcut v = %d, want %d", got, KeyV)
	}
	if got := keysymText(0x0101f600); got != "\U0001f600" {
		t.Fatalf("unicode keysym text = %q, want grinning face", got)
	}
	if got := keysymText(0xff51); got != "" {
		t.Fatalf("special key text = %q, want empty", got)
	}
}

func TestX11DecodeKeyEvents(t *testing.T) {
	win := &x11Window{keysyms: map[uint8][]uint32{
		22: {0xff08, 0},
		36: {0xff0d, 0},
		38: {'a', 'A'},
		54: {'c', 'C'},
	}}

	ev, ok := win.decodeKey(38, 0)
	if !ok || ev.kind != x11EventKey || ev.text != "a" {
		t.Fatalf("decode a = %#v ok=%v, want text a", ev, ok)
	}

	ev, ok = win.decodeKey(38, x11ShiftMask)
	if !ok || ev.kind != x11EventKey || ev.text != "A" {
		t.Fatalf("decode shifted a = %#v ok=%v, want text A", ev, ok)
	}

	ev, ok = win.decodeKey(22, 0)
	if !ok || ev.kind != x11EventKey || ev.key != KeyBackspace {
		t.Fatalf("decode backspace = %#v ok=%v, want KeyBackspace", ev, ok)
	}

	ev, ok = win.decodeKey(36, 0)
	if !ok || ev.kind != x11EventKey || ev.key != KeyEnter {
		t.Fatalf("decode enter = %#v ok=%v, want KeyEnter", ev, ok)
	}

	ev, ok = win.decodeKey(54, x11ControlMask)
	if !ok || ev.kind != x11EventKey || ev.shortcut != KeyC {
		t.Fatalf("decode ctrl-c = %#v ok=%v, want shortcut KeyC", ev, ok)
	}
}

func TestX11LoadKeyboardMappingRequestAndDecode(t *testing.T) {
	client, server := net.Pipe()
	defer client.Close()
	defer server.Close()
	win := &x11Window{conn: client, minKeycode: 22, maxKeycode: 23}

	errCh := make(chan error, 1)
	go func() {
		errCh <- win.loadKeyboardMapping()
	}()

	req := make([]byte, 8)
	if _, err := readFull(server, req); err != nil {
		t.Fatal(err)
	}
	wantReq := []byte{101, 0, 2, 0, 22, 2, 0, 0}
	if !bytes.Equal(req, wantReq) {
		t.Fatalf("GetKeyboardMapping request = %#v, want %#v", req, wantReq)
	}

	reply := make([]byte, 32)
	reply[0] = 1
	reply[1] = 2
	put32(reply[4:], 4)
	data := make([]byte, 16)
	put32(data[0:], 0xff08)
	put32(data[4:], 0)
	put32(data[8:], 'a')
	put32(data[12:], 'A')
	if _, err := server.Write(append(reply, data...)); err != nil {
		t.Fatal(err)
	}
	if err := <-errCh; err != nil {
		t.Fatal(err)
	}

	if got := win.keysyms[22][0]; got != 0xff08 {
		t.Fatalf("loaded keycode 22 = %#x, want Backspace", got)
	}
	ev, ok := win.decodeKey(22, 0)
	if !ok || ev.key != KeyBackspace {
		t.Fatalf("loaded backspace event = %#v ok=%v", ev, ok)
	}
	ev, ok = win.decodeKey(23, x11ShiftMask)
	if !ok || ev.text != "A" {
		t.Fatalf("loaded shifted text event = %#v ok=%v, want A", ev, ok)
	}
}

func TestWindowRuntimeDelegatesCompatInput(t *testing.T) {
	base := New(AppConfig{}).(*runtime)
	rt := &windowRuntime{Runtime: base}

	base.QueueMouseButton(MouseButtonLeft, 12, 34)
	if got, want := rt.MousePosition(), (Vector2{X: 12, Y: 34}); got != want {
		t.Fatalf("delegated mouse position = %#v, want %#v", got, want)
	}
	if !rt.MouseButtonPressed(MouseButtonLeft) {
		t.Fatal("delegated mouse button was not pressed")
	}

	base.QueueMouseWheel(-1)
	if got := rt.MouseWheelMove(); got != -1 {
		t.Fatalf("delegated wheel = %v, want -1", got)
	}

	base.QueueKey(KeyEscape)
	if !rt.KeyPressed(KeyEscape) {
		t.Fatal("delegated key was not pressed")
	}

	base.QueueText("x")
	if got := rt.CharPressed(); got != 'x' {
		t.Fatalf("delegated char = %q, want x", rune(got))
	}
}

func TestX11DecodeMouseReleaseAndMotion(t *testing.T) {
	win := &x11Window{}
	buf := make([]byte, 32)

	buf[0] = x11EventButtonRelease
	buf[1] = 1
	put16(buf[24:], 42)
	put16(buf[26:], 58)
	ev, ok := win.decodeEvent(buf)
	if !ok || ev.kind != x11EventRelease || ev.button != MouseButtonLeft || ev.x != 42 || ev.y != 58 {
		t.Fatalf("decoded release = %#v ok=%v", ev, ok)
	}

	buf = make([]byte, 32)
	buf[0] = x11EventMotionNotify
	put16(buf[24:], 77)
	put16(buf[26:], 91)
	ev, ok = win.decodeEvent(buf)
	if !ok || ev.kind != x11EventMotion || ev.x != 77 || ev.y != 91 {
		t.Fatalf("decoded motion = %#v ok=%v", ev, ok)
	}
}

func TestX11SetInputFocusRequest(t *testing.T) {
	client, server := net.Pipe()
	defer client.Close()
	defer server.Close()
	win := &x11Window{conn: client, window: 0x01020304}

	errCh := make(chan error, 1)
	go func() {
		errCh <- win.setInputFocus()
	}()

	buf := make([]byte, 12)
	if _, err := readFull(server, buf); err != nil {
		t.Fatal(err)
	}
	if err := <-errCh; err != nil {
		t.Fatal(err)
	}

	want := []byte{42, 1, 3, 0, 4, 3, 2, 1, 0, 0, 0, 0}
	if !bytes.Equal(buf, want) {
		t.Fatalf("SetInputFocus request = %#v, want %#v", buf, want)
	}
}

func TestX11AuthReadsMITCookie(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, ".Xauthority")
	data := xauthRecord(256, "localhost", "44", "MIT-MAGIC-COOKIE-1", []byte{1, 2, 3, 4})
	if err := os.WriteFile(path, data, 0o600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("XAUTHORITY", path)

	name, auth := x11Auth(":44.0")
	if got, want := string(name), "MIT-MAGIC-COOKIE-1"; got != want {
		t.Fatalf("auth name = %q, want %q", got, want)
	}
	if !bytes.Equal(auth, []byte{1, 2, 3, 4}) {
		t.Fatalf("auth data = %v, want [1 2 3 4]", auth)
	}
}

func TestX11ImageDataUsesVisualMasksAndStride(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 2, 1))
	img.SetRGBA(0, 0, rgbaTest(Color{R: 0x11, G: 0x22, B: 0x33, A: 0xff}))
	img.SetRGBA(1, 0, rgbaTest(Color{R: 0xaa, G: 0xbb, B: 0xcc, A: 0xff}))

	win := &x11Window{
		bpp:        24,
		scanPad:    32,
		byteOrder:  0,
		redMask:    0xff0000,
		greenMask:  0x00ff00,
		blueMask:   0x0000ff,
		redShift:   16,
		greenShift: 8,
		blueShift:  0,
	}
	data, stride := win.imageData(img)
	if stride != 8 {
		t.Fatalf("24-bit stride = %d, want 8", stride)
	}
	want := []byte{0x33, 0x22, 0x11, 0xcc, 0xbb, 0xaa, 0x00, 0x00}
	if !bytes.Equal(data, want) {
		t.Fatalf("24-bit image data = %#v, want %#v", data, want)
	}
}

func TestX11PixelScalesToRGB565(t *testing.T) {
	win := &x11Window{
		redMask:    0xf800,
		greenMask:  0x07e0,
		blueMask:   0x001f,
		redShift:   11,
		greenShift: 5,
		blueShift:  0,
	}
	if got, want := win.pixel(color.RGBA{R: 255, G: 0, B: 0, A: 255}), uint32(0xf800); got != want {
		t.Fatalf("red 565 pixel = %#x, want %#x", got, want)
	}
	if got, want := win.pixel(color.RGBA{R: 0, G: 255, B: 0, A: 255}), uint32(0x07e0); got != want {
		t.Fatalf("green 565 pixel = %#x, want %#x", got, want)
	}
	if got, want := win.pixel(color.RGBA{R: 0, G: 0, B: 255, A: 255}), uint32(0x001f); got != want {
		t.Fatalf("blue 565 pixel = %#x, want %#x", got, want)
	}
}

func xauthRecord(family uint16, address, number, name string, data []byte) []byte {
	var out []byte
	out = binary.BigEndian.AppendUint16(out, family)
	out = xauthAppend(out, []byte(address))
	out = xauthAppend(out, []byte(number))
	out = xauthAppend(out, []byte(name))
	out = xauthAppend(out, data)
	return out
}

func xauthAppend(out, data []byte) []byte {
	out = binary.BigEndian.AppendUint16(out, uint16(len(data)))
	return append(out, data...)
}
