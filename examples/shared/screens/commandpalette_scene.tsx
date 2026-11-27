import * as App from "kryon/app";
import * as Native from "kryon/command_palette";
import { KeyDown, KeyUp } from "kryon/input_protocol";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { CommandPalette } from "kryon/tsx/command_palette";
import { EditorExample } from "editor_example";
import { ReadEditorInput, UseEscapeForEditing } from "editor_input";
import { OverlayFrameBounds, OverlaySceneAnchor, OverlaySceneBounds, OverlaySceneTime } from "overlay_scene_input";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

interface PaletteExampleState {
    initialized: boolean;
    palette: Native.CommandPaletteState;
    open: boolean;
}

let standaloneState: PaletteExampleState = {initialized: false, palette: nativeInit<Native.CommandPaletteState>({}), open: false};
let galleryState: PaletteExampleState = {initialized: false, palette: nativeInit<Native.CommandPaletteState>({}), open: false};
let captureState: PaletteExampleState = {initialized: false, palette: nativeInit<Native.CommandPaletteState>({}), open: false};

function DrawPalette(frame: App.Frame, gallery: boolean, live: boolean): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const bounds = OverlayFrameBounds(frame.viewport, live);
    const anchor = OverlaySceneAnchor(bounds);
    const viewport = OverlaySceneBounds(bounds, {x: 24, y: 64, width: 432, height: 208});
    const commands: [Native.CommandPaletteCommand, Native.CommandPaletteCommand, Native.CommandPaletteCommand] = [
        nativeInit<Native.CommandPaletteCommand>({title: "Find component", group: "Search", shortcut: "Ctrl+F", action: u64(11)}),
        nativeInit<Native.CommandPaletteCommand>({title: "Format source", group: "Edit", shortcut: "Ctrl+Shift+F", action: u64(12)}),
        nativeInit<Native.CommandPaletteCommand>({title: "Focus outline", group: "Accessibility", action: u64(13)}),
    ];
    const liveState = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const state = live ? liveState : nativeRef(captureState);
    if (!state.initialized) {
        state.palette = Native.CommandPaletteStateFor(u64(701));
        state.initialized = true;
    }
    const triggerKey = gallery ? u64(719) : u64(0);
    if (Button({session: session, props: {key: triggerKey, bounds: anchor, label: "Open palette", disabled: exampleDisabled}})) {
        state.open = true;
    }
    const input = ReadEditorInput(session, viewport);
    let surface = Native.CommandPaletteSurfaceDefaultStyle();
    if (live) {
        surface.motion_reduced = exampleReducedMotion;
    }
    const result = CommandPalette({session: session, state: nativeRef(state.palette), props: {key: gallery ? u64(720) : u64(0),
        owner: u64(701), open: state.open, placeholder: "Search commands", viewport: viewport, width: 380,
        maximum_height: 96, now_ms: live ? OverlaySceneTime(session) : s64(500), commands: nativeSlice(commands), disabled: exampleDisabled,
        input: {activate_index: s32(-1), text: input.text, escape: input.text.escape, enter: input.text.commit,
            up: input.key === KeyUp, down: input.key === KeyDown}, surface_style: surface,
        item_style: Native.CommandPaletteItemDefaultStyle(), scrim_style: Native.CommandPaletteSurfaceDefaultStyle()}});
    if (result.close_requested || result.chosen_index >= s32(0)) {
        state.open = false;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    UseEscapeForEditing();
    <EditorExample frame={frame} title="CommandPalette">{DrawPalette(frame, false, true)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawPalette(frame, true, true);
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    DrawPalette(frame, true, false);
}

export function main(): s32 {
    return App.Run({title: "commandpalette"}, Frame);
}
