//go:build linux

package kryon

// Native IBus input-method bridge for the pure-Go X11 window backend. IBus
// feeds the same composition queue as every other platform; text editing and
// rendering remain owned by TextField/TextArea.

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

const (
	ibusService      = "org.freedesktop.IBus"
	ibusRootPath     = "/org/freedesktop/IBus"
	ibusContextIface = "org.freedesktop.IBus.InputContext"
	ibusCapPreedit   = uint32(1 << 0)
	ibusCapFocus     = uint32(1 << 3)
	ibusCallTimeout  = 750 * time.Millisecond
	ibusCloseTimeout = 100 * time.Millisecond
	ibusPendingLimit = 16
)

type ibusInputMethod struct {
	conn *dbusConn
	path string

	mu        sync.Mutex
	pending   []KryTextCompositionEvent
	forwarded []ibusForwardedKey
	preedit   bool
	focused   bool
	cursor    [4]int32
	hasCursor bool
}

type ibusForwardedKey struct {
	keyval uint32
	state  uint32
}

func openIBusInputMethod() (*ibusInputMethod, error) {
	if !ibusRequested() {
		return nil, errors.New("ibus: not selected by desktop environment")
	}
	addr, err := ibusAddress()
	if err != nil {
		return nil, err
	}
	conn, err := dbusOpen(addr)
	if err != nil {
		return nil, err
	}
	fail := func(err error) (*ibusInputMethod, error) {
		conn.close()
		return nil, err
	}
	reply, err := conn.call(ibusService, ibusRootPath, ibusService,
		"CreateInputContext", "s", []any{"kryon"}, ibusCallTimeout)
	if err != nil {
		return fail(err)
	}
	values, err := dbusDecodeAll("o", reply.body)
	if err != nil {
		return fail(fmt.Errorf("ibus: invalid input context reply: %w", err))
	}
	if len(values) != 1 {
		return fail(errors.New("ibus: invalid input context reply"))
	}
	path, ok := values[0].(string)
	if !ok || path == "" {
		return fail(errors.New("ibus: empty input context path"))
	}
	im := &ibusInputMethod{conn: conn, path: path}
	rule := "type='signal',path='" + path + "',interface='" + ibusContextIface + "'"
	if err := conn.onSignal(rule, im.handleSignal); err != nil {
		return fail(err)
	}
	if _, err := conn.call(ibusService, path, ibusContextIface,
		"SetCapabilities", "u", []any{ibusCapPreedit | ibusCapFocus}, ibusCallTimeout); err != nil {
		return fail(err)
	}
	return im, nil
}

func ibusRequested() bool {
	if os.Getenv("IBUS_ADDRESS") != "" {
		return true
	}
	for _, name := range []string{"GTK_IM_MODULE", "QT_IM_MODULE"} {
		if strings.EqualFold(strings.TrimSpace(os.Getenv(name)), "ibus") {
			return true
		}
	}
	return strings.Contains(strings.ToLower(os.Getenv("XMODIFIERS")), "@im=ibus")
}

func ibusAddress() (string, error) {
	if addr := strings.TrimSpace(os.Getenv("IBUS_ADDRESS")); addr != "" {
		return addr, nil
	}
	config, err := os.UserConfigDir()
	if err != nil {
		return "", fmt.Errorf("ibus: config directory: %w", err)
	}
	dir := filepath.Join(config, "ibus", "bus")
	entries, err := os.ReadDir(dir)
	if err != nil {
		return "", fmt.Errorf("ibus: address directory: %w", err)
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].Name() < entries[j].Name() })
	suffix := ibusDisplaySuffix(os.Getenv("DISPLAY"))
	var fallback string
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		path := filepath.Join(dir, entry.Name())
		data, readErr := os.ReadFile(path)
		if readErr != nil || len(data) > 4096 {
			continue
		}
		addr := ibusAddressFromFile(string(data))
		if addr == "" {
			continue
		}
		if suffix != "" && strings.HasSuffix(entry.Name(), suffix) {
			return addr, nil
		}
		if fallback == "" {
			fallback = addr
		}
	}
	if fallback != "" {
		return fallback, nil
	}
	return "", errors.New("ibus: no address found")
}

