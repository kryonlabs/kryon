//go:build linux

package kryon

import (
	"image"
	"testing"
)

func TestX11SocketParsesDisplay(t *testing.T) {
	tests := map[string]string{
		":0":     "/tmp/.X11-unix/X0",
		":1.0":   "/tmp/.X11-unix/X1",
		"bad":    "/tmp/.X11-unix/X0",
		":44.12": "/tmp/.X11-unix/X44",
	}
	for in, want := range tests {
		if got := x11Socket(in); got != want {
			t.Fatalf("x11Socket(%q) = %q, want %q", in, got, want)
		}
	}
}

func TestX11KeysymMapping(t *testing.T) {
	if got, want := keysymText('a'), "a"; got != want {
		t.Fatalf("keysym text = %q, want %q", got, want)
	}
	if got, want := keysymText(0x010020ac), "€"; got != want {
		t.Fatalf("unicode keysym text = %q, want %q", got, want)
	}
	if got, want := specialKey(0xff08), KeyBackspace; got != want {
		t.Fatalf("backspace keysym = %d, want %d", got, want)
	}
	if got, want := shortcutKey('c'), KeyC; got != want {
		t.Fatalf("shortcut keysym = %d, want %d", got, want)
	}
}

func TestX11ImageDataPacksTrueColorPixels(t *testing.T) {
	win := &x11Window{
		bpp:        32,
		scanPad:    32,
		byteOrder:  0,
		redShift:   16,
		greenShift: 8,
		blueShift:  0,
	}
	img := image.NewRGBA(image.Rect(0, 0, 2, 1))
	img.SetRGBA(0, 0, rgbaTest(Color{R: 0x12, G: 0x34, B: 0x56, A: 0xff}))
	img.SetRGBA(1, 0, rgbaTest(Color{R: 0xab, G: 0xcd, B: 0xef, A: 0xff}))

	data, stride := win.imageData(img)
	if stride != 8 {
		t.Fatalf("stride = %d, want 8", stride)
	}
	want := []byte{0x56, 0x34, 0x12, 0x00, 0xef, 0xcd, 0xab, 0x00}
	if string(data) != string(want) {
		t.Fatalf("packed pixels = % x, want % x", data, want)
	}
}
