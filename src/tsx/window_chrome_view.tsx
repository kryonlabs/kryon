import * as NativeWindowChrome from "kryon/window_chrome";
import * as Widgets from "kryon/tsx/widget";

export function WindowChrome(widget: Widgets.WidgetProps<NativeWindowChrome.Props>): NativeWindowChrome.Action {
    return NativeWindowChrome.Draw(widget.session, widget.props);
}
