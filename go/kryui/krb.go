// KRB (Kryon Runtime Bridge) bindings - cartridge format support
// This allows loading and executing .krb files from Go
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/../../build/linux-aarch64/libkryon.a ${SRCDIR}/../../build/linux-aarch64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <string.h>
#include <kryon.h>
#include <krb.h>

// Forward declaration - actual implementation is in Go via //export
extern int krbGoCallback(int slot, void *userdata);

// Trampoline for Go callbacks
static int krbGoCallbackTrampoline(void *userdata) {
    return krbGoCallback(*(int*)userdata, userdata);
}
*/
import "C"

import (
	"sync"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const (
	KRBMagic       = 0x0042524B // "KRB\0" little-endian
	KRBVersion     = 2
	KRBNodeSize    = 28
	KRBControlSize = 24
	KRBBindMax     = 32
	KRBMountMax    = 16
	KRBFieldMax    = 64
)

// Node types
const (
	KRBNodeBackground = 1
	KRBNodeText       = 2
	KRBNodeRect       = 3
	KRBNodeButton     = 4
	KRBNodeData       = 5
	KRBNodePicture    = 6
	KRBNodeCheckbox   = 7
	KRBNodeToggle     = 8
	KRBNodeControl    = 9
	KRBNodeCircle     = 10
	KRBNodeRing       = 11
	KRBNodeScroll     = 12
	KRBNodeTextInput  = 13
)

// Control kinds
const (
	KRBCtrlSlider   = 1
	KRBCtrlVSlider  = 2
	KRBCtrlSpinbox  = 3
	KRBCtrlDropdown = 4
	KRBCtrlCombobox = 5
)

// Flags
const (
	KRBFlagNav    = 1 << 6
	KRBFlagScaleX = 1 << 2
	KRBFlagScaleY = 1 << 3
	KRBFlagScaleW = 1 << 4
	KRBFlagScaleH = 1 << 5
	KRBColorTheme = 0x80000000
)

// Field kinds
const (
	KRBI32  = 1
	KRBU32  = 2
	KRBF32  = 3
	KRBBool = 4
	KRBCStr = 5
)

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

type KRBHeader struct {
	Magic        uint32
	Version      uint16
	Flags        uint16
	NodeCount    uint32
	StringBytes  uint32
	ProgBytes    uint32
	ImportCount  uint32
	ControlCount uint32
}

type KRBNode struct {
	ID       uint16
	Parent   int16
	NameOff  uint16
	Type     uint8
	Flags    uint8
	BindSlot uint16
	X        int16
	Y        int16
	W        int16
	H        int16
	Color    uint32
	TextOff  uint16
	FontSize uint16
	Style    uint8
	Pad      uint8
}

type KRBControl struct {
	Kind        uint8
	OptionCount uint8
	ID          uint16
	Min         int32
	Max         int32
	Step        int32
	ValueOff    uint16
	LabelOff    uint16
	OptionsOff  uint16
	Reserved    uint16
}

type KRBField struct {
	Path   string
	Offset uint32
	Kind   uint32
	Size   uint32
}

type KRBImage struct {
	cimg *C.KrbImage
}

type KRBFn func() int

// Global callback registry
var (
	krbCallbacks   = make(map[int]KRBFn)
	krbCallbacksMu sync.RWMutex
	krbNextID      = 1
)

//export krbGoCallback
func krbGoCallback(slot C.int, userdata unsafe.Pointer) C.int {
	krbCallbacksMu.RLock()
	fn, ok := krbCallbacks[int(slot)]
	krbCallbacksMu.RUnlock()

	if !ok {
		return 0
	}

	return C.int(fn())
}

// ---------------------------------------------------------------------------
// Image Loading
// ---------------------------------------------------------------------------

func KRBLoad(bytes []byte) (*KRBImage, error) {
	if len(bytes) == 0 {
		return nil, nil
	}

	img := &KRBImage{
		cimg: (*C.KrbImage)(C.malloc(C.sizeof_KrbImage)),
	}

	if img.cimg == nil {
		return nil, nil
	}

	result := C.KrbLoad(img.cimg, (*C.uchar)(unsafe.Pointer(&bytes[0])), C.size_t(len(bytes)))
	if result == 0 {
		C.free(unsafe.Pointer(img.cimg))
		return nil, nil
	}

	return img, nil
}

func KRBLoadFile(path string) (*KRBImage, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	img := &KRBImage{
		cimg: (*C.KrbImage)(C.malloc(C.sizeof_KrbImage)),
	}

	if img.cimg == nil {
		return nil, nil
	}

	result := C.KrbLoadFile(img.cimg, cpath)
	if result == 0 {
		C.free(unsafe.Pointer(img.cimg))
		return nil, nil
	}

	return img, nil
}

func (img *KRBImage) Free() {
	if img.cimg != nil {
		C.KrbFree(img.cimg)
		C.free(unsafe.Pointer(img.cimg))
		img.cimg = nil
	}
}

// ---------------------------------------------------------------------------
// Binding Functions
// ---------------------------------------------------------------------------

func (img *KRBImage) Bind(name string, fn KRBFn) bool {
	if img.cimg == nil {
		return false
	}

	krbCallbacksMu.Lock()
	slot := krbNextID
	krbNextID++
	krbCallbacks[slot] = fn
	krbCallbacksMu.Unlock()

	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	slotPtr := C.malloc(C.sizeof_int)
	*(*C.int)(slotPtr) = C.int(slot)

	result := C.KrbBind(img.cimg, cname, C.KrbFn(C.krbGoCallbackTrampoline), slotPtr)
	return result != 0
}

func (img *KRBImage) BindSlot(slot uint32, fn KRBFn) bool {
	if img.cimg == nil {
		return false
	}

	krbCallbacksMu.Lock()
	cbSlot := krbNextID
	krbNextID++
	krbCallbacks[cbSlot] = fn
	krbCallbacksMu.Unlock()

	slotPtr := C.malloc(C.sizeof_int)
	*(*C.int)(slotPtr) = C.int(cbSlot)

	result := C.KrbBindSlot(img.cimg, C.uint(slot), C.KrbFn(C.krbGoCallbackTrampoline), slotPtr)
	return result != 0
}

// ---------------------------------------------------------------------------
// Query Functions
// ---------------------------------------------------------------------------

func (img *KRBImage) String(off uint32) string {
	if img.cimg == nil {
		return ""
	}
	return C.GoString(C.KrbString(img.cimg, C.uint(off)))
}

func (img *KRBImage) ImportName(slot uint32) string {
	if img.cimg == nil {
		return ""
	}
	return C.GoString(C.KrbImportName(img.cimg, C.uint(slot)))
}

func (img *KRBImage) NodeCount() uint32 {
	if img.cimg == nil {
		return 0
	}
	return uint32(C.KrbNodeCount(img.cimg))
}

func (img *KRBImage) ImportCount() uint32 {
	if img.cimg == nil {
		return 0
	}
	return uint32(C.KrbImportCount(img.cimg))
}

func (img *KRBImage) ReadNode(index uint32) (KRBNode, bool) {
	if img.cimg == nil {
		return KRBNode{}, false
	}

	var cn C.KrbNode
	result := C.KrbReadNode(img.cimg, C.uint(index), &cn)
	if result == 0 {
		return KRBNode{}, false
	}

	return KRBNode{
		ID:       uint16(cn.id),
		Parent:   int16(cn.parent),
		NameOff:  uint16(cn.name_off),
		Type:     uint8(cn._type),
		Flags:    uint8(cn.flags),
		BindSlot: uint16(cn.bind_slot),
		X:        int16(cn.x),
		Y:        int16(cn.y),
		W:        int16(cn.w),
		H:        int16(cn.h),
		Color:    uint32(cn.color),
		TextOff:  uint16(cn.text_off),
		FontSize: uint16(cn.font_size),
		Style:    uint8(cn.style),
		Pad:      uint8(cn.pad),
	}, true
}

func (img *KRBImage) ControlCount() uint32 {
	if img.cimg == nil {
		return 0
	}
	return uint32(C.KrbControlCount(img.cimg))
}

func (img *KRBImage) ReadControl(index uint32) (KRBControl, bool) {
	if img.cimg == nil {
		return KRBControl{}, false
	}

	var cc C.KrbControl
	result := C.KrbReadControl(img.cimg, C.uint(index), &cc)
	if result == 0 {
		return KRBControl{}, false
	}

	return KRBControl{
		Kind:        uint8(cc.kind),
		OptionCount: uint8(cc.option_count),
		ID:          uint16(cc.id),
		Min:         int32(cc.min),
		Max:         int32(cc.max),
		Step:        int32(cc.step),
		ValueOff:    uint16(cc.value_off),
		LabelOff:    uint16(cc.label_off),
		OptionsOff:  uint16(cc.options_off),
		Reserved:    uint16(cc.reserved),
	}, true
}

// ---------------------------------------------------------------------------
// Mount and State Functions
// ---------------------------------------------------------------------------

func (img *KRBImage) Mount(root string, base unsafe.Pointer, fields []KRBField) bool {
	if img.cimg == nil {
		return false
	}

	croot := C.CString(root)
	defer C.free(unsafe.Pointer(croot))

	cfields := make([]C.KrbField, len(fields))
	cpaths := make([]*C.char, len(fields))

	for i, f := range fields {
		cpaths[i] = C.CString(f.Path)
		cfields[i].path = cpaths[i]
		cfields[i].offset = C.uint(f.Offset)
		cfields[i].kind = C.uint(f.Kind)
		cfields[i].size = C.uint(f.Size)
	}

	defer func() {
		for _, cp := range cpaths {
			C.free(unsafe.Pointer(cp))
		}
	}()

	var cfieldsPtr *C.KrbField
	if len(cfields) > 0 {
		cfieldsPtr = &cfields[0]
	}

	result := C.KrbMount(img.cimg, croot, base, cfieldsPtr)
	return result != 0
}

func (img *KRBImage) BindMem(path string, ptr unsafe.Pointer, kind, size uint32) bool {
	if img.cimg == nil {
		return false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	result := C.KrbBindMem(img.cimg, cpath, ptr, C.uint(kind), C.uint(size))
	return result != 0
}

func (img *KRBImage) ReadI32(path string) (int32, bool) {
	if img.cimg == nil {
		return 0, false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	var val C.int
	result := C.KrbReadI32(img.cimg, cpath, &val)
	return int32(val), result != 0
}

func (img *KRBImage) WriteI32(path string, value int32) bool {
	if img.cimg == nil {
		return false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	result := C.KrbWriteI32(img.cimg, cpath, C.int(value))
	return result != 0
}

func (img *KRBImage) ReadF32(path string) (float32, bool) {
	if img.cimg == nil {
		return 0, false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	var val C.float
	result := C.KrbReadF32(img.cimg, cpath, &val)
	return float32(val), result != 0
}

func (img *KRBImage) WriteF32(path string, value float32) bool {
	if img.cimg == nil {
		return false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	result := C.KrbWriteF32(img.cimg, cpath, C.float(value))
	return result != 0
}

func (img *KRBImage) ReadCStr(path string, maxLen int) (string, bool) {
	if img.cimg == nil {
		return "", false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	buf := make([]byte, maxLen)
	result := C.KrbReadCStr(img.cimg, cpath, (*C.char)(unsafe.Pointer(&buf[0])), C.size_t(maxLen))
	if result == 0 {
		return "", false
	}

	// Find null terminator
	n := 0
	for n < len(buf) && buf[n] != 0 {
		n++
	}
	return string(buf[:n]), true
}

func (img *KRBImage) WriteCStr(path, value string) bool {
	if img.cimg == nil {
		return false
	}

	cpath := C.CString(path)
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cpath))
	defer C.free(unsafe.Pointer(cvalue))

	result := C.KrbWriteCStr(img.cimg, cpath, cvalue)
	return result != 0
}

// ---------------------------------------------------------------------------
// Asset Functions
// ---------------------------------------------------------------------------

func (img *KRBImage) AssetFind(path string) (data []byte, kind, width, height uint32, ok bool) {
	if img.cimg == nil {
		return nil, 0, 0, 0, false
	}

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	var cdata *C.uchar
	var clen, ckind, cw, ch C.uint

	result := C.KrbAssetFind(img.cimg, cpath, &cdata, &clen, &ckind, &cw, &ch)
	if result == 0 {
		return nil, 0, 0, 0, false
	}

	// Copy data to Go slice
	data = C.GoBytes(unsafe.Pointer(cdata), C.int(clen))
	return data, uint32(ckind), uint32(cw), uint32(ch), true
}

// ---------------------------------------------------------------------------
// Execution Functions
// ---------------------------------------------------------------------------

func (img *KRBImage) Exec() bool {
	if img.cimg == nil {
		return false
	}
	result := C.KrbExec(img.cimg)
	return result != 0
}

func (img *KRBImage) Draw(x, y, w, h int32) {
	if img.cimg == nil {
		return
	}
	C.KrbDraw(img.cimg, C.int(x), C.int(y), C.int(w), C.int(h))
}
