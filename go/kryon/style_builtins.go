package kryon

const materialStyleSource = `@pack material;

tokens {
  color {
    canvas: #101217;
    text: #f5f2ff;
    muted: #8d919a;
    surface: #151922;
    card: #1a1f29;
    panel: #202631;
    panel-hover: #2a3140;
    panel-pressed: #181d26;
    border: #3a4250;
    border-soft: #2a303b;
    border-hover: #4a5363;
    focus: #c9a8ff;
    accent: #c9a8ff;
    accent-hover: #d5bbff;
    accent-ink: #171022;
    danger: #f07178;
    danger-ink: #220b0d;
    danger-border: #ff9da2;
    transparent: #00000000;
  }
  length {
    radius.sm: 6;
    radius.md: 8;
    radius.lg: 10;
    border: 1;
    border.none: 0;
    space.2: 8;
    space.3: 12;
    space.4: 16;
    space.5: 20;
    pad.control-y: 10;
    pad.field-y: 9;
    font.sm: 14;
    font.md: 16;
    font.lg: 18;
    icon.md: 20;
  }
  material {
    default: Flat;
  }
}

@layer base;

App {
  background: canvas;
  foreground: text;
  material: default;
}

Surface {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: 14;
  gap: space.3;
  material: default;
}

Popup {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Popup[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Canvas {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Card {
  background: card;
  foreground: text;
  border: #343b48;
  radius: radius.md;
  border-width: border;
  padding-x: 18;
  padding-y: space.4;
  gap: space.3;
  material: default;
}

Image {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Image[role=Label] {
  foreground: muted;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Focus {
  foreground: focus;
  border: focus;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Focus:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Box] {
  foreground: muted;
  border: border-hover;
  border-width: 2;
  padding-x: 3;
  opacity: 1;
  material: default;
}

Focus[role=Box]:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Label] {
  foreground: muted;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

Focus[role=Label]:focus {
  foreground: focus;
}

NavigationBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.lg;
  border-width: border;
  padding-x: space.4;
  padding-y: space.3;
  gap: space.3;
  opacity: 1;
  material: default;
}

NavigationBar[role=Panel] {
  offset-x: 340;
  icon-size: 128;
  padding-y: 58;
}

NavigationBar[role=Row] {
  icon-size: 58;
  offset-y: 22;
  padding-x: 36;
  padding-y: 36;
}

NavigationBar[role=Action] {
  icon-size: 34;
  offset-x: 180;
  offset-y: 16;
  padding-x: 92;
  padding-y: 36;
  gap: 8;
}

NavigationBar[role=Divider] {
  icon-size: 48;
  offset-y: 8;
  padding-y: 12;
  gap: 8;
}

NavigationBarItem {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.lg;
  border-width: border.none;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

NavigationBarItem:hover {
  background: panel;
  foreground: text;
}

NavigationBarItem:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

NavigationBarItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 72;
  icon-size: 48;
  offset-x: 168;
  gap: space.2;
  opacity: 1;
  material: default;
}

Tab {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.lg;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 44;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Tab:hover {
  background: panel;
  foreground: text;
}

Tab:pressed {
  background: panel-pressed;
  foreground: text;
}

Tab:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Tab:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabClose {
  background: transparent;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 24;
  opacity: 1;
  material: default;
}

TabClose:hover {
  background: panel;
  foreground: text;
}

SegmentedControl {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 2;
  padding-y: 2;
  icon-size: 30;
  offset-x: 72;
  offset-y: 180;
  gap: space.2;
  opacity: 1;
  material: default;
}

Segment {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

Segment:hover {
  background: panel;
  foreground: text;
}

Segment:pressed {
  background: panel-pressed;
  foreground: text;
}

Segment:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Segment:disabled {
  foreground: muted;
  opacity: 0.45;
}

Menu {
  background: surface;
  foreground: text;
  border: border;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: space.2;
  opacity: 1;
  material: default;
}

Menu[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  padding-x: 12;
  padding-y: 3;
  gap: 2;
}

Menu[role=Popup] {
  background: card;
  foreground: text;
  border: border;
  padding-x: 12;
  gap: 4;
  offset-x: 180;
}

Menu[role=Context] {
  background: card;
  foreground: text;
  border: border;
}

MenuItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  offset-x: 88;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

MenuItem:hover {
  background: panel-hover;
  foreground: text;
}

MenuItem:pressed {
  background: panel-pressed;
  foreground: text;
}

MenuItem:selected {
  background: panel-hover;
  foreground: text;
}

MenuItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

MenuSeparator {
  background: transparent;
  foreground: border-soft;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  gap: 4;
  icon-size: 32;
  offset-x: 60;
  opacity: 1;
  material: default;
}

Spinbox {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

SpinboxValue {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Drag {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  font-size: font.sm;
  padding-x: 6;
  padding-y: 4;
  opacity: 1;
  material: default;
}

DragValue {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

DragValue:disabled {
  background: surface;
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

ColorPicker {
  gap: 4;
  icon-size: 36;
  padding-y: 2;
  offset-x: 20;
  offset-y: 28;
}

ColorPickerSwatch {
  background: transparent;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.2;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBox {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  offset-y: 8;
  gap: space.2;
  opacity: 1;
  material: default;
}

ListBoxItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 30;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBoxItem:hover {
  background: panel-hover;
  foreground: text;
}

ListBoxItem:selected {
  background: accent;
  foreground: accent-ink;
}

ListBoxItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TreeView {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: space.2;
  opacity: 1;
  material: default;
}

TreeViewItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  gap: 2;
  icon-size: 16;
  offset-x: 18;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TreeViewItem:hover {
  background: panel-hover;
  foreground: text;
}

TreeViewItem:selected {
  background: accent;
  foreground: accent-ink;
}

TreeViewItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

ListBoxMulti {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: space.2;
  opacity: 1;
  material: default;
}

ListBoxMultiItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 28;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBoxMultiItem:hover {
  background: panel-hover;
  foreground: text;
}

ListBoxMultiItem:selected {
  background: accent;
  foreground: accent-ink;
}

ListBoxMultiItem:focus {
  border: focus;
}

ListBoxMultiItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

DragDropTarget {
  background: transparent;
  border: border-hover;
  focus: focus;
  radius: radius.md;
  border-width: border;
  material: default;
}

DragDropTarget:hover {
  border: accent;
  border-width: 2;
}

Selectable {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.md;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.2;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Selectable:hover {
  background: panel;
  foreground: text;
}

Selectable:pressed {
  background: panel-pressed;
  foreground: text;
}

Selectable:selected {
  background: accent;
  foreground: accent-ink;
}

Selectable:disabled {
  foreground: muted;
  opacity: 0.45;
}

Fieldset {
  background: canvas;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 8;
  padding-y: 8;
  gap: 9;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

PanedView {
  background: transparent;
  foreground: muted;
  border: transparent;
  opacity: 1;
  material: default;
}

PanedView[role=Handle] {
  background: border-soft;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 8;
  opacity: 1;
  material: default;
}

PanedView[role=Handle]:hover {
  background: border-hover;
}

PanedView[role=Handle]:pressed {
  background: accent;
}

Toast {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 14;
  padding-y: 10;
  gap: 18;
  opacity: 0.98;
  material: default;
}

Toast[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible[role=Header] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  font-size: font.md;
  icon-size: 20;
  padding-x: 8;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=Header]:hover {
  background: panel-hover;
}

Collapsible[role=Header]:pressed {
  background: panel-pressed;
}

Collapsible[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Collapsible[role=Header]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=TreeHeader] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  font-size: font.md;
  icon-size: 20;
  padding-x: 20;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=TreeHeader]:hover {
  background: panel;
}

Collapsible[role=TreeHeader]:selected {
  background: transparent;
  foreground: accent;
}

Collapsible[role=TreeHeader]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=Close] {
  background: transparent;
  foreground: muted;
  border: transparent;
  font-size: font.md;
  icon-size: 28;
  opacity: 1;
  material: default;
}

Collapsible[role=Close]:hover {
  foreground: text;
}

Collapsible[role=Close]:disabled {
  opacity: 0.45;
}

TitleBar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

TitleBar[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  opacity: 1;
  material: default;
}

TitleBar[role=Title] {
  background: transparent;
  foreground: text;
  border: transparent;
  font-size: font.lg;
  icon-size: 48;
  padding-x: 16;
  opacity: 1;
  material: default;
}

TitleBar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 20;
  padding-x: 10;
  opacity: 1;
  material: default;
}

TitleBar[role=Action]:hover {
  background: panel;
}

TitleBar[role=Action]:pressed {
  background: panel-pressed;
}

Toolbar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Toolbar[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  opacity: 1;
  material: default;
}

Toolbar[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: default;
}

Toolbar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  gap: 0;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Toolbar[role=Action]:hover {
  background: panel;
}

Toolbar[role=Action]:pressed {
  background: panel-pressed;
}

Modal {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Modal[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.62;
  material: default;
}

Modal[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 18;
  padding-y: 18;
  gap: 24;
  icon-size: 280;
  offset-x: 420;
  offset-y: 58;
  opacity: 1;
  material: default;
}

Modal[role=Title] {
  foreground: text;
  font-size: font.lg;
  padding-x: 24;
  padding-y: 18;
  icon-size: 48;
  offset-y: 14;
  opacity: 1;
  material: default;
}

Modal[role=Message] {
  foreground: text;
  font-size: font.md;
  padding-x: 120;
  padding-y: 160;
  gap: 18;
  icon-size: 38;
  offset-y: 18;
  opacity: 1;
  material: default;
}

Modal[role=Action] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: 24;
  gap: 8;
  icon-size: 44;
  offset-x: 88;
  offset-y: 150;
  opacity: 1;
  material: default;
}

Modal[role=Action]:hover {
  background: panel-hover;
}

Modal[role=Action]:pressed {
  background: panel-pressed;
}

Modal[role=Action][tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Modal[role=Action][tone=Danger] {
  background: danger;
  foreground: danger-ink;
  border: danger-border;
}

Modal[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  padding-y: 16;
  gap: 6;
  icon-size: 20;
  offset-x: 120;
  offset-y: 96;
  opacity: 1;
  material: default;
}

Modal[role=Close]:hover {
  background: panel-hover;
}

Modal[role=Close]:pressed {
  background: panel-pressed;
}

Guide {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Guide[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.34;
  material: default;
}

Guide[role=Bar] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border.none;
  padding-x: 12;
  gap: 12;
  icon-size: 48;
  offset-x: 48;
  opacity: 1;
  material: default;
}

Guide[role=Divider] {
  background: transparent;
  foreground: text;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: default;
}

Guide[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 12;
  padding-y: 12;
  gap: 20;
  offset-x: 300;
  offset-y: 112;
  opacity: 1;
  material: default;
}

Guide[role=Anchor] {
  background: transparent;
  foreground: accent;
  border: accent;
  border-width: border;
  padding-x: 4;
  opacity: 1;
  material: default;
}

Guide[role=Label] {
  foreground: text;
  font-size: font.md;
  padding-x: 8;
  padding-y: 6;
  gap: 8;
  opacity: 1;
  material: default;
}

Guide[role=Action] {
  background: surface;
  foreground: text;
  border: border-hover;
  radius: radius.md;
  border-width: border;
  padding-x: 7;
  gap: 8;
  icon-size: 19;
  offset-y: 34;
  opacity: 1;
  material: default;
}

Guide[role=Action]:hover {
  background: panel-hover;
}

Guide[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 6;
  gap: 12;
  icon-size: 16;
  offset-y: 28;
  opacity: 1;
  material: default;
}

Guide[role=Close]:hover {
  background: panel-hover;
}

TableView {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-y: 28;
  opacity: 1;
  material: default;
}

TableView[role=Panel] {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Header] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.sm;
  border-width: border;
  padding-x: 6;
  offset-y: 30;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TableView[role=Header]:hover {
  background: panel-hover;
}

TableView[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

TableView[role=Row] {
  background: #171c25;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: border.none;
  opacity: 1;
  material: default;
}

TableView[role=Cell] {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-x: 32;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TableView[role=Selection] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  radius: radius.sm;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Selection]:hover {
  background: accent-hover;
}

TableView[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  padding-x: 5;
  offset-y: 8;
  opacity: 1;
  material: default;
}

Plot {
  background: card;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 6;
  padding-y: 4;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

PlotMark {
  background: accent;
  foreground: accent-ink;
  border: accent;
  opacity: 1;
  material: default;
}

Text {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Page {
  gap: 0;
  padding-x: 0;
}

Section {
  gap: 0;
  padding-x: 0;
}

Heading {
  foreground: text;
  font-size: 24;
  opacity: 1;
}

ParagraphText {
  foreground: text;
  font-size: font.md;
  gap: space.2;
  opacity: 1;
}

Link {
  foreground: accent;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Link:hover {
  foreground: accent-hover;
}

Link:disabled {
  foreground: muted;
  opacity: 0.45;
}

Separator {
  background: #343b48;
  foreground: muted;
  gap: space.3;
  opacity: 1;
}

Separator[role=Line] {
  background: #343b48;
  foreground: muted;
  gap: space.3;
  opacity: 1;
}

Separator[role=Label] {
  background: #343b48;
  foreground: muted;
  font-size: font.sm;
  gap: space.3;
  opacity: 1;
}

Separator[role=Bullet] {
  background: #343b48;
  foreground: muted;
  icon-size: 6;
  gap: space.3;
  opacity: 1;
}

Progress {
  background: #222832;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Track] {
  background: #222832;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  opacity: 1;
  material: default;
}

Progress[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  padding-x: 6;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

@layer components;

Button {
  background: panel;
  background-end: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: pad.control-y;
  gap: space.2;
  font-size: font.md;
  icon-size: icon.md;
  opacity: 1;
  material: default;
}

Button:hover {
  background: panel-hover;
  background-end: panel;
  border: border-hover;
}

Button:pressed {
  background: panel-pressed;
  background-end: panel;
}

Button:focus {
  border: focus;
}

Button:disabled {
  foreground: muted;
  border: #2b3039;
  opacity: 0.55;
}

Button:loading {
  foreground: accent;
}

Button[tone=Accent] {
  background: accent;
  background-end: accent-hover;
  foreground: accent-ink;
  border: #d8c0ff;
}

Button[tone=Accent]:hover {
  background: accent-hover;
  background-end: accent;
}

Button[tone=Accent]:focus {
  border: focus;
}

Button[tone=Accent]:loading {
  foreground: accent;
}

Button[tone=Danger] {
  background: danger;
  foreground: danger-ink;
  border: danger-border;
}

Button[emphasis=Outline] {
  background: transparent;
  foreground: text;
  border: #4b5361;
}

Button[emphasis=Ghost] {
  background: transparent;
  foreground: text;
  border: transparent;
  border-width: border.none;
}

Button[size=Small] {
  radius: radius.sm;
  padding-x: space.3;
  padding-y: 7;
  font-size: font.sm;
  icon-size: 18;
}

Button[size=Large] {
  radius: radius.lg;
  padding-x: space.5;
  padding-y: 13;
  font-size: font.lg;
  icon-size: 22;
}

Dropdown {
  background: #171c25;
  foreground: #f5f2ff;
  border: #3a4250;
  focus: #c9a8ff;
  radius: 8;
  border-width: 1;
  padding-x: 14;
  padding-y: 9;
  gap: 8;
  offset-y: 16;
  font-size: font.md;
  material: Flat;
}

Dropdown:hover {
  background: panel-hover;
  border: border-hover;
}

Dropdown:pressed {
  background: panel-pressed;
}

Dropdown:focus {
  border: focus;
}

Dropdown:disabled {
  foreground: muted;
  opacity: 0.55;
}

Dropdown[tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
}

Dropdown:selected {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
}

Slider {
  background: #2a303b;
  foreground: text;
  border: #3a4250;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Slider[role=Track] {
  padding-x: 32;
  padding-y: 6;
  gap: 0;
  icon-size: 8;
  offset-x: 36;
  offset-y: 28;
  opacity: 1;
}

Slider[role=Label] {
  foreground: text;
  font-size: font.sm;
}

Slider:hover {
  background: #343b48;
}

Slider:pressed {
  background: #202631;
}

Slider[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
}

Slider:disabled {
  foreground: muted;
  opacity: 0.5;
}

SliderThumb {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
  focus: accent;
  radius: radius.lg;
  border-width: border;
  gap: 8;
  icon-size: 22;
  opacity: 1;
  material: flat;
}

SliderThumb:disabled {
  background: muted;
  foreground: surface;
  border: border;
  focus: border;
  opacity: 0.5;
}

Scroll {
  background: #202631;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 10;
  padding-x: 16;
  gap: 20;
  offset-y: 42;
  opacity: 1;
  material: default;
}

ScrollThumb {
  background: #6f7480;
  foreground: surface;
  border: #9ca2ad;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 16;
  padding-x: 2;
  opacity: 1;
  material: default;
}

ScrollThumb:hover {
  background: accent-hover;
  foreground: accent-ink;
  border: #d8c0ff;
}

ScrollThumb:pressed {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
}

Toggle {
  background: #2a303b;
  foreground: text;
  border: #3a4250;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Toggle[role=Track] {
  padding-x: 54;
  padding-y: 32;
  gap: 4;
  opacity: 1;
}

Toggle[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Toggle:hover {
  background: #343b48;
}

Toggle:pressed {
  background: #202631;
}

Toggle[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: #d8c0ff;
  gap: 3;
}

Toggle:disabled {
  foreground: muted;
  opacity: 0.5;
}

ToggleThumb {
  background: surface;
  foreground: #ffffff;
  border: border-hover;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: flat;
}

ToggleThumb[tone=Accent] {
  background: accent-ink;
  foreground: #ffffff;
  border: #d8c0ff;
  focus: accent;
  icon-size: 24;
}

ToggleThumb:disabled {
  background: muted;
  foreground: surface;
  border: border;
  focus: border;
  opacity: 0.5;
}

Checkbox {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  padding-x: 22;
  padding-y: 3;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Checkbox:hover {
  background: panel;
}

Checkbox:pressed {
  background: panel-pressed;
}

Checkbox:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Checkbox:disabled {
  background: surface;
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

Checkbox[role=Box] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: default;
}

Checkbox[role=Mark] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  focus: focus;
  border-width: border;
  padding-x: 5;
  icon-size: 2.4;
  opacity: 1;
  material: default;
}

Checkbox[role=Label] {
  foreground: text;
  gap: 10;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Checkbox[role=Label]:disabled {
  foreground: muted;
  opacity: 0.55;
}

Radio {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: default;
}

Radio:hover {
  border: focus;
}

Radio:selected {
  background: accent;
  foreground: text;
  border: accent;
}

Radio:disabled {
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

Radio[role=Ring] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  padding-x: 40;
  gap: 4;
  opacity: 1;
  material: default;
}

Radio[role=Mark] {
  background: accent;
  foreground: text;
  border: accent;
  focus: focus;
  border-width: border;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Radio[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Radio[role=Label]:disabled {
  foreground: muted;
  opacity: 0.55;
}

TextField {
  background: #11151d;
  foreground: #f5f2ff;
  border: #3a4250;
  focus: #c9a8ff;
  radius: 8;
  border-width: 1;
  padding-x: 12;
  padding-y: 9;
  font-size: 16;
  material: Flat;
}

TextArea {
  background: #11151d;
  foreground: #f5f2ff;
  border: #3a4250;
  focus: #c9a8ff;
  radius: 8;
  border-width: 1;
  padding-x: 12;
  padding-y: 10;
  font-size: 16;
  gap: 6;
  material: Flat;
}
`

