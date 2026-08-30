//go:build linux

package kryon

// Hermetic DBus codec and client tests: everything runs against an
// in-process fake bus over a unix socket, never the user's session.

import (
	"bufio"
	"encoding/binary"
	"log"
	"net"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"
)

func TestDBusRoundTripScalars(t *testing.T) {
	cases := []struct {
		sig string
		val any
	}{
		{"y", byte(7)},
		{"b", true},
		{"i", int32(-42)},
		{"u", uint32(0xdeadbeef)},
		{"s", "Schöne Grüße"},
		{"g", "a{sv}"},
	}
	for _, c := range cases {
		e := &dbusEncoder{}
		if err := dbusAppendValue(e, c.sig, c.val); err != nil {
			t.Fatalf("%s: %v", c.sig, err)
		}
		got, err := dbusDecodeAll(c.sig, e.buf)
		if err != nil {
			t.Fatalf("%s decode: %v", c.sig, err)
		}
		if len(got) != 1 || got[0] != c.val {
			t.Fatalf("%s: got %#v want %#v", c.sig, got, c.val)
		}
	}
}

func TestDBusRoundTripComposites(t *testing.T) {
	sig := "susasa{sv}(iiay)v"
	dict := map[string]any{
		"urgency": dbusVariant{Sig: "y", Val: byte(2)},
		"name":    dbusVariant{Sig: "s", Val: "notify"},
	}
	want := []any{
		"app", uint32(5), "icon", []string{"open", "Open"}, dict,
		dbusStruct{Sig: "(iiay)", Val: []any{int32(2), int32(1), []byte{1, 2, 3, 4, 5, 6, 7, 8}}},
		dbusVariant{Sig: "u", Val: uint32(9)},
	}
	parts, _ := dbusSplitAll(sig)
	e := &dbusEncoder{}
	for i, p := range parts {
		if err := dbusAppendValue(e, p, want[i]); err != nil {
			t.Fatalf("%s: %v", p, err)
		}
	}
	got, err := dbusDecodeAll(sig, e.buf)
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != len(want) {
		t.Fatalf("got %d values want %d", len(got), len(want))
	}
	if got[0] != "app" || got[1] != uint32(5) || got[2] != "icon" {
		t.Fatalf("scalars: %#v", got[:3])
	}
	actions := got[3].([]string)
	if len(actions) != 2 || actions[0] != "open" || actions[1] != "Open" {
		t.Fatalf("actions: %#v", actions)
	}
	hints := got[4].(map[string]any)
	if hints["urgency"].(dbusVariant).Val != byte(2) {
		t.Fatalf("hints: %#v", hints)
	}
	pix := got[5].(dbusStruct)
	if pix.Val[0] != int32(2) || len(pix.Val[2].([]byte)) != 8 {
		t.Fatalf("pixmap: %#v", pix)
	}
	if got[6].(dbusVariant).Val != uint32(9) {
		t.Fatalf("variant: %#v", got[6])
	}
}

