// Kryon web runtime for k2js-generated ESM.

export const Text8 = 8;
export const Text12 = 12;
export const Text14 = 14;
export const Text16 = 16;
export const Text18 = 18;
export const Text20 = 20;
export const Text24 = 24;
export const Text32 = 32;
export const Text48 = 48;

export const WHITE = Color(255, 255, 255, 255);
export const BLACK = Color(0, 0, 0, 255);
export const RAYWHITE = Color(245, 245, 245, 255);
export const BLANK = Color(0, 0, 0, 0);
export const LIGHTGRAY = Color(200, 200, 200, 255);
export const GRAY = Color(130, 130, 130, 255);
export const DARKGRAY = Color(80, 80, 80, 255);
export const YELLOW = Color(253, 249, 0, 255);
export const GOLD = Color(255, 203, 0, 255);
export const ORANGE = Color(255, 161, 0, 255);
export const PINK = Color(255, 109, 194, 255);
export const RED = Color(230, 41, 55, 255);
export const MAROON = Color(190, 33, 55, 255);
export const GREEN = Color(0, 228, 48, 255);
export const LIME = Color(0, 158, 47, 255);
export const DARKGREEN = Color(0, 117, 44, 255);
export const SKYBLUE = Color(102, 191, 255, 255);
export const BLUE = Color(0, 121, 241, 255);
export const DARKBLUE = Color(0, 82, 172, 255);
export const PURPLE = Color(200, 122, 255, 255);
export const VIOLET = Color(135, 60, 190, 255);
export const DARKPURPLE = Color(112, 31, 126, 255);
export const BEIGE = Color(211, 176, 131, 255);
export const BROWN = Color(127, 106, 79, 255);
export const DARKBROWN = Color(76, 63, 47, 255);
export const MAGENTA = Color(255, 0, 255, 255);

export const ButtonStylePrimary = 0;
export const ButtonStyleSecondary = 1;
export const ButtonStyleDanger = 2;
export const ButtonStyleTab = 3;
export const ButtonStyleTabSelected = 4;
export const SideTop = 0;
export const SideBottom = 1;
export const SideLeft = 2;
export const SideRight = 3;
export const SyntaxNone = 0;
export const SyntaxKry = 1;
export const SyntaxC = 2;
export const SyntaxMake = 3;
export const PICTURE_FIT_STRETCH = 0;
export const PICTURE_FIT_CONTAIN = 1;
export const PICTURE_FIT_COVER = 2;

export function createRuntime(options = {}) {
  return {
    app: options.app || null,
    target: options.target || null,
    frame: [],
    statements: [],
    hostCalls: [],
    mounted: false
  };
}

export function beginFrame(rt) {
  rt.frame = [];
  rt.statements = [];
  rt.hostCalls = [];
  return rt;
}

export function endFrame(rt) {
  return snapshot(rt);
}

export function snapshot(rt) {
  return {
    app: rt.app || null,
    frame: rt.frame.slice(),
    statements: rt.statements.slice(),
    hostCalls: rt.hostCalls.slice()
  };
}

export function widget(rt, name, args) {
  const item = { kind: "widget", name, args };
  rt.frame.push(item);
  return item;
}

export function statement(rt, text) {
  const item = { kind: "statement", text };
  rt.statements.push(item);
  return item;
}

export function expr(text) {
  return { kind: "expr", text };
}

export function struct(type, value) {
  return { type, value };
}

export function ref(object, key) {
  return {
    get value() { return object ? object[key] : undefined; },
    set value(next) {
      if (object) object[key] = next;
    },
    object,
    key
  };
}

export function stateForModule() {
  return {};
}

export function hostCall(host, method, args = []) {
  if (host && typeof host[method] === "function")
    return host[method].apply(host, args);
  return undefined;
}

export function mount(rt, target) {
  const node = typeof target === "string" && typeof document !== "undefined"
    ? document.querySelector(target)
    : target;
  if (!node || typeof document === "undefined") {
    rt.mounted = !!node;
    return rt;
  }
  node.innerHTML = "";
  const root = document.createElement("div");
  root.className = "kryon-runtime";
  root.style.fontFamily = "system-ui, sans-serif";
  root.style.display = "grid";
  root.style.gap = "8px";
  for (const item of rt.frame) {
    const el = document.createElement(item.name === "Button" ? "button" : "div");
    el.className = "kryon-widget kryon-widget-" + item.name.toLowerCase();
    el.textContent = item.name + (item.args ? "(" + item.args + ")" : "");
    root.appendChild(el);
  }
  node.appendChild(root);
  rt.mounted = true;
  return rt;
}

export function Color(r = 0, g = 0, b = 0, a = 255) {
  return { r, g, b, a };
}

