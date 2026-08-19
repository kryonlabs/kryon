package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/../../build/linux-aarch64/libkryon.a ${SRCDIR}/../../build/linux-aarch64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <kryon.h>
#include <ui_nav.h>
*/
import "C"

import (
	"strings"
	"unsafe"
)

// TabBar draws the kryon tab strip with plain-string labels (labels, colors
// and overflow scrolling come from the theme; no UITab structs needed). The
// clicked tab index is returned, or -1 when no tab was clicked this frame.
// selected is updated in place; scrollOffset may be nil.
func TabBar(bounds Rectangle, labels []string, selected *int32, scrollOffset *int32) int32 {
	if len(labels) == 0 || selected == nil {
		return -1
	}
	tabs := make([]C.UITab, len(labels))
	for i, label := range labels {
		cstr := C.CString(label)
		tabs[i].label = cstr
		// CString must stay alive until the C call returns.
		defer C.free(unsafe.Pointer(cstr))
	}
	sel := C.int(*selected)
	var scroll *C.int
	if scrollOffset != nil {
		scroll = (*C.int)(unsafe.Pointer(scrollOffset))
	}
	props := C.TabBarProps{}
	props.bounds = bounds.toC()
	props.tabs = &tabs[0]
	props.count = C.int(len(tabs))
	props.selected_index = sel
	props.font = 0
	props.min_tab_width = 0
	props.max_tab_width = 0
	props.scroll_offset = scroll
	props.focus_selected = 0
	props.closed_index = nil
	props.double_clicked_index = nil
	clicked := C.DrawUITabBar(props)
	if clicked >= 0 {
		*selected = int32(clicked)
	}
	return int32(clicked)
}

// TabBarHeight is the theme's tab strip height in scaled pixels.
func TabBarHeight() int32 {
	return int32(C.GetUITabBarHeight())
}

// DropdownLabels splits the ";"-joined option list used by the cartridge
// Dropdown builtin into Go strings.
func DropdownLabels(options string) []string {
	return strings.Split(options, ";")
}
