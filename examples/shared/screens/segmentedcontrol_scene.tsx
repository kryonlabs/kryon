import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { SegmentOption } from "kryon/segmented_control";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { SegmentedControl } from "kryon/tsx/segmented_control";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let standaloneSegment: s32 = s32(1);
let gallerySegment: s32 = s32(1);

function DrawSegment(session: Session, bounds: Rectangle, selected: s32): s32 {
    const options: [SegmentOption, SegmentOption, SegmentOption] = [
        nativeInit<SegmentOption>({label: "Day"}), nativeInit<SegmentOption>({label: "Week"}), nativeInit<SegmentOption>({label: "Month"}),
    ];
    const result = SegmentedControl({session: session, options: nativeSlice(options), props: {bounds: bounds,
        selected_index: selected, has_selection: true, disabled: exampleDisabled}});
    return result.selected_index;
}

function DrawStandaloneSegment(session: Session): void {
    standaloneSegment = DrawSegment(session, nativeInit<Rectangle>({height: 44}), standaloneSegment);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "SegmentedControl", size: s32(23)}}/>
        {DrawStandaloneSegment(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    gallerySegment = DrawSegment(session, {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480)), height: 44}, gallerySegment);
}

export function main(): s32 {
    return App.Run({title: "segmentedcontrol"}, Frame);
}
