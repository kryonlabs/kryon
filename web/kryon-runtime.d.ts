export interface RuntimeOptions {
  app?: AppMeta;
  target?: Element | string | null;
  webStyleSheets?: string | WebStyleSheet | Array<string | WebStyleSheet>;
}

export interface AppMeta {
  title: string;
  width: number;
  height: number;
  fps: number;
  frame: string;
  styles?: AppStyleMeta[];
  routes?: AppRouteMeta[];
}

export interface AppStyleMeta {
  kind: "file" | "builtin" | string;
  target: string;
  alias: string;
  source?: string;
}

export interface AppRouteMeta {
  id: string;
  title: string;
  group: string;
  page: string;
  path: string;
}

export interface Runtime {
  app: AppMeta | null;
  target: Element | string | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
  mounted: boolean;
  webStyleSheets?: Array<string | WebStyleSheet>;
  instanceFrame: number;
  instances: Map<string, Map<bigint, { value: unknown; frameSeen: number }>>;
  input: {
    events: unknown[];
    focus: number;
    clipboard: string;
    selections: Map<number, { anchor: number; cursor: number }>;
    dropdownOpen: number | null;
    focusOrder: number[];
    lastFocusOrder: number[];
  };
  QueueText(text: string): void;
  QueueKey(key: number): void;
  QueueShiftKey(key: number): void;
  QueueShortcut(key: number): void;
  QueueTap(x: number, y: number): void;
  SetClipboardText(text: string): void;
  ClipboardText(): string;
  SetSelection(focusID: number, anchor: number, cursor: number): void;
  SetFocus(id: number): void;
  Focus(): number;
}

export interface RuntimeItem {
  kind: string;
  name?: string;
  args?: unknown;
  meta?: Record<string, unknown> | null;
  text?: string;
}

export interface WebDocumentNode {
  index: number;
  kind: string;
  tag: string;
  key: string;
  name: string;
  path: string;
  parentPath: string;
  sourcePath: string;
  sourceLine: number;
  domId: string;
  domName: string;
  classes: string[];
  title: string;
  placeholder: string;
  tabIndex: number | null;
  text: string;
  value: unknown;
  level: number;
  href: string;
  target: string;
  rel: string;
  dataAttrs: Record<string, string>;
  inputType: string;
  alt: string;
  asset: string;
  role: string;
  ariaLabel: string;
  ariaDescription: string;
  ariaDescribedBy: string;
  ariaControls: string;
  ariaLive: string;
  onClick: string;
  onInput: string;
  onChange: string;
  onKey: string;
  onSubmit: string;
  onFocus: string;
  onBlur: string;
  onMouseEnter: string;
  onMouseLeave: string;
  onMouseDown: string;
  onMouseUp: string;
  action: (() => unknown) | null;
  inputAction: ((value: unknown) => unknown) | null;
  changeAction: ((value: unknown) => unknown) | null;
  keyAction: ((key: string) => unknown) | null;
  submitAction: ((values: Record<string, unknown>) => unknown) | null;
  focusAction: (() => unknown) | null;
  blurAction: (() => unknown) | null;
  mouseEnterAction: (() => unknown) | null;
  mouseLeaveAction: (() => unknown) | null;
  mouseDownAction: (() => unknown) | null;
  mouseUpAction: (() => unknown) | null;
  pageTitle: string;
  pageDescription: string;
  pageCanonicalURL: string;
  pageThemeColor: string;
  bounds: { x: number; y: number; width: number; height: number };
  hasBounds: boolean;
  state: {
    disabled: boolean;
    loading: boolean;
    selected: boolean;
    checked: boolean;
    invalid: boolean;
    expanded: boolean;
    open: boolean;
    hover: boolean;
    pressed: boolean;
    focus: boolean;
  };
  styleFacts: WebNodeStyleFacts;
}

export interface WebNodeStyleFacts {
  kind: string;
  tag: string;
  key: string;
  name: string;
  path: string;
  parentPath: string;
  sourcePath: string;
  sourceLine: number;
  id: string;
  domName: string;
  href: string;
  target: string;
  rel: string;
  inputType: string;
  classes: string[];
  dataAttrs: Record<string, string>;
  role: string;
  state: Record<string, boolean>;
}

