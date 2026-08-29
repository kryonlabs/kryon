//go:build linux

package kryon

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"image/color"
	"math/bits"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"
)

type windowRuntime struct {
	Runtime
	window *x11Window
	fps    int
	last   time.Time
	closed bool
}

type legacyPointerController interface {
	QueueMouseButton(int32, float32, float32)
	QueueMouseWheel(float32)
}

func openWindowRuntime(config AppConfig) (Runtime, error) {
	base := New(config)
	win, err := openX11Window(config)
	if err != nil {
		return nil, err
	}
	return &windowRuntime{Runtime: base, window: win, fps: config.FPS}, nil
}

func (r *windowRuntime) Close() {
	r.closed = true
	if r.window != nil {
		r.window.close()
	}
}

func (r *windowRuntime) WindowShouldClose() bool {
	return r.closed
}

func (r *windowRuntime) BeginFrame() {
	r.pumpEvents()
	r.Runtime.BeginFrame()
}

func (r *windowRuntime) EndFrame() {
	r.Runtime.EndFrame()
	if r.window == nil {
		return
	}
	ops := []FrameOp(nil)
	if fr, ok := r.Runtime.(frameOpController); ok {
		ops = fr.FrameOps()
	}
	img := RenderFrame(int(r.GetScreenWidth()), int(r.GetScreenHeight()), ops)
	if err := r.window.present(img); err != nil {
		r.closed = true
		return
	}
	if r.fps > 0 {
		frame := time.Second / time.Duration(r.fps)
		if !r.last.IsZero() {
			if wait := frame - time.Since(r.last); wait > 0 {
				time.Sleep(wait)
			}
		}
		r.last = time.Now()
	}
}

func (r *windowRuntime) GetScreenWidth() int32 {
	if r.window != nil && r.window.width > 0 {
		return int32(r.window.width)
	}
	return r.Runtime.GetScreenWidth()
}

func (r *windowRuntime) GetScreenHeight() int32 {
	if r.window != nil && r.window.height > 0 {
		return int32(r.window.height)
	}
	return r.Runtime.GetScreenHeight()
}

func (r *windowRuntime) pumpEvents() {
	if r.window == nil {
		return
	}
	events, err := r.window.poll()
	if err != nil {
		r.closed = true
		return
	}
	for _, ev := range events {
		switch ev.kind {
		case x11EventClose:
			r.closed = true
		case x11EventResize:
			r.window.width = ev.x
			r.window.height = ev.y
		case x11EventTap:
			if c, ok := r.Runtime.(mouseController); ok {
				c.QueueMouseButtonDown(ev.button, float32(ev.x), float32(ev.y))
			} else if c, ok := r.Runtime.(legacyPointerController); ok {
				c.QueueMouseButton(ev.button, float32(ev.x), float32(ev.y))
			}
		case x11EventMotion:
			if c, ok := r.Runtime.(mouseController); ok {
				c.QueueMouseMove(float32(ev.x), float32(ev.y))
			}
		case x11EventRelease:
			if c, ok := r.Runtime.(mouseController); ok {
				c.QueueMouseButtonUp(ev.button, float32(ev.x), float32(ev.y))
			}
		case x11EventWheel:
			if c, ok := r.Runtime.(legacyPointerController); ok {
				c.QueueMouseWheel(ev.wheel)
			}
		case x11EventKey:
			if c, ok := r.Runtime.(inputController); ok {
				if ev.shortcut != 0 {
					c.QueueShortcut(ev.shortcut)
				} else if ev.key != 0 {
					c.QueueKey(ev.key)
				} else if ev.text != "" {
					c.QueueText(ev.text)
				}
			}
		}
	}
}

func (r *windowRuntime) QueueTap(x, y float32) {
	if c, ok := r.Runtime.(pointerController); ok {
		c.QueueTap(x, y)
	}
}

func (r *windowRuntime) QueueText(text string) {
	if c, ok := r.Runtime.(inputController); ok {
		c.QueueText(text)
	}
}

