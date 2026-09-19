//go:build linux

package kryon

import (
	"bufio"
	"context"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/godbus/dbus/v5"
)

func accessibilityTestBus(t *testing.T) (string, *dbus.Conn) {
	t.Helper()
	command := exec.Command("dbus-daemon", "--session", "--nofork", "--print-address=1")
	pipe, err := command.StdoutPipe()
	if err != nil {
		t.Fatal(err)
	}
	if err = command.Start(); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { command.Process.Kill(); command.Wait() })
	address, err := bufio.NewReader(pipe).ReadString('\n')
	if err != nil {
		t.Fatal(err)
	}
	address = strings.TrimSpace(address)
	registry, err := dbus.Connect(address)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { registry.Close() })
	_, err = registry.RequestName("org.a11y.atspi.Registry", dbus.NameFlagDoNotQueue)
	if err != nil {
		t.Fatal(err)
	}
	err = registry.ExportMethodTable(map[string]any{
		"Embed": func(reference accessibleReference) (accessibleReference, *dbus.Error) {
			return accessibleReference{"org.a11y.atspi.Registry", atspiRoot}, nil
		},
	}, atspiRoot, atspiPrefix+"Socket")
	if err != nil {
		t.Fatal(err)
	}
	return address, registry
}

func TestAccessibilityDBusNativeC(t *testing.T) {
	binary := os.Getenv("KRYON_ACCESSIBILITY_C_FIXTURE")
	if binary == "" {
		t.Skip("native C fixture is run by accessibility-dbus-test")
	}
	address, client := accessibilityTestBus(t)
	registered := make(chan accessibleReference, 1)
	if err := client.ExportMethodTable(map[string]any{
		"Embed": func(reference accessibleReference) (accessibleReference, *dbus.Error) {
			registered <- reference
			return accessibleReference{"org.a11y.atspi.Registry", atspiRoot}, nil
		},
	}, atspiRoot, atspiPrefix+"Socket"); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	command := exec.CommandContext(ctx, binary)
	command.Env = append(os.Environ(), "AT_SPI_BUS_ADDRESS="+address, "NO_AT_BRIDGE=0", "KRYON_ACCESSIBILITY=1")
	command.Stderr = os.Stderr
	pipe, err := command.StdoutPipe()
	if err != nil {
		t.Fatal(err)
	}
	if err := command.Start(); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { command.Process.Kill(); command.Wait() })
	reader := bufio.NewScanner(pipe)
	if !reader.Scan() || reader.Text() != "READY" {
		t.Fatalf("native bridge did not start: %s %v", reader.Text(), reader.Err())
	}
	var reference accessibleReference
	select {
	case reference = <-registered:
	case <-ctx.Done():
		t.Fatal("native bridge did not register")
	}
	bus := &accessibilityBus{name: reference.Bus}
	var children []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&children)
	if len(children) != 3 {
		t.Fatalf("native children = %v", children)
	}
	button, field, password := children[0].Path, children[1].Path, children[2].Path
	var cache []accessibleCacheItem
	if err := accessibilityCall(t, client, bus, atspiCache, atspiPrefix+"Cache.GetItems").Store(&cache); err != nil || len(cache) != 5 {
		t.Fatalf("native cache = %+v: %v", cache, err)
	}
	var rangeText string
	var rangeStart, rangeEnd int32
	accessibilityCall(t, client, bus, field, atspiPrefix+"Text.GetStringAtOffset", int32(2), uint32(0)).Store(&rangeText, &rangeStart, &rangeEnd)
	if rangeText != "e\u0301" || rangeStart != 1 || rangeEnd != 3 {
		t.Fatalf("native grapheme range = %q %d:%d", rangeText, rangeStart, rangeEnd)
	}
	var text string
	accessibilityCall(t, client, bus, password, atspiPrefix+"Text.GetText", int32(0), int32(-1)).Store(&text)
	if text != "" {
		t.Fatal("native password leaked")
	}
	var count dbus.Variant
	accessibilityCall(t, client, bus, field, "org.freedesktop.DBus.Properties.Get", atspiPrefix+"Text", "CharacterCount").Store(&count)
	if count.Value() != int32(4) {
		t.Fatalf("native scalar count = %v", count)
	}
	var accepted bool
	accessibilityCall(t, client, bus, field, atspiPrefix+"Text.SetSelection", int32(0), int32(2), int32(4)).Store(&accepted)
	if !accepted {
		t.Fatal("native selection rejected")
	}
	var start, end int32
	selectionDeadline := time.Now().Add(time.Second)
	for time.Now().Before(selectionDeadline) {
		err := client.Object(bus.name, field).CallWithContext(ctx,
			atspiPrefix+"Text.GetSelection", 0, int32(0)).Store(&start, &end)
		if err == nil && start == 1 && end == 4 {
			break
		}
		time.Sleep(time.Millisecond)
	}
	if start != 1 || end != 4 {
		t.Fatalf("native grapheme selection = %d:%d", start, end)
	}
	accessibilityCall(t, client, bus, button, atspiPrefix+"Action.DoAction", int32(0)).Store(&accepted)
	if !accepted {
		t.Fatal("native activation rejected")
	}
	accessibilityCall(t, client, bus, field, atspiPrefix+"EditableText.SetTextContents", "\u754c").Store(&accepted)
	if !accepted {
		t.Fatal("native replacement rejected")
	}
	if !reader.Scan() || reader.Text() != "APPLIED" {
		t.Fatalf("native application did not receive actions: %s", reader.Text())
	}
	if err := command.Wait(); err != nil {
		t.Fatalf("native shutdown failed: %v", err)
	}
}