export interface WebDocumentFrame {
  app: AppMeta | null;
  nodes: WebDocumentNode[];
  metadata: {
    title: string;
    description: string;
    canonicalURL: string;
    themeColor: string;
  };
}

export interface WebAccessibilityNode {
  path: string;
  sourcePath: string;
  sourceLine: number;
  name: string;
  kind: string;
  tag: string;
  id: string;
  classes: string[];
  role: string;
  label: string;
  description: string;
  text: string;
  value: unknown;
  href: string;
  inputType: string;
  level: number;
  state: Record<string, boolean>;
}

export interface WebAccessibilitySnapshot {
  title: string;
  description: string;
  nodes: WebAccessibilityNode[];
}

export interface WebDOMObject {
  ref: string;
  node: WebDocumentNode;
  element: Element;
}

export interface WebStyleRule {
  selector: Record<string, unknown>;
  style: Record<string, unknown>;
  layer: number;
  order: number;
  score: number;
}

export interface WebStyleSheet {
  pack?: string;
  rules: WebStyleRule[];
}

export interface Ref<T = unknown> {
  value: T;
  object: Record<string, T> | null;
  key: string;
}

export interface ColorValue {
  r: number;
  g: number;
  b: number;
  a: number;
}

export interface ThemeValue {
  name?: string;
  mode: number;
  colors: Record<string, ColorValue>;
  metrics: Record<string, number>;
}

export interface ThemeFamilyValue {
  name?: string;
  light: ThemeValue;
  dark: ThemeValue;
}

export function createRuntime(options?: RuntimeOptions): Runtime;
export function instanceState<T>(rt: Runtime, type: string, key: number | bigint,
  create: () => T): { value: T; frameSeen: number };
export function viewport(rt: Runtime, app?: AppMeta | null): {
  x: number;
  y: number;
  width: number;
  height: number;
};
export function beginFrame(rt: Runtime): Runtime;
export function endFrame(rt: Runtime): ReturnType<typeof snapshot>;
export function snapshot(rt: Runtime): {
  app: AppMeta | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
};
export function widget(rt: Runtime, name: string, args: string,
  state?: Record<string, unknown> | null, meta?: Record<string, unknown> | null): unknown;
