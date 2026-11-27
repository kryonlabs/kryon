import * as App from "kryon/app";
import * as Native from "kryon/money_field";
import { Session } from "kryon/session";
import { MoneyField } from "kryon/tsx/money_field";
import { EditorBounds, EditorExample } from "editor_example";
import { ReadEditorInput } from "editor_input";

let exampleDisabled: boolean = false;
let initialized: boolean = false;
let standaloneState: Native.MoneyFieldState = nativeInit<Native.MoneyFieldState>({});
let galleryState: Native.MoneyFieldState = nativeInit<Native.MoneyFieldState>({});

function DrawEditor(frame: App.Frame, gallery: boolean): void {
    if (!initialized) {
        standaloneState = Native.MoneyFieldStateFor(float64(-1234.56), s32(2));
        galleryState = Native.MoneyFieldStateFor(float64(-1234.56), s32(2));
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const bounds = EditorBounds(frame.viewport);
    const input = ReadEditorInput(session, bounds);
    const state = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const key = gallery ? u64(604) : u64(0);
    <MoneyField state={state} widget={{session: session, props: {key: key, bounds: bounds,
        label: "Balance", symbol: "$", code: "USD", hint: "Two decimal places", initial_value: -1234.56,
        minimum: -10000, maximum: 10000, precision: s32(2), input: input.text, grouping: true,
        grouping_separator: ",", decimal_separator: ".", negative_style: Native.MoneyNegativeStyle.MoneyNegativeParentheses,
        locale: Native.MoneyFieldLocale.MoneyLocaleDot, disabled: exampleDisabled, height: 42}}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="MoneyField">{DrawEditor(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawEditor(frame, true);
}

export function main(): s32 {
    return App.Run({title: "moneyfield"}, Frame);
}
