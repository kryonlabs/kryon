import * as Model from "component_owned_model";
import * as Life from "component_lifecycle";
import * as Types from "component_types";
import * as Actions from "component_actions";
import { Owned } from "std/callable";
import { Text, TextCopy } from "std/vec";

export function cleanup(owner: Types.Instance): void {
    const captured: Text = TextCopy("cleanup");
    const dispose: Owned<Life.Action> = (instance: Types.Instance): void => {
        Model.Clean(instance, TextView(captured).count);
    };
    Actions.OnCleanup(owner, dispose);
}

export function mount(owner: Types.Instance, seconds: float64): Types.Token {
    Model.Mount(owner);
    const captured: Text = TextCopy("owned");
    const run: Owned<Life.Action> = (instance: Types.Instance): void => {
        Model.Call(instance, TextView(captured).count);
    };
    const token: Types.Token = Actions.Callback(owner, run);
    cleanup(owner);
    const timerText: Text = TextCopy("due");
    const timer: Owned<Life.Action> = (instance: Types.Instance): void => {
        Model.Timer(instance, TextView(timerText).count);
    };
    Actions.Timer(owner, seconds + float64(0.1), float64(0), timer);
    return token;
}