export function statement(rt: Runtime, text: string): RuntimeItem;
export function expr(text: string): { kind: "expr"; text: string };
export function struct(type: string, value: unknown): { type: string; value: unknown };
export function copyValue<T>(value: T): T;
export function recordValue<T>(type: string, value: T): T;
export function ref<T>(object: Record<string, T>, key: string): Ref<T>;
export function stateForModule(name?: string): Record<string, unknown>;
export function hostCall(host: unknown, method: string, args?: unknown[]): unknown;
export function webDocumentFrame(rt: Runtime): WebDocumentFrame;
export function webNodeStyleFacts(node: WebDocumentNode): WebNodeStyleFacts;
export function webAccessibilitySnapshot(source: Runtime | WebDocumentFrame): WebAccessibilitySnapshot;
export function parseWebStyleSheet(source: string): WebStyleSheet;
export function resolveWebStyle(node: WebDocumentNode, sheets?: string | WebStyleSheet | Array<string | WebStyleSheet>): Record<string, unknown>;
export function setWebStyleSheets(rt: Runtime, sheets: string | WebStyleSheet | Array<string | WebStyleSheet>): Runtime;
export function renderWebDocument(rt: Runtime, target: Element | string | null): Runtime;
export function findWebNode(rt: Runtime, query: string): WebDocumentNode | null;
export function webNodeQuery(rt: Runtime, selector: string): WebDocumentNode | null;
export function webNodeQueryAll(rt: Runtime, selector: string): WebDocumentNode[];
export function findWebElement(target: Element | string | null, query: string): Element | null;
export function webDOMObject(target: Element | string | null, query: string): WebDOMObject | null;
export function webDOMObjects(target: Element | string | null): WebDOMObject[];
export function webDOMQuery(target: Element | string | null, selector: string): WebDOMObject | null;
export function webDOMQueryAll(target: Element | string | null, selector: string): WebDOMObject[];
export function webFormValue(target: Element | string | null, query: string): unknown;
export function webFormValues(target: Element | string | null): Record<string, unknown>;
export function mount(rt: Runtime, target: Element | string | null): Runtime;
export function Color(r?: number, g?: number, b?: number, a?: number): ColorValue;
export function NewVector2(x?: number, y?: number): { x: number; y: number };
export function NewRectangle(x?: number, y?: number, width?: number, height?: number): {
  x: number;
  y: number;
  width: number;
  height: number;
};
export function Key(value: unknown): string;
export function Scale(value: number): number;
export function GetScreenWidth(): number;
export function GetScreenHeight(): number;
export function GetViewWidth(): number;
export function GetViewHeight(): number;
export function GetPageSidePadding(): number;
export function GetThemeBackground(): ColorValue;
export function GetThemeSurface(): ColorValue;
export function GetThemeText(): ColorValue;
export function ThemeDefaultLight(): ThemeValue;
export function ThemeDefaultDark(): ThemeValue;
export function SetTheme(theme: ThemeValue): void;
export function SetThemeFamily(family: ThemeFamilyValue): void;
export function GetThemeFamily(): ThemeFamilyValue | null;
export function GetTheme(): ThemeValue;
export function SetThemeMode(mode: number): void;
export function GetThemeMode(): number;
export function GetThemeButton(): ColorValue;
export function GetThemeButtonHover(): ColorValue;
export function GetThemeCircle(): ColorValue;
export function GetThemeIcon(): ColorValue;
export function GetThemeLink(): ColorValue;
export function SystemThemePrefersDark(): boolean;
export function SetPageTitle(title: string): void;
export function SetPageDescription(description: string): void;
export function SetPageCanonicalURL(url: string): void;
export function SetPageThemeColor(color: ColorValue | string): void;
export function GetRoutePath(): string;
export function GetRouteHash(): string;
export function GetRouteVersion(): number;
export function PushRoute(path: string): string;
export function ReplaceRoute(path: string): string;
export function Fade(color: ColorValue, alpha: number): ColorValue;
export function DarkenColor(color: ColorValue, amount: number): ColorValue;
export function LightenColor(color: ColorValue, amount: number): ColorValue;
export function TextFormat(format: string, ...values: unknown[]): string;
export function GetUIClipboardTextValue(): string;
export function UpdateFileDialog(...args: unknown[]): number;
export function IsKeyPressed(key: number): boolean;
export function IsKeyDown(key: number): boolean;
export function IsMouseButtonReleased(button: number): boolean;
export function GetThemeMetrics(): Record<string, unknown>;
export function BeginScrollContainer(...args: unknown[]): unknown;
export function CanvasHitTest(canvas: unknown, screen: unknown): Record<string, unknown>;

