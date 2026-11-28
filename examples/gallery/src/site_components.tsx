import {ButtonEmphasis, ContentAlignment} from "kryon/control_props";
import {StyleTokenColor} from "kryon/drawing_props";
import {Rectangle} from "kryon/geometry";
import {ScrollResult} from "kryon/scroll";
import {SegmentOption} from "kryon/segmented_control";
import {Session} from "kryon/session";
import {TreeSubmittedCount, TreeSubmittedNodeAt} from "kryon/tree";
import {TreeFocusSubmittedAt} from "kryon/tree_input";
import {Box} from "kryon/tsx/box";
import {Button} from "kryon/tsx/button";
import {Checkbox} from "kryon/tsx/checkbox";
import {Group} from "kryon/tsx/layout";
import {Scroll} from "kryon/tsx/scroll";
import {SegmentedControl} from "kryon/tsx/segmented_control";
import {TextArea} from "kryon/tsx/text_area";
import {GalleryNavigationScene, GallerySceneCode, GallerySceneCount, GallerySceneGroup, GallerySceneLabel,
    GallerySceneName, GallerySceneSummary, SetGalleryPreview} from "catalog";
import * as Gallery from "gallery_state";
import * as State from "site_state";
import {SiteMuted, SitePanel, SiteText} from "site_widgets";

function WidgetNavigation(session: Session, bounds: Rectangle): void {
    const selection = Gallery.scene;
    const height = State.SiteNavigationHeight(bounds.height);
    const scroll = Scroll({session: session, props: {key: u64(9003n), bounds: bounds,
        content_height: s32(height), scroll_offset: Gallery.catalog_scroll}, children: (view: ScrollResult): void => {
        nativeSet(nativeRef(Gallery.catalog_scroll), view.scroll_offset);
        let group = "";
        for (let position = s32(0); position < s32(GallerySceneCount); position += s32(1)) {
            const index = GalleryNavigationScene(position);
            const label = GallerySceneGroup(index);
            const y = view.content.y + State.SiteNavigationPosition(position);
            if (label !== group) {
                SiteMuted(session, u64(s32(19000) + position),
                    {x: bounds.x + 12.0, y: y - 28.0, width: bounds.width - 24.0, height: 20}, label, s32(11));
                group = label;
            }
            if (Button({session: session, props: {key: u64(index + s32(1)),
                bounds: {x: bounds.x + 6.0, y: y, width: bounds.width - 12.0, height: 32},
                label: GallerySceneName(index), class_name: s32(11010),
                emphasis: ButtonEmphasis.ButtonEmphasisGhost, content_alignment: ContentAlignment.ContentAlignmentStart,
                selected: index === Gallery.scene}})) { State.SiteSelectScene(index); }
        }
    }});
    if (selection !== Gallery.scene || Gallery.navigation_focus_pending) {
        nativeSet(nativeRef(Gallery.navigation_focus_pending), false);
        for (let i = s32(0); i < TreeSubmittedCount(session); i += s32(1)) {
            const entry = TreeSubmittedNodeAt(session, i);
            if (entry.parent === scroll.node && entry.key === u64(Gallery.scene + s32(1))) {
                TreeFocusSubmittedAt(session, i);
            }
        }
    }
}

function PreviewEditor(session: Session, bounds: Rectangle, source: boolean): void {
    State.SiteResetEditorForSelection();
    State.SiteCompile();
    SetGalleryPreview(bounds, true, State.site_now, State.site_reduced_motion);
    if (!source) {
        <Group session={session} props={{key: u64(10101n), bounds: bounds}} children={(): void => {}}/>;
        nativeSet(nativeRef(State.site_native_bounds), bounds);
        nativeSet(nativeRef(State.site_native_visible), true);
        return;
    }
    const value = State.SiteEditorText();
    const input = State.SiteEditorInput(session);
    const area = TextArea({session: session, props: {key: u64(10102n), bounds: bounds, class_name: s32(11020),
        value: value, wrap: false, input: input, cursor: State.site_source_cursor, anchor: State.site_source_anchor,
        scroll_y: State.site_source_scroll, colors: State.SiteCodeColors(value)}});
    nativeSet(nativeRef(State.site_source_cursor), area.cursor);
    nativeSet(nativeRef(State.site_source_anchor), area.anchor);
    nativeSet(nativeRef(State.site_source_scroll), area.scroll_y);
    if (area.edit.changed) { State.SiteApplyEditorChange(area.edit); }
    nativeSet(nativeRef(State.site_preview_bounds), {x: bounds.x, y: bounds.y + 244.0, width: bounds.width, height: 480});
    nativeSet(nativeRef(State.site_preview_visible), true);
    const status = State.site_compile_busy ? "Compiling…" :
        State.site_diagnostic_count > s32(0) ? State.SiteDiagnostic() : "Live preview";
    SiteMuted(session, u64(10106n), {x: bounds.x, y: bounds.y + 204.0, width: bounds.width, height: 32}, status, s32(13));
}

