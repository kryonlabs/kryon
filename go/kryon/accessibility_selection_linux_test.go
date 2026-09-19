//go:build linux

package kryon

import (
	"bufio"
	"context"
	"os"
	"os/exec"
	"testing"
	"time"

	"github.com/godbus/dbus/v5"
)

func TestAccessibilityDBusSelection(t *testing.T) {
	address, client := accessibilityTestBus(t)
	bus, err := openAccessibilityBus(address, "Selection")
	if err != nil {
		t.Fatal(err)
	}
	defer bus.close()
	r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
	defer r.Close()
	selected := []int32{0, 0, 0}
	props := ListBoxProps{ID: 91, Bounds: NewRectangle(0, 0, 100, 80),
		Items: []string{"A", "B", "C"}, ItemKeys: []int32{1, 2, 3}, Selected: selected, RowHeight: 20}
	draw := func() {
		bus.drain(r)
		r.BeginFrame()
		r.ListBox(props)
		r.EndFrame()
		bus.publish(r.GetAccessibilitySnapshot(), NewRectangle(0, 0, 200, 100))
	}
	draw()
	var roots, children []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&roots)
	if len(roots) != 1 {
		t.Fatalf("list roots = %v", roots)
	}
	list := roots[0].Path
	accessibilityCall(t, client, bus, list, atspiPrefix+"Accessible.GetChildren").Store(&children)
	if len(children) != 3 {
		t.Fatalf("options = %v", children)
	}
	var accepted bool
	for _, index := range []int32{0, 2} {
		accessibilityCall(t, client, bus, list, atspiPrefix+"Selection.SelectChild", index).Store(&accepted)
		if !accepted {
			t.Fatal("selection rejected")
		}
	}
	if selected[0] != 0 || selected[2] != 0 {
		t.Fatal("selection was applied off the UI thread")
	}
	draw()
	var count dbus.Variant
	accessibilityCall(t, client, bus, list, "org.freedesktop.DBus.Properties.Get", atspiPrefix+"Selection", "NSelectedChildren").Store(&count)
	if count.Value() != int32(2) {
		t.Fatalf("selected count = %v", count)
	}
	var child accessibleReference
	accessibilityCall(t, client, bus, list, atspiPrefix+"Selection.GetSelectedChild", int32(1)).Store(&child)
	if child != children[2] {
		t.Fatal("selected-child index was confused with row index")
	}
	accessibilityCall(t, client, bus, list, atspiPrefix+"Selection.DeselectSelectedChild", int32(1)).Store(&accepted)
	if !accepted {
		t.Fatal("deselection rejected")
	}
	draw()
	if selected[0] != 1 || selected[2] != 0 {
		t.Fatal("deselected the wrong row")
	}
	props.Items[0], props.Items[2] = props.Items[2], props.Items[0]
	props.ItemKeys[0], props.ItemKeys[2] = props.ItemKeys[2], props.ItemKeys[0]
	draw()
	var reordered []accessibleReference
	accessibilityCall(t, client, bus, list, atspiPrefix+"Accessible.GetChildren").Store(&reordered)
	if reordered[0] != children[2] || reordered[2] != children[0] {
		t.Fatal("keyed options lost identity across reorder")
	}
}

func TestAccessibilityDBusNativeCSelection(t *testing.T) {
	binary := os.Getenv("KRYON_ACCESSIBILITY_C_FIXTURE")
	if binary == "" {
		t.Skip("requires the C fixture")
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
	command.Env = append(os.Environ(), "AT_SPI_BUS_ADDRESS="+address, "NO_AT_BRIDGE=0", "KRYON_ACCESSIBILITY=1", "KRYON_ACCESSIBILITY_TEST_LIST=1")
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
		t.Fatalf("C list fixture did not start: %s", reader.Text())
	}
	var application accessibleReference
	select {
	case application = <-registered:
	case <-ctx.Done():
		t.Fatal("C list fixture did not register")
	}
	bus := &accessibilityBus{name: application.Bus}
	var lists []accessibleReference
	accessibilityCall(t, client, bus, atspiWindow, atspiPrefix+"Accessible.GetChildren").Store(&lists)
	if len(lists) != 2 {
		t.Fatalf("C lists = %v", lists)
	}
	var accepted bool
	accessibilityCall(t, client, bus, lists[0].Path, atspiPrefix+"Selection.SelectChild", int32(2)).Store(&accepted)
	if !accepted {
		t.Fatal("single selection rejected")
	}
	accessibilityCall(t, client, bus, lists[1].Path, atspiPrefix+"Selection.SelectChild", int32(0)).Store(&accepted)
	if !accepted {
		t.Fatal("multi selection rejected")
	}
	var selected dbus.Variant
	for deadline := time.Now().Add(time.Second); time.Now().Before(deadline); {
		accessibilityCall(t, client, bus, lists[1].Path, "org.freedesktop.DBus.Properties.Get", atspiPrefix+"Selection", "NSelectedChildren").Store(&selected)
		if selected.Value() == int32(1) {
			break
		}
		time.Sleep(time.Millisecond)
	}
	if selected.Value() != int32(1) {
		t.Fatalf("C selection count = %v", selected)
	}
	accessibilityCall(t, client, bus, lists[1].Path, atspiPrefix+"Selection.SelectChild", int32(2)).Store(&accepted)
	if !accepted || !reader.Scan() || reader.Text() != "APPLIED" {
		t.Fatal("C list selection/reveal did not complete")
	}
	if err := command.Wait(); err != nil {
		t.Fatalf("C selection shutdown: %v", err)
	}
}
