import * as App from "kryon/app";
import * as Styles from "responsive_styles";
import * as Layout from "kryon/layout";
import * as NativeGrid from "kryon/grid_props";
import * as NativeCard from "kryon/card";
import * as NativeText from "kryon/text";
import * as NativeScroll from "kryon/scroll";
import { ColorFromPacked } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { ResolvedCard } from "kryon/tsx/card";
import { MeasuredGrid } from "kryon/tsx/grid";
import { Column } from "kryon/tsx/layout";
import { Heading } from "kryon/tsx/page";
import { Scroll } from "kryon/tsx/scroll";
import { Text } from "kryon/tsx/text";

let showCosts: boolean = false;
let ledgerOffset: s32 = s32(0);

function Label(session: Session, bounds: Rectangle, key: u64, text: string, className: string): void {
    <Text session={session} props={{bounds: bounds, key: key, text: text, class_name: Styles.Class(className), wrap: NativeText.Wrap.TextWrapNone}}/>;
}
function Metrics(session: Session): void {
    const items: [NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem] = [
        {height: s32(82)}, {height: s32(82)}, {height: s32(82)}, {height: s32(82)}];
    const names: [string, string, string, string] = ["Total return", "CAGR", "Benchmark", "Trades"];
    const values: [string, string, string, string] = ["+10.87%", "+8.14%", "+6.31%", "212"];
    let placements: [Rectangle, Rectangle, Rectangle, Rectangle] = [{}, {}, {}, {}];
    const positions = nativeSlice(placements);
    const labels = nativeSlice(names);
    const data = nativeSlice(values);
    <MeasuredGrid session={session} props={{key: u64(20), class_name: Styles.Class("metrics")}} items={nativeSlice(items)} itemBounds={positions}
        children={(_grid: Layout.GridResult): void => {
            for (let i: s32 = s32(0); i < s32(4); i += s32(1)) {
                const key = u64(s32(100) + i * s32(3));
                <ResolvedCard session={session} props={{bounds: nativeAt(positions, i), key: key, class_name: Styles.Class("panel"), variant: NativeCard.CardVariant.CardOutlined}}
                    children={(panel: NativeCard.CardResult): void => {
                        Label(session, {x: panel.content.x, y: panel.content.y, width: panel.content.width, height: 19}, key + u64(1), nativeAt(labels, i), "muted");
                        Label(session, {x: panel.content.x, y: panel.content.y + 20, width: panel.content.width, height: 28}, key + u64(2), nativeAt(data, i), "positive");
                    }}/>;
            }
        }}/>;
}
function ChartSections(session: Session): void {
    const items: [NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem] = [
        {height: s32(106), column_span: s32(-1)}, {height: s32(86)}, {height: s32(86)}];
    const titles: [string, string, string] = ["Equity versus benchmark", "Monthly returns", "Drawdown"];
    const descriptions: [string, string, string] = ["Shared time axis", "Signed monthly values", "Peak-to-trough values"];
    let placements: [Rectangle, Rectangle, Rectangle] = [{}, {}, {}];
    const positions = nativeSlice(placements);
    const labels = nativeSlice(titles);
    const details = nativeSlice(descriptions);
    <MeasuredGrid session={session} props={{key: u64(30), class_name: Styles.Class("charts")}} items={nativeSlice(items)} itemBounds={positions}
        children={(_grid: Layout.GridResult): void => {
            for (let i: s32 = s32(0); i < s32(3); i += s32(1)) {
                const key = u64(s32(200) + i * s32(3));
                <ResolvedCard session={session} props={{bounds: nativeAt(positions, i), key: key, class_name: Styles.Class("panel"), variant: NativeCard.CardVariant.CardOutlined}}
                    children={(panel: NativeCard.CardResult): void => {
                        <Heading session={session} props={{bounds: {x: panel.content.x, y: panel.content.y, width: panel.content.width, height: 24},
                            key: key + u64(1), text: nativeAt(labels, i), level: s32(2), class_name: Styles.Class("panel-title")}}/>;
                        Label(session, {x: panel.content.x, y: panel.content.y + 26, width: panel.content.width, height: 20}, key + u64(2), nativeAt(details, i), "muted");
                    }}/>;
            }
        }}/>;
}
function Ledger(session: Session): void {
    const width = Layout.ColumnWidth(session);
    const tableWidth = width < 640 ? float32(640) : width;
    const view = Scroll({session: session, props: {key: u64(41), bounds: {width: width, height: 75}, horizontal: true,
        content_width: s32(tableWidth), scroll_offset: ledgerOffset}, children: (opened: NativeScroll.ScrollResult): void => {
        const cells: [string, string, string, string, string, string, string, string] = [
            "Date", "Market", "Notional", "Net P&L", "2024-07-15", "EUR/USD", "4,000.00", "−12.40"];
        const items: [NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem,
            NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem, NativeGrid.GridItem] = [
            {height: s32(20)}, {height: s32(20)}, {height: s32(20)}, {height: s32(20)},
            {height: s32(20)}, {height: s32(20)}, {height: s32(20)}, {height: s32(20)}];
        let placements: [Rectangle, Rectangle, Rectangle, Rectangle, Rectangle, Rectangle, Rectangle, Rectangle] = [{}, {}, {}, {}, {}, {}, {}, {}];
        const positions = nativeSlice(placements);
        const labels = nativeSlice(cells);
        <MeasuredGrid session={session} props={{key: u64(42), class_name: Styles.Class("ledger"), bounds: {x: opened.content.x, y: opened.content.y, width: tableWidth}}}
            items={nativeSlice(items)} itemBounds={positions} children={(_grid: Layout.GridResult): void => {
                for (let i: s32 = s32(0); i < s32(8); i += s32(1)) {
                    Label(session, nativeAt(positions, i), u64(s32(300) + i), nativeAt(labels, i), i === s32(7) ? "negative" : "muted");
                }
            }}/>;
    }});
    ledgerOffset = view.scroll_offset;
}
export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    if (!Styles.Install(viewport)) { print("responsive styles failed\n"); return; }
    <Box session={session} props={{key: u64(1), bounds: viewport, fill: ColorFromPacked(u32(0x0d1420ff))}}/>;
    let bounds = viewport;
    if (bounds.width > 1480) { bounds.x = bounds.x + (bounds.width - 1480) / 2; bounds.width = 1480; }
    <Column session={session} props={{bounds: bounds, key: u64(2), class_name: Styles.Class("dashboard")}} children={(): void => {
        Label(session, {height: 20}, u64(3), "AUTOTRADE · OFFLINE RESEARCH", "eyebrow");
        <Heading session={session} props={{key: u64(4), text: "Strategy DNA", bounds: {height: 50}, level: s32(1), class_name: Styles.Class("title")}}/>;
        Metrics(session);
        ChartSections(session);
        <Heading session={session} props={{key: u64(40), text: "Trade ledger", bounds: {height: 24}, level: s32(2), class_name: Styles.Class("panel-title")}}/>;
        Ledger(session);
        if (Button({session: session, props: {key: u64(43), bounds: {width: 170, height: 32}, label: showCosts ? "Hide costs" : "Show costs", class_name: Styles.Class("costs")}})) {
            showCosts = !showCosts;
            print("dashboard input passed at width %\n", s32(viewport.width));
        }
        if (showCosts) { Label(session, {height: 20}, u64(44), "Fees included in net P&L", "muted"); }
    }}/>;
}
export function main(): s32 { return App.Run({title: "responsive dashboard"}, Frame); }
