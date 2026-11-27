import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import * as NativeTabBar from "kryon/tab_bar";
import { ThemeMetricRole } from "kryon/theme";
import { TransitionMotionState } from "kryon/transition";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { TabBar } from "kryon/tsx/tab_bar";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let selected: s32 = s32(1);
let gallerySelected: s32 = s32(1);
let motion: TransitionMotionState = nativeInit<TransitionMotionState>({});

interface DemoTabsProps {
    session: Session;
    props: NativeTabBar.Props;
}

function DemoTabs(widget: DemoTabsProps): s32 {
    const tabs: [NativeTabBar.Tab, NativeTabBar.Tab, NativeTabBar.Tab] = [
        nativeInit<NativeTabBar.Tab>({label: "Overview"}), nativeInit<NativeTabBar.Tab>({label: "Notes"}), nativeInit<NativeTabBar.Tab>({label: "Files"}),
    ];
    const result = TabBar({session: widget.session, props: widget.props, tabs: nativeSlice(tabs)});
    return result.selected_index;
}

function DrawStandaloneTabs(session: Session, now: float64, disabled: boolean, reducedMotion: boolean): void {
    selected = DemoTabs({session: session, props: nativeInit<NativeTabBar.Props>({bounds: {height: 44}, selected_index: selected,
        disabled: disabled, motion: nativeRef(motion), now: now, reduced_motion: reducedMotion})});
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    const reducedMotion = exampleReducedMotion;
    const now = float64(CaptureFrameInput(session).elapsed_ms) / float64(1000);
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "TabBar", size: s32(23)}}/>
        {DrawStandaloneTabs(session, now, disabled, reducedMotion)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    gallerySelected = DemoTabs({session: session, props: nativeInit<NativeTabBar.Props>({selected_index: gallerySelected, disabled: exampleDisabled,
        bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
            y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480)), height: 44}})});
}

export function main(): s32 {
    return App.Run({title: "tabbar"}, Frame);
}
