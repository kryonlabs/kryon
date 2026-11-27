import * as NativeNavigationBarConfig from "kryon/navigation_bar_config";
import { Session } from "kryon/session";

export interface NavigationBarConfigProps {
    session: Session;
    props: NativeNavigationBarConfig.Props;
    routes: s32[];
    slot_labels: string[];
    options: NativeNavigationBarConfig.Option[];
}

export function NavigationBarConfig(widget: NavigationBarConfigProps): NativeNavigationBarConfig.Result {
    return NativeNavigationBarConfig.Draw(widget.session, widget.props,
        widget.routes, widget.slot_labels, widget.options);
}
