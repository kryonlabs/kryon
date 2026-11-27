import * as NativeToggle from "kryon/toggle";
import * as Widgets from "kryon/tsx/widget";

export function Toggle(widget: Widgets.WidgetProps<NativeToggle.Props>): NativeToggle.ToggleValueResult {
    return NativeToggle.Draw(widget.session, widget.props);
}