func (r *windowRuntime) QueueKey(key int32) {
	if c, ok := r.Runtime.(inputController); ok {
		c.QueueKey(key)
	}
}

func (r *windowRuntime) QueueShiftKey(key int32) {
	if c, ok := r.Runtime.(inputController); ok {
		c.QueueShiftKey(key)
	}
}

func (r *windowRuntime) QueueShortcut(key int32) {
	if c, ok := r.Runtime.(inputController); ok {
		c.QueueShortcut(key)
	}
}

func (r *windowRuntime) SetFocus(id int32) {
	if c, ok := r.Runtime.(focusController); ok {
		c.SetFocus(id)
	}
}

func (r *windowRuntime) Focus() int32 {
	if c, ok := r.Runtime.(focusController); ok {
		return c.Focus()
	}
	return 0
}

func (r *windowRuntime) FrameOps() []FrameOp {
	if c, ok := r.Runtime.(frameOpController); ok {
		return c.FrameOps()
	}
	return nil
}

func (r *windowRuntime) MousePosition() Vector2 {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.MousePosition()
	}
	return Vector2{}
}

func (r *windowRuntime) MouseButtonPressed(button int32) bool {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.MouseButtonPressed(button)
	}
	return false
}

func (r *windowRuntime) MouseButtonDown(button int32) bool {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.MouseButtonDown(button)
	}
	return false
}

func (r *windowRuntime) MouseButtonReleased(button int32) bool {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.MouseButtonReleased(button)
	}
	return false
}

func (r *windowRuntime) MouseWheelMove() float32 {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.MouseWheelMove()
	}
	return 0
}

func (r *windowRuntime) KeyPressed(key int32) bool {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.KeyPressed(key)
	}
	return false
}

// KeyDown completes compatInputRuntime: without it every compat input read
// (mouse position, buttons, keys) fails its interface assertion against
// windowRuntime and the whole mouse UI goes dead.
func (r *windowRuntime) KeyDown(key int32) bool {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.KeyDown(key)
	}
	return false
}

func (r *windowRuntime) CharPressed() int32 {
	if c, ok := r.Runtime.(compatInputRuntime); ok {
		return c.CharPressed()
	}
	return 0
}

const (
	x11EventKeyPress        = 2
	x11EventButtonPress     = 4
	x11EventButtonRelease   = 5
	x11EventMotionNotify    = 6
	x11EventExpose          = 12
	x11EventConfigureNotify = 22
	x11EventClientMessage   = 33

	x11InputOutput = 1
	x11ZPixmap     = 2

	x11EventMaskKeyPress        = 1 << 0
	x11EventMaskButtonPress     = 1 << 2
	x11EventMaskButtonRelease   = 1 << 3
	x11EventMaskPointerMotion   = 1 << 6
	x11EventMaskExposure        = 1 << 15
	x11EventMaskStructureNotify = 1 << 17

	x11CWBackPixel = 1 << 1
	x11CWEventMask = 1 << 11

	x11AtomAtom = 4

	x11ShiftMask   = 1
	x11ControlMask = 4
)

type x11Window struct {
	conn net.Conn
	seq  uint16

	resourceBase uint32
	resourceMask uint32
	nextResource uint32
	maxRequest   uint32

	root       uint32
	visual     uint32
	depth      uint8
	bpp        uint8
	scanPad    uint8
	byteOrder  byte
	redMask    uint32
	greenMask  uint32
	blueMask   uint32
	redShift   uint
	greenShift uint
	blueShift  uint

	window uint32
	gc     uint32
	width  int
	height int

	wmProtocols    uint32
	wmDeleteWindow uint32

	minKeycode uint8
	maxKeycode uint8
	keysyms    map[uint8][]uint32
}

type x11EventKind int

const (
	x11EventClose x11EventKind = iota + 1
	x11EventResize
	x11EventTap
	x11EventMotion
	x11EventRelease
	x11EventWheel
	x11EventKey
)

