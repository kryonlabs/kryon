import * as NativeTimePicker from "kryon/time_picker";
import * as Widgets from "kryon/tsx/widget";

export function TimePicker(widget: Widgets.WidgetProps<NativeTimePicker.Props>): NativeTimePicker.TimePickerResult {
    return NativeTimePicker.Draw(widget.session, widget.props);
}