func TestAccessibilityDBusNativeWindow(t *testing.T) {
	if os.Getenv("KRYON_ACCESSIBILITY_TEST_WINDOW") == "" {
		t.Skip("requires an X11 display and explicit opt-in")
	}
	address, client := accessibilityTestBus(t)
	t.Setenv("AT_SPI_BUS_ADDRESS", address)
	t.Setenv("DBUS_SESSION_BUS_ADDRESS", address)
	t.Setenv("KRYON_ACCESSIBILITY", "1")
	t.Setenv("NO_AT_BRIDGE", "0")
	rt, err := openWindowRuntime(AppConfig{Title: "Kryon Accessibility Window", Width: 200, Height: 100})
	if err != nil {
		t.Fatal(err)
	}
	defer rt.Close()
	window := rt.(*windowRuntime)
	clicks := 0
	draw := func() {
		rt.BeginFrame()
		if rt.Button(ButtonProps{ID: 11, Label: "Run", Bounds: NewRectangle(10, 10, 100, 30)}) {
			clicks++
		}
		rt.EndFrame()
	}
	deadline := time.Now().Add(5 * time.Second)
	for window.accessibility.current() == nil && time.Now().Before(deadline) {
		draw()
		time.Sleep(time.Millisecond)
	}
	bus := window.accessibility.current()
	if bus == nil {
		t.Fatal("window did not register with accessibility bus")
	}
	draw()
	var items []accessibleCacheItem
	if err := accessibilityCall(t, client, bus, atspiCache, atspiPrefix+"Cache.GetItems").Store(&items); err != nil || len(items) != 3 {
		t.Fatalf("window cache = %+v: %v", items, err)
	}
	var accepted bool
	accessibilityCall(t, client, bus, items[2].Object.Path, atspiPrefix+"Action.DoAction", int32(0)).Store(&accepted)
	if !accepted || clicks != 0 {
		t.Fatal("window action rejected or delivered off the UI thread")
	}
	draw()
	if clicks != 1 {
		t.Fatal("window action was not delivered at frame start")
	}
}

