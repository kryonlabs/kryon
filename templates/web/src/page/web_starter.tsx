// The Web starter's frame, shared by all app profiles.
import * as App from "kryon/app";
import { ButtonEmphasis, ButtonTone } from "kryon/control_props";
import { Props as ButtonProps } from "kryon/button";
import { Rectangle } from "kryon/geometry";
import { NextRow, RowPart } from "kryon/PageLayout";
import { SemanticKind } from "kryon/semantic";
import { Session } from "kryon/session";
import { Wrap } from "kryon/text";
import { Button } from "kryon/tsx/button";
import { Link } from "kryon/tsx/link";
import { Text } from "kryon/tsx/text";
import * as Model from "counter";

// One heading line, marked for the accessibility tree.
interface HeadingProps { session: Session; text: string; level: s32;
    bounds: Rectangle; size: s32; }

function Heading(heading: HeadingProps): void {
    <Text session={heading.session} props={{bounds: heading.bounds, text: heading.text,
        size: heading.size, semantic_kind: SemanticKind.SemanticHeading,
        heading_level: heading.level}}/>;
}

// One wrapped paragraph, marked for the accessibility tree.
interface ParagraphProps { session: Session; text: string; bounds: Rectangle; size: s32; }

function Paragraph(paragraph: ParagraphProps): void {
    <Text session={paragraph.session} props={{bounds: paragraph.bounds,
        text: paragraph.text, size: paragraph.size,
        semantic_kind: SemanticKind.SemanticParagraph, wrap: Wrap.TextWrapAuto}}/>;
}

// One clickable link.
interface WebLinkProps { session: Session; text: string; url: string; bounds: Rectangle; }

function WebLink(link: WebLinkProps): void {
    <Link session={link.session} props={{bounds: link.bounds, text: link.text,
        link: link.url}}/>;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const cursor = Model.Page();
    Model.StartPage(frame.viewport);
    Heading({session: session, text: "{{name}}", level: s32(1),
        bounds: NextRow(cursor, 44.0), size: s32(34)});
    Paragraph({session: session,
        text: "This page is a Kryon application. The same Ziran source runs on a canvas and in a desktop window.",
        bounds: NextRow(cursor, 48.0), size: s32(17)});
    Heading({session: session, text: "Why Kryon", level: s32(2),
        bounds: NextRow(cursor, 30.0), size: s32(22)});
    Paragraph({session: session,
        text: "One interface, declared every frame from your state. No HTML templates and no JavaScript.",
        bounds: NextRow(cursor, 48.0), size: s32(17)});
    WebLink({session: session, text: "Kryon on GitHub",
        url: "https://github.com/kryonlabs/kryon", bounds: NextRow(cursor, 24.0)});
    WebLink({session: session, text: "The Ziran language",
        url: "https://github.com/ziranlang/ziran", bounds: NextRow(cursor, 24.0)});
    const like: ButtonProps = {id: s32(7), bounds: RowPart(cursor,
        NextRow(cursor, 40.0), s32(0), s32(3)), label: "Like",
        tone: ButtonTone.ButtonToneAccent, emphasis: ButtonEmphasis.ButtonEmphasisFilled};
    const pressed = Button({session: session, props: like});
    if (pressed) { Model.Like(); }
    Paragraph({session: session, text: Model.LikesText(),
        bounds: NextRow(cursor, 24.0), size: s32(16)});
}
