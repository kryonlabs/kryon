import * as NativeSpinbox from "kryon/spinbox";
import * as Widgets from "kryon/tsx/widget";

export function Spinbox(widget: Widgets.WidgetProps<NativeSpinbox.Props>): NativeSpinbox.SpinboxStepResult {
    return NativeSpinbox.Draw(widget.session, widget.props);
}