export const Text8: number;
export const Text12: number;
export const Text14: number;
export const Text16: number;
export const Text18: number;
export const Text20: number;
export const Text24: number;
export const Text32: number;
export const Text48: number;
export const WHITE: ColorValue;
export const BLACK: ColorValue;
export const RAYWHITE: ColorValue;
export const BLANK: ColorValue;
export const LIGHTGRAY: ColorValue;
export const GRAY: ColorValue;
export const DARKGRAY: ColorValue;
export const YELLOW: ColorValue;
export const GOLD: ColorValue;
export const ORANGE: ColorValue;
export const PINK: ColorValue;
export const RED: ColorValue;
export const MAROON: ColorValue;
export const GREEN: ColorValue;
export const LIME: ColorValue;
export const DARKGREEN: ColorValue;
export const SKYBLUE: ColorValue;
export const BLUE: ColorValue;
export const DARKBLUE: ColorValue;
export const PURPLE: ColorValue;
export const VIOLET: ColorValue;
export const DARKPURPLE: ColorValue;
export const BEIGE: ColorValue;
export const BROWN: ColorValue;
export const DARKBROWN: ColorValue;
export const MAGENTA: ColorValue;
export const ButtonToneAccent: number;
export const ButtonToneNeutral: number;
export const ButtonToneDanger: number;
export const ButtonToneSuccess: number;
export const ButtonToneWarning: number;
export const ButtonEmphasisFilled: number;
export const ButtonEmphasisSoft: number;
export const ButtonEmphasisOutline: number;
export const ButtonEmphasisGhost: number;
export const ButtonEmphasisLink: number;
export const ButtonStateAuto: number;
export const ControlSizeMedium: number;
export const ControlSizeSmall: number;
export const ControlSizeLarge: number;
export const IconPlacementLeading: number;
export const IconPlacementTrailing: number;
export const StyleBackground: number;
export const StyleForeground: number;
export const StyleBorder: number;
export const StyleFocus: number;
export const StyleRadius: number;
export const StyleBorderWidth: number;
export const StyleOpacity: number;
export const StylePaddingX: number;
export const StylePaddingY: number;
export const StyleGap: number;
export const StyleFontSize: number;
export const StyleIconSize: number;
export const StyleContentOffset: number;
export const StyleBackgroundEnd: number;
export const StyleMaterial: number;
export const StyleTypeface: number;
export const MaterialLightfield: number;
export const MaterialFlat: number;
export const ButtonStateNormal: number;
export const ButtonStateHover: number;
export const ButtonStatePressed: number;
export const ButtonStateFocus: number;
export const ButtonStateDisabled: number;
export const ButtonStateLoading: number;
export const ButtonStateSelected: number;
export const KeyTab: number;
export const KeyBackspace: number;
export const KeyRight: number;
export const KeyLeft: number;
export const KeyC: number;
export const MouseButtonLeft: number;
export const KEY_SPACE: number;
export const KEY_C: number;
export const KEY_TAB: number;
export const KEY_BACKSPACE: number;
export const KEY_RIGHT: number;
export const KEY_LEFT: number;
export const KEY_DOWN: number;
export const KEY_UP: number;
export const MOUSE_BUTTON_LEFT: number;
export const THEME_SKY: number;
export const THEME_COUNT: number;
export const THEME_MODE_SYSTEM: number;
export const THEME_MODE_LIGHT: number;
export const THEME_MODE_DARK: number;
export const THEME_SOURCE_SYSTEM: number;
export const THEME_SOURCE_APP: number;
export const THEME_STYLE_SYSTEM: number;
export const THEME_STYLE_DEFAULT: number;
export function SetFancyEffectsEnabled(enabled: unknown): void;
export function FancyEffectsEnabled(): number;

export function Background(...args: unknown[]): unknown;
export function Bevel(...args: unknown[]): unknown;
export function BottomNav(...args: unknown[]): unknown;
export function Button(...args: unknown[]): unknown;
export function Card(...args: unknown[]): unknown;
export function BeginButton(...args: unknown[]): unknown;
export function BeginCanvas(canvas: unknown): Record<string, unknown>;
export function CanvasGrid(...args: unknown[]): unknown;
export function Checkbox(...args: unknown[]): unknown;
export function ClearBackground(...args: unknown[]): unknown;
export function Collapsible(...args: unknown[]): unknown;
export function Column(...args: unknown[]): unknown;
export function Dropdown(...args: unknown[]): unknown;
export function EndCanvas(...args: unknown[]): unknown;
export function EndScroll(...args: unknown[]): unknown;
export function Icon(...args: unknown[]): unknown;
export function Fieldset(...args: unknown[]): unknown;
export function Link(...args: unknown[]): unknown;
export function ListBox(...args: unknown[]): unknown;
export function Modal(...args: unknown[]): unknown;
export function Paragraph(...args: unknown[]): unknown;
export function Image(...args: unknown[]): unknown;
export function Progress(...args: unknown[]): unknown;
export function Radio(...args: unknown[]): unknown;
export function Rect(...args: unknown[]): unknown;
export function Row(...args: unknown[]): unknown;
export function Screen(...args: unknown[]): unknown;
export function Scroll(...args: unknown[]): unknown;
export function SelectableText(...args: unknown[]): unknown;
export function SetCurrentTheme(...args: unknown[]): unknown;
export function SetThemeDarkMode(...args: unknown[]): unknown;
export function ShowToast(...args: unknown[]): unknown;
export function Slider(...args: unknown[]): unknown;
export function Spinbox(...args: unknown[]): unknown;
export function Stack(...args: unknown[]): unknown;
export function TabBar(...args: unknown[]): unknown;
export function Text(...args: unknown[]): unknown;
export function TextArea(...args: unknown[]): unknown;
export function TextField(...args: unknown[]): unknown;
export function TitleBar(...args: unknown[]): unknown;
export function Toggle(...args: unknown[]): unknown;
export function Toolbar(...args: unknown[]): unknown;
