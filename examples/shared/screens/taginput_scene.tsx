import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { TagInputDefaultStyle } from "kryon/tag_input";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { TagInput } from "kryon/tsx/tag_input";
import { Text } from "kryon/tsx/text";
import * as Model from "taginput_model";

let exampleDisabled: boolean = false;

function DrawTags(session: Session, viewport: Rectangle, gallery: boolean): void {
    const bounds: Rectangle = {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 96, width: float32(432) * (viewport.width / float32(480)), height: 104};
    let style = TagInputDefaultStyle();
    style.width = bounds.width;
    style.height = bounds.height;
    const state = Model.Tags(gallery);
    const key = gallery ? u64(600) : u64(0);
    <TagInput state={state} widget={{session: session, props: {key: key, bounds: bounds,
        label: "Project tags", placeholder: "Add tag", max_tags: s32(6), allow_duplicates: false,
        trim_spaces: true, separator: ",", disabled: exampleDisabled, input: Model.Input(session), style: style}}}/>;
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
        <Text session={session} props={{bounds: {height: 36}, text: "TagInput", size: s32(23)}}/>
        {DrawTags(session, viewport, false)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    DrawTags(session, frame.viewport, true);
}

export function main(): s32 {
    return App.Run({title: "taginput"}, Frame);
}
