import * as NativeNavigationBar from "kryon/navigation";
import { Session } from "kryon/session";

export interface NavigationBarProps {
    session: Session;
    props: NativeNavigationBar.Props;
    items: NativeNavigationBar.NavigationBarItem[];
}

export function NavigationBar(widget: NavigationBarProps): NativeNavigationBar.NavigationBarResult {
    return NativeNavigationBar.Draw(widget.session, widget.props, widget.items);
}
