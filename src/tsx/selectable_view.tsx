import * as NativeSelectable from "kryon/selectable";
import * as Widgets from "kryon/tsx/widget";

export function Selectable(widget: Widgets.WidgetProps<NativeSelectable.Props>): NativeSelectable.SelectableToggleResult {
    return NativeSelectable.Draw(widget.session, widget.props);
}
