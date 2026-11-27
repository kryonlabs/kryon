import * as NativeMarkdownEditor from "kryon/markdown_editor";
import * as NativeTextArea from "kryon/text_area";
import * as Widgets from "kryon/tsx/widget";

export function MarkdownEditor(widget: Widgets.WidgetProps<NativeMarkdownEditor.Props>): NativeTextArea.Result {
    return NativeMarkdownEditor.Draw(widget.session, widget.props);
}
