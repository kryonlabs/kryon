import * as NativeAvatar from "kryon/avatar";
import { Session } from "kryon/session";
import * as Widgets from "kryon/tsx/widget";

export interface AvatarGroupProps {
    session: Session;
    props: NativeAvatar.AvatarGroupProps;
    names: string[];
}

export function Avatar(widget: Widgets.WidgetProps<NativeAvatar.Props>): NativeAvatar.AvatarResult {
    return NativeAvatar.Draw(widget.session, widget.props);
}

export function AvatarGroup(widget: AvatarGroupProps): NativeAvatar.AvatarGroupResult {
    return NativeAvatar.AvatarGroup(widget.session, widget.props, widget.names);
}
