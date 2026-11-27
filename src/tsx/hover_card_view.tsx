import * as NativeHoverCard from "kryon/hover_card";
import { Session } from "kryon/session";

export interface HoverCardProps {
    session: Session;
    props: NativeHoverCard.Props;
    state: NativeRef<NativeHoverCard.HoverCardState>;
}

export function HoverCard(widget: HoverCardProps): NativeHoverCard.HoverCardResult {
    return NativeHoverCard.Draw(widget.session, widget.props, widget.state);
}
