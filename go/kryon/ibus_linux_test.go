//go:build linux

package kryon

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestIBusAddressUsesCurrentDisplay(t *testing.T) {
	config := t.TempDir()
	dir := filepath.Join(config, "ibus", "bus")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "machine-unix-1"),
		[]byte("IBUS_ADDRESS=unix:path=/wrong\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "machine-unix-2"),
		[]byte("# generated\nIBUS_ADDRESS='unix:path=/right,guid=abc'\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("IBUS_ADDRESS", "")
	t.Setenv("XDG_CONFIG_HOME", config)
	t.Setenv("DISPLAY", ":2.0")
	address, err := ibusAddress()
	if err != nil {
		t.Fatal(err)
	}
	if address != "unix:path=/right,guid=abc" {
		t.Fatalf("address = %q", address)
	}
}

func TestIBusInputMethodKeyAndCompositionProtocol(t *testing.T) {
	fb := newFakeBus(t)
	defer fb.close()
	t.Setenv("IBUS_ADDRESS", os.Getenv("DBUS_SESSION_BUS_ADDRESS"))
	seen := make(chan *dbusMessage, 32)
	go fb.run(seen)

	const contextPath = "/org/freedesktop/IBus/InputContext_7"
	fb.handler = func(message *dbusMessage, reply func(string, []any)) {
		switch message.member {
		case "CreateInputContext":
			reply("o", []any{contextPath})
		case "ProcessKeyEvent":
			reply("b", []any{true})
		default:
			reply("", nil)
		}
	}
	im, err := openIBusInputMethod()
	if err != nil {
		t.Fatal(err)
	}
	defer im.close()

	if !im.processKey(uint32('a'), 38, x11ShiftMask) {
		t.Fatal("ProcessKeyEvent was not handled")
	}
	deadline := time.After(3 * time.Second)
	for {
		select {
		case message := <-seen:
			if message.member != "ProcessKeyEvent" {
				continue
			}
			values, decodeErr := dbusDecodeAll("uuu", message.body)
			if decodeErr != nil {
				t.Fatal(decodeErr)
			}
			if values[0] != uint32('a') || values[1] != uint32(30) || values[2] != uint32(x11ShiftMask) {
				t.Fatalf("ProcessKeyEvent args = %#v", values)
			}
			goto keySeen
		case <-deadline:
			t.Fatal("ProcessKeyEvent did not reach IBus")
		}
	}

keySeen:
	serialized := dbusVariant{
		Sig: "(sa{sv}sv)",
		Val: dbusStruct{Sig: "(sa{sv}sv)", Val: []any{
			"IBusText", map[string]any{}, "é文", dbusVariant{Sig: "s", Val: ""},
		}},
	}
	fb.sendSignal(contextPath, ibusContextIface, "UpdatePreeditText", "vub",
		[]any{serialized, uint32(1), true})
	events := waitIBusEvents(t, im, 1)
	if events[0].Phase != KRY_TEXT_COMPOSITION_START || events[0].Text != "é文" || events[0].Cursor != 2 {
		t.Fatalf("preedit event = %+v", events[0])
	}

	fb.sendSignal(contextPath, ibusContextIface, "CommitText", "v", []any{serialized})
	events = waitIBusEvents(t, im, 1)
	if events[0].Phase != KRY_TEXT_COMPOSITION_COMMIT || events[0].Text != "é文" {
		t.Fatalf("commit event = %+v", events[0])
	}

	fb.sendSignal(contextPath, ibusContextIface, "ForwardKeyEvent", "uuu",
		[]any{uint32(0xff51), uint32(105), uint32(x11ShiftMask)})
	forwarded := waitIBusForwarded(t, im)
	if forwarded.keyval != 0xff51 || forwarded.state != x11ShiftMask {
		t.Fatalf("forwarded key = %+v", forwarded)
	}
}

func TestIBusLiveConnection(t *testing.T) {
	if os.Getenv("KRYON_LIVE_IBUS_TEST") == "" {
		t.Skip("set KRYON_LIVE_IBUS_TEST=1 to exercise the desktop IBus daemon")
	}
	im, err := openIBusInputMethod()
	if err != nil {
		t.Fatal(err)
	}
	im.focusIn()
	im.mu.Lock()
	focused := im.focused
	im.mu.Unlock()
	if !focused {
		t.Fatal("IBus input context did not accept FocusIn")
	}
	im.close()
}

func waitIBusEvents(t *testing.T, im *ibusInputMethod, count int) []KryTextCompositionEvent {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if events := im.drain(); len(events) >= count {
			return events
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatalf("timed out waiting for %d IBus event(s)", count)
	return nil
}

func waitIBusForwarded(t *testing.T, im *ibusInputMethod) ibusForwardedKey {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if keys := im.drainForwarded(); len(keys) > 0 {
			return keys[0]
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatal("timed out waiting for IBus forwarded key")
	return ibusForwardedKey{}
}