const tkStyleSource = `@pack tk;

tokens {
  color {
    canvas: #eceff3;
    text: #15171b;
    muted: #777d86;
    surface: #f7f8fa;
    card: #ffffff;
    button: #f8f9fb;
    button-hover: #ffffff;
    button-pressed: #dfe4eb;
    button-disabled: #edf0f4;
    border: #9da5b1;
    border-soft: #b8bec8;
    border-hover: #7f8998;
    border-disabled: #c8ced6;
    accent: #2f6bff;
    accent-border: #1d55d8;
    accent-ink: #ffffff;
    outline-border: #8c95a3;
    transparent: #00000000;
  }
  length {
    radius: 3;
    border: 1;
    border.none: 0;
    gap: 6;
    space.1: 5;
    space.2: 8;
    space.3: 10;
    field-x: 7;
    font: 14;
    icon: 16;
  }
  material {
    default: Flat;
  }
}

@layer base;

App {
  background: canvas;
  foreground: text;
  material: default;
}

Surface {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  material: default;
}

Popup {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Popup[role=Panel] {
  background: card;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Canvas {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Card {
  background: card;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: space.3;
  padding-y: space.2;
  gap: gap;
  material: default;
}

Image {
  background: card;
  foreground: text;
  border: border-soft;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Image[role=Label] {
  foreground: muted;
  font-size: font;
  opacity: 1;
  material: default;
}

Focus {
  foreground: accent;
  border: accent-border;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Focus:focus {
  foreground: accent;
  border: accent-border;
}

Focus[role=Box] {
  foreground: muted;
  border: outline-border;
  border-width: 2;
  padding-x: 3;
  opacity: 1;
  material: default;
}

Focus[role=Box]:focus {
  foreground: accent;
  border: accent-border;
}

Focus[role=Label] {
  foreground: muted;
  font-size: font;
  opacity: 1;
  material: default;
}

Focus[role=Label]:focus {
  foreground: accent;
}

NavigationBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: default;
}

NavigationBar[role=Panel] {
  offset-x: 340;
  icon-size: 128;
  padding-y: 58;
}

NavigationBar[role=Row] {
  icon-size: 58;
  offset-y: 22;
  padding-x: 36;
  padding-y: 36;
}

NavigationBar[role=Action] {
  icon-size: 34;
  offset-x: 180;
  offset-y: 16;
  padding-x: 92;
  padding-y: 36;
  gap: 8;
}

NavigationBar[role=Divider] {
  icon-size: 48;
  offset-y: 8;
  padding-y: 12;
  gap: 8;
}

NavigationBarItem {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  font-size: font;
  opacity: 1;
  material: default;
}

NavigationBarItem:hover {
  background: button-hover;
  foreground: text;
}

NavigationBarItem:selected {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

NavigationBarItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: 120;
  icon-size: 36;
  offset-x: 120;
  gap: gap;
  opacity: 1;
  material: default;
}

Tab {
  background: button;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  padding-y: space.1;
  icon-size: 44;
  opacity: 1;
  material: default;
}

Tab:hover {
  background: button-hover;
  foreground: text;
  border: border-hover;
}

Tab:pressed {
  background: button-pressed;
  foreground: text;
}

Tab:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

Tab:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

TabClose {
  background: transparent;
  foreground: muted;
  border: transparent;
  radius: radius;
  border-width: border.none;
  icon-size: 24;
  opacity: 1;
  material: default;
}

TabClose:hover {
  background: button-hover;
  foreground: accent;
}

SegmentedControl {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 2;
  padding-y: 2;
  icon-size: 30;
  offset-x: 72;
  offset-y: 180;
  gap: 0;
  opacity: 1;
  material: default;
}

Segment {
  background: button;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  padding-y: 5;
  font-size: font;
  opacity: 1;
  material: default;
}

Segment:hover {
  background: button-hover;
  foreground: text;
}

Segment:pressed {
  background: button-pressed;
  foreground: text;
}

Segment:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

Segment:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

Menu {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: space.1;
  padding-y: space.1;
  gap: 4;
  opacity: 1;
  material: default;
}

Menu[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  padding-x: 12;
  padding-y: 3;
  gap: 2;
}

Menu[role=Popup] {
  background: card;
  foreground: text;
  border: outline-border;
  padding-x: 12;
  gap: 4;
  offset-x: 180;
}

Menu[role=Context] {
  background: card;
  foreground: text;
  border: outline-border;
}

MenuItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  padding-x: space.2;
  padding-y: 8;
  offset-x: 88;
  font-size: font;
  opacity: 1;
  material: default;
}

MenuItem:hover {
  background: button-hover;
  foreground: text;
  border: border-hover;
}

MenuItem:pressed {
  background: button-pressed;
  foreground: text;
}

MenuItem:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

MenuItem:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

MenuSeparator {
  background: transparent;
  foreground: border;
  border: border;
  radius: 0;
  border-width: border;
  padding-x: 12;
  gap: 4;
  icon-size: 32;
  offset-x: 60;
  opacity: 1;
  material: default;
}

Spinbox {
  background: surface;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

SpinboxValue {
  background: card;
  foreground: text;
  border: border-soft;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  font-size: font;
  opacity: 1;
  material: default;
}

Drag {
  background: surface;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  font-size: font;
  padding-x: 6;
  padding-y: 4;
  opacity: 1;
  material: default;
}

DragValue {
  background: card;
  foreground: text;
  border: border-soft;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  font-size: font;
  opacity: 1;
  material: default;
}

DragValue:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

ColorPicker {
  gap: space.1;
  icon-size: 36;
  padding-y: 2;
  offset-x: 20;
  offset-y: 28;
}

ColorPickerSwatch {
  background: transparent;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.1;
  font-size: font;
  opacity: 1;
  material: default;
}

ListBox {
  background: surface;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.1;
  padding-y: space.1;
  offset-y: 8;
  gap: space.1;
  opacity: 1;
  material: default;
}

ListBoxItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.1;
  icon-size: 30;
  font-size: font;
  opacity: 1;
  material: default;
}

ListBoxItem:hover {
  background: button-hover;
  foreground: text;
  border: border-hover;
}

ListBoxItem:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

ListBoxItem:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

TreeView {
  background: surface;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.1;
  padding-y: space.1;
  gap: space.1;
  opacity: 1;
  material: default;
}

TreeViewItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  padding-x: space.2;
  padding-y: 7;
  gap: 2;
  icon-size: 16;
  offset-x: 18;
  font-size: font;
  opacity: 1;
  material: default;
}

TreeViewItem:hover {
  background: button-hover;
  foreground: text;
  border: border-hover;
}

TreeViewItem:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

TreeViewItem:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

ListBoxMulti {
  background: surface;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.1;
  padding-y: space.1;
  gap: space.1;
  opacity: 1;
  material: default;
}

ListBoxMultiItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.1;
  icon-size: 28;
  font-size: font;
  opacity: 1;
  material: default;
}

ListBoxMultiItem:hover {
  background: button-hover;
  foreground: text;
  border: border-hover;
}

ListBoxMultiItem:selected {
  background: card;
  foreground: accent;
  border: accent-border;
}

ListBoxMultiItem:focus {
  border: accent;
}

ListBoxMultiItem:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.55;
}

DragDropTarget {
  background: transparent;
  border: border-hover;
  focus: accent;
  radius: radius;
  border-width: border;
  material: default;
}

DragDropTarget:hover {
  border: accent-border;
  border-width: 2;
}

Selectable {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: accent;
  radius: radius;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.1;
  font-size: font;
  opacity: 1;
  material: default;
}

Selectable:hover {
  background: button-hover;
  foreground: text;
}

Selectable:pressed {
  background: button-pressed;
  foreground: text;
}

Selectable:selected {
  background: accent;
  foreground: accent-ink;
}

Selectable:disabled {
  foreground: muted;
  opacity: 0.45;
}

Fieldset {
  background: canvas;
  foreground: muted;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 6;
  padding-y: 6;
  gap: 7;
  font-size: font;
  opacity: 1;
  material: default;
}

PanedView {
  background: transparent;
  foreground: muted;
  border: transparent;
  opacity: 1;
  material: default;
}

PanedView[role=Handle] {
  background: border;
  foreground: muted;
  border: transparent;
  radius: 0;
  border-width: border.none;
  icon-size: 8;
  opacity: 1;
  material: default;
}

PanedView[role=Handle]:hover {
  background: border-hover;
}

PanedView[role=Handle]:pressed {
  background: accent;
}

Toast {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 14;
  padding-y: 10;
  gap: 18;
  opacity: 1;
  material: default;
}

Toast[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible[role=Header] {
  background: button;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  font-size: font;
  icon-size: 20;
  padding-x: 8;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=Header]:hover {
  background: button-hover;
}

Collapsible[role=Header]:pressed {
  background: button-pressed;
}

Collapsible[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Collapsible[role=Header]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=TreeHeader] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: border.none;
  font-size: font;
  icon-size: 20;
  padding-x: 20;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=TreeHeader]:hover {
  background: button-hover;
}

Collapsible[role=TreeHeader]:selected {
  background: transparent;
  foreground: accent;
}

Collapsible[role=TreeHeader]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=Close] {
  background: transparent;
  foreground: muted;
  border: transparent;
  font-size: font;
  icon-size: 28;
  opacity: 1;
  material: default;
}

Collapsible[role=Close]:hover {
  foreground: text;
}

Collapsible[role=Close]:disabled {
  opacity: 0.45;
}

TitleBar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

TitleBar[role=Bar] {
  background: surface;
  foreground: text;
  border: border;
  radius: 0;
  border-width: border;
  opacity: 1;
  material: default;
}

TitleBar[role=Title] {
  background: transparent;
  foreground: text;
  border: transparent;
  font-size: font;
  icon-size: 48;
  padding-x: 16;
  opacity: 1;
  material: default;
}

TitleBar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: border.none;
  icon-size: 20;
  padding-x: 10;
  opacity: 1;
  material: default;
}

TitleBar[role=Action]:hover {
  background: button-hover;
}

TitleBar[role=Action]:pressed {
  background: button-pressed;
}

Toolbar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Toolbar[role=Bar] {
  background: surface;
  foreground: text;
  border: border;
  radius: 0;
  border-width: border;
  padding-x: 12;
  opacity: 1;
  material: default;
}

Toolbar[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border;
  border-width: border;
  opacity: 1;
  material: default;
}

Toolbar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: border.none;
  padding-x: 8;
  gap: 6;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Toolbar[role=Action]:hover {
  background: button-hover;
}

Toolbar[role=Action]:pressed {
  background: button-pressed;
}

Modal {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Modal[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.32;
  material: default;
}

Modal[role=Panel] {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: 18;
  padding-y: 18;
  gap: 24;
  icon-size: 280;
  offset-x: 420;
  offset-y: 58;
  opacity: 1;
  material: default;
}

Modal[role=Title] {
  foreground: text;
  font-size: font;
  padding-x: 24;
  padding-y: 18;
  icon-size: 48;
  offset-y: 14;
  opacity: 1;
  material: default;
}

Modal[role=Message] {
  foreground: text;
  font-size: font;
  padding-x: 120;
  padding-y: 160;
  gap: 18;
  icon-size: 38;
  offset-y: 18;
  opacity: 1;
  material: default;
}

Modal[role=Action] {
  background: button;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: 24;
  gap: 8;
  icon-size: 44;
  offset-x: 88;
  offset-y: 150;
  opacity: 1;
  material: default;
}

Modal[role=Action]:hover {
  background: button-hover;
  border: border-hover;
}

Modal[role=Action]:pressed {
  background: button-pressed;
}

Modal[role=Action][tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Modal[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: border.none;
  padding-x: 8;
  padding-y: 16;
  gap: 6;
  icon-size: 20;
  offset-x: 120;
  offset-y: 96;
  opacity: 1;
  material: default;
}

Modal[role=Close]:hover {
  background: button-hover;
  border: border;
}

Modal[role=Close]:pressed {
  background: button-pressed;
}

Guide {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Guide[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.22;
  material: default;
}

Guide[role=Bar] {
  background: card;
  foreground: text;
  border: border;
  radius: 0;
  border-width: border.none;
  padding-x: 12;
  gap: 12;
  icon-size: 48;
  offset-x: 48;
  opacity: 1;
  material: default;
}

Guide[role=Divider] {
  background: transparent;
  foreground: text;
  border: border;
  border-width: border;
  opacity: 1;
  material: default;
}

Guide[role=Panel] {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: 12;
  padding-y: 12;
  gap: 20;
  offset-x: 300;
  offset-y: 112;
  opacity: 1;
  material: default;
}

Guide[role=Anchor] {
  background: transparent;
  foreground: accent;
  border: accent-border;
  border-width: border;
  padding-x: 4;
  opacity: 1;
  material: default;
}

Guide[role=Label] {
  foreground: text;
  font-size: font;
  padding-x: 8;
  padding-y: 6;
  gap: 8;
  opacity: 1;
  material: default;
}

Guide[role=Action] {
  background: button;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 7;
  gap: 8;
  icon-size: 19;
  offset-y: 34;
  opacity: 1;
  material: default;
}

Guide[role=Action]:hover {
  background: button-hover;
  border: border-hover;
}

Guide[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: border.none;
  padding-x: 6;
  gap: 12;
  icon-size: 16;
  offset-y: 28;
  opacity: 1;
  material: default;
}

Guide[role=Close]:hover {
  background: button-hover;
  border: border;
}

TableView {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-y: 28;
  opacity: 1;
  material: default;
}

TableView[role=Panel] {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Header] {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 6;
  offset-y: 30;
  font-size: font;
  opacity: 1;
  material: default;
}

TableView[role=Header]:hover {
  background: button-hover;
  border: border-hover;
}

TableView[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

TableView[role=Row] {
  background: #f3f5f8;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: border.none;
  opacity: 1;
  material: default;
}

TableView[role=Cell] {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-x: 32;
  font-size: font;
  opacity: 1;
  material: default;
}

TableView[role=Selection] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border;
  border-width: border;
  padding-x: 5;
  offset-y: 8;
  opacity: 1;
  material: default;
}

Plot {
  background: card;
  foreground: muted;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 6;
  padding-y: 4;
  font-size: font;
  opacity: 1;
  material: default;
}

PlotMark {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
  opacity: 1;
  material: default;
}

Text {
  foreground: text;
  font-size: font;
}

Page {
  gap: 0;
  padding-x: 0;
}

Section {
  gap: 0;
  padding-x: 0;
}

Heading {
  foreground: text;
  font-size: 24;
}

ParagraphText {
  foreground: text;
  font-size: font;
  gap: 4;
}

Link {
  foreground: accent;
  font-size: font;
  opacity: 1;
  material: default;
}

Link:hover {
  foreground: accent-border;
}

Link:disabled {
  foreground: muted;
  opacity: 0.45;
}

Separator {
  background: border-soft;
  foreground: muted;
  gap: gap;
}

Separator[role=Line] {
  background: border-soft;
  foreground: muted;
  gap: gap;
}

Separator[role=Label] {
  background: border-soft;
  foreground: muted;
  font-size: font;
  gap: gap;
}

Separator[role=Bullet] {
  background: border-soft;
  foreground: muted;
  icon-size: 6;
  gap: gap;
}

Progress {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Track] {
  background: surface;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
  opacity: 1;
  material: default;
}

Progress[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  padding-x: 4;
  font-size: font;
  opacity: 1;
  material: default;
}

@layer components;

Button {
  background: button;
  background-end: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.3;
  padding-y: space.1;
  gap: gap;
  font-size: font;
  icon-size: icon;
  opacity: 1;
  material: default;
}

Button:hover {
  background: button-hover;
  background-end: card;
  border: border-hover;
}

Button:pressed {
  background: button-pressed;
  background-end: button;
}

Button:focus {
  border: accent;
}

Button:disabled {
  foreground: muted;
  background: button-disabled;
  background-end: card;
  border: border-disabled;
  opacity: 0.7;
}

Button:loading {
  foreground: accent;
}

Button[tone=Accent] {
  background: accent;
  background-end: #5589ff;
  foreground: accent-ink;
  border: accent-border;
}

Button[tone=Accent]:loading {
  foreground: accent;
}

Button[emphasis=Outline] {
  background: button-hover;
  foreground: text;
  border: outline-border;
}

Button[emphasis=Ghost] {
  background: transparent;
  border: transparent;
  border-width: border.none;
}

Dropdown {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: space.2;
  padding-y: space.1;
  gap: gap;
  offset-y: 16;
  font-size: font;
  material: default;
}

Dropdown:hover {
  background: button-hover;
  border: border-hover;
}

Dropdown:pressed {
  background: button-pressed;
}

Dropdown:focus {
  border: accent;
}

Dropdown:disabled {
  foreground: muted;
  background: button-disabled;
  border: border-disabled;
  opacity: 0.7;
}

Dropdown[tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Dropdown:selected {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Slider {
  background: button-pressed;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Slider[role=Track] {
  padding-x: 28;
  padding-y: 4;
  gap: 0;
  icon-size: 6;
  offset-x: 28;
  offset-y: 24;
  opacity: 1;
}

Slider[role=Label] {
  foreground: text;
  font-size: font;
}

Slider:hover {
  background: button-hover;
  border: border-hover;
}

Slider[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Slider:disabled {
  foreground: muted;
  background: button-disabled;
  border: border-disabled;
  opacity: 0.7;
}

SliderThumb {
  background: accent;
  foreground: #ffffff;
  border: accent-border;
  focus: accent;
  radius: radius;
  border-width: border;
  gap: 6;
  icon-size: 18;
  opacity: 1;
  material: default;
}

SliderThumb:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  focus: border-disabled;
  opacity: 0.7;
}

Scroll {
  background: button-pressed;
  foreground: text;
  border: border-soft;
  focus: accent;
  radius: radius;
  border-width: border;
  icon-size: 10;
  padding-x: 16;
  gap: 20;
  offset-y: 42;
  opacity: 1;
  material: default;
}

ScrollThumb {
  background: button;
  foreground: text;
  border: outline-border;
  focus: accent;
  radius: radius;
  border-width: border;
  icon-size: 16;
  padding-x: 2;
  opacity: 1;
  material: default;
}

ScrollThumb:hover {
  background: button-hover;
  border: border-hover;
}

ScrollThumb:pressed {
  background: button-pressed;
  border: accent-border;
}

Toggle {
  background: button;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: default;
}

Toggle[role=Track] {
  padding-x: 46;
  padding-y: 24;
  gap: 2;
  opacity: 1;
}

Toggle[role=Label] {
  foreground: text;
  font-size: font;
  opacity: 1;
}

Toggle:hover {
  background: button-hover;
  border: border-hover;
}

Toggle:pressed {
  background: button-pressed;
}

Toggle[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
  gap: 2;
}

Toggle:disabled {
  foreground: muted;
  background: button-disabled;
  border: border-disabled;
  opacity: 0.7;
}

ToggleThumb {
  background: card;
  foreground: #ffffff;
  border: border-hover;
  focus: accent;
  radius: radius;
  border-width: border;
  gap: 4;
  icon-size: 16;
  opacity: 1;
  material: default;
}

ToggleThumb[tone=Accent] {
  background: accent-ink;
  foreground: #ffffff;
  border: accent-border;
  focus: accent;
  icon-size: 18;
}

ToggleThumb:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  focus: border-disabled;
  opacity: 0.7;
}

Checkbox {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  border-width: border;
  padding-x: 22;
  padding-y: 3;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Checkbox:hover {
  background: button-hover;
  border: border-hover;
}

Checkbox:pressed {
  background: button-pressed;
}

Checkbox:selected {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
}

Checkbox:disabled {
  background: button-disabled;
  foreground: muted;
  border: border-disabled;
  opacity: 0.7;
}

Checkbox[role=Box] {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  border-width: border;
  opacity: 1;
  material: default;
}

Checkbox[role=Mark] {
  background: accent;
  foreground: accent-ink;
  border: accent-border;
  focus: accent;
  border-width: border;
  padding-x: 5;
  icon-size: 2.4;
  opacity: 1;
  material: default;
}

Checkbox[role=Label] {
  foreground: text;
  gap: 10;
  font-size: font;
  opacity: 1;
  material: default;
}

Checkbox[role=Label]:disabled {
  foreground: muted;
  opacity: 0.7;
}

Radio {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  border-width: border;
  opacity: 1;
  material: default;
}

Radio:hover {
  border: border-hover;
}

Radio:selected {
  background: accent;
  foreground: text;
  border: accent-border;
}

Radio:disabled {
  foreground: muted;
  border: border-disabled;
  opacity: 0.7;
}

Radio[role=Ring] {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  border-width: border;
  padding-x: 20;
  gap: gap;
  opacity: 1;
  material: default;
}

Radio[role=Mark] {
  background: accent;
  foreground: text;
  border: accent-border;
  focus: accent;
  border-width: border;
  font-size: font;
  opacity: 1;
  material: default;
}

Radio[role=Label] {
  foreground: text;
  font-size: font;
  opacity: 1;
  material: default;
}

Radio[role=Label]:disabled {
  foreground: muted;
  opacity: 0.7;
}

TextField {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: field-x;
  padding-y: space.1;
  font-size: font;
  material: default;
}

TextArea {
  background: card;
  foreground: text;
  border: border;
  focus: accent;
  radius: radius;
  border-width: border;
  padding-x: field-x;
  padding-y: gap;
  font-size: font;
  gap: space.1;
  material: default;
}
`

