import {CardVariant} from "kryon/card";
import {Rectangle} from "kryon/geometry";
import {ImageFit} from "kryon/image";
import {OpenUri} from "kryon/open_uri";
import {ScrollResult} from "kryon/scroll";
import {Session} from "kryon/session";
import {Box} from "kryon/tsx/box";
import {Button} from "kryon/tsx/button";
import {Card} from "kryon/tsx/card";
import {ConfirmButton} from "kryon/tsx/confirm_button";
import {HoldButton} from "kryon/tsx/hold_button";
import {Image} from "kryon/tsx/image";
import {Scroll} from "kryon/tsx/scroll";
import {Slider} from "kryon/tsx/slider";
import {Text} from "kryon/tsx/text";
import {TextArea} from "kryon/tsx/text_area";
import {Toggle} from "kryon/tsx/toggle";
import * as State from "site_state";
import {SiteAction, SiteCenter, SiteMuted, SitePanel, SiteText} from "site_widgets";

interface OverviewCardProps {
    session: Session;
    bounds: Rectangle;
}

function CounterCard(card: OverviewCardProps): void {
    const session = card.session;
    const x = card.bounds.x + 20.0;
    const y = card.bounds.y;
    const width: float32 = card.bounds.width - 40.0;
    <Card session={session} props={{key: u64(10036n), bounds: card.bounds, variant: CardVariant.CardOutlined}}
        children={(): void => {
        SiteText(session, u64(10037n), {x: x, y: y + 20.0, width: width, height: 32}, "Every click counts", s32(20));
        if (Button({session: session, props: {key: u64(10038n), bounds: {x: x, y: y + 72.0, width: width, height: 42},
            label: State.SiteFormatCount()}})) { nativeSet(nativeRef(State.site_count), State.site_count + s32(1)); }
        const notifications = Toggle({session: session, props: {key: u64(10039n),
            bounds: {x: x, y: y + 136.0, width: width, height: 40}, label: "Notifications", value: State.site_notifications}});
        nativeSet(nativeRef(State.site_notifications), notifications.value);
        const volume = Slider({session: session, props: {key: u64(10040n),
            bounds: {x: x, y: y + 188.0, width: width, height: 48}, label: "Volume", value: State.site_volume, max: 100}});
        nativeSet(nativeRef(State.site_volume), volume.value);
    }}/>;
}

function IntentCard(card: OverviewCardProps): void {
    const session = card.session;
    const x = card.bounds.x + 20.0;
    const y = card.bounds.y;
    const width: float32 = card.bounds.width - 40.0;
    <Card session={session} props={{key: u64(10041n), bounds: card.bounds, variant: CardVariant.CardOutlined}}
        children={(): void => {
        SiteText(session, u64(10042n), {x: x, y: y + 20.0, width: width, height: 32}, "Actions with intent", s32(20));
        const held = HoldButton({widget: {session: session, props:{button: {key: u64(10043n),
            bounds: {x: x, y: y + 72.0, width: width, height: 42}, label: "Hold to complete"},
            now: State.site_now, duration: 1.2, key: State.site_key, key_held: State.site_space_held,
            blur: !State.site_focused}}, state: nativeRef(State.site_hold)});
        if (held.fired) { nativeSet(nativeRef(State.site_hold_count), State.site_hold_count + s32(1)); }
        const confirmed = ConfirmButton({widget: {session: session, props:{button: {key: u64(10044n),
            bounds: {x: x, y: y + 136.0, width: width, height: 42}, label: "Delete"},
            confirm_button: {label: "Confirm delete"}, now: State.site_now, key: State.site_key}},
            state: nativeRef(State.site_confirm)});
        if (confirmed.committed) { nativeSet(nativeRef(State.site_confirm_count), State.site_confirm_count + s32(1)); }
        SiteText(session, u64(10045n), {x: x, y: y + 196.0, width: width, height: 34}, "Hold, release, confirm or Escape.", s32(13));
    }}/>;
}