func accessibilityCall(t *testing.T, client *dbus.Conn, bus *accessibilityBus, path dbus.ObjectPath, method string, args ...any) *dbus.Call {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	call := client.Object(bus.name, path).CallWithContext(ctx, method, 0, args...)
	if call.Err != nil {
		t.Fatalf("%s: %v", method, call.Err)
	}
	return call
}

func TestAccessibilityDBusRoundTrip(t *testing.T) {
	address, client := accessibilityTestBus(t)
	bus, err := openAccessibilityBus(address, "Accessible App")
	if err != nil {
		t.Fatal(err)
	}
	defer bus.close()
	r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
	defer r.Close()
	text := make([]byte, 32)
	copy(text, "Ae\u0301Z")
	cursor := int32(5)
	clicks := 0
	visible := true
	draw := func() {
		bus.drain(r)
		r.BeginFrame()
		if r.Button(ButtonProps{ID: 11, Label: "Run", Bounds: NewRectangle(10, 5, 60, 25)}) {
			clicks++
		}
		if visible {
			r.TextField(TextFieldProps{FocusID: 12, Text: text, CursorPosition: &cursor})
		}
		r.EndFrame()
		bus.publish(r.GetAccessibilitySnapshot(), NewRectangle(30, 40, 200, 100))
	}
	draw()
	var children []accessibleReference
	if err := accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&children); err != nil {
		t.Fatal(err)
	}
	if len(children) != 2 {
		t.Fatalf("children = %v", children)
	}
	button, field := children[0].Path, children[1].Path
	var value dbus.Variant
	accessibilityCall(t, client, bus, field, "org.freedesktop.DBus.Properties.Get", atspiPrefix+"Text", "CharacterCount").Store(&value)
	if value.Value() != int32(4) {
		t.Fatalf("character count is not scalar-based: %v", value)
	}
	var selected bool
	accessibilityCall(t, client, bus, field, atspiPrefix+"Text.SetSelection", int32(0), int32(2), int32(4)).Store(&selected)
	if !selected || cursor != 5 {
		t.Fatal("selection rejected or applied from D-Bus goroutine")
	}
	draw()
	node := accessibilityNode(t, r, 12)
	if node.SelectionAnchor != 1 || node.SelectionCursor != 5 {
		t.Fatalf("selection split grapheme: %+v", node)
	}
	var start, end int32
	accessibilityCall(t, client, bus, field, atspiPrefix+"Text.GetSelection", int32(0)).Store(&start, &end)
	if start != 1 || end != 4 {
		t.Fatalf("wire selection = %d:%d", start, end)
	}
	var rect accessibleRectangle
	accessibilityCall(t, client, bus, button, atspiPrefix+"Component.GetExtents", uint32(0)).Store(&rect)
	if rect.X != 40 || rect.Y != 45 {
		t.Fatalf("screen coordinates = %+v", rect)
	}
	var accepted bool
	accessibilityCall(t, client, bus, button, atspiPrefix+"Action.DoAction", int32(0)).Store(&accepted)
	if !accepted || clicks != 0 {
		t.Fatal("activation rejected or applied outside UI thread")
	}
	draw()
	if clicks != 1 {
		t.Fatal("activation not delivered")
	}
	accessibilityCall(t, client, bus, field, atspiPrefix+"EditableText.SetTextContents", "\u754c").Store(&accepted)
	if !accepted {
		t.Fatal("value request rejected")
	}
	draw()
	if string(text[:zeroIndex(text)]) != "\u754c" {
		t.Fatal("value not applied")
	}
	accessibilityCall(t, client, bus, field, atspiPrefix+"EditableText.InsertText", int32(1), "\u65e5a", int32(3)).Store(&accepted)
	if !accepted {
		t.Fatal("byte-limited insertion rejected")
	}
	draw()
	if string(text[:zeroIndex(text)]) != "\u754c\u65e5" {
		t.Fatal("insertion length was not measured in bytes")
	}
	accessibilityCall(t, client, bus, field, atspiPrefix+"EditableText.InsertText", int32(0), "\u65e5", int32(1)).Store(&accepted)
	if accepted {
		t.Fatal("insertion accepted a partial UTF-8 sequence")
	}
	accessibilityCall(t, client, bus, field, atspiPrefix+"EditableText.DeleteText", int32(1), int32(2)).Store(&accepted)
	if !accepted {
		t.Fatal("scalar-offset deletion rejected")
	}
	draw()
	if string(text[:zeroIndex(text)]) != "\u754c" {
		t.Fatal("scalar-offset deletion removed the wrong range")
	}
	var introspection string
	accessibilityCall(t, client, bus, field, "org.freedesktop.DBus.Introspectable.Introspect").Store(&introspection)
	if !strings.Contains(introspection, "EditableText") || !strings.Contains(introspection, "CharacterCount") {
		t.Fatal("incomplete introspection")
	}
	visible = false
	draw()
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	call := client.Object(bus.name, field).CallWithContext(ctx, atspiPrefix+"Text.GetText", 0, int32(0), int32(-1))
	if call.Err == nil {
		t.Fatal("retired object still responds")
	}
	visible = true
	draw()
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&children)
	if children[0].Path != button || children[1].Path == field {
		t.Fatal("stable and retired identities were conflated")
	}
}

