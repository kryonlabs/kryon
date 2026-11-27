import * as NativeFlow from "kryon/flow";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface FlowProps {
    session: Session;
    props: NativeFlow.Props;
    children: Scoped<() => void>;
}

export interface ResolvedFlowProps {
    session: Session;
    props: NativeFlow.Props;
    children: Scoped<(flow: NativeFlow.FlowResult) => void>;
}

export function Flow(widget: FlowProps): NativeFlow.FlowResult {
    const result = NativeFlow.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export function ResolvedFlow(widget: ResolvedFlowProps): NativeFlow.FlowResult {
    const result = NativeFlow.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}
