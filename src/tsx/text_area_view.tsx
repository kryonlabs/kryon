import * as NativeTextArea from "kryon/text_area";
import * as Widgets from "kryon/tsx/widget";

export function TextArea(widget: Widgets.WidgetProps<NativeTextArea.Props>): NativeTextArea.Result {
    return NativeTextArea.Draw(widget.session, widget.props);
}