type x11Event struct {
	kind     x11EventKind
	x, y     int
	button   int32
	wheel    float32
	key      int32
	shortcut int32
	text     string
}

func openX11Window(config AppConfig) (*x11Window, error) {
	display := os.Getenv("DISPLAY")
	if display == "" {
		return nil, errors.New("DISPLAY is not set")
	}
	conn, err := net.Dial("unix", x11Socket(display))
	if err != nil {
		return nil, err
	}
	win := &x11Window{conn: conn}
	if err := win.handshake(); err != nil {
		conn.Close()
		return nil, err
	}
	if config.Width <= 0 {
		config.Width = 640
	}
	if config.Height <= 0 {
		config.Height = 480
	}
	if err := win.create(config); err != nil {
		conn.Close()
		return nil, err
	}
	if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
		fmt.Fprintf(os.Stderr, "kryon: opened x11 window %#x\n", win.window)
	}
	return win, nil
}

func x11Socket(display string) string {
	number := x11DisplayNumber(display)
	n, err := strconv.Atoi(number)
	if err != nil {
		n = 0
	}
	return fmt.Sprintf("/tmp/.X11-unix/X%d", n)
}

func x11DisplayNumber(display string) string {
	if strings.HasPrefix(display, ":") {
		display = display[1:]
	}
	if i := strings.IndexAny(display, "."); i >= 0 {
		display = display[:i]
	}
	if _, err := strconv.Atoi(display); err != nil {
		return "0"
	}
	return display
}

func x11Auth(display string) ([]byte, []byte) {
	path := os.Getenv("XAUTHORITY")
	if path == "" {
		if home, err := os.UserHomeDir(); err == nil {
			path = home + "/.Xauthority"
		}
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, nil
	}
	wantNumber := []byte(x11DisplayNumber(display))
	var bestName, bestData []byte
	for len(data) >= 12 {
		family := binary.BigEndian.Uint16(data[:2])
		data = data[2:]
		address, ok := xauthPart(&data)
		if !ok {
			break
		}
		number, ok := xauthPart(&data)
		if !ok {
			break
		}
		name, ok := xauthPart(&data)
		if !ok {
			break
		}
		auth, ok := xauthPart(&data)
		if !ok {
			break
		}
		_ = address
		if !bytes.Equal(number, wantNumber) || !bytes.Equal(name, []byte("MIT-MAGIC-COOKIE-1")) {
			continue
		}
		if family == 256 || family == 65535 {
			return name, auth
		}
		if bestName == nil {
			bestName, bestData = name, auth
		}
	}
	return bestName, bestData
}

func xauthPart(data *[]byte) ([]byte, bool) {
	if len(*data) < 2 {
		return nil, false
	}
	n := int(binary.BigEndian.Uint16((*data)[:2]))
	*data = (*data)[2:]
	if len(*data) < n {
		return nil, false
	}
	out := (*data)[:n]
	*data = (*data)[n:]
	return out, true
}