const vanillaStyleSource = `@pack vanilla;

tokens {
  color {
    canvas: #f6f7fb;
    text: #151820;
    muted: #6c727d;
    surface: #ffffff;
    card: #ffffff;
    panel: #f4f6fa;
    panel-hover: #ffffff;
    panel-pressed: #e5e9f0;
    border: #c8ced8;
    border-soft: #dde2ea;
    border-hover: #aeb6c3;
    focus: #2f6bff;
    accent: #2f6bff;
    accent-hover: #245be0;
    accent-ink: #ffffff;
    danger: #bd2430;
    danger-hover: #a41f2a;
    danger-ink: #ffffff;
    transparent: #00000000;
  }
  length {
    radius.sm: 5;
    radius.md: 7;
    radius.lg: 9;
    border: 1;
    border.none: 0;
    gap: 8;
    space.2: 8;
    space.3: 12;
    space.4: 16;
    space.5: 20;
    pad.control-y: 9;
    pad.field-y: 8;
    font.sm: 14;
    font.md: 16;
    font.lg: 18;
    icon.md: 20;
  }
  material {
    default: Flat;
  }
}

@layer base;

App {
  background: canvas;
  foreground: text;
  material: default;
}

Surface {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: space.4;
  gap: gap;
  material: default;
}

Popup {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Popup[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Canvas {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Card {
  background: card;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: space.4;
  gap: gap;
  material: default;
}

Image {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Image[role=Label] {
  foreground: muted;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Focus {
  foreground: focus;
  border: focus;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Focus:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Box] {
  foreground: muted;
  border: border-hover;
  border-width: 2;
  padding-x: 3;
  opacity: 1;
  material: default;
}

Focus[role=Box]:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Label] {
  foreground: muted;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

Focus[role=Label]:focus {
  foreground: focus;
}

NavigationBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.lg;
  border-width: border;
  padding-x: space.4;
  padding-y: space.3;
  gap: gap;
  opacity: 1;
  material: default;
}

NavigationBar[role=Panel] {
  offset-x: 340;
  icon-size: 128;
  padding-y: 58;
}

NavigationBar[role=Row] {
  icon-size: 58;
  offset-y: 22;
  padding-x: 36;
  padding-y: 36;
}

NavigationBar[role=Action] {
  icon-size: 34;
  offset-x: 180;
  offset-y: 16;
  padding-x: 92;
  padding-y: 36;
  gap: 8;
}

NavigationBar[role=Divider] {
  icon-size: 48;
  offset-y: 8;
  padding-y: 12;
  gap: 8;
}

NavigationBarItem {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.lg;
  border-width: border.none;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

NavigationBarItem:hover {
  background: panel-hover;
  foreground: text;
}

NavigationBarItem:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

NavigationBarItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabBar {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 72;
  icon-size: 48;
  offset-x: 168;
  gap: gap;
  opacity: 1;
  material: default;
}

Tab {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 44;
  opacity: 1;
  material: default;
}

Tab:hover {
  background: panel-hover;
  foreground: text;
  border: border-hover;
}

Tab:pressed {
  background: panel-pressed;
  foreground: text;
}

Tab:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

Tab:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabClose {
  background: transparent;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 24;
  opacity: 1;
  material: default;
}

TabClose:hover {
  background: panel-hover;
  foreground: accent;
}

SegmentedControl {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.sm;
  border-width: border;
  padding-x: 2;
  padding-y: 2;
  icon-size: 30;
  offset-x: 72;
  offset-y: 180;
  gap: space.2;
  opacity: 1;
  material: default;
}

Segment {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

Segment:hover {
  background: panel-hover;
  foreground: text;
}

Segment:pressed {
  background: panel-pressed;
  foreground: text;
}

Segment:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

Segment:disabled {
  foreground: muted;
  opacity: 0.45;
}

Menu {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: default;
}

Menu[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  padding-x: 12;
  padding-y: 3;
  gap: 2;
}

Menu[role=Popup] {
  background: card;
  foreground: text;
  border: border;
  padding-x: 12;
  gap: 4;
  offset-x: 180;
}

Menu[role=Context] {
  background: card;
  foreground: text;
  border: border;
}

MenuItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  offset-x: 88;
  font-size: font.md;
  opacity: 1;
  material: default;
}

MenuItem:hover {
  background: panel-hover;
  foreground: text;
  border: border-hover;
}

MenuItem:pressed {
  background: panel-pressed;
  foreground: text;
}

MenuItem:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

MenuItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

MenuSeparator {
  background: transparent;
  foreground: border-soft;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  gap: 4;
  icon-size: 32;
  offset-x: 60;
  opacity: 1;
  material: default;
}

Spinbox {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

SpinboxValue {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Drag {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  font-size: font.sm;
  padding-x: 6;
  padding-y: 4;
  opacity: 1;
  material: default;
}

DragValue {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

DragValue:disabled {
  background: panel;
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

ColorPicker {
  gap: 4;
  icon-size: 36;
  padding-y: 2;
  offset-x: 20;
  offset-y: 28;
}

ColorPickerSwatch {
  background: transparent;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.2;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBox {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  offset-y: 8;
  gap: gap;
  opacity: 1;
  material: default;
}

ListBoxItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 30;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBoxItem:hover {
  background: panel-hover;
  foreground: text;
  border: border-hover;
}

ListBoxItem:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

ListBoxItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TreeView {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: default;
}

TreeViewItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  gap: 2;
  icon-size: 16;
  offset-x: 18;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TreeViewItem:hover {
  background: panel-hover;
  foreground: text;
  border: border-hover;
}

TreeViewItem:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

TreeViewItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

ListBoxMulti {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: default;
}

ListBoxMultiItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 28;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

ListBoxMultiItem:hover {
  background: panel-hover;
  foreground: text;
  border: border-hover;
}

ListBoxMultiItem:selected {
  background: surface;
  foreground: accent;
  border: accent;
}

ListBoxMultiItem:focus {
  border: focus;
}

ListBoxMultiItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

DragDropTarget {
  background: transparent;
  border: border-hover;
  focus: focus;
  radius: radius.md;
  border-width: border;
  material: default;
}

DragDropTarget:hover {
  border: accent;
  border-width: 2;
}

Selectable {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.2;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Selectable:hover {
  background: panel-hover;
  foreground: text;
}

Selectable:pressed {
  background: panel-pressed;
  foreground: text;
}

Selectable:selected {
  background: accent;
  foreground: accent-ink;
}

Selectable:disabled {
  foreground: muted;
  opacity: 0.45;
}

Fieldset {
  background: canvas;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 8;
  padding-y: 8;
  gap: 9;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

PanedView {
  background: transparent;
  foreground: muted;
  border: transparent;
  opacity: 1;
  material: default;
}

PanedView[role=Handle] {
  background: border-soft;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 8;
  opacity: 1;
  material: default;
}

PanedView[role=Handle]:hover {
  background: border-hover;
}

PanedView[role=Handle]:pressed {
  background: accent;
}

Toast {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 14;
  padding-y: 10;
  gap: 18;
  opacity: 1;
  material: default;
}

Toast[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Collapsible[role=Header] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  font-size: font.md;
  icon-size: 20;
  padding-x: 8;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=Header]:hover {
  background: panel-hover;
}

Collapsible[role=Header]:pressed {
  background: panel-pressed;
}

Collapsible[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Collapsible[role=Header]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=TreeHeader] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  font-size: font.md;
  icon-size: 20;
  padding-x: 20;
  padding-y: 8;
  opacity: 1;
  material: default;
}

Collapsible[role=TreeHeader]:hover {
  background: panel;
}

Collapsible[role=TreeHeader]:selected {
  background: transparent;
  foreground: accent;
}

Collapsible[role=TreeHeader]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=Close] {
  background: transparent;
  foreground: muted;
  border: transparent;
  font-size: font.md;
  icon-size: 28;
  opacity: 1;
  material: default;
}

Collapsible[role=Close]:hover {
  foreground: text;
}

Collapsible[role=Close]:disabled {
  opacity: 0.45;
}

TitleBar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

TitleBar[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  opacity: 1;
  material: default;
}

TitleBar[role=Title] {
  background: transparent;
  foreground: text;
  border: transparent;
  font-size: font.lg;
  icon-size: 48;
  padding-x: 16;
  opacity: 1;
  material: default;
}

TitleBar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 20;
  padding-x: 10;
  opacity: 1;
  material: default;
}

TitleBar[role=Action]:hover {
  background: panel-hover;
}

TitleBar[role=Action]:pressed {
  background: panel-pressed;
}

Toolbar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Toolbar[role=Bar] {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  opacity: 1;
  material: default;
}

Toolbar[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: default;
}

Toolbar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  gap: 6;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Toolbar[role=Action]:hover {
  background: panel-hover;
}

Toolbar[role=Action]:pressed {
  background: panel-pressed;
}

Modal {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Modal[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.58;
  material: default;
}

Modal[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 18;
  padding-y: 18;
  gap: 24;
  icon-size: 280;
  offset-x: 420;
  offset-y: 58;
  opacity: 1;
  material: default;
}

Modal[role=Title] {
  foreground: text;
  font-size: font.lg;
  padding-x: 24;
  padding-y: 18;
  icon-size: 48;
  offset-y: 14;
  opacity: 1;
  material: default;
}

Modal[role=Message] {
  foreground: text;
  font-size: font.md;
  padding-x: 120;
  padding-y: 160;
  gap: 18;
  icon-size: 38;
  offset-y: 18;
  opacity: 1;
  material: default;
}

Modal[role=Action] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: 24;
  gap: 8;
  icon-size: 44;
  offset-x: 88;
  offset-y: 150;
  opacity: 1;
  material: default;
}

Modal[role=Action]:hover {
  background: panel-hover;
}

Modal[role=Action]:pressed {
  background: panel-pressed;
}

Modal[role=Action][tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Modal[role=Action][tone=Danger] {
  background: danger;
  foreground: danger-ink;
  border: danger;
}

Modal[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  padding-y: 16;
  gap: 6;
  icon-size: 20;
  offset-x: 120;
  offset-y: 96;
  opacity: 1;
  material: default;
}

Modal[role=Close]:hover {
  background: panel-hover;
}

Modal[role=Close]:pressed {
  background: panel-pressed;
}

Guide {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: default;
}

Guide[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.32;
  material: default;
}

Guide[role=Bar] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border.none;
  padding-x: 12;
  gap: 12;
  icon-size: 48;
  offset-x: 48;
  opacity: 1;
  material: default;
}

Guide[role=Divider] {
  background: transparent;
  foreground: text;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: default;
}

Guide[role=Panel] {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 12;
  padding-y: 12;
  gap: 20;
  offset-x: 300;
  offset-y: 112;
  opacity: 1;
  material: default;
}

Guide[role=Anchor] {
  background: transparent;
  foreground: accent;
  border: accent;
  border-width: border;
  padding-x: 4;
  opacity: 1;
  material: default;
}

Guide[role=Label] {
  foreground: text;
  font-size: font.md;
  padding-x: 8;
  padding-y: 6;
  gap: 8;
  opacity: 1;
  material: default;
}

Guide[role=Action] {
  background: surface;
  foreground: text;
  border: border-hover;
  radius: radius.md;
  border-width: border;
  padding-x: 7;
  gap: 8;
  icon-size: 19;
  offset-y: 34;
  opacity: 1;
  material: default;
}

Guide[role=Action]:hover {
  background: panel-hover;
}

Guide[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 6;
  gap: 12;
  icon-size: 16;
  offset-y: 28;
  opacity: 1;
  material: default;
}

Guide[role=Close]:hover {
  background: panel-hover;
}

TableView {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-y: 28;
  opacity: 1;
  material: default;
}

TableView[role=Panel] {
  background: surface;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Header] {
  background: panel;
  foreground: text;
  border: border-soft;
  radius: radius.sm;
  border-width: border;
  padding-x: 6;
  offset-y: 30;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TableView[role=Header]:hover {
  background: panel-hover;
}

TableView[role=Header]:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

TableView[role=Row] {
  background: #171d25;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: border.none;
  opacity: 1;
  material: default;
}

TableView[role=Cell] {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-x: 32;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

TableView[role=Selection] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  radius: radius.sm;
  border-width: border;
  opacity: 1;
  material: default;
}

TableView[role=Selection]:hover {
  background: accent;
}

TableView[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  padding-x: 5;
  offset-y: 8;
  opacity: 1;
  material: default;
}

Plot {
  background: panel;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 6;
  padding-y: 4;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

PlotMark {
  background: accent;
  foreground: accent-ink;
  border: accent;
  opacity: 1;
  material: default;
}

Text {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Page {
  gap: 0;
  padding-x: 0;
}

Section {
  gap: 0;
  padding-x: 0;
}

Heading {
  foreground: text;
  font-size: 24;
  opacity: 1;
}

ParagraphText {
  foreground: text;
  font-size: font.md;
  gap: space.2;
  opacity: 1;
}

Link {
  foreground: accent;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Link:hover {
  foreground: accent-hover;
}

Link:disabled {
  foreground: muted;
  opacity: 0.45;
}

Separator {
  background: border-soft;
  foreground: muted;
  gap: gap;
  opacity: 1;
}

Separator[role=Line] {
  background: border-soft;
  foreground: muted;
  gap: gap;
  opacity: 1;
}

Separator[role=Label] {
  background: border-soft;
  foreground: muted;
  font-size: font.sm;
  gap: gap;
  opacity: 1;
}

Separator[role=Bullet] {
  background: border-soft;
  foreground: muted;
  icon-size: 6;
  gap: gap;
  opacity: 1;
}

Progress {
  background: panel;
  foreground: text;
  border: border;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Track] {
  background: panel;
  foreground: text;
  border: border;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: default;
}

Progress[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  opacity: 1;
  material: default;
}

Progress[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  padding-x: 6;
  font-size: font.sm;
  opacity: 1;
  material: default;
}

@layer components;

Button {
  background: panel;
  background-end: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: pad.control-y;
  gap: gap;
  font-size: font.md;
  icon-size: icon.md;
  opacity: 1;
  material: default;
}

Button:hover {
  background: panel-hover;
  background-end: panel;
  border: border-hover;
}

Button:pressed {
  background: panel-pressed;
  background-end: panel;
}

Button:focus {
  border: focus;
}

Button:disabled {
  foreground: muted;
  opacity: 0.58;
}

Button:loading {
  foreground: accent;
}

Button[tone=Accent] {
  background: accent;
  background-end: accent-hover;
  foreground: accent-ink;
  border: accent;
}

Button[tone=Accent]:hover {
  background: accent-hover;
  border: accent-hover;
}

Button[tone=Accent]:loading {
  foreground: accent;
}

Button[tone=Danger] {
  background: danger;
  foreground: danger-ink;
  border: danger;
}

Button[tone=Danger]:hover {
  background: danger-hover;
  border: danger-hover;
}

Button[emphasis=Outline] {
  background: transparent;
  foreground: text;
  border: border-hover;
}

Button[emphasis=Ghost] {
  background: transparent;
  border: transparent;
  border-width: border.none;
}

Button[size=Small] {
  radius: radius.sm;
  padding-x: space.3;
  padding-y: 7;
  font-size: font.sm;
  icon-size: 18;
}

Button[size=Large] {
  radius: radius.lg;
  padding-x: space.5;
  padding-y: 12;
  font-size: font.lg;
  icon-size: 22;
}

Dropdown {
  background: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.field-y;
  gap: gap;
  offset-y: 16;
  font-size: font.md;
  material: default;
}

Dropdown:hover {
  background: panel-hover;
  border: border-hover;
}

Dropdown:pressed {
  background: panel-pressed;
}

Dropdown:focus {
  border: focus;
}

Dropdown:disabled {
  foreground: muted;
  opacity: 0.58;
}

Dropdown[tone=Accent] {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Dropdown:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Slider {
  background: panel-pressed;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Slider[role=Track] {
  padding-x: 32;
  padding-y: 6;
  gap: 0;
  icon-size: 8;
  offset-x: 36;
  offset-y: 28;
  opacity: 1;
}

Slider[role=Label] {
  foreground: text;
  font-size: font.sm;
}

Slider:hover {
  background: panel-hover;
  border: border-hover;
}

Slider[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Slider:disabled {
  foreground: muted;
  opacity: 0.58;
}

SliderThumb {
  background: accent;
  foreground: accent-ink;
  border: accent;
  focus: accent;
  radius: radius.lg;
  border-width: border;
  gap: 8;
  icon-size: 22;
  opacity: 1;
  material: default;
}

SliderThumb:disabled {
  background: muted;
  foreground: panel;
  border: border;
  focus: border;
  opacity: 0.58;
}

Scroll {
  background: panel;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 10;
  padding-x: 16;
  gap: 20;
  offset-y: 42;
  opacity: 1;
  material: default;
}

ScrollThumb {
  background: muted;
  foreground: surface;
  border: border-hover;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 16;
  padding-x: 2;
  opacity: 1;
  material: default;
}

ScrollThumb:hover {
  background: accent-hover;
  foreground: accent-ink;
  border: accent-hover;
}

ScrollThumb:pressed {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Toggle {
  background: panel;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: default;
}

Toggle[role=Track] {
  padding-x: 54;
  padding-y: 32;
  gap: 4;
  opacity: 1;
}

Toggle[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Toggle:hover {
  background: panel-hover;
  border: border-hover;
}

Toggle:pressed {
  background: panel-pressed;
}

Toggle[role=Fill] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  gap: 3;
}

Toggle:disabled {
  foreground: muted;
  opacity: 0.58;
}

ToggleThumb {
  background: surface;
  foreground: #ffffff;
  border: border-hover;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: flat;
}

ToggleThumb[tone=Accent] {
  background: accent-ink;
  foreground: #ffffff;
  border: accent;
  focus: accent;
  icon-size: 24;
}

ToggleThumb:disabled {
  background: muted;
  foreground: panel;
  border: border;
  focus: border;
  opacity: 0.58;
}

Checkbox {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  padding-x: 22;
  padding-y: 3;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: default;
}

Checkbox:hover {
  background: panel-hover;
  border: border-hover;
}

Checkbox:pressed {
  background: panel-pressed;
}

Checkbox:selected {
  background: accent;
  foreground: accent-ink;
  border: accent;
}

Checkbox:disabled {
  foreground: muted;
  opacity: 0.58;
}

Checkbox[role=Box] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: default;
}

Checkbox[role=Mark] {
  background: accent;
  foreground: accent-ink;
  border: accent;
  focus: focus;
  border-width: border;
  padding-x: 5;
  icon-size: 2.4;
  opacity: 1;
  material: default;
}

Checkbox[role=Label] {
  foreground: text;
  gap: 10;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Checkbox[role=Label]:disabled {
  foreground: muted;
  opacity: 0.58;
}

Radio {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: default;
}

Radio:hover {
  border: focus;
}

Radio:selected {
  background: accent;
  foreground: text;
  border: accent;
}

Radio:disabled {
  foreground: muted;
  border: border;
  opacity: 0.58;
}

Radio[role=Ring] {
  background: surface;
  foreground: text;
  border: border-hover;
  focus: focus;
  border-width: border;
  padding-x: 40;
  gap: 4;
  opacity: 1;
  material: default;
}

Radio[role=Mark] {
  background: accent;
  foreground: text;
  border: accent;
  focus: focus;
  border-width: border;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Radio[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
  material: default;
}

Radio[role=Label]:disabled {
  foreground: muted;
  opacity: 0.58;
}

TextField {
  background: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.field-y;
  font-size: font.md;
  material: default;
}

TextArea {
  background: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.control-y;
  font-size: font.md;
  gap: 0;
  material: default;
}
`

