import * as NativeInlineEdit from "kryon/inline_edit";
import * as Widgets from "kryon/tsx/widget";

export interface InlineEditProps {
    widget: Widgets.WidgetProps<NativeInlineEdit.Props>;
    state: NativeRef<NativeInlineEdit.InlineEditState>;
}

export function InlineEdit(view: InlineEditProps): NativeInlineEdit.InlineEditResult {
    return NativeInlineEdit.Draw(view.widget.session, view.widget.props, view.state);
}