func (w *x11Window) handshake() error {
	authName, authData := x11Auth(os.Getenv("DISPLAY"))
	req := make([]byte, 12+pad4(len(authName))+pad4(len(authData)))
	req[0] = 'l'
	put16(req[2:], 11)
	put16(req[6:], uint16(len(authName)))
	put16(req[8:], uint16(len(authData)))
	copy(req[12:], authName)
	copy(req[12+pad4(len(authName)):], authData)
	if _, err := w.conn.Write(req); err != nil {
		return err
	}
	head := make([]byte, 8)
	if _, err := readFull(w.conn, head); err != nil {
		return err
	}
	extra := int(get16(head[6:])) * 4
	body := make([]byte, extra)
	if _, err := readFull(w.conn, body); err != nil {
		return err
	}
	if head[0] != 1 {
		reason := ""
		if int(head[1]) <= len(body) {
			reason = string(body[:head[1]])
		}
		return fmt.Errorf("x11 setup rejected: %s", reason)
	}
	if len(body) < 32 {
		return errors.New("x11 setup response too short")
	}
	w.resourceBase = get32(body[4:])
	w.resourceMask = get32(body[8:])
	vendorLen := int(get16(body[16:]))
	w.maxRequest = uint32(get16(body[18:]))
	roots := int(body[20])
	formats := int(body[21])
	w.byteOrder = body[22]
	w.minKeycode = body[26]
	w.maxKeycode = body[27]
	off := 32 + pad4(vendorLen)
	formatsByDepth := map[uint8][2]uint8{}
	for i := 0; i < formats && off+8 <= len(body); i++ {
		formatsByDepth[body[off]] = [2]uint8{body[off+1], body[off+2]}
		off += 8
	}
	if roots == 0 || off+40 > len(body) {
		return errors.New("x11 setup has no screen")
	}
	w.root = get32(body[off:])
	white := get32(body[off+8:])
	_ = white
	w.width = int(get16(body[off+20:]))
	w.height = int(get16(body[off+22:]))
	w.visual = get32(body[off+32:])
	w.depth = body[off+38]
	depths := int(body[off+39])
	off += 40
	for d := 0; d < depths && off+8 <= len(body); d++ {
		depth := body[off]
		visuals := int(get16(body[off+2:]))
		off += 8
		for v := 0; v < visuals && off+24 <= len(body); v++ {
			visualID := get32(body[off:])
			if visualID == w.visual {
				w.redMask = get32(body[off+8:])
				w.greenMask = get32(body[off+12:])
				w.blueMask = get32(body[off+16:])
				w.redShift = lowBit(w.redMask)
				w.greenShift = lowBit(w.greenMask)
				w.blueShift = lowBit(w.blueMask)
			}
			off += 24
		}
		_ = depth
	}
	if f, ok := formatsByDepth[w.depth]; ok {
		w.bpp = f[0]
		w.scanPad = f[1]
	}
	if w.bpp == 0 {
		w.bpp = 32
	}
	if w.maxRequest == 0 {
		w.maxRequest = 65535
	}
	if w.scanPad == 0 {
		w.scanPad = 32
	}
	if w.redMask == 0 {
		w.redMask, w.greenMask, w.blueMask = 0xff0000, 0x00ff00, 0x0000ff
		w.redShift, w.greenShift, w.blueShift = 16, 8, 0
	}
	if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
		fmt.Fprintf(os.Stderr, "kryon: x11 visual=%#x depth=%d bpp=%d scanPad=%d byteOrder=%d maxRequest=%d masks=%#x/%#x/%#x\n",
			w.visual, w.depth, w.bpp, w.scanPad, w.byteOrder, w.maxRequest, w.redMask, w.greenMask, w.blueMask)
	}
	return nil
}

func (w *x11Window) create(config AppConfig) error {
	width, height := uint16(config.Width), uint16(config.Height)
	w.width, w.height = int(width), int(height)
	w.window = w.allocID()
	w.gc = w.allocID()

	values := []uint32{
		0xffffff,
		x11EventMaskKeyPress | x11EventMaskButtonPress | x11EventMaskButtonRelease | x11EventMaskPointerMotion | x11EventMaskExposure | x11EventMaskStructureNotify,
	}
	req := make([]byte, 32+len(values)*4)
	req[0] = 1
	req[1] = w.depth
	put16(req[2:], uint16(len(req)/4))
	put32(req[4:], w.window)
	put32(req[8:], w.root)
	put16(req[16:], width)
	put16(req[18:], height)
	put16(req[22:], x11InputOutput)
	put32(req[24:], w.visual)
	put32(req[28:], x11CWBackPixel|x11CWEventMask)
	for i, v := range values {
		put32(req[32+i*4:], v)
	}
	if err := w.write(req); err != nil {
		return err
	}
	if config.Title != "" {
		_ = w.changeStringProperty("WM_NAME", config.Title)
		_ = w.changeStringProperty("_NET_WM_NAME", config.Title)
	}
	var err error
	w.wmProtocols, err = w.internAtom("WM_PROTOCOLS")
	if err == nil {
		w.wmDeleteWindow, err = w.internAtom("WM_DELETE_WINDOW")
	}
	if err == nil {
		_ = w.changeAtomProperty(w.wmProtocols, []uint32{w.wmDeleteWindow})
	}
	if err := w.createGC(); err != nil {
		return err
	}
	if err := w.loadKeyboardMapping(); err != nil {
		return err
	}
	if err := w.mapWindow(); err != nil {
		return err
	}
	return w.sync()
}

