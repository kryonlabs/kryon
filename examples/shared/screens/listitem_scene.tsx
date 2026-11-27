import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { ListTrailing } from "kryon/list_item";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { ListItem } from "kryon/tsx/list_item";

let exampleDisabled: boolean = false;
let notifications: boolean = true;

function DrawNotifications(session: Session, disabled: boolean): void {
    const row = ListItem({session: session, props: {title: "Notifications", trailing: ListTrailing.ListTrailingSwitch,
        switch_on: notifications, disabled: disabled}});
    if (row.switch_changed) {
        notifications = row.switch_on;
    }
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace3), padding_override: true}}>
        <ListItem session={session} props={{title: "Ada Lovelace", subtitle: "Account and profile",
            leading_name: "Ada Lovelace", trailing: ListTrailing.ListTrailingChevron, disabled: disabled}}/>
        {DrawNotifications(session, disabled)}
        <ListItem session={session} props={{title: "Inbox", trailing: ListTrailing.ListTrailingBadge,
            trailing_text: "12", disabled: disabled}}/>
        <ListItem session={session} props={{title: "Storage", subtitle: "4.2 GB of 15 GB",
            trailing: ListTrailing.ListTrailingText, trailing_text: "28%", disabled: disabled}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "listitem"}, Frame);
}
