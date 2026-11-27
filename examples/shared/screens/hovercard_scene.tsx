import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import * as Native from "kryon/hover_card";
import { PointInBounds } from "kryon/input_protocol";
import { Session } from "kryon/session";
import { OverlayPlacement } from "kryon/tooltip_common";
import { HoverCard } from "kryon/tsx/hover_card";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";
import { OverlayFrameBounds, OverlaySceneAnchor, OverlayScenePointer,
    OverlaySceneTime, OverlaySceneViewport } from "overlay_scene_input";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

interface CardExampleState {
    initialized: boolean;
    card: Native.HoverCardState;
    bounds: Rectangle;
}

let standaloneState: CardExampleState = {initialized: false, card: nativeInit<Native.HoverCardState>({}), bounds: nativeInit<Rectangle>({})};
let galleryState: CardExampleState = {initialized: false, card: nativeInit<Native.HoverCardState>({}), bounds: nativeInit<Rectangle>({})};
let captureState: CardExampleState = {initialized: false, card: nativeInit<Native.HoverCardState>({}), bounds: nativeInit<Rectangle>({})};

function DrawCard(frame: App.Frame, gallery: boolean, live: boolean): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = OverlayFrameBounds(frame.viewport, live);
    const anchor = OverlaySceneAnchor(viewport);
    const liveState = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const state = live ? liveState : nativeRef(captureState);
    if (!state.initialized) {
        state.card = Native.HoverCardStateFor();
        state.initialized = true;
    }
    let props: Native.Props = nativeInit<Native.Props>({key: gallery ? u64(731) : u64(0), anchor: anchor,
        viewport: gallery ? OverlaySceneViewport(viewport) : viewport, title: "Source preview", body: "Every card opens from caller-owned state.",
        action_label: "Open", action: u64(31), placement: OverlayPlacement.OverlayPlaceBottom,
        width: 270, delay_ms: 350, grace_ms: 150, movement_tolerance: 6, gap: 8, disabled: exampleDisabled,
        style: Native.HoverCardDefaultStyle(), action_style: Native.HoverCardActionDefaultStyle()});
    if (live) {
        props.pointer = OverlayScenePointer(session, anchor);
        props.pointer.over_content = PointInBounds(props.pointer.x, props.pointer.y, state.bounds);
        props.now_ms = OverlaySceneTime(session);
        props.style.motion_reduced = exampleReducedMotion;
    } else {
        props.pointer.over_anchor = true;
        Native.HoverCardApply(nativeRef(state.card), props);
        props.now_ms = s64(450);
    }
    <Text session={session} props={{key: gallery ? u64(730) : u64(0), bounds: anchor, text: "Hover source", size: s32(17)}}/>;
    const result = HoverCard({session: session, props: props, state: nativeRef(state.card)});
    state.bounds = result.bounds;
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="HoverCard">{DrawCard(frame, false, true)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawCard(frame, true, true);
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    DrawCard(frame, true, false);
}

export function main(): s32 {
    return App.Run({title: "hovercard"}, Frame);
}
