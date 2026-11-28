import {ButtonEmphasis} from "kryon/control_props";
import {Rectangle} from "kryon/geometry";
import {ImageFit} from "kryon/image";
import {OpenUri} from "kryon/open_uri";
import {ScrollResult} from "kryon/scroll";
import {Session} from "kryon/session";
import {Box} from "kryon/tsx/box";
import {Button} from "kryon/tsx/button";
import {Image} from "kryon/tsx/image";
import {Scroll} from "kryon/tsx/scroll";
import {SiteApp, SiteDesktopApp, SiteDesktopAppCount, SiteWebApp, SiteWebAppCount} from "site_app_catalog";
import * as State from "site_state";
import {SiteMuted, SiteText} from "site_widgets";

interface AppCardProps {
    session: Session;
    key: u64;
    bounds: Rectangle;
    app: SiteApp;
    imageHeight: float32;
}

function AppCard(card: AppCardProps): void {
    const session = card.session;
    const key = card.key;
    const bounds = card.bounds;
    const app = card.app;
    <Box session={session} props={{key: key, bounds: bounds, fill: State.SiteSurface(),
        border: State.SiteBorder(), radius: 14}}/>;
    const inner = bounds.width - 24.0;
    const x: float32 = bounds.x + 12.0;
    const imageBounds: Rectangle = {x: x, y: bounds.y + 12.0, width: inner, height: card.imageHeight};
    <Box session={session} props={{key: key + u64(5n), bounds: imageBounds,
        fill: State.SiteBackground(), border: State.SiteBorder(), radius: 10}}/>;
    <Image session={session} props={{key: key + u64(1n), asset_path: app.image, alt_text: app.name,
        bounds: imageBounds, fit: ImageFit.ImageFitContain, radius: 10, smooth: true}}/>;
    const textY = bounds.y + card.imageHeight + 26.0;
    SiteText(session, key + u64(2n), {x: x, y: textY, width: inner, height: 32}, app.name, s32(22));
    SiteMuted(session, key + u64(3n), {x: x, y: textY + 36.0, width: inner, height: 40}, app.summary, s32(14));
    const footer: Rectangle = {x: x, y: bounds.y + bounds.height - 46.0, width: inner, height: 34};
    if (app.play !== "") {
        const playWidth = app.source !== "" ? (inner - 10.0) * 0.5 : inner;
        if (Button({session: session, props: {key: key + u64(4n),
            bounds: {x: footer.x, y: footer.y, width: playWidth, height: footer.height},
            label: "Play in browser  ↗", class_name: s32(11012)}})) { OpenUri(app.play); }
        if (app.source !== "" && Button({session: session, props: {key: key + u64(6n),
            bounds: {x: footer.x + playWidth + 10.0, y: footer.y, width: playWidth, height: footer.height},
            label: "Source  ↗", class_name: s32(11010), emphasis: ButtonEmphasis.ButtonEmphasisGhost}})) {
            OpenUri(app.source);
        }
        return;
    }
    SiteMuted(session, key + u64(7n), {x: x, y: textY + 80.0, width: inner, height: 40}, app.desktop_reason, s32(13));
    if (Button({session: session, props: {key: key + u64(4n), bounds: footer,
        label: "View source  ↗", class_name: s32(11010), emphasis: ButtonEmphasis.ButtonEmphasisGhost}})) {
        OpenUri(app.source);
    }
}

export function SiteApps(session: Session, view: Rectangle): void {
    const wide = view.width >= 900.0;
    const outside: float32 = view.width >= 760.0 ? 96.0 : 40.0;
    let width = view.width - outside;
    if (width > 1120.0) { width = 1120.0; }
    const x: float32 = (view.width - width) * 0.5;
    const columns = view.width >= 1200.0 ? s32(3) : wide ? s32(2) : s32(1);
    const gap: float32 = 20.0;
    const cardWidth: float32 = (width - gap * float32(columns - s32(1))) / float32(columns);
    const aspect: float32 = columns > s32(1) ? 0.58 : 0.6;
    const imageHeight = cardWidth * aspect;
    const webHeight: float32 = imageHeight + 168.0;
    const desktopHeight: float32 = imageHeight + 214.0;
    const webRows = (s32(SiteWebAppCount) + columns - s32(1)) / columns;
    const desktopRows = (s32(SiteDesktopAppCount) + columns - s32(1)) / columns;
    const contentHeight = 240.0 + 56.0 + float32(webRows) * (webHeight + gap) +
        218.0 + 56.0 + float32(desktopRows) * (desktopHeight + gap) + 96.0;
    <Scroll session={session} props={{key: u64(10700n), bounds: view, content_height: s32(contentHeight),
        scroll_offset: State.site_scroll}} children={(scroll: ScrollResult): void => {
        nativeSet(nativeRef(State.site_scroll), scroll.scroll_offset);
        let y: float32 = scroll.content.y + 44.0;
        SiteMuted(session, u64(10701n), {x: x, y: y, width: width, height: 24}, "KRYON LABS  /  APPS", s32(12));
        y += 42.0;
        SiteText(session, u64(10702n), {x: x, y: y, width: width, height: 52}, "Apps built with Kryon", wide ? s32(42) : s32(32));
        y += 62.0;
        SiteMuted(session, u64(10703n), {x: x, y: y, width: width, height: 52},
            "Explore the desktop apps built with Kryon modules.\nTry the browser builds below.", s32(16));
        y += 92.0;
        SiteText(session, u64(10708n), {x: x, y: y, width: width, height: 36}, "Desktop apps from Kryon Labs", s32(26));
        y += 56.0;
        for (let i = s32(0); i < s32(SiteDesktopAppCount); i += s32(1)) {
            <AppCard session={session} key={u64(s32(10800) + i * s32(10))} app={SiteDesktopApp(i)} imageHeight={imageHeight}
                bounds={{x: x + float32(i % columns) * (cardWidth + gap),
                    y: y + float32(i / columns) * (desktopHeight + gap), width: cardWidth, height: desktopHeight}}/>;
        }
        y += float32(desktopRows) * (desktopHeight + gap) + 28.0;
        SiteText(session, u64(10704n), {x: x, y: y, width: width, height: 36}, "Play in your browser", s32(26));
        y += 56.0;
        for (let i = s32(0); i < s32(SiteWebAppCount); i += s32(1)) {
            <AppCard session={session} key={u64(s32(10710) + i * s32(10))} app={SiteWebApp(i)} imageHeight={imageHeight}
                bounds={{x: x + float32(i % columns) * (cardWidth + gap),
                    y: y + float32(i / columns) * (webHeight + gap), width: cardWidth, height: webHeight}}/>;
        }
        y += float32(webRows) * (webHeight + gap) + 28.0;
        SiteText(session, u64(10705n), {x: x, y: y, width: width, height: 36}, "Examples", s32(26));
        SiteMuted(session, u64(10706n), {x: x, y: y + 48.0, width: width, height: 52},
            "The component examples run live on the Components page,\nwith their TSX source ready to edit and run again.", s32(15));
        if (Button({session: session, props: {key: u64(10707n), bounds: {x: x, y: y + 118.0, width: 260, height: 40},
            label: "Open the examples  →", class_name: s32(11012)}})) { State.SiteNavigate(s32(2)); }
        y += 198.0;
        if (Button({session: session, props: {key: u64(10790n), bounds: {x: x, y: y, width: 232, height: 40},
            label: "Kryon Labs repositories  ↗", class_name: s32(11012)}})) { OpenUri("https://git.waozi.xyz/kryonlabs"); }
    }}/>;
}
