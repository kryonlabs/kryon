import * as Scene from "kryon/examples/toast";
import * as App from "kryon/app";

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void { Scene.SetExampleOptions(disabled, reducedMotion); }
export function Frame(frame: App.Frame): void { Scene.Frame(frame); }
export function main(): s32 { return Scene.main(); }
