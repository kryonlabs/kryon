import * as App from "kryon/app";
import * as Layout from "kryon/layout";
import * as NativeListBox from "kryon/list_box";
import * as Model from "gallery_state";
import * as Scenes from "scenes";
import * as Shot from "catalog_shot";
import {ComposeGallery, GallerySceneName, InstallGalleryPalette, SetGalleryPreview} from "catalog";
import {StyleTokenColor} from "kryon/drawing_props";
import {CaptureFrameInput} from "kryon/frame_snapshot";
import {Rectangle} from "kryon/geometry";
import {KeyDown, KeyEnd, KeyHome, KeyUp} from "kryon/input_protocol";
import {Session} from "kryon/session";
import {StyleKindColumn, StyleKindRow, StyleKindText} from "kryon/style_sheet";
import {CurrentTheme, ThemeMetricRole} from "kryon/theme";
import {End, TreePushScope, TreeSubmittedCount, TreeSubmittedNodeAt} from "kryon/tree";
import {TreeFocusSubmittedAt} from "kryon/tree_input";
import {WidgetKind} from "kryon/widget_kind";
import {Box} from "kryon/tsx/box";
import {Checkbox} from "kryon/tsx/checkbox";
import {MeasuredColumn, MeasuredRow, Row} from "kryon/tsx/layout";
import {ListBox} from "kryon/tsx/list_box";
import {Text} from "kryon/tsx/text";
import {Toggle} from "kryon/tsx/toggle";

interface GalleryHeaderProps {
    session: Session;
    bounds: Rectangle;
}

function GalleryHeader(header: GalleryHeaderProps): void {
    <Text session={header.session} props={{key: u64(9001n), bounds: header.bounds,
        text: GallerySceneName(Model.scene), size: s32(23)}}/>;
}

function DrawPreview(session: Session, bounds: Rectangle, elapsed: float32): void {
    SetGalleryPreview(bounds, true, elapsed, CurrentTheme().motion.reduced_motion);
    ComposeGallery(session, Model.scene, Model.disabled_controls);
}

interface GalleryControlsProps {
    session: Session;
    sidebar: Rectangle;
    controls: Rectangle;
    gap: s32;
    restoreFocus: boolean;
}

