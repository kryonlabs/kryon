import * as NativeSeparator from "kryon/separator";
import * as Widgets from "kryon/tsx/widget";

export function Separator(widget: Widgets.WidgetProps<NativeSeparator.Props>): void {
    NativeSeparator.Draw(widget.session, widget.props);
}
