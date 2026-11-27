import * as NativeBox from "kryon/box";
import * as Widgets from "kryon/tsx/widget";

export function Box(widget: Widgets.WidgetProps<NativeBox.Props>): s32 {
    return NativeBox.Draw(widget.session, widget.props);
}
