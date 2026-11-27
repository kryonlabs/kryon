// {{name}}: a Kryon application with three pages behind a tab row. Each
// page is a component; the state its pages share lives in src/library.zi,
// so a setting changed on one page shows on another.
//
//   kryon run          the desktop window
//   kryon run tui      the same pages in a terminal
//   kryon run web      a Canvas2D browser page (needs Emscripten)
import * as App from "kryon/app";
import { ButtonEmphasis, ButtonTone } from "kryon/control_props";
import { Rectangle } from "kryon/geometry";
import { Props as ButtonProps } from "kryon/button";
import { Props as CheckboxProps } from "kryon/checkbox";
import { Props as ListBoxProps } from "kryon/list_box";
import { RowPart, FontSize, NextRow } from "kryon/PageLayout";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { Checkbox } from "kryon/tsx/checkbox";
import { ListBox } from "kryon/tsx/list_box";
import { Text } from "kryon/tsx/text";
import * as Model from "library";
import { PageLibrary } from "library";

// One row-sized line of text.
interface LabelProps { session: Session; text: string; bounds: Rectangle; size: s32; }

function Label(label: LabelProps): void {
    <Text session={label.session} props={{text: label.text, bounds: label.bounds,
        size: label.size}}/>;
}

// One tab in the tab row; the selected tab draws filled.
interface TabProps { session: Session; row: Rectangle; index: s32; label: string; }

function Tab(tab: TabProps): void {
    const props: ButtonProps = {id: s32(10) + tab.index,
        bounds: RowPart(Model.Page(), tab.row, tab.index, s32(3)), label: tab.label,
        selected: Model.Current() === tab.index};
    if (Model.Current() === tab.index) {
        props.tone = ButtonTone.ButtonToneAccent;
        props.emphasis = ButtonEmphasis.ButtonEmphasisFilled;
    }
    const pressed = Button({session: tab.session, props: props});
    if (pressed) { Model.Show(tab.index); }
}

// The welcome page: two lines and a button that opens the library.
interface HomeProps { session: Session; }

function Home(home: HomeProps): void {
    Label({session: home.session, text: "Welcome",
        bounds: NextRow(Model.Page(), 30.0), size: FontSize(Model.Page(), s32(24))});
    Label({session: home.session,
        text: "Pick a book in the library. Settings change what it shows.",
        bounds: NextRow(Model.Page(), 22.0), size: FontSize(Model.Page(), s32(16))});
    const open: ButtonProps = {id: s32(22), bounds: RowPart(Model.Page(),
        NextRow(Model.Page(), 40.0), s32(0), s32(2)), label: "Open the library",
        tone: ButtonTone.ButtonToneAccent, emphasis: ButtonEmphasis.ButtonEmphasisFilled};
    const pressed = Button({session: home.session, props: open});
    if (pressed) { Model.Show(PageLibrary); }
}

// The library page: the visible books in a list, and what is selected.
interface LibraryProps { session: Session; }

function Library(library: LibraryProps): void {
    const cursor = Model.Page();
    const books = Model.VisibleBooks();
    const props: ListBoxProps = {id: s32(30), bounds: NextRow(cursor, 6.0 * 34.0),
        row_height: s32(34)};
    if (cursor.cells) {
        // Kryon measures the list in pixels; a cell is eight of them.
        props.bounds.height = 6.0;
        props.row_height = s32(8);
        props.scale = 0.125;
        cursor.y = cursor.y + 5.0;
    }
    props.selected_index = Model.Selection();
    props.scroll_offset = Model.Scroll();
    const result = ListBox({session: library.session, props: props, items: books});
    Model.SetSelection(result.selected_index);
    Model.SetScroll(result.scroll_offset);
    if (Model.BookCount() > s32(0)) {
        Label({session: library.session, text: Model.SelectedSummary(),
            bounds: NextRow(cursor, 22.0), size: FontSize(cursor, s32(16))});
    }
}

// The settings page: two checkboxes that change what the library shows.
interface SettingsProps { session: Session; }

function Settings(settings: SettingsProps): void {
    const favorites: CheckboxProps = {id: s32(40), bounds: NextRow(Model.Page(), 32.0),
        label: "Show favorites only", checked: Model.FavoritesOnly()};
    const favoritesChanged = Checkbox({session: settings.session, props: favorites});
    if (favoritesChanged.changed) { Model.SetFavoritesOnly(favoritesChanged.checked); }
    const authors: CheckboxProps = {id: s32(41), bounds: NextRow(Model.Page(), 32.0),
        label: "Show authors", checked: Model.ShowAuthors()};
    const authorsChanged = Checkbox({session: settings.session, props: authors});
    if (authorsChanged.changed) { Model.SetShowAuthors(authorsChanged.checked); }
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    Model.StartPage(frame.viewport);
    const cursor = Model.Page();
    Label({session: session, text: "{{name}}", bounds: NextRow(cursor, 36.0),
        size: FontSize(cursor, s32(28))});
    const tabs = NextRow(cursor, 40.0);
    <Tab session={session} row={tabs} index={s32(0)} label={"Home"}/>;
    <Tab session={session} row={tabs} index={s32(1)} label={"Library"}/>;
    <Tab session={session} row={tabs} index={s32(2)} label={"Settings"}/>;
    if (Model.Current() === s32(0)) { Home({session: session}); }
    if (Model.Current() === s32(1)) { Library({session: session}); }
    if (Model.Current() === s32(2)) { Settings({session: session}); }
}

export function main(): s32 {
    return App.Run({title: "{{name}}"}, Frame);
}
