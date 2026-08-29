export interface RuntimeOptions {
  app?: AppMeta;
  target?: Element | string | null;
}

export interface AppMeta {
  title: string;
  width: number;
  height: number;
  fps: number;
  frame: string;
}

export interface Runtime {
  app: AppMeta | null;
  target: Element | string | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
  mounted: boolean;
}

export interface RuntimeItem {
  kind: string;
  name?: string;
  args?: unknown;
  text?: string;
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

export function createRuntime(options?: RuntimeOptions): Runtime;
export function beginFrame(rt: Runtime): Runtime;
export function endFrame(rt: Runtime): ReturnType<typeof snapshot>;
export function snapshot(rt: Runtime): {
  app: AppMeta | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
};
export function widget(rt: Runtime, name: string, args: string): RuntimeItem;
export function statement(rt: Runtime, text: string): RuntimeItem;
export function expr(text: string): { kind: "expr"; text: string };
export function struct(type: string, value: unknown): { type: string; value: unknown };
export function ref<T>(object: Record<string, T>, key: string): Ref<T>;
export function stateForModule(name?: string): Record<string, unknown>;
export function hostCall(host: unknown, method: string, args?: unknown[]): unknown;
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
export function ScaleUIPx(value: number): number;
export function GetScreenWidth(): number;
export function GetScreenHeight(): number;
export function GetThemeBackground(): ColorValue;
export function GetThemeSurface(): ColorValue;
export function GetThemeText(): ColorValue;
export function GetThemeButton(): ColorValue;
export function GetThemeIcon(): ColorValue;
export function GetThemeLink(): ColorValue;
export function Fade(color: ColorValue, alpha: number): ColorValue;

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

export function Background(...args: unknown[]): unknown;
export function Bevel(...args: unknown[]): unknown;
export function BottomNav(...args: unknown[]): unknown;
export function Button(...args: unknown[]): unknown;
export function CanvasGrid(...args: unknown[]): unknown;
export function Checkbox(...args: unknown[]): unknown;
export function ClearBackground(...args: unknown[]): unknown;
export function Collapsible(...args: unknown[]): unknown;
export function Column(...args: unknown[]): unknown;
export function Combobox(...args: unknown[]): unknown;
export function Dropdown(...args: unknown[]): unknown;
export function EndCanvas(...args: unknown[]): unknown;
export function EndScroll(...args: unknown[]): unknown;
export function Href(...args: unknown[]): unknown;
export function IconButton(...args: unknown[]): unknown;
export function IconTexture(...args: unknown[]): unknown;
export function LabelFrame(...args: unknown[]): unknown;
export function ListBox(...args: unknown[]): unknown;
export function Modal(...args: unknown[]): unknown;
export function Notebook(...args: unknown[]): unknown;
export function Paragraph(...args: unknown[]): unknown;
export function Picture(...args: unknown[]): unknown;
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
export function TextInRect(...args: unknown[]): unknown;
export function TextLines(...args: unknown[]): unknown;
export function TitleBar(...args: unknown[]): unknown;
export function Toggle(...args: unknown[]): unknown;
export function Toolbar(...args: unknown[]): unknown;
export function TopNav(...args: unknown[]): unknown;
