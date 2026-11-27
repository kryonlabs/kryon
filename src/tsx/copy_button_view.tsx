import * as NativeCopyButton from "kryon/copy_button";
import * as Widgets from "kryon/tsx/widget";

export interface CopyButtonProps {
    widget: Widgets.WidgetProps<NativeCopyButton.Props>;
    state: NativeRef<NativeCopyButton.CopyButtonState>;
}

export function CopyButton(view: CopyButtonProps): NativeCopyButton.CopyButtonResult {
    return NativeCopyButton.Draw(view.widget.session, view.widget.props, view.state);
}
