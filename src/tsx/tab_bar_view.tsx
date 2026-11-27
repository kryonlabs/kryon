import * as NativeTabBar from "kryon/tab_bar";
import { Session } from "kryon/session";

export interface TabBarProps {
    session: Session;
    props: NativeTabBar.Props;
    tabs: NativeTabBar.Tab[];
}

export function TabBar(widget: TabBarProps): NativeTabBar.Result {
    return NativeTabBar.Draw(widget.session, widget.props, widget.tabs);
}
