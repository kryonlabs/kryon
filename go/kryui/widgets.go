// Kryon toolkit bindings beyond the raylib-compat surface: scroll
// containers and toasts. Widgets that render with kryon theme fonts
// (DrawUIButton & co.) are deliberately not bound yet — adopting them is a
// visual redesign of app's Win95-style controls and needs to happen tab by
// tab with eyes on the result.
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/../../build/linux-aarch64/libkryon.a ${SRCDIR}/../../build/linux-aarch64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <kryon.h>
#include <ui_scroll.h>
#include <ui_toast.h>
*/
import "C"

import "unsafe"

func unsafePtr(p *int) unsafe.Pointer { return unsafe.Pointer(p) }

func cfree(s *C.char) { C.free(unsafe.Pointer(s)) }

// ScrollView is the measured scroll state returned when a scroll container
// begins; draw content offset by (ContentX, ContentY) inside it.
type ScrollView struct {
	ContentX  int
	ContentY  int
	ContentW  int
	ViewportH int
	ContentH  int
	MaxScroll int
}

// ScrollOptions tunes a scroll container. Zero values pick kryon defaults.
type ScrollOptions struct {
	ContentX   int // left edge of the content area inside bounds
	ContentW   int // content width; 0 = bounds width minus the scrollbar
	WheelStep  int // pixels per wheel notch; 0 = kryon default
	ScrollbarX int // x of the scrollbar lane; 0 = right edge
}

// BeginScrollContainer starts a clipped, wheel-scrollable region and returns
// the measured view. offset is owned by the caller (persist it per tab);
// it is clamped to [0, view.MaxScroll]. Call EndScrollContainer after the
// content is drawn.
func BeginScrollContainer(bounds Rectangle, contentHeight int, offset *int, o ScrollOptions) ScrollView {
	area := C.UIScrollArea{
		bounds:         bounds.toC(),
		content_height: C.int(contentHeight),
		content_x:      C.int(o.ContentX),
		content_width:  C.int(o.ContentW),
		scroll_offset:  (*C.int)(unsafePtr(offset)),
		wheel_step:     C.int(o.WheelStep),
		scrollbar_x:    C.int(o.ScrollbarX),
	}
	view := C.BeginUIScrollContainer(area)
	return ScrollView{
		ContentX:  int(view.content_x),
		ContentY:  int(view.content_y),
		ContentW:  int(view.content_w),
		ViewportH: int(view.viewport_h),
		ContentH:  int(view.content_h),
		MaxScroll: int(view.max_scroll),
	}
}

// EndScrollContainer finishes the region started by BeginScrollContainer.
func EndScrollContainer(bounds Rectangle, view ScrollView) {
	area := C.UIScrollArea{bounds: bounds.toC()}
	cview := C.UIScrollView{
		content_x:  C.int(view.ContentX),
		content_y:  C.int(view.ContentY),
		content_w:  C.int(view.ContentW),
		viewport_h: C.int(view.ViewportH),
		content_h:  C.int(view.ContentH),
		max_scroll: C.int(view.MaxScroll),
	}
	C.EndUIScrollContainer(area, cview)
}

// ShowToast queues a transient toast with kryon's default duration.
func ShowToast(message string) {
	cm := C.CString(message)
	defer cfree(cm)
	C.ShowUIToast(cm)
}

// ShowToastFor queues a transient toast for the given seconds.
func ShowToastFor(message string, seconds float64) {
	cm := C.CString(message)
	defer cfree(cm)
	C.ShowUIToastFor(cm, C.double(seconds))
}

// ClearToast dismisses any visible toast.
func ClearToast() { C.ClearUIToast() }
