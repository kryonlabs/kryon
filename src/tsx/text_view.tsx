import * as NativeText from "kryon/text";
import * as Widgets from "kryon/tsx/widget";

export function Text(widget: Widgets.WidgetProps<NativeText.Props>): void {
    NativeText.Draw(widget.session, widget.props);
}
