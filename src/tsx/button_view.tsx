import * as NativeButton from "kryon/button";
import * as Widgets from "kryon/tsx/widget";

export function Button(widget: Widgets.WidgetProps<NativeButton.Props>): boolean {
    return NativeButton.Draw(widget.session, widget.props) === s32(1);
}
