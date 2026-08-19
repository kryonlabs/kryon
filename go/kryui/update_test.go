package kryui

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func pollFlow(t *testing.T, u *UpdateFlow, want UpdateFlowState, timeout time.Duration) UpdateFlowState {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for {
		u.Poll()
		if s := u.State(); s == want {
			return s
		} else if s == UpdateFlowFailed && want != UpdateFlowFailed {
			t.Fatalf("flow failed early: %s", u.Error())
		}
		if time.Now().After(deadline) {
			t.Fatalf("timed out in state %d, wanted %d (err: %s)", u.State(), want, u.Error())
		}
		time.Sleep(10 * time.Millisecond)
	}
}

func TestUpdateFlowLifecycle(t *testing.T) {
	dir := t.TempDir()
	artifact := filepath.Join(dir, "gopass.AppImage")
	payload := []byte("go-binding-artifact")
	if err := os.WriteFile(artifact, payload, 0o755); err != nil {
		t.Fatal(err)
	}
	digest := sha256.Sum256(payload)
	appcast := fmt.Sprintf(`{"version":"9.9.9","notes_url":"https://x/rel",`+
		`"channels":{"appimage-amd64":{"url":"file://%s","sha256":"%s","size":%d}}}`,
		artifact, hex.EncodeToString(digest[:]), len(payload))
	appcastPath := filepath.Join(dir, "appcast.json")
	if err := os.WriteFile(appcastPath, []byte(appcast), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("XDG_DATA_HOME", dir)
	t.Setenv("APPIMAGE", filepath.Join(dir, "running.AppImage"))

	if StartUpdateFlow("", "1.0", "file://"+appcastPath) != nil {
		t.Fatal("empty app name should be rejected")
	}
	u := StartUpdateFlow("gotest", "1.0", "file://"+appcastPath)
	if u == nil {
		t.Skip("HTTP client unavailable")
	}
	defer u.Free()
	if u.State() != UpdateFlowChecking {
		t.Fatalf("state = %d, want Checking", u.State())
	}
	pollFlow(t, u, UpdateFlowAvailable, 5*time.Second)
	if u.NewVersion() != "9.9.9" {
		t.Fatalf("NewVersion = %q", u.NewVersion())
	}
	if u.ReleaseURL() != "https://x/rel" {
		t.Fatalf("ReleaseURL = %q", u.ReleaseURL())
	}
	if !u.HasArtifact() {
		t.Fatal("appimage channel should expose an artifact")
	}
	if !u.Download() {
		t.Fatal("download did not start")
	}
	if u.Download() {
		t.Fatal("double download should be rejected")
	}
	pollFlow(t, u, UpdateFlowReady, 5*time.Second)
	if !u.Apply() {
		t.Fatal("apply refused on READY")
	}
	if !u.ExecPending() {
		t.Fatal("exec pending refused after apply")
	}
	/* ExecPending staged the verified payload over the fake $APPIMAGE and
	 * tried to exec it; the exec of a text file fails harmlessly. */
	staged, err := os.ReadFile(filepath.Join(dir, "running.AppImage"))
	if err != nil || string(staged) != string(payload) {
		t.Fatalf("staged AppImage mismatch: %v %q", err, staged)
	}
}