function GalleryControls(view: GalleryControlsProps): void {
    const session = view.session;
    // Modal scenes own the input layer. Submit the controls in that same layer.
    let modal = s32(-1);
    for (let index = s32(0); index < TreeSubmittedCount(session); index += s32(1)) {
        if (TreeSubmittedNodeAt(session, index).kind === s32(WidgetKind.WidgetKindModal)) {
            modal = index;
        }
    }
    const controlsScope = modal >= s32(0) && TreePushScope(session, modal);
    let navigation = s32(0);
    if (Model.picker_key === KeyHome) { navigation = NativeListBox.ListBoxKeyHome(); }
    else if (Model.picker_key === KeyEnd) { navigation = NativeListBox.ListBoxKeyEnd(); }
    else if (Model.picker_key === KeyUp) { navigation = NativeListBox.ListBoxKeyUp(); }
    else if (Model.picker_key === KeyDown) { navigation = NativeListBox.ListBoxKeyDown(); }
    const picked = ListBox({session: session, props: {key: u64(9003n), id: s32(9003),
        bounds: view.sidebar, selected_index: Model.scene, scroll_offset: Model.catalog_scroll,
        row_height: s32(30), focused: Model.picker_focused, navigation: navigation},
        items: Model.GalleryPickerItems()});
    nativeSet(nativeRef(Model.catalog_scroll), picked.scroll_offset);
    if (picked.changed) {
        nativeSet(nativeRef(Model.scene), picked.selected_index);
        nativeSet(nativeRef(Scenes.dismissed), false);
        nativeSet(nativeRef(Model.navigation_focus_pending), true);
    }
    if (navigation !== s32(0) || view.restoreFocus) {
        for (let index = s32(0); index < TreeSubmittedCount(session); index += s32(1)) {
            const entry = TreeSubmittedNodeAt(session, index);
            if (entry.parent === picked.node && entry.key === u64(Model.scene + s32(1))) {
                TreeFocusSubmittedAt(session, index);
            }
        }
    }
    <Row session={session} props={{bounds: view.controls, gap: view.gap, gap_override: true}} children={(): void => {
            const disabled = Checkbox({session: session, props: {key: u64(9004n),
                bounds: {width: 208, height: 40}, label: "Disabled", checked: Model.disabled_controls}});
            nativeSet(nativeRef(Model.disabled_controls), disabled.checked);
            const theme = Toggle({session: session, props: {key: u64(9005n),
                bounds: {width: 96, height: 40}, label: "Dark", value: Scenes.dark}});
            nativeSet(nativeRef(Scenes.dark), theme.value);
        }}/>;
    if (controlsScope) { End(session); }
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    if (Shot.CatalogShotActive()) {
        SetGalleryPreview(viewport, false, 0.0, true);
        InstallGalleryPalette(Shot.shot_dark);
        <Box session={session} props={{key: u64(9100n), bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
        ComposeGallery(session, Shot.shot_scene, false);
        return;
    }
    const elapsed = float32(CaptureFrameInput(session).elapsed_ms) / 1000.0;
    InstallGalleryPalette(Scenes.dark);
    <Box session={session} props={{key: u64(9000n), bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    if (Model.embedded) {
        <GalleryHeader session={session} bounds={{x: 24, y: 12, width: 432, height: 36}}/>;
        DrawPreview(session, nativeInit<Rectangle>({width: 480, height: viewport.height - 88.0}), elapsed);
        return;
    }
    const restoreFocus = Model.navigation_focus_pending;
    nativeSet(nativeRef(Model.navigation_focus_pending), false);
    const gap = Layout.Gap(ThemeMetricRole.ThemeMetricSpace4);
    const pageItems: [Layout.FlexItem, Layout.FlexItem, Layout.FlexItem] = [
        nativeInit<Layout.FlexItem>({kind: StyleKindText(), basis: 36, has_basis: true}),
        nativeInit<Layout.FlexItem>({kind: StyleKindColumn(), grow: 1, grow_override: true}),
        nativeInit<Layout.FlexItem>({kind: StyleKindRow(), basis: 40, has_basis: true})
    ];
    let bands: [Layout.FlexPlacement, Layout.FlexPlacement, Layout.FlexPlacement] = [nativeInit<Layout.FlexPlacement>({}), nativeInit<Layout.FlexPlacement>({}), nativeInit<Layout.FlexPlacement>({})];
    const rowItems: [Layout.FlexItem, Layout.FlexItem] = [
        nativeInit<Layout.FlexItem>({kind: StyleKindColumn(), basis: 480, has_basis: true, grow: 1, grow_override: true, min_width: 240}),
        nativeInit<Layout.FlexItem>({kind: StyleKindColumn(), basis: 280, has_basis: true, grow: 0, grow_override: true})
    ];
    let slots: [Layout.FlexPlacement, Layout.FlexPlacement] = [nativeInit<Layout.FlexPlacement>({}), nativeInit<Layout.FlexPlacement>({})];
    const rowInput = nativeSlice(rowItems);
    const bandPositions = nativeSlice(bands);
    const rowPositions = nativeSlice(slots);
    const page = MeasuredColumn({session: session, props: {key: u64(9006n), bounds: viewport,
        padding: Layout.Gap(ThemeMetricRole.ThemeMetricSpace6), padding_override: true,
        gap: gap, gap_override: true}, items: nativeSlice(pageItems), placements: bandPositions,
        children: (_page: Layout.LayoutContainer): void => {
            <GalleryHeader session={session} bounds={bandPositions[s32(0)].bounds}/>;
            <MeasuredRow session={session} props={{bounds: bandPositions[s32(1)].bounds, gap: gap, gap_override: true}}
                items={rowInput} placements={rowPositions} children={(_row: Layout.LayoutContainer): void => {}}/>;
        }});
    if (!page.opened) { return; }
    // The scene stays in the root scope so its overlays can anchor to the viewport.
    DrawPreview(session, slots[s32(0)].bounds, elapsed);
    <GalleryControls session={session} sidebar={slots[s32(1)].bounds} controls={bands[s32(2)].bounds}
        gap={gap} restoreFocus={restoreFocus}/>;
}

export function main(): s32 {
    return App.Run({title: "app"}, Frame);
}
