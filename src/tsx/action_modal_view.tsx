import * as NativeModal from "kryon/modal";
import { Session } from "kryon/session";

export interface ActionModalProps {
    session: Session;
    props: NativeModal.ActionModalProps;
    actions: NativeModal.ActionModalAction[];
}

export function ActionModal(widget: ActionModalProps): NativeModal.ActionModalResult {
    return NativeModal.ActionModal(widget.session, widget.props, widget.actions);
}