func (w *x11Window) allocID() uint32 {
	w.nextResource = (w.nextResource + 1) & w.resourceMask
	return w.resourceBase | w.nextResource
}

func (w *x11Window) write(req []byte) error {
	_, err := w.conn.Write(req)
	if err == nil {
		w.seq++
	}
	return err
}

func (w *x11Window) mapWindow() error {
	req := make([]byte, 8)
	req[0] = 8
	put16(req[2:], 2)
	put32(req[4:], w.window)
	return w.write(req)
}

func (w *x11Window) setInputFocus() error {
	req := make([]byte, 12)
	req[0] = 42
	req[1] = 1
	put16(req[2:], 3)
	put32(req[4:], w.window)
	put32(req[8:], 0)
	return w.write(req)
}

func (w *x11Window) sync() error {
	req := []byte{43, 0, 1, 0}
	if err := w.write(req); err != nil {
		return err
	}
	_, err := w.readReply()
	return err
}

func (w *x11Window) createGC() error {
	req := make([]byte, 16)
	req[0] = 55
	put16(req[2:], 4)
	put32(req[4:], w.gc)
	put32(req[8:], w.window)
	put32(req[12:], 0)
	return w.write(req)
}

func (w *x11Window) changeStringProperty(name, value string) error {
	atom, err := w.internAtom(name)
	if err != nil {
		return err
	}
	utf8Atom := uint32(31)
	if name == "_NET_WM_NAME" {
		if a, err := w.internAtom("UTF8_STRING"); err == nil {
			utf8Atom = a
		}
	}
	data := []byte(value)
	padded := padBytes(data)
	req := make([]byte, 24+len(padded))
	req[0] = 18
	req[1] = 0
	put16(req[2:], uint16(len(req)/4))
	put32(req[4:], w.window)
	put32(req[8:], atom)
	put32(req[12:], utf8Atom)
	req[16] = 8
	put32(req[20:], uint32(len(data)))
	copy(req[24:], padded)
	return w.write(req)
}

func (w *x11Window) changeAtomProperty(property uint32, atoms []uint32) error {
	req := make([]byte, 24+len(atoms)*4)
	req[0] = 18
	put16(req[2:], uint16(len(req)/4))
	put32(req[4:], w.window)
	put32(req[8:], property)
	put32(req[12:], x11AtomAtom)
	req[16] = 32
	put32(req[20:], uint32(len(atoms)))
	for i, atom := range atoms {
		put32(req[24+i*4:], atom)
	}
	return w.write(req)
}

func (w *x11Window) internAtom(name string) (uint32, error) {
	padded := padBytes([]byte(name))
	req := make([]byte, 8+len(padded))
	req[0] = 16
	put16(req[2:], uint16(len(req)/4))
	put16(req[4:], uint16(len(name)))
	copy(req[8:], padded)
	if err := w.write(req); err != nil {
		return 0, err
	}
	reply, err := w.readReply()
	if err != nil {
		return 0, err
	}
	return get32(reply[8:]), nil
}

