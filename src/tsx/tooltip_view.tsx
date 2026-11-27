import * as NativeTooltip from "kryon/tooltip";
import { Session } from "kryon/session";

export interface TooltipProps {
    session: Session;
    props: NativeTooltip.Props;
    state: NativeRef<NativeTooltip.TooltipState>;
}

export function Tooltip(widget: TooltipProps): NativeTooltip.TooltipResult {
    return NativeTooltip.Draw(widget.session, widget.props, widget.state);
}
