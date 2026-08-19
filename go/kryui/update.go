package kryui

// Go bindings for kryon's self-update lifecycle (include/kry_update_flow.h):
// check an appcast, download+verify the channel artifact, apply it. The
// host app calls Start once, Poll each frame, and renders from State().
// The Windows-portable zip extractor callback is not bound here (the Go
// apps embedding this today ship Linux AppImages); Windows callers fall
// back to the release URL.

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/vendor/curl/lib64/libcurl.a -lssl -lcrypto -lz -lpthread -lbrotlidec -lbrotlicommon -lzstd
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/../../build/linux-aarch64/vendor/curl/lib64/libcurl.a -lssl -lcrypto -lz -lpthread -lbrotlidec -lbrotlicommon -lzstd
#include <kry_update_flow.h>
#include <stdlib.h>
*/
import "C"

import (
	"unsafe"
)

// UpdateFlowState mirrors KryUpdateFlowState.
type UpdateFlowState int

const (
	UpdateFlowIdle UpdateFlowState = iota
	UpdateFlowChecking
	UpdateFlowAvailable
	UpdateFlowDownloading
	UpdateFlowReady
	UpdateFlowFailed
	UpdateFlowUpToDate
)

// UpdateFlow drives one appcast-driven self-update lifecycle. Create with
// StartUpdateFlow; Free when done (or leave it for process exit).
type UpdateFlow struct {
	flow *C.KryUpdateFlow
}

// StartUpdateFlow starts an appcast check for appName running
// currentVersion (e.g. "1.2.3"). Returns nil when the HTTP client is
// unavailable.
func StartUpdateFlow(appName, currentVersion, appcastURL string) *UpdateFlow {
	ca := C.CString(appName)
	cv := C.CString(currentVersion)
	cu := C.CString(appcastURL)
	defer func() {
		C.free(unsafe.Pointer(ca))
		C.free(unsafe.Pointer(cv))
		C.free(unsafe.Pointer(cu))
	}()
	cfg := C.KryUpdateFlowConfig{app_name: ca, current_version: cv}
	f := C.kry_update_flow_start(&cfg, cu)
	if f == nil {
		return nil
	}
	return &UpdateFlow{flow: f}
}

// Poll drives in-flight work; call every frame.
func (u *UpdateFlow) Poll() {
	if u == nil || u.flow == nil {
		return
	}
	C.kry_update_flow_poll(u.flow)
}

// State is the current lifecycle state.
func (u *UpdateFlow) State() UpdateFlowState {
	if u == nil || u.flow == nil {
		return UpdateFlowIdle
	}
	return UpdateFlowState(C.kry_update_flow_state(u.flow))
}

// NewVersion is the newest release version ("" before a check resolves).
func (u *UpdateFlow) NewVersion() string {
	if u == nil || u.flow == nil {
		return ""
	}
	return C.GoString(C.kry_update_flow_new_version(u.flow))
}

// ReleaseURL is the release page URL ("" before a check resolves).
func (u *UpdateFlow) ReleaseURL() string {
	if u == nil || u.flow == nil {
		return ""
	}
	return C.GoString(C.kry_update_flow_release_url(u.flow))
}

// HasArtifact reports whether the running channel has a downloadable
// artifact (AppImage, Windows portable); false means system-managed or
// source install — present ReleaseURL instead.
func (u *UpdateFlow) HasArtifact() bool {
	if u == nil || u.flow == nil {
		return false
	}
	return C.kry_update_flow_artifact(u.flow) != nil
}

// Progress is the download fraction 0..1, or -1 while unknown.
func (u *UpdateFlow) Progress() float64 {
	if u == nil || u.flow == nil {
		return -1
	}
	return float64(C.kry_update_flow_progress(u.flow))
}

// Error is the failure diagnostic on UpdateFlowFailed ("" otherwise).
func (u *UpdateFlow) Error() string {
	if u == nil || u.flow == nil {
		return ""
	}
	if e := C.kry_update_flow_error(u.flow); e != nil {
		return C.GoString(e)
	}
	return ""
}

// Download begins (or retries) the verified artifact download.
func (u *UpdateFlow) Download() bool {
	if u == nil || u.flow == nil {
		return false
	}
	return C.kry_update_flow_download(u.flow) == 1
}

// Apply stages the update: on AppImage it arms the exit re-exec, so the
// app should then quit and call ExecPending after teardown.
func (u *UpdateFlow) Apply() bool {
	if u == nil || u.flow == nil {
		return false
	}
	return C.kry_update_flow_apply(u.flow) == 1
}

// ExecPending performs the armed re-exec; call once the UI and state are
// down. Reports whether an update was pending.
func (u *UpdateFlow) ExecPending() bool {
	if u == nil || u.flow == nil {
		return false
	}
	return C.kry_update_flow_exec_pending(u.flow) == 1
}

// Free releases the flow (and any in-flight request).
func (u *UpdateFlow) Free() {
	if u == nil || u.flow == nil {
		return
	}
	C.kry_update_flow_free(u.flow)
	u.flow = nil
}