func (w *x11Window) loadKeyboardMapping() error {
	if w.maxKeycode < w.minKeycode {
		return nil
	}
	count := int(w.maxKeycode-w.minKeycode) + 1
	req := make([]byte, 8)
	req[0] = 101
	put16(req[2:], 2)
	req[4] = w.minKeycode
	req[5] = uint8(count)
	if err := w.write(req); err != nil {
		return err
	}
	reply, err := w.readReply()
	if err != nil {
		return err
	}
	per := int(reply[1])
	dataLen := int(get32(reply[4:])) * 4
	data := make([]byte, dataLen)
	if _, err := readFull(w.conn, data); err != nil {
		return err
	}
	w.keysyms = map[uint8][]uint32{}
	off := 0
	for kc := w.minKeycode; kc <= w.maxKeycode && off+per*4 <= len(data); kc++ {
		syms := make([]uint32, per)
		for i := range syms {
			syms[i] = get32(data[off+i*4:])
		}
		w.keysyms[kc] = syms
		off += per * 4
	}
	return nil
}

func (w *x11Window) readReply() ([]byte, error) {
	for {
		head := make([]byte, 32)
		if _, err := readFull(w.conn, head); err != nil {
			return nil, err
		}
		if head[0] == 1 {
			return head, nil
		}
		if head[0] == 0 {
			return nil, fmt.Errorf("x11 error opcode=%d code=%d", head[10], head[1])
		}
	}
}

func (w *x11Window) present(img *image.RGBA) error {
	if img == nil {
		return nil
	}
	b := img.Bounds()
	if b.Dx() <= 0 || b.Dy() <= 0 {
		return nil
	}
	data, stride := w.imageData(img)
	maxData := int(w.maxRequest)*4 - 24
	if maxData <= 0 {
		maxData = 65535*4 - 24
	}
	rowsPerChunk := maxData / stride
	if rowsPerChunk < 1 {
		rowsPerChunk = 1
	}
	for y := 0; y < b.Dy(); y += rowsPerChunk {
		rows := rowsPerChunk
		if y+rows > b.Dy() {
			rows = b.Dy() - y
		}
		chunk := data[y*stride : (y+rows)*stride]
		req := make([]byte, 24+len(chunk))
		req[0] = 72
		req[1] = x11ZPixmap
		put16(req[2:], uint16(len(req)/4))
		put32(req[4:], w.window)
		put32(req[8:], w.gc)
		put16(req[12:], uint16(b.Dx()))
		put16(req[14:], uint16(rows))
		put16(req[18:], uint16(y))
		req[21] = w.depth
		copy(req[24:], chunk)
		if err := w.write(req); err != nil {
			return err
		}
	}
	return nil
}

func (w *x11Window) imageData(img *image.RGBA) ([]byte, int) {
	b := img.Bounds()
	bytesPerPixel := int((w.bpp + 7) / 8)
	if bytesPerPixel < 1 || bytesPerPixel > 4 {
		bytesPerPixel = 4
	}
	rowBits := b.Dx() * int(w.bpp)
	padBits := int(w.scanPad)
	if padBits <= 0 {
		padBits = 32
	}
	stride := ((rowBits + padBits - 1) / padBits) * (padBits / 8)
	out := make([]byte, stride*b.Dy())
	for y := 0; y < b.Dy(); y++ {
		for x := 0; x < b.Dx(); x++ {
			c := img.RGBAAt(b.Min.X+x, b.Min.Y+y)
			pixel := w.pixel(c)
			off := y*stride + x*bytesPerPixel
			switch bytesPerPixel {
			case 1:
				out[off] = byte(pixel)
			case 2:
				if w.byteOrder == 0 {
					binary.LittleEndian.PutUint16(out[off:], uint16(pixel))
				} else {
					binary.BigEndian.PutUint16(out[off:], uint16(pixel))
				}
			case 3:
				if w.byteOrder == 0 {
					out[off] = byte(pixel)
					out[off+1] = byte(pixel >> 8)
					out[off+2] = byte(pixel >> 16)
				} else {
					out[off] = byte(pixel >> 16)
					out[off+1] = byte(pixel >> 8)
					out[off+2] = byte(pixel)
				}
			default:
				if w.byteOrder == 0 {
					binary.LittleEndian.PutUint32(out[off:], pixel)
				} else {
					binary.BigEndian.PutUint32(out[off:], pixel)
				}
			}
		}
	}
	return out, stride
}