export function NewVector2(x = 0, y = 0) {
  return { x, y };
}

export function NewRectangle(x = 0, y = 0, width = 0, height = 0) {
  return { x, y, width, height };
}

export function Key(value) {
  return String(value);
}

export function ScaleUIPx(value) {
  return value | 0;
}

export function GetScreenWidth() {
  return 800;
}

export function GetScreenHeight() {
  return 600;
}

export function GetThemeBackground() { return Color(247, 244, 236, 255); }
export function GetThemeSurface() { return Color(255, 254, 249, 255); }
export function GetThemeText() { return Color(42, 59, 64, 255); }
export function GetThemeButton() { return Color(35, 101, 125, 255); }
export function GetThemeIcon() { return Color(31, 83, 102, 255); }
export function GetThemeLink() { return Color(13, 93, 120, 255); }

export function Fade(color, alpha) {
  return Object.assign({}, color, { a: Math.round((alpha || 0) * 255) });
}

const runtimeCallNames = [
  "Background", "Bevel", "BottomNav", "Button", "CanvasGrid", "Checkbox",
  "ClearBackground", "Collapsible", "Column", "Combobox", "Dropdown", "EndCanvas",
  "EndScroll", "Href", "IconButton", "IconTexture", "LabelFrame", "ListBox",
  "Modal", "Notebook", "Paragraph", "Picture", "Progress", "Radio", "Rect",
  "Row", "Screen", "Scroll", "SelectableText", "SetCurrentTheme",
  "SetThemeDarkMode", "ShowToast", "Slider", "Spinbox", "Stack", "TabBar",
  "Text", "TextArea", "TextField", "TextInRect", "TextLines", "TitleBar",
  "Toggle", "Toolbar", "TopNav"
];

for (const name of runtimeCallNames) {
  if (!Object.prototype.hasOwnProperty.call(globalThis, "__kryonRuntimeInit")) {
    // Marker only; named exports are declared below for ESM static analysis.
  }
}
globalThis.__kryonRuntimeInit = true;

export function Background(...args) { return struct("Background", args); }
export function Bevel(...args) { return struct("Bevel", args); }
export function BottomNav(...args) { return struct("BottomNav", args); }
export function Button(...args) { return struct("Button", args); }
export function CanvasGrid(...args) { return struct("CanvasGrid", args); }
export function Checkbox(...args) { return struct("Checkbox", args); }
export function ClearBackground(...args) { return struct("ClearBackground", args); }
export function Collapsible(...args) { return struct("Collapsible", args); }
export function Column(...args) { return struct("Column", args); }
export function Combobox(...args) { return struct("Combobox", args); }
export function Dropdown(...args) { return struct("Dropdown", args); }
export function EndCanvas(...args) { return struct("EndCanvas", args); }
export function EndScroll(...args) { return struct("EndScroll", args); }
export function Href(...args) { return struct("Href", args); }
export function IconButton(...args) { return struct("IconButton", args); }
export function IconTexture(...args) { return struct("IconTexture", args); }
export function LabelFrame(...args) { return struct("LabelFrame", args); }
export function ListBox(...args) { return struct("ListBox", args); }
export function Modal(...args) { return struct("Modal", args); }
export function Notebook(...args) { return struct("Notebook", args); }
export function Paragraph(...args) { return struct("Paragraph", args); }
export function Picture(...args) { return struct("Picture", args); }
export function Progress(...args) { return struct("Progress", args); }
export function Radio(...args) { return struct("Radio", args); }
export function Rect(...args) { return struct("Rect", args); }
export function Row(...args) { return struct("Row", args); }
export function Screen(...args) { return struct("Screen", args); }
export function Scroll(...args) { return struct("Scroll", args); }
export function SelectableText(...args) { return struct("SelectableText", args); }
export function SetCurrentTheme(...args) { return struct("SetCurrentTheme", args); }
export function SetThemeDarkMode(...args) { return struct("SetThemeDarkMode", args); }
export function ShowToast(...args) { return struct("ShowToast", args); }
export function Slider(...args) { return struct("Slider", args); }
export function Spinbox(...args) { return struct("Spinbox", args); }
export function Stack(...args) { return struct("Stack", args); }
export function TabBar(...args) { return struct("TabBar", args); }
export function Text(...args) { return struct("Text", args); }
export function TextArea(...args) { return struct("TextArea", args); }
export function TextField(...args) { return struct("TextField", args); }
export function TextInRect(...args) { return struct("TextInRect", args); }
export function TextLines(...args) { return struct("TextLines", args); }
export function TitleBar(...args) { return struct("TitleBar", args); }
export function Toggle(...args) { return struct("Toggle", args); }
export function Toolbar(...args) { return struct("Toolbar", args); }
export function TopNav(...args) { return struct("TopNav", args); }
