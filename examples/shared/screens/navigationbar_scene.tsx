import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import * as NativeNavigationBar from "kryon/navigation";
import { Session } from "kryon/session";
import { TransitionMotionState } from "kryon/transition";
import { NavigationBar } from "kryon/tsx/navigation";
import { EditorExample } from "editor_example";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let standaloneChoice: s32 = s32(0);
let galleryChoice: s32 = s32(0);
let standaloneMotion: TransitionMotionState = nativeInit<TransitionMotionState>({});

function DrawNavigation(frame: App.Frame, props: NativeNavigationBar.Props, active: s32): s32 {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const items: [NativeNavigationBar.NavigationBarItem, NativeNavigationBar.NavigationBarItem,
        NativeNavigationBar.NavigationBarItem] = [
        nativeInit<NativeNavigationBar.NavigationBarItem>({label: "Home", active: active === s32(0), icon_shape: "F:M3 10 L12 3 L21 10 V21 H15 V14 H9 V21 H3 Z"}),
        nativeInit<NativeNavigationBar.NavigationBarItem>({label: "Library", active: active === s32(1), icon_shape: "F:M4 3 H10 V21 H4 Z M14 3 H20 V21 H14 Z"}),
        nativeInit<NativeNavigationBar.NavigationBarItem>({label: "Settings", active: active === s32(2), disabled: exampleDisabled,
            icon_shape: "F:M12 3 L21 8 V16 L12 21 L3 16 V8 Z M9 9 H15 V15 H9 Z"})];
    const result = NavigationBar({session: session, props: props, items: nativeSlice(items)});
    return result.clicked_index >= s32(0) ? result.clicked_index : active;
}

function DrawStandaloneNavigation(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const now = float64(CaptureFrameInput(session).elapsed_ms) / 1000.0;
    standaloneChoice = DrawNavigation(frame, nativeInit<NativeNavigationBar.Props>({bounds: {height: 44}, compact: true, fit_items: true,
        motion: nativeRef(standaloneMotion), now: now, reduced_motion: exampleReducedMotion}), standaloneChoice);
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="NavigationBar">{DrawStandaloneNavigation(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    const viewport = frame.viewport;
    const height = viewport.height < float32(320) ? viewport.height : float32(320);
    galleryChoice = DrawNavigation(frame, nativeInit<NativeNavigationBar.Props>({view_width: s32(viewport.width), view_height: s32(height),
        view_origin: {x: viewport.x, y: viewport.y}}), galleryChoice);
}

export function main(): s32 {
    return App.Run({title: "navigationbar"}, Frame);
}
