import * as App from "kryon/app";
import * as Scene from "kryon/examples/contextmenu";

export function main(): s32 {
    return Scene.main();
}

export function Frame(frame: App.Frame): void {
    Scene.Frame(frame);
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    Scene.SetExampleOptions(disabled, reducedMotion);
}
