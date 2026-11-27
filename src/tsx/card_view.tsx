import * as NativeCard from "kryon/card";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface CardProps {
    session: Session;
    props: NativeCard.Props;
    children: Scoped<() => void>;
}

export interface ResolvedCardProps {
    session: Session;
    props: NativeCard.Props;
    children: Scoped<(card: NativeCard.CardResult) => void>;
}

export function Card(widget: CardProps): NativeCard.CardResult {
    const result = NativeCard.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export function ResolvedCard(widget: ResolvedCardProps): NativeCard.CardResult {
    const result = NativeCard.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}
