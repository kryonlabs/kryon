import * as NativeCheckbox from "kryon/checkbox";
import * as Widgets from "kryon/tsx/widget";

export function Checkbox(widget: Widgets.WidgetProps<NativeCheckbox.Props>): NativeCheckbox.CheckboxValueResult {
    return NativeCheckbox.Draw(widget.session, widget.props);
}