func ibusDisplaySuffix(display string) string {
	display = strings.TrimSpace(display)
	colon := strings.LastIndexByte(display, ':')
	if colon < 0 {
		return ""
	}
	host := display[:colon]
	number := display[colon+1:]
	if dot := strings.IndexByte(number, '.'); dot >= 0 {
		number = number[:dot]
	}
	if _, err := strconv.Atoi(number); err != nil {
		return ""
	}
	if host == "" || host == "unix" || host == "localhost" {
		host = "unix"
	}
	return "-" + host + "-" + number
}

func ibusAddressFromFile(contents string) string {
	for _, line := range strings.Split(contents, "\n") {
		line = strings.TrimSpace(line)
		if !strings.HasPrefix(line, "IBUS_ADDRESS=") {
			continue
		}
		return strings.Trim(strings.TrimSpace(strings.TrimPrefix(line, "IBUS_ADDRESS=")), "\"'")
	}
	return ""
}

func (im *ibusInputMethod) focusIn() {
	im.mu.Lock()
	if im.focused {
		im.mu.Unlock()
		return
	}
	im.mu.Unlock()
	if _, err := im.conn.call(ibusService, im.path, ibusContextIface, "FocusIn", "", nil, ibusCallTimeout); err != nil {
		return
	}
	im.mu.Lock()
	im.focused = true
	im.mu.Unlock()
}

func (im *ibusInputMethod) focusOut() {
	im.mu.Lock()
	if !im.focused {
		im.mu.Unlock()
		return
	}
	im.focused = false
	im.hasCursor = false
	im.mu.Unlock()
	_, _ = im.conn.call(ibusService, im.path, ibusContextIface, "Reset", "", nil, ibusCloseTimeout)
	_, _ = im.conn.call(ibusService, im.path, ibusContextIface, "FocusOut", "", nil, ibusCloseTimeout)
}

func (im *ibusInputMethod) close() {
	if im == nil || im.conn == nil {
		return
	}
	im.focusOut()
	im.conn.close()
}

func (im *ibusInputMethod) processKey(keyval uint32, keycode uint8, state uint32) bool {
	if im == nil || keyval == 0 {
		return false
	}
	im.focusIn()
	im.mu.Lock()
	focused := im.focused
	im.mu.Unlock()
	if !focused {
		return false
	}
	xkbCode := uint32(0)
	if keycode >= 8 {
		xkbCode = uint32(keycode - 8)
	}
	reply, err := im.conn.call(ibusService, im.path, ibusContextIface,
		"ProcessKeyEvent", "uuu", []any{keyval, xkbCode, state}, ibusCallTimeout)
	if err != nil {
		return false
	}
	values, err := dbusDecodeAll("b", reply.body)
	return err == nil && len(values) == 1 && values[0] == true
}

func (im *ibusInputMethod) setCursor(x, y, width, height int32) {
	if im == nil {
		return
	}
	next := [4]int32{x, y, width, height}
	im.mu.Lock()
	if !im.focused || im.hasCursor && im.cursor == next {
		im.mu.Unlock()
		return
	}
	im.cursor = next
	im.hasCursor = true
	im.mu.Unlock()
	_, _ = im.conn.call(ibusService, im.path, ibusContextIface,
		"SetCursorLocationRelative", "iiii", []any{x, y, width, height}, ibusCallTimeout)
}

