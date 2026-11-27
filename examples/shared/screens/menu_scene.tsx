import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Rectangle } from "kryon/geometry";
import * as NativeMenu from "kryon/menu";
import { Session } from "kryon/session";
import { TransitionMotionState } from "kryon/transition";
import { Menu } from "kryon/tsx/menu";
import { EditorExample } from "editor_example";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let standaloneState: NativeMenu.MenuState = nativeInit<NativeMenu.MenuState>({});
let galleryState: NativeMenu.MenuState = nativeInit<NativeMenu.MenuState>({});
let standaloneMotion: TransitionMotionState = nativeInit<TransitionMotionState>({});

function DrawMenu(session: Session, props: NativeMenu.Props, state: NativeRef<NativeMenu.MenuState>): void {
    const items: [NativeMenu.MenuItem, NativeMenu.MenuItem] = [
        nativeInit<NativeMenu.MenuItem>({id: s32(1), label: "New note"}), nativeInit<NativeMenu.MenuItem>({id: s32(2), label: "Open...", disabled: exampleDisabled})];
    const groups: [NativeMenu.MenuGroup] = [{label: "File", items: nativeSlice(items)}];
    if (!state.initialized) {
        NativeMenu.MenuStateInit(state);
        state.open_index = s32(0);
    }
    <Menu session={session} props={props} groups={nativeSlice(groups)} state={state}/>;
}

function DrawStandalone(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const now = float64(CaptureFrameInput(session).elapsed_ms) / 1000.0;
    DrawMenu(session, nativeInit<NativeMenu.Props>({bounds: {x: viewport.x + 24, y: viewport.y + 64, width: viewport.width - 48, height: 44},
        now: now, reduced_motion: exampleReducedMotion, motion: nativeRef(standaloneMotion)}), nativeRef(standaloneState));
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Menu">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    DrawMenu(session, nativeInit<NativeMenu.Props>({bounds: SelectionBounds(frame.viewport, 64, 44)}), nativeRef(galleryState));
}

export function main(): s32 {
    return App.Run({title: "menu"}, Frame);
}
