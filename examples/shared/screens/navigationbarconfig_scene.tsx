import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import * as NativeNavigationBarConfig from "kryon/navigation_bar_config";
import { Session } from "kryon/session";
import { PointerWheelTake } from "kryon/tree_input";
import { Button } from "kryon/tsx/button";
import { NavigationBarConfig } from "kryon/tsx/navigation_bar_config";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { SelectionBounds } from "selection_bounds";

interface ConfigState {
    routes: [s32, s32, s32, s32];
    count: s32;
    scroll: s32;
    open: boolean;
    row: s32;
    dismissed: boolean;
}

let exampleDisabled: boolean = false;
let standaloneConfig: ConfigState = {routes: [s32(1), s32(2), s32(0), s32(0)],
    count: s32(2), scroll: s32(0), open: false, row: s32(0), dismissed: false};
let galleryConfig: ConfigState = {routes: [s32(1), s32(2), s32(0), s32(0)],
    count: s32(2), scroll: s32(0), open: false, row: s32(0), dismissed: false};

function DrawConfig(frame: App.Frame, state: NativeRef<ConfigState>, openerBounds: Rectangle): void {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (state.dismissed) {
        const reopen = Button({session: session, props: {bounds: openerBounds, label: "Configure navigation",
            disabled: exampleDisabled}});
        if (reopen) { state.dismissed = false; }
        return;
    }
    const labels: [string, string, string, string] = ["First", "Second", "Third", "Fourth"];
    const options: [NativeNavigationBarConfig.Option, NativeNavigationBarConfig.Option] = [
        nativeInit<NativeNavigationBarConfig.Option>({route: s32(1), label: "Home"}), nativeInit<NativeNavigationBarConfig.Option>({route: s32(2), label: "Library"})];
    const bounds = SelectionBounds(frame.viewport, 48, 256);
    const wheel = exampleDisabled || state.open ? float32(0) : PointerWheelTake(session, bounds);
    const result = NavigationBarConfig({session: session, props: {bounds: bounds,
        title: "Navigation", route_count: state.count, max_route_count: s32(4), scroll_offset: state.scroll,
        dropdown_open: state.open, dropdown_row: state.row, close_label: "Close", remove_label: "Remove",
        add_label: "Add", save_label: "Save", cancel_label: "Cancel", disabled: exampleDisabled, wheel: wheel},
        routes: nativeSlice(state.routes),
        slot_labels: nativeSlice(labels), options: nativeSlice(options)});
    state.count = result.route_count;
    state.scroll = result.scroll_offset;
    state.open = result.dropdown_open;
    state.row = result.dropdown_row;
    if (result.action !== s32(0)) { state.dismissed = true; }
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="NavigationBarConfig">
        {DrawConfig(frame, nativeRef(standaloneConfig), nativeInit<Rectangle>({height: 44}))}
    </EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawConfig(frame, nativeRef(galleryConfig), SelectionBounds(frame.viewport, 64, 44));
}

export function main(): s32 {
    return App.Run({title: "navigationbarconfig"}, Frame);
}
