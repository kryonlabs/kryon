import * as NativeConfirmPopover from "kryon/confirm_popover";
import { Session } from "kryon/session";

export interface ConfirmPopoverProps {
    session: Session;
    props: NativeConfirmPopover.Props;
    state: NativeRef<NativeConfirmPopover.ConfirmPopoverState>;
}

export function ConfirmPopover(widget: ConfirmPopoverProps): NativeConfirmPopover.ConfirmPopoverResult {
    return NativeConfirmPopover.Draw(widget.session, widget.props, widget.state);
}
