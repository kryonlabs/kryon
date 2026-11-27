import * as NativeTagInput from "kryon/tag_input";
import * as Widgets from "kryon/tsx/widget";

export interface TagInputProps {
    widget: Widgets.WidgetProps<NativeTagInput.Props>;
    state: NativeRef<NativeTagInput.TagInputState>;
}

export function TagInput(view: TagInputProps): NativeTagInput.TagInputResult {
    return NativeTagInput.Draw(view.widget.session, view.widget.props, view.state);
}