export function SiteOverview(session: Session, view: Rectangle): void {
    const large = view.width >= 760.0;
    const inset: float32 = large ? 96.0 : 40.0;
    let width = view.width - inset;
    if (width > 1180.0) { width = 1180.0; }
    const x = (view.width - width) * 0.5;
    <Scroll session={session} props={{key: u64(10030n), bounds: view, content_height: large ? s32(1840) : s32(2200),
        scroll_offset: State.site_scroll}} children={(scroll: ScrollResult): void => {
        nativeSet(nativeRef(State.site_scroll), scroll.scroll_offset);
        const start: float32 = large ? 60.0 : 32.0;
        let y: float32 = scroll.content.y + start;
        <Box session={session} props={{key: u64(10200n),
            bounds: {y: scroll.content.y, width: view.width, height: large ? 820.0 : 720.0},
            fill: State.SiteSurface(), fill_bottom: State.SiteBackground(), has_fill_bottom: true}}/>;
        const mascotSize: float32 = large ? 180.0 : 140.0;
        <Image session={session} props={{key: u64(10201n), asset_path: "assets/brand/mascot-dragon.png",
            alt_text: "Kryon pastel dragon mascot",
            bounds: {x: (view.width - mascotSize) * 0.5, y: y, width: mascotSize, height: mascotSize},
            fit: ImageFit.ImageFitContain, smooth: true}}/>;
        const mascotGap: float32 = large ? 20.0 : 16.0;
        y += mascotSize + mascotGap;
        SiteCenter(session, u64(10031n), {x: x, y: y, width: width, height: large ? 140.0 : 104.0},
            large ? "Native interfaces.\nOne expressive language." : "Native interfaces.\nWritten in Ziran.", large ? s32(56) : s32(36));
        y += large ? float32(160.0) : float32(126.0);
        SiteCenter(session, u64(10032n), {x: x, y: y, width: width, height: 64},
            "Native TSX screens with Ziran logic.\nUsed in Terminal, Explorer, and Workbook.", large ? s32(18) : s32(16));
        y += 88.0;
        const actions: float32 = width > 360.0 ? 360.0 : width;
        const ax = (view.width - actions) * 0.5;
        if (Button({session: session, props: {key: u64(10033n),
            bounds: {x: ax, y: y, width: actions * 0.5 - 6.0, height: 42}, label: "Explore modules", class_name: s32(11011)}})) {
            State.SiteNavigate(s32(6));
        }
        if (Button({session: session, props: {key: u64(10034n),
            bounds: {x: ax + actions * 0.5 + 6.0, y: y, width: actions * 0.5 - 6.0, height: 42},
            label: "See the apps", class_name: s32(11012)}})) { State.SiteNavigate(s32(3)); }
        y += 64.0;
        SiteCenter(session, u64(10202n), {x: x, y: y, width: width, height: 24},
            "$ ziran add https://git.waozi.xyz/kryonlabs/kryon", large ? s32(13) : s32(10));
        y += 76.0;
        SiteMuted(session, u64(10035n), {x: x, y: y, width: width, height: 24}, "A SMALL TASTE OF KRYON", s32(12));
        y += 38.0;
        const column: float32 = large ? (width - 20.0) * 0.5 : width;
        <CounterCard session={session} bounds={{x: x, y: y, width: column, height: 250}}/>;
        if (!large) { y += 270.0; }
        <IntentCard session={session} bounds={{x: large ? x + column + 20.0 : x, y: y, width: column, height: 250}}/>;
        y += 306.0;
        <Box session={session} props={{key: u64(10204n), bounds: {x: x, y: y, width: width, height: 1}, fill: State.SiteBorder()}}/>;
        y += 36.0;
        SiteText(session, u64(10205n), {x: x, y: y, width: width, height: 76},
            "Everything starts with a component.", large ? s32(32) : s32(26));
        y += 92.0;
        SiteMuted(session, u64(10206n), {x: x, y: y, width: width, height: 76},
            "Controls, text, navigation, overlays, charts and data views.\nEvery example runs the real library, with the TSX source beside it.", s32(16));
        y += 110.0;
        const facts: [string, string, string] = ["Native components", "Easy theming", "Live TSX source"];
        const details: [string, string, string] = ["One maintained collection.", "Your colors, type and motion.", "Edit, compile, and interact."];
        const factWidth = width / 3.0;
        const fw: float32 = large ? factWidth - 20.0 : width;
        for (let i = s32(0); i < s32(3); i += s32(1)) {
            const fx: float32 = large ? x + float32(i) * factWidth : x;
            const fy: float32 = large ? y : y + float32(i) * 100.0;
            SiteText(session, u64(s32(10210) + i), {x: fx, y: fy, width: fw, height: 32}, facts[i], s32(22));
            SiteMuted(session, u64(s32(10220) + i), {x: fx, y: fy + 42.0, width: fw, height: 30}, details[i], s32(14));
        }
        y += large ? float32(150.0) : float32(340.0);
        SitePanel(session, u64(10230n), {x: x, y: y, width: width, height: 180});
        SiteCenter(session, u64(10231n), {x: x + 20.0, y: y + 28.0, width: width - 40.0, height: 40}, "Start with one window.", s32(28));
        SiteCenter(session, u64(10232n), {x: x + 20.0, y: y + 80.0, width: width - 40.0, height: 42},
            "A few lines of Ziran. An interface you can use.", s32(14));
        SiteAction(session, u64(10233n), {x: (view.width - 160.0) * 0.5, y: y + 130.0, width: 160, height: 36}, "Read the guide  →", s32(1));
        SiteMuted(session, u64(10234n), {x: x, y: y + 226.0, width: width, height: 32},
            "KRYON    /    OPEN SOURCE, WRITTEN IN ZIRAN", s32(12));
    }}/>;
}