const glowStyleSource = `@pack glow;

tokens {
  color {
    canvas: #0e1118;
    text: #f7f3ff;
    muted: #a5a8b3;
    surface: #171b24;
    surface-end: #202632;
    card: #191f2a;
    card-end: #252c39;
    panel: #202633;
    panel-end: #2b3342;
    panel-hover: #293244;
    panel-hover-end: #354057;
    panel-pressed: #171d28;
    panel-pressed-end: #232b3a;
    border: #465066;
    border-soft: #343c4c;
    focus: #caa8ff;
    accent: #caa8ff;
    accent-end: #b88cff;
    accent-hover: #d8c0ff;
    accent-hover-end: #cba6ff;
    accent-ink: #180f23;
    danger: #f07178;
    danger-end: #df5962;
    danger-ink: #220b0d;
    transparent: #00000000;
  }
  length {
    radius.sm: 7;
    radius.md: 10;
    radius.lg: 14;
    border: 1;
    border.none: 0;
    gap: 8;
    space.2: 8;
    space.3: 12;
    space.4: 16;
    space.5: 20;
    pad.control-y: 10;
    pad.field-y: 9;
    font.sm: 14;
    font.md: 16;
    font.lg: 18;
    icon.md: 20;
  }
  material {
    app: Flat;
    glow: Glass;
  }
}

@layer base;

App {
  background: canvas;
  foreground: text;
  material: app;
}

Surface {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: space.4;
  gap: gap;
  material: glow;
}

Popup {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: glow;
}

Popup[role=Panel] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: glow;
}

Canvas {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

Card {
  background: card;
  background-end: card-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.5;
  padding-y: space.4;
  gap: gap;
  material: glow;
}

Image {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

Image[role=Label] {
  foreground: muted;
  font-size: font.md;
  opacity: 1;
  material: app;
}

Focus {
  foreground: focus;
  border: focus;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: app;
}

Focus:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Box] {
  foreground: muted;
  border: border;
  border-width: 2;
  padding-x: 3;
  opacity: 1;
  material: app;
}

Focus[role=Box]:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Label] {
  foreground: muted;
  font-size: font.sm;
  opacity: 1;
  material: app;
}

Focus[role=Label]:focus {
  foreground: focus;
}

NavigationBar {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius.lg;
  border-width: border;
  padding-x: space.4;
  padding-y: space.3;
  gap: gap;
  opacity: 1;
  material: glow;
}

NavigationBar[role=Panel] {
  offset-x: 340;
  icon-size: 128;
  padding-y: 58;
}

NavigationBar[role=Row] {
  icon-size: 58;
  offset-y: 22;
  padding-x: 36;
  padding-y: 36;
}

NavigationBar[role=Action] {
  icon-size: 34;
  offset-x: 180;
  offset-y: 16;
  padding-x: 92;
  padding-y: 36;
  gap: 8;
}

NavigationBar[role=Divider] {
  icon-size: 48;
  offset-y: 8;
  padding-y: 12;
  gap: 8;
}

NavigationBarItem {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.lg;
  border-width: border.none;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

NavigationBarItem:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

NavigationBarItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

NavigationBarItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabBar {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 120;
  icon-size: 36;
  offset-x: 120;
  gap: gap;
  opacity: 1;
  material: glow;
}

Tab {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 44;
  opacity: 1;
  material: glow;
}

Tab:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

Tab:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Tab:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Tab:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabClose {
  background: transparent;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 24;
  opacity: 1;
  material: glow;
}

TabClose:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: accent;
}

SegmentedControl {
  background: surface;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 2;
  padding-y: 2;
  icon-size: 30;
  offset-x: 72;
  offset-y: 180;
  gap: space.2;
  opacity: 1;
  material: glow;
}

Segment {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

Segment:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

Segment:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
  foreground: text;
}

Segment:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Segment:disabled {
  foreground: muted;
  opacity: 0.45;
}

Menu {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: glow;
}

Menu[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  padding-x: 12;
  padding-y: 3;
  gap: 2;
}

Menu[role=Popup] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: border;
  padding-x: 12;
  gap: 4;
  offset-x: 180;
}

Menu[role=Context] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: border;
}

MenuItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  offset-x: 88;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

MenuItem:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

MenuItem:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
  foreground: text;
}

MenuItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

MenuItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

MenuSeparator {
  background: transparent;
  foreground: border-soft;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  gap: 4;
  icon-size: 32;
  offset-x: 60;
  opacity: 1;
  material: glow;
}

Spinbox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

SpinboxValue {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Drag {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  font-size: font.sm;
  padding-x: 6;
  padding-y: 4;
  opacity: 1;
  material: glow;
}

DragValue {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.3;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

DragValue:disabled {
  background: surface;
  background-end: surface-end;
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

ColorPicker {
  gap: 4;
  icon-size: 36;
  padding-y: 2;
  offset-x: 20;
  offset-y: 28;
}

ColorPickerSwatch {
  background: transparent;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.sm;
  border-width: border;
  padding-x: space.2;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

ListBox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  offset-y: 8;
  gap: gap;
  opacity: 1;
  material: glow;
}

ListBoxItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 30;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

ListBoxItem:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

ListBoxItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

ListBoxItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TreeView {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: glow;
}

TreeViewItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: 7;
  gap: 2;
  icon-size: 16;
  offset-x: 18;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

TreeViewItem:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

TreeViewItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

TreeViewItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

ListBoxMulti {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.2;
  padding-y: space.2;
  gap: gap;
  opacity: 1;
  material: glow;
}

ListBoxMultiItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.sm;
  border-width: border.none;
  padding-x: space.3;
  padding-y: space.2;
  icon-size: 28;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

ListBoxMultiItem:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

ListBoxMultiItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

ListBoxMultiItem:focus {
  border: focus;
}

ListBoxMultiItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

DragDropTarget {
  background: transparent;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  material: glow;
}

DragDropTarget:hover {
  border: accent-hover;
  border-width: 2;
}

Selectable {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius.md;
  border-width: border.none;
  padding-x: space.2;
  padding-y: space.2;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Selectable:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  foreground: text;
}

Selectable:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
  foreground: text;
}

Selectable:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
}

Selectable:disabled {
  foreground: muted;
  opacity: 0.45;
}

Fieldset {
  background: canvas;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 8;
  padding-y: 8;
  gap: 9;
  font-size: font.sm;
  opacity: 1;
  material: app;
}

PanedView {
  background: transparent;
  foreground: muted;
  border: transparent;
  opacity: 1;
  material: app;
}

PanedView[role=Handle] {
  background: border-soft;
  background-end: border;
  foreground: muted;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 8;
  opacity: 1;
  material: glow;
}

PanedView[role=Handle]:hover {
  background: border;
  background-end: focus;
}

PanedView[role=Handle]:pressed {
  background: accent;
  background-end: accent-end;
}

Toast {
  background: card;
  background-end: card-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 14;
  padding-y: 10;
  gap: 18;
  opacity: 0.98;
  material: glow;
}

Toast[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Collapsible {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Collapsible[role=Header] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  font-size: font.md;
  icon-size: 20;
  padding-x: 8;
  padding-y: 8;
  opacity: 1;
  material: glow;
}

Collapsible[role=Header]:hover {
  background: panel-hover;
  background-end: panel-hover-end;
}

Collapsible[role=Header]:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Collapsible[role=Header]:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Collapsible[role=Header]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=TreeHeader] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  font-size: font.md;
  icon-size: 20;
  padding-x: 20;
  padding-y: 8;
  opacity: 1;
  material: app;
}

Collapsible[role=TreeHeader]:hover {
  background: panel;
  background-end: panel-end;
  material: glow;
}

Collapsible[role=TreeHeader]:selected {
  background: transparent;
  foreground: accent;
}

Collapsible[role=TreeHeader]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=Close] {
  background: transparent;
  foreground: muted;
  border: transparent;
  font-size: font.md;
  icon-size: 28;
  opacity: 1;
  material: app;
}

Collapsible[role=Close]:hover {
  foreground: text;
}

Collapsible[role=Close]:disabled {
  opacity: 0.45;
}

TitleBar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

TitleBar[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  opacity: 1;
  material: glow;
}

TitleBar[role=Title] {
  background: transparent;
  foreground: text;
  border: transparent;
  font-size: font.lg;
  icon-size: 48;
  padding-x: 16;
  opacity: 1;
  material: app;
}

TitleBar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  icon-size: 20;
  padding-x: 10;
  opacity: 1;
  material: app;
}

TitleBar[role=Action]:hover {
  background: panel;
  background-end: panel-end;
  material: glow;
}

TitleBar[role=Action]:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Toolbar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Toolbar[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  opacity: 1;
  material: glow;
}

Toolbar[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: app;
}

Toolbar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  gap: 6;
  icon-size: 20;
  opacity: 1;
  material: app;
}

Toolbar[role=Action]:hover {
  background: panel;
  background-end: panel-end;
  material: glow;
}

Toolbar[role=Action]:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Modal {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Modal[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.6;
  material: app;
}

Modal[role=Panel] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 18;
  padding-y: 18;
  gap: 24;
  icon-size: 280;
  offset-x: 420;
  offset-y: 58;
  opacity: 1;
  material: glow;
}

Modal[role=Title] {
  foreground: text;
  font-size: font.lg;
  padding-x: 24;
  padding-y: 18;
  icon-size: 48;
  offset-y: 14;
  opacity: 1;
  material: app;
}

Modal[role=Message] {
  foreground: text;
  font-size: font.md;
  padding-x: 120;
  padding-y: 160;
  gap: 18;
  icon-size: 38;
  offset-y: 18;
  opacity: 1;
  material: app;
}

Modal[role=Action] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: 24;
  gap: 8;
  icon-size: 44;
  offset-x: 88;
  offset-y: 150;
  opacity: 1;
  material: glow;
}

Modal[role=Action]:hover {
  background: panel-hover;
  background-end: panel-hover-end;
}

Modal[role=Action]:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Modal[role=Action][tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Modal[role=Action][tone=Danger] {
  background: danger;
  background-end: danger-end;
  foreground: danger-ink;
  border: danger;
}

Modal[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 8;
  padding-y: 16;
  gap: 6;
  icon-size: 20;
  offset-x: 120;
  offset-y: 96;
  opacity: 1;
  material: app;
}

Modal[role=Close]:hover {
  background: panel;
  background-end: panel-end;
  material: glow;
}

Modal[role=Close]:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Guide {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Guide[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.34;
  material: app;
}

Guide[role=Bar] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border.none;
  padding-x: 12;
  gap: 12;
  icon-size: 48;
  offset-x: 48;
  opacity: 1;
  material: glow;
}

Guide[role=Divider] {
  background: transparent;
  foreground: text;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: app;
}

Guide[role=Panel] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  padding-x: 12;
  padding-y: 12;
  gap: 20;
  offset-x: 300;
  offset-y: 112;
  opacity: 1;
  material: glow;
}

Guide[role=Anchor] {
  background: transparent;
  foreground: accent;
  border: accent-hover;
  border-width: border;
  padding-x: 4;
  opacity: 1;
  material: app;
}

Guide[role=Label] {
  foreground: text;
  font-size: font.md;
  padding-x: 8;
  padding-y: 6;
  gap: 8;
  opacity: 1;
  material: app;
}

Guide[role=Action] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  radius: radius.md;
  border-width: border;
  padding-x: 7;
  gap: 8;
  icon-size: 19;
  offset-y: 34;
  opacity: 1;
  material: glow;
}

Guide[role=Action]:hover {
  background: panel-hover;
  background-end: panel-hover-end;
}

Guide[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius.sm;
  border-width: border.none;
  padding-x: 6;
  gap: 12;
  icon-size: 16;
  offset-y: 28;
  opacity: 1;
  material: app;
}

Guide[role=Close]:hover {
  background: panel;
  background-end: panel-end;
  material: glow;
}

TableView {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-y: 28;
  opacity: 1;
  material: app;
}

TableView[role=Panel] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

TableView[role=Header] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  radius: radius.sm;
  border-width: border;
  padding-x: 6;
  offset-y: 30;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

TableView[role=Header]:hover {
  background: panel-hover;
  background-end: panel-hover-end;
}

TableView[role=Header]:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

TableView[role=Row] {
  background: #171d27;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: border.none;
  opacity: 1;
  material: app;
}

TableView[role=Cell] {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-x: 32;
  font-size: font.sm;
  opacity: 1;
  material: app;
}

TableView[role=Selection] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  radius: radius.sm;
  border-width: border;
  opacity: 1;
  material: glow;
}

TableView[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  padding-x: 5;
  offset-y: 8;
  opacity: 1;
  material: app;
}

Plot {
  background: card;
  background-end: card-end;
  foreground: muted;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  padding-x: 6;
  padding-y: 4;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

PlotMark {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  opacity: 1;
  material: glow;
}

Text {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Page {
  gap: 0;
  padding-x: 0;
}

Section {
  gap: 0;
  padding-x: 0;
}

Heading {
  foreground: text;
  font-size: 24;
  opacity: 1;
}

ParagraphText {
  foreground: text;
  font-size: font.md;
  gap: space.2;
  opacity: 1;
}

Link {
  foreground: accent;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Link:hover {
  foreground: accent-hover;
}

Link:disabled {
  foreground: muted;
  opacity: 0.45;
}

Separator {
  background: border-soft;
  foreground: muted;
  gap: gap;
  opacity: 1;
}

Separator[role=Line] {
  background: border-soft;
  foreground: muted;
  gap: gap;
  opacity: 1;
}

Separator[role=Label] {
  background: border-soft;
  foreground: muted;
  font-size: font.sm;
  gap: gap;
  opacity: 1;
}

Separator[role=Bullet] {
  background: border-soft;
  foreground: muted;
  icon-size: 6;
  gap: gap;
  opacity: 1;
}

Progress {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

Progress[role=Track] {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  radius: radius.md;
  border-width: border;
  opacity: 1;
  material: glow;
}

Progress[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  opacity: 1;
  material: glow;
}

Progress[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  padding-x: 6;
  font-size: font.sm;
  opacity: 1;
  material: glow;
}

@layer components;

Button {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.4;
  padding-y: pad.control-y;
  gap: gap;
  font-size: font.md;
  icon-size: icon.md;
  opacity: 1;
  material: glow;
}

Button:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  border: focus;
}

Button:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Button:focus {
  border: focus;
}

Button:disabled {
  foreground: muted;
  opacity: 0.52;
}

Button:loading {
  foreground: accent;
}

Button[tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Button[tone=Accent]:hover {
  background: accent-hover;
  background-end: accent-hover-end;
}

Button[tone=Accent]:loading {
  foreground: accent;
}

Button[tone=Danger] {
  background: danger;
  background-end: danger-end;
  foreground: danger-ink;
  border: #ff9da2;
}

Button[emphasis=Outline] {
  background: transparent;
  foreground: text;
  border: border;
  material: app;
}

Button[emphasis=Ghost] {
  background: transparent;
  border: transparent;
  border-width: border.none;
  material: app;
}

Dropdown {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: 14;
  padding-y: pad.field-y;
  gap: gap;
  offset-y: 16;
  font-size: font.md;
  material: glow;
}

Dropdown:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  border: focus;
}

Dropdown:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Dropdown:focus {
  border: focus;
}

Dropdown:disabled {
  foreground: muted;
  opacity: 0.52;
}

Dropdown[tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Dropdown:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Slider {
  background: panel-pressed;
  background-end: panel-pressed-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: glow;
}

Slider[role=Track] {
  padding-x: 32;
  padding-y: 6;
  gap: 0;
  icon-size: 8;
  offset-x: 36;
  offset-y: 28;
  opacity: 1;
}

Slider[role=Label] {
  foreground: text;
  font-size: font.sm;
}

Slider:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  border: focus;
}

Slider[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Slider:disabled {
  foreground: muted;
  opacity: 0.52;
}

SliderThumb {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  focus: accent;
  radius: radius.lg;
  border-width: border;
  gap: 8;
  icon-size: 22;
  opacity: 1;
  material: glow;
}

SliderThumb:disabled {
  background: muted;
  foreground: panel;
  border: border;
  focus: border;
  opacity: 0.52;
}

Scroll {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 10;
  padding-x: 16;
  gap: 20;
  offset-y: 42;
  opacity: 1;
  material: glow;
}

ScrollThumb {
  background: muted;
  foreground: surface;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  icon-size: 16;
  padding-x: 2;
  opacity: 1;
  material: glow;
}

ScrollThumb:hover {
  background: accent-hover;
  background-end: accent-hover-end;
  foreground: accent-ink;
  border: accent-hover;
}

ScrollThumb:pressed {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Toggle {
  background: panel;
  background-end: panel-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  opacity: 1;
  material: glow;
}

Toggle[role=Track] {
  padding-x: 54;
  padding-y: 32;
  gap: 4;
  opacity: 1;
}

Toggle[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
}

Toggle:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  border: focus;
}

Toggle:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Toggle[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  gap: 3;
}

Toggle:disabled {
  foreground: muted;
  opacity: 0.52;
}

ToggleThumb {
  background: surface;
  background-end: surface-end;
  foreground: #ffffff;
  border: border;
  focus: focus;
  radius: radius.lg;
  border-width: border;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: glow;
}

ToggleThumb[tone=Accent] {
  background: accent-ink;
  icon-size: 24;
  foreground: #ffffff;
  border: accent-hover;
  focus: accent;
}

ToggleThumb:disabled {
  background: muted;
  foreground: panel;
  border: border;
  focus: border;
  opacity: 0.52;
}

Checkbox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  padding-x: 22;
  padding-y: 3;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: glow;
}

Checkbox:hover {
  background: panel-hover;
  background-end: panel-hover-end;
  border: focus;
}

Checkbox:pressed {
  background: panel-pressed;
  background-end: panel-pressed-end;
}

Checkbox:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
}

Checkbox:disabled {
  foreground: muted;
  opacity: 0.52;
}

Checkbox[role=Box] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: glow;
}

Checkbox[role=Mark] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-hover;
  focus: focus;
  border-width: border;
  padding-x: 5;
  icon-size: 2.4;
  opacity: 1;
  material: glow;
}

Checkbox[role=Label] {
  foreground: text;
  gap: 10;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Checkbox[role=Label]:disabled {
  foreground: muted;
  opacity: 0.52;
}

Radio {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: glow;
}

Radio:hover {
  border: focus;
}

Radio:selected {
  background: accent;
  background-end: accent-end;
  foreground: text;
  border: accent-hover;
}

Radio:disabled {
  foreground: muted;
  opacity: 0.52;
}

Radio[role=Ring] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  padding-x: 40;
  gap: 4;
  opacity: 1;
  material: glow;
}

Radio[role=Mark] {
  background: accent;
  background-end: accent-end;
  foreground: text;
  border: accent-hover;
  focus: focus;
  border-width: border;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Radio[role=Label] {
  foreground: text;
  font-size: font.md;
  opacity: 1;
  material: glow;
}

Radio[role=Label]:disabled {
  foreground: muted;
  opacity: 0.52;
}

TextField {
  background: #11151d;
  background-end: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.field-y;
  font-size: font.md;
  material: glow;
}

TextArea {
  background: #11151d;
  background-end: surface;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius.md;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.control-y;
  font-size: font.md;
  gap: 0;
  material: glow;
}
`

