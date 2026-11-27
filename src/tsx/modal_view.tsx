import * as NativeModal from "kryon/modal";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface ModalProps {
    session: Session;
    props: NativeModal.Props;
    children: Scoped<() => void>;
}

export function Modal(widget: ModalProps): NativeModal.ModalFrameResult {
    const result = NativeModal.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export interface ResolvedModalProps {
    session: Session;
    props: NativeModal.Props;
    children: Scoped<(modal: NativeModal.ModalFrameResult) => void>;
}

export function ResolvedModal(widget: ResolvedModalProps): NativeModal.ModalFrameResult {
    const result = NativeModal.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}