func (w *x11Window) pixel(c color.RGBA) uint32 {
	return x11Component(c.R, w.redMask, w.redShift) |
		x11Component(c.G, w.greenMask, w.greenShift) |
		x11Component(c.B, w.blueMask, w.blueShift)
}

func x11Component(value uint8, mask uint32, shift uint) uint32 {
	if mask == 0 {
		return 0
	}
	width := bits.OnesCount32(mask)
	if width <= 0 {
		return 0
	}
	max := uint32((1 << width) - 1)
	return ((uint32(value)*max + 127) / 255) << shift
}

func (w *x11Window) poll() ([]x11Event, error) {
	var out []x11Event
	for {
		_ = w.conn.SetReadDeadline(time.Now().Add(time.Millisecond))
		buf := make([]byte, 32)
		_, err := readFull(w.conn, buf)
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				_ = w.conn.SetReadDeadline(time.Time{})
				return out, nil
			}
			return out, err
		}
		if ev, ok := w.decodeEvent(buf); ok {
			out = append(out, ev)
		}
	}
}

func (w *x11Window) decodeEvent(buf []byte) (x11Event, bool) {
	switch buf[0] & 0x7f {
	case x11EventButtonPress:
		_ = w.setInputFocus()
		button := int32(buf[1])
		x := int(int16(get16(buf[24:])))
		y := int(int16(get16(buf[26:])))
		switch button {
		case 1:
			return x11Event{kind: x11EventTap, button: MouseButtonLeft, x: x, y: y}, true
		case 3:
			return x11Event{kind: x11EventTap, button: MouseButtonRight, x: x, y: y}, true
		case 4:
			return x11Event{kind: x11EventWheel, wheel: 1}, true
		case 5:
			return x11Event{kind: x11EventWheel, wheel: -1}, true
		}
	case x11EventButtonRelease:
		button := int32(buf[1])
		x := int(int16(get16(buf[24:])))
		y := int(int16(get16(buf[26:])))
		switch button {
		case 1:
			return x11Event{kind: x11EventRelease, button: MouseButtonLeft, x: x, y: y}, true
		case 3:
			return x11Event{kind: x11EventRelease, button: MouseButtonRight, x: x, y: y}, true
		}
	case x11EventMotionNotify:
		x := int(int16(get16(buf[24:])))
		y := int(int16(get16(buf[26:])))
		return x11Event{kind: x11EventMotion, x: x, y: y}, true
	case x11EventConfigureNotify:
		return x11Event{kind: x11EventResize, x: int(get16(buf[20:])), y: int(get16(buf[22:]))}, true
	case x11EventClientMessage:
		if get32(buf[8:]) == w.wmProtocols && get32(buf[12:]) == w.wmDeleteWindow {
			return x11Event{kind: x11EventClose}, true
		}
	case x11EventKeyPress:
		if ev, ok := w.decodeKey(buf[1], get16(buf[28:])); ok {
			return ev, true
		}
	case x11EventExpose:
		return x11Event{}, false
	}
	return x11Event{}, false
}