func TestDBusArrayElementAlignment(t *testing.T) {
	// a(i): u32 length, pad to 8, then structs on 8-byte boundaries
	// (1 struct = align8 + 4 data bytes).
	e := &dbusEncoder{}
	err := dbusAppendValue(e, "a(i)", []dbusStruct{
		{Sig: "(i)", Val: []any{int32(1)}},
		{Sig: "(i)", Val: []any{int32(2)}},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(e.buf) != 20 {
		t.Fatalf("a(i) frame = %d bytes, want 20: % x", len(e.buf), e.buf)
	}
	if bodyLen := binary.LittleEndian.Uint32(e.buf); bodyLen != 12 {
		t.Fatalf("a(i) body length = %d, want 12", bodyLen)
	}
	got, err := dbusDecodeAll("a(i)", e.buf)
	if err != nil {
		t.Fatal(err)
	}
	arr := got[0].([]dbusStruct)
	if len(arr) != 2 || arr[1].Val[0] != int32(2) {
		t.Fatalf("a(i): %#v", got[0])
	}
}

func TestDBusSplitComplete(t *testing.T) {
	cases := []struct{ in, head, rest string }{
		{"sus", "s", "us"},
		{"a{sv}i", "a{sv}", "i"},
		{"(ia{sv}av)u", "(ia{sv}av)", "u"},
		{"a(iiay)", "a(iiay)", ""},
		{"sa{sv}as", "s", "a{sv}as"},
	}
	for _, c := range cases {
		h, r, ok := dbusSplitComplete(c.in)
		if !ok || h != c.head || r != c.rest {
			t.Fatalf("split(%q) = %q,%q,%v want %q,%q", c.in, h, r, ok, c.head, c.rest)
		}
	}
}

// fakeBus is a just-capable bus daemon: EXTERNAL auth, Hello, AddMatch and
// RequestName are answered by the book; everything else goes to the test's
// handler, and sendSignal() injects signals back over the sockets.
type fakeBus struct {
	t       *testing.T
	ln      net.Listener
	conns   chan net.Conn
	handler func(m *dbusMessage, reply func(sig string, args []any))

	mu      sync.Mutex
	writers []chan []byte
}

func newFakeBus(t *testing.T) *fakeBus {
	path := filepath.Join(t.TempDir(), "bus")
	ln, err := net.Listen("unix", path)
	if err != nil {
		t.Fatal(err)
	}
	fb := &fakeBus{t: t, ln: ln, conns: make(chan net.Conn, 4)}
	go func() {
		for {
			c, err := ln.Accept()
			if err != nil {
				return
			}
			fb.conns <- c
		}
	}()
	t.Setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path="+path)
	return fb
}

func (fb *fakeBus) close() { fb.ln.Close() }

// run serves every accepted connection on the bus side of the protocol:
// EXTERNAL auth, Hello/AddMatch/RequestName answered by the book, anything
// else goes to the handler and every call lands in seen. Clients dial in
// from the tests via dbusSession().
func (fb *fakeBus) run(seen chan<- *dbusMessage) {
	for c := range fb.conns {
		go fb.serveConn(c, seen)
	}
}

func (fb *fakeBus) serveConn(c net.Conn, seen chan<- *dbusMessage) {
	r := bufio.NewReader(c)
	if _, err := r.ReadString('\n'); err != nil { // \0 + AUTH EXTERNAL …
		return
	}
	if _, err := c.Write([]byte("OK 0123456789abcdef\r\n")); err != nil {
		return
	}
	if _, err := r.ReadString('\n'); err != nil { // BEGIN
		return
	}
	serial := uint32(0)
	fb.writeInit(c)
	reply := func(m *dbusMessage, sig string, args []any) {
		e := &dbusEncoder{}
		parts, _ := dbusSplitAll(sig)
		for i, p := range parts {
			if err := dbusAppendValue(e, p, args[i]); err != nil {
				fb.t.Errorf("fake bus encode: %v", err)
				return
			}
		}
		serial++
		frame := frameBytes(dbusMsgReturn, [][3]any{{byte(5), "u", m.serial}}, serial, e.buf)
		if os.Getenv("DBUS_DEBUG") != "" {
			log.Printf("fakebus reply: %s -> % x", m.member, frame)
		}
		fb.write(frame)
	}
	for {
		var head [16]byte
		if err := readFullBuf(r, head[:]); err != nil {
			if os.Getenv("DBUS_DEBUG") != "" {
				log.Printf("fakebus: head read: %v", err)
			}
			return
		}
		m := &dbusMessage{typ: head[1]}
		if os.Getenv("DBUS_DEBUG") != "" {
			log.Printf("fakebus: head % x", head[:])
		}
		bodyLen := binary.LittleEndian.Uint32(head[4:8])
		m.serial = binary.LittleEndian.Uint32(head[8:12])
		n := binary.LittleEndian.Uint32(head[12:16]) // fields length lives in the fixed header
		fields := make([]byte, n)
		if err := readFullBuf(r, fields); err != nil {
			return
		}
		pad := (8 - (16+int(n))%8) % 8
		if pad > 0 {
			if err := readFullBuf(r, make([]byte, pad)); err != nil {
				return
			}
		}
		(&dbusConn{}).parseFields(m, fields)
		if bodyLen > 0 {
			m.body = make([]byte, bodyLen)
			if err := readFullBuf(r, m.body); err != nil {
				return
			}
		}
		if m.typ != dbusMsgMethodCall {
			continue
		}
		if os.Getenv("DBUS_DEBUG") != "" {
			log.Printf("fakebus read: type=%d dest=%s iface=%s member=%s sig=%s bodylen=%d", m.typ, m.destination, m.iface, m.member, m.sig, len(m.body))
		}
		switch m.member {
		case "Hello":
			reply(m, "s", []any{":1.99"})
		case "RequestName":
			reply(m, "u", []any{uint32(1)})
		case "AddMatch":
			reply(m, "", nil)
		default:
			if fb.handler != nil {
				fb.handler(m, func(sig string, args []any) { reply(m, sig, args) })
			} else {
				reply(m, "", nil)
			}
		}
		if seen != nil {
			seen <- m
		}
	}
}

// writeInit registers a writer channel for one client socket.
func (fb *fakeBus) writeInit(c net.Conn) chan []byte {
	ch := make(chan []byte, 32)
	fb.mu.Lock()
	fb.writers = append(fb.writers, ch)
	fb.mu.Unlock()
	go func() {
		for b := range ch {
			if os.Getenv("DBUS_DEBUG") != "" {
				log.Printf("fakebus writer: writing %d bytes", len(b))
			}
			if _, err := c.Write(b); err != nil {
				if os.Getenv("DBUS_DEBUG") != "" {
					log.Printf("fakebus writer: %v", err)
				}
				return
			}
		}
	}()
	return ch
}

// write fans a frame out to every connected client.
func (fb *fakeBus) write(b []byte) {
	fb.mu.Lock()
	defer fb.mu.Unlock()
	if os.Getenv("DBUS_DEBUG") != "" {
		log.Printf("fakebus write: %d writers", len(fb.writers))
	}
	for _, ch := range fb.writers {
		select {
		case ch <- b:
		default:
		}
	}
}

// sendSignal injects a signal frame back over the client socket.
func (fb *fakeBus) sendSignal(path, iface, member, sig string, args []any) {
	e := &dbusEncoder{}
	parts, _ := dbusSplitAll(sig)
	for i, p := range parts {
		if err := dbusAppendValue(e, p, args[i]); err != nil {
			fb.t.Errorf("signal encode: %v", err)
			return
		}
	}
	fb.write(frameBytes(dbusMsgSignal, [][3]any{
		{byte(1), "o", path}, {byte(2), "s", iface}, {byte(3), "s", member}, {byte(8), "g", sig},
	}, 1, e.buf))
}

// frameBytes renders a complete message frame.
func frameBytes(typ byte, fields [][3]any, serial uint32, body []byte) []byte {
	e := &dbusEncoder{}
	e.buf = append(e.buf, 'l', typ, 1, 1)
	e.buf = binary.LittleEndian.AppendUint32(e.buf, uint32(len(body)))
	e.buf = binary.LittleEndian.AppendUint32(e.buf, serial)
	f := &dbusEncoder{}
	for _, fd := range fields {
		f.align(8)
		f.buf = append(f.buf, fd[0].(byte))
		_ = dbusAppendValue(f, "v", dbusVariant{Sig: fd[1].(string), Val: fd[2]})
	}
	e.buf = binary.LittleEndian.AppendUint32(e.buf, uint32(len(f.buf)))
	e.buf = append(e.buf, f.buf...)
	e.align(8)
	return append(e.buf, body...)
}

func TestDBusSessionHandshakeAndCall(t *testing.T) {
	fb := newFakeBus(t)
	defer fb.close()
	seen := make(chan *dbusMessage, 16)
	go fb.run(seen)

	fb.handler = func(m *dbusMessage, reply func(string, []any)) {
		reply("u", []any{uint32(42)})
	}
	conn, err := dbusSession()
	if err != nil {
		t.Fatal(err)
	}
	reply, err := conn.call("org.test", "/t", "org.test.Iface", "Echo",
		"su", []any{"hello", uint32(7)}, 3*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	vals, err := dbusDecodeAll("u", reply.body)
	if err != nil || vals[0].(uint32) != 42 {
		t.Fatalf("echo reply: %#v %v", vals, err)
	}
	conn.close()
}

func TestSendNotificationActionEndToEnd(t *testing.T) {
	fb := newFakeBus(t)
	defer fb.close()
	seen := make(chan *dbusMessage, 16)
	go fb.run(seen)

	// The fake daemon answers Notify with id 7.
	fb.handler = func(m *dbusMessage, reply func(string, []any)) {
		if m.member == "Notify" {
			reply("u", []any{uint32(7)})
		}
	}

	notifyRT.mu.Lock()
	notifyRT.conn = nil // force a fresh dial against the fake bus
	notifyRT.mu.Unlock()
	SetNotificationAppName("dash-test")
	if !NotificationSupported() {
		t.Fatal("NotificationSupported = false against fake bus")
	}
	if !SendNotificationAction("CI failed", "build broke", "", 15000, 4, "Open", "https://x/y") {
		t.Fatal("SendNotificationAction returned false")
	}
	deadlineNotify := time.After(3 * time.Second)
	for {
		select {
		case m := <-seen:
			if m.member != "Notify" {
				continue
			}
			vals, err := dbusDecodeAll("susssasa{sv}i", m.body)
			if err != nil {
				t.Fatal(err)
			}
			if vals[0].(string) != "dash-test" || vals[3].(string) != "CI failed" {
				t.Fatalf("notify args: %#v", vals)
			}
			acts := vals[5].([]string)
			if len(acts) != 2 || acts[0] != "open" || acts[1] != "Open" {
				t.Fatalf("actions: %#v", acts)
			}
			goto gotNotify
		case <-deadlineNotify:
			t.Fatal("Notify never reached the bus")
		}
	}
gotNotify:
	if act, url := PollNotificationAction(); act != 0 || url != "" {
		t.Fatalf("premature poll: %d %q", act, url)
	}

	// The daemon fires ActionInvoked(7, "open") back at the client.
	fb.sendSignal("/org/freedesktop/Notifications", "org.freedesktop.Notifications",
		"ActionInvoked", "us", []any{uint32(7), "open"})
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if act, url := PollNotificationAction(); act != 0 {
			if act != 4 || url != "https://x/y" {
				t.Fatalf("polled %d %q", act, url)
			}
			return
		}
		time.Sleep(20 * time.Millisecond)
	}
	t.Fatal("action never arrived")
}

func TestDesktopTrayEndToEnd(t *testing.T) {
	fb := newFakeBus(t)
	defer fb.close()
	seen := make(chan *dbusMessage, 16)
	go fb.run(seen)

	registered := make(chan string, 1)
	fb.handler = func(m *dbusMessage, reply func(string, []any)) {
		switch {
		case m.iface == "org.kde.StatusNotifierWatcher" && m.member == "RegisterStatusNotifierItem":
			vals, err := dbusDecodeAll("s", m.body)
			if err != nil || len(vals) != 1 {
				t.Errorf("register args: %v", err)
				return
			}
			registered <- vals[0].(string)
			reply("", nil)
		default:
			reply("", nil)
		}
	}

	ok := InitDesktopTray(DesktopTraySpec{
		ID: "dash", Title: "dashboard", IconName: "dashboard",
		ActivateAction: 9,
		MenuItems: []DesktopTrayMenuItem{
			{Kind: DesktopTrayMenuAction, Label: "Show", Action: 1, Enabled: true},
			{Kind: DesktopTrayMenuSeparator},
			{Kind: DesktopTrayMenuAction, Label: "Quit", Action: 3, Enabled: true},
		},
	})
	if !ok {
		t.Fatal("InitDesktopTray failed against fake bus")
	}
	defer ShutdownDesktopTray()
	select {
	case name := <-registered:
		if name != "/StatusNotifierItem" {
			t.Fatalf("registered as %q, want the object path", name)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("never registered with the watcher")
	}

	// The tray host opens the menu: GetLayout on /MenuBar.
	fb.call(t, menuObjectPath, menuIface, "GetLayout", "iias",
		[]any{int32(0), int32(-1), []string{}}, "u(ia{sv}av)")
	// A menu pick: Event(id=1, "clicked", v{}, u0).
	fb.call(t, menuObjectPath, menuIface, "Event", "isvu",
		[]any{int32(1), "clicked", dbusVariant{Sig: "s", Val: ""}, uint32(0)}, "")
	// A plain icon click delivers the activate action.
	fb.call(t, sniObjectPath, sniIface, "Activate", "ii",
		[]any{int32(0), int32(0)}, "")

	deadline := time.After(3 * time.Second)
	for want := []int{1, 9}; len(want) > 0; {
		select {
		case got := <-trayRT.actionCh:
			if got != want[0] {
				t.Fatalf("action %d, want %d", got, want[0])
			}
			want = want[1:]
		case <-deadline:
			t.Fatalf("missing actions, still want %v", want)
		}
	}
}

// call sends a method call INTO the client (as the tray host would) and
// waits for the reply to drain; the served methods reply fire-and-forget.
func (fb *fakeBus) call(t *testing.T, path, iface, member, inSig string, args []any, outSig string) {
	e := &dbusEncoder{}
	parts, _ := dbusSplitAll(inSig)
	for i, p := range parts {
		if err := dbusAppendValue(e, p, args[i]); err != nil {
			t.Fatalf("call encode: %v", err)
		}
	}
	fb.write(frameBytes(dbusMsgMethodCall, [][3]any{
		{byte(1), "o", path}, {byte(2), "s", iface}, {byte(3), "s", member},
		{byte(6), "s", ":1.1"}, {byte(8), "g", inSig},
	}, 900, e.buf))
	time.Sleep(150 * time.Millisecond) // let the client's read loop dispatch
}
