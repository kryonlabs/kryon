import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import * as Native from "kryon/guided_tour";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { OverlayPlacement } from "kryon/tooltip_common";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { GuidedTour } from "kryon/tsx/guided_tour";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { UseEscapeForEditing } from "editor_input";
import { OverlayFrameBounds, OverlaySceneAnchor, OverlaySceneBounds, OverlaySceneKeys, OverlayScenePointer,
    OverlaySceneTime, OverlaySceneViewport } from "overlay_scene_input";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

interface TourExampleState {
    initialized: boolean;
    tour: Native.GuidedTourState;
    step: s32;
}

let standaloneState: TourExampleState = {initialized: false, tour: nativeInit<Native.GuidedTourState>({}), step: s32(-1)};
let galleryState: TourExampleState = {initialized: false, tour: nativeInit<Native.GuidedTourState>({}), step: s32(-1)};
let captureState: TourExampleState = {initialized: false, tour: nativeInit<Native.GuidedTourState>({}), step: s32(-1)};

function DrawTour(frame: App.Frame, gallery: boolean, live: boolean): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = OverlayFrameBounds(frame.viewport, live);
    const anchor = OverlaySceneAnchor(viewport);
    const steps: [Native.GuidedTourStep, Native.GuidedTourStep] = [
        {target_key: u64(710), target: OverlaySceneBounds(viewport, {x: 168, y: 112, width: 144, height: 42}),
            title: "Highlight the target", body: "The scrim leaves the current control available.",
            placement: OverlayPlacement.OverlayPlaceBottomStart},
        {target_key: u64(711), target: OverlaySceneBounds(viewport, {x: 168, y: 180, width: 144, height: 42}),
            title: "Finish the tour", body: "Back, Next, Done, and Skip are explicit actions.",
            placement: OverlayPlacement.OverlayPlaceTopStart},
    ];
    const liveState = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const state = live ? liveState : nativeRef(captureState);
    if (!state.initialized) {
        state.tour = Native.GuidedTourStateFor(s32(0));
        state.initialized = true;
    }
    let props: Native.Props = nativeInit<Native.Props>({key: gallery ? u64(751) : u64(0), steps: nativeSlice(steps), step: s32(0),
        width: 290, padding: 12, block_target: false, next_label: "Next", back_label: "Back",
        done_label: "Done", skip_label: "Skip", count_format: "%d of %d", viewport: gallery ? OverlaySceneViewport(viewport) : viewport,
        now_ms: s64(500), disabled: exampleDisabled, card_style: Native.GuidedTourCardDefaultStyle(),
        scrim_style: Native.GuidedTourScrimDefaultStyle(), button_style: Native.GuidedTourButtonDefaultStyle()});
    if (live) {
        props.input.pointer = OverlayScenePointer(session, anchor);
        const keys = OverlaySceneKeys(session);
        props.input.escape = keys.escape;
        props.input.enter = keys.enter;
        props.input.left = keys.left;
        props.input.right = keys.right;
    }
    <Text session={session} props={{key: gallery ? u64(750) : u64(0), bounds: steps[s32(0)].target,
        text: "Catalog target", size: s32(17)}}/>;
    if (live) {
        const started = Button({session: session, props: {key: gallery ? u64(749) : u64(0), bounds: anchor,
            label: "Start tour", disabled: exampleDisabled}});
        if (started && state.tour.step < s32(0)) {
            state.tour = Native.GuidedTourStateFor(s32(0));
            state.step = s32(0);
            props.input.enter = false;
        }
        props.step = state.step;
        props.now_ms = OverlaySceneTime(session);
        props.card_style.motion_reduced = exampleReducedMotion;
    }
    const result = GuidedTour({session: session, props: props, state: nativeRef(state.tour)});
    if (live) {
        state.step = result.step;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Box session={session} props={{bounds: frame.viewport, fill: StyleTokenColor("canvas")}}/>;
    const page = Column({session: session, props: {bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}, children: (): void => {
        <Text session={session} props={{bounds: {height: 36}, text: "GuidedTour", size: s32(23)}}/>;
    }});
    // The tour spans the whole view, above the page column.
    if (page.opened) {
        DrawTour(frame, false, true);
    }
}

export function GalleryFrame(frame: App.Frame): void {
    DrawTour(frame, true, true);
}

export function GalleryCaptureFrame(frame: App.Frame): void {
    DrawTour(frame, true, false);
}

export function main(): s32 {
    return App.Run({title: "guidedtour"}, Frame);
}