func (w *x11Window) decodeKey(keycode uint8, state uint16) (x11Event, bool) {
	syms := w.keysyms[keycode]
	if len(syms) == 0 {
		if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
			fmt.Fprintf(os.Stderr, "kryon: x11 key keycode=%d state=%#x no mapping\n", keycode, state)
		}
		return x11Event{}, false
	}
	shift := state&x11ShiftMask != 0
	ctrl := state&x11ControlMask != 0
	ks := syms[0]
	if shift && len(syms) > 1 && syms[1] != 0 {
		ks = syms[1]
	}
	if ctrl {
		if k := shortcutKey(ks); k != 0 {
			if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
				fmt.Fprintf(os.Stderr, "kryon: x11 key keycode=%d state=%#x keysym=%#x shortcut=%d\n", keycode, state, ks, k)
			}
			return x11Event{kind: x11EventKey, shortcut: k}, true
		}
	}
	if key := specialKey(ks); key != 0 {
		if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
			fmt.Fprintf(os.Stderr, "kryon: x11 key keycode=%d state=%#x keysym=%#x special=%d\n", keycode, state, ks, key)
		}
		return x11Event{kind: x11EventKey, key: key}, true
	}
	if text := keysymText(ks); text != "" {
		if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
			fmt.Fprintf(os.Stderr, "kryon: x11 key keycode=%d state=%#x keysym=%#x text=%q\n", keycode, state, ks, text)
		}
		return x11Event{kind: x11EventKey, text: text}, true
	}
	if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
		fmt.Fprintf(os.Stderr, "kryon: x11 key keycode=%d state=%#x keysym=%#x ignored\n", keycode, state, ks)
	}
	return x11Event{}, false
}

func (w *x11Window) close() {
	if w.conn != nil {
		_ = w.conn.Close()
		w.conn = nil
	}
}

func specialKey(ks uint32) int32 {
	switch ks {
	case 0xff1b:
		return KeyEscape
	case 0xff08:
		return KeyBackspace
	case 0xff09:
		return KeyTab
	case 0xff0d, 0xff8d:
		return KeyEnter
	case 0xffff, 0xff9f:
		return KeyDelete
	case 0xff51, 0xff96:
		return KeyLeft
	case 0xff52, 0xff97:
		return KeyUp
	case 0xff53, 0xff98:
		return KeyRight
	case 0xff54, 0xff99:
		return KeyDown
	case 0xff50, 0xff95:
		return KeyHome
	case 0xff57, 0xff9c:
		return KeyEnd
	case 0xffbf:
		return KeyF2
	}
	return 0
}

func shortcutKey(ks uint32) int32 {
	switch ks {
	case 'a', 'A':
		return KeyA
	case 'c', 'C':
		return KeyC
	case 'v', 'V':
		return KeyV
	case 'x', 'X':
		return KeyX
	}
	return 0
}

func keysymText(ks uint32) string {
	if ks >= 0x20 && ks <= 0x7e {
		return string(rune(ks))
	}
	if ks >= 0xa0 && ks <= 0xff {
		return string(rune(ks))
	}
	if ks >= 0x01000000 && ks <= 0x0110ffff {
		r := rune(ks - 0x01000000)
		if utf8.ValidRune(r) {
			return string(r)
		}
	}
	return ""
}

func readFull(conn net.Conn, buf []byte) (int, error) {
	read := 0
	for read < len(buf) {
		n, err := conn.Read(buf[read:])
		read += n
		if err != nil {
			return read, err
		}
	}
	return read, nil
}

func pad4(n int) int {
	return (n + 3) &^ 3
}

func padBytes(b []byte) []byte {
	out := make([]byte, pad4(len(b)))
	copy(out, b)
	return out
}

func put16(b []byte, v uint16) { binary.LittleEndian.PutUint16(b, v) }
func put32(b []byte, v uint32) { binary.LittleEndian.PutUint32(b, v) }
func get16(b []byte) uint16    { return binary.LittleEndian.Uint16(b) }
func get32(b []byte) uint32    { return binary.LittleEndian.Uint32(b) }

func lowBit(mask uint32) uint {
	for i := uint(0); i < 32; i++ {
		if mask&(1<<i) != 0 {
			return i
		}
	}
	return 0
}

// Compile-time guard: windowRuntime must satisfy the compat input surface or
// every compat input read silently fails (see KeyDown above).
var _ compatInputRuntime = (*windowRuntime)(nil)
