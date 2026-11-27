import * as NativeOneTimeCode from "kryon/one_time_code";
import * as Widgets from "kryon/tsx/widget";

export interface OneTimeCodeProps {
    widget: Widgets.WidgetProps<NativeOneTimeCode.Props>;
    state: NativeRef<NativeOneTimeCode.OneTimeCodeState>;
}

export function OneTimeCode(view: OneTimeCodeProps): NativeOneTimeCode.OneTimeCodeResult {
    return NativeOneTimeCode.Draw(view.widget.session, view.widget.props, view.state);
}