const GUIDE_WIDGETS: string = "import * as App from \"kryon/app\";\nimport { Session } from \"kryon/session\";\nimport { Button } from \"kryon/tsx/button\";\n\nexport function Frame(frame: App.Frame): void {\n    const session: Session = {slot: frame.slot, epoch: frame.epoch};\n    <Button session={session} props={{key: u64(1n),\n        bounds: frame.viewport, label: \"Ready\"}}/>;\n}";

const GUIDE_THEME: string = "import * as Theme from \"kryon/theme\";\n\nexport function InstallPalette(): void {\n    let theme = Theme.DefaultTheme(false);\n    theme = Theme.WithAccent(theme,\n        u32(0x4338caff), u32(0xffffffff),\n        u32(0x312e81ff), u32(0xe0e7ffff));\n    Theme.InstallTheme(theme);\n}";

function GuideCode(session: Session, key: u64, bounds: Rectangle, code: string): void {
    <Box session={session} props={{key: key, bounds: bounds, fill: State.SiteSurface(), border: State.SiteBorder(), radius: 10}}/>;
    <TextArea session={session} props={{key: key + u64(1n),
        bounds: {x: bounds.x + 16.0, y: bounds.y + 14.0, width: bounds.width - 32.0, height: bounds.height - 28.0},
        value: code, read_only: true, wrap: false, manual_scroll: true,
        class_name: s32(11040), colors: State.SiteCodeColors(code)}}/>;
}

function SiteGuide(session: Session, view: Rectangle): void {
    const wide = view.width >= 1000.0;
    const inset: float32 = wide ? 96.0 : 40.0;
    let width = view.width - inset;
    if (width > 1120.0) { width = 1120.0; }
    const x = (view.width - width) * 0.5;
    const rail: float32 = wide ? 220.0 : 0.0;
    const ax = x + rail;
    const aw = width - rail;
    <Scroll session={session} props={{key: u64(10400n), bounds: view, content_height: s32(1900), scroll_offset: State.site_scroll}}
        children={(scroll: ScrollResult): void => {
        nativeSet(nativeRef(State.site_scroll), scroll.scroll_offset);
        let y: float32 = scroll.content.y + 40.0;
        if (wide) {
            SiteMuted(session, u64(10600n), {x: x, y: y, width: 180, height: 24}, "BUILD WITH KRYON", s32(11));
            const labels: [string, string, string] = ["01  Get started", "02  Use a component", "03  Make it yours"];
            for (let i = s32(0); i < s32(3); i += s32(1)) {
                if (Button({session: session, props: {key: u64(s32(10601) + i),
                    bounds: {x: x, y: y + 42.0 + float32(i) * 42.0, width: 190, height: 36}, label: labels[i], class_name: s32(11010)}})) {
                    nativeSet(nativeRef(State.site_scroll), i === s32(0) ? s32(0) : i === s32(1) ? s32(550) : s32(1110));
                }
            }
        }
        SiteMuted(session, u64(10610n), {x: ax, y: y, width: aw, height: 24}, "GUIDE / THE ESSENTIALS", s32(11));
        SiteText(session, u64(10050n), {x: ax, y: y + 42.0, width: aw, height: 96}, "From first window\nto your own style.", wide ? s32(42) : s32(30));
        SiteMuted(session, u64(10051n), {x: ax, y: y + 158.0, width: aw, height: 72},
            "Start small. Compose real controls.\nMake them yours with a shared theme.", s32(16));
        y += 264.0;
        SiteText(session, u64(10611n), {x: ax, y: y, width: aw, height: 38}, "01 / Get started", s32(24));
        SiteMuted(session, u64(10612n), {x: ax, y: y + 54.0, width: aw, height: 68},
            "With Ziran and the Kryon tool installed,\ncreate a project and run its native window.", s32(15));
        GuideCode(session, u64(10613n), {x: ax, y: y + 138.0, width: aw, height: 150}, "kryon new hello\ncd hello\nkryon run\nkryon run web");
        y += 332.0;
        <Box session={session} props={{key: u64(10615n), bounds: {x: ax, y: y, width: aw, height: 1}, fill: State.SiteBorder()}}/>;
        y += 36.0;
        SiteText(session, u64(10616n), {x: ax, y: y, width: aw, height: 38}, "02 / Use a component", s32(24));
        SiteMuted(session, u64(10617n), {x: ax, y: y + 54.0, width: aw, height: 82},
            "Import the widgets. Keep state in your app.\nA stable key preserves input and focus\nwhen you submit the next frame.", s32(15));
        GuideCode(session, u64(10618n), {x: ax, y: y + 150.0, width: aw, height: 244}, GUIDE_WIDGETS);
        if (Button({session: session, props: {key: u64(10620n), bounds: {x: ax, y: y + 410.0, width: aw, height: 42},
            label: "Explore editable examples  →", class_name: s32(11012)}})) { State.SiteNavigate(s32(2)); }
        y += 496.0;
        <Box session={session} props={{key: u64(10621n), bounds: {x: ax, y: y, width: aw, height: 1}, fill: State.SiteBorder()}}/>;
        y += 36.0;
        SiteText(session, u64(10622n), {x: ax, y: y, width: aw, height: 38}, "03 / Easy theming", s32(24));
        SiteMuted(session, u64(10623n), {x: ax, y: y + 54.0, width: aw, height: 90},
            "One theme carries colors, typography,\nspacing and motion across your controls.\nStart with a palette, then set your accent.", s32(15));
        GuideCode(session, u64(10624n), {x: ax, y: y + 158.0, width: aw, height: 206}, GUIDE_THEME);
        SiteMuted(session, u64(10626n), {x: ax, y: y + 384.0, width: aw, height: 100},
            "Use SetThemeMode for light or dark;\nreapply custom accents after a mode change.\nSetReducedMotion controls motion.", s32(14));
        if (Button({session: session, props: {key: u64(10052n), bounds: {x: ax, y: y + 508.0, width: aw, height: 42},
            label: "Read the theme API  ↗", class_name: s32(11012)}})) {
            OpenUri("https://git.waozi.xyz/kryonlabs/kryon/src/branch/master/src/ui/theme.zi");
        }
    }}/>;
}