func TestAccessibilityDBusPasswordAndValidation(t *testing.T) {
	address, client := accessibilityTestBus(t)
	bus, err := openAccessibilityBus(address, "Secure")
	if err != nil {
		t.Fatal(err)
	}
	defer bus.close()
	nodes := []AccessibilityNode{{Role: "textbox", FocusID: 11, Secure: true, Value: "secret",
		SelectionAnchor: 1, SelectionCursor: 6, Generation: 1,
		Actions: uint32(AccessibilityActionFocus | AccessibilityActionSetValue | AccessibilityActionSetSelection)}}
	bus.publish(nodes, Rectangle{})
	var children []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&children)
	path := children[0].Path
	var text string
	accessibilityCall(t, client, bus, path, atspiPrefix+"Text.GetText", int32(0), int32(-1)).Store(&text)
	if text != "" {
		t.Fatal("password leaked")
	}
	var accepted bool
	accessibilityCall(t, client, bus, path, atspiPrefix+"Text.SetCaretOffset", int32(1)).Store(&accepted)
	if accepted {
		t.Fatal("password offsets exposed through selection")
	}
	accessibilityCall(t, client, bus, path, atspiPrefix+"EditableText.SetTextContents", "replacement").Store(&accepted)
	if !accepted {
		t.Fatal("secure replacement rejected")
	}
	bus.mu.Lock()
	payload := bus.pending[0].value
	bus.mu.Unlock()
	accessibilityCall(t, client, bus, path, atspiPrefix+"EditableText.SetTextContents", "bad\x01value").Store(&accepted)
	if accepted {
		t.Fatal("control character accepted")
	}
	bus.close()
	for _, value := range payload {
		if value != 0 {
			t.Fatal("closed bridge retained password payload")
		}
	}
}

func TestAccessibilityDBusUnicodeRanges(t *testing.T) {
	for _, tc := range []struct {
		text        string
		offset      int32
		granularity uint32
		want        string
		start, end  int32
	}{
		{"Ae\u0301Z", 2, 0, "e\u0301", 1, 3},
		{"hello world", 7, 1, "world", 6, 11},
		{"a\r\nb", 3, 3, "b", 3, 4},
		{"text", 4, 0, "", 4, 4},
	} {
		text, start, end, err := accessibleStringAtOffset(tc.text, tc.offset, tc.granularity)
		if err != nil || text != tc.want || start != tc.start || end != tc.end {
			t.Fatalf("range = %q %d:%d %v, want %+v", text, start, end, err, tc)
		}
	}
}
