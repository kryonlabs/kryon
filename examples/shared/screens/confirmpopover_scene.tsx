import * as App from "kryon/app";
import * as Native from "kryon/confirm_popover";
import { Session } from "kryon/session";
import { OverlayPlacement } from "kryon/tooltip_common";
import { Button } from "kryon/tsx/button";
import { ConfirmPopover } from "kryon/tsx/confirm_popover";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { OverlayFrameBounds, OverlaySceneAnchor, OverlaySceneKeys, OverlayScenePointer,
    OverlaySceneTime, OverlaySceneViewport } from "overlay_scene_input";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

interface ConfirmationExampleState {
    initialized: boolean;
    confirmation: Native.ConfirmPopoverState;
    open: boolean;
}

let standaloneState: ConfirmationExampleState = {initialized: false, confirmation: nativeInit<Native.ConfirmPopoverState>({}), open: false};
let galleryState: ConfirmationExampleState = {initialized: false, confirmation: nativeInit<Native.ConfirmPopoverState>({}), open: false};
let captureState: ConfirmationExampleState = {initialized: false, confirmation: nativeInit<Native.ConfirmPopoverState>({}), open: false};

function DrawConfirmation(frame: App.Frame, gallery: boolean, live: boolean): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = OverlayFrameBounds(frame.viewport, live);
    const anchor = OverlaySceneAnchor(viewport);
    const liveState = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const state = live ? liveState : nativeRef(captureState);
    if (!state.initialized) {
        state.confirmation = Native.ConfirmPopoverStateFor();
        state.initialized = true;
    }
    let props: Native.Props = nativeInit<Native.Props>({key: gallery ? u64(741) : u64(0), title: "Delete source?",
        description: "This action cannot be undone.", confirm_label: "Delete", cancel_label: "Cancel",
        anchor: anchor, viewport: gallery ? OverlaySceneViewport(viewport) : viewport, placement: OverlayPlacement.OverlayPlaceBottom,
        width: 270, gap: 8, danger: true, open: true, now_ms: s64(500), disabled: exampleDisabled,
        focus: {anchor_focused: true}, style: Native.ConfirmPopoverDefaultStyle(),
        confirm_style: Native.ConfirmPopoverConfirmDefaultStyle(true), cancel_style: Native.ConfirmPopoverDefaultStyle()});
    if (live) {
        props.pointer = OverlayScenePointer(session, anchor);
        props.keys = OverlaySceneKeys(session);
        const triggerKey = gallery ? u64(739) : u64(0);
        const opened = Button({session: session, props: {key: triggerKey, bounds: anchor,
            label: "Delete source", disabled: exampleDisabled}});
        if (opened && !state.open) {
            state.open = true;
            // The activation belongs to the trigger, so the popup starts with fresh input.
            props.pointer.released = false;
            props.keys.enter = false;
        }
        props.open = state.open;
        props.now_ms = OverlaySceneTime(session);
        props.style.motion_reduced = exampleReducedMotion;
    } else {
        <Text session={session} props={{key: u64(740), bounds: anchor, text: "Delete source", size: s32(17)}}/>;
    }
    const result = ConfirmPopover({session: session, props: props, state: nativeRef(state.confirmation)});
    if (live && (result.closed || result.confirmed || result.cancelled)) {
        state.open = false;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    UseEscapeForEditing();
    <EditorExample frame={frame} title="ConfirmPopover">{DrawConfirmation(frame, false, true)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawConfirmation(frame, true, true);
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    DrawConfirmation(frame, true, false);
}

export function main(): s32 {
    return App.Run({title: "confirmpopover"}, Frame);
}
