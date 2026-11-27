import * as App from "kryon/app";
import * as Dashboard from "dashboard";
import { Session } from "kryon/session";

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    Dashboard.DashboardFrame(session, frame.viewport);
}
export function main(): s32 { return App.Run({title: "Dashboard"}, Frame); }
