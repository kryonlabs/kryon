import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { PressResult } from "kryon/press";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Press } from "kryon/tsx/press";
import { Text } from "kryon/tsx/text";
import { Scoped } from "std/callable";
import { UseEscapeForEditing } from "editor_input";

export interface EditorExampleProps {
    frame: App.Frame;
    title: string;
    children: Scoped<() => void>;
}

export function EditorExample(example: EditorExampleProps): void {
    const session: Session = {slot: example.frame.slot, epoch: example.frame.epoch};
    const viewport = example.frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: example.title, size: s32(23)}}/>
        {example.children()}
    </Column>;
}

export function EditorBounds(viewport: Rectangle): Rectangle {
    return {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 96, width: float32(432) * (viewport.width / float32(480)), height: 104};
}

export function EditorPress(session: Session, bounds: Rectangle, id: s32, label: string,
    disabled: boolean, capture: boolean): PressResult {
    UseEscapeForEditing();
    return Press({session: session, props: {key: u64(id), id: id, bounds: bounds,
        label: label, disabled: disabled, capture: capture}});
}
