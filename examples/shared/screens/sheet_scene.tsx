import * as App from "kryon/app";
import * as Native from "kryon/sheet";
import { Session } from "kryon/session";
import { OverlaySide } from "kryon/tooltip_common";
import { Button } from "kryon/tsx/button";
import { Sheet } from "kryon/tsx/sheet";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { OverlayFrameBounds, OverlaySceneAnchor, OverlaySceneKeys, OverlayScenePointer,
    OverlaySceneTime, OverlaySceneViewport } from "overlay_scene_input";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

interface SheetExampleState {
    initialized: boolean;
    sheet: Native.SheetState;
    open: boolean;
}

let standaloneState: SheetExampleState = {initialized: false, sheet: nativeInit<Native.SheetState>({}), open: false};
let galleryState: SheetExampleState = {initialized: false, sheet: nativeInit<Native.SheetState>({}), open: false};
let captureState: SheetExampleState = {initialized: false, sheet: nativeInit<Native.SheetState>({}), open: false};

function DrawSheet(frame: App.Frame, gallery: boolean, live: boolean): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = OverlayFrameBounds(frame.viewport, live);
    const liveState = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const state = live ? liveState : nativeRef(captureState);
    if (!state.initialized) {
        state.sheet = Native.SheetStateFor();
        state.initialized = true;
    }
    let props: Native.Props = nativeInit<Native.Props>({key: u64(710), title: "Publish checklist",
        description: "Run local tests, review captures, then stop.", close_label: "Close",
        open: true, side: OverlaySide.OverlaySideBottom, size: 156, viewport: gallery ? OverlaySceneViewport(viewport) : viewport,
        scrim_close: true, drag_close_distance: 72, drag_enabled: true, now_ms: s64(500), disabled: exampleDisabled,
        surface_style: Native.SheetSurfaceDefaultStyle(), scrim_style: Native.SheetScrimDefaultStyle()});
    if (live) {
        const anchor = OverlaySceneAnchor(viewport);
        props.input.pointer = OverlayScenePointer(session, anchor);
        props.input.keys = OverlaySceneKeys(session);
        const triggerKey = gallery ? u64(709) : u64(0);
        const opened = Button({session: session, props: {key: triggerKey, bounds: anchor,
            label: "Open sheet", disabled: exampleDisabled}});
        if (opened && !state.open) {
            state.open = true;
            // The activation belongs to the trigger, so the popup starts with fresh input.
            props.input.pointer.released = false;
            props.input.keys.enter = false;
        }
        props.open = state.open;
        props.now_ms = OverlaySceneTime(session);
        props.surface_style.motion_reduced = exampleReducedMotion;
    }
    const result = Sheet({session: session, props: props, state: nativeRef(state.sheet)});
    if (live && result.close_requested) {
        state.open = false;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    UseEscapeForEditing();
    <EditorExample frame={frame} title="Sheet">{DrawSheet(frame, false, true)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawSheet(frame, true, true);
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    DrawSheet(frame, true, false);
}

export function main(): s32 {
    return App.Run({title: "sheet"}, Frame);
}
