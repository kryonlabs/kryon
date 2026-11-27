import * as App from "kryon/app";
import * as Dashboard from "dashboard";
import { Session } from "kryon/session";
let previewReady: boolean = false;
export function Frame(frame: App.Frame): void {
    if (!previewReady) { previewReady = true; Dashboard.DashboardSetFixture("settings"); }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    Dashboard.DashboardFrame(session, frame.viewport);
}
export function main(): s32 { return App.Run({title: "settings"}, Frame); }
