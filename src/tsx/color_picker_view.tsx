import * as NativeColorPicker from "kryon/color_picker";
import * as Widgets from "kryon/tsx/widget";

export function ColorPicker(widget: Widgets.WidgetProps<NativeColorPicker.Props>): NativeColorPicker.ColorPickerResult {
    return NativeColorPicker.Draw(widget.session, widget.props);
}
