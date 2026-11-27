// Screen composition and event wiring shared by the desktop input harness and app.
import * as App from "kryon/app";
import * as State from "time_picker_state";
import * as NativeText from "kryon/text";
import * as NativeSegmented from "kryon/segmented_control";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { StyleTokenColor } from "kryon/drawing_props";
import { TextAlign } from "kryon/text_align";
import { Box } from "kryon/tsx/box";
import { SegmentedControl } from "kryon/tsx/segmented_control";
import { Text } from "kryon/tsx/text";
import { TimePicker } from "kryon/tsx/time_picker";

function Label(session: Session, text: string, bounds: Rectangle, font: s32, muted: boolean): void {
    <Text session={session} props={{bounds: bounds, text: text, size: font, disabled: muted,
        wrap: NativeText.Wrap.TextWrapNone, vertical_align: TextAlign.TextAlignCenter}}/>;
}

function Card(session: Session, bounds: Rectangle): void {
    <Box session={session} props={{bounds: bounds, fill: StyleTokenColor("surface"),
        border: StyleTokenColor("border"), radius: 10}}/>;
}

function Picker(session: Session, id: s32, label: string, row: Rectangle,
    value: NativeRef<s32>, actions: boolean, disabled: boolean): void {
    Label(session, label, {x: row.x, y: row.y, width: 200, height: row.height}, s32(15), false);
    const result = TimePicker({session: session, props: {id: id, label: label,
        bounds: {x: row.x + float32(210), y: row.y, width: 180, height: row.height},
        value: nativeDeref(value), now: State.now_minutes, now_label: actions ? "Now" : "",
        done_label: actions ? "Done" : "", disabled: disabled}});
    if (result.changed) {
        nativeSet(value, result.value);
        State.RecordChange(label, result.value);
    }
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const want = State.Palette();
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    let width = viewport.width - float32(48);
    if (width > float32(560)) { width = float32(560); }
    const x = viewport.x + (viewport.width - width) * float32(0.5);
    let y = viewport.y + float32(24);
    Label(session, "Time picker", {x: x, y: y, width: 260, height: 36}, s32(26), false);
    let options: [NativeSegmented.SegmentOption, NativeSegmented.SegmentOption] = [{label: "Light"}, {label: "Dark"}];
    const switched = SegmentedControl({session: session, props: {id: s32(3),
        bounds: {x: x + width - float32(170), y: y + float32(2), width: 170, height: 0},
        has_selection: true, selected_index: want}, options: nativeSlice(options)});
    if (switched.changed) { nativeSet(nativeRef(State.dark), switched.selected_index === s32(1)); }
    y += float32(48);
    Label(session, "Tap a field, then an hour and a minute; the time changes as you pick.",
        {x: x, y: y, width: width, height: 20}, s32(14), false);
    y += float32(22);
    Label(session, "Keys: Down opens, arrows step, Left and Right switch columns, Enter or Escape closes.",
        {x: x, y: y, width: width, height: 20}, s32(14), false);
    y += float32(36);
    Card(session, {x: x, y: y, width: width, height: 212});
    Label(session, "States", {x: x + float32(20), y: y + float32(14), width: 200, height: 24}, s32(17), false);
    let row: Rectangle = {x: x + float32(20), y: y + float32(52), width: width - float32(40), height: 40};
    Picker(session, s32(20), "No time yet", row, nativeRef(State.empty_time), true, false);
    row.y = row.y + float32(52);
    Picker(session, s32(30), "A chosen time", row, nativeRef(State.chosen_time), false, false);
    row.y = row.y + float32(52);
    Picker(session, s32(40), "Disabled", row, nativeRef(State.disabled_time), false, true);
    y += float32(228);
    Card(session, {x: x, y: y, width: width, height: 252});
    Label(session, "Fasting window", {x: x + float32(20), y: y + float32(14), width: 300, height: 24}, s32(17), false);
    row = {x: x + float32(20), y: y + float32(52), width: width - float32(40), height: 40};
    Picker(session, s32(60), "Stopped eating", row, nativeRef(State.stopped_eating), true, false);
    row.y = row.y + float32(52);
    Picker(session, s32(70), "Started eating", row, nativeRef(State.started_eating), true, false);
    row.y = row.y + float32(50);
    Label(session, State.FastText(), {x: row.x + float32(210), y: row.y, width: 300, height: 22}, s32(14), false);
    row.y = row.y + float32(32);
    Picker(session, s32(90), "When did I start eating?", row, nativeRef(State.first_meal), true, false);
    y += float32(268);
    if (State.last_change_length > s32(0)) {
        Label(session, State.LastChangeText(), {x: x, y: y, width: width, height: 20}, s32(14), false);
    }
}

export function main(): s32 {
    return App.Run({title: "time_picker_demo"}, Frame);
}