func (im *ibusInputMethod) handleSignal(message *dbusMessage) {
	if message.path != im.path || message.iface != ibusContextIface {
		return
	}
	switch message.member {
	case "CommitText":
		values, err := dbusDecodeAll("v", message.body)
		if err == nil && len(values) == 1 {
			if text, ok := ibusText(values[0]); ok {
				im.enqueue(KryTextCompositionEvent{Phase: KRY_TEXT_COMPOSITION_COMMIT, Text: text})
				im.mu.Lock()
				im.preedit = false
				im.mu.Unlock()
			}
		}
	case "UpdatePreeditText", "UpdatePreeditTextWithMode":
		sig := "vub"
		if message.member == "UpdatePreeditTextWithMode" {
			sig = "vubu"
		}
		values, err := dbusDecodeAll(sig, message.body)
		if err != nil || len(values) < 3 {
			return
		}
		text, ok := ibusText(values[0])
		cursor, cursorOK := values[1].(uint32)
		visible, visibleOK := values[2].(bool)
		if !ok || !cursorOK || !visibleOK {
			return
		}
		if !visible || text == "" {
			im.cancelPreedit()
			return
		}
		im.mu.Lock()
		phase := KRY_TEXT_COMPOSITION_UPDATE
		if !im.preedit {
			phase = KRY_TEXT_COMPOSITION_START
			im.preedit = true
		}
		im.enqueueLocked(KryTextCompositionEvent{
			Phase: phase, Text: text, Cursor: ibusCursorByteOffset(text, cursor),
		})
		im.mu.Unlock()
	case "HidePreeditText":
		im.cancelPreedit()
	case "ForwardKeyEvent":
		values, err := dbusDecodeAll("uuu", message.body)
		if err != nil || len(values) != 3 {
			return
		}
		keyval, keyOK := values[0].(uint32)
		state, stateOK := values[2].(uint32)
		if !keyOK || !stateOK {
			return
		}
		im.mu.Lock()
		if len(im.forwarded) == ibusPendingLimit {
			copy(im.forwarded, im.forwarded[1:])
			im.forwarded = im.forwarded[:len(im.forwarded)-1]
		}
		im.forwarded = append(im.forwarded, ibusForwardedKey{keyval: keyval, state: state})
		im.mu.Unlock()
	}
}

func ibusText(value any) (string, bool) {
	variant, ok := value.(dbusVariant)
	if !ok {
		return "", false
	}
	object, ok := variant.Val.(dbusStruct)
	if !ok || len(object.Val) < 3 {
		return "", false
	}
	typeName, typeOK := object.Val[0].(string)
	text, textOK := object.Val[2].(string)
	return text, typeOK && typeName == "IBusText" && textOK && utf8.ValidString(text)
}

func ibusCursorByteOffset(text string, runeOffset uint32) int32 {
	if runeOffset == 0 {
		return 0
	}
	count := uint32(0)
	for offset := range text {
		if count == runeOffset {
			return int32(offset)
		}
		count++
	}
	return int32(len(text))
}

func (im *ibusInputMethod) cancelPreedit() {
	im.mu.Lock()
	defer im.mu.Unlock()
	if !im.preedit {
		return
	}
	im.preedit = false
	im.enqueueLocked(KryTextCompositionEvent{Phase: KRY_TEXT_COMPOSITION_CANCEL})
}

func (im *ibusInputMethod) enqueue(event KryTextCompositionEvent) {
	im.mu.Lock()
	im.enqueueLocked(event)
	im.mu.Unlock()
}

func (im *ibusInputMethod) enqueueLocked(event KryTextCompositionEvent) {
	if event.Phase == KRY_TEXT_COMPOSITION_UPDATE && len(im.pending) > 0 &&
		im.pending[len(im.pending)-1].Phase == KRY_TEXT_COMPOSITION_UPDATE {
		im.pending[len(im.pending)-1] = event
		return
	}
	if len(im.pending) == ibusPendingLimit {
		copy(im.pending, im.pending[1:])
		im.pending = im.pending[:len(im.pending)-1]
	}
	im.pending = append(im.pending, event)
}

func (im *ibusInputMethod) drain() []KryTextCompositionEvent {
	im.mu.Lock()
	defer im.mu.Unlock()
	events := append([]KryTextCompositionEvent(nil), im.pending...)
	clear(im.pending)
	im.pending = im.pending[:0]
	return events
}

func (im *ibusInputMethod) drainForwarded() []ibusForwardedKey {
	im.mu.Lock()
	defer im.mu.Unlock()
	keys := append([]ibusForwardedKey(nil), im.forwarded...)
	clear(im.forwarded)
	im.forwarded = im.forwarded[:0]
	return keys
}
