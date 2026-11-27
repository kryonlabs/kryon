import {ButtonEmphasis} from "kryon/control_props";
import {Rectangle} from "kryon/geometry";
import {Session} from "kryon/session";
import {TextAlign} from "kryon/text_align";
import {Box} from "kryon/tsx/box";
import {Button} from "kryon/tsx/button";
import {Text} from "kryon/tsx/text";
import * as State from "site_state";

export function SiteText(session: Session, key: u64, bounds: Rectangle, text: string, font: s32): void {
    <Text session={session} props={{key: key, bounds: bounds, text: text, size: font,
        class_name: font >= s32(20) ? s32(11003) : s32(11001)}}/>;
}

export function SiteMuted(session: Session, key: u64, bounds: Rectangle, text: string, font: s32): void {
    <Text session={session} props={{key: key, bounds: bounds, text: text, size: font, class_name: s32(11002)}}/>;
}

export function SiteCenter(session: Session, key: u64, bounds: Rectangle, text: string, font: s32): void {
    <Text session={session} props={{key: key, bounds: bounds, text: text, size: font,
        class_name: font >= s32(20) ? s32(11003) : s32(11002), align: TextAlign.TextAlignCenter}}/>;
}

export function SitePanel(session: Session, key: u64, bounds: Rectangle): void {
    <Box session={session} props={{key: key, bounds: bounds, fill: State.SiteBackground(),
        border: State.SiteBorder(), radius: 14}}/>;
}

export function SiteAction(session: Session, key: u64, bounds: Rectangle, label: string, page: s32): void {
    if (Button({session: session, props: {key: key, bounds: bounds, label: label, class_name: s32(11010),
        emphasis: ButtonEmphasis.ButtonEmphasisGhost, selected: State.site_page === page,
        disabled: State.site_modal_active}})) {
        State.SiteNavigate(page);
    }
}
