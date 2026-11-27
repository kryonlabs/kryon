import * as NativePress from "kryon/press";
import * as Widgets from "kryon/tsx/widget";

export function Press(widget: Widgets.WidgetProps<NativePress.Props>): NativePress.PressResult {
    return NativePress.Draw(widget.session, widget.props);
}