export function SiteReading(session: Session, view: Rectangle): void {
    if (State.site_page === s32(1)) { SiteGuide(session, view); return; }
    let title = "Documentation";
    let body = "";
    if (State.site_page === s32(4)) {
        body = "Kryon owns widget composition, behavior, accessibility and UI policy.\n\nZiran owns the language, compiler and runtime.\n\nApplications own their screens, workflows and state.\n\nScreen authoring: TSX components in examples/shared/screens/*.tsx\nPublic policy APIs: src/ui/*.zi\nBuild inventory: src/ui/modules.txt\nProject integration: docs/PROJECTS.md\nBackend integration: docs/BACKENDS.md\n\nStart a project\nkryon new hello\ncd hello\nkryon run";
    } else if (State.site_page === s32(5)) {
        title = "Platforms";
        body = "Desktop\nThe native gallery uses the canonical desktop host.\n\nBrowser\nThis page runs compiled TSX/Ziran through the Canvas host. Its controls use the same Kryon widget implementations.\n\nPortable\nKryon can host .zx applications through explicit capabilities.\n\nAndroid\nDevice validation is tracked separately.";
    } else if (State.site_page === s32(6)) {
        title = "Modules";
        body = "Kryon\n82 controls, text, layout and navigation examples.\n\nPlot\n6 chart examples.\n\nDataViews\n3 tree, board and table examples.\n\nInputs\n6 focused editing examples.\n\nOverlays\n6 overlay examples.\n\nAll 103 examples are available from Components.";
    }
    let width: float32 = view.width - 40.0;
    if (width > 840.0) { width = 840.0; }
    const x = (view.width - width) * 0.5;
    <Scroll session={session} props={{key: u64(10400n), bounds: view, content_height: s32(1080), scroll_offset: State.site_scroll}}
        children={(scroll: ScrollResult): void => {
        nativeSet(nativeRef(State.site_scroll), scroll.scroll_offset);
        const y = scroll.content.y + 44.0;
        SiteMuted(session, u64(10401n), {x: x, y: y, width: width, height: 24},
            State.site_page === s32(6) ? "KRYON  /  MODULES" : "KRYON  /  DOCUMENTATION", s32(12));
        SiteText(session, u64(10050n), {x: x, y: y + 48.0, width: width, height: 92}, title, s32(36));
        <Box session={session} props={{key: u64(10402n), bounds: {x: x, y: y + 162.0, width: width, height: 1}, fill: State.SiteBorder()}}/>;
        <Text session={session} props={{key: u64(10051n), bounds: {x: x, y: y + 198.0, width: width, height: 640},
            text: body, size: s32(17), line_gap: s32(8), class_name: s32(11002), selectable: true}}/>;
        if (Button({session: session, props: {key: u64(10052n), bounds: {x: x, y: y + 900.0, width: width, height: 42},
            label: "Open source & API", class_name: s32(11012)}})) {
            OpenUri("https://git.waozi.xyz/kryonlabs/kryon/src/branch/master/docs/API.md");
        }
    }}/>;
}