function ComponentDetails(session: Session, x: float32, top: float32, width: float32, panelHeight: float32, source: boolean): void {
    const index = Gallery.scene;
    SiteMuted(session, u64(10301n), {x: x, y: top, width: width, height: 24}, GallerySceneLabel(index), s32(12));
    SiteText(session, u64(10100n), {x: x, y: top + 38.0, width: width, height: 42}, GallerySceneName(index), s32(30));
    SiteMuted(session, u64(10302n), {x: x, y: top + 92.0, width: width, height: 52}, GallerySceneSummary(index), s32(16));
    const panelTop = top + 156.0;
    SitePanel(session, u64(10303n), {x: x, y: panelTop, width: width, height: panelHeight});
    PreviewEditor(session, {x: x + 16.0, y: panelTop + 56.0, width: width - 32.0, height: source ? 196.0 : 480.0}, source);
    const below = panelTop + panelHeight + 40.0;
    SiteText(session, u64(10304n), {x: x, y: below, width: width, height: 32}, "Usage", s32(21));
    SiteMuted(session, u64(10305n), {x: x, y: below + 46.0, width: width, height: 38},
        "Maintained TSX source. Select Source above to edit and run it.", s32(14));
    const usageCode = GallerySceneCode(index);
    const usage = TextArea({session: session, props: {key: u64(10309n),
        bounds: {x: x, y: below + 96.0, width: width, height: 240},
        value: usageCode, read_only: true, wrap: false, class_name: s32(11020),
        scroll_y: State.site_usage_scroll, manual_scroll: true, colors: State.SiteCodeColors(usageCode)}});
    nativeSet(nativeRef(State.site_usage_scroll), usage.scroll_y);
    <Box session={session} props={{key: u64(10306n),
        bounds: {x: x, y: below + 374.0, width: width, height: 1}, fill: State.SiteBorder()}}/>;
    SiteText(session, u64(10307n), {x: x, y: below + 404.0, width: width, height: 32}, "Keyboard & interaction", s32(21));
    SiteMuted(session, u64(10308n), {x: x, y: below + 454.0, width: width, height: 94},
        "Tab moves focus. Enter and Space activate controls. Arrow keys navigate choices.\nExamples follow your reduced motion preference.", s32(14));
    const options: [SegmentOption, SegmentOption] = [nativeInit<SegmentOption>({label: "Preview"}), nativeInit<SegmentOption>({label: "Source"})];
    const chosen = SegmentedControl({session: session, props: {key: u64(10104n),
        bounds: {x: x + 12.0, y: panelTop + 12.0, width: width > 330.0 ? 184.0 : 146.0, height: 32},
        has_selection: true, selected_index: source ? s32(1) : s32(0)}, options: nativeSlice(options)});
    nativeSet(nativeRef(State.site_source), chosen.selected_index === s32(1));
    const disabled = Checkbox({session: session, props: {key: u64(9004n),
        bounds: {x: x + width - 116.0, y: panelTop + 12.0, width: 108, height: 32}, label: "Disabled",
        checked: Gallery.disabled_controls}});
    nativeSet(nativeRef(Gallery.disabled_controls), disabled.checked);
}

export function SiteComponents(session: Session, view: Rectangle): void {
    const wide = view.width >= 760.0;
    const left: float32 = wide ? 264.0 : 0.0;
    const gutter: float32 = wide ? 48.0 : 20.0;
    const rail: float32 = view.width >= 1280.0 ? 240.0 : 0.0;
    let width = view.width - left - gutter * 2.0 - rail;
    if (width > 900.0) { width = 900.0; }
    const x = left + gutter;
    const source = State.site_source;
    const panelHeight: float32 = source ? 800.0 : 552.0;
    const topGap: float32 = wide ? 28.0 : 58.0;
    <Scroll session={session} props={{key: u64(10300n),
        bounds: {x: left, y: view.y, width: view.width - left, height: view.height},
        content_height: s32(view.height) + (source ? s32(1100) : s32(860)),
        scroll_offset: State.site_component_scroll}} children={(scroll: ScrollResult): void => {
        nativeSet(nativeRef(State.site_component_scroll), scroll.scroll_offset);
        ComponentDetails(session, x, scroll.content.y + topGap, width, panelHeight, source);
    }}/>;
    if (wide) {
        <Box session={session} props={{key: u64(10310n), bounds: {y: view.y, width: 264, height: view.height},
            fill: State.SiteBackground()}}/>;
        <Box session={session} props={{key: u64(10311n), bounds: {x: 263, y: view.y, width: 1, height: view.height},
            fill: State.SiteBorder()}}/>;
        SiteMuted(session, u64(10312n), {x: 24, y: view.y + 24.0, width: 220, height: 24}, "COMPONENT EXAMPLES", s32(12));
        WidgetNavigation(session, {x: 12, y: view.y + 64.0, width: 240, height: view.height - 80.0});
    } else if (Button({session: session, props: {key: u64(10103n),
        bounds: {x: 20, y: view.y + 10.0, width: view.width - 40.0, height: 34}, label: "All widgets"}})) {
        nativeSet(nativeRef(State.site_mobile_navigation), !State.site_mobile_navigation);
    }
    if (view.width >= 1280.0) {
        const railX = x + width + 36.0;
        SiteMuted(session, u64(10500n), {x: railX, y: view.y + 32.0, width: 130, height: 24}, "ON THIS PAGE", s32(11));
        const sections: [string, string, string] = ["Example", "Usage", "Keyboard"];
        for (let i = s32(0); i < s32(3); i += s32(1)) {
            if (Button({session: session, props: {key: u64(s32(10501) + i),
                bounds: {x: railX, y: view.y + 70.0 + float32(i) * 38.0, width: 130, height: 32},
                label: sections[i], class_name: s32(11010)}})) {
                nativeSet(nativeRef(State.site_component_scroll), i === s32(0) ? s32(0) :
                    s32(topGap + 156.0 + panelHeight + 12.0 + (i === s32(2) ? 402.0 : 0.0)));
            }
        }
    }
    if (!wide && State.site_mobile_navigation) {
        <Box session={session} props={{key: u64(10105n),
            bounds: {x: 12, y: view.y + 50.0, width: view.width - 24.0, height: view.height - 62.0},
            fill: StyleTokenColor("surface"), border: StyleTokenColor("border"), radius: 10}}/>;
        WidgetNavigation(session, {x: 16, y: view.y + 58.0, width: view.width - 32.0, height: view.height - 78.0});
    }
}