const lightfieldStyleSource = `@pack lightfield;

tokens {
  color {
    canvas: #0d1118;
    text: #f4f0ff;
    muted: #969aa5;
    surface: #171c25dd;
    surface-end: #222936ee;
    card: #1a202bdd;
    card-end: #252c39ee;
    button: #202633dd;
    button-end: #2b3342ee;
    button-hover: #283142ee;
    button-hover-end: #333c4fee;
    button-pressed: #171c25ee;
    button-pressed-end: #202734ee;
    dropdown-end: #242b38ee;
    border: #41495a;
    border-soft: #343c4c;
    card-border: #384252;
    focus: #caa8ff;
    accent: #caa8ff;
    accent-end: #b990ff;
    accent-hover: #d5bcff;
    accent-hover-end: #c49fff;
    accent-ink: #171022;
    accent-border: #dbc4ff;
    transparent: #00000000;
  }
  length {
    radius: 10;
    border: 1;
    gap: 8;
    space.3: 12;
    space.4: 16;
    space.5: 18;
    pad.control-y: 10;
    pad.dropdown-y: 9;
    pad.field-y: 9;
    font: 16;
    font.md: 16;
    icon: 20;
  }
  material {
    app: Flat;
    premium: Lightfield;
  }
}

@layer base;

App {
  background: canvas;
  foreground: text;
  material: app;
}

Surface {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: space.5;
  padding-y: space.4;
  gap: space.3;
  material: premium;
}

Popup {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: premium;
}

Popup[role=Panel] {
  background: button;
  background-end: dropdown-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Canvas {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Card {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  radius: radius;
  border-width: border;
  padding-x: space.5;
  padding-y: space.4;
  gap: space.3;
  material: premium;
}

Image {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Image[role=Label] {
  foreground: muted;
  font-size: font;
  opacity: 1;
  material: app;
}

Focus {
  foreground: focus;
  border: focus;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: app;
}

Focus:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Box] {
  foreground: muted;
  border: border;
  border-width: 2;
  padding-x: 3;
  opacity: 1;
  material: app;
}

Focus[role=Box]:focus {
  foreground: focus;
  border: focus;
}

Focus[role=Label] {
  foreground: muted;
  font-size: font;
  opacity: 1;
  material: app;
}

Focus[role=Label]:focus {
  foreground: focus;
}

NavigationBar {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: space.5;
  padding-y: space.3;
  gap: gap;
  opacity: 1;
  material: premium;
}

NavigationBar[role=Panel] {
  offset-x: 340;
  icon-size: 128;
  padding-y: 58;
}

NavigationBar[role=Row] {
  icon-size: 58;
  offset-y: 22;
  padding-x: 36;
  padding-y: 36;
}

NavigationBar[role=Action] {
  icon-size: 34;
  offset-x: 180;
  offset-y: 16;
  padding-x: 92;
  padding-y: 36;
  gap: 8;
}

NavigationBar[role=Divider] {
  icon-size: 48;
  offset-y: 8;
  padding-y: 12;
  gap: 8;
}

NavigationBarItem {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  font-size: font;
  opacity: 1;
  material: premium;
}

NavigationBarItem:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

NavigationBarItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

NavigationBarItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabBar {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: 120;
  icon-size: 36;
  offset-x: 120;
  gap: gap;
  opacity: 1;
  material: premium;
}

Tab {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: space.3;
  padding-y: 8;
  icon-size: 44;
  opacity: 1;
  material: premium;
}

Tab:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Tab:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Tab:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Tab:disabled {
  foreground: muted;
  opacity: 0.45;
}

TabClose {
  background: transparent;
  foreground: muted;
  border: transparent;
  radius: radius;
  border-width: 0;
  icon-size: 24;
  opacity: 1;
  material: premium;
}

TabClose:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: accent;
}

SegmentedControl {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  radius: radius;
  border-width: 1;
  padding-x: 2;
  padding-y: 2;
  icon-size: 30;
  offset-x: 72;
  offset-y: 180;
  gap: gap;
  opacity: 1;
  material: premium;
}

Segment {
  background: transparent;
  foreground: muted;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: 8;
  font-size: font;
  opacity: 1;
  material: premium;
}

Segment:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

Segment:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
  foreground: text;
}

Segment:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Segment:disabled {
  foreground: muted;
  opacity: 0.45;
}

Menu {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  radius: radius;
  border-width: 1;
  padding-x: space.3;
  padding-y: gap;
  gap: gap;
  opacity: 1;
  material: premium;
}

Menu[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  padding-x: 12;
  padding-y: 3;
  gap: 2;
}

Menu[role=Popup] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  padding-x: 12;
  gap: 4;
  offset-x: 180;
}

Menu[role=Context] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
}

MenuItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: 7;
  offset-x: 88;
  font-size: font;
  opacity: 1;
  material: premium;
}

MenuItem:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

MenuItem:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
  foreground: text;
}

MenuItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

MenuItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

MenuSeparator {
  background: transparent;
  foreground: border;
  border: border;
  radius: 0;
  border-width: 1;
  opacity: 1;
  material: premium;
}

Spinbox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: 1;
  opacity: 1;
  material: premium;
}

SpinboxValue {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: space.3;
  font-size: font;
  opacity: 1;
  material: premium;
}

Drag {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: 1;
  font-size: font;
  opacity: 1;
  material: premium;
}

DragValue {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: space.3;
  font-size: font;
  opacity: 1;
  material: premium;
}

DragValue:disabled {
  background: surface;
  background-end: surface-end;
  foreground: muted;
  border: border-soft;
  opacity: 0.55;
}

ColorPicker {
  gap: 4;
  icon-size: 36;
  padding-y: 2;
  offset-x: 20;
  offset-y: 28;
}

ColorPickerSwatch {
  background: transparent;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: gap;
  font-size: font;
  opacity: 1;
  material: premium;
}

ListBox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: gap;
  padding-y: gap;
  offset-y: 8;
  gap: gap;
  opacity: 1;
  material: premium;
}

ListBoxItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: gap;
  icon-size: 30;
  font-size: font;
  opacity: 1;
  material: premium;
}

ListBoxItem:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

ListBoxItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

ListBoxItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

TreeView {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: gap;
  padding-y: gap;
  gap: gap;
  opacity: 1;
  material: premium;
}

TreeViewItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: 6;
  gap: 2;
  icon-size: 16;
  offset-x: 18;
  font-size: font;
  opacity: 1;
  material: premium;
}

TreeViewItem:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

TreeViewItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

TreeViewItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

ListBoxMulti {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: 1;
  padding-x: gap;
  padding-y: gap;
  gap: gap;
  opacity: 1;
  material: premium;
}

ListBoxMultiItem {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: gap;
  icon-size: 28;
  font-size: font;
  opacity: 1;
  material: premium;
}

ListBoxMultiItem:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

ListBoxMultiItem:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

ListBoxMultiItem:focus {
  border: focus;
}

ListBoxMultiItem:disabled {
  foreground: muted;
  opacity: 0.45;
}

DragDropTarget {
  background: transparent;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  material: premium;
}

DragDropTarget:hover {
  border: accent-border;
  border-width: 2;
}

Selectable {
  background: transparent;
  foreground: text;
  border: transparent;
  focus: focus;
  radius: radius;
  border-width: 0;
  padding-x: space.3;
  padding-y: 8;
  font-size: font;
  opacity: 1;
  material: premium;
}

Selectable:hover {
  background: button-hover;
  background-end: button-hover-end;
  foreground: text;
}

Selectable:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
  foreground: text;
}

Selectable:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
}

Selectable:disabled {
  foreground: muted;
  opacity: 0.45;
}

Fieldset {
  background: canvas;
  foreground: muted;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: 8;
  padding-y: 8;
  gap: 9;
  font-size: font;
  opacity: 1;
  material: app;
}

PanedView {
  background: transparent;
  foreground: muted;
  border: transparent;
  opacity: 1;
  material: app;
}

PanedView[role=Handle] {
  background: border-soft;
  background-end: border;
  foreground: muted;
  border: transparent;
  radius: radius;
  border-width: 0;
  icon-size: 8;
  opacity: 1;
  material: premium;
}

PanedView[role=Handle]:hover {
  background: border;
  background-end: focus;
}

PanedView[role=Handle]:pressed {
  background: accent;
  background-end: accent-end;
}

Toast {
  background: card;
  background-end: card-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  padding-x: 14;
  padding-y: 10;
  gap: 18;
  opacity: 0.98;
  material: premium;
}

Toast[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Collapsible {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Collapsible[role=Header] {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  font-size: font;
  padding-x: 6;
  padding-y: 8;
  icon-size: 20;
  opacity: 1;
  material: premium;
}

Collapsible[role=Header]:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Collapsible[role=Header]:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Collapsible[role=Header]:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Collapsible[role=Header]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=TreeHeader] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: 0;
  font-size: font;
  icon-size: 20;
  padding-x: 20;
  padding-y: 8;
  opacity: 1;
  material: app;
}

Collapsible[role=TreeHeader]:hover {
  background: button;
  background-end: button-end;
  material: premium;
}

Collapsible[role=TreeHeader]:selected {
  background: transparent;
  foreground: accent;
}

Collapsible[role=TreeHeader]:disabled {
  foreground: muted;
  opacity: 0.45;
}

Collapsible[role=Close] {
  background: transparent;
  foreground: muted;
  border: transparent;
  font-size: font;
  icon-size: 28;
  opacity: 1;
  material: app;
}

Collapsible[role=Close]:hover {
  foreground: text;
}

Collapsible[role=Close]:disabled {
  opacity: 0.45;
}

TitleBar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

TitleBar[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  gap: 4;
  icon-size: 32;
  offset-x: 60;
  opacity: 1;
  material: premium;
}

TitleBar[role=Title] {
  background: transparent;
  foreground: text;
  border: transparent;
  font-size: font.md;
  icon-size: 48;
  padding-x: 16;
  opacity: 1;
  material: app;
}

TitleBar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: 0;
  icon-size: 20;
  padding-x: 10;
  opacity: 1;
  material: app;
}

TitleBar[role=Action]:hover {
  background: button;
  background-end: button-end;
  material: premium;
}

TitleBar[role=Action]:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Toolbar {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Toolbar[role=Bar] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  radius: 0;
  border-width: border;
  padding-x: 12;
  opacity: 1;
  material: premium;
}

Toolbar[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  opacity: 1;
  material: app;
}

Toolbar[role=Action] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: 0;
  padding-x: 8;
  gap: 6;
  icon-size: 20;
  opacity: 1;
  material: app;
}

Toolbar[role=Action]:hover {
  background: button;
  background-end: button-end;
  material: premium;
}

Toolbar[role=Action]:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Modal {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Modal[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.62;
  material: app;
}

Modal[role=Panel] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: 18;
  padding-y: 18;
  gap: 24;
  icon-size: 280;
  offset-x: 420;
  offset-y: 58;
  opacity: 1;
  material: premium;
}

Modal[role=Title] {
  foreground: text;
  font-size: font.md;
  padding-x: 24;
  padding-y: 18;
  icon-size: 48;
  offset-y: 14;
  opacity: 1;
  material: app;
}

Modal[role=Message] {
  foreground: text;
  font-size: font.md;
  padding-x: 120;
  padding-y: 160;
  gap: 18;
  icon-size: 38;
  offset-y: 18;
  opacity: 1;
  material: app;
}

Modal[role=Action] {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: 24;
  gap: 8;
  icon-size: 44;
  offset-x: 88;
  offset-y: 150;
  opacity: 1;
  material: premium;
}

Modal[role=Action]:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Modal[role=Action]:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Modal[role=Action][tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Modal[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: 0;
  padding-x: 8;
  padding-y: 16;
  gap: 6;
  icon-size: 20;
  offset-x: 120;
  offset-y: 96;
  opacity: 1;
  material: app;
}

Modal[role=Close]:hover {
  background: button;
  background-end: button-end;
  material: premium;
}

Modal[role=Close]:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Guide {
  background: transparent;
  foreground: text;
  border: transparent;
  opacity: 1;
  material: app;
}

Guide[role=Scrim] {
  background: #000000;
  foreground: text;
  border: transparent;
  opacity: 0.36;
  material: app;
}

Guide[role=Bar] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  radius: 0;
  border-width: 0;
  padding-x: 12;
  gap: 12;
  icon-size: 48;
  offset-x: 48;
  opacity: 1;
  material: premium;
}

Guide[role=Divider] {
  background: transparent;
  foreground: text;
  border: card-border;
  border-width: border;
  opacity: 1;
  material: app;
}

Guide[role=Panel] {
  background: card;
  background-end: card-end;
  foreground: text;
  border: card-border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: 12;
  padding-y: 12;
  gap: 20;
  offset-x: 300;
  offset-y: 112;
  opacity: 1;
  material: premium;
}

Guide[role=Anchor] {
  background: transparent;
  foreground: accent;
  border: accent-border;
  border-width: border;
  padding-x: 4;
  opacity: 1;
  material: app;
}

Guide[role=Label] {
  foreground: text;
  font-size: font;
  padding-x: 8;
  padding-y: 6;
  gap: 8;
  opacity: 1;
  material: app;
}

Guide[role=Action] {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 7;
  gap: 8;
  icon-size: 19;
  offset-y: 34;
  opacity: 1;
  material: premium;
}

Guide[role=Action]:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Guide[role=Close] {
  background: transparent;
  foreground: text;
  border: transparent;
  radius: radius;
  border-width: 0;
  padding-x: 6;
  gap: 12;
  icon-size: 16;
  offset-y: 28;
  opacity: 1;
  material: app;
}

Guide[role=Close]:hover {
  background: button;
  background-end: button-end;
  material: premium;
}

TableView {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-y: 28;
  opacity: 1;
  material: app;
}

TableView[role=Panel] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

TableView[role=Header] {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  radius: radius;
  border-width: border;
  padding-x: 6;
  offset-y: 30;
  font-size: font;
  opacity: 1;
  material: premium;
}

TableView[role=Header]:hover {
  background: button-hover;
  background-end: button-hover-end;
}

TableView[role=Header]:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

TableView[role=Row] {
  background: #171d27aa;
  foreground: text;
  border: transparent;
  radius: 0;
  border-width: 0;
  opacity: 1;
  material: app;
}

TableView[role=Cell] {
  background: transparent;
  foreground: text;
  border: transparent;
  offset-x: 32;
  font-size: font;
  opacity: 1;
  material: app;
}

TableView[role=Selection] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

TableView[role=Divider] {
  background: transparent;
  foreground: muted;
  border: border-soft;
  border-width: border;
  padding-x: 5;
  offset-y: 8;
  opacity: 1;
  material: app;
}

Plot {
  background: card;
  background-end: card-end;
  foreground: muted;
  border: card-border;
  radius: radius;
  border-width: border;
  padding-x: 6;
  padding-y: 4;
  font-size: font;
  opacity: 1;
  material: premium;
}

PlotMark {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  opacity: 1;
  material: premium;
}

Text {
  foreground: text;
  font-size: font;
}

Page {
  gap: 0;
  padding-x: 0;
}

Section {
  gap: 0;
  padding-x: 0;
}

Heading {
  foreground: text;
  font-size: 24;
}

ParagraphText {
  foreground: text;
  font-size: font;
  gap: 0;
}

Link {
  foreground: accent;
  font-size: font;
  opacity: 1;
  material: premium;
}

Link:hover {
  foreground: accent-hover;
}

Link:disabled {
  foreground: muted;
  opacity: 0.45;
}

Separator {
  background: border;
  foreground: muted;
  gap: gap;
}

Separator[role=Line] {
  background: border;
  foreground: muted;
  gap: gap;
}

Separator[role=Label] {
  background: border;
  foreground: muted;
  font-size: font;
  gap: gap;
}

Separator[role=Bullet] {
  background: border;
  foreground: muted;
  icon-size: 6;
  gap: gap;
}

Progress {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Progress[role=Track] {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Progress[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  opacity: 1;
  material: premium;
}

Progress[role=Label] {
  background: transparent;
  foreground: text;
  border: transparent;
  padding-x: 6;
  font-size: font;
  opacity: 1;
  material: premium;
}

@layer components;

Button {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: space.4;
  padding-y: pad.control-y;
  gap: gap;
  font-size: font;
  icon-size: icon;
  opacity: 1;
  material: premium;
}

Button:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Button:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Button:focus {
  border: focus;
}

Button:disabled {
  foreground: muted;
  opacity: 0.55;
}

Button:loading {
  foreground: accent;
}

Button[tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Button[tone=Accent]:hover {
  background: accent-hover;
  background-end: accent-hover-end;
}

Button[tone=Accent]:loading {
  foreground: accent;
}

Dropdown {
  background: surface;
  background-end: dropdown-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: 14;
  padding-y: pad.dropdown-y;
  gap: gap;
  offset-y: 16;
  font-size: font;
  material: premium;
}

Dropdown:hover {
  background: button-hover;
  background-end: button-hover-end;
  border: focus;
}

Dropdown:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Dropdown:focus {
  border: focus;
}

Dropdown:disabled {
  foreground: muted;
  opacity: 0.55;
}

Dropdown[tone=Accent] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Dropdown:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Slider {
  background: button-pressed;
  background-end: button-pressed-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Slider[role=Track] {
  padding-x: 32;
  padding-y: 6;
  gap: 0;
  icon-size: 8;
  offset-x: 36;
  offset-y: 28;
  opacity: 1;
}

Slider[role=Label] {
  foreground: text;
  font-size: font;
}

Slider:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Slider[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Slider:disabled {
  foreground: muted;
  opacity: 0.55;
}

SliderThumb {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  focus: accent;
  radius: radius;
  border-width: border;
  gap: 8;
  icon-size: 22;
  opacity: 1;
  material: premium;
}

SliderThumb:disabled {
  background: muted;
  foreground: button;
  border: border;
  focus: border;
  opacity: 0.55;
}

Scroll {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border-soft;
  focus: focus;
  radius: radius;
  border-width: border;
  icon-size: 10;
  padding-x: 16;
  gap: 20;
  offset-y: 42;
  opacity: 1;
  material: premium;
}

ScrollThumb {
  background: muted;
  foreground: surface;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  icon-size: 16;
  padding-x: 2;
  opacity: 1;
  material: premium;
}

ScrollThumb:hover {
  background: accent-hover;
  background-end: accent-hover-end;
  foreground: accent-ink;
  border: accent-border;
}

ScrollThumb:pressed {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Toggle {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  opacity: 1;
  material: premium;
}

Toggle[role=Track] {
  padding-x: 54;
  padding-y: 32;
  gap: 4;
  opacity: 1;
}

Toggle[role=Label] {
  foreground: text;
  font-size: font;
  opacity: 1;
}

Toggle:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Toggle:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Toggle[role=Fill] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  gap: 3;
}

Toggle:disabled {
  foreground: muted;
  opacity: 0.55;
}

ToggleThumb {
  background: surface;
  background-end: surface-end;
  foreground: #ffffff;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: premium;
}

ToggleThumb[tone=Accent] {
  background: accent-ink;
  foreground: #ffffff;
  border: accent-border;
  focus: accent;
  icon-size: 24;
}

ToggleThumb:disabled {
  background: muted;
  foreground: button;
  border: border;
  focus: border;
  opacity: 0.55;
}

Checkbox {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  padding-x: 22;
  padding-y: 3;
  gap: 4;
  icon-size: 20;
  opacity: 1;
  material: premium;
}

Checkbox:hover {
  background: button-hover;
  background-end: button-hover-end;
}

Checkbox:pressed {
  background: button-pressed;
  background-end: button-pressed-end;
}

Checkbox:selected {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
}

Checkbox:disabled {
  foreground: muted;
  opacity: 0.55;
}

Checkbox[role=Box] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: premium;
}

Checkbox[role=Mark] {
  background: accent;
  background-end: accent-end;
  foreground: accent-ink;
  border: accent-border;
  focus: focus;
  border-width: border;
  padding-x: 5;
  icon-size: 2.4;
  opacity: 1;
  material: premium;
}

Checkbox[role=Label] {
  foreground: text;
  gap: 10;
  font-size: font;
  opacity: 1;
  material: premium;
}

Checkbox[role=Label]:disabled {
  foreground: muted;
  opacity: 0.55;
}

Radio {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  opacity: 1;
  material: premium;
}

Radio:hover {
  border: focus;
}

Radio:selected {
  background: accent;
  background-end: accent-end;
  foreground: text;
  border: accent-border;
}

Radio:disabled {
  foreground: muted;
  opacity: 0.55;
}

Radio[role=Ring] {
  background: surface;
  background-end: surface-end;
  foreground: text;
  border: border;
  focus: focus;
  border-width: border;
  padding-x: 40;
  gap: 4;
  opacity: 1;
  material: premium;
}

Radio[role=Mark] {
  background: accent;
  background-end: accent-end;
  foreground: text;
  border: accent-border;
  focus: focus;
  border-width: border;
  font-size: font;
  opacity: 1;
  material: premium;
}

Radio[role=Label] {
  foreground: text;
  font-size: font;
  opacity: 1;
  material: premium;
}

Radio[role=Label]:disabled {
  foreground: muted;
  opacity: 0.55;
}

TextField {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.field-y;
  font-size: font.md;
  material: premium;
}

TextArea {
  background: button;
  background-end: button-end;
  foreground: text;
  border: border;
  focus: focus;
  radius: radius;
  border-width: border;
  padding-x: space.3;
  padding-y: pad.control-y;
  font-size: font.md;
  gap: 4;
  material: premium;
}
`

func RegisterBuiltInStylePacks() bool {
	for _, pack := range []struct {
		id, label, description, source string
	}{
		{"material", "Material", "Clean Material-like controls with flat paint", materialStyleSource},
		{"tk", "TK", "Dense toolkit controls for desktop utilities", tkStyleSource},
		{"vanilla", "Vanilla", "Current Kryon default controls as an explicit pack", vanillaStyleSource},
		{"glow", "Glow", "Current glow treatment as an explicit pack", glowStyleSource},
		{"lightfield", "Lightfield", "Premium translucent Lightfield controls", lightfieldStyleSource},
	} {
		if !RegisterStylePackSource(pack.source, pack.label, pack.description) {
			return false
		}
		if FindStylePack(pack.id) == nil {
			return false
		}
	}
	return SetActiveStylePack("material")
}
