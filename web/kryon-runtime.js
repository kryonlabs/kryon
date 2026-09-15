import { editTextEvent } from "./text_edit.js";
// Kryon web runtime for k2js-generated ESM.
import { Instance_InstanceExpired } from "./instance.js";

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

export * from "./control_props.js";
export const KeyTab = 258;
export const KeyBackspace = 259;
export const KeyRight = 262;
export const KeyLeft = 263;
export const KeyC = 67;
export const MouseButtonLeft = 0;
export const KEY_SPACE = 32;
export const KEY_C = KeyC;
export const KEY_TAB = KeyTab;
export const KEY_BACKSPACE = KeyBackspace;
export const KEY_RIGHT = KeyRight;
export const KEY_LEFT = KeyLeft;
export const KEY_DOWN = 264;
export const KEY_UP = 265;
export const MOUSE_BUTTON_LEFT = MouseButtonLeft;

let activeTheme = null;
let activeThemeFamily = null;
let activeThemeMode = 1;
const pageMeta = {
  title: "",
  description: "",
  canonicalURL: "",
  themeColor: ""
};
const routeState = {
  path: "/",
  hash: "",
  version: 0,
  params: {}
};
let routeListenersInstalled = false;

function normalizeRoute(path) {
  let text = String(path || "/");
  const hashIndex = text.indexOf("#");
  const hash = hashIndex >= 0 ? text.slice(hashIndex) : "";
  text = hashIndex >= 0 ? text.slice(0, hashIndex) : text;
  if (!text)
    text = "/";
  return { path: text, hash };
}

function browserRoute() {
  const location = globalThis.location;
  if (!location)
    return null;
  return normalizeRoute(String(location.pathname || "/") + String(location.hash || ""));
}

function setRouteState(next) {
  if (routeState.path === next.path && routeState.hash === next.hash)
    return false;
  routeState.path = next.path;
  routeState.hash = next.hash;
  routeState.params = {};
  routeState.version++;
  return true;
}

function splitRouteSegments(path) {
  const text = normalizeRoute(path).path;
  if (text === "/")
    return [];
  return text.replace(/^\/+|\/+$/g, "").split("/").filter(Boolean)
    .map((part) => {
      try {
        return decodeURIComponent(part);
      } catch {
        return part;
      }
    });
}

export function MatchRoute(pattern, path = GetRoutePath()) {
  const patternParts = splitRouteSegments(pattern || "/");
  const pathParts = splitRouteSegments(path || "/");
  if (patternParts.length !== pathParts.length)
    return null;
  const params = {};
  for (let i = 0; i < patternParts.length; i++) {
    const expected = patternParts[i];
    const actual = pathParts[i];
    if (expected.startsWith(":")) {
      const name = expected.slice(1);
      if (!name)
        return null;
      params[name] = actual;
    } else if (expected !== actual) {
      return null;
    }
  }
  return params;
}

export function RouteMatches(pattern, path = GetRoutePath()) {
  return MatchRoute(pattern, path) !== null;
}

export function SetRouteParams(params = {}) {
  routeState.params = { ...(params || {}) };
  return routeState.params;
}

export function GetRouteParams() {
  ensureRouteListeners();
  syncRouteFromBrowser();
  return { ...routeState.params };
}

export function GetRouteParam(name) {
  return GetRouteParams()[String(name || "")] || "";
}

function syncRouteFromBrowser() {
  const next = browserRoute();
  if (next)
    setRouteState(next);
}

function ensureRouteListeners() {
  if (routeListenersInstalled || typeof globalThis.addEventListener !== "function")
    return;
  routeListenersInstalled = true;
  globalThis.addEventListener("popstate", syncRouteFromBrowser);
  globalThis.addEventListener("hashchange", syncRouteFromBrowser);
}

function updateBrowserRoute(path, replace) {
  const next = normalizeRoute(path);
  const url = next.path + next.hash;
  const history = globalThis.history;
  if (history && typeof history[replace ? "replaceState" : "pushState"] === "function") {
    try {
      history[replace ? "replaceState" : "pushState"](null, "", url);
    } catch {
      /* file:// and locked-down hosts can reject history URLs; Kry route state still updates below. */
    }
  }
  setRouteState(next);
  return url;
}

export function ThemeDefaultLight() {
  return {
    name: "Default light",
    mode: 1,
    colors: {
      background: Color(245, 248, 252, 255), surface: Color(255, 255, 255, 255),
      text: Color(16, 35, 58, 255), textDisabled: Color(140, 154, 169, 255),
      icon: Color(24, 60, 99, 255), border: Color(197, 212, 227, 255),
      focus: Color(6, 108, 255, 255), selection: Color(216, 233, 255, 255),
      accent: Color(23, 105, 232, 255), onAccent: WHITE,
      accentHover: Color(15, 94, 216, 255), accentPressed: Color(10, 77, 184, 255),
      success: Color(7, 128, 90, 255), warning: Color(181, 109, 0, 255),
      danger: Color(214, 36, 69, 255), link: Color(7, 95, 209, 255),
    },
    metrics: GetThemeMetrics(),
  };
}

export function ThemeDefaultDark() {
  return {
    name: "Default dark",
    mode: 2,
    colors: {
      background: Color(7, 20, 38, 255),
      surface: Color(13, 33, 56, 255),
      text: Color(245, 248, 255, 255),
      textDisabled: Color(114, 131, 154, 255),
      icon: Color(216, 229, 245, 255),
      border: Color(49, 80, 111, 255), focus: Color(77, 163, 255, 255),
      accent: Color(20, 120, 255, 255),
      onAccent: WHITE, accentHover: Color(45, 140, 255, 255),
      accentPressed: Color(8, 98, 217, 255),
      success: Color(7, 150, 105, 255), warning: Color(200, 135, 0, 255),
      danger: Color(220, 47, 79, 255),
      link: Color(89, 168, 255, 255),
    },
    metrics: GetThemeMetrics(),
  };
}

export function SetTheme(theme) {
  activeThemeFamily = null;
  activeTheme = theme;
  activeThemeMode = theme?.mode === 2 ? 2 : 1;
}
export function SetThemeFamily(family) {
  activeThemeFamily = family;
  activeTheme = effectiveThemeDark() ? family.dark : family.light;
}
export function GetThemeFamily() { return activeThemeFamily; }
function effectiveThemeDark() {
  return activeThemeMode === 2 || (activeThemeMode === 0 &&
    typeof globalThis.matchMedia === "function" &&
    globalThis.matchMedia("(prefers-color-scheme: dark)").matches);
}
export function SetThemeMode(mode) {
  activeThemeMode = Number.isInteger(mode) && mode >= 0 && mode <= 2 ? mode : 0;
  if (activeThemeFamily)
    activeTheme = effectiveThemeDark() ? activeThemeFamily.dark : activeThemeFamily.light;
}
export function GetThemeMode() { return activeThemeMode; }
export function GetTheme() {
  if (activeThemeFamily)
    return effectiveThemeDark() ? activeThemeFamily.dark : activeThemeFamily.light;
  return activeTheme || (effectiveThemeDark() ? ThemeDefaultDark() : ThemeDefaultLight());
}
export const THEME_SKY = 0;
export const THEME_COUNT = 6;
export const THEME_MODE_SYSTEM = 0;
export const THEME_MODE_LIGHT = 1;
export const THEME_MODE_DARK = 2;
export const THEME_SOURCE_SYSTEM = 0;
export const THEME_SOURCE_APP = 1;
export const SyntaxNone = 0;
export const SyntaxKry = 1;
export const SyntaxC = 2;
export const SyntaxMake = 3;
export const ImageFitStretch = 0;
export const ImageFitContain = 1;
export const ImageFitCover = 2;

export function createRuntime(options = {}) {
  ensureRouteListeners();
  syncRouteFromBrowser();
  const rt = {
    app: options.app || null,
    target: options.target || null,
    frame: [],
    statements: [],
    hostCalls: [],
    mounted: false,
    instanceFrame: 0,
    instances: new Map(),
    disabledStack: [],
    webCompositeStack: [],
    input: {
      events: [],
      focus: 0,
      clipboard: "",
      selections: new Map(),
      preedit: new Map(),
      textHandoff: 0,
      deferredTextEvents: 0,
      dropdownOpen: null,
      focusOrder: [],
      lastFocusOrder: []
    }
  };
  rt.QueueText = (text) => { rt.input.events.push({ type: "text", text: String(text) }); };
  rt.QueueKey = (key) => { rt.input.events.push({ type: "key", key }); };
  rt.QueueShiftKey = (key) => { rt.input.events.push({ type: "key", key, shift: true }); };
  rt.QueueShortcut = (key) => { rt.input.events.push({ type: "shortcut", key }); };
  rt.QueueTap = (x, y) => { rt.input.events.push({ type: "tap", x: Number(x), y: Number(y) }); };
  rt.SetClipboardText = (text) => { rt.input.clipboard = String(text); };
  rt.ClipboardText = () => rt.input.clipboard;
  rt.SetSelection = (focusID, anchor, cursor) => {
    rt.input.selections.set(Number(focusID), { anchor: Number(anchor), cursor: Number(cursor) });
  };
  rt.SetFocus = (id) => {
    if (rt.input.focus !== Number(id)) rt.input.preedit.clear();
    rt.input.focus = Number(id);
  };
  rt.SubmitTextComposition = (phase, text, cursor, selectionLength) => {
    rt.input.events.push({type: "composition", phase, text: String(text), cursor,
      selectionLength, owner: rt.input.focus});
  };
  rt.Focus = () => rt.input.focus;
  if (options.webStyleSheets !== undefined) {
    setWebStyleSheets(rt, options.webStyleSheets);
  } else {
    const embedded = (rt.app?.styles || [])
      .map((style) => style?.source)
      .filter((source) => typeof source === "string" && source.length > 0);
    if (embedded.length > 0)
      setWebStyleSheets(rt, embedded);
  }
  return rt;
}

export function instanceState(rt, type, key, create) {
  if (!rt)
    throw new Error("instance state requires a render host");
  let instances = rt.instances.get(type);
  if (!instances) {
    instances = new Map();
    rt.instances.set(type, instances);
  }
  key = BigInt.asUintN(64, BigInt(key));
  let entry = instances.get(key);
  if (!entry) {
    entry = { value: create(), frameSeen: rt.instanceFrame };
    instances.set(key, entry);
  }
  entry.frameSeen = rt.instanceFrame;
  return entry;
}

export function beginFrame(rt) {
  rt.frame = [];
  rt.statements = [];
  rt.hostCalls = [];
  rt.disabledStack = [];
  rt.webCompositeStack = [];
  for (const [type, instances] of rt.instances) {
    for (const [key, entry] of instances) {
      if (Instance_InstanceExpired(null, null, null, BigInt(rt.instanceFrame - entry.frameSeen)))
        instances.delete(key);
    }
    if (instances.size === 0)
      rt.instances.delete(type);
  }
  if (rt.input) {
    if (rt.input.textHandoff && rt.input.textHandoff !== rt.input.focus)
      rt.input.events.splice(0, rt.input.deferredTextEvents);
    rt.input.textHandoff = 0;
    rt.input.deferredTextEvents = 0;
    rt.input.focusOrder = [];
  }
  return rt;
}

export function endFrame(rt) {
  if (rt.input) {
    rt.input.lastFocusOrder = rt.input.focusOrder.slice();
    if (!rt.input.textHandoff || rt.input.textHandoff !== rt.input.focus)
      rt.input.events = [];
    else
      rt.input.deferredTextEvents = rt.input.events.length;
    for (const id of rt.input.preedit.keys()) {
      if (id !== rt.input.focus || !rt.input.focusOrder.includes(id))
        rt.input.preedit.delete(id);
    }
  }
  rt.instanceFrame++;
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

export function viewport(rt, app = null) {
  const target = typeof rt.target === "string" && typeof document !== "undefined"
    ? document.querySelector(rt.target) : rt.target;
  const metadata = rt.app || app || {};
  const width = Number(target?.clientWidth);
  const height = Number(target?.clientHeight);
  return {
    x: 0,
    y: 0,
    width: Number.isFinite(width) && width > 0 ? width : numberValue(metadata.width, 800),
    height: Number.isFinite(height) && height > 0 ? height : numberValue(metadata.height, 600)
  };
}

export function widget(rt, name, args, state = null, meta = null) {
  const item = { kind: "widget", name, args, meta: remapWebCompositeMeta(rt, meta) };
  if (name === "Disabled" && String(args || "").trim() === "end")
    return handleWidget(rt, name, args, state);
  rt.frame.push(item);
  return handleWidget(rt, name, args, state);
}

function remapCompositePath(value, sourceRoot, targetRoot) {
  const text = String(value || "");
  if (!text || !sourceRoot || !targetRoot)
    return text;
  if (text === sourceRoot)
    return targetRoot;
  if (text.startsWith(sourceRoot + "/"))
    return targetRoot + text.slice(sourceRoot.length);
  return text;
}

function remapWebCompositeMeta(rt, meta) {
  const context = rt?.webCompositeStack?.[rt.webCompositeStack.length - 1];
  if (!context || !meta || typeof meta !== "object")
    return meta;
  const sourceRoot = context.name || "";
  const targetRoot = context.path || "";
  if (!sourceRoot || !targetRoot)
    return meta;
  const next = { ...meta };
  const oldPath = next.path === undefined || next.path === null ? "" : String(next.path);
  const oldParentPath = next.parentPath === undefined || next.parentPath === null ? "" : String(next.parentPath);
  const oldKey = next.key === undefined || next.key === null ? "" : String(next.key);
  next.path = remapCompositePath(oldPath, sourceRoot, targetRoot);
  if (oldParentPath) {
    next.parentPath = remapCompositePath(oldParentPath, sourceRoot, targetRoot);
  } else if (next.path && next.path !== oldPath) {
    next.parentPath = targetRoot;
  }
  if (oldKey === oldPath || oldKey.startsWith(sourceRoot + "/"))
    next.key = remapCompositePath(oldKey, sourceRoot, targetRoot);
  return next;
}

export function beginWebComposite(rt, name, meta = null) {
  if (!rt)
    return null;
  const context = {
    name: String(name || ""),
    path: meta?.path === undefined || meta?.path === null ? "" : String(meta.path)
  };
  if (!rt.webCompositeStack)
    rt.webCompositeStack = [];
  rt.webCompositeStack.push(context);
  return context;
}

export function endWebComposite(rt, context = null) {
  if (!rt?.webCompositeStack?.length)
    return null;
  if (context == null)
    return rt.webCompositeStack.pop();
  const index = rt.webCompositeStack.lastIndexOf(context);
  if (index < 0)
    return null;
  return rt.webCompositeStack.splice(index, 1)[0] || null;
}

export function statement(rt, text) {
  const item = { kind: "statement", text };
  rt.statements.push(item);
  return item;
}

export function expr(text) {
  return { kind: "expr", text };
}

/* Byte-aware value index shared with generated KSS/strict code: string bases
 * yield byte numbers, array bases index normally. */
export function index(base, index) {
  return typeof base === "string" ? base.charCodeAt(index) : base[index];
}

export function struct(type, value) {
  return { type, value };
}

const references = new WeakSet();
const valueRecords = new WeakMap();
const recordFields = {
  Vector2: ["x", "y"],
  Vector3: ["x", "y", "z"],
  Vector4: ["x", "y", "z", "w"],
  Rectangle: ["x", "y", "width", "height"],
  Color: ["r", "g", "b", "a"]
};

// Retain the compact positional representation used by widget adapters while
// giving generated source its declared record-field semantics.
export function recordValue(type, value) {
  const fields = recordFields[type];
  if (!fields) {
    throw new TypeError(`Unknown positional record: ${type}`);
  }
  if (!Array.isArray(value)) {
    for (const field of fields) {
      if (!Object.prototype.hasOwnProperty.call(value, field)) {
        value[field] = 0;
      }
    }
    return value;
  }
  fields.forEach((field, index) => {
    if (index >= value.length) {
      value[index] = 0;
    }
    Object.defineProperty(value, field, {
      get() { return this[index]; },
      set(next) { this[index] = next; }
    });
  });
  valueRecords.set(value, type);
  return value;
}

// Generated value declarations and assignments own their records and arrays. Explicit
// references remain shared, just as pointer fields do in C and Go.
export function copyValue(value) {
  if (value === null || typeof value !== "object" || references.has(value)) {
    return value;
  }
  if (Array.isArray(value)) {
    const copied = value.map(copyValue);
    const type = valueRecords.get(value);
    return type ? recordValue(type, copied) : copied;
  }
  const prototype = Object.getPrototypeOf(value);
  if (prototype !== Object.prototype && prototype !== null) {
    return value;
  }
  return Object.fromEntries(Object.entries(value).map(([key, field]) => [key, copyValue(field)]));
}

export function ResolveFont(requested, inherited, fallback) {
  requested = numberValue(requested, 0);
  inherited = numberValue(inherited, 0);
  fallback = numberValue(fallback, 0);
  if (requested > 0)
    return Math.trunc(requested);
  if (inherited > 0)
    return Math.trunc(inherited);
  if (fallback > 0)
    return Math.trunc(fallback);
  return 16;
}

function unitValue(value) {
  value = numberValue(value, 0);
  if (value < 0)
    return 0;
  if (value > 1)
    return 1;
  return value;
}

function opacityColor(color, opacity) {
  opacity = unitValue(opacity);
  const alpha = Math.trunc((Number(color) & 255) * opacity);
  return (((Number(color) >>> 8) << 8) | alpha) >>> 0;
}

export function Opacity(color, opacity) {
  return opacityColor(color, opacity);
}

export function GradientColor(top, bottom, position) {
  const t = unitValue(position);
  top = Number(top) >>> 0;
  bottom = Number(bottom) >>> 0;
  let result = 0;
  for (let shift = 0; shift <= 24; shift += 8) {
    const a = (top >>> shift) & 255;
    const b = (bottom >>> shift) & 255;
    result = (result | (Math.trunc(a + (b - a) * t) << shift)) >>> 0;
  }
  return result >>> 0;
}

function mixValue(from, to, amount) {
  amount = numberValue(amount, 0);
  if (amount <= 0)
    return from;
  if (amount >= 1)
    return to;
  return from + (to - from) * amount;
}

export function InteractionValue(normal, hover, press, focus, hoverAmount,
  pressAmount, focusAmount) {
  const resting = mixValue(numberValue(normal, 0), numberValue(focus, 0),
    numberValue(focusAmount, 0));
  return mixValue(mixValue(resting, numberValue(hover, 0),
    numberValue(hoverAmount, 0)), numberValue(press, 0), numberValue(pressAmount, 0));
}

export function InteractionColor(normal, hover, press, focus, hoverAmount,
  pressAmount, focusAmount) {
  const resting = GradientColor(normal, focus, focusAmount);
  return GradientColor(GradientColor(resting, hover, hoverAmount), press, pressAmount);
}

export function FillState(fields, start, end) {
  return {
    normal: (Number(fields) & 8192) !== 0,
    hover: false,
    press: false,
    focus: false,
    normal_start: Number(start) >>> 0,
    normal_end: Number(end) >>> 0,
    hover_start: 0,
    hover_end: 0,
    press_start: 0,
    press_end: 0,
    focus_start: 0,
    focus_end: 0,
    hover_amount: 0,
    press_amount: 0,
    focus_amount: 0
  };
}

export function FillTransition(normal, hover, press, focus, h, p, f) {
  normal = { ...normal };
  normal.hover = !!hover?.normal;
  normal.press = !!press?.normal;
  normal.focus = !!focus?.normal;
  normal.hover_start = Number(hover?.normal_start || 0) >>> 0;
  normal.hover_end = Number(hover?.normal_end || 0) >>> 0;
  normal.press_start = Number(press?.normal_start || 0) >>> 0;
  normal.press_end = Number(press?.normal_end || 0) >>> 0;
  normal.focus_start = Number(focus?.normal_start || 0) >>> 0;
  normal.focus_end = Number(focus?.normal_end || 0) >>> 0;
  normal.hover_amount = numberValue(h, 0);
  normal.press_amount = numberValue(p, 0);
  normal.focus_amount = numberValue(f, 0);
  return normal;
}

function colorChroma(color) {
  color = Number(color) >>> 0;
  let lowest = 255;
  let highest = 0;
  for (let shift = 8; shift <= 24; shift += 8) {
    const channel = (color >>> shift) & 255;
    if (channel < lowest)
      lowest = channel;
    if (channel > highest)
      highest = channel;
  }
  return highest - lowest;
}

export function LoadingRing(width, height, iconSize, elapsedMs, color, ambient) {
  const ring = {
    x: numberValue(width, 0) * 0.5,
    y: numberValue(height, 0) * 0.5,
    inner_radius: 0,
    outer_radius: 0,
    start_angle: 0,
    end_angle: 0,
    color: 0,
    track_color: 0,
    tip_color: 0,
    trail_opacity: 0,
    glow_blur: 0
  };
  iconSize = numberValue(iconSize, 0);
  if (iconSize <= 0)
    return ring;
  ambient = Number(ambient) >>> 0;
  color = Number(color) >>> 0;
  const brightness = ((ambient >>> 24) & 255) + ((ambient >>> 16) & 255) + ((ambient >>> 8) & 255);
  const lightSurroundings = brightness > 450;
  ring.outer_radius = iconSize * 0.5;
  if (!lightSurroundings)
    ring.outer_radius += 1;
  if (ring.outer_radius < 3)
    ring.outer_radius = 3;
  ring.inner_radius = ring.outer_radius - (lightSurroundings ? 2 : 2.5);
  const phaseMs = numberValue(elapsedMs, 0) - Math.trunc(numberValue(elapsedMs, 0) / 1500) * 1500;
  let angle = phaseMs * 0.24;
  if (!lightSurroundings)
    angle += 110;
  angle -= Math.trunc(angle / 360) * 360;
  ring.start_angle = angle;
  ring.end_angle = angle + (lightSurroundings ? 270 : 315);
  ring.color = color;
  ring.track_color = opacityColor(color, 0.22);
  const white = ((16777215 << 8) | (color & 255)) >>> 0;
  ring.tip_color = lightSurroundings ? GradientColor(color, white, 0.88) : white;
  ring.trail_opacity = 0.55;
  if (!lightSurroundings) {
    const chroma = unitValue((colorChroma(color) - 64) / 64);
    ring.trail_opacity += 0.25 * chroma;
    ring.glow_blur = 6 * chroma;
  }
  return ring;
}

export function ResolveFlags(state, disabled, loading, selected) {
  state = Number(state) || 0;
  return {
    disabled: !!disabled || state === 5,
    loading: !!loading || state === 6,
    selected: !!selected || state === 7
  };
}

export function ResolveState(explicitState, disabled, loading, pressed, hovered,
  focused, selected) {
  explicitState = Number(explicitState) || 0;
  if (disabled)
    return 5;
  if (loading)
    return 6;
  if (explicitState !== 0)
    return explicitState;
  if (pressed)
    return 3;
  if (hovered)
    return 2;
  if (focused)
    return 4;
  if (selected)
    return 7;
  return 1;
}

export function ResolveInteraction(explicitState, disabled, loading, pressed,
  hovered, focused, selected) {
  const state = ResolveState(explicitState, disabled, loading, pressed, hovered,
    focused, selected);
  const interaction = { state, hovered: !!hovered, pressed: !!pressed, focused: !!focused };
  if (Number(explicitState) !== 0) {
    interaction.hovered = state === 2;
    interaction.pressed = state === 3;
    interaction.focused = state === 4;
  }
  if (disabled || loading) {
    interaction.hovered = false;
    interaction.pressed = false;
    interaction.focused = false;
  }
  return interaction;
}

export function CanActivate(disabled, loading) {
  return !disabled && !loading;
}

export function ContentBounds(width, height, paddingX, paddingY) {
  paddingX = Math.max(0, numberValue(paddingX, 0));
  paddingY = Math.max(0, numberValue(paddingY, 0));
  const box = {
    x: paddingX,
    y: paddingY,
    width: numberValue(width, 0) - 2 * paddingX,
    height: numberValue(height, 0) - 2 * paddingY
  };
  if (box.width < 0)
    box.width = 0;
  if (box.height < 0)
    box.height = 0;
  return box;
}

export function InsetBounds(bounds, paddingX, paddingY, scale) {
  scale = numberValue(scale, 1);
  if (scale <= 0)
    scale = 1;
  const content = ContentBounds(numberValue(bounds?.width, 0) / scale,
    numberValue(bounds?.height, 0) / scale, paddingX, paddingY);
  return {
    x: numberValue(bounds?.x, 0) + content.x * scale,
    y: numberValue(bounds?.y, 0) + content.y * scale,
    width: content.width * scale,
    height: content.height * scale
  };
}

export function CenterChild(declared, measured, content) {
  measured = { ...(measured || {}) };
  if (numberValue(declared?.x, 0) !== 0 || numberValue(declared?.y, 0) !== 0)
    return measured;
  if (numberValue(measured.width, 0) <= 0)
    measured.width = numberValue(content?.width, 0);
  if (numberValue(measured.height, 0) <= 0)
    measured.height = numberValue(content?.height, 0);
  measured.x = numberValue(content?.x, 0) +
    (numberValue(content?.width, 0) - numberValue(measured.width, 0)) * 0.5;
  measured.y = numberValue(content?.y, 0) +
    (numberValue(content?.height, 0) - numberValue(measured.height, 0)) * 0.5;
  return measured;
}

export function FitHeight(requested, minimum, content, padding) {
  requested = numberValue(requested, 0);
  if (requested > 0)
    return requested;
  padding = Math.max(0, numberValue(padding, 0));
  content = Math.max(0, numberValue(content, 0));
  const height = content + 2 * padding;
  return height < numberValue(minimum, 0) ? numberValue(minimum, 0) : height;
}

export function MaterialLayer(material, index, width, height, radius, borderWidth,
  background, border, light, focusColor, hover, press, focused, disabled, opacity,
  ambient) {
  const layer = {
    x: 0, y: 0, width: numberValue(width, 0), height: numberValue(height, 0),
    radius: numberValue(radius, 0), stroke: 0, blur: 0, inner_blur: 0,
    outside_only: false, is_face: false, color: 0, end_color: 0,
    gradient: false, gradient_bias: 0
  };
  if (Number(material) !== 1)
    return layer;
  if (Number(index) === 0) {
    layer.color = opacityColor(background, opacity);
    layer.is_face = true;
  } else if (Number(index) === 1 && numberValue(borderWidth, 0) > 0) {
    layer.color = opacityColor(border, opacity);
    layer.stroke = numberValue(borderWidth, 0);
  } else if (Number(index) === 2 && !disabled) {
    layer.color = opacityColor(focusColor, numberValue(opacity, 0) * unitValue(focused));
    layer.stroke = 1;
  }
  return layer;
}

export function ApplyFillStates(layer, states, opacity) {
  if (!states?.normal && !states?.hover && !states?.press && !states?.focus)
    return layer;
  const end = layer.gradient ? layer.end_color : layer.color;
  const fillEndpoint = (material, enabled, custom) => enabled ? opacityColor(custom, opacity) : material;
  layer.end_color = InteractionColor(
    fillEndpoint(end, states.normal, states.normal_end),
    fillEndpoint(end, states.hover, states.hover_end),
    fillEndpoint(end, states.press, states.press_end),
    fillEndpoint(end, states.focus, states.focus_end),
    states.hover_amount, states.press_amount, states.focus_amount);
  layer.color = InteractionColor(
    fillEndpoint(layer.color, states.normal, states.normal_start),
    fillEndpoint(layer.color, states.hover, states.hover_start),
    fillEndpoint(layer.color, states.press, states.press_start),
    fillEndpoint(layer.color, states.focus, states.focus_start),
    states.hover_amount, states.press_amount, states.focus_amount);
  layer.gradient = true;
  layer.gradient_bias = InteractionValue(
    states.normal ? 0 : numberValue(layer.gradient_bias, 0),
    states.hover ? 0 : numberValue(layer.gradient_bias, 0),
    states.press ? 0 : numberValue(layer.gradient_bias, 0),
    states.focus ? 0 : numberValue(layer.gradient_bias, 0),
    states.hover_amount, states.press_amount, states.focus_amount);
  return layer;
}

export function FillGradient(layer, enabled, start, end, opacity) {
  if (enabled) {
    layer.color = opacityColor(start, opacity);
    layer.end_color = opacityColor(end, opacity);
    layer.gradient = true;
    layer.gradient_bias = 0;
  }
  return layer;
}

export function ref(object, key) {
  const reference = {
    get value() { return object ? object[key] : undefined; },
    set value(next) {
      if (object) object[key] = next;
    },
    object,
    key
  };
  references.add(reference);
  return reference;
}

export function stateForModule() {
  return {};
}

export function hostCall(host, method, args = []) {
  if (host && typeof host[method] === "function")
    return host[method].apply(host, args);
  return undefined;
}

function splitTopLevel(text) {
  const out = [];
  let depth = 0;
  let start = 0;
  let quote = "";
  for (let i = 0; i <= text.length; i++) {
    const ch = text[i] || ",";
    if (quote) {
      if (ch === "\\" && i + 1 < text.length) i++;
      else if (ch === quote) quote = "";
      continue;
    }
    if (ch === '"' || ch === "'") {
      quote = ch;
      continue;
    }
    if (ch === "(" || ch === "[" || ch === "{") depth++;
    else if (ch === ")" || ch === "]" || ch === "}") depth = Math.max(0, depth - 1);
    else if (ch === "," && depth === 0) {
      out.push(text.slice(start, i).trim());
      start = i + 1;
    }
  }
  return out.filter((part) => part.length > 0);
}

function numberValue(text, fallback = 0) {
  if (typeof text === "number") return text;
  let s = String(text || "").trim();
  while (/^Scale\s*\(/.test(s) && s.endsWith(")"))
    s = s.slice(s.indexOf("(") + 1, -1).trim();
  s = s.replace(/[fF]\b/g, "");
  if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16);
  const n = Number(s);
  return Number.isFinite(n) ? n : fallback;
}

function stringValue(text, fallback = "") {
  const s = String(text || "").trim();
  const m = s.match(/^"((?:[^"\\]|\\.)*)"$/);
  if (!m) return fallback;
  return m[1].replace(/\\"/g, '"').replace(/\\n/g, "\n").replace(/\\t/g, "\t");
}

function propString(args, prop, fallback = "") {
  if (args && typeof args === "object" && !Array.isArray(args)) {
    const value = args[prop];
    return value === undefined || value === null ? fallback : String(value);
  }
  const pattern = new RegExp("\\." + prop + "\\s*=\\s*(\"(?:[^\"\\\\]|\\\\.)*\"|[^,}]+)");
  const match = String(args || "").match(pattern);
  if (!match)
    return fallback;
  return stringValue(match[1], String(match[1] || "").trim());
}

function propStringAny(args, props, fallback = "") {
  for (const prop of props) {
    const value = propString(args, prop, undefined);
    if (value !== undefined && value !== null && value !== "")
      return String(value);
  }
  return fallback;
}

function propClassList(args) {
  const classes = [];
  const add = (value) => {
    if (value === undefined || value === null || value === false || value === 0)
      return;
    String(value).split(/\s+/).filter(Boolean).forEach((name) => classes.push(name));
  };
  if (args && typeof args === "object" && !Array.isArray(args)) {
    add(args.class);
    add(args.className);
    add(args.class_name);
    if (Array.isArray(args.classes))
      args.classes.forEach(add);
  } else {
    add(propString(args, "class", ""));
    add(propString(args, "class_name", ""));
  }
  return [...new Set(classes)];
}

function parseBounds(args) {
  if (args && typeof args === "object") {
    const bounds = args.bounds || {};
    return {
      x: numberValue(bounds.x ?? bounds[0]),
      y: numberValue(bounds.y ?? bounds[1]),
      width: numberValue(bounds.width ?? bounds[2]),
      height: numberValue(bounds.height ?? bounds[3])
    };
  }
  const prop = String(args || "").match(/\.bounds\s*=\s*(?:\([^)]+\))?\{([^{}]+)\}/);
  if (prop) {
    const p = splitTopLevel(prop[1]);
    return {
      x: numberValue(p[0]),
      y: numberValue(p[1]),
      width: numberValue(p[2]),
      height: numberValue(p[3])
    };
  }
  const rect = String(args || "").match(/\(Rectangle\)\s*\{([^{}]+)\}/);
  if (rect) {
    const p = splitTopLevel(rect[1]);
    return {
      x: numberValue(p[0]),
      y: numberValue(p[1]),
      width: numberValue(p[2]),
      height: numberValue(p[3])
    };
  }
  const p = splitTopLevel(String(args || ""));
  if (p.length >= 5) {
    return {
      x: numberValue(p[1]),
      y: numberValue(p[2]),
      width: numberValue(p[3]),
      height: numberValue(p[4], 24)
    };
  }
  return { x: 0, y: 0, width: 0, height: 0 };
}

function propNumber(args, prop, fallback = 0) {
  if (args && typeof args === "object")
    return numberValue(args[prop], fallback);
  const m = String(args || "").match(new RegExp("\\." + prop + "\\s*=\\s*([^,}]+)"));
  return m ? numberValue(m[1], fallback) : fallback;
}

function propRef(args, prop) {
  const m = String(args || "").match(new RegExp("\\." + prop + "\\s*=\\s*&([A-Za-z_]\\w*)"));
  return m ? m[1] : null;
}

function firstRef(args) {
  const m = String(args || "").match(/&([A-Za-z_]\w*)/);
  return m ? m[1] : null;
}

function propIdent(args, prop) {
  const m = String(args || "").match(new RegExp("\\." + prop + "\\s*=\\s*([A-Za-z_]\\w*)"));
  return m ? m[1] : null;
}

function propStateArray(args, prop) {
  const name = propIdent(args, prop);
  return name && name !== "NULL" && name !== "null" ? name : null;
}

function writeStateValue(state, name, index, value) {
  if (!state || !name)
    return false;
  if (Array.isArray(state[name])) {
    state[name][index || 0] = value;
    return true;
  }
  state[name] = value;
  return true;
}

function hit(bounds, x, y) {
  return x >= bounds.x && y >= bounds.y &&
    x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

function consumeFirstEvent(rt, predicate) {
  const events = rt.input?.events || [];
  for (let i = 0; i < events.length; i++) {
    if (predicate(events[i])) {
      const [event] = events.splice(i, 1);
      return event;
    }
  }
  return null;
}

function moveFocus(rt, id, shift) {
  const order = rt.input.lastFocusOrder.length ? rt.input.lastFocusOrder : rt.input.focusOrder;
  const at = order.indexOf(id);
  if (at < 0 || order.length === 0)
    return;
  const next = shift
    ? (at + order.length - 1) % order.length
    : (at + 1) % order.length;
  rt.input.focus = order[next];
}

function applyTextInput(rt, state, props) {
  if (!state || !props.textKey || !props.cursorKey)
    return false;
  let changed = false;
  while (rt.input.focus === props.focusID) {
    const event = consumeFirstEvent(rt, ev =>
      ["text", "key", "shortcut", "composition"].includes(ev.type));
    if (!event)
      break;
    if (event.type === "key" && Number(event.key) === KeyTab) {
      moveFocus(rt, props.focusID, !!event.shift);
      if (rt.input.focus !== props.focusID) {
        rt.input.textHandoff = rt.input.focus;
        rt.input.preedit.delete(props.focusID);
      }
      rt.input.selections.delete(props.focusID);
    } else {
      changed = editTextEvent(rt.input, state, props, event) || changed;
    }
  }
  return changed;
}

function parseTextInputProps(args, state, multiline) {
  const bounds = parseBounds(args);
  const textKey = propIdent(args, "text");
  return {
    bounds,
    textKey,
    cursorKey: propRef(args, "cursor_position"),
    focusID: propNumber(args, "focus_id", 0),
    maxCodepoints: propNumber(args, "max_codepoints", 4095),
    secure: isTruthyProp(args, "secure"),
    readOnly: isTruthyProp(args, "read_only") || !!state?.[propIdent(args, "read_only")],
    multiline,
    textSize: propNumber(args, "text_size", 2147483647) || 2147483647,
    pageRows: Math.max(1, Math.floor((bounds.height - 8) / 20)),
    commitKey: propRef(args, "commit_pressed")
  };
}

function handleTextInput(rt, state, args, multiline) {
  const props = parseTextInputProps(args, state, multiline);
  if (state && props.commitKey) state[props.commitKey] = false;
  if (props.readOnly || rt.input.focus !== props.focusID)
    rt.input.preedit.delete(props.focusID);
  if (props.focusID) {
    if (!rt.input.focusOrder.includes(props.focusID))
      rt.input.focusOrder.push(props.focusID);
    const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(props.bounds, ev.x, ev.y));
    if (tap) {
      rt.input.focus = props.focusID;
      if (state && props.cursorKey)
        state[props.cursorKey] = 0;
    }
    return applyTextInput(rt, state, props);
  }
  return false;
}

function handleButton(rt, args) {
  const bounds = parseBounds(args);
  return !!consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
}

function isTruthyProp(args, name) {
  if (args && typeof args === "object" && !Array.isArray(args))
    return !!args[name];
  const text = String(args || "");
  return new RegExp(`\\b\\.?${name}\\s*(?:=|:)\\s*(?:true|1)\\b`, "i").test(text);
}

function isTruthyPropAny(args, names) {
  return names.some((name) => isTruthyProp(args, name));
}

function handleCard(rt, args) {
  if (!isTruthyProp(args, "clickable") && !isTruthyProp(args, "Clickable"))
    return false;
  return handleButton(rt, args);
}

function handlePopup(args) {
  if (isTruthyProp(args, "disabled"))
    return false;
  const open = args && typeof args === "object" ? args.open : null;
  if (open && typeof open === "object" && "value" in open)
    return !!open.value;
  const flags = propNumber(args, "flags", 0);
  return (flags & 1) !== 0;
}

function handleSlider(rt, state, args) {
  if (String(args || "").includes("SliderProps")) {
    const bounds = parseBounds(args);
    const min = propNumber(args, "min", 0);
    const max = propNumber(args, "max", 100);
    const vertical = propNumber(args, "vertical", 0);
    const intValues = propStateArray(args, "int_values");
    const floatValues = propStateArray(args, "float_values");
    const floatValue = propRef(args, "float_value");
    const target = intValues || floatValues || floatValue;
    const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
    if (!tap || !state || !target)
      return false;
    const raw = vertical
      ? (bounds.y + bounds.height - tap.y) / Math.max(1, bounds.height)
      : (tap.x - bounds.x) / Math.max(1, bounds.width);
    const t = Math.max(0, Math.min(1, raw));
    const value = min + t * (max - min);
    return writeStateValue(state, target, 0, intValues ? Math.round(value) : value);
  }
  const p = splitTopLevel(String(args || ""));
  const ref = firstRef(args);
  const bounds = { x: numberValue(p[1]), y: numberValue(p[2]), width: numberValue(p[3]), height: 40 };
  const min = numberValue(p[5]);
  const max = numberValue(p[6], 100);
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state || !ref)
    return false;
  const t = bounds.width > 0 ? Math.max(0, Math.min(1, (tap.x - bounds.x) / bounds.width)) : 0;
  state[ref] = Math.round(min + t * (max - min));
  return true;
}

function handleToggle(rt, state, args) {
  if (String(args || "").includes("ToggleProps")) {
    const ref = propRef(args, "value");
    const bounds = parseBounds(args);
    const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
    if (!tap || !state || !ref)
      return false;
    state[ref] = state[ref] ? 0 : 1;
    return true;
  }
  const p = splitTopLevel(String(args || ""));
  const ref = firstRef(args);
  const bounds = { x: numberValue(p[1]), y: numberValue(p[2]), width: numberValue(p[3]), height: numberValue(p[4], 24) };
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state || !ref)
    return false;
  state[ref] = state[ref] ? 0 : 1;
  return true;
}

function handleCheckbox(rt, state, args) {
  if (String(args || "").includes("CheckboxProps")) {
    const valueRef = propRef(args, "value");
    const flagsRef = propRef(args, "flags");
    const flagsValue = propNumber(args, "flags_value", 0);
    const bounds = parseBounds(args);
    const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
    if (!tap || !state || (!valueRef && !flagsRef))
      return false;
    if (flagsRef) {
      state[flagsRef] = (state[flagsRef] & flagsValue) ? (state[flagsRef] & ~flagsValue) : (state[flagsRef] | flagsValue);
      return true;
    }
    state[valueRef] = state[valueRef] ? 0 : 1;
    return true;
  }
  const p = splitTopLevel(String(args || ""));
  const ref = firstRef(args);
  const bounds = { x: numberValue(p[1]), y: numberValue(p[2]), width: 120, height: 24 };
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state || !ref)
    return false;
  state[ref] = state[ref] ? 0 : 1;
  return true;
}

function handleDropdown(rt, state, args) {
  if (String(args || "").includes("DropdownProps")) {
    const id = propNumber(args, "id", 0);
    const ref = propRef(args, "selected_index");
    const bounds = parseBounds(args);
    const count = propNumber(args, "option_count", 0);
    const tap = consumeFirstEvent(rt, (ev) =>
      ev.type === "tap" &&
      (hit(bounds, ev.x, ev.y) ||
       (rt.input.dropdownOpen === id && hit({ x: bounds.x, y: bounds.y + bounds.height, width: bounds.width, height: bounds.height * count }, ev.x, ev.y))));
    if (!tap || !state || !ref)
      return false;
    if (hit(bounds, tap.x, tap.y)) {
      rt.input.dropdownOpen = rt.input.dropdownOpen === id ? null : id;
      return true;
    }
    if (rt.input.dropdownOpen === id) {
      const index = Math.max(0, Math.floor((tap.y - (bounds.y + bounds.height)) / Math.max(1, bounds.height)));
      state[ref] = Math.min(index, Math.max(0, count - 1));
      rt.input.dropdownOpen = null;
      return true;
    }
    return false;
  }
  const p = splitTopLevel(String(args || ""));
  const id = numberValue(p[0]);
  const ref = firstRef(args);
  const bounds = { x: numberValue(p[1]), y: numberValue(p[2]), width: numberValue(p[3]), height: numberValue(p[4], 24) };
  const tap = consumeFirstEvent(rt, (ev) =>
    ev.type === "tap" &&
    (hit(bounds, ev.x, ev.y) ||
     (rt.input.dropdownOpen === id && hit({ x: bounds.x, y: bounds.y + bounds.height, width: bounds.width, height: bounds.height * numberValue(p[6], 0) }, ev.x, ev.y))));
  if (!tap || !state || !ref)
    return false;
  if (hit(bounds, tap.x, tap.y)) {
    rt.input.dropdownOpen = rt.input.dropdownOpen === id ? null : id;
    return true;
  }
  if (rt.input.dropdownOpen === id) {
    const index = Math.max(0, Math.floor((tap.y - (bounds.y + bounds.height)) / Math.max(1, bounds.height)));
    state[ref] = Math.min(index, Math.max(0, numberValue(p[6], index + 1) - 1));
    rt.input.dropdownOpen = null;
    return true;
  }
  return false;
}

function handleListBox(rt, state, args) {
  const bounds = parseBounds(args);
  const ref = propRef(args, "selected_index");
  const rowH = propNumber(args, "row_height", 24);
  const count = propNumber(args, "item_count", 0);
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state || !ref)
    return false;
  state[ref] = Math.min(Math.max(0, Math.floor((tap.y - bounds.y) / rowH)), Math.max(0, count - 1));
  return true;
}

function handleTreeView(rt, state, args) {
  const bounds = parseBounds(args);
  const ref = propRef(args, "selected_id");
  const rowH = propNumber(args, "row_height", 28);
  const count = propNumber(args, "item_count", 0);
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state || !ref)
    return false;
  state[ref] = Math.min(Math.max(0, Math.floor((tap.y - bounds.y) / rowH)), Math.max(0, count - 1));
  return true;
}

function handleTableView(rt, state, args) {
  const bounds = parseBounds(args);
  const rowH = propNumber(args, "row_height", 24);
  const selectedRow = propRef(args, "selected_row");
  const selectedColumn = propRef(args, "selected_column");
  const activatedRow = propRef(args, "activated_row");
  const activatedColumn = propRef(args, "activated_column");
  const sortColumn = propRef(args, "sort_column");
  const widths = [90, 140, 70];
  const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(bounds, ev.x, ev.y));
  if (!tap || !state)
    return false;
  let x = bounds.x;
  let col = widths.length - 1;
  for (let i = 0; i < widths.length; i++) {
    if (tap.x >= x && tap.x < x + widths[i]) {
      col = i;
      break;
    }
    x += widths[i];
  }
  if (tap.y < bounds.y + rowH) {
    if (selectedRow) state[selectedRow] = -1;
    if (selectedColumn) state[selectedColumn] = col;
    if (sortColumn) state[sortColumn] = col;
    return true;
  }
  const row = Math.max(0, Math.floor((tap.y - (bounds.y + rowH)) / rowH));
  if (state[selectedRow] === row && state[selectedColumn] === col) {
    if (activatedRow) state[activatedRow] = row;
    if (activatedColumn) state[activatedColumn] = col;
  }
  if (selectedRow) state[selectedRow] = row;
  if (selectedColumn) state[selectedColumn] = col;
  return true;
}

function handleTabBar(rt, state, args, interactive = true) {
  const bounds = parseBounds(args);
  const count = propNumber(args, "count", 0);
  const id = propNumber(args, "id", 0);
  const selectedRef = firstRef(args);
  const selectedName = propIdent(args, "selected_index");
  let selected = state && selectedRef ? Number(state[selectedRef]) :
    state && selectedName ? Number(state[selectedName]) : 0;
  if (count <= 0 || bounds.width <= 0 || bounds.height <= 0)
    return { open: false, count: 0, selected: 0 };
  if (selected < 0 || selected >= count)
    selected = 0;
  if (interactive && id && !rt.input.focusOrder.includes(id))
    rt.input.focusOrder.push(id);
  const tap = interactive ? consumeFirstEvent(rt, (ev) =>
    ev.type === "tap" && hit(bounds, ev.x, ev.y)) : null;
  if (tap) {
    selected = Math.min(count - 1,
      Math.max(0, Math.floor((tap.x - bounds.x) * count / bounds.width)));
    if (id)
      rt.input.focus = id;
  }
  if (interactive && id && rt.input.focus === id) {
    const key = consumeFirstEvent(rt, (ev) => ev.type === "key" &&
      (Number(ev.key) === KeyLeft || Number(ev.key) === KeyRight));
    if (key)
      selected = Number(key.key) === KeyRight ?
        (selected + 1) % count : (selected + count - 1) % count;
  }
  if (state && selectedRef)
    state[selectedRef] = selected;
  return { open: true, count, selected };
}

function handleWidget(rt, name, args, state) {
  if (!rt.input)
    return false;
  if (name === "Disabled") {
    if (String(args || "").trim() === "end") {
      if (rt.disabledStack.length > 0)
        rt.disabledStack.pop();
      return false;
    }
    rt.disabledStack.push(numberValue(args) !== 0);
    return false;
  }
  if (rt.disabledStack.some(Boolean))
    return false;
  switch (name) {
  case "Button":
    return handleButton(rt, args);
  case "Card":
    return handleCard(rt, args);
  case "Popup":
    return handlePopup(args);
  case "TextField":
  case "TextArea":
    return handleTextInput(rt, state, args, name === "TextArea");
  case "Slider":
    return handleSlider(rt, state, args);
  case "Toggle":
    return handleToggle(rt, state, args);
  case "Checkbox":
    return handleCheckbox(rt, state, args);
  case "Dropdown":
    return handleDropdown(rt, state, args);
  case "ListBox":
    return handleListBox(rt, state, args);
  case "TreeView":
    return handleTreeView(rt, state, args);
  case "TableView":
    return handleTableView(rt, state, args);
  case "TabBar":
    return handleTabBar(rt, state, args).selected;
  default:
    return false;
  }
}

function widgetTag(item) {
  const args = item.args || {};
  if (item.meta?.tag)
    return String(item.meta.tag).toLowerCase();
  const authoredTag = propStringAny(args, ["dom", "dom_tag", "html_tag", "tag"]);
  if (/^[a-z][a-z0-9-]*$/i.test(authoredTag))
    return authoredTag.toLowerCase();
  if (item.name !== "Output" &&
      (metaString(item.meta, "htmlFor") || propStringAny(args, ["for", "dom_for", "html_for"])))
    return "label";
  switch (item.name) {
  case "Screen":
  case "Page":
  case "Main":
    return "main";
  case "Section":
    return "section";
  case "Search":
    return "search";
  case "Article":
    return "article";
  case "Aside":
    return "aside";
  case "Base":
    return "base";
  case "Meta":
    return "meta";
  case "Title":
    return "title";
  case "ImageMap":
    return "map";
  case "Area":
    return "area";
  case "LineBreak":
    return "br";
  case "Template":
    return "template";
  case "Slot":
    return "slot";
  case "Script":
    return "script";
  case "StyleElement":
    return "style";
  case "NoScript":
    return "noscript";
  case "Header":
    return "header";
  case "Footer":
    return "footer";
  case "NavigationBar":
  case "Navigation":
    return "nav";
  case "TitleBar":
    return "header";
  case "HGroup":
    return "hgroup";
  case "Fieldset":
    return "fieldset";
  case "Legend":
    return "legend";
  case "Collapsible":
  case "Details":
    return "details";
  case "Summary":
    return "summary";
  case "Modal":
  case "Dialog":
    return "dialog";
  case "Form":
    return "form";
  case "Label":
    return "label";
  case "Output":
    return "output";
  case "Disabled":
    return "fieldset";
  case "Popup":
    return (propNumber(args, "flags", 0) & 2) !== 0 ? "dialog" : "div";
  case "Heading":
    return "h" + Math.max(1, Math.min(6, propNumber(args, "level", 2)));
  case "Paragraph":
  case "ParagraphText":
    return "p";
  case "BlockQuote":
    return "blockquote";
  case "Quote":
    return "q";
  case "CodeBlock":
  case "Pre":
    return "pre";
  case "Code":
    return "code";
  case "Strong":
  case "Bold":
    return "strong";
  case "Emphasis":
  case "Italic":
    return "em";
  case "Abbreviation":
    return "abbr";
  case "Data":
    return "data";
  case "Deleted":
    return "del";
  case "Inserted":
    return "ins";
  case "Subscript":
    return "sub";
  case "Superscript":
    return "sup";
  case "Keyboard":
    return "kbd";
  case "Sample":
    return "samp";
  case "Variable":
    return "var";
  case "Cite":
    return "cite";
  case "Mark":
    return "mark";
  case "Time":
    return "time";
  case "Address":
    return "address";
  case "Small":
    return "small";
  case "Figure":
    return "figure";
  case "Figcaption":
    return "figcaption";
  case "UnorderedList":
    return "ul";
  case "OrderedList":
    return "ol";
  case "DescriptionList":
    return "dl";
  case "DataList":
    return "datalist";
  case "DescriptionTerm":
    return "dt";
  case "DescriptionDetails":
    return "dd";
  case "Ruby":
    return "ruby";
  case "RubyText":
    return "rt";
  case "RubyParenthesis":
    return "rp";
  case "BidirectionalIsolate":
    return "bdi";
  case "BidirectionalOverride":
    return "bdo";
  case "WordBreakOpportunity":
    return "wbr";
  case "Text":
  case "Icon":
    return "span";
  case "Bullet":
  case "ListItem":
    return "li";
  case "Link":
    return "a";
  case "Button":
    return "button";
  case "Card":
    return isTruthyProp(args, "clickable") || isTruthyProp(args, "Clickable")
      ? "button" : "div";
  case "TextField":
  case "Input":
    return "input";
  case "TextArea":
    return "textarea";
  case "ColorPicker":
    return "input";
  case "Slider":
  case "Spinbox":
    return "input";
  case "Dropdown":
  case "ListBox":
  case "Select":
    return "select";
  case "OptionGroup":
    return "optgroup";
  case "Option":
    return "option";
  case "Image":
    return "img";
  case "Video":
    return "video";
  case "Audio":
    return "audio";
  case "Source":
    return "source";
  case "Track":
    return "track";
  case "IFrame":
    return "iframe";
  case "Embed":
    return "embed";
  case "EmbeddedObject":
    return "object";
  case "Param":
    return "param";
  case "Checkbox":
  case "Toggle":
  case "Radio":
    return "input";
  case "Progress":
    return "progress";
  case "Meter":
    return "meter";
  case "Line":
  case "Separator":
    return "hr";
  case "Menu":
    return "menu";
  case "Table":
  case "TableView":
    return "table";
  case "TableCaption":
    return "caption";
  case "TableHead":
    return "thead";
  case "TableBody":
    return "tbody";
  case "TableFoot":
    return "tfoot";
  case "TableRow":
    return "tr";
  case "TableColumnGroup":
    return "colgroup";
  case "TableColumn":
    return "col";
  case "TableCell":
    return /^(col|row|colgroup|rowgroup)$/i.test(
      metaString(item.meta, "scope") || propStringAny(args, ["scope", "dom_scope", "html_scope"]))
      ? "th" : "td";
  case "Canvas":
  case "Plot":
  case "CanvasGrid":
    return "canvas";
  case "Toast":
    return "output";
  default:
    return "div";
  }
}

function widgetHasAuthoredTag(item) {
  const args = item.args || {};
  if (item.meta?.tag)
    return true;
  return /^[a-z][a-z0-9-]*$/i.test(
    propStringAny(args, ["dom", "dom_tag", "html_tag", "tag"]));
}

function widgetText(item) {
  const args = item.args || {};
  switch (item.name) {
  case "Text":
  case "Heading":
  case "Paragraph":
  case "ParagraphText":
  case "Link":
  case "Article":
  case "Aside":
  case "Header":
  case "Footer":
  case "Main":
  case "Navigation":
  case "BlockQuote":
  case "Quote":
  case "CodeBlock":
  case "Pre":
  case "Code":
  case "Strong":
  case "Bold":
  case "Emphasis":
  case "Italic":
  case "Abbreviation":
  case "Data":
  case "Deleted":
  case "Inserted":
  case "Subscript":
  case "Superscript":
  case "Keyboard":
  case "Sample":
  case "Variable":
  case "Cite":
  case "Mark":
  case "Time":
  case "Address":
  case "Small":
  case "Ruby":
  case "RubyText":
  case "RubyParenthesis":
  case "BidirectionalIsolate":
  case "BidirectionalOverride":
  case "Figure":
  case "Figcaption":
  case "Video":
  case "Audio":
  case "IFrame":
  case "Summary":
  case "Legend":
  case "Label":
  case "Option":
  case "ListItem":
  case "DescriptionTerm":
  case "DescriptionDetails":
  case "Script":
  case "NoScript":
  case "Title":
  case "StyleElement":
    return propString(args, "text", "");
  case "Output":
    return propString(args, "text", propString(args, "value", ""));
  case "Button":
    return propString(args, "label", "");
  case "Selectable":
    return propString(args, "text", propString(args, "label", ""));
  case "Progress":
  case "Meter":
    return propString(args, "label", "");
  case "TextField":
  case "TextArea":
    return propString(args, "text", propString(args, "value", ""));
  case "TableCaption":
  case "TableCell":
    return propString(args, "text", "");
  default:
    return "";
  }
}

function widgetLinkURL(item) {
  if (item.name !== "Link" && item.name !== "Area")
    return "";
  return propStringAny(item.args, ["href", "url", "link", "dom_href", "html_href"]);
}

function widgetInputType(item) {
  switch (item.name) {
  case "Checkbox":
  case "Toggle":
    return "checkbox";
  case "Radio":
    return "radio";
  case "ColorPicker":
    return "color";
  case "Slider":
    return "range";
  case "Spinbox":
    return "number";
  case "TextField":
    return propStringAny(item.args, ["input_type", "type", "dom_type", "html_type", "dom_input_type"], "text");
  case "Input":
    return propStringAny(item.args, ["input_type", "type", "dom_type", "html_type", "dom_input_type"], "number");
  default:
    return "";
  }
}

function progressPositionalProp(args, index, fallback = "") {
  const text = String(args || "");
  const match = text.match(/ProgressProps\)\s*\{\s*(?:\{[^{}]*\}|\([^)]+\)\s*\{[^{}]*\})\s*,\s*([^,}]+)\s*,\s*([^,}]+)\s*,\s*([^,}]+)/);
  if (!match)
    return fallback;
  return String(match[index] || "").trim() || fallback;
}

function widgetDOMValue(item) {
  if (item.name === "ColorPicker")
    return propString(item.args, "value", "");
  if (item.name === "Slider" || item.name === "Spinbox" || item.name === "Input")
    return propString(item.args, "value", "");
  if (item.name === "Selectable" || item.name === "Option")
    return propStringAny(item.args, ["value", "dom_value", "html_value"]);
  if (item.name === "Output")
    return propString(item.args, "value", propString(item.args, "text", ""));
  if (item.name !== "Progress" && item.name !== "Meter")
    return "";
  if (item.args && typeof item.args === "object" && !Array.isArray(item.args)) {
    const value = item.args.value;
    return value === undefined || value === null ? "" : String(value);
  }
  return propString(item.args, "value", progressPositionalProp(item.args, 3));
}

function widgetMin(item) {
  if (item.name === "Slider" || item.name === "Spinbox" || item.name === "Input")
    return propStringAny(item.args, ["min", "dom_min", "html_min", "form_min"]);
  if (item.name === "TextField")
    return propStringAny(item.args, ["min", "dom_min", "html_min", "form_min"]);
  if (item.name !== "Progress" && item.name !== "Meter")
    return "";
  if (item.args && typeof item.args === "object" && !Array.isArray(item.args)) {
    const value = item.args.min;
    return value === undefined || value === null ? "" : String(value);
  }
  return propString(item.args, "min", progressPositionalProp(item.args, 1));
}

function widgetMax(item) {
  if (item.name === "Slider" || item.name === "Spinbox" || item.name === "Input")
    return propStringAny(item.args, ["max", "dom_max", "html_max", "form_max"]);
  if (item.name === "TextField")
    return propStringAny(item.args, ["max", "dom_max", "html_max", "form_max"]);
  if (item.name !== "Progress" && item.name !== "Meter")
    return "";
  if (item.args && typeof item.args === "object" && !Array.isArray(item.args)) {
    const value = item.args.max;
    return value === undefined || value === null ? "" : String(value);
  }
  return propString(item.args, "max", progressPositionalProp(item.args, 2));
}

function widgetLevel(item) {
  if (item.name !== "Heading")
    return 0;
  return Math.max(1, Math.min(6, propNumber(item.args || {}, "level", 2)));
}

function widgetAccessibleLabel(item) {
  const args = item.args || {};
  switch (item.name) {
  case "Checkbox":
  case "Radio":
  case "Input":
  case "Slider":
  case "Spinbox":
  case "ColorPicker":
  case "Drag":
  case "Progress":
  case "Meter":
    return propString(args, "label", "");
  case "Toggle":
    return propString(args, "label",
      propString(args, "on_label", propString(args, "off_label", "")));
  case "Fieldset":
    return propString(args, "title", "");
  case "Modal":
  case "Section":
  case "NavigationBar":
  case "TitleBar":
    return propString(args, "label", propString(args, "title", ""));
  default:
    return "";
  }
}

function widgetFallbackRole(item) {
  if (item.name !== "Popup")
    return "";
  const flags = propNumber(item.args || {}, "flags", 0);
  if ((flags & 2) !== 0)
    return "";
  if ((flags & 4) !== 0)
    return "menu";
  if ((flags & 1) !== 0)
    return "tooltip";
  return "";
}

function widgetAriaLive(item) {
  if (item.name === "Toast")
    return "polite";
  return "";
}

function propPrefixedAttrs(source, prefixes, normalize, valid) {
  const out = {};
  const add = (rawName, value) => {
    const key = String(rawName || "").trim();
    const prefix = prefixes.find((candidate) => key.startsWith(candidate));
    if (!prefix || key.length <= prefix.length)
      return;
    const attr = normalize(key.slice(prefix.length));
    if (!attr || !valid.test(attr))
      return;
    out[attr] = value === undefined || value === null ? "" : String(value);
  };
  if (source && typeof source === "object" && !Array.isArray(source)) {
    for (const [name, value] of Object.entries(source))
      add(name, value);
  } else {
    const prefixPattern = prefixes.map((prefix) => prefix.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|");
    const pattern = new RegExp(`\\.(${prefixPattern}[A-Za-z0-9_.:-]+)\\s*=\\s*("(?:[^"\\\\]|\\\\.)*"|[^,}]+)`, "g");
    let match = null;
    while ((match = pattern.exec(String(source || ""))) !== null)
      add(match[1], stringValue(match[2], String(match[2] || "").trim()));
  }
  return out;
}

function propDataAttrs(meta, args = null) {
  const out = propPrefixedAttrs(args, ["data_", "dom_data_", "html_data_"],
    (name) => String(name).replace(/_/g, "-").toLowerCase(),
    /^[a-z0-9][a-z0-9.-]*$/);
  const data = meta && typeof meta.data === "object" && !Array.isArray(meta.data) ? meta.data : null;
  for (const [name, value] of Object.entries(data || {})) {
    const attr = String(name).trim().replace(/_/g, "-").toLowerCase();
    if (!attr || !/^[a-z0-9][a-z0-9.-]*$/.test(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
  }
  return out;
}

const canonicalAriaAttrNames = new Set([
  "activedescendant", "busy", "checked", "colcount", "colindex", "controls",
  "current", "describedby", "description", "details", "disabled", "errormessage",
  "expanded", "flowto", "haspopup", "invalid", "label", "labelledby", "level", "live", "multiselectable",
  "orientation", "owns", "posinset", "pressed", "readonly", "required",
  "rowcount", "rowindex", "selected", "setsize", "sort"
]);

const promotedAriaRelationAttrNames = new Set([
  "details", "errormessage", "flowto"
]);

function propAriaAttrs(meta, args = null) {
  const out = propPrefixedAttrs(args, ["aria_", "dom_aria_", "html_aria_"],
    (name) => String(name).replace(/_/g, "-").toLowerCase(),
    /^[a-z0-9][a-z0-9.-]*$/);
  for (const name of Object.keys(out)) {
    if (canonicalAriaAttrNames.has(name))
      delete out[name];
  }
  const aria = meta && typeof meta.aria === "object" && !Array.isArray(meta.aria) ? meta.aria : null;
  for (const [name, value] of Object.entries(aria || {})) {
    const attr = String(name).trim().replace(/_/g, "-").toLowerCase();
    if (!attr || !/^[a-z0-9][a-z0-9.-]*$/.test(attr))
      continue;
    if (promotedAriaRelationAttrNames.has(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
  }
  return out;
}

function propExtraAttrs(meta, args = null) {
  const out = propPrefixedAttrs(args, ["attr_", "dom_attr_", "html_attr_"],
    (name) => String(name).replace(/_/g, "-").toLowerCase(),
    /^[a-z][a-z0-9._:-]*$/);
  const attrs = meta && typeof meta.extraAttrs === "object" && !Array.isArray(meta.extraAttrs)
    ? meta.extraAttrs : null;
  for (const [name, value] of Object.entries(attrs || {})) {
    const attr = String(name).trim().toLowerCase();
    if (!attr || !/^[a-z][a-z0-9._:-]*$/.test(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
  }
  return out;
}

function setWidgetNativeAttr(out, name, value) {
  if (value === undefined || value === null || value === false || value === "")
    return;
  out[name] = value === true ? true : String(value);
}

function widgetNativeAttrs(item, meta, args) {
  const out = {};
  switch (item.name) {
  case "BlockQuote":
  case "Quote":
    setWidgetNativeAttr(out, "cite", metaString(meta, "cite") ||
      propStringAny(args, ["cite", "dom_cite", "html_cite"]));
    break;
  case "Time":
    setWidgetNativeAttr(out, "datetime", metaString(meta, "dateTime") ||
      propStringAny(args, ["datetime", "date_time", "dom_datetime", "html_datetime"]));
    break;
  case "Data":
    setWidgetNativeAttr(out, "value", metaString(meta, "domValue") ||
      propStringAny(args, ["value", "dom_value", "html_value"]));
    break;
  case "Deleted":
  case "Inserted":
    setWidgetNativeAttr(out, "cite", metaString(meta, "cite") ||
      propStringAny(args, ["cite", "dom_cite", "html_cite"]));
    setWidgetNativeAttr(out, "datetime", metaString(meta, "dateTime") ||
      propStringAny(args, ["datetime", "date_time", "dom_datetime", "html_datetime"]));
    break;
  case "OrderedList":
    setWidgetNativeAttr(out, "start", metaString(meta, "start") ||
      propStringAny(args, ["start", "dom_start", "html_start"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "listType") ||
      propStringAny(args, ["type", "list_type", "dom_type", "html_type"]));
    setWidgetNativeAttr(out, "reversed", metaBool(meta, "reversed") ||
      isTruthyPropAny(args, ["reversed", "dom_reversed", "html_reversed"]));
    break;
  case "UnorderedList":
    setWidgetNativeAttr(out, "type", metaString(meta, "listType") ||
      propStringAny(args, ["type", "list_type", "dom_type", "html_type"]));
    break;
  case "ListItem":
    setWidgetNativeAttr(out, "value", metaString(meta, "domValue") ||
      propStringAny(args, ["value", "dom_value", "html_value"]));
    break;
  case "Link":
    setWidgetNativeAttr(out, "ping", metaString(meta, "ping") ||
      propStringAny(args, ["ping", "dom_ping", "html_ping"]));
    setWidgetNativeAttr(out, "hreflang", metaString(meta, "hrefLang") ||
      propStringAny(args, ["hreflang", "href_lang", "dom_hreflang", "html_hreflang"]));
    setWidgetNativeAttr(out, "referrerpolicy", metaString(meta, "referrerPolicy") ||
      propStringAny(args, ["referrerpolicy", "referrer_policy", "dom_referrerpolicy", "html_referrerpolicy"]));
    break;
  case "Base":
    setWidgetNativeAttr(out, "href", metaString(meta, "href") ||
      propStringAny(args, ["href", "url", "dom_href", "html_href"]));
    setWidgetNativeAttr(out, "target", metaString(meta, "target") ||
      propStringAny(args, ["target", "dom_target", "html_target"]));
    break;
  case "Meta":
    setWidgetNativeAttr(out, "name", metaString(meta, "domName") ||
      propStringAny(args, ["name", "meta_name", "dom_name", "html_name"]));
    setWidgetNativeAttr(out, "content", metaString(meta, "content") ||
      propStringAny(args, ["content", "dom_content", "html_content"]));
    setWidgetNativeAttr(out, "charset", metaString(meta, "charset") ||
      propStringAny(args, ["charset", "char_set", "dom_charset", "html_charset"]));
    setWidgetNativeAttr(out, "http-equiv", metaString(meta, "httpEquiv") ||
      propStringAny(args, ["http_equiv", "httpequiv", "dom_http_equiv", "html_http_equiv"]));
    setWidgetNativeAttr(out, "property", metaString(meta, "property") ||
      propStringAny(args, ["property", "meta_property", "dom_property", "html_property"]));
    setWidgetNativeAttr(out, "media", metaString(meta, "media") ||
      propStringAny(args, ["media", "dom_media", "html_media"]));
    setWidgetNativeAttr(out, "itemprop", metaString(meta, "itemProp") ||
      propStringAny(args, ["itemprop", "item_prop", "dom_itemprop", "html_itemprop"]));
    break;
  case "OptionGroup":
    setWidgetNativeAttr(out, "label", metaString(meta, "optionLabel") ||
      propStringAny(args, ["label", "title", "dom_label", "html_label"]));
    break;
  case "Area":
    setWidgetNativeAttr(out, "ping", metaString(meta, "ping") ||
      propStringAny(args, ["ping", "dom_ping", "html_ping"]));
    setWidgetNativeAttr(out, "hreflang", metaString(meta, "hrefLang") ||
      propStringAny(args, ["hreflang", "href_lang", "dom_hreflang", "html_hreflang"]));
    setWidgetNativeAttr(out, "referrerpolicy", metaString(meta, "referrerPolicy") ||
      propStringAny(args, ["referrerpolicy", "referrer_policy", "dom_referrerpolicy", "html_referrerpolicy"]));
    setWidgetNativeAttr(out, "shape", metaString(meta, "shape") ||
      propStringAny(args, ["shape", "dom_shape", "html_shape"]));
    setWidgetNativeAttr(out, "coords", metaString(meta, "coords") ||
      propStringAny(args, ["coords", "coordinates", "dom_coords", "html_coords"]));
    break;
  case "Image":
    setWidgetNativeAttr(out, "srcset", metaString(meta, "srcSet") ||
      propStringAny(args, ["srcset", "src_set", "dom_srcset", "html_srcset"]));
    setWidgetNativeAttr(out, "sizes", metaString(meta, "sizes") ||
      propStringAny(args, ["sizes", "dom_sizes", "html_sizes"]));
    setWidgetNativeAttr(out, "loading", metaString(meta, "loading") ||
      propStringAny(args, ["loading", "dom_loading", "html_loading"]));
    setWidgetNativeAttr(out, "decoding", metaString(meta, "decoding") ||
      propStringAny(args, ["decoding", "dom_decoding", "html_decoding"]));
    setWidgetNativeAttr(out, "fetchpriority", metaString(meta, "fetchPriority") ||
      propStringAny(args, ["fetchpriority", "fetch_priority", "dom_fetchpriority", "html_fetchpriority"]));
    setWidgetNativeAttr(out, "referrerpolicy", metaString(meta, "referrerPolicy") ||
      propStringAny(args, ["referrerpolicy", "referrer_policy", "dom_referrerpolicy", "html_referrerpolicy"]));
    setWidgetNativeAttr(out, "crossorigin", metaString(meta, "crossOrigin") ||
      propStringAny(args, ["crossorigin", "cross_origin", "dom_crossorigin", "html_crossorigin"]));
    setWidgetNativeAttr(out, "width", metaString(meta, "width") ||
      propStringAny(args, ["width", "dom_width", "html_width"]));
    setWidgetNativeAttr(out, "height", metaString(meta, "height") ||
      propStringAny(args, ["height", "dom_height", "html_height"]));
    break;
  case "TableColumnGroup":
  case "TableColumn":
    setWidgetNativeAttr(out, "span", metaString(meta, "span") ||
      propStringAny(args, ["span", "dom_span", "html_span"]));
    break;
  case "Video":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "poster", metaString(meta, "poster") ||
      propStringAny(args, ["poster", "dom_poster", "html_poster"]));
    setWidgetNativeAttr(out, "controls", metaBool(meta, "controls") ||
      isTruthyPropAny(args, ["controls", "dom_controls", "html_controls"]));
    setWidgetNativeAttr(out, "autoplay", metaBool(meta, "autoplay") ||
      isTruthyPropAny(args, ["autoplay", "auto_play", "dom_autoplay", "html_autoplay"]));
    setWidgetNativeAttr(out, "loop", metaBool(meta, "loop") ||
      isTruthyPropAny(args, ["loop", "dom_loop", "html_loop"]));
    setWidgetNativeAttr(out, "muted", metaBool(meta, "muted") ||
      isTruthyPropAny(args, ["muted", "dom_muted", "html_muted"]));
    setWidgetNativeAttr(out, "preload", metaString(meta, "preload") ||
      propStringAny(args, ["preload", "dom_preload", "html_preload"]));
    break;
  case "Audio":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "controls", metaBool(meta, "controls") ||
      isTruthyPropAny(args, ["controls", "dom_controls", "html_controls"]));
    setWidgetNativeAttr(out, "autoplay", metaBool(meta, "autoplay") ||
      isTruthyPropAny(args, ["autoplay", "auto_play", "dom_autoplay", "html_autoplay"]));
    setWidgetNativeAttr(out, "loop", metaBool(meta, "loop") ||
      isTruthyPropAny(args, ["loop", "dom_loop", "html_loop"]));
    setWidgetNativeAttr(out, "muted", metaBool(meta, "muted") ||
      isTruthyPropAny(args, ["muted", "dom_muted", "html_muted"]));
    setWidgetNativeAttr(out, "preload", metaString(meta, "preload") ||
      propStringAny(args, ["preload", "dom_preload", "html_preload"]));
    break;
  case "Source":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "srcset", metaString(meta, "srcSet") ||
      propStringAny(args, ["srcset", "src_set", "dom_srcset", "html_srcset"]));
    setWidgetNativeAttr(out, "sizes", metaString(meta, "sizes") ||
      propStringAny(args, ["sizes", "dom_sizes", "html_sizes"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "type") ||
      propStringAny(args, ["type", "mime_type", "dom_type", "html_type"]));
    setWidgetNativeAttr(out, "media", metaString(meta, "media") ||
      propStringAny(args, ["media", "dom_media", "html_media"]));
    break;
  case "Track":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "kind", metaString(meta, "kindAttr") ||
      propStringAny(args, ["kind", "track_kind", "dom_kind", "html_kind"]));
    setWidgetNativeAttr(out, "srclang", metaString(meta, "srcLang") ||
      propStringAny(args, ["srclang", "src_lang", "dom_srclang", "html_srclang"]));
    setWidgetNativeAttr(out, "label", metaString(meta, "trackLabel") ||
      propStringAny(args, ["label", "track_label", "dom_label", "html_label"]));
    setWidgetNativeAttr(out, "default", metaBool(meta, "default") ||
      isTruthyPropAny(args, ["default", "dom_default", "html_default"]));
    break;
  case "Script":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "type") ||
      propStringAny(args, ["type", "mime_type", "dom_type", "html_type"]));
    setWidgetNativeAttr(out, "async", metaBool(meta, "async") ||
      isTruthyPropAny(args, ["async", "dom_async", "html_async"]));
    setWidgetNativeAttr(out, "defer", metaBool(meta, "defer") ||
      isTruthyPropAny(args, ["defer", "dom_defer", "html_defer"]));
    setWidgetNativeAttr(out, "crossorigin", metaString(meta, "crossOrigin") ||
      propStringAny(args, ["crossorigin", "cross_origin", "dom_crossorigin", "html_crossorigin"]));
    setWidgetNativeAttr(out, "integrity", metaString(meta, "integrity") ||
      propStringAny(args, ["integrity", "dom_integrity", "html_integrity"]));
    setWidgetNativeAttr(out, "referrerpolicy", metaString(meta, "referrerPolicy") ||
      propStringAny(args, ["referrerpolicy", "referrer_policy", "dom_referrerpolicy", "html_referrerpolicy"]));
    setWidgetNativeAttr(out, "nomodule", metaBool(meta, "noModule") ||
      isTruthyPropAny(args, ["nomodule", "no_module", "dom_nomodule", "html_nomodule"]));
    setWidgetNativeAttr(out, "nonce", metaString(meta, "nonce") ||
      propStringAny(args, ["nonce", "dom_nonce", "html_nonce"]));
    break;
  case "StyleElement":
    setWidgetNativeAttr(out, "media", metaString(meta, "media") ||
      propStringAny(args, ["media", "dom_media", "html_media"]));
    setWidgetNativeAttr(out, "nonce", metaString(meta, "nonce") ||
      propStringAny(args, ["nonce", "dom_nonce", "html_nonce"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "type") ||
      propStringAny(args, ["type", "mime_type", "dom_type", "html_type"]));
    break;
  case "IFrame":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "loading", metaString(meta, "loading") ||
      propStringAny(args, ["loading", "dom_loading", "html_loading"]));
    setWidgetNativeAttr(out, "allow", metaString(meta, "allow") ||
      propStringAny(args, ["allow", "dom_allow", "html_allow"]));
    setWidgetNativeAttr(out, "allowfullscreen", metaBool(meta, "allowFullscreen") ||
      isTruthyPropAny(args, ["allowfullscreen", "allow_fullscreen", "dom_allowfullscreen", "html_allowfullscreen"]));
    setWidgetNativeAttr(out, "sandbox", metaString(meta, "sandbox") ||
      propStringAny(args, ["sandbox", "dom_sandbox", "html_sandbox"]));
    setWidgetNativeAttr(out, "referrerpolicy", metaString(meta, "referrerPolicy") ||
      propStringAny(args, ["referrerpolicy", "referrer_policy", "dom_referrerpolicy", "html_referrerpolicy"]));
    setWidgetNativeAttr(out, "credentialless", metaBool(meta, "credentialless") ||
      isTruthyPropAny(args, ["credentialless", "dom_credentialless", "html_credentialless"]));
    setWidgetNativeAttr(out, "name", metaString(meta, "frameName") ||
      propStringAny(args, ["frame_name", "iframe_name", "dom_frame_name", "html_frame_name"]));
    setWidgetNativeAttr(out, "width", metaString(meta, "width") ||
      propStringAny(args, ["width", "dom_width", "html_width"]));
    setWidgetNativeAttr(out, "height", metaString(meta, "height") ||
      propStringAny(args, ["height", "dom_height", "html_height"]));
    break;
  case "Embed":
    setWidgetNativeAttr(out, "src", metaString(meta, "src") ||
      propStringAny(args, ["src", "asset_path", "dom_src", "html_src"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "type") ||
      propStringAny(args, ["type", "mime_type", "dom_type", "html_type"]));
    break;
  case "EmbeddedObject":
    setWidgetNativeAttr(out, "data", metaString(meta, "objectData") ||
      propStringAny(args, ["data", "object_data", "dom_object_data", "html_object_data"]));
    setWidgetNativeAttr(out, "type", metaString(meta, "type") ||
      propStringAny(args, ["type", "mime_type", "dom_type", "html_type"]));
    setWidgetNativeAttr(out, "width", metaString(meta, "width") ||
      propStringAny(args, ["width", "dom_width", "html_width"]));
    setWidgetNativeAttr(out, "height", metaString(meta, "height") ||
      propStringAny(args, ["height", "dom_height", "html_height"]));
    break;
  case "Param":
    setWidgetNativeAttr(out, "name", metaString(meta, "paramName") ||
      propStringAny(args, ["name", "param_name", "dom_name", "html_name"]));
    setWidgetNativeAttr(out, "value", metaString(meta, "domValue") ||
      propStringAny(args, ["value", "param_value", "dom_value", "html_value"]));
    break;
  default:
    break;
  }
  return out;
}

function metaBool(meta, name) {
  const value = meta?.[name];
  if (typeof value === "string")
    return /^(true|1|yes)$/i.test(value);
  return !!value;
}

function metaString(meta, name) {
  const value = meta?.[name];
  return value === undefined || value === null ? "" : String(value);
}

function metaStringOrProp(meta, name, args, props) {
  const value = meta?.[name];
  return value === undefined || value === null ? propStringAny(args, props) : String(value);
}

function metaAriaStringOrProp(meta, name, ariaName, args, props) {
  const value = meta?.[name];
  if (value !== undefined && value !== null)
    return String(value);
  const aria = meta && typeof meta.aria === "object" && !Array.isArray(meta.aria) ? meta.aria : null;
  const ariaValue = aria?.[ariaName];
  return ariaValue === undefined || ariaValue === null ? propStringAny(args, props) : String(ariaValue);
}

function metaBoolOrProp(meta, name, args, props) {
  const value = meta?.[name];
  if (value === undefined || value === null)
    return isTruthyPropAny(args, props);
  if (typeof value === "string")
    return /^(true|1|yes)$/i.test(value);
  return !!value;
}

function metaNumberOrProp(meta, name, args, props, fallback = null) {
  const value = meta?.[name];
  const raw = value === undefined || value === null ? propStringAny(args, props) : value;
  if (raw === undefined || raw === null || raw === "")
    return fallback;
  const n = Number(raw);
  return Number.isFinite(n) ? Math.trunc(n) : fallback;
}

function webNodeFromWidget(item, index) {
  const args = item.args || {};
  const meta = item.meta || {};
  const authoredTag = widgetHasAuthoredTag(item);
  const bounds = parseBounds(args);
  const classes = [...propClassList(args)];
  if (meta.class !== undefined && meta.class !== null)
    String(meta.class).split(/\s+/).filter(Boolean).forEach((name) => classes.push(name));
  const state = {
    disabled: isTruthyProp(args, "disabled"),
    loading: isTruthyProp(args, "loading"),
    selected: isTruthyProp(args, "selected"),
    checked: isTruthyProp(args, "checked"),
    invalid: isTruthyProp(args, "invalid"),
    valid: isTruthyProp(args, "valid"),
    indeterminate: isTruthyProp(args, "indeterminate"),
    default: isTruthyProp(args, "default"),
    autofill: isTruthyProp(args, "autofill"),
    "placeholder-shown": isTruthyProp(args, "placeholder_shown") ||
      isTruthyProp(args, "placeholderShown"),
    expanded: isTruthyProp(args, "expanded"),
    open: isTruthyProp(args, "open"),
    hover: false,
    pressed: false,
    focus: false
  };
  if (item.name === "Disabled")
    state.disabled = numberValue(args, 0) !== 0;
  if (item.name === "Popup")
    state.open = handlePopup(args);
  const node = {
    index,
    kind: item.name,
    tag: widgetTag(item),
    key: meta.key === undefined || meta.key === null
      ? meta.nodeName || propString(args, "key", propString(args, "id", String(index)))
      : String(meta.key),
    name: meta.nodeName || propString(args, "name", ""),
    webRef: metaStringOrProp(meta, "ref", args, ["dom_ref", "web_ref", "kry_ref"]),
    path: meta.path === undefined || meta.path === null ? "" : String(meta.path),
    parentPath: meta.parentPath === undefined || meta.parentPath === null ? "" : String(meta.parentPath),
    sourcePath: meta.sourcePath === undefined || meta.sourcePath === null ? "" : String(meta.sourcePath),
    sourceLine: Number.isFinite(Number(meta.sourceLine)) ? Math.trunc(Number(meta.sourceLine)) : 0,
    sourceColumn: Number.isFinite(Number(meta.sourceColumn)) ? Math.trunc(Number(meta.sourceColumn)) : 0,
    sourceEndLine: Number.isFinite(Number(meta.sourceEndLine)) ? Math.trunc(Number(meta.sourceEndLine)) : 0,
    sourceEndColumn: Number.isFinite(Number(meta.sourceEndColumn)) ? Math.trunc(Number(meta.sourceEndColumn)) : 0,
    domId: metaStringOrProp(meta, "id", args, ["dom_id", "html_id"]),
    domName: metaStringOrProp(meta, "domName", args, ["dom_name", "html_name", "name_attr"]),
    classes: [...new Set(classes)],
    title: metaStringOrProp(meta, "title", args, ["title", "dom_title", "html_title"]),
    lang: metaStringOrProp(meta, "lang", args, ["lang", "language", "dom_lang", "html_lang"]),
    dir: metaStringOrProp(meta, "dir", args, ["dir", "dom_dir", "html_dir"]),
    translate: metaStringOrProp(meta, "translate", args, ["translate", "dom_translate", "html_translate"]),
    dirname: metaStringOrProp(meta, "dirname", args, ["dirname", "dir_name", "dom_dirname", "html_dirname"]),
    placeholder: meta.placeholder === undefined || meta.placeholder === null
      ? propStringAny(args, ["placeholder", "dom_placeholder"])
      : String(meta.placeholder),
    tabIndex: metaNumberOrProp(meta, "tabIndex", args,
      ["tab_index", "tabindex", "dom_tab_index", "html_tab_index"]),
    clickable: isTruthyProp(args, "clickable") || isTruthyProp(args, "Clickable"),
    text: widgetText(item),
    value: widgetText(item),
    domValue: metaString(meta, "domValue") || widgetDOMValue(item),
    level: widgetLevel(item),
    href: meta.href === undefined || meta.href === null ? widgetLinkURL(item) : String(meta.href),
    target: meta.target === undefined || meta.target === null
      ? propStringAny(args, ["target", "dom_target", "html_target"])
      : String(meta.target),
    rel: meta.rel === undefined || meta.rel === null
      ? propStringAny(args, ["rel", "dom_rel", "html_rel"])
      : String(meta.rel),
    htmlFor: metaStringOrProp(meta, "htmlFor", args, ["for", "dom_for", "html_for"]),
    dataList: metaStringOrProp(meta, "dataList", args, ["list", "datalist", "data_list", "dom_list", "html_list"]),
    useMap: metaStringOrProp(meta, "useMap", args, ["usemap", "use_map", "image_map", "dom_usemap", "html_usemap"]),
    part: metaStringOrProp(meta, "part", args, ["part", "dom_part", "html_part"]),
    slot: metaStringOrProp(meta, "slot", args, ["slot", "dom_slot", "html_slot"]),
    dataAttrs: propDataAttrs(meta, args),
    extraAttrs: { ...widgetNativeAttrs(item, meta, args), ...propExtraAttrs(meta, args) },
    inputType: meta.inputType === undefined || meta.inputType === null ? widgetInputType(item) : String(meta.inputType),
    formOwner: meta.formOwner === undefined || meta.formOwner === null
      ? propStringAny(args, ["form", "dom_form", "html_form"])
      : String(meta.formOwner),
    formAction: meta.formAction === undefined || meta.formAction === null
      ? propStringAny(args, ["form_action", "dom_action", "html_action"])
      : String(meta.formAction),
    formMethod: meta.formMethod === undefined || meta.formMethod === null
      ? propStringAny(args, ["form_method", "dom_method", "html_method"])
      : String(meta.formMethod),
    formEncType: meta.formEncType === undefined || meta.formEncType === null
      ? propStringAny(args, ["form_enctype", "dom_enctype", "html_enctype"])
      : String(meta.formEncType),
    autoComplete: meta.autoComplete === undefined || meta.autoComplete === null
      ? propStringAny(args, ["autocomplete", "dom_autocomplete", "html_autocomplete"])
      : String(meta.autoComplete),
    hidden: metaBoolOrProp(meta, "hidden", args, ["hidden", "dom_hidden", "html_hidden"]),
    draggable: metaStringOrProp(meta, "draggable", args, ["draggable", "dom_draggable", "html_draggable"]),
    spellCheck: metaStringOrProp(meta, "spellCheck", args,
      ["spellcheck", "spell_check", "dom_spellcheck", "html_spellcheck"]),
    contentEditable: metaStringOrProp(meta, "contentEditable", args,
      ["contenteditable", "content_editable", "dom_contenteditable", "html_contenteditable"]),
    autoFocus: metaBoolOrProp(meta, "autoFocus", args,
      ["autofocus", "auto_focus", "dom_autofocus", "html_autofocus"]),
    inert: metaBoolOrProp(meta, "inert", args, ["inert", "dom_inert", "html_inert"]),
    autoCapitalize: metaStringOrProp(meta, "autoCapitalize", args,
      ["autocapitalize", "auto_capitalize", "dom_autocapitalize", "html_autocapitalize"]),
    enterKeyHint: metaString(meta, "enterKeyHint") ||
      propStringAny(args, ["enterkeyhint", "enter_key_hint", "dom_enterkeyhint", "html_enterkeyhint"]),
    download: metaString(meta, "download") ||
      propStringAny(args, ["download", "dom_download", "html_download"]),
    formNoValidate: metaBoolOrProp(meta, "formNoValidate", args,
      ["form_no_validate", "formnovalidate", "dom_formnovalidate", "html_formnovalidate"]),
    noValidate: metaBoolOrProp(meta, "noValidate", args,
      ["no_validate", "novalidate", "dom_novalidate", "html_novalidate"]),
    popover: metaStringOrProp(meta, "popover", args, ["popover", "dom_popover", "html_popover"]),
    popoverTarget: metaStringOrProp(meta, "popoverTarget", args,
      ["popover_target", "popovertarget", "dom_popover_target", "html_popover_target"]),
    popoverTargetAction: metaStringOrProp(meta, "popoverTargetAction", args,
      ["popover_target_action", "popovertargetaction", "dom_popover_target_action", "html_popover_target_action"]),
    readOnly: metaBoolOrProp(meta, "readOnly", args,
      ["readonly", "read_only", "dom_readonly", "html_readonly"]),
    required: metaBoolOrProp(meta, "required", args, ["required", "dom_required", "html_required"]),
    min: metaString(meta, "min") || widgetMin(item),
    max: metaString(meta, "max") || widgetMax(item),
    step: metaString(meta, "step") ||
      propStringAny(args, ["step", "dom_step", "html_step"]),
    minLength: metaString(meta, "minLength") ||
      propStringAny(args, ["min_length", "minlength", "dom_minlength", "html_minlength"]),
    maxLength: metaString(meta, "maxLength") ||
      propStringAny(args, ["max_length", "maxlength", "dom_maxlength", "html_maxlength"]),
    pattern: metaString(meta, "pattern") ||
      propStringAny(args, ["pattern", "dom_pattern", "html_pattern"]),
    accept: metaString(meta, "accept") ||
      propStringAny(args, ["accept", "dom_accept", "html_accept"]),
    multiple: metaBoolOrProp(meta, "multiple", args, ["multiple", "dom_multiple", "html_multiple"]),
    inputMode: metaString(meta, "inputMode") ||
      propStringAny(args, ["input_mode", "inputmode", "dom_inputmode", "html_inputmode"]),
    headers: metaStringOrProp(meta, "headers", args, ["headers", "dom_headers", "html_headers"]),
    scope: metaStringOrProp(meta, "scope", args, ["scope", "dom_scope", "html_scope"]),
    colSpan: metaStringOrProp(meta, "colSpan", args, ["colspan", "col_span", "dom_colspan", "html_colspan"]),
    rowSpan: metaStringOrProp(meta, "rowSpan", args, ["rowspan", "row_span", "dom_rowspan", "html_rowspan"]),
    alt: propString(args, "alt", propString(args, "alt_text", "")),
    asset: propString(args, "asset_path", propString(args, "src", "")),
    role: metaStringOrProp(meta, "role", args, ["role", "dom_role", "html_role"]) ||
      widgetFallbackRole(item),
    ariaLabel: metaStringOrProp(meta, "ariaLabel", args,
      ["aria_label", "accessible_label", "dom_aria_label", "html_aria_label"]) ||
      widgetAccessibleLabel(item),
    ariaDescription: metaStringOrProp(meta, "ariaDescription", args,
      ["aria_description", "accessible_description", "dom_aria_description", "html_aria_description"]),
    ariaDescribedBy: metaStringOrProp(meta, "ariaDescribedBy", args,
      ["aria_describedby", "aria_described_by", "dom_aria_describedby", "html_aria_describedby"]),
    ariaDetails: metaAriaStringOrProp(meta, "ariaDetails", "details", args,
      ["aria_details", "aria_detail", "dom_aria_details", "html_aria_details"]),
    ariaErrorMessage: metaAriaStringOrProp(meta, "ariaErrorMessage", "errormessage", args,
      ["aria_errormessage", "aria_error_message", "dom_aria_errormessage", "html_aria_errormessage"]),
    ariaFlowTo: metaAriaStringOrProp(meta, "ariaFlowTo", "flowto", args,
      ["aria_flowto", "aria_flow_to", "dom_aria_flowto", "html_aria_flowto"]),
    ariaLabelledBy: metaStringOrProp(meta, "ariaLabelledBy", args,
      ["aria_labelledby", "aria_labelled_by", "dom_aria_labelledby", "html_aria_labelledby"]),
    ariaActiveDescendant: metaStringOrProp(meta, "ariaActiveDescendant", args,
      ["aria_activedescendant", "aria_active_descendant", "dom_aria_activedescendant", "html_aria_activedescendant"]),
    ariaControls: metaStringOrProp(meta, "ariaControls", args,
      ["aria_controls", "dom_aria_controls", "html_aria_controls"]),
    ariaOwns: metaStringOrProp(meta, "ariaOwns", args,
      ["aria_owns", "aria_own", "dom_aria_owns", "html_aria_owns"]),
    ariaSort: metaStringOrProp(meta, "ariaSort", args,
      ["aria_sort", "aria_sorted", "dom_aria_sort", "html_aria_sort"]),
    ariaOrientation: metaStringOrProp(meta, "ariaOrientation", args,
      ["aria_orientation", "dom_aria_orientation", "html_aria_orientation"]),
    ariaLevel: metaStringOrProp(meta, "ariaLevel", args,
      ["aria_level", "dom_aria_level", "html_aria_level"]),
    ariaPosInSet: metaStringOrProp(meta, "ariaPosInSet", args,
      ["aria_posinset", "aria_pos_in_set", "dom_aria_posinset", "html_aria_posinset"]),
    ariaSetSize: metaStringOrProp(meta, "ariaSetSize", args,
      ["aria_setsize", "aria_set_size", "dom_aria_setsize", "html_aria_setsize"]),
    ariaHasPopup: metaStringOrProp(meta, "ariaHasPopup", args,
      ["aria_haspopup", "aria_has_popup", "dom_aria_haspopup", "html_aria_haspopup"]),
    ariaMultiSelectable: metaStringOrProp(meta, "ariaMultiSelectable", args,
      ["aria_multiselectable", "aria_multi_selectable", "dom_aria_multiselectable", "html_aria_multiselectable"]),
    ariaRowIndex: metaStringOrProp(meta, "ariaRowIndex", args,
      ["aria_rowindex", "aria_row_index", "dom_aria_rowindex", "html_aria_rowindex"]),
    ariaColIndex: metaStringOrProp(meta, "ariaColIndex", args,
      ["aria_colindex", "aria_col_index", "dom_aria_colindex", "html_aria_colindex"]),
    ariaRowCount: metaStringOrProp(meta, "ariaRowCount", args,
      ["aria_rowcount", "aria_row_count", "dom_aria_rowcount", "html_aria_rowcount"]),
    ariaColCount: metaStringOrProp(meta, "ariaColCount", args,
      ["aria_colcount", "aria_col_count", "dom_aria_colcount", "html_aria_colcount"]),
    ariaLive: metaStringOrProp(meta, "ariaLive", args,
      ["aria_live", "live", "dom_aria_live", "html_aria_live"]) ||
      widgetAriaLive(item),
    ariaAttrs: propAriaAttrs(meta, args),
    onClick: metaStringOrProp(meta, "onClick", args, ["on_click", "onClick", "click"]),
    onDoubleClick: metaStringOrProp(meta, "onDoubleClick", args,
      ["on_double_click", "onDoubleClick", "on_dblclick", "dblclick"]),
    onInput: metaStringOrProp(meta, "onInput", args, ["on_input", "onInput", "input"]),
    onBeforeInput: metaStringOrProp(meta, "onBeforeInput", args,
      ["on_before_input", "on_beforeinput", "onBeforeInput", "beforeinput"]),
    onChange: metaStringOrProp(meta, "onChange", args, ["on_change", "onChange", "change"]),
    onSelect: metaStringOrProp(meta, "onSelect", args, ["on_select", "onSelect", "select"]),
    onKey: metaStringOrProp(meta, "onKey", args, ["on_key", "on_key_down", "onKey", "keydown"]),
    onKeyUp: metaStringOrProp(meta, "onKeyUp", args, ["on_key_up", "onKeyUp", "keyup"]),
    onInvalid: metaStringOrProp(meta, "onInvalid", args, ["on_invalid", "onInvalid", "invalid"]),
    onSubmit: metaStringOrProp(meta, "onSubmit", args, ["on_submit", "onSubmit", "submit"]),
    onReset: metaStringOrProp(meta, "onReset", args, ["on_reset", "onReset", "reset"]),
    onToggle: metaStringOrProp(meta, "onToggle", args, ["on_toggle", "onToggle", "toggle"]),
    onClose: metaStringOrProp(meta, "onClose", args, ["on_close", "onClose", "close"]),
    onCancel: metaStringOrProp(meta, "onCancel", args, ["on_cancel", "onCancel", "cancel"]),
    onFocus: metaStringOrProp(meta, "onFocus", args, ["on_focus", "onFocus", "focus"]),
    onBlur: metaStringOrProp(meta, "onBlur", args, ["on_blur", "onBlur", "blur"]),
    onScroll: metaStringOrProp(meta, "onScroll", args, ["on_scroll", "onScroll", "scroll"]),
    onMouseEnter: metaStringOrProp(meta, "onMouseEnter", args, ["on_mouse_enter", "onMouseEnter", "mouseenter"]),
    onMouseLeave: metaStringOrProp(meta, "onMouseLeave", args, ["on_mouse_leave", "onMouseLeave", "mouseleave"]),
    onMouseMove: metaStringOrProp(meta, "onMouseMove", args, ["on_mouse_move", "onMouseMove", "mousemove"]),
    onMouseDown: metaStringOrProp(meta, "onMouseDown", args, ["on_mouse_down", "onMouseDown", "mousedown"]),
    onMouseUp: metaStringOrProp(meta, "onMouseUp", args, ["on_mouse_up", "onMouseUp", "mouseup"]),
    onPointerEnter: metaStringOrProp(meta, "onPointerEnter", args,
      ["on_pointer_enter", "onPointerEnter", "pointerenter"]),
    onPointerLeave: metaStringOrProp(meta, "onPointerLeave", args,
      ["on_pointer_leave", "onPointerLeave", "pointerleave"]),
    onPointerMove: metaStringOrProp(meta, "onPointerMove", args,
      ["on_pointer_move", "onPointerMove", "pointermove"]),
    onPointerDown: metaStringOrProp(meta, "onPointerDown", args,
      ["on_pointer_down", "onPointerDown", "pointerdown"]),
    onPointerUp: metaStringOrProp(meta, "onPointerUp", args, ["on_pointer_up", "onPointerUp", "pointerup"]),
    onPointerCancel: metaStringOrProp(meta, "onPointerCancel", args,
      ["on_pointer_cancel", "onPointerCancel", "pointercancel"]),
    onWheel: metaStringOrProp(meta, "onWheel", args, ["on_wheel", "onWheel", "wheel"]),
    onContextMenu: metaStringOrProp(meta, "onContextMenu", args,
      ["on_context_menu", "onContextMenu", "contextmenu"]),
    onDragStart: metaStringOrProp(meta, "onDragStart", args,
      ["on_drag_start", "on_dragstart", "onDragStart", "dragstart"]),
    onDragEnd: metaStringOrProp(meta, "onDragEnd", args,
      ["on_drag_end", "on_dragend", "onDragEnd", "dragend"]),
    onDragOver: metaStringOrProp(meta, "onDragOver", args,
      ["on_drag_over", "on_dragover", "onDragOver", "dragover"]),
    onDrop: metaStringOrProp(meta, "onDrop", args, ["on_drop", "onDrop", "drop"]),
    onCopy: metaStringOrProp(meta, "onCopy", args, ["on_copy", "onCopy", "copy"]),
    onCut: metaStringOrProp(meta, "onCut", args, ["on_cut", "onCut", "cut"]),
    onPaste: metaStringOrProp(meta, "onPaste", args, ["on_paste", "onPaste", "paste"]),
    action: typeof meta.action === "function" ? meta.action : null,
    doubleClickAction: typeof meta.doubleClickAction === "function" ? meta.doubleClickAction : null,
    inputAction: typeof meta.inputAction === "function" ? meta.inputAction : null,
    beforeInputAction: typeof meta.beforeInputAction === "function" ? meta.beforeInputAction : null,
    changeAction: typeof meta.changeAction === "function" ? meta.changeAction : null,
    selectAction: typeof meta.selectAction === "function" ? meta.selectAction : null,
    keyAction: typeof meta.keyAction === "function" ? meta.keyAction : null,
    keyUpAction: typeof meta.keyUpAction === "function" ? meta.keyUpAction : null,
    invalidAction: typeof meta.invalidAction === "function" ? meta.invalidAction : null,
    submitAction: typeof meta.submitAction === "function" ? meta.submitAction : null,
    resetAction: typeof meta.resetAction === "function" ? meta.resetAction : null,
    toggleAction: typeof meta.toggleAction === "function" ? meta.toggleAction : null,
    closeAction: typeof meta.closeAction === "function" ? meta.closeAction : null,
    cancelAction: typeof meta.cancelAction === "function" ? meta.cancelAction : null,
    focusAction: typeof meta.focusAction === "function" ? meta.focusAction : null,
    blurAction: typeof meta.blurAction === "function" ? meta.blurAction : null,
    scrollAction: typeof meta.scrollAction === "function" ? meta.scrollAction : null,
    mouseEnterAction: typeof meta.mouseEnterAction === "function" ? meta.mouseEnterAction : null,
    mouseLeaveAction: typeof meta.mouseLeaveAction === "function" ? meta.mouseLeaveAction : null,
    mouseMoveAction: typeof meta.mouseMoveAction === "function" ? meta.mouseMoveAction : null,
    mouseDownAction: typeof meta.mouseDownAction === "function" ? meta.mouseDownAction : null,
    mouseUpAction: typeof meta.mouseUpAction === "function" ? meta.mouseUpAction : null,
    pointerEnterAction: typeof meta.pointerEnterAction === "function" ? meta.pointerEnterAction : null,
    pointerLeaveAction: typeof meta.pointerLeaveAction === "function" ? meta.pointerLeaveAction : null,
    pointerMoveAction: typeof meta.pointerMoveAction === "function" ? meta.pointerMoveAction : null,
    pointerDownAction: typeof meta.pointerDownAction === "function" ? meta.pointerDownAction : null,
    pointerUpAction: typeof meta.pointerUpAction === "function" ? meta.pointerUpAction : null,
    pointerCancelAction: typeof meta.pointerCancelAction === "function" ? meta.pointerCancelAction : null,
    wheelAction: typeof meta.wheelAction === "function" ? meta.wheelAction : null,
    contextMenuAction: typeof meta.contextMenuAction === "function" ? meta.contextMenuAction : null,
    dragStartAction: typeof meta.dragStartAction === "function" ? meta.dragStartAction : null,
    dragEndAction: typeof meta.dragEndAction === "function" ? meta.dragEndAction : null,
    dragOverAction: typeof meta.dragOverAction === "function" ? meta.dragOverAction : null,
    dropAction: typeof meta.dropAction === "function" ? meta.dropAction : null,
    copyAction: typeof meta.copyAction === "function" ? meta.copyAction : null,
    cutAction: typeof meta.cutAction === "function" ? meta.cutAction : null,
    pasteAction: typeof meta.pasteAction === "function" ? meta.pasteAction : null,
    pageTitle: propString(args, "title", ""),
    pageDescription: propString(args, "description", ""),
    pageCanonicalURL: propString(args, "canonical_url", ""),
    bounds,
    hasBounds: bounds.width > 0 || bounds.height > 0,
    scrollLeft: 0,
    scrollTop: 0,
    state
  };
  Object.defineProperty(node, "__kryAuthoredTag", {
    configurable: true,
    enumerable: false,
    value: authoredTag
  });
  node.styleFacts = webNodeStyleFacts(node);
  return node;
}

export function webNodeStyleFacts(node) {
  return {
    index: node?.index || 0,
    kind: node?.kind || "",
    tag: node?.tag || "",
    key: node?.key || "",
    name: node?.name || "",
    path: node?.path || "",
    parentPath: node?.parentPath || "",
    ref: webNodeRef(node),
    webRef: node?.webRef || "",
    sourcePath: node?.sourcePath || "",
    sourceLine: node?.sourceLine || 0,
    sourceColumn: node?.sourceColumn || 0,
    sourceEndLine: node?.sourceEndLine || 0,
    sourceEndColumn: node?.sourceEndColumn || 0,
    sourceRef: webNodeSourceRef(node),
    sourceColumnRef: webNodeSourceColumnRef(node),
    sourceRangeRef: webNodeSourceRangeRef(node),
    id: node?.domId || "",
    domName: node?.domName || "",
    title: node?.title || "",
    lang: node?.lang || "",
    dir: node?.dir || "",
    translate: node?.translate || "",
    dirname: node?.dirname || "",
    placeholder: node?.placeholder || "",
    tabIndex: Number.isFinite(Number(node?.tabIndex)) ? Math.trunc(Number(node.tabIndex)) : null,
    domValue: node?.domValue || "",
    href: node?.href || "",
    target: node?.target || "",
    rel: node?.rel || "",
    alt: node?.alt || "",
    asset: node?.asset || "",
    src: node?.asset || "",
    htmlFor: node?.htmlFor || "",
    dataList: node?.dataList || "",
    useMap: node?.useMap || "",
    part: node?.part || "",
    slot: node?.slot || "",
    inputType: node?.inputType || "",
    formOwner: node?.formOwner || "",
    formAction: node?.formAction || "",
    formMethod: node?.formMethod || "",
    formEncType: node?.formEncType || "",
    autoComplete: node?.autoComplete || "",
    hidden: !!node?.hidden,
    draggable: node?.draggable || "",
    spellCheck: node?.spellCheck || "",
    contentEditable: node?.contentEditable || "",
    autoFocus: !!node?.autoFocus,
    inert: !!node?.inert,
    autoCapitalize: node?.autoCapitalize || "",
    enterKeyHint: node?.enterKeyHint || "",
    download: node?.download || "",
    formNoValidate: !!node?.formNoValidate,
    noValidate: !!node?.noValidate,
    clickable: !!node?.clickable,
    popover: node?.popover || "",
    popoverTarget: node?.popoverTarget || "",
    popoverTargetAction: node?.popoverTargetAction || "",
    open: !!node?.state?.open,
    scrollLeft: Number.isFinite(Number(node?.scrollLeft)) ? Number(node.scrollLeft) : 0,
    scrollTop: Number.isFinite(Number(node?.scrollTop)) ? Number(node.scrollTop) : 0,
    readOnly: !!node?.readOnly,
    required: !!node?.required,
    min: node?.min || "",
    max: node?.max || "",
    step: node?.step || "",
    minLength: node?.minLength || "",
    maxLength: node?.maxLength || "",
    pattern: node?.pattern || "",
    accept: node?.accept || "",
    multiple: !!node?.multiple,
    inputMode: node?.inputMode || "",
    headers: node?.headers || "",
    scope: node?.scope || "",
    colSpan: node?.colSpan || "",
    rowSpan: node?.rowSpan || "",
    classes: [...(node?.classes || [])],
    dataAttrs: { ...(node?.dataAttrs || {}) },
    ariaAttrs: { ...(node?.ariaAttrs || {}) },
    extraAttrs: { ...(node?.extraAttrs || {}) },
    role: node?.role || "",
    ariaLabel: node?.ariaLabel || "",
    ariaDescription: node?.ariaDescription || "",
    ariaDescribedBy: node?.ariaDescribedBy || "",
    ariaDetails: node?.ariaDetails || "",
    ariaErrorMessage: node?.ariaErrorMessage || "",
    ariaFlowTo: node?.ariaFlowTo || "",
    ariaLabelledBy: node?.ariaLabelledBy || "",
    ariaActiveDescendant: node?.ariaActiveDescendant || "",
    ariaControls: node?.ariaControls || "",
    ariaOwns: node?.ariaOwns || "",
    ariaSort: node?.ariaSort || "",
    ariaOrientation: node?.ariaOrientation || "",
    ariaLevel: node?.ariaLevel || "",
    ariaPosInSet: node?.ariaPosInSet || "",
    ariaSetSize: node?.ariaSetSize || "",
    ariaHasPopup: node?.ariaHasPopup || "",
    ariaMultiSelectable: node?.ariaMultiSelectable || "",
    ariaRowIndex: node?.ariaRowIndex || "",
    ariaColIndex: node?.ariaColIndex || "",
    ariaRowCount: node?.ariaRowCount || "",
    ariaColCount: node?.ariaColCount || "",
    ariaLive: node?.ariaLive || "",
    state: { ...(node?.state || {}) }
  };
}

export function webNodeIdentity(node) {
  const ref = webNodeRef(node);
  const sourceRef = webNodeSourceRef(node);
  const sourceColumnRef = webNodeSourceColumnRef(node);
  const sourceRangeRef = webNodeSourceRangeRef(node);
  const aliases = [...new Set([
    ref,
    node?.path || "",
    node?.name || "",
    node?.key || "",
    node?.domId || "",
    node?.domName || "",
    sourceRef,
    sourceColumnRef,
    sourceRangeRef
  ].filter(Boolean))];
  return {
    ref,
    aliases,
    index: node?.index || 0,
    kind: node?.kind || "",
    tag: node?.tag || "",
    key: node?.key || "",
    name: node?.name || "",
    path: node?.path || "",
    parentPath: node?.parentPath || "",
    webRef: node?.webRef || "",
    domId: node?.domId || "",
    domName: node?.domName || "",
    sourcePath: node?.sourcePath || "",
    sourceLine: node?.sourceLine || 0,
    sourceColumn: node?.sourceColumn || 0,
    sourceEndLine: node?.sourceEndLine || 0,
    sourceEndColumn: node?.sourceEndColumn || 0,
    sourceRef,
    sourceColumnRef,
    sourceRangeRef
  };
}

export function webAccessibilitySnapshot(source) {
  const frame = source?.nodes ? source : webDocumentFrame(source);
  return {
    title: frame.metadata?.title || "",
    description: frame.metadata?.description || "",
    nodes: (frame.nodes || []).map(webAccessibilityNodeFromNode)
  };
}

function webAccessibilityNodeFromNode(node) {
  const range = webNodeValueRange(node);
  const label = node.ariaLabel || (node.tag === "img" ? node.alt : "") ||
    node.text || node.name;
  return {
    path: node.path,
    sourcePath: node.sourcePath,
    sourceLine: node.sourceLine,
    sourceColumn: node.sourceColumn,
    sourceEndLine: node.sourceEndLine,
    sourceEndColumn: node.sourceEndColumn,
    name: node.name,
    kind: node.kind,
    tag: node.tag,
    id: node.domId,
    classes: [...node.classes],
    role: node.role || implicitRole(node),
    label,
    description: node.ariaDescription,
    text: node.text,
    value: node.tag === "progress" ? node.domValue
      : node.tag === "input" || node.tag === "textarea" ? node.value : "",
    min: range.min,
    max: range.max,
    valueNow: range.valueNow,
    href: node.href,
    alt: node.alt || "",
    asset: node.asset || "",
    src: node.asset || "",
    inputType: node.inputType,
    level: node.level || 0,
    rowIndex: node.ariaRowIndex,
    colIndex: node.ariaColIndex,
    rowCount: node.ariaRowCount,
    colCount: node.ariaColCount,
    state: { ...node.state }
  };
}

function webAccessibilityNodeFromDOMSnapshot(snapshot) {
  const facts = snapshot?.styleFacts || {};
  const label = facts.ariaLabel || (snapshot?.tag === "img" ? snapshot?.alt : "") ||
    snapshot?.text || snapshot?.name || "";
  return {
    path: snapshot?.path || "",
    sourcePath: snapshot?.sourcePath || "",
    sourceLine: snapshot?.sourceLine || 0,
    sourceColumn: snapshot?.sourceColumn || 0,
    sourceEndLine: snapshot?.sourceEndLine || 0,
    sourceEndColumn: snapshot?.sourceEndColumn || 0,
    name: snapshot?.name || "",
    kind: snapshot?.kind || "",
    tag: snapshot?.tag || "",
    id: snapshot?.id || "",
    classes: [...(snapshot?.classes || [])],
    role: snapshot?.role || "",
    label,
    description: facts.ariaDescription || "",
    text: snapshot?.text || "",
    value: snapshot?.value ?? "",
    min: snapshot?.min || "",
    max: snapshot?.max || "",
    valueNow: snapshot?.valueNow || "",
    href: facts.href || "",
    alt: snapshot?.alt || facts.alt || "",
    asset: snapshot?.asset || facts.asset || "",
    src: snapshot?.src || facts.src || "",
    inputType: facts.inputType || "",
    level: Number(facts.ariaLevel || 0) || 0,
    rowIndex: facts.ariaRowIndex || "",
    colIndex: facts.ariaColIndex || "",
    rowCount: facts.ariaRowCount || "",
    colCount: facts.ariaColCount || "",
    state: { ...(snapshot?.state || {}) }
  };
}

function webNodeValueRange(node) {
  const rangeLike = node?.tag === "progress" || node?.tag === "meter" ||
    (node?.tag === "input" && (node?.inputType === "range" || node?.inputType === "number"));
  if (!rangeLike)
    return { min: "", max: "", valueNow: "" };
  return {
    min: node?.min || "",
    max: node?.max || "",
    valueNow: node?.domValue || (node?.value === undefined || node?.value === null ? "" : String(node.value))
  };
}

function implicitRole(node) {
  if (!node)
    return "";
  if (node.tag === "button")
    return "button";
  if (node.tag === "a" && node.href)
    return "link";
  if (node.tag === "input") {
    if (node.kind === "Toggle")
      return "switch";
    if (node.inputType === "checkbox")
      return "checkbox";
    if (node.inputType === "radio")
      return "radio";
    if (node.inputType === "color")
      return "";
    if (node.inputType === "range")
      return "slider";
    if (node.inputType === "number")
      return "spinbutton";
    return "textbox";
  }
  if (node.tag === "select")
    return node.kind === "ListBox" ? "listbox" : "combobox";
  if (node.tag === "datalist")
    return "listbox";
  if (node.tag === "textarea")
    return "textbox";
  if (node.tag === "progress")
    return "progressbar";
  if (node.tag === "meter")
    return "meter";
  if (node.tag === "output")
    return "status";
  if (node.tag === "summary")
    return "button";
  if (node.tag === "form")
    return "form";
  if (node.tag === "hr")
    return "separator";
  if (node.tag === "li")
    return "listitem";
  if (node.tag === "optgroup")
    return "group";
  if (node.tag === "option")
    return "option";
  if (node.tag === "table")
    return "table";
  if (node.tag === "thead" || node.tag === "tbody" || node.tag === "tfoot")
    return "rowgroup";
  if (node.tag === "tr")
    return "row";
  if (node.tag === "th") {
    const scope = String(node.scope || "").toLowerCase();
    if (scope === "row" || scope === "rowgroup")
      return "rowheader";
    if (scope === "col" || scope === "colgroup")
      return "columnheader";
  }
  if (/^h[1-6]$/.test(node.tag))
    return "heading";
  if (node.tag === "main")
    return "main";
  if (node.tag === "nav")
    return "navigation";
  if (node.tag === "search")
    return "search";
  if (node.tag === "aside")
    return "complementary";
  if (node.tag === "header")
    return "banner";
  if (node.tag === "footer")
    return "contentinfo";
  if (node.tag === "figure")
    return "figure";
  if (node.tag === "ul" || node.tag === "ol")
    return "list";
  if (node.tag === "blockquote")
    return "blockquote";
  if (node.tag === "fieldset" || node.tag === "details")
    return "group";
  if (node.tag === "dialog")
    return "dialog";
  switch (node.kind) {
  case "Toolbar":
    return "toolbar";
  case "SegmentedControl":
    return "group";
  case "TabBar":
    return "tablist";
  case "TreeView":
    return "tree";
  case "Menu":
    return "menu";
  case "Toast":
    return "status";
  case "Icon":
  case "Plot":
  case "CanvasGrid":
    return "img";
  default:
    break;
  }
  return "";
}

function webDOMRole(node) {
  const role = node?.role || "";
  if (role)
    return role;
  const implicit = implicitRole(node);
  return node?.tag === "div" || node?.tag === "canvas" || node?.tag === "menu" ||
      (node?.tag === "span" && node?.kind === "Icon")
    ? implicit : "";
}

const webStyleLayers = {
  reset: 0,
  base: 0,
  defaults: 0,
  components: 1,
  widgets: 1,
  app: 2,
  overrides: 3
};

const webKssColorProperties = new Set([
  "background", "foreground", "border", "focus", "background-end",
  "color", "background-color",
  "accent-color", "caret-color", "border-color", "border-top-color",
  "border-right-color", "border-bottom-color", "border-left-color",
  "border-inline-color", "border-block-color", "border-inline-start-color",
  "border-inline-end-color", "border-block-start-color", "border-block-end-color",
  "outline-color", "text-decoration-color", "text-emphasis-color",
  "column-rule-color"
]);

const webKssLengthProperties = new Set([
  "radius", "border-radius", "border-width", "opacity",
  "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
  "border-inline-width", "border-block-width", "border-inline-start-width",
  "border-inline-end-width", "border-block-start-width", "border-block-end-width",
  "border-top-left-radius", "border-top-right-radius",
  "border-bottom-right-radius", "border-bottom-left-radius",
  "border-start-start-radius", "border-start-end-radius",
  "border-end-start-radius", "border-end-end-radius",
  "padding", "padding-x", "padding-y",
  "padding-left", "padding-right", "padding-top", "padding-bottom",
  "padding-inline", "padding-block",
  "padding-inline-start", "padding-inline-end",
  "padding-block-start", "padding-block-end",
  "margin", "margin-x", "margin-y",
  "margin-left", "margin-right", "margin-top", "margin-bottom",
  "margin-inline", "margin-block",
  "margin-inline-start", "margin-inline-end",
  "margin-block-start", "margin-block-end",
  "width", "height", "min-width", "max-width", "min-height", "max-height",
  "inline-size", "block-size", "min-inline-size", "max-inline-size",
  "min-block-size", "max-block-size",
  "inset", "top", "right", "bottom", "left",
  "inset-inline", "inset-block",
  "inset-inline-start", "inset-inline-end",
  "inset-block-start", "inset-block-end",
  "gap", "row-gap", "column-gap", "font-size", "letter-spacing", "line-height",
  "flex-basis",
  "text-indent", "text-decoration-thickness", "text-underline-offset",
  "vertical-align", "perspective", "offset-distance",
  "outline-width", "outline-offset", "tab-size",
  "column-count", "column-width", "column-rule-width", "border-spacing",
  "contain-intrinsic-size", "contain-intrinsic-width",
  "contain-intrinsic-height", "contain-intrinsic-inline-size",
  "contain-intrinsic-block-size", "overflow-clip-margin",
  "shape-margin",
  "scroll-margin", "scroll-margin-top", "scroll-margin-right",
  "scroll-margin-bottom", "scroll-margin-left",
  "scroll-margin-inline", "scroll-margin-block",
  "scroll-margin-inline-start", "scroll-margin-inline-end",
  "scroll-margin-block-start", "scroll-margin-block-end",
  "scroll-padding", "scroll-padding-top", "scroll-padding-right",
  "scroll-padding-bottom", "scroll-padding-left",
  "scroll-padding-inline", "scroll-padding-block",
  "scroll-padding-inline-start", "scroll-padding-inline-end",
  "scroll-padding-block-start", "scroll-padding-block-end",
  "icon-size", "offset-x", "offset-y", "content-offset-x", "content-offset-y"
]);

const webKssMaterialProperties = new Set(["material"]);

const webKssLiteralProperties = new Set([
  "font", "typeface", "font-family", "font-weight", "font-style", "font-variant", "font-stretch",
  "font-kerning", "font-optical-sizing", "font-feature-settings",
  "font-variation-settings", "font-size-adjust", "font-synthesis",
  "font-synthesis-weight", "font-synthesis-style",
  "font-synthesis-small-caps", "font-synthesis-position",
  "font-variant-alternates", "font-variant-caps", "font-variant-east-asian",
  "font-variant-ligatures", "font-variant-numeric",
  "font-variant-position", "font-language-override", "font-palette",
  "text-align", "text-align-last", "text-rendering",
  "text-decoration", "text-decoration-line", "text-decoration-style",
  "text-decoration-skip", "text-decoration-skip-ink",
  "text-underline-position", "text-shadow", "text-emphasis",
  "text-emphasis-style", "text-emphasis-position",
  "text-transform", "text-overflow", "white-space",
  "text-size-adjust", "text-orientation", "text-wrap",
  "text-wrap-mode", "text-wrap-style", "text-justify", "line-break",
  "hanging-punctuation",
  "text-combine-upright", "ruby-align", "ruby-position",
  "text-spacing-trim", "text-autospace", "text-box-trim", "text-box-edge",
  "word-break", "overflow-wrap", "word-wrap", "display", "position", "z-index",
  "overflow", "overflow-inline", "overflow-block",
  "border-top", "border-right", "border-bottom", "border-left",
  "border-inline", "border-block", "border-inline-start",
  "border-inline-end", "border-block-start", "border-block-end",
  "border-style", "border-top-style", "border-right-style",
  "border-bottom-style", "border-left-style", "border-inline-style",
  "border-block-style", "border-inline-start-style",
  "border-inline-end-style", "border-block-start-style", "border-block-end-style",
  "border-image", "border-image-source", "border-image-slice",
  "border-image-width", "border-image-outset", "border-image-repeat",
  "overflow-x", "overflow-y", "box-sizing", "direction", "writing-mode",
  "hyphens", "line-clamp", "list-style", "list-style-type",
  "list-style-position", "list-style-image", "counter-reset",
  "counter-increment", "counter-set", "quotes", "marker-side",
  "marker-start", "marker-end", "orphans", "widows",
  "box-decoration-break",
  "border-collapse", "table-layout", "caption-side", "empty-cells",
  "scroll-behavior", "overscroll-behavior", "overscroll-behavior-x",
  "overscroll-behavior-y", "overscroll-behavior-inline",
  "overscroll-behavior-block", "scroll-snap-type", "scroll-snap-align",
  "scroll-snap-stop", "scrollbar-color", "scrollbar-width",
  "scrollbar-gutter", "touch-action",
  "align-items", "justify-content", "align-self", "justify-self",
  "flex-direction", "flex-wrap", "flex-flow", "flex", "flex-grow", "flex-shrink",
  "grid", "grid-template", "grid-template-columns", "grid-template-rows", "grid-template-areas",
  "grid-auto-columns", "grid-auto-rows", "grid-auto-flow",
  "grid-column", "grid-column-start", "grid-column-end", "grid-area",
  "grid-row", "grid-row-start", "grid-row-end",
  "align-content", "justify-items", "place-items", "place-content", "place-self",
  "align-tracks", "justify-tracks",
  "object-fit", "object-position", "object-view-box", "aspect-ratio",
  "image-rendering", "image-orientation", "image-resolution",
  "background-image", "background-size", "background-position",
  "background-position-x", "background-position-y",
  "background-repeat", "background-repeat-x", "background-repeat-y",
  "background-clip", "background-origin",
  "background-attachment", "background-blend-mode", "visibility",
  "transition", "transition-property", "transition-duration",
  "transition-timing-function", "transition-delay", "transition-behavior",
  "animation", "animation-name", "animation-duration",
  "animation-timing-function", "animation-delay",
  "animation-iteration-count", "animation-direction",
  "animation-fill-mode", "animation-play-state",
  "animation-composition", "animation-timeline", "animation-range", "animation-range-start",
  "animation-range-end", "scroll-timeline", "scroll-timeline-name",
  "scroll-timeline-axis", "view-timeline", "view-timeline-name",
  "view-timeline-axis", "view-timeline-inset", "timeline-scope",
  "transform", "transform-origin", "transform-box", "transform-style",
  "translate", "rotate", "scale", "perspective-origin", "backface-visibility",
  "offset-path", "offset-rotate", "offset-anchor", "offset-position",
  "filter", "backdrop-filter", "clip-path",
  "mask", "mask-image", "mask-size", "mask-position", "mask-repeat",
  "mask-origin", "mask-clip", "mask-composite", "mask-mode",
  "cursor", "pointer-events", "appearance", "user-select", "resize",
  "outline", "outline-style", "box-shadow", "color-scheme",
  "field-sizing", "interpolate-size", "overlay", "forced-color-adjust",
  "print-color-adjust", "color-interpolation", "color-interpolation-filters",
  "paint-order", "shape-outside", "shape-image-threshold",
  "contain", "content-visibility", "contain-intrinsic-size",
  "contain-intrinsic-width", "contain-intrinsic-height",
  "contain-intrinsic-inline-size", "contain-intrinsic-block-size",
  "container", "container-type", "container-name", "will-change",
  "anchor-name", "position-anchor", "position-area", "position-try",
  "position-try-fallbacks", "position-try-order", "position-visibility",
  "view-transition-name", "isolation", "mix-blend-mode", "columns",
  "column-fill", "column-span", "column-rule", "column-rule-style",
  "break-before", "break-after", "break-inside", "float", "clear",
  "order"
]);

function stripKssComments(source) {
  return String(source || "")
    .replace(/\/\*[\s\S]*?\*\//g, "")
    .replace(/\/\/.*$/gm, "");
}

function splitSelectorSequence(text) {
  const parts = [];
  let token = "";
  let bracketDepth = 0;
  let parenDepth = 0;
  let quote = "";
  let pendingCombinator = "";
  const push = () => {
    const value = token.trim();
    if (!value)
      return;
    parts.push({ combinator: pendingCombinator || (parts.length ? " " : ""), text: value });
    token = "";
    pendingCombinator = "";
  };
  for (const ch of String(text || "")) {
    if (quote) {
      token += ch;
      if (ch === quote)
        quote = "";
      continue;
    }
    if (ch === "\"" || ch === "'") {
      token += ch;
      quote = ch;
      continue;
    }
    if (ch === "[") {
      bracketDepth++;
      token += ch;
      continue;
    }
    if (ch === "]" && bracketDepth > 0) {
      bracketDepth--;
      token += ch;
      continue;
    }
    if (!bracketDepth && ch === "(") {
      parenDepth++;
      token += ch;
      continue;
    }
    if (!bracketDepth && ch === ")" && parenDepth > 0) {
      parenDepth--;
      token += ch;
      continue;
    }
    if (!bracketDepth && !parenDepth && (ch === ">" || ch === "+" || ch === "~")) {
      push();
      pendingCombinator = ch;
      continue;
    }
    if (!bracketDepth && !parenDepth && /\s/.test(ch)) {
      push();
      if (!pendingCombinator)
        pendingCombinator = " ";
      continue;
    }
    token += ch;
  }
  push();
  return parts;
}

function splitSelectorList(text) {
  const parts = [];
  let token = "";
  let bracketDepth = 0;
  let parenDepth = 0;
  let quote = "";
  const push = () => {
    const value = token.trim();
    if (value)
      parts.push(value);
    token = "";
  };
  for (const ch of String(text || "")) {
    if (quote) {
      token += ch;
      if (ch === quote)
        quote = "";
      continue;
    }
    if (ch === "\"" || ch === "'") {
      token += ch;
      quote = ch;
      continue;
    }
    if (ch === "[") {
      bracketDepth++;
      token += ch;
      continue;
    }
    if (ch === "]" && bracketDepth > 0) {
      bracketDepth--;
      token += ch;
      continue;
    }
    if (!bracketDepth && ch === "(") {
      parenDepth++;
      token += ch;
      continue;
    }
    if (!bracketDepth && ch === ")" && parenDepth > 0) {
      parenDepth--;
      token += ch;
      continue;
    }
    if (!bracketDepth && !parenDepth && ch === ",") {
      push();
      continue;
    }
    token += ch;
  }
  push();
  return parts;
}

function parseSimpleSelector(text) {
  let source = String(text || "").trim();
  const selector = {
    kind: "*",
    id: "",
    classes: [],
    attrs: {},
    attrOps: {},
    pseudos: [],
    not: [],
    matches: [],
    state: "",
    states: [],
    specificity: 0
  };
  source = source.replace(/\[([A-Za-z_][\w.-]*)\s*([~|^$*]?=)\s*([^\]]+)\]/g, (_all, key, op, value) => {
    selector.attrs[key] = String(value).trim().replace(/^["']|["']$/g, "");
    selector.attrOps[key] = op || "=";
    selector.specificity += 10;
    return "";
  });
  source = source.replace(/:([A-Za-z_][\w-]*)(?:\(\s*([^)]*?)\s*\))?/g, (_all, name, arg) => {
    const pseudo = name.replace(/_/g, "-").toLowerCase();
    if (arg !== undefined && (pseudo === "not" || pseudo === "is" || pseudo === "where")) {
      const selectors = splitSelectorList(arg).map(parseSimpleSelector);
      if (pseudo === "not")
        selector.not.push(...selectors);
      else
        selector.matches.push(...selectors);
    } else if (arg === undefined && (pseudo === "hover" || pseudo === "pressed" || pseudo === "active" ||
        pseudo === "focus" || pseudo === "focused" || pseudo === "focus-visible" ||
        pseudo === "normal" || pseudo === "disabled" ||
        pseudo === "loading" || pseudo === "selected" || pseudo === "checked" ||
        pseudo === "invalid" || pseudo === "valid" || pseudo === "indeterminate" ||
        pseudo === "default" || pseudo === "autofill" || pseudo === "placeholder-shown" ||
        pseudo === "expanded" || pseudo === "open" ||
        pseudo === "readonly" || pseudo === "read-only" || pseudo === "required" ||
        pseudo === "enabled" || pseudo === "optional")) {
      const state = pseudo === "active" ? "pressed" :
        (pseudo === "focused" || pseudo === "focus-visible" ? "focus" :
        (pseudo === "read-only" ? "readonly" : pseudo));
      selector.state = state;
      if (!selector.states.includes(state))
        selector.states.push(state);
    } else
      selector.pseudos.push(arg === undefined ? pseudo : `${pseudo}(${String(arg).trim()})`);
    selector.specificity += 10;
    return "";
  });
  source = source.replace(/\[([A-Za-z_][\w.-]*)\]/g, (_all, key) => {
    selector.attrs[key] = null;
    selector.specificity += 10;
    return "";
  });
  source = source.replace(/#([A-Za-z_][\w-]*)/g, (_all, id) => {
    selector.id = id;
    selector.specificity += 100;
    return "";
  });
  source = source.replace(/\.([A-Za-z_][\w-]*)/g, (_all, name) => {
    selector.classes.push(name);
    selector.specificity += 20;
    return "";
  });
  source = source.trim();
  if (source)
    selector.kind = source;
  if (selector.kind !== "*")
    selector.specificity += 1;
  return selector;
}

function parseSelector(text) {
  const parts = splitSelectorSequence(text);
  if (parts.length <= 1)
    return parseSimpleSelector(text);
  const selectors = parts.map((part) => ({
    ...parseSimpleSelector(part.text),
    combinator: part.combinator
  }));
  return {
    kind: selectors[selectors.length - 1]?.kind || "*",
    id: "",
    classes: [],
    attrs: {},
    attrOps: {},
    pseudos: [],
    not: [],
    matches: [],
    state: "",
    states: [],
    specificity: selectors.reduce((sum, selector) => sum + selector.specificity, 0),
    parts: selectors
  };
}

function parseKssValue(value) {
  const text = String(value || "").trim();
  const number = Number(text.replace(/px$/, ""));
  if (Number.isFinite(number) && /^-?\d+(?:\.\d+)?(?:px)?$/.test(text))
    return number;
  return text;
}

function parseKssDurationValue(value) {
  const text = String(value || "").trim();
  const match = text.match(/^(-?\d+(?:\.\d+)?)(ms|s)?$/i);
  if (!match)
    throw new Error(`expected token duration ${text}`);
  const number = Number(match[1]);
  return match[2]?.toLowerCase() === "s" ? number * 1000 : number;
}

function parseKssDeclarationValue(name, value, tokens) {
  const parsed = parseKssValue(value);
  if (typeof parsed !== "string")
    return parsed;
  const key = parsed.trim();
  const property = String(name || "").toLowerCase();
  if (webKssColorProperties.has(property))
    return tokens.colors.get(key) ?? parsed;
  if (webKssLengthProperties.has(property))
    return tokens.lengths.get(key) ?? parsed;
  if (webKssMaterialProperties.has(property))
    return tokens.materials.get(key) ?? parsed;
  if (property.startsWith("--"))
    return parsed;
  if (!webKssLiteralProperties.has(property))
    throw new Error(`unknown KSS property ${name}`);
  return parsed;
}

function parseKssDeclarations(body, tokens = emptyWebStyleTokens()) {
  const style = {};
  for (const part of String(body || "").split(";")) {
    const colon = part.indexOf(":");
    if (colon < 0)
      continue;
    const name = part.slice(0, colon).trim();
    if (!name)
      continue;
    style[name] = parseKssDeclarationValue(name, part.slice(colon + 1), tokens);
  }
  return style;
}

function emptyWebStyleTokens() {
  return { colors: new Map(), lengths: new Map(), materials: new Map() };
}

function findMatchingBrace(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === "{")
      depth++;
    else if (text[i] === "}") {
      depth--;
      if (depth === 0)
        return i;
    }
  }
  return -1;
}

function parseWebStyleTokens(text) {
  const tokens = emptyWebStyleTokens();
  let stripped = "";
  let cursor = 0;
  const tokenPattern = /\btokens\s*\{/g;
  for (let match; (match = tokenPattern.exec(text));) {
    const open = tokenPattern.lastIndex - 1;
    const close = findMatchingBrace(text, open);
    if (close < 0)
      break;
    stripped += text.slice(cursor, match.index);
    const body = text.slice(open + 1, close);
    const groupPattern = /([A-Za-z_][\w-]*)\s*\{([^{}]*)\}/g;
    for (let group; (group = groupPattern.exec(body));) {
      const kind = group[1].toLowerCase();
      const target = kind === "color" ? tokens.colors :
        (kind === "length" || kind === "number" || kind === "duration") ? tokens.lengths :
        kind === "material" ? tokens.materials : null;
      if (!target)
        throw new Error(`unknown KSS token group ${group[1]}`);
      for (const part of group[2].split(";")) {
        const colon = part.indexOf(":");
        if (colon < 0)
          continue;
        const name = part.slice(0, colon).trim();
        if (!name)
          continue;
        target.set(name, kind === "duration"
          ? parseKssDurationValue(part.slice(colon + 1))
          : parseKssValue(part.slice(colon + 1)));
      }
    }
    cursor = close + 1;
    tokenPattern.lastIndex = close + 1;
  }
  stripped += text.slice(cursor);
  return { text: stripped, tokens };
}

function parseWebStyleKeyframes(text, tokens) {
  const keyframes = [];
  let stripped = "";
  let cursor = 0;
  const keyframePattern = /@keyframes\s+([A-Za-z_][\w-]*)\s*\{/g;
  for (let match; (match = keyframePattern.exec(text));) {
    const open = keyframePattern.lastIndex - 1;
    const close = findMatchingBrace(text, open);
    if (close < 0)
      break;
    stripped += text.slice(cursor, match.index);
    const body = text.slice(open + 1, close);
    const frames = [];
    const framePattern = /([^{}]+)\{([^{}]*)\}/g;
    for (let frame; (frame = framePattern.exec(body));) {
      const selector = splitSelectorList(frame[1]).join(", ");
      if (!selector)
        continue;
      frames.push({
        selector,
        style: parseKssDeclarations(frame[2], tokens)
      });
    }
    keyframes.push({ name: match[1], frames });
    cursor = close + 1;
    keyframePattern.lastIndex = close + 1;
  }
  stripped += text.slice(cursor);
  return { text: stripped, keyframes };
}

function parseWebStyleRuleItems(text, tokens, initialLayer = 0) {
  const rules = [];
  let layer = initialLayer;
  const itemPattern = /@([A-Za-z_][\w-]*)\s+([^;{}]+);|([^@{}]+)\{([^{}]*)\}/g;
  for (let match; (match = itemPattern.exec(text));) {
    if (match[1]) {
      const name = match[1].toLowerCase();
      const value = match[2].trim();
      if (name === "layer") {
        const nextLayer = webStyleLayers[value.toLowerCase()];
        if (nextLayer === undefined)
          throw new Error(`unknown KSS layer ${value}`);
        layer = nextLayer;
      } else {
        throw new Error(`unknown KSS directive @${match[1]}`);
      }
      continue;
    }
    for (const selectorText of splitSelectorList(match[3])) {
      if (!selectorText.trim())
        continue;
      const selector = parseSelector(selectorText);
      const order = rules.length;
      rules.push({
        selector,
        style: parseKssDeclarations(match[4], tokens),
        layer,
        order,
        score: layer * 1000000 + selector.specificity * 1000 + order
      });
    }
  }
  return rules;
}

function parseWebStyleConditionalGroups(text, tokens) {
  const groups = [];
  let stripped = "";
  let cursor = 0;
  const conditionalPattern = /@(media|supports|container)\s+([^{}]+)\{/g;
  for (let match; (match = conditionalPattern.exec(text));) {
    const open = conditionalPattern.lastIndex - 1;
    const close = findMatchingBrace(text, open);
    if (close < 0)
      break;
    stripped += text.slice(cursor, match.index);
    const query = match[2].trim();
    if (query) {
      groups.push({
        kind: match[1].toLowerCase(),
        query,
        rules: parseWebStyleRuleItems(text.slice(open + 1, close), tokens)
      });
    }
    cursor = close + 1;
    conditionalPattern.lastIndex = close + 1;
  }
  stripped += text.slice(cursor);
  return { text: stripped, groups };
}

export function parseWebStyleSheet(source, colors = {}) {
  const parsedTokens = parseWebStyleTokens(stripKssComments(source));
  for (const [name, color] of Object.entries(colors)) {
    if (parsedTokens.tokens.colors.has(name))
      parsedTokens.tokens.colors.set(name, parseKssValue(color));
  }
  const parsedKeyframes = parseWebStyleKeyframes(parsedTokens.text, parsedTokens.tokens);
  const parsedGroups = parseWebStyleConditionalGroups(parsedKeyframes.text, parsedTokens.tokens);
  const text = parsedGroups.text;
  const tokens = parsedTokens.tokens;
  let pack = "";
  const rules = parseWebStyleRuleItems(text.replace(/@pack\s+([^;{}]+);/g, (_all, value) => {
    pack = value.trim();
    return "";
  }), tokens);
  const directivePattern = /@([A-Za-z_][\w-]*)\s+([^;{}]+);/g;
  for (let match; (match = directivePattern.exec(text));) {
    const name = match[1].toLowerCase();
    if (name !== "pack" && name !== "layer")
      throw new Error(`unknown KSS directive @${match[1]}`);
  }
  return {
    pack,
    rules,
    keyframes: parsedKeyframes.keyframes,
    groups: parsedGroups.groups
  };
}

function cssEscapeString(value) {
  return String(value ?? "").replace(/["\\\n\r\f]/g, (ch) => {
    switch (ch) {
      case "\"": return "\\\"";
      case "\\": return "\\\\";
      case "\n": return "\\a ";
      case "\r": return "\\d ";
      case "\f": return "\\c ";
      default: return ch;
    }
  });
}

function cssEscapeIdent(value) {
  return String(value ?? "").replace(/[^A-Za-z0-9_-]/g, (ch) =>
    "\\" + ch.charCodeAt(0).toString(16) + " ");
}

function webStyleSelectorAttrToCSS(key, value, op = "=") {
  const present = value === null || value === undefined;
  const operator = /^(?:=|~=|\|=|\^=|\$=|\*=)$/.test(op) ? op : "=";
  const attr = (name) => present
    ? `[${name}]`
    : `[${name}${operator}"${cssEscapeString(value)}"]`;
  if (key.startsWith("data."))
    return attr("data-" + key.slice(5).replace(/_/g, "-").toLowerCase());
  if (key.startsWith("aria."))
    return attr("aria-" + key.slice(5).replace(/_/g, "-").toLowerCase());
  if (key === "ref")
    return attr("data-kry-ref");
  if (key === "webRef")
    return attr("data-kry-web-ref");
  if (key === "path")
    return attr("data-kry-path");
  if (key === "parentPath")
    return attr("data-kry-parent-path");
  if (key === "kind")
    return attr("data-kry-kind");
  if (key === "key")
    return attr("data-kry-key");
  if (key === "source")
    return attr("data-kry-source");
  if (key === "line")
    return attr("data-kry-line");
  if (key === "column")
    return attr("data-kry-column");
  if (key === "sourceRef")
    return attr("data-kry-source-ref");
  if (key === "sourceColumnRef")
    return attr("data-kry-source-column-ref");
  if (key === "sourceRangeRef")
    return attr("data-kry-source-range-ref");
  if (key === "endLine" || key === "sourceEndLine")
    return attr("data-kry-end-line");
  if (key === "endColumn" || key === "sourceEndColumn")
    return attr("data-kry-end-column");
  if (key === "state")
    return present ? "[data-kry-state]" : `[data-kry-state~="${cssEscapeString(value)}"]`;
  return attr(key);
}

function webStylePseudoToCSS(pseudo) {
  const text = String(pseudo || "").trim();
  const functional = text.match(/^([A-Za-z_][\w-]*)\(([^)]*)\)$/);
  if (!functional)
    return ":" + cssEscapeIdent(text);
  const rawName = functional[1].replace(/_/g, "-").toLowerCase();
  const name = cssEscapeIdent(rawName);
  if (rawName === "has") {
    const arg = splitSelectorList(functional[2]).map((raw) => {
      let text = String(raw || "").trim();
      const relation = /^[>+~]/.test(text) ? text[0] : "";
      if (relation)
        text = text.slice(1).trim();
      return (relation ? relation + " " : "") + webStyleSelectorToCSS(parseSelector(text));
    }).filter(Boolean).join(",");
    return `:${name}(${arg})`;
  }
  const arg = functional[2].trim().replace(/[^0-9nN+\-\sA-Za-z]/g, "");
  return `:${name}(${arg})`;
}

function webStyleStateSelectorToCSS(state) {
  const text = String(state || "").trim();
  if (!text)
    return "";
  if (text === "normal")
    return ":not(:is(:hover,:focus,:active,:disabled,:checked,:invalid,:read-only,:required,[readonly],[required],[open],[selected],[aria-pressed=\"true\"],[aria-disabled=\"true\"],[aria-busy=\"true\"],[aria-checked=\"true\"],[aria-selected=\"true\"],[aria-current],[aria-invalid=\"true\"],[aria-expanded=\"true\"],[data-kry-state]))";
  const key = text === "active" ? "pressed" :
    (text === "focused" || text === "focus-visible" ? "focus" : text);
  const mirrored = `[data-kry-state~="${cssEscapeString(key)}"]`;
  const native = {
    hover: [":hover"],
    pressed: [":active", "[aria-pressed=\"true\"]"],
    focus: [":focus", ":focus-visible"],
    disabled: [":disabled", "[aria-disabled=\"true\"]"],
    enabled: [":enabled"],
    loading: ["[aria-busy=\"true\"]"],
    checked: [":checked", "[aria-checked=\"true\"]"],
    selected: [":checked", "[selected]", "[aria-selected=\"true\"]", "[aria-current]"],
    invalid: [":invalid", "[aria-invalid=\"true\"]"],
    valid: [":valid", "[aria-invalid=\"false\"]"],
    indeterminate: [":indeterminate", "[aria-checked=\"mixed\"]"],
    default: [":default"],
    autofill: [":autofill", ":-webkit-autofill"],
    "placeholder-shown": [":placeholder-shown"],
    expanded: ["[aria-expanded=\"true\"]"],
    readonly: [":read-only", "[readonly]"],
    required: [":required", "[required]"],
    optional: [":optional"],
    open: ["[open]"]
  }[key] || [];
  return native.length ? `:is(${[...native, mirrored].join(",")})` : mirrored;
}

export function webStyleSelectorToCSS(selector) {
  if (Array.isArray(selector?.parts) && selector.parts.length)
    return selector.parts.map((part, index) => {
      const prefix = index === 0 ? "" :
        (part.combinator === ">" || part.combinator === "+" || part.combinator === "~"
          ? ` ${part.combinator} ` : " ");
      return prefix + webStyleSelectorToCSS(part);
    }).join("");
  const parts = [];
  if (!selector || selector.kind === "*")
    parts.push(".kryon-node");
  else
    parts.push(`[data-kry-kind="${cssEscapeString(selector.kind)}"]`);
  if (selector.id)
    parts.push(`:is(#${cssEscapeIdent(selector.id)},[data-kry-name="${cssEscapeString(selector.id)}"],[data-kry-key="${cssEscapeString(selector.id)}"])`);
  for (const cls of selector.classes || [])
    parts.push("." + cssEscapeIdent(cls));
  for (const [key, value] of Object.entries(selector.attrs || {}))
    parts.push(webStyleSelectorAttrToCSS(key, value, selector.attrOps?.[key] || "="));
  for (const pseudo of selector.pseudos || [])
    parts.push(webStylePseudoToCSS(pseudo));
  for (const notSelector of selector.not || [])
    parts.push(`:not(${webStyleSelectorToCSS(notSelector)})`);
  if (selector.matches?.length)
    parts.push(`:is(${selector.matches.map(webStyleSelectorToCSS).join(",")})`);
  const states = selector.states?.length ? selector.states : (selector.state ? [selector.state] : []);
  for (const state of states) {
    const stateSelector = webStyleStateSelectorToCSS(state);
    if (stateSelector)
      parts.push(stateSelector);
  }
  return parts.join("");
}

const webCSSPropertyNames = new Map([
  ["foreground", "color"],
  ["background", "background"],
  ["color", "color"],
  ["background-color", "background-color"],
  ["accent-color", "accent-color"],
  ["caret-color", "caret-color"],
  ["border", "border-color"],
  ["border-color", "border-color"],
  ["border-width", "border-width"],
  ["border-top-width", "border-top-width"],
  ["border-right-width", "border-right-width"],
  ["border-bottom-width", "border-bottom-width"],
  ["border-left-width", "border-left-width"],
  ["border-top-color", "border-top-color"],
  ["border-right-color", "border-right-color"],
  ["border-bottom-color", "border-bottom-color"],
  ["border-left-color", "border-left-color"],
  ["border-inline-color", "border-inline-color"],
  ["border-block-color", "border-block-color"],
  ["border-inline-start-color", "border-inline-start-color"],
  ["border-inline-end-color", "border-inline-end-color"],
  ["border-block-start-color", "border-block-start-color"],
  ["border-block-end-color", "border-block-end-color"],
  ["border-inline-width", "border-inline-width"],
  ["border-block-width", "border-block-width"],
  ["border-inline-start-width", "border-inline-start-width"],
  ["border-inline-end-width", "border-inline-end-width"],
  ["border-block-start-width", "border-block-start-width"],
  ["border-block-end-width", "border-block-end-width"],
  ["border-top", "border-top"],
  ["border-right", "border-right"],
  ["border-bottom", "border-bottom"],
  ["border-left", "border-left"],
  ["border-inline", "border-inline"],
  ["border-block", "border-block"],
  ["border-inline-start", "border-inline-start"],
  ["border-inline-end", "border-inline-end"],
  ["border-block-start", "border-block-start"],
  ["border-block-end", "border-block-end"],
  ["border-image", "border-image"],
  ["border-image-source", "border-image-source"],
  ["border-image-slice", "border-image-slice"],
  ["border-image-width", "border-image-width"],
  ["border-image-outset", "border-image-outset"],
  ["border-image-repeat", "border-image-repeat"],
  ["border-style", "border-style"],
  ["border-top-style", "border-top-style"],
  ["border-right-style", "border-right-style"],
  ["border-bottom-style", "border-bottom-style"],
  ["border-left-style", "border-left-style"],
  ["border-inline-style", "border-inline-style"],
  ["border-block-style", "border-block-style"],
  ["border-inline-start-style", "border-inline-start-style"],
  ["border-inline-end-style", "border-inline-end-style"],
  ["border-block-start-style", "border-block-start-style"],
  ["border-block-end-style", "border-block-end-style"],
  ["radius", "border-radius"],
  ["border-radius", "border-radius"],
  ["border-top-left-radius", "border-top-left-radius"],
  ["border-top-right-radius", "border-top-right-radius"],
  ["border-bottom-right-radius", "border-bottom-right-radius"],
  ["border-bottom-left-radius", "border-bottom-left-radius"],
  ["border-start-start-radius", "border-start-start-radius"],
  ["border-start-end-radius", "border-start-end-radius"],
  ["border-end-start-radius", "border-end-start-radius"],
  ["border-end-end-radius", "border-end-end-radius"],
  ["opacity", "opacity"],
  ["padding", "padding"],
  ["padding-x", "padding-left"],
  ["padding-y", "padding-top"],
  ["padding-left", "padding-left"],
  ["padding-right", "padding-right"],
  ["padding-top", "padding-top"],
  ["padding-bottom", "padding-bottom"],
  ["padding-inline", "padding-inline"],
  ["padding-block", "padding-block"],
  ["padding-inline-start", "padding-inline-start"],
  ["padding-inline-end", "padding-inline-end"],
  ["padding-block-start", "padding-block-start"],
  ["padding-block-end", "padding-block-end"],
  ["margin", "margin"],
  ["margin-x", "margin-left"],
  ["margin-y", "margin-top"],
  ["margin-left", "margin-left"],
  ["margin-right", "margin-right"],
  ["margin-top", "margin-top"],
  ["margin-bottom", "margin-bottom"],
  ["margin-inline", "margin-inline"],
  ["margin-block", "margin-block"],
  ["margin-inline-start", "margin-inline-start"],
  ["margin-inline-end", "margin-inline-end"],
  ["margin-block-start", "margin-block-start"],
  ["margin-block-end", "margin-block-end"],
  ["width", "width"],
  ["height", "height"],
  ["min-width", "min-width"],
  ["max-width", "max-width"],
  ["min-height", "min-height"],
  ["max-height", "max-height"],
  ["inline-size", "inline-size"],
  ["block-size", "block-size"],
  ["min-inline-size", "min-inline-size"],
  ["max-inline-size", "max-inline-size"],
  ["min-block-size", "min-block-size"],
  ["max-block-size", "max-block-size"],
  ["inset", "inset"],
  ["top", "top"],
  ["right", "right"],
  ["bottom", "bottom"],
  ["left", "left"],
  ["inset-inline", "inset-inline"],
  ["inset-block", "inset-block"],
  ["inset-inline-start", "inset-inline-start"],
  ["inset-inline-end", "inset-inline-end"],
  ["inset-block-start", "inset-block-start"],
  ["inset-block-end", "inset-block-end"],
  ["gap", "gap"],
  ["row-gap", "row-gap"],
  ["column-gap", "column-gap"],
  ["font", "font"],
  ["font-size", "font-size"],
  ["typeface", "font-family"],
  ["font-family", "font-family"],
  ["font-weight", "font-weight"],
  ["font-style", "font-style"],
  ["font-variant", "font-variant"],
  ["font-stretch", "font-stretch"],
  ["font-kerning", "font-kerning"],
  ["font-optical-sizing", "font-optical-sizing"],
  ["font-feature-settings", "font-feature-settings"],
  ["font-variation-settings", "font-variation-settings"],
  ["font-size-adjust", "font-size-adjust"],
  ["font-synthesis", "font-synthesis"],
  ["font-synthesis-weight", "font-synthesis-weight"],
  ["font-synthesis-style", "font-synthesis-style"],
  ["font-synthesis-small-caps", "font-synthesis-small-caps"],
  ["font-synthesis-position", "font-synthesis-position"],
  ["font-variant-alternates", "font-variant-alternates"],
  ["font-variant-caps", "font-variant-caps"],
  ["font-variant-east-asian", "font-variant-east-asian"],
  ["font-variant-ligatures", "font-variant-ligatures"],
  ["font-variant-numeric", "font-variant-numeric"],
  ["font-variant-position", "font-variant-position"],
  ["font-language-override", "font-language-override"],
  ["font-palette", "font-palette"],
  ["letter-spacing", "letter-spacing"],
  ["line-height", "line-height"],
  ["text-indent", "text-indent"],
  ["text-align", "text-align"],
  ["text-align-last", "text-align-last"],
  ["text-rendering", "text-rendering"],
  ["text-decoration", "text-decoration"],
  ["text-decoration-line", "text-decoration-line"],
  ["text-decoration-color", "text-decoration-color"],
  ["text-decoration-style", "text-decoration-style"],
  ["text-decoration-skip", "text-decoration-skip"],
  ["text-decoration-skip-ink", "text-decoration-skip-ink"],
  ["text-decoration-thickness", "text-decoration-thickness"],
  ["text-underline-offset", "text-underline-offset"],
  ["text-underline-position", "text-underline-position"],
  ["text-shadow", "text-shadow"],
  ["text-emphasis", "text-emphasis"],
  ["text-emphasis-color", "text-emphasis-color"],
  ["text-emphasis-style", "text-emphasis-style"],
  ["text-emphasis-position", "text-emphasis-position"],
  ["text-transform", "text-transform"],
  ["text-overflow", "text-overflow"],
  ["white-space", "white-space"],
  ["text-size-adjust", "text-size-adjust"],
  ["text-orientation", "text-orientation"],
  ["text-wrap", "text-wrap"],
  ["text-wrap-mode", "text-wrap-mode"],
  ["text-wrap-style", "text-wrap-style"],
  ["text-justify", "text-justify"],
  ["text-combine-upright", "text-combine-upright"],
  ["ruby-align", "ruby-align"],
  ["ruby-position", "ruby-position"],
  ["text-spacing-trim", "text-spacing-trim"],
  ["text-autospace", "text-autospace"],
  ["text-box-trim", "text-box-trim"],
  ["text-box-edge", "text-box-edge"],
  ["word-break", "word-break"],
  ["overflow-wrap", "overflow-wrap"],
  ["word-wrap", "word-wrap"],
  ["line-break", "line-break"],
  ["hanging-punctuation", "hanging-punctuation"],
  ["vertical-align", "vertical-align"],
  ["display", "display"],
  ["position", "position"],
  ["z-index", "z-index"],
  ["overflow", "overflow"],
  ["overflow-inline", "overflow-inline"],
  ["overflow-block", "overflow-block"],
  ["overflow-x", "overflow-x"],
  ["overflow-y", "overflow-y"],
  ["box-sizing", "box-sizing"],
  ["direction", "direction"],
  ["writing-mode", "writing-mode"],
  ["tab-size", "tab-size"],
  ["hyphens", "hyphens"],
  ["line-clamp", "line-clamp"],
  ["list-style", "list-style"],
  ["list-style-type", "list-style-type"],
  ["list-style-position", "list-style-position"],
  ["list-style-image", "list-style-image"],
  ["counter-reset", "counter-reset"],
  ["counter-increment", "counter-increment"],
  ["counter-set", "counter-set"],
  ["quotes", "quotes"],
  ["marker-side", "marker-side"],
  ["marker-start", "marker-start"],
  ["marker-end", "marker-end"],
  ["orphans", "orphans"],
  ["widows", "widows"],
  ["box-decoration-break", "box-decoration-break"],
  ["border-collapse", "border-collapse"],
  ["border-spacing", "border-spacing"],
  ["table-layout", "table-layout"],
  ["caption-side", "caption-side"],
  ["empty-cells", "empty-cells"],
  ["scroll-behavior", "scroll-behavior"],
  ["overscroll-behavior", "overscroll-behavior"],
  ["overscroll-behavior-x", "overscroll-behavior-x"],
  ["overscroll-behavior-y", "overscroll-behavior-y"],
  ["overscroll-behavior-inline", "overscroll-behavior-inline"],
  ["overscroll-behavior-block", "overscroll-behavior-block"],
  ["scroll-snap-type", "scroll-snap-type"],
  ["scroll-snap-align", "scroll-snap-align"],
  ["scroll-snap-stop", "scroll-snap-stop"],
  ["scroll-timeline", "scroll-timeline"],
  ["scroll-timeline-name", "scroll-timeline-name"],
  ["scroll-timeline-axis", "scroll-timeline-axis"],
  ["view-timeline", "view-timeline"],
  ["view-timeline-name", "view-timeline-name"],
  ["view-timeline-axis", "view-timeline-axis"],
  ["view-timeline-inset", "view-timeline-inset"],
  ["timeline-scope", "timeline-scope"],
  ["scrollbar-color", "scrollbar-color"],
  ["scrollbar-width", "scrollbar-width"],
  ["scrollbar-gutter", "scrollbar-gutter"],
  ["overflow-clip-margin", "overflow-clip-margin"],
  ["scroll-margin", "scroll-margin"],
  ["scroll-margin-top", "scroll-margin-top"],
  ["scroll-margin-right", "scroll-margin-right"],
  ["scroll-margin-bottom", "scroll-margin-bottom"],
  ["scroll-margin-left", "scroll-margin-left"],
  ["scroll-margin-inline", "scroll-margin-inline"],
  ["scroll-margin-block", "scroll-margin-block"],
  ["scroll-margin-inline-start", "scroll-margin-inline-start"],
  ["scroll-margin-inline-end", "scroll-margin-inline-end"],
  ["scroll-margin-block-start", "scroll-margin-block-start"],
  ["scroll-margin-block-end", "scroll-margin-block-end"],
  ["scroll-padding", "scroll-padding"],
  ["scroll-padding-top", "scroll-padding-top"],
  ["scroll-padding-right", "scroll-padding-right"],
  ["scroll-padding-bottom", "scroll-padding-bottom"],
  ["scroll-padding-left", "scroll-padding-left"],
  ["scroll-padding-inline", "scroll-padding-inline"],
  ["scroll-padding-block", "scroll-padding-block"],
  ["scroll-padding-inline-start", "scroll-padding-inline-start"],
  ["scroll-padding-inline-end", "scroll-padding-inline-end"],
  ["scroll-padding-block-start", "scroll-padding-block-start"],
  ["scroll-padding-block-end", "scroll-padding-block-end"],
  ["touch-action", "touch-action"],
  ["align-items", "align-items"],
  ["justify-content", "justify-content"],
  ["align-self", "align-self"],
  ["justify-self", "justify-self"],
  ["flex-direction", "flex-direction"],
  ["flex-wrap", "flex-wrap"],
  ["flex-flow", "flex-flow"],
  ["flex", "flex"],
  ["flex-grow", "flex-grow"],
  ["flex-shrink", "flex-shrink"],
  ["flex-basis", "flex-basis"],
  ["grid", "grid"],
  ["grid-template", "grid-template"],
  ["grid-template-columns", "grid-template-columns"],
  ["grid-template-rows", "grid-template-rows"],
  ["grid-template-areas", "grid-template-areas"],
  ["grid-auto-columns", "grid-auto-columns"],
  ["grid-auto-rows", "grid-auto-rows"],
  ["grid-auto-flow", "grid-auto-flow"],
  ["grid-column", "grid-column"],
  ["grid-column-start", "grid-column-start"],
  ["grid-column-end", "grid-column-end"],
  ["grid-area", "grid-area"],
  ["grid-row", "grid-row"],
  ["grid-row-start", "grid-row-start"],
  ["grid-row-end", "grid-row-end"],
  ["align-content", "align-content"],
  ["align-tracks", "align-tracks"],
  ["justify-items", "justify-items"],
  ["justify-tracks", "justify-tracks"],
  ["place-items", "place-items"],
  ["place-content", "place-content"],
  ["place-self", "place-self"],
  ["object-fit", "object-fit"],
  ["object-position", "object-position"],
  ["object-view-box", "object-view-box"],
  ["aspect-ratio", "aspect-ratio"],
  ["image-rendering", "image-rendering"],
  ["image-orientation", "image-orientation"],
  ["image-resolution", "image-resolution"],
  ["background-image", "background-image"],
  ["background-size", "background-size"],
  ["background-position", "background-position"],
  ["background-position-x", "background-position-x"],
  ["background-position-y", "background-position-y"],
  ["background-repeat", "background-repeat"],
  ["background-repeat-x", "background-repeat-x"],
  ["background-repeat-y", "background-repeat-y"],
  ["background-clip", "background-clip"],
  ["background-origin", "background-origin"],
  ["background-attachment", "background-attachment"],
  ["background-blend-mode", "background-blend-mode"],
  ["visibility", "visibility"],
  ["transition", "transition"],
  ["transition-property", "transition-property"],
  ["transition-duration", "transition-duration"],
  ["transition-timing-function", "transition-timing-function"],
  ["transition-delay", "transition-delay"],
  ["transition-behavior", "transition-behavior"],
  ["animation", "animation"],
  ["animation-name", "animation-name"],
  ["animation-duration", "animation-duration"],
  ["animation-timing-function", "animation-timing-function"],
  ["animation-delay", "animation-delay"],
  ["animation-iteration-count", "animation-iteration-count"],
  ["animation-direction", "animation-direction"],
  ["animation-fill-mode", "animation-fill-mode"],
  ["animation-play-state", "animation-play-state"],
  ["animation-composition", "animation-composition"],
  ["animation-timeline", "animation-timeline"],
  ["animation-range", "animation-range"],
  ["animation-range-start", "animation-range-start"],
  ["animation-range-end", "animation-range-end"],
  ["transform", "transform"],
  ["transform-origin", "transform-origin"],
  ["transform-box", "transform-box"],
  ["transform-style", "transform-style"],
  ["translate", "translate"],
  ["rotate", "rotate"],
  ["scale", "scale"],
  ["perspective", "perspective"],
  ["perspective-origin", "perspective-origin"],
  ["backface-visibility", "backface-visibility"],
  ["offset-path", "offset-path"],
  ["offset-distance", "offset-distance"],
  ["offset-rotate", "offset-rotate"],
  ["offset-anchor", "offset-anchor"],
  ["offset-position", "offset-position"],
  ["filter", "filter"],
  ["backdrop-filter", "backdrop-filter"],
  ["clip-path", "clip-path"],
  ["mask", "mask"],
  ["mask-image", "mask-image"],
  ["mask-size", "mask-size"],
  ["mask-position", "mask-position"],
  ["mask-repeat", "mask-repeat"],
  ["mask-origin", "mask-origin"],
  ["mask-clip", "mask-clip"],
  ["mask-composite", "mask-composite"],
  ["mask-mode", "mask-mode"],
  ["cursor", "cursor"],
  ["pointer-events", "pointer-events"],
  ["appearance", "appearance"],
  ["user-select", "user-select"],
  ["resize", "resize"],
  ["field-sizing", "field-sizing"],
  ["interpolate-size", "interpolate-size"],
  ["overlay", "overlay"],
  ["outline", "outline"],
  ["outline-width", "outline-width"],
  ["outline-offset", "outline-offset"],
  ["outline-style", "outline-style"],
  ["outline-color", "outline-color"],
  ["box-shadow", "box-shadow"],
  ["color-scheme", "color-scheme"],
  ["forced-color-adjust", "forced-color-adjust"],
  ["print-color-adjust", "print-color-adjust"],
  ["color-interpolation", "color-interpolation"],
  ["color-interpolation-filters", "color-interpolation-filters"],
  ["paint-order", "paint-order"],
  ["shape-outside", "shape-outside"],
  ["shape-margin", "shape-margin"],
  ["shape-image-threshold", "shape-image-threshold"],
  ["contain", "contain"],
  ["content-visibility", "content-visibility"],
  ["contain-intrinsic-size", "contain-intrinsic-size"],
  ["contain-intrinsic-width", "contain-intrinsic-width"],
  ["contain-intrinsic-height", "contain-intrinsic-height"],
  ["contain-intrinsic-inline-size", "contain-intrinsic-inline-size"],
  ["contain-intrinsic-block-size", "contain-intrinsic-block-size"],
  ["container", "container"],
  ["container-type", "container-type"],
  ["container-name", "container-name"],
  ["anchor-name", "anchor-name"],
  ["position-anchor", "position-anchor"],
  ["position-area", "position-area"],
  ["position-try", "position-try"],
  ["position-try-fallbacks", "position-try-fallbacks"],
  ["position-try-order", "position-try-order"],
  ["position-visibility", "position-visibility"],
  ["will-change", "will-change"],
  ["view-transition-name", "view-transition-name"],
  ["isolation", "isolation"],
  ["mix-blend-mode", "mix-blend-mode"],
  ["columns", "columns"],
  ["column-count", "column-count"],
  ["column-width", "column-width"],
  ["column-fill", "column-fill"],
  ["column-span", "column-span"],
  ["column-rule", "column-rule"],
  ["column-rule-color", "column-rule-color"],
  ["column-rule-style", "column-rule-style"],
  ["column-rule-width", "column-rule-width"],
  ["break-before", "break-before"],
  ["break-after", "break-after"],
  ["break-inside", "break-inside"],
  ["float", "float"],
  ["clear", "clear"],
  ["order", "order"],
  ["focus", "outline-color"]
]);

function webStyleCSSValue(name, value) {
  return typeof value === "number" && name !== "opacity" &&
      name !== "font-weight" && name !== "fontWeight" &&
      name !== "line-height" && name !== "lineHeight" &&
      name !== "z-index" && name !== "zIndex" &&
      name !== "tab-size" && name !== "tabSize" &&
      name !== "font-size-adjust" && name !== "fontSizeAdjust" &&
      name !== "column-count" && name !== "columnCount" &&
      name !== "order" &&
      name !== "flex-grow" && name !== "flexGrow" &&
      name !== "flex-shrink" && name !== "flexShrink" &&
      name !== "border-image-slice" && name !== "borderImageSlice" &&
      name !== "border-image-width" && name !== "borderImageWidth" &&
      name !== "border-image-outset" && name !== "borderImageOutset" &&
      name !== "grid-column-start" && name !== "gridColumnStart" &&
      name !== "grid-column-end" && name !== "gridColumnEnd" &&
      name !== "grid-row-start" && name !== "gridRowStart" &&
      name !== "grid-row-end" && name !== "gridRowEnd" &&
      name !== "line-clamp" && name !== "lineClamp" &&
      name !== "webkitLineClamp" &&
      name !== "orphans" && name !== "widows" &&
      name !== "animation-iteration-count" && name !== "animationIterationCount" &&
      name !== "shape-image-threshold" && name !== "shapeImageThreshold" &&
      name !== "scale"
    ? value + "px" : String(value);
}

function webStyleValueToCSS(name, value) {
  if (value === undefined || value === null || value === "")
    return "";
  let prop = webCSSPropertyNames.get(name) || (name.startsWith("--") ? name : "");
  if (name === "border" && isWebBorderShorthandValue(value))
    prop = "border";
  if (!prop)
    return "";
  const cssValue = webStyleCSSValue(prop, value);
  return `  ${prop}: ${cssValue};`;
}

function isWebBorderShorthandValue(value) {
  return /\s/.test(String(value || "").trim());
}

function webStyleRuleToCSS(rule) {
  const selector = webStyleSelectorToCSS(rule.selector);
  const lines = [];
  const style = rule.style || {};
  const offsetX = style["offset-x"];
  const offsetY = style["offset-y"];
  const backgroundStart = style.background;
  const backgroundEnd = style["background-end"];
  for (const [name, value] of Object.entries(rule.style || {})) {
    if (name === "padding-x") {
      const cssValue = webStyleCSSValue("padding-left", value);
      lines.push(`  padding-left: ${cssValue};`);
      lines.push(`  padding-right: ${cssValue};`);
      continue;
    }
    if (name === "padding-y") {
      const cssValue = webStyleCSSValue("padding-top", value);
      lines.push(`  padding-top: ${cssValue};`);
      lines.push(`  padding-bottom: ${cssValue};`);
      continue;
    }
    if (name === "margin-x") {
      const cssValue = webStyleCSSValue("margin-left", value);
      lines.push(`  margin-left: ${cssValue};`);
      lines.push(`  margin-right: ${cssValue};`);
      continue;
    }
    if (name === "margin-y") {
      const cssValue = webStyleCSSValue("margin-top", value);
      lines.push(`  margin-top: ${cssValue};`);
      lines.push(`  margin-bottom: ${cssValue};`);
      continue;
    }
    if (name === "offset-x" || name === "offset-y")
      continue;
    if (name === "transform" &&
        ((offsetX !== undefined && offsetX !== null && offsetX !== "") ||
         (offsetY !== undefined && offsetY !== null && offsetY !== "")))
      continue;
    if (name === "content-offset-y") {
      lines.push(`  --kry-content-offset-y: ${webStyleCSSValue("--kry-content-offset-y", value)};`);
      continue;
    }
    if (name === "content-offset-x") {
      lines.push(`  --kry-content-offset-x: ${webStyleCSSValue("--kry-content-offset-x", value)};`);
      continue;
    }
    if (name === "icon-size") {
      lines.push(`  --kry-icon-size: ${webStyleCSSValue("--kry-icon-size", value)};`);
      continue;
    }
    if (name === "background-end") {
      lines.push(`  --kry-background-end: ${webStyleCSSValue("--kry-background-end", value)};`);
      continue;
    }
    const line = webStyleValueToCSS(name, value);
    if (line)
      lines.push(line);
  }
  if (backgroundStart !== undefined && backgroundStart !== null && backgroundStart !== "" &&
      backgroundEnd !== undefined && backgroundEnd !== null && backgroundEnd !== "")
    lines.push(`  background-image: linear-gradient(${webStyleCSSValue("background", backgroundStart)}, ${webStyleCSSValue("background", backgroundEnd)});`);
  if (offsetX !== undefined && offsetX !== null && offsetX !== "")
    lines.push(`  --kry-offset-x: ${webStyleCSSValue("--kry-offset-x", offsetX)};`);
  if (offsetY !== undefined && offsetY !== null && offsetY !== "")
    lines.push(`  --kry-offset-y: ${webStyleCSSValue("--kry-offset-y", offsetY)};`);
  if ((offsetX !== undefined && offsetX !== null && offsetX !== "") ||
      (offsetY !== undefined && offsetY !== null && offsetY !== "")) {
    const transform = style.transform ? ` ${webStyleCSSValue("transform", style.transform)}` : "";
    lines.push(`  transform: translate(var(--kry-offset-x, 0px), var(--kry-offset-y, 0px))${transform};`);
  }
  if (!lines.length)
    return "";
  return `${selector} {\n${lines.join("\n")}\n}`;
}

function webKeyframeSelectorToCSS(selector) {
  return splitSelectorList(selector)
    .map((part) => part.trim().toLowerCase())
    .filter((part) => part === "from" || part === "to" || /^\d+(?:\.\d+)?%$/.test(part))
    .join(", ");
}

function webKeyframesToCSS(keyframes) {
  if (!keyframes || !keyframes.name || !Array.isArray(keyframes.frames))
    return "";
  const frames = [];
  for (const frame of keyframes.frames) {
    const selector = webKeyframeSelectorToCSS(frame.selector);
    if (!selector)
      continue;
    const lines = [];
    for (const [name, value] of Object.entries(frame.style || {})) {
      const line = webStyleValueToCSS(name, value);
      if (line)
        lines.push("  " + line);
    }
    if (lines.length)
      frames.push(`  ${selector} {\n${lines.join("\n")}\n  }`);
  }
  if (!frames.length)
    return "";
  return `@keyframes ${cssEscapeIdent(keyframes.name)} {\n${frames.join("\n")}\n}`;
}

function webConditionalGroupToCSS(group) {
  if (!group || !group.kind || !group.query || !Array.isArray(group.rules))
    return "";
  if (group.kind !== "media" && group.kind !== "supports" && group.kind !== "container")
    return "";
  const query = String(group.query).replace(/[{}]/g, "").trim();
  if (!query)
    return "";
  const rules = group.rules
    .map(webStyleRuleToCSS)
    .filter(Boolean)
    .map((rule) => rule.replace(/^/gm, "  "));
  if (!rules.length)
    return "";
  return `@${group.kind} ${query} {\n${rules.join("\n\n")}\n}`;
}

export function webStyleSheetToCSS(sheet) {
  const parsed = typeof sheet === "string" ? parseWebStyleSheet(sheet) : sheet;
  return [
    ...(parsed?.keyframes || []).map(webKeyframesToCSS),
    ...(parsed?.rules || []).map(webStyleRuleToCSS),
    ...(parsed?.groups || []).map(webConditionalGroupToCSS)
  ]
    .filter(Boolean)
    .join("\n\n");
}

export function installWebStyleSheet(sheet, target = null, id = "kryon") {
  if (typeof document === "undefined")
    return null;
  const css = webStyleSheetToCSS(sheet);
  if (!css)
    return null;
  const owner = target || document.head || document.documentElement || document.body;
  if (!owner || typeof owner.appendChild !== "function")
    return null;
  const key = String(id || "kryon");
  const selector = `style[data-kry-style="${cssEscapeString(key)}"]`;
  let el = typeof owner.querySelector === "function" ? owner.querySelector(selector) : null;
  if (!el && typeof document.querySelector === "function")
    el = document.querySelector(selector);
  if (!el) {
    el = document.createElement("style");
    el.setAttribute("data-kry-style", key);
    owner.appendChild(el);
  }
  el.textContent = css;
  return () => {
    if (el?.parentNode && typeof el.parentNode.removeChild === "function")
      el.parentNode.removeChild(el);
  };
}

export function loadAppWebStyleSheets(app) {
  return (app?.styles || [])
    .map((style) => style?.source)
    .filter((source) => typeof source === "string" && source.length > 0)
    .map((source) => parseWebStyleSheet(source));
}

export function installAppWebStyleSheets(app, target = null, id = "kryon-app") {
  const sheets = loadAppWebStyleSheets(app);
  if (!sheets.length)
    return null;
  return installWebStyleSheet({
    pack: app?.title || id || "kryon-app",
    rules: sheets.flatMap((sheet) => sheet.rules || []),
    keyframes: sheets.flatMap((sheet) => sheet.keyframes || []),
    groups: sheets.flatMap((sheet) => sheet.groups || [])
  }, target, id);
}

function styleStateMatches(name, state, facts = {}) {
  if (!name || name === "any")
    return true;
  const key = String(name).toLowerCase();
  if (key === "normal")
    return !Object.values(state || {}).some(Boolean) && !facts.readOnly && !facts.required;
  if (key === "hover" || key === "pressed" || key === "active" ||
      key === "focus" || key === "focused" || key === "focus-visible") {
    const stateKey = key === "active" ? "pressed" :
      (key === "focused" || key === "focus-visible" ? "focus" : key);
    return !!state?.[key] || !!state?.[stateKey];
  }
  if (key === "enabled")
    return !state?.disabled && !facts.disabled;
  if (key === "readonly" || key === "read-only")
    return !!facts.readOnly || !!state?.readonly || !!state?.readOnly;
  if (key === "required")
    return !!facts.required || !!state?.required;
  if (key === "optional")
    return !facts.required && !state?.required;
  if (key === "valid") {
    if (state?.valid)
      return true;
    const invalid = !!state?.invalid || facts.ariaAttrs?.invalid === "true" ||
      facts.extraAttrs?.["aria-invalid"] === "true";
    const formControl = webStyleFactsDescribeFormControl(facts);
    return formControl && !invalid;
  }
  if (key === "indeterminate" || key === "default" || key === "autofill")
    return !!state?.[key];
  if (key === "placeholder-shown" || key === "placeholder_shown") {
    if (state?.["placeholder-shown"] || state?.placeholderShown)
      return true;
    return webStyleFactsDescribeFormControl(facts) && !!facts.placeholder &&
      !String(facts.domValue || facts.value || "");
  }
  return !!state?.[key];
}

function webStyleFactsDescribeFormControl(facts) {
  return facts.tag === "input" || facts.tag === "select" || facts.tag === "textarea" ||
    ["TextField", "Input", "TextArea", "ColorPicker", "Slider", "Spinbox", "Dropdown",
     "ListBox", "Checkbox", "Toggle", "Radio"].includes(facts.kind);
}

function selectorDataAttrValue(key, facts) {
  if (key.startsWith("data-"))
    return facts.dataAttrs?.[key.slice(5)] ?? facts.extraAttrs?.[key];
  if (key.startsWith("data."))
    return facts.dataAttrs?.[key.slice(5).replace(/_/g, "-").toLowerCase()] ??
      facts.extraAttrs?.["data-" + key.slice(5).replace(/_/g, "-").toLowerCase()];
  return undefined;
}

function selectorNativeAttrValue(key, facts) {
  if (facts.extraAttrs && Object.prototype.hasOwnProperty.call(facts.extraAttrs, key))
    return facts.extraAttrs[key];
  switch (key) {
    case "source": return facts.sourcePath;
    case "line": return facts.sourceLine;
    case "column": return facts.sourceColumn;
    case "sourceRef": return facts.sourceRef;
    case "sourceColumnRef": return facts.sourceColumnRef;
    case "sourceRangeRef": return facts.sourceRangeRef;
    case "endLine":
    case "sourceEndLine": return facts.sourceEndLine;
    case "endColumn":
    case "sourceEndColumn": return facts.sourceEndColumn;
    case "id": return facts.id;
    case "name": return facts.domName;
    case "title": return facts.title;
    case "lang": return facts.lang;
    case "dir": return facts.dir;
    case "translate": return facts.translate;
    case "dirname": return facts.dirname;
    case "tabindex": return facts.tabIndex;
    case "value": return facts.domValue || facts.value;
    case "type": return facts.inputType;
    case "list": return facts.dataList;
    case "usemap": return facts.useMap;
    case "useMap": return facts.useMap;
    case "form": return facts.formOwner;
    case "part": return facts.part || facts.extraAttrs?.part;
    case "slot": return facts.slot || facts.extraAttrs?.slot;
    case "action": return facts.formAction;
    case "method": return facts.formMethod;
    case "enctype": return facts.formEncType;
    case "autocomplete": return facts.autoComplete;
    case "hidden": return facts.hidden;
    case "draggable": return facts.draggable;
    case "spellcheck": return facts.spellCheck;
    case "contenteditable": return facts.contentEditable;
    case "autofocus": return facts.autoFocus;
    case "inert": return facts.inert;
    case "autocapitalize": return facts.autoCapitalize;
    case "enterkeyhint": return facts.enterKeyHint;
    case "download": return facts.download;
    case "formnovalidate": return facts.formNoValidate;
    case "novalidate": return facts.noValidate;
    case "popover": return facts.popover;
    case "popovertarget": return facts.popoverTarget;
    case "popoverTarget": return facts.popoverTarget;
    case "popovertargetaction": return facts.popoverTargetAction;
    case "popoverTargetAction": return facts.popoverTargetAction;
    case "open": return facts.open;
    case "scrollleft": return facts.scrollLeft;
    case "scrolltop": return facts.scrollTop;
    case "readonly": return facts.readOnly;
    case "required": return facts.required;
    case "min": return facts.min;
    case "max": return facts.max;
    case "step": return facts.step;
    case "minlength": return facts.minLength;
    case "maxlength": return facts.maxLength;
    case "pattern": return facts.pattern;
    case "accept": return facts.accept;
    case "inputmode": return facts.inputMode;
    case "headers": return facts.headers;
    case "scope": return facts.scope;
    case "colspan": return facts.colSpan;
    case "rowspan": return facts.rowSpan;
    case "aria-sort":
    case "ariaSort": return facts.ariaSort;
    case "aria-orientation":
    case "ariaOrientation": return facts.ariaOrientation;
    case "aria-level":
    case "ariaLevel": return facts.ariaLevel;
    case "aria-posinset":
    case "ariaPosInSet": return facts.ariaPosInSet;
    case "aria-setsize":
    case "ariaSetSize": return facts.ariaSetSize;
    case "aria-haspopup":
    case "ariaHasPopup": return facts.ariaHasPopup;
    case "aria-multiselectable":
    case "ariamultiselectable":
    case "ariaMultiSelectable": return facts.ariaMultiSelectable;
    case "aria-rowindex":
    case "ariarowindex":
    case "ariaRowIndex": return facts.ariaRowIndex;
    case "aria-colindex":
    case "ariacolindex":
    case "ariaColIndex": return facts.ariaColIndex;
    case "aria-rowcount":
    case "ariarowcount":
    case "ariaRowCount": return facts.ariaRowCount;
    case "aria-colcount":
    case "ariacolcount":
    case "ariaColCount": return facts.ariaColCount;
    case "multiple": return facts.multiple;
    case "for": return facts.htmlFor;
    default: return facts[key] ?? facts.extraAttrs?.[key];
  }
}

function selectorDataAttrPresent(key, facts) {
  if (key.startsWith("data-"))
    return Object.prototype.hasOwnProperty.call(facts.dataAttrs || {}, key.slice(5)) ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, key);
  if (key.startsWith("data."))
    return Object.prototype.hasOwnProperty.call(facts.dataAttrs || {},
      key.slice(5).replace(/_/g, "-").toLowerCase()) ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {},
        "data-" + key.slice(5).replace(/_/g, "-").toLowerCase());
  return false;
}

function selectorAriaAttrValue(key, facts) {
  if (key === "aria-label" || key === "aria.label")
    return facts.ariaLabel || facts.ariaAttrs?.label || facts.extraAttrs?.["aria-label"];
  if (key === "aria-description" || key === "aria.description")
    return facts.ariaDescription || facts.ariaAttrs?.description ||
      facts.extraAttrs?.["aria-description"];
  if (key === "aria-describedby" || key === "aria.describedby" ||
      key === "aria.described_by")
    return facts.ariaDescribedBy || facts.ariaAttrs?.describedby ||
      facts.extraAttrs?.["aria-describedby"];
  if (key === "aria-details" || key === "aria.details")
    return facts.ariaDetails || facts.ariaAttrs?.details ||
      facts.extraAttrs?.["aria-details"];
  if (key === "aria-errormessage" || key === "aria.errormessage" ||
      key === "aria.error_message")
    return facts.ariaErrorMessage || facts.ariaAttrs?.errormessage ||
      facts.extraAttrs?.["aria-errormessage"];
  if (key === "aria-flowto" || key === "aria.flowto" || key === "aria.flow_to")
    return facts.ariaFlowTo || facts.ariaAttrs?.flowto ||
      facts.extraAttrs?.["aria-flowto"];
  if (key === "aria-labelledby" || key === "aria.labelledby" ||
      key === "aria.labelled_by")
    return facts.ariaLabelledBy || facts.ariaAttrs?.labelledby ||
      facts.extraAttrs?.["aria-labelledby"];
  if (key === "aria-activedescendant" || key === "aria.activedescendant" ||
      key === "aria.active_descendant")
    return facts.ariaActiveDescendant || facts.ariaAttrs?.activedescendant ||
      facts.extraAttrs?.["aria-activedescendant"];
  if (key === "aria-controls" || key === "aria.controls")
    return facts.ariaControls || facts.ariaAttrs?.controls ||
      facts.extraAttrs?.["aria-controls"];
  if (key === "aria-owns" || key === "aria.owns")
    return facts.ariaOwns || facts.ariaAttrs?.owns || facts.extraAttrs?.["aria-owns"];
  if (key === "aria-sort" || key === "aria.sort")
    return facts.ariaSort || facts.ariaAttrs?.sort || facts.extraAttrs?.["aria-sort"];
  if (key === "aria-orientation" || key === "aria.orientation")
    return facts.ariaOrientation || facts.ariaAttrs?.orientation ||
      facts.extraAttrs?.["aria-orientation"];
  if (key === "aria-level" || key === "aria.level")
    return facts.ariaLevel || facts.ariaAttrs?.level ||
      facts.extraAttrs?.["aria-level"];
  if (key === "aria-posinset" || key === "aria.posinset" ||
      key === "aria.pos_in_set")
    return facts.ariaPosInSet || facts.ariaAttrs?.posinset ||
      facts.extraAttrs?.["aria-posinset"];
  if (key === "aria-setsize" || key === "aria.setsize" ||
      key === "aria.set_size")
    return facts.ariaSetSize || facts.ariaAttrs?.setsize ||
      facts.extraAttrs?.["aria-setsize"];
  if (key === "aria-haspopup" || key === "aria.haspopup" ||
      key === "aria.has_popup")
    return facts.ariaHasPopup || facts.ariaAttrs?.haspopup ||
      facts.extraAttrs?.["aria-haspopup"];
  if (key === "aria-multiselectable" || key === "aria.multiselectable" ||
      key === "aria.multi_selectable")
    return facts.ariaMultiSelectable || facts.ariaAttrs?.multiselectable ||
      facts.extraAttrs?.["aria-multiselectable"];
  if (key === "aria-rowindex" || key === "aria.rowindex" ||
      key === "aria.row_index")
    return facts.ariaRowIndex || facts.ariaAttrs?.rowindex ||
      facts.extraAttrs?.["aria-rowindex"];
  if (key === "aria-colindex" || key === "aria.colindex" ||
      key === "aria.col_index")
    return facts.ariaColIndex || facts.ariaAttrs?.colindex ||
      facts.extraAttrs?.["aria-colindex"];
  if (key === "aria-rowcount" || key === "aria.rowcount" ||
      key === "aria.row_count")
    return facts.ariaRowCount || facts.ariaAttrs?.rowcount ||
      facts.extraAttrs?.["aria-rowcount"];
  if (key === "aria-colcount" || key === "aria.colcount" ||
      key === "aria.col_count")
    return facts.ariaColCount || facts.ariaAttrs?.colcount ||
      facts.extraAttrs?.["aria-colcount"];
  if (key === "aria-live" || key === "aria.live")
    return facts.ariaLive || facts.ariaAttrs?.live || facts.extraAttrs?.["aria-live"];
  if (key.startsWith("aria-"))
    return facts.ariaAttrs?.[key.slice(5)] ?? facts.extraAttrs?.[key];
  if (key.startsWith("aria."))
    return facts.ariaAttrs?.[key.slice(5).replace(/_/g, "-").toLowerCase()] ??
      facts.extraAttrs?.["aria-" + key.slice(5).replace(/_/g, "-").toLowerCase()];
  return undefined;
}

function selectorAriaAttrPresent(key, facts) {
  if (key === "aria-label" || key === "aria.label")
    return !!facts.ariaLabel ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "label") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-label");
  if (key === "aria-description" || key === "aria.description")
    return !!facts.ariaDescription ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "description") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-description");
  if (key === "aria-describedby" || key === "aria.describedby" ||
      key === "aria.described_by")
    return !!facts.ariaDescribedBy ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "describedby") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-describedby");
  if (key === "aria-details" || key === "aria.details")
    return !!facts.ariaDetails ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "details") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-details");
  if (key === "aria-errormessage" || key === "aria.errormessage" ||
      key === "aria.error_message")
    return !!facts.ariaErrorMessage ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "errormessage") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-errormessage");
  if (key === "aria-flowto" || key === "aria.flowto" || key === "aria.flow_to")
    return !!facts.ariaFlowTo ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "flowto") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-flowto");
  if (key === "aria-labelledby" || key === "aria.labelledby" ||
      key === "aria.labelled_by")
    return !!facts.ariaLabelledBy ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "labelledby") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-labelledby");
  if (key === "aria-activedescendant" || key === "aria.activedescendant" ||
      key === "aria.active_descendant")
    return !!facts.ariaActiveDescendant ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "activedescendant") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-activedescendant");
  if (key === "aria-controls" || key === "aria.controls")
    return !!facts.ariaControls ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "controls") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-controls");
  if (key === "aria-owns" || key === "aria.owns")
    return !!facts.ariaOwns ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "owns") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-owns");
  if (key === "aria-sort" || key === "aria.sort")
    return !!facts.ariaSort ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "sort") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-sort");
  if (key === "aria-orientation" || key === "aria.orientation")
    return !!facts.ariaOrientation ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "orientation") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-orientation");
  if (key === "aria-level" || key === "aria.level")
    return !!facts.ariaLevel ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "level") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-level");
  if (key === "aria-posinset" || key === "aria.posinset" ||
      key === "aria.pos_in_set")
    return !!facts.ariaPosInSet ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "posinset") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-posinset");
  if (key === "aria-setsize" || key === "aria.setsize" ||
      key === "aria.set_size")
    return !!facts.ariaSetSize ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "setsize") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-setsize");
  if (key === "aria-haspopup" || key === "aria.haspopup" ||
      key === "aria.has_popup")
    return !!facts.ariaHasPopup ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "haspopup") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-haspopup");
  if (key === "aria-multiselectable" || key === "aria.multiselectable" ||
      key === "aria.multi_selectable")
    return !!facts.ariaMultiSelectable ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "multiselectable") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-multiselectable");
  if (key === "aria-rowindex" || key === "aria.rowindex" ||
      key === "aria.row_index")
    return !!facts.ariaRowIndex ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "rowindex") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-rowindex");
  if (key === "aria-colindex" || key === "aria.colindex" ||
      key === "aria.col_index")
    return !!facts.ariaColIndex ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "colindex") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-colindex");
  if (key === "aria-rowcount" || key === "aria.rowcount" ||
      key === "aria.row_count")
    return !!facts.ariaRowCount ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "rowcount") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-rowcount");
  if (key === "aria-colcount" || key === "aria.colcount" ||
      key === "aria.col_count")
    return !!facts.ariaColCount ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "colcount") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-colcount");
  if (key === "aria-live" || key === "aria.live")
    return !!facts.ariaLive ||
      Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, "live") ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, "aria-live");
  if (key.startsWith("aria-"))
    return Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {}, key.slice(5)) ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {}, key);
  if (key.startsWith("aria."))
    return Object.prototype.hasOwnProperty.call(facts.ariaAttrs || {},
      key.slice(5).replace(/_/g, "-").toLowerCase()) ||
      Object.prototype.hasOwnProperty.call(facts.extraAttrs || {},
        "aria-" + key.slice(5).replace(/_/g, "-").toLowerCase());
  return false;
}

function selectorAttrPresent(key, facts) {
  if (key.startsWith("data-") || key.startsWith("data."))
    return selectorDataAttrPresent(key, facts);
  if (key.startsWith("aria-") || key.startsWith("aria."))
    return selectorAriaAttrPresent(key, facts);
  const value = selectorNativeAttrValue(key, facts);
  return value !== undefined && value !== null && value !== false && value !== "";
}

function selectorAttrValue(key, facts) {
  if (key === "role")
    return facts.role;
  if (key.startsWith("data-") || key.startsWith("data."))
    return selectorDataAttrValue(key, facts);
  if (key.startsWith("aria-") || key.startsWith("aria."))
    return selectorAriaAttrValue(key, facts);
  return selectorNativeAttrValue(key, facts);
}

function selectorAttrValueMatches(actual, expected, op) {
  const value = String(actual ?? "");
  const needle = String(expected ?? "");
  switch (op || "=") {
    case "~=":
      return value.split(/\s+/).filter(Boolean).includes(needle);
    case "^=":
      return value.startsWith(needle);
    case "$=":
      return value.endsWith(needle);
    case "*=":
      return value.includes(needle);
    case "|=":
      return value === needle || value.startsWith(needle + "-");
    case "=":
    default:
      return value === needle;
  }
}

function selectorKindMatches(kind, facts) {
  const text = String(kind || "");
  if (!text || text === "*")
    return true;
  const lower = text.toLowerCase();
  if (lower === String(facts.kind || "").toLowerCase())
    return true;
  return [facts.path, facts.name, facts.key, facts.ref, facts.webRef, facts.id]
    .some((value) => text === String(value || ""));
}

function selectorMatchesFacts(selector, facts) {
  if (!selectorKindMatches(selector.kind, facts))
    return false;
  if (selector.id && selector.id !== facts.id && selector.id !== facts.name && selector.id !== facts.key)
    return false;
  for (const cls of selector.classes)
    if (!facts.classes?.includes(cls))
      return false;
  for (const [key, value] of Object.entries(selector.attrs)) {
    const op = selector.attrOps?.[key] || "=";
    if (value === null) {
      if (!selectorAttrPresent(key, facts))
        return false;
    }
    else if (key === "state" && !styleStateMatches(value, facts.state, facts))
      return false;
    else if (key !== "state" &&
             !selectorAttrValueMatches(selectorAttrValue(key, facts), value, op))
      return false;
  }
  const states = selector.states?.length ? selector.states : (selector.state ? [selector.state] : []);
  return states.every((state) => styleStateMatches(state, facts.state, facts));
}

function webNodeParentFromFrame(node) {
  const parentPath = node?.parentPath || "";
  if (!node || !parentPath || parentPath === node.path)
    return null;
  for (const candidate of node.__kryFrameNodes || [])
    if (candidate?.path === parentPath)
      return candidate;
  return null;
}

function webNodeSiblingsFromFrame(node) {
  if (!node)
    return [];
  const nodes = node.__kryFrameNodes || [];
  const parentPath = node.parentPath || "";
  return nodes.filter((candidate) => {
    if (!candidate || candidate.path === candidate.parentPath)
      return false;
    return (candidate.parentPath || "") === parentPath;
  });
}

function webNodePreviousSiblingsFromFrame(node) {
  const siblings = webNodeSiblingsFromFrame(node);
  const index = siblings.indexOf(node);
  return index <= 0 ? [] : siblings.slice(0, index);
}

function webNodePreviousSiblingFromFrame(node) {
  return webNodePreviousSiblingsFromFrame(node).pop() || null;
}

function webNodeNextSiblingFromFrame(node) {
  const siblings = webNodeSiblingsFromFrame(node);
  const index = siblings.indexOf(node);
  return index < 0 || index >= siblings.length - 1 ? null : siblings[index + 1];
}

function webNodeChildrenFromFrame(node) {
  if (!node)
    return [];
  return (node.__kryFrameNodes || []).filter((candidate) =>
    candidate && candidate.path !== node.path && candidate.parentPath === node.path);
}

function webNodeDescendantsFromFrame(node) {
  const path = node?.path || "";
  if (!path)
    return [];
  return (node.__kryFrameNodes || []).filter((candidate) =>
    candidate && candidate.path !== path && candidate.path?.startsWith(path + "/"));
}

function nthChildPseudoMatches(pseudo, siblings, node) {
  const match = String(pseudo || "").match(/^nth-child\(([^)]*)\)$/);
  if (!match)
    return false;
  return nthChildPositionMatches(match[1], siblings, node, false);
}

function nthLastChildPseudoMatches(pseudo, siblings, node) {
  const match = String(pseudo || "").match(/^nth-last-child\(([^)]*)\)$/);
  if (!match)
    return false;
  return nthChildPositionMatches(match[1], siblings, node, true);
}

function webNodeSameTypeSiblingsFromFrame(siblings, node) {
  const kind = String(node?.kind || "").toLowerCase();
  return siblings.filter((candidate) =>
    String(candidate?.kind || "").toLowerCase() === kind);
}

function nthOfTypePseudoMatches(pseudo, siblings, node) {
  const match = String(pseudo || "").match(/^nth-of-type\(([^)]*)\)$/);
  if (!match)
    return false;
  return nthChildPositionMatches(match[1], webNodeSameTypeSiblingsFromFrame(siblings, node), node, false);
}

function nthLastOfTypePseudoMatches(pseudo, siblings, node) {
  const match = String(pseudo || "").match(/^nth-last-of-type\(([^)]*)\)$/);
  if (!match)
    return false;
  return nthChildPositionMatches(match[1], webNodeSameTypeSiblingsFromFrame(siblings, node), node, true);
}

function nthChildPositionMatches(text, siblings, node, fromEnd = false) {
  const index = siblings.indexOf(node);
  const position = fromEnd ? siblings.length - index : index + 1;
  const value = String(text || "").trim().toLowerCase();
  if (index < 0)
    return false;
  if (value === "odd")
    return position % 2 === 1;
  if (value === "even")
    return position > 0 && position % 2 === 0;
  const number = Number(value);
  if (Number.isInteger(number) && number > 0)
    return position === number;
  const compact = value.replace(/\s+/g, "");
  const formula = compact.match(/^([+-]?\d*)n(?:([+-]\d+))?$/);
  if (!formula)
    return false;
  const rawA = formula[1];
  const a = rawA === "" || rawA === "+" ? 1 : rawA === "-" ? -1 : Number(rawA);
  const b = formula[2] === undefined ? 0 : Number(formula[2]);
  if (!Number.isInteger(a) || !Number.isInteger(b))
    return false;
  if (a === 0)
    return position === b;
  const delta = position - b;
  return delta / a >= 0 && delta % a === 0;
}

function webNodeHasFocusWithin(node) {
  if (!node)
    return false;
  if (node.state?.focus || node.state?.["focus-within"])
    return true;
  const parentPath = node.path || "";
  if (!parentPath)
    return false;
  return (node.__kryFrameNodes || []).some((candidate) =>
    candidate && candidate !== node && candidate.path !== parentPath &&
    candidate.path?.startsWith(parentPath + "/") &&
    (candidate.state?.focus || candidate.state?.["focus-within"]));
}

function webNodeMatchesRouteTarget(node) {
  const hash = GetRouteHash().replace(/^#/, "");
  if (!node || !hash)
    return false;
  let decoded = hash;
  try {
    decoded = decodeURIComponent(hash);
  } catch {
    decoded = hash;
  }
  const targets = new Set([hash, decoded]);
  const identity = webNodeIdentity(node);
  if (identity.aliases.some((alias) => targets.has(alias)))
    return true;
  return targets.has(webDOMGeneratedId({ node, ref: identity.ref }));
}

function selectorHasPseudoMatches(pseudo, node, scopeNode = null) {
  const match = String(pseudo || "").match(/^has\(([\s\S]*)\)$/);
  if (!match)
    return false;
  return splitSelectorList(match[1]).some((rawSelector) => {
    const raw = String(rawSelector || "").trim();
    if (!raw || /:has\s*\(/i.test(raw))
      return false;
    let selectorText = raw;
    let candidates = webNodeDescendantsFromFrame(node);
    if (raw.startsWith(">")) {
      selectorText = raw.slice(1).trim();
      candidates = webNodeChildrenFromFrame(node);
    } else if (raw.startsWith("+")) {
      selectorText = raw.slice(1).trim();
      candidates = [webNodeNextSiblingFromFrame(node)].filter(Boolean);
    } else if (raw.startsWith("~")) {
      selectorText = raw.slice(1).trim();
      const siblings = webNodeSiblingsFromFrame(node);
      const index = siblings.indexOf(node);
      candidates = index < 0 ? [] : siblings.slice(index + 1);
    }
    if (!selectorText)
      return false;
    const parsed = parseSelector(selectorText);
    return candidates.some((candidate) => selectorMatchesWebNode(parsed, candidate, scopeNode));
  });
}

function selectorStructuralPseudosMatch(selector, node, scopeNode = null) {
  for (const pseudo of selector?.pseudos || []) {
    const siblings = webNodeSiblingsFromFrame(node);
    const typeSiblings = webNodeSameTypeSiblingsFromFrame(siblings, node);
    if (pseudo === "root") {
      if (webNodeParentFromFrame(node))
        return false;
    } else if (pseudo === "scope") {
      if (scopeNode) {
        if (node !== scopeNode && node?.path !== scopeNode.path)
          return false;
      } else if (webNodeParentFromFrame(node))
        return false;
    } else if (pseudo === "first-child") {
      if (siblings[0] !== node)
        return false;
    } else if (pseudo === "last-child") {
      if (siblings[siblings.length - 1] !== node)
        return false;
    } else if (pseudo === "only-child") {
      if (siblings.length !== 1 || siblings[0] !== node)
        return false;
    } else if (pseudo === "first-of-type") {
      if (typeSiblings[0] !== node)
        return false;
    } else if (pseudo === "last-of-type") {
      if (typeSiblings[typeSiblings.length - 1] !== node)
        return false;
    } else if (pseudo === "only-of-type") {
      if (typeSiblings.length !== 1 || typeSiblings[0] !== node)
        return false;
    } else if (pseudo === "empty") {
      const hasChildren = (node.__kryFrameNodes || []).some((candidate) =>
        candidate && candidate.parentPath === node.path && candidate.path !== node.path);
      if (hasChildren || String(node.text || node.domValue || "").length > 0)
        return false;
    } else if (pseudo === "focus-within") {
      if (!webNodeHasFocusWithin(node))
        return false;
    } else if (pseudo === "target") {
      if (!webNodeMatchesRouteTarget(node))
        return false;
    } else if (String(pseudo).startsWith("has(")) {
      if (!selectorHasPseudoMatches(pseudo, node, scopeNode))
        return false;
    } else if (String(pseudo).startsWith("nth-child(")) {
      if (!nthChildPseudoMatches(pseudo, siblings, node))
        return false;
    } else if (String(pseudo).startsWith("nth-last-child(")) {
      if (!nthLastChildPseudoMatches(pseudo, siblings, node))
        return false;
    } else if (String(pseudo).startsWith("nth-of-type(")) {
      if (!nthOfTypePseudoMatches(pseudo, siblings, node))
        return false;
    } else if (String(pseudo).startsWith("nth-last-of-type(")) {
      if (!nthLastOfTypePseudoMatches(pseudo, siblings, node))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

function selectorMatchesSimpleWebNode(selector, node, scopeNode = null) {
  const facts = { ...(node?.styleFacts || {}), ...webNodeStyleFacts(node) };
  if (!selectorMatchesFacts(selector, facts) ||
      !selectorStructuralPseudosMatch(selector, node, scopeNode))
    return false;
  if ((selector.not || []).some((notSelector) =>
      selectorMatchesSimpleWebNode(notSelector, node, scopeNode)))
    return false;
  if (selector.matches?.length &&
      !selector.matches.some((matchSelector) =>
        selectorMatchesSimpleWebNode(matchSelector, node, scopeNode)))
    return false;
  return true;
}

function selectorChainMatchesWebNode(selector, node, scopeNode = null) {
  const parts = selector?.parts || [];
  if (!parts.length)
    return false;
  let current = node;
  for (let index = parts.length - 1; index >= 0; index--) {
    const part = parts[index];
    if (!current || !selectorMatchesSimpleWebNode(part, current, scopeNode))
      return false;
    if (index === 0)
      return true;
    const relation = parts[index].combinator || " ";
    if (relation === ">") {
      current = webNodeParentFromFrame(current);
      continue;
    }
    if (relation === "+") {
      current = webNodePreviousSiblingsFromFrame(current).pop() || null;
      continue;
    }
    if (relation === "~") {
      const siblingSelector = parts[index - 1];
      const sibling = webNodePreviousSiblingsFromFrame(current)
        .reverse()
        .find((candidate) => selectorMatchesSimpleWebNode(siblingSelector, candidate, scopeNode));
      if (!sibling)
        return false;
      current = sibling;
      continue;
    }
    let ancestor = webNodeParentFromFrame(current);
    const ancestorSelector = parts[index - 1];
    while (ancestor && !selectorMatchesSimpleWebNode(ancestorSelector, ancestor, scopeNode))
      ancestor = webNodeParentFromFrame(ancestor);
    if (!ancestor)
      return false;
    current = ancestor;
  }
  return true;
}

function selectorMatchesWebNode(selector, node, scopeNode = null) {
  if (Array.isArray(selector?.parts) && selector.parts.length)
    return selectorChainMatchesWebNode(selector, node, scopeNode);
  return selectorMatchesSimpleWebNode(selector, node, scopeNode);
}

export function resolveWebStyle(node, sheets = []) {
  const resolved = {};
  const scores = {};
  const list = Array.isArray(sheets) ? sheets : [sheets];
  for (const sheet of list) {
    const rules = typeof sheet === "string" ? parseWebStyleSheet(sheet).rules : (sheet?.rules || []);
    for (const rule of rules) {
      if (!selectorMatchesWebNode(rule.selector, node))
        continue;
      const score = rule.score ?? ((rule.layer || 0) * 1000000 + (rule.selector?.specificity || 0) * 1000 + (rule.order || 0));
      for (const [name, value] of Object.entries(rule.style || {})) {
        if (scores[name] === undefined || score >= scores[name]) {
          scores[name] = score;
          resolved[name] = value;
        }
      }
    }
  }
  return resolved;
}

export function traceWebStyle(node, sheets = []) {
  const resolved = {};
  const winners = {};
  const matchedRules = [];
  const list = Array.isArray(sheets) ? sheets : [sheets];
  for (const sheet of list) {
    const parsed = typeof sheet === "string" ? parseWebStyleSheet(sheet) : sheet;
    const rules = parsed?.rules || [];
    for (const rule of rules) {
      if (!selectorMatchesWebNode(rule.selector, node))
        continue;
      const score = rule.score ?? ((rule.layer || 0) * 1000000 + (rule.selector?.specificity || 0) * 1000 + (rule.order || 0));
      const selector = webStyleSelectorToCSS(rule.selector);
      matchedRules.push({
        selector,
        layer: rule.layer || 0,
        order: rule.order || 0,
        specificity: rule.selector?.specificity || 0,
        score,
        style: { ...(rule.style || {}) },
        pack: parsed?.pack || ""
      });
      for (const [name, value] of Object.entries(rule.style || {})) {
        if (!winners[name] || score >= winners[name].score) {
          resolved[name] = value;
          winners[name] = {
            value,
            selector,
            layer: rule.layer || 0,
            order: rule.order || 0,
            specificity: rule.selector?.specificity || 0,
            score,
            pack: parsed?.pack || ""
          };
        }
      }
    }
  }
  return {
    facts: webNodeStyleFacts(node),
    matchedRules,
    resolved,
    winners
  };
}

function applyResolvedWebStyle(el, style) {
  if (!el)
    return;
  for (const name of el.__kryAppliedStyleProps || [])
    el.style[name] = "";
  const applied = new Set();
  const set = (name, value) => {
    if (value === undefined || value === null || value === "")
      return;
    el.style[name] = webStyleCSSValue(name, value);
    applied.add(name);
  };
  style = style || {};
  for (const [name, value] of Object.entries(style)) {
    if (name.startsWith("--"))
      set(name, value);
  }
  const backgroundStart = style.background;
  const backgroundEnd = style["background-end"];
  const offsetX = style["offset-x"];
  const offsetY = style["offset-y"];
  set("background", backgroundStart);
  set("backgroundColor", style["background-color"]);
  set("--kry-background-end", backgroundEnd);
  if (backgroundStart !== undefined && backgroundStart !== null && backgroundStart !== "" &&
      backgroundEnd !== undefined && backgroundEnd !== null && backgroundEnd !== "")
    set("backgroundImage", `linear-gradient(${webStyleCSSValue("background", backgroundStart)}, ${webStyleCSSValue("background", backgroundEnd)})`);
  set("backgroundImage", style["background-image"]);
  set("color", style.foreground);
  set("color", style.color);
  set("accentColor", style["accent-color"]);
  set("caretColor", style["caret-color"]);
  if (isWebBorderShorthandValue(style.border))
    set("border", style.border);
  else
    set("borderColor", style.border);
  set("borderColor", style["border-color"]);
  set("borderWidth", style["border-width"]);
  set("borderTopWidth", style["border-top-width"]);
  set("borderRightWidth", style["border-right-width"]);
  set("borderBottomWidth", style["border-bottom-width"]);
  set("borderLeftWidth", style["border-left-width"]);
  set("borderTopColor", style["border-top-color"]);
  set("borderRightColor", style["border-right-color"]);
  set("borderBottomColor", style["border-bottom-color"]);
  set("borderLeftColor", style["border-left-color"]);
  set("borderInlineColor", style["border-inline-color"]);
  set("borderBlockColor", style["border-block-color"]);
  set("borderInlineStartColor", style["border-inline-start-color"]);
  set("borderInlineEndColor", style["border-inline-end-color"]);
  set("borderBlockStartColor", style["border-block-start-color"]);
  set("borderBlockEndColor", style["border-block-end-color"]);
  set("borderInlineWidth", style["border-inline-width"]);
  set("borderBlockWidth", style["border-block-width"]);
  set("borderInlineStartWidth", style["border-inline-start-width"]);
  set("borderInlineEndWidth", style["border-inline-end-width"]);
  set("borderBlockStartWidth", style["border-block-start-width"]);
  set("borderBlockEndWidth", style["border-block-end-width"]);
  set("borderTop", style["border-top"]);
  set("borderRight", style["border-right"]);
  set("borderBottom", style["border-bottom"]);
  set("borderLeft", style["border-left"]);
  set("borderInline", style["border-inline"]);
  set("borderBlock", style["border-block"]);
  set("borderInlineStart", style["border-inline-start"]);
  set("borderInlineEnd", style["border-inline-end"]);
  set("borderBlockStart", style["border-block-start"]);
  set("borderBlockEnd", style["border-block-end"]);
  set("borderImage", style["border-image"]);
  set("borderImageSource", style["border-image-source"]);
  set("borderImageSlice", style["border-image-slice"]);
  set("borderImageWidth", style["border-image-width"]);
  set("borderImageOutset", style["border-image-outset"]);
  set("borderImageRepeat", style["border-image-repeat"]);
  set("borderStyle", style["border-style"]);
  set("borderTopStyle", style["border-top-style"]);
  set("borderRightStyle", style["border-right-style"]);
  set("borderBottomStyle", style["border-bottom-style"]);
  set("borderLeftStyle", style["border-left-style"]);
  set("borderInlineStyle", style["border-inline-style"]);
  set("borderBlockStyle", style["border-block-style"]);
  set("borderInlineStartStyle", style["border-inline-start-style"]);
  set("borderInlineEndStyle", style["border-inline-end-style"]);
  set("borderBlockStartStyle", style["border-block-start-style"]);
  set("borderBlockEndStyle", style["border-block-end-style"]);
  set("borderRadius", style.radius);
  set("borderRadius", style["border-radius"]);
  set("borderTopLeftRadius", style["border-top-left-radius"]);
  set("borderTopRightRadius", style["border-top-right-radius"]);
  set("borderBottomRightRadius", style["border-bottom-right-radius"]);
  set("borderBottomLeftRadius", style["border-bottom-left-radius"]);
  set("borderStartStartRadius", style["border-start-start-radius"]);
  set("borderStartEndRadius", style["border-start-end-radius"]);
  set("borderEndStartRadius", style["border-end-start-radius"]);
  set("borderEndEndRadius", style["border-end-end-radius"]);
  set("opacity", style.opacity);
  set("padding", style.padding);
  set("paddingLeft", style["padding-x"]);
  set("paddingRight", style["padding-x"]);
  set("paddingTop", style["padding-y"]);
  set("paddingBottom", style["padding-y"]);
  set("paddingLeft", style["padding-left"]);
  set("paddingRight", style["padding-right"]);
  set("paddingTop", style["padding-top"]);
  set("paddingBottom", style["padding-bottom"]);
  set("paddingInline", style["padding-inline"]);
  set("paddingBlock", style["padding-block"]);
  set("paddingInlineStart", style["padding-inline-start"]);
  set("paddingInlineEnd", style["padding-inline-end"]);
  set("paddingBlockStart", style["padding-block-start"]);
  set("paddingBlockEnd", style["padding-block-end"]);
  set("margin", style.margin);
  set("marginLeft", style["margin-x"]);
  set("marginRight", style["margin-x"]);
  set("marginTop", style["margin-y"]);
  set("marginBottom", style["margin-y"]);
  set("marginLeft", style["margin-left"]);
  set("marginRight", style["margin-right"]);
  set("marginTop", style["margin-top"]);
  set("marginBottom", style["margin-bottom"]);
  set("marginInline", style["margin-inline"]);
  set("marginBlock", style["margin-block"]);
  set("marginInlineStart", style["margin-inline-start"]);
  set("marginInlineEnd", style["margin-inline-end"]);
  set("marginBlockStart", style["margin-block-start"]);
  set("marginBlockEnd", style["margin-block-end"]);
  set("width", style.width);
  set("height", style.height);
  set("minWidth", style["min-width"]);
  set("maxWidth", style["max-width"]);
  set("minHeight", style["min-height"]);
  set("maxHeight", style["max-height"]);
  set("inlineSize", style["inline-size"]);
  set("blockSize", style["block-size"]);
  set("minInlineSize", style["min-inline-size"]);
  set("maxInlineSize", style["max-inline-size"]);
  set("minBlockSize", style["min-block-size"]);
  set("maxBlockSize", style["max-block-size"]);
  set("inset", style.inset);
  set("top", style.top);
  set("right", style.right);
  set("bottom", style.bottom);
  set("left", style.left);
  set("insetInline", style["inset-inline"]);
  set("insetBlock", style["inset-block"]);
  set("insetInlineStart", style["inset-inline-start"]);
  set("insetInlineEnd", style["inset-inline-end"]);
  set("insetBlockStart", style["inset-block-start"]);
  set("insetBlockEnd", style["inset-block-end"]);
  set("gap", style.gap);
  set("rowGap", style["row-gap"]);
  set("columnGap", style["column-gap"]);
  set("font", style.font);
  set("fontSize", style["font-size"]);
  set("fontFamily", style.typeface);
  set("fontFamily", style["font-family"]);
  set("fontWeight", style["font-weight"]);
  set("fontStyle", style["font-style"]);
  set("fontVariant", style["font-variant"]);
  set("fontStretch", style["font-stretch"]);
  set("fontKerning", style["font-kerning"]);
  set("fontOpticalSizing", style["font-optical-sizing"]);
  set("fontFeatureSettings", style["font-feature-settings"]);
  set("fontVariationSettings", style["font-variation-settings"]);
  set("fontSizeAdjust", style["font-size-adjust"]);
  set("fontSynthesis", style["font-synthesis"]);
  set("fontSynthesisWeight", style["font-synthesis-weight"]);
  set("fontSynthesisStyle", style["font-synthesis-style"]);
  set("fontSynthesisSmallCaps", style["font-synthesis-small-caps"]);
  set("fontSynthesisPosition", style["font-synthesis-position"]);
  set("fontVariantAlternates", style["font-variant-alternates"]);
  set("fontVariantCaps", style["font-variant-caps"]);
  set("fontVariantEastAsian", style["font-variant-east-asian"]);
  set("fontVariantLigatures", style["font-variant-ligatures"]);
  set("fontVariantNumeric", style["font-variant-numeric"]);
  set("fontVariantPosition", style["font-variant-position"]);
  set("fontLanguageOverride", style["font-language-override"]);
  set("fontPalette", style["font-palette"]);
  set("letterSpacing", style["letter-spacing"]);
  set("lineHeight", style["line-height"]);
  set("textIndent", style["text-indent"]);
  set("textAlign", style["text-align"]);
  set("textAlignLast", style["text-align-last"]);
  set("textRendering", style["text-rendering"]);
  set("textDecoration", style["text-decoration"]);
  set("textDecorationLine", style["text-decoration-line"]);
  set("textDecorationColor", style["text-decoration-color"]);
  set("textDecorationStyle", style["text-decoration-style"]);
  set("textDecorationSkip", style["text-decoration-skip"]);
  set("textDecorationSkipInk", style["text-decoration-skip-ink"]);
  set("textDecorationThickness", style["text-decoration-thickness"]);
  set("textUnderlineOffset", style["text-underline-offset"]);
  set("textUnderlinePosition", style["text-underline-position"]);
  set("textShadow", style["text-shadow"]);
  set("textEmphasis", style["text-emphasis"]);
  set("textEmphasisColor", style["text-emphasis-color"]);
  set("textEmphasisStyle", style["text-emphasis-style"]);
  set("textEmphasisPosition", style["text-emphasis-position"]);
  set("textTransform", style["text-transform"]);
  set("textOverflow", style["text-overflow"]);
  set("whiteSpace", style["white-space"]);
  set("textSizeAdjust", style["text-size-adjust"]);
  set("textOrientation", style["text-orientation"]);
  set("textWrap", style["text-wrap"]);
  set("textWrapMode", style["text-wrap-mode"]);
  set("textWrapStyle", style["text-wrap-style"]);
  set("textJustify", style["text-justify"]);
  set("textCombineUpright", style["text-combine-upright"]);
  set("rubyAlign", style["ruby-align"]);
  set("rubyPosition", style["ruby-position"]);
  set("textSpacingTrim", style["text-spacing-trim"]);
  set("textAutospace", style["text-autospace"]);
  set("textBoxTrim", style["text-box-trim"]);
  set("textBoxEdge", style["text-box-edge"]);
  set("wordBreak", style["word-break"]);
  set("overflowWrap", style["overflow-wrap"]);
  set("wordWrap", style["word-wrap"]);
  set("lineBreak", style["line-break"]);
  set("hangingPunctuation", style["hanging-punctuation"]);
  set("verticalAlign", style["vertical-align"]);
  set("display", style.display);
  set("position", style.position);
  set("zIndex", style["z-index"]);
  set("overflow", style.overflow);
  set("overflowInline", style["overflow-inline"]);
  set("overflowBlock", style["overflow-block"]);
  set("overflowX", style["overflow-x"]);
  set("overflowY", style["overflow-y"]);
  set("boxSizing", style["box-sizing"]);
  set("direction", style.direction);
  set("writingMode", style["writing-mode"]);
  set("tabSize", style["tab-size"]);
  set("hyphens", style.hyphens);
  set("lineClamp", style["line-clamp"]);
  set("webkitLineClamp", style["line-clamp"]);
  set("listStyle", style["list-style"]);
  set("listStyleType", style["list-style-type"]);
  set("listStylePosition", style["list-style-position"]);
  set("listStyleImage", style["list-style-image"]);
  set("counterReset", style["counter-reset"]);
  set("counterIncrement", style["counter-increment"]);
  set("counterSet", style["counter-set"]);
  set("quotes", style.quotes);
  set("markerSide", style["marker-side"]);
  set("markerStart", style["marker-start"]);
  set("markerEnd", style["marker-end"]);
  set("orphans", style.orphans);
  set("widows", style.widows);
  set("boxDecorationBreak", style["box-decoration-break"]);
  set("webkitBoxDecorationBreak", style["box-decoration-break"]);
  set("borderCollapse", style["border-collapse"]);
  set("borderSpacing", style["border-spacing"]);
  set("tableLayout", style["table-layout"]);
  set("captionSide", style["caption-side"]);
  set("emptyCells", style["empty-cells"]);
  set("scrollBehavior", style["scroll-behavior"]);
  set("overscrollBehavior", style["overscroll-behavior"]);
  set("overscrollBehaviorX", style["overscroll-behavior-x"]);
  set("overscrollBehaviorY", style["overscroll-behavior-y"]);
  set("overscrollBehaviorInline", style["overscroll-behavior-inline"]);
  set("overscrollBehaviorBlock", style["overscroll-behavior-block"]);
  set("scrollSnapType", style["scroll-snap-type"]);
  set("scrollSnapAlign", style["scroll-snap-align"]);
  set("scrollSnapStop", style["scroll-snap-stop"]);
  set("scrollTimeline", style["scroll-timeline"]);
  set("scrollTimelineName", style["scroll-timeline-name"]);
  set("scrollTimelineAxis", style["scroll-timeline-axis"]);
  set("viewTimeline", style["view-timeline"]);
  set("viewTimelineName", style["view-timeline-name"]);
  set("viewTimelineAxis", style["view-timeline-axis"]);
  set("viewTimelineInset", style["view-timeline-inset"]);
  set("timelineScope", style["timeline-scope"]);
  set("scrollbarColor", style["scrollbar-color"]);
  set("scrollbarWidth", style["scrollbar-width"]);
  set("scrollbarGutter", style["scrollbar-gutter"]);
  set("overflowClipMargin", style["overflow-clip-margin"]);
  set("scrollMargin", style["scroll-margin"]);
  set("scrollMarginTop", style["scroll-margin-top"]);
  set("scrollMarginRight", style["scroll-margin-right"]);
  set("scrollMarginBottom", style["scroll-margin-bottom"]);
  set("scrollMarginLeft", style["scroll-margin-left"]);
  set("scrollMarginInline", style["scroll-margin-inline"]);
  set("scrollMarginBlock", style["scroll-margin-block"]);
  set("scrollMarginInlineStart", style["scroll-margin-inline-start"]);
  set("scrollMarginInlineEnd", style["scroll-margin-inline-end"]);
  set("scrollMarginBlockStart", style["scroll-margin-block-start"]);
  set("scrollMarginBlockEnd", style["scroll-margin-block-end"]);
  set("scrollPadding", style["scroll-padding"]);
  set("scrollPaddingTop", style["scroll-padding-top"]);
  set("scrollPaddingRight", style["scroll-padding-right"]);
  set("scrollPaddingBottom", style["scroll-padding-bottom"]);
  set("scrollPaddingLeft", style["scroll-padding-left"]);
  set("scrollPaddingInline", style["scroll-padding-inline"]);
  set("scrollPaddingBlock", style["scroll-padding-block"]);
  set("scrollPaddingInlineStart", style["scroll-padding-inline-start"]);
  set("scrollPaddingInlineEnd", style["scroll-padding-inline-end"]);
  set("scrollPaddingBlockStart", style["scroll-padding-block-start"]);
  set("scrollPaddingBlockEnd", style["scroll-padding-block-end"]);
  set("touchAction", style["touch-action"]);
  set("alignItems", style["align-items"]);
  set("justifyContent", style["justify-content"]);
  set("alignSelf", style["align-self"]);
  set("justifySelf", style["justify-self"]);
  set("flexDirection", style["flex-direction"]);
  set("flexWrap", style["flex-wrap"]);
  set("flex", style.flex);
  set("flexGrow", style["flex-grow"]);
  set("flexShrink", style["flex-shrink"]);
  set("flexBasis", style["flex-basis"]);
  set("gridTemplateColumns", style["grid-template-columns"]);
  set("gridTemplateRows", style["grid-template-rows"]);
  set("gridTemplateAreas", style["grid-template-areas"]);
  set("gridAutoColumns", style["grid-auto-columns"]);
  set("gridAutoRows", style["grid-auto-rows"]);
  set("gridAutoFlow", style["grid-auto-flow"]);
  set("gridColumn", style["grid-column"]);
  set("gridColumnStart", style["grid-column-start"]);
  set("gridColumnEnd", style["grid-column-end"]);
  set("gridArea", style["grid-area"]);
  set("gridRow", style["grid-row"]);
  set("gridRowStart", style["grid-row-start"]);
  set("gridRowEnd", style["grid-row-end"]);
  set("alignContent", style["align-content"]);
  set("justifyItems", style["justify-items"]);
  set("placeItems", style["place-items"]);
  set("placeContent", style["place-content"]);
  set("placeSelf", style["place-self"]);
  set("objectFit", style["object-fit"]);
  set("objectPosition", style["object-position"]);
  set("objectViewBox", style["object-view-box"]);
  set("aspectRatio", style["aspect-ratio"]);
  set("imageRendering", style["image-rendering"]);
  set("imageOrientation", style["image-orientation"]);
  set("imageResolution", style["image-resolution"]);
  set("backgroundImage", style["background-image"]);
  set("backgroundSize", style["background-size"]);
  set("backgroundPosition", style["background-position"]);
  set("backgroundPositionX", style["background-position-x"]);
  set("backgroundPositionY", style["background-position-y"]);
  set("backgroundRepeat", style["background-repeat"]);
  set("backgroundRepeatX", style["background-repeat-x"]);
  set("backgroundRepeatY", style["background-repeat-y"]);
  set("backgroundClip", style["background-clip"]);
  set("backgroundOrigin", style["background-origin"]);
  set("backgroundAttachment", style["background-attachment"]);
  set("backgroundBlendMode", style["background-blend-mode"]);
  set("visibility", style.visibility);
  set("transition", style.transition);
  set("transitionProperty", style["transition-property"]);
  set("transitionDuration", style["transition-duration"]);
  set("transitionTimingFunction", style["transition-timing-function"]);
  set("transitionDelay", style["transition-delay"]);
  set("transitionBehavior", style["transition-behavior"]);
  set("animation", style.animation);
  set("animationName", style["animation-name"]);
  set("animationDuration", style["animation-duration"]);
  set("animationTimingFunction", style["animation-timing-function"]);
  set("animationDelay", style["animation-delay"]);
  set("animationIterationCount", style["animation-iteration-count"]);
  set("animationDirection", style["animation-direction"]);
  set("animationFillMode", style["animation-fill-mode"]);
  set("animationPlayState", style["animation-play-state"]);
  set("animationComposition", style["animation-composition"]);
  set("animationTimeline", style["animation-timeline"]);
  set("animationRange", style["animation-range"]);
  set("animationRangeStart", style["animation-range-start"]);
  set("animationRangeEnd", style["animation-range-end"]);
  if (!((offsetX !== undefined && offsetX !== null && offsetX !== "") ||
        (offsetY !== undefined && offsetY !== null && offsetY !== "")))
    set("transform", style.transform);
  set("transformOrigin", style["transform-origin"]);
  set("transformBox", style["transform-box"]);
  set("transformStyle", style["transform-style"]);
  set("translate", style.translate);
  set("rotate", style.rotate);
  set("scale", style.scale);
  set("perspective", style.perspective);
  set("perspectiveOrigin", style["perspective-origin"]);
  set("backfaceVisibility", style["backface-visibility"]);
  set("offsetPath", style["offset-path"]);
  set("offsetDistance", style["offset-distance"]);
  set("offsetRotate", style["offset-rotate"]);
  set("offsetAnchor", style["offset-anchor"]);
  set("offsetPosition", style["offset-position"]);
  set("filter", style.filter);
  set("backdropFilter", style["backdrop-filter"]);
  set("clipPath", style["clip-path"]);
  set("mask", style.mask);
  set("maskImage", style["mask-image"]);
  set("maskSize", style["mask-size"]);
  set("maskPosition", style["mask-position"]);
  set("maskRepeat", style["mask-repeat"]);
  set("maskOrigin", style["mask-origin"]);
  set("maskClip", style["mask-clip"]);
  set("maskComposite", style["mask-composite"]);
  set("maskMode", style["mask-mode"]);
  set("cursor", style.cursor);
  set("pointerEvents", style["pointer-events"]);
  set("appearance", style.appearance);
  set("userSelect", style["user-select"]);
  set("resize", style.resize);
  set("fieldSizing", style["field-sizing"]);
  set("interpolateSize", style["interpolate-size"]);
  set("overlay", style.overlay);
  set("outline", style.outline);
  set("outlineWidth", style["outline-width"]);
  set("outlineOffset", style["outline-offset"]);
  set("outlineStyle", style["outline-style"]);
  set("outlineColor", style["outline-color"]);
  set("boxShadow", style["box-shadow"]);
  set("colorScheme", style["color-scheme"]);
  set("forcedColorAdjust", style["forced-color-adjust"]);
  set("printColorAdjust", style["print-color-adjust"]);
  set("colorInterpolation", style["color-interpolation"]);
  set("colorInterpolationFilters", style["color-interpolation-filters"]);
  set("paintOrder", style["paint-order"]);
  set("shapeOutside", style["shape-outside"]);
  set("shapeMargin", style["shape-margin"]);
  set("shapeImageThreshold", style["shape-image-threshold"]);
  set("contain", style.contain);
  set("contentVisibility", style["content-visibility"]);
  set("containIntrinsicSize", style["contain-intrinsic-size"]);
  set("containIntrinsicWidth", style["contain-intrinsic-width"]);
  set("containIntrinsicHeight", style["contain-intrinsic-height"]);
  set("containIntrinsicInlineSize", style["contain-intrinsic-inline-size"]);
  set("containIntrinsicBlockSize", style["contain-intrinsic-block-size"]);
  set("container", style.container);
  set("containerType", style["container-type"]);
  set("containerName", style["container-name"]);
  set("anchorName", style["anchor-name"]);
  set("positionAnchor", style["position-anchor"]);
  set("positionArea", style["position-area"]);
  set("positionTry", style["position-try"]);
  set("positionTryFallbacks", style["position-try-fallbacks"]);
  set("positionTryOrder", style["position-try-order"]);
  set("positionVisibility", style["position-visibility"]);
  set("willChange", style["will-change"]);
  set("viewTransitionName", style["view-transition-name"]);
  set("isolation", style.isolation);
  set("mixBlendMode", style["mix-blend-mode"]);
  set("columns", style.columns);
  set("columnCount", style["column-count"]);
  set("columnWidth", style["column-width"]);
  set("columnFill", style["column-fill"]);
  set("columnSpan", style["column-span"]);
  set("columnRule", style["column-rule"]);
  set("columnRuleColor", style["column-rule-color"]);
  set("columnRuleStyle", style["column-rule-style"]);
  set("columnRuleWidth", style["column-rule-width"]);
  set("breakBefore", style["break-before"]);
  set("breakAfter", style["break-after"]);
  set("breakInside", style["break-inside"]);
  set("float", style.float);
  set("clear", style.clear);
  set("order", style.order);
  set("--kry-content-offset-x", style["content-offset-x"]);
  set("--kry-content-offset-y", style["content-offset-y"]);
  set("--kry-icon-size", style["icon-size"]);
  if ((offsetX !== undefined && offsetX !== null && offsetX !== "") ||
      (offsetY !== undefined && offsetY !== null && offsetY !== "")) {
    set("--kry-offset-x", offsetX ?? "0px");
    set("--kry-offset-y", offsetY ?? "0px");
    const transform = style.transform ? ` ${webStyleCSSValue("transform", style.transform)}` : "";
    set("transform", `translate(var(--kry-offset-x, 0px), var(--kry-offset-y, 0px))${transform}`);
  }
  set("outlineColor", style["outline-color"] ?? style.focus);
  if (style.border || style["border-width"]) {
    el.style.borderStyle = el.style.borderStyle || "solid";
    applied.add("borderStyle");
  }
  el.__kryAppliedStyleProps = applied;
  applyWebInlineStyles(el);
}

function setStyleProperty(style, name, value) {
  if (style && typeof style.setProperty === "function")
    style.setProperty(name, value);
  else if (style)
    style[name] = value;
}

function removeStyleProperty(style, name) {
  if (style && typeof style.removeProperty === "function")
    style.removeProperty(name);
  else if (style)
    style[name] = "";
}

function getStyleProperty(style, name) {
  if (!style)
    return "";
  if (typeof style.getPropertyValue === "function") {
    const value = style.getPropertyValue(name);
    if (value !== undefined && value !== null && value !== "")
      return String(value);
  }
  return style[name] === undefined || style[name] === null ? "" : String(style[name]);
}

function cleanDOMStyleName(name) {
  const value = String(name || "").trim();
  return value && /^(--[A-Za-z0-9_-]+|[A-Za-z][A-Za-z0-9_-]*)$/.test(value) ? value : "";
}

function applyWebInlineStyles(el) {
  if (!el)
    return;
  const styles = el.__kryInlineStyles || {};
  for (const [name, value] of Object.entries(styles))
    setStyleProperty(el.style, name, value);
}

export function setWebStyleSheets(rt, sheets) {
  if (!rt)
    return rt;
  rt.webStyleSheets = Array.isArray(sheets) ? sheets : [sheets];
  return rt;
}

export function webDocumentFrame(rt) {
  const nodes = (rt?.frame || []).map(webNodeFromWidget);
  normalizeWebDocumentNodes(nodes);
  const frame = {
    app: rt?.app || null,
    nodes
  };
  frame.metadata = webDocumentMetadata(frame, rt?.webStyleSheets || []);
  return frame;
}

function cleanWebPathSegment(value, fallback = "node") {
  const text = String(value || "").trim()
    .replace(/[^A-Za-z0-9_.:-]+/g, "_")
    .replace(/^_+|_+$/g, "");
  return text || fallback;
}

function webNodeFallbackPath(node, index, parentPath) {
  const suffix = node.sourceLine
    ? `${node.kind}@${node.sourceLine}-${index}`
    : `${node.kind}@${index}`;
  const segment = cleanWebPathSegment(node.name || suffix, suffix);
  return parentPath ? `${parentPath}/${segment}` : segment;
}

function normalizeWebDocumentNodes(nodes) {
  let rootPath = "";
  let lastParentPath = "";
  for (const [index, node] of nodes.entries()) {
    if (!node)
      continue;
    const hadPath = !!node.path;
    const parentPath = node.parentPath || (!hadPath ? lastParentPath || rootPath || "" : "");
    if (!hadPath)
      node.path = webNodeFallbackPath(node, index, parentPath);
    if (!hadPath && !node.parentPath && parentPath && parentPath !== node.path)
      node.parentPath = parentPath;
    if (!node.key)
      node.key = node.name || cleanWebPathSegment(node.path.split("/").pop(), String(index));
    if (!node.name && node.key && !/^\d+$/.test(String(node.key)))
      node.name = String(node.key);
    try {
      Object.defineProperty(node, "__kryFrameNodes", {
        configurable: true,
        enumerable: false,
        value: nodes
      });
    } catch {
      node.__kryFrameNodes = nodes;
    }
    if (!node.parentPath || node.parentPath === node.path)
      rootPath = node.path || rootPath;
    else
      lastParentPath = node.parentPath;
  }
  const byPath = new Map(nodes.filter(Boolean).map((node) => [node.path, node]));
  for (const node of nodes) {
    const parent = byPath.get(node?.parentPath || "");
    if (node?.kind === "Selectable" && !node.__kryAuthoredTag &&
        (parent?.kind === "Dropdown" || parent?.kind === "ListBox"))
      node.tag = "option";
    if (node?.kind === "Selectable" && node.tag === "div" &&
        !node.__kryAuthoredTag &&
        (parent?.kind === "Menu" || parent?.kind === "TabBar" ||
         parent?.kind === "TreeView"))
      node.tag = "button";
    if (!node?.role && parent?.kind === "Menu" &&
        (node?.kind === "Button" || node?.kind === "Selectable"))
      node.role = "menuitem";
    if (!node?.role && parent?.kind === "TabBar" &&
        (node?.kind === "Button" || node?.kind === "Selectable"))
      node.role = "tab";
    if (!node?.role && parent?.kind === "TreeView" &&
        (node?.kind === "Button" || node?.kind === "Selectable"))
      node.role = "treeitem";
    node.styleFacts = webNodeStyleFacts(node);
  }
}

function webDocumentMetadata(frame, sheets = []) {
  const page = (frame.nodes || []).find((node) => node.kind === "Page");
  const pageStyle = page ? resolveWebStyle(page, sheets) : {};
  const pageThemeColor = pageStyle.background
    ? webStyleCSSValue("background", pageStyle.background)
    : "";
  return {
    title: pageMeta.title || page?.pageTitle || frame.app?.title || "",
    description: pageMeta.description || page?.pageDescription || "",
    canonicalURL: pageMeta.canonicalURL || page?.pageCanonicalURL || "",
    themeColor: pageMeta.themeColor || pageThemeColor
  };
}

function removeAttr(el, name) {
  if (el && typeof el.removeAttribute === "function")
    el.removeAttribute(name);
}

function setAttr(el, name, value) {
  if (value === undefined || value === null || value === false || value === "") {
    removeAttr(el, name);
    return;
  }
  el.setAttribute(name, value === true ? "" : String(value));
}

function documentHead() {
  if (typeof document === "undefined")
    return null;
  return document.head || document.documentElement || document.body || null;
}

function ensureDocumentMeta(name) {
  if (typeof document === "undefined")
    return null;
  const selector = name === "canonical"
    ? 'link[rel="canonical"]'
    : `meta[name="${name}"]`;
  let el = typeof document.querySelector === "function"
    ? document.querySelector(selector)
    : null;
  if (!el) {
    el = document.createElement(name === "canonical" ? "link" : "meta");
    if (name === "canonical")
      el.setAttribute("rel", "canonical");
    else
      el.setAttribute("name", name);
    const head = documentHead();
    if (head)
      head.appendChild(el);
  }
  return el;
}

function applyDocumentMetadata(metadata) {
  if (typeof document === "undefined" || !metadata)
    return;
  if (metadata.title && document.title !== undefined)
    document.title = metadata.title;
  const description = ensureDocumentMeta("description");
  if (description)
    setAttr(description, "content", metadata.description);
  const canonical = ensureDocumentMeta("canonical");
  if (canonical)
    setAttr(canonical, "href", metadata.canonicalURL);
  const themeColor = ensureDocumentMeta("theme-color");
  if (themeColor)
    setAttr(themeColor, "content", metadata.themeColor);
}

function bindNodeEvents(el) {
  if (el.__kryClickBound)
    return;
  el.__kryClickBound = true;
  const interactiveState = (changes) => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    Object.assign(docNode.state, changes);
    updateWebElementStateDataset(el, docNode.state);
    docNode.styleFacts = webNodeStyleFacts(docNode);
    applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
      ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
      : null);
  };
  el.addEventListener("mouseenter", () => {
    interactiveState({ hover: true });
    const docNode = el.__kryDocNode;
    if (docNode?.mouseEnterAction)
      docNode.mouseEnterAction();
  });
  el.addEventListener("mouseleave", () => {
    interactiveState({ hover: false, pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.mouseLeaveAction)
      docNode.mouseLeaveAction();
  });
  el.addEventListener("mousemove", () => {
    const docNode = el.__kryDocNode;
    if (docNode?.mouseMoveAction)
      docNode.mouseMoveAction();
  });
  el.addEventListener("mousedown", () => {
    interactiveState({ pressed: true });
    const docNode = el.__kryDocNode;
    if (docNode?.mouseDownAction)
      docNode.mouseDownAction();
  });
  el.addEventListener("mouseup", () => {
    interactiveState({ pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.mouseUpAction)
      docNode.mouseUpAction();
  });
  el.addEventListener("pointerenter", () => {
    interactiveState({ hover: true });
    const docNode = el.__kryDocNode;
    if (docNode?.pointerEnterAction)
      docNode.pointerEnterAction();
  });
  el.addEventListener("pointerleave", () => {
    interactiveState({ hover: false, pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.pointerLeaveAction)
      docNode.pointerLeaveAction();
  });
  el.addEventListener("pointermove", () => {
    const docNode = el.__kryDocNode;
    if (docNode?.pointerMoveAction)
      docNode.pointerMoveAction();
  });
  el.addEventListener("pointerdown", () => {
    interactiveState({ pressed: true });
    const docNode = el.__kryDocNode;
    if (docNode?.pointerDownAction)
      docNode.pointerDownAction();
  });
  el.addEventListener("pointerup", () => {
    interactiveState({ pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.pointerUpAction)
      docNode.pointerUpAction();
  });
  el.addEventListener("pointercancel", () => {
    interactiveState({ pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.pointerCancelAction)
      docNode.pointerCancelAction();
  });
  el.addEventListener("wheel", (event) => {
    const docNode = el.__kryDocNode;
    const value = Number.isFinite(Number(event?.deltaY)) ? Number(event.deltaY) :
      Number.isFinite(Number(event?.wheelDelta)) ? -Number(event.wheelDelta) : 0;
    if (docNode?.wheelAction)
      docNode.wheelAction(value);
  });
  el.addEventListener("contextmenu", (event) => {
    const docNode = el.__kryDocNode;
    if (docNode?.contextMenuAction) {
      if (event?.preventDefault)
        event.preventDefault();
      docNode.contextMenuAction();
    }
  });
  el.addEventListener("focus", () => {
    interactiveState({ focus: true });
    const docNode = el.__kryDocNode;
    if (docNode?.focusAction)
      docNode.focusAction();
  });
  el.addEventListener("blur", () => {
    interactiveState({ focus: false, pressed: false });
    const docNode = el.__kryDocNode;
    if (docNode?.blurAction)
      docNode.blurAction();
  });
  el.addEventListener("scroll", () => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    docNode.scrollLeft = Number(el.scrollLeft) || 0;
    docNode.scrollTop = Number(el.scrollTop) || 0;
    docNode.styleFacts = webNodeStyleFacts(docNode);
    applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
      ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
      : null);
    if (docNode.scrollAction)
      docNode.scrollAction(docNode.scrollTop);
  });
  el.addEventListener("click", () => {
    const docNode = el.__kryDocNode;
    const rt = el.__kryRuntime;
    if (!docNode)
      return;
    if (rt?.QueueTap) {
      const x = docNode.bounds.x + Math.max(1, docNode.bounds.width) * 0.5;
      const y = docNode.bounds.y + Math.max(1, docNode.bounds.height) * 0.5;
      rt.QueueTap(x, y);
    }
    if (docNode.action)
      docNode.action();
  });
  el.addEventListener("dblclick", () => {
    const docNode = el.__kryDocNode;
    if (docNode?.doubleClickAction)
      docNode.doubleClickAction();
  });
  el.addEventListener("dragstart", (event) => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = webDragValue(el, docNode);
    if (event?.dataTransfer?.setData)
      event.dataTransfer.setData("text/plain", String(value));
    if (docNode.dragStartAction)
      docNode.dragStartAction(value);
  });
  el.addEventListener("dragend", () => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = webDragValue(el, docNode);
    if (docNode.dragEndAction)
      docNode.dragEndAction(value);
  });
  el.addEventListener("dragover", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    if (docNode?.dragOverAction)
      docNode.dragOverAction();
  });
  el.addEventListener("drop", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = event?.dataTransfer?.getData
      ? event.dataTransfer.getData("text/plain")
      : updateElementFormValue(el);
    if (docNode.dropAction)
      docNode.dropAction(value);
  });
  el.addEventListener("copy", (event) => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = webDragValue(el, docNode);
    if (event?.clipboardData?.setData)
      event.clipboardData.setData("text/plain", String(value));
    if (docNode.copyAction)
      docNode.copyAction(value);
  });
  el.addEventListener("cut", (event) => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = webDragValue(el, docNode);
    if (event?.clipboardData?.setData)
      event.clipboardData.setData("text/plain", String(value));
    if (docNode.cutAction)
      docNode.cutAction(value);
  });
  el.addEventListener("paste", (event) => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    const value = event?.clipboardData?.getData
      ? event.clipboardData.getData("text/plain")
      : "";
    if (docNode.pasteAction)
      docNode.pasteAction(value);
  });
  el.addEventListener("input", () => {
    const docNode = el.__kryDocNode;
    const value = updateElementFormValue(el);
    if (docNode?.inputAction)
      docNode.inputAction(value);
  });
  el.addEventListener("beforeinput", (event) => {
    const docNode = el.__kryDocNode;
    const value = event?.data === undefined || event?.data === null
      ? String(event?.inputType || "")
      : String(event.data);
    if (docNode?.beforeInputAction)
      docNode.beforeInputAction(value);
  });
  el.addEventListener("change", () => {
    const docNode = el.__kryDocNode;
    const value = el.type === "checkbox" || el.type === "radio"
      ? !!el.checked
      : (el.value ?? "");
    updateElementFormValue(el, value);
    if (docNode?.changeAction)
      docNode.changeAction(value);
  });
  el.addEventListener("select", () => {
    const docNode = el.__kryDocNode;
    const value = webElementSelection(el);
    if (docNode?.selectAction)
      docNode.selectAction(value);
  });
  el.addEventListener("keydown", (event) => {
    const docNode = el.__kryDocNode;
    const key = event?.key ?? event?.code ?? "";
    if (docNode?.keyAction)
      docNode.keyAction(String(key));
  });
  el.addEventListener("keyup", (event) => {
    const docNode = el.__kryDocNode;
    const key = event?.key ?? event?.code ?? "";
    if (docNode?.keyUpAction)
      docNode.keyUpAction(String(key));
  });
  el.addEventListener("invalid", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    const value = updateElementFormValue(el);
    if (docNode?.invalidAction)
      docNode.invalidAction(value);
  });
  el.addEventListener("submit", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    if (docNode?.submitAction)
      docNode.submitAction(webFormValuesForNode(el.__kryMountRoot, docNode));
  });
  el.addEventListener("reset", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    if (docNode?.resetAction)
      docNode.resetAction(webFormValuesForNode(el.__kryMountRoot, docNode));
  });
  el.addEventListener("toggle", () => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    interactiveState({ open: !!el.open || !!el.popoverOpen });
    if (docNode.toggleAction)
      docNode.toggleAction();
  });
  el.addEventListener("close", () => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    interactiveState({ open: false });
    if (docNode.closeAction)
      docNode.closeAction();
  });
  el.addEventListener("cancel", () => {
    const docNode = el.__kryDocNode;
    if (!docNode)
      return;
    if (docNode.cancelAction)
      docNode.cancelAction();
  });
}

function webElementValue(el, docNode = el?.__kryDocNode) {
  if (!el || !docNode)
    return undefined;
  if (docNode.tag === "input") {
    if (docNode.inputType === "checkbox" || docNode.inputType === "radio")
      return !!el.checked;
    return el.value ?? "";
  }
  if (docNode.tag === "textarea")
    return el.value ?? "";
  return undefined;
}

function webElementSelection(el) {
  if (!el || typeof el.value !== "string")
    return "";
  const start = Number.isFinite(Number(el.selectionStart))
    ? Math.max(0, Number(el.selectionStart))
    : 0;
  const end = Number.isFinite(Number(el.selectionEnd))
    ? Math.max(start, Number(el.selectionEnd))
    : start;
  return el.value.slice(start, end);
}

function recordFormValue(root, docNode, value) {
  if (!root || !docNode || value === undefined)
    return;
  if (!root.__kryFormValues)
    root.__kryFormValues = new Map();
  for (const key of [webNodeRef(docNode), docNode.path, docNode.name,
                     docNode.key, docNode.domId, docNode.domName]) {
    if (key)
      root.__kryFormValues.set(key, value);
  }
}

function collectFormValue(out, docNode, value) {
  if (!out || !docNode || value === undefined)
    return;
  for (const key of [webNodeRef(docNode), docNode.path, docNode.name,
                     docNode.key, docNode.domId, docNode.domName]) {
    if (key && out[key] === undefined)
      out[key] = value;
  }
}

function updateElementFormValue(el, value = webElementValue(el)) {
  const docNode = el?.__kryDocNode;
  if (!docNode || value === undefined)
    return value;
  docNode.value = value;
  if (docNode.inputType === "checkbox" || docNode.inputType === "radio")
    docNode.state.checked = !!value;
  recordFormValue(el.__kryMountRoot, docNode, value);
  return value;
}

function webNodeBelongsToForm(root, node, formNode) {
  if (!root || !node || !formNode || node === formNode)
    return false;
  const formPath = formNode.path || "";
  if (formPath && node.parentPath &&
      (node.parentPath === formPath || node.parentPath.startsWith(formPath + "/")))
    return true;
  const owner = webDOMRelationList(root, node.formOwner)[0] || null;
  return !!owner && (owner.node === formNode || owner.ref === webNodeRef(formNode));
}

function webFormValuesForNode(root, formNode) {
  if (!root || !formNode)
    return webFormValuesFromRoot(root);
  const out = {};
  for (const el of root.__kryChildren?.values?.() || []) {
    const node = el.__kryDocNode || null;
    if (!webNodeBelongsToForm(root, node, formNode))
      continue;
    collectFormValue(out, node, updateElementFormValue(el));
  }
  return out;
}

function webDragValue(el, docNode = el?.__kryDocNode) {
  const formValue = webElementValue(el, docNode);
  if (docNode?.domValue)
    return docNode.domValue;
  if (formValue !== undefined)
    return formValue;
  if (docNode?.value !== undefined && docNode?.value !== null && docNode.value !== "")
    return docNode.value;
  return webNodeRef(docNode);
}

function webNodeRef(docNode) {
  return docNode?.webRef || docNode?.path || docNode?.name || docNode?.key ||
    docNode?.domId || "";
}

function webNodeSourceRef(docNode) {
  return docNode?.sourcePath && docNode?.sourceLine
    ? `${docNode.sourcePath}:${docNode.sourceLine}`
    : "";
}

function webNodeSourceColumnRef(docNode) {
  return docNode?.sourcePath && docNode?.sourceLine && docNode?.sourceColumn
    ? `${docNode.sourcePath}:${docNode.sourceLine}:${docNode.sourceColumn}`
    : "";
}

function webNodeSourceRangeRef(docNode) {
  if (!docNode?.sourcePath || !docNode?.sourceLine || !docNode?.sourceColumn ||
      !docNode?.sourceEndLine || !docNode?.sourceEndColumn)
    return "";
  return `${docNode.sourcePath}:${docNode.sourceLine}:${docNode.sourceColumn}-${docNode.sourceEndLine}:${docNode.sourceEndColumn}`;
}

export function webSourceRef(sourcePath, sourceLine, sourceColumn = 0) {
  const path = String(sourcePath || "").trim();
  const line = Number.isFinite(Number(sourceLine)) ? Math.trunc(Number(sourceLine)) : 0;
  const column = Number.isFinite(Number(sourceColumn)) ? Math.trunc(Number(sourceColumn)) : 0;
  if (!path || line <= 0)
    return "";
  return column > 0 ? `${path}:${line}:${column}` : `${path}:${line}`;
}

export function webSourceRangeRef(sourcePath, sourceLine, sourceColumn,
                                  sourceEndLine, sourceEndColumn) {
  const start = webSourceRef(sourcePath, sourceLine, sourceColumn);
  const endLine = Number.isFinite(Number(sourceEndLine)) ? Math.trunc(Number(sourceEndLine)) : 0;
  const endColumn = Number.isFinite(Number(sourceEndColumn)) ? Math.trunc(Number(sourceEndColumn)) : 0;
  if (!start || endLine <= 0 || endColumn <= 0)
    return "";
  return `${start}-${endLine}:${endColumn}`;
}

function webNodeHasSource(node) {
  return !!(node?.sourcePath && node?.sourceLine);
}

function sourcePositionWithinNode(node, sourcePath, line, column = 0) {
  if (!webNodeHasSource(node) || node.sourcePath !== sourcePath || line <= 0)
    return false;
  const startLine = node.sourceLine || 0;
  const startColumn = node.sourceColumn || 0;
  const endLine = node.sourceEndLine || startLine;
  const endColumn = node.sourceEndColumn || startColumn;
  if(line < startLine || line > endLine)
    return false;
  if(column > 0 && line === startLine && startColumn > 0 && column < startColumn)
    return false;
  if(column > 0 && line === endLine && endColumn > 0 && column > endColumn)
    return false;
  return true;
}

function compareSourcePositions(aLine, aColumn, bLine, bColumn) {
  const lineDelta = (aLine || 0) - (bLine || 0);
  if (lineDelta !== 0)
    return lineDelta;
  if (aColumn <= 0 || bColumn <= 0)
    return 0;
  return aColumn - bColumn;
}

function normalizeSourceRange(sourceLine, sourceColumn, sourceEndLine, sourceEndColumn) {
  let startLine = Number.isFinite(Number(sourceLine)) ? Math.trunc(Number(sourceLine)) : 0;
  let startColumn = Number.isFinite(Number(sourceColumn)) ? Math.trunc(Number(sourceColumn)) : 0;
  let endLine = Number.isFinite(Number(sourceEndLine)) ? Math.trunc(Number(sourceEndLine)) : 0;
  let endColumn = Number.isFinite(Number(sourceEndColumn)) ? Math.trunc(Number(sourceEndColumn)) : 0;
  if (startLine <= 0)
    return null;
  if (endLine <= 0)
    endLine = startLine;
  if (compareSourcePositions(endLine, endColumn, startLine, startColumn) < 0) {
    const swapLine = startLine;
    const swapColumn = startColumn;
    startLine = endLine;
    startColumn = endColumn;
    endLine = swapLine;
    endColumn = swapColumn;
  }
  return { startLine, startColumn, endLine, endColumn };
}

function sourceRangeOverlapsNode(node, sourcePath, sourceLine, sourceColumn,
                                 sourceEndLine, sourceEndColumn) {
  if (!webNodeHasSource(node) || node.sourcePath !== sourcePath)
    return false;
  const range = normalizeSourceRange(sourceLine, sourceColumn, sourceEndLine, sourceEndColumn);
  if (!range)
    return false;
  const nodeStartLine = node.sourceLine || 0;
  const nodeStartColumn = node.sourceColumn || 0;
  const nodeEndLine = node.sourceEndLine || nodeStartLine;
  const nodeEndColumn = node.sourceEndColumn || nodeStartColumn;
  if (compareSourcePositions(nodeEndLine, nodeEndColumn,
      range.startLine, range.startColumn) < 0)
    return false;
  if (compareSourcePositions(nodeStartLine, nodeStartColumn,
      range.endLine, range.endColumn) > 0)
    return false;
  return true;
}

function webNodePathDepth(node) {
  return String(node?.path || "").split("/").filter(Boolean).length;
}

function compareWebNodesDeepestFirst(a, b) {
  return webNodePathDepth(b) - webNodePathDepth(a) ||
    (b.sourceLine || 0) - (a.sourceLine || 0) ||
    (b.sourceColumn || 0) - (a.sourceColumn || 0);
}

function sortWebNodesDeepestFirst(nodes) {
  return [...(nodes || [])].sort(compareWebNodesDeepestFirst);
}

function compareWebDOMObjectsDeepestFirst(a, b) {
  return compareWebNodesDeepestFirst(a?.node, b?.node);
}

function sortWebDOMObjectsDeepestFirst(objects) {
  return [...(objects || [])].sort(compareWebDOMObjectsDeepestFirst);
}

function sourceRefMatches(docNode, query) {
  const text = String(query || "");
  return webNodeSourceRef(docNode) === text || webNodeSourceColumnRef(docNode) === text;
}

function pushIndex(map, key, value) {
  if (!key)
    return;
  const items = map.get(key) || [];
  items.push(value);
  map.set(key, items);
}

function applyDataAttrs(el, attrs) {
  const previous = el.__kryDataAttrs || new Set();
  const next = new Set();
  for (const [name, value] of Object.entries(attrs || {})) {
    const attr = "data-" + name;
    next.add(attr);
    setAttr(el, attr, value);
  }
  for (const attr of previous) {
    if (!next.has(attr))
      el.removeAttribute(attr);
  }
  el.__kryDataAttrs = next;
}

function applyAriaAttrs(el, attrs) {
  const previous = el.__kryAriaAttrs || new Set();
  const next = new Set();
  for (const [name, value] of Object.entries(attrs || {})) {
    const attr = "aria-" + name;
    next.add(attr);
    setAttr(el, attr, value);
  }
  for (const attr of previous) {
    if (!next.has(attr))
      el.removeAttribute(attr);
  }
  el.__kryAriaAttrs = next;
}

function applyExtraAttrs(el, attrs) {
  const previous = el.__kryAppliedExtraAttrs || new Set();
  const next = new Set();
  for (const [name, value] of Object.entries(attrs || {})) {
    const attr = String(name || "").trim();
    if (!attr)
      continue;
    next.add(attr);
    setAttr(el, attr, value);
  }
  for (const attr of previous) {
    if (!next.has(attr))
      el.removeAttribute(attr);
  }
  el.__kryAppliedExtraAttrs = next;
}

function bindWebDOMObjectProperties(el) {
  if (!el || el.__kryObjectPropertiesBound)
    return;
  Object.defineProperties(el, {
    kryRef: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeRef(this.__kryDocNode);
      }
    },
    kryPath: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.path || "";
      }
    },
    kryAliases: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeIdentity(this.__kryDocNode).aliases;
      }
    },
    kryIndex: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.index || 0;
      }
    },
    kryKind: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.kind || "";
      }
    },
    kryTag: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.tag || String(this.tagName || "").toLowerCase();
      }
    },
    kryName: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.name || "";
      }
    },
    kryKey: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.key || "";
      }
    },
    krySourceRef: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeSourceRef(this.__kryDocNode);
      }
    },
    krySourceColumnRef: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeSourceColumnRef(this.__kryDocNode);
      }
    },
    krySourceRangeRef: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeSourceRangeRef(this.__kryDocNode);
      }
    },
    krySourcePath: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.sourcePath || "";
      }
    },
    krySourceLine: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.sourceLine || 0;
      }
    },
    krySourceColumn: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.sourceColumn || 0;
      }
    },
    krySourceEndLine: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.sourceEndLine || 0;
      }
    },
    krySourceEndColumn: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode?.sourceEndColumn || 0;
      }
    },
    kryNode: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode || null;
      }
    },
    kryRoot: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryMountRoot || mountedRoot(this);
      }
    },
    kryObject: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        return makeWebDOMObject(this.__kryMountRoot || mountedRoot(this), node, this);
      }
    },
    kryIdentity: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode ? webNodeIdentity(this.__kryDocNode) : null;
      }
    },
    krySnapshot: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMObjectSnapshot(root, makeWebDOMObject(root, node, this)) : null;
      }
    },
    kryStyleFacts: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode ? webNodeStyleFacts(this.__kryDocNode) : null;
      }
    },
    kryStyleTrace: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMStyleTrace(root, webNodeRef(node)) : null;
      }
    },
    kryParent: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMParent(root, node.path) : null;
      }
    },
    kryAncestors: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMAncestors(root, node.path) : [];
      }
    },
    kryPreviousSibling: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMPreviousSibling(root, node.path) : null;
      }
    },
    kryNextSibling: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMNextSibling(root, node.path) : null;
      }
    },
    kryPreviousSiblings: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMPreviousSiblings(root, node.path) : [];
      }
    },
    kryNextSiblings: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMNextSiblings(root, node.path) : [];
      }
    },
    krySiblings: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMSiblings(root, node.path) : [];
      }
    },
    kryChildren: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMChildren(root, node.path) : [];
      }
    },
    kryRelations: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMRelationsForNode(root, node) : null;
      }
    },
    kryRelationRefs: {
      configurable: true,
      enumerable: false,
      get() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMRelationRefs(root, webNodeRef(node)) : null;
      }
    },
    kryEventRefs: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryDocNode ? webNodeEventRefs(this.__kryDocNode) : null;
      }
    },
    kryDescendants: {
      configurable: true,
      enumerable: false,
      value() {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMDescendants(root, node.path) : [];
      }
    },
    kryQuery: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMQueryWithin(root, node.path, selector) : null;
      }
    },
    kryQueryAll: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const node = this.__kryDocNode || null;
        const root = this.__kryMountRoot || mountedRoot(this);
        return node && root ? webDOMQueryAllWithin(root, node.path, selector) : [];
      }
    },
    kryMatches: {
      configurable: true,
      enumerable: false,
      value(selector) {
        return webDOMElementMatches(this, selector);
      }
    },
    kryClosest: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const object = webDOMObjectFromElement(this);
        const root = object?.element?.__kryMountRoot || mountedRoot(object?.element || null);
        return object && root ? webDOMClosest(root, object.node.path, selector) : null;
      }
    },
    krySourceMap: {
      configurable: true,
      enumerable: false,
      get() {
        return webDOMSourceMap(this);
      }
    },
    kryListen: {
      configurable: true,
      enumerable: false,
      value(type, handler, options) {
        const eventType = cleanDOMEventType(type);
        if (!eventType || typeof handler !== "function" ||
            typeof this.addEventListener !== "function")
          return null;
        const el = this;
        const listener = (event) => {
          bindWebDOMEventProperties(event);
          return handler(event, webDOMObjectFromElement(event?.target || el) ||
            webDOMObjectFromElement(el));
        };
        el.addEventListener(eventType, listener, options);
        return () => {
          if (typeof el.removeEventListener === "function")
            el.removeEventListener(eventType, listener, options);
        };
      }
    },
    kryAddClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMAddClass(root, query, className);
      }
    },
    kryRemoveClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMRemoveClass(root, query, className);
      }
    },
    kryToggleClass: {
      configurable: true,
      enumerable: false,
      value(className, force) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMToggleClass(root, query, className, force);
      }
    },
    kryHasClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMHasClass(root, query, className);
      }
    },
    kryGetAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMGetAttribute(root, query, name) : undefined;
      }
    },
    krySetAttr: {
      configurable: true,
      enumerable: false,
      value(name, value = "") {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMSetAttribute(root, query, name, value);
      }
    },
    kryRemoveAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMRemoveAttribute(root, query, name);
      }
    },
    kryHasAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMHasAttribute(root, query, name);
      }
    },
    kryGetProp: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMGetProperty(root, query, name) : undefined;
      }
    },
    krySetProp: {
      configurable: true,
      enumerable: false,
      value(name, value) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMSetProperty(root, query, name, value);
      }
    },
    kryGetStyle: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMGetStyle(root, query, name) : undefined;
      }
    },
    krySetStyle: {
      configurable: true,
      enumerable: false,
      value(name, value = "") {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMSetStyle(root, query, name, value);
      }
    },
    kryRemoveStyle: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMRemoveStyle(root, query, name);
      }
    },
    kryComputedStyle: {
      configurable: true,
      enumerable: false,
      value(name = "") {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMComputedStyle(root, query, name) : undefined;
      }
    },
    kryGetState: {
      configurable: true,
      enumerable: false,
      value(name) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMGetState(root, query, name) : undefined;
      }
    },
    krySetState: {
      configurable: true,
      enumerable: false,
      value(name, value) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMSetState(root, query, name, value);
      }
    },
    kryText: {
      configurable: true,
      enumerable: false,
      value(text) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        if (!root || !query)
          return text === undefined ? undefined : false;
        return text === undefined ? webDOMGetText(root, query) : webDOMSetText(root, query, text);
      }
    },
    kryValue: {
      configurable: true,
      enumerable: false,
      value(value) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        if (!root || !query)
          return value === undefined ? undefined : false;
        return value === undefined ? webDOMGetValue(root, query) : webDOMSetValue(root, query, value);
      }
    },
    kryDispatch: {
      configurable: true,
      enumerable: false,
      value(type, init = {}) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMDispatchEvent(root, query, type, init);
      }
    },
    kryClick: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMClick(root, query);
      }
    },
    kryFocus: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMFocus(root, query);
      }
    },
    kryBlur: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMBlur(root, query);
      }
    },
    krySubmit: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMSubmit(root, query);
      }
    },
    kryReset: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMReset(root, query);
      }
    },
    kryRect: {
      configurable: true,
      enumerable: false,
      value() {
        return webDOMRectFromElement(this);
      }
    },
    kryScroll: {
      configurable: true,
      enumerable: false,
      value(left, top = null) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        if (!root || !query)
          return left === undefined ? null : false;
        return left === undefined
          ? webDOMGetScroll(root, query)
          : webDOMSetScroll(root, query, left, top);
      }
    },
    kryScrollIntoView: {
      configurable: true,
      enumerable: false,
      value(options = true) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMScrollIntoView(root, query, options);
      }
    },
    kryShowModal: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMShowModal(root, query);
      }
    },
    kryClose: {
      configurable: true,
      enumerable: false,
      value(returnValue = "") {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMClose(root, query, returnValue);
      }
    },
    kryShowPopover: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMShowPopover(root, query);
      }
    },
    kryHidePopover: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMHidePopover(root, query);
      }
    },
    kryTogglePopover: {
      configurable: true,
      enumerable: false,
      value(force) {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return !!root && !!query && webDOMTogglePopover(root, query, force);
      }
    },
    krySync: {
      configurable: true,
      enumerable: false,
      value() {
        const root = this.__kryMountRoot || mountedRoot(this);
        const query = webNodeRef(this.__kryDocNode);
        return root && query ? webDOMSync(root, query) : null;
      }
    }
  });
  el.__kryObjectPropertiesBound = true;
}

function webDOMObjectRoot(object) {
  return object?.root || object?.element?.__kryMountRoot ||
    mountedRoot(object?.element || null);
}

function webDOMObjectQuery(object) {
  return object?.ref || webNodeRef(object?.node) || object?.node?.path || "";
}

function makeWebDOMObject(root, node, element, ref = "") {
  if (!node || !element)
    return null;
  const object = {
    ref: ref || webNodeRef(node),
    node,
    element
  };
  Object.defineProperties(object, {
    root: {
      configurable: true,
      enumerable: false,
      get() {
        return root || element.__kryMountRoot || mountedRoot(element) || null;
      }
    },
    identity: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeIdentity(this.node);
      }
    },
    snapshot: {
      configurable: true,
      enumerable: false,
      get() {
        return webDOMObjectSnapshot(webDOMObjectRoot(this), this);
      }
    },
    parent: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMParent(target, webDOMObjectQuery(this)) : null;
      }
    },
    ancestors: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMAncestors(target, webDOMObjectQuery(this)) : [];
      }
    },
    previousSibling: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMPreviousSibling(target, webDOMObjectQuery(this)) : null;
      }
    },
    nextSibling: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMNextSibling(target, webDOMObjectQuery(this)) : null;
      }
    },
    previousSiblings: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMPreviousSiblings(target, webDOMObjectQuery(this)) : [];
      }
    },
    nextSiblings: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMNextSiblings(target, webDOMObjectQuery(this)) : [];
      }
    },
    siblings: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMSiblings(target, webDOMObjectQuery(this)) : [];
      }
    },
    children: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMChildren(target, webDOMObjectQuery(this)) : [];
      }
    },
    relations: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMRelationsForNode(target, this.node) : null;
      }
    },
    relationRefs: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMRelationRefs(target, webDOMObjectQuery(this)) : null;
      }
    },
    eventRefs: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeEventRefs(this.node);
      }
    },
    styleFacts: {
      configurable: true,
      enumerable: false,
      get() {
        return webNodeStyleFacts(this.node);
      }
    },
    styleTrace: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMStyleTrace(target, webDOMObjectQuery(this)) : null;
      }
    },
    descendants: {
      configurable: true,
      enumerable: false,
      get() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMDescendants(target, webDOMObjectQuery(this)) : [];
      }
    },
    query: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMQueryWithin(target, webDOMObjectQuery(this), selector) : null;
      }
    },
    queryAll: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMQueryAllWithin(target, webDOMObjectQuery(this), selector) : [];
      }
    },
    matches: {
      configurable: true,
      enumerable: false,
      value(selector) {
        return selectorMatchesWebNode(parseSelector(String(selector || "").trim()), this.node);
      }
    },
    closest: {
      configurable: true,
      enumerable: false,
      value(selector) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMClosest(target, webDOMObjectQuery(this), selector) : null;
      }
    },
    listen: {
      configurable: true,
      enumerable: false,
      value(type, handler, options) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMAddEventListener(target, webDOMObjectQuery(this), type, handler, options) : null;
      }
    },
    addClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMAddClass(target, webDOMObjectQuery(this), className);
      }
    },
    removeClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMRemoveClass(target, webDOMObjectQuery(this), className);
      }
    },
    toggleClass: {
      configurable: true,
      enumerable: false,
      value(className, force) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMToggleClass(target, webDOMObjectQuery(this), className, force);
      }
    },
    hasClass: {
      configurable: true,
      enumerable: false,
      value(className) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMHasClass(target, webDOMObjectQuery(this), className);
      }
    },
    getAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMGetAttribute(target, webDOMObjectQuery(this), name) : undefined;
      }
    },
    setAttr: {
      configurable: true,
      enumerable: false,
      value(name, value = "") {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMSetAttribute(target, webDOMObjectQuery(this), name, value);
      }
    },
    removeAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMRemoveAttribute(target, webDOMObjectQuery(this), name);
      }
    },
    hasAttr: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMHasAttribute(target, webDOMObjectQuery(this), name);
      }
    },
    getProp: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMGetProperty(target, webDOMObjectQuery(this), name) : undefined;
      }
    },
    setProp: {
      configurable: true,
      enumerable: false,
      value(name, value) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMSetProperty(target, webDOMObjectQuery(this), name, value);
      }
    },
    getStyle: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMGetStyle(target, webDOMObjectQuery(this), name) : undefined;
      }
    },
    setStyle: {
      configurable: true,
      enumerable: false,
      value(name, value = "") {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMSetStyle(target, webDOMObjectQuery(this), name, value);
      }
    },
    removeStyle: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMRemoveStyle(target, webDOMObjectQuery(this), name);
      }
    },
    computedStyle: {
      configurable: true,
      enumerable: false,
      value(name = "") {
        const target = webDOMObjectRoot(this);
        return target ? webDOMComputedStyle(target, webDOMObjectQuery(this), name) : undefined;
      }
    },
    getState: {
      configurable: true,
      enumerable: false,
      value(name) {
        const target = webDOMObjectRoot(this);
        return target ? webDOMGetState(target, webDOMObjectQuery(this), name) : undefined;
      }
    },
    setState: {
      configurable: true,
      enumerable: false,
      value(name, value) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMSetState(target, webDOMObjectQuery(this), name, value);
      }
    },
    text: {
      configurable: true,
      enumerable: false,
      value(text) {
        const target = webDOMObjectRoot(this);
        if (!target)
          return text === undefined ? undefined : false;
        return text === undefined
          ? webDOMGetText(target, webDOMObjectQuery(this))
          : webDOMSetText(target, webDOMObjectQuery(this), text);
      }
    },
    value: {
      configurable: true,
      enumerable: false,
      value(value) {
        const target = webDOMObjectRoot(this);
        if (!target)
          return value === undefined ? undefined : false;
        return value === undefined
          ? webDOMGetValue(target, webDOMObjectQuery(this))
          : webDOMSetValue(target, webDOMObjectQuery(this), value);
      }
    },
    dispatch: {
      configurable: true,
      enumerable: false,
      value(type, init = {}) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMDispatchEvent(target, webDOMObjectQuery(this), type, init);
      }
    },
    click: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMClick(target, webDOMObjectQuery(this));
      }
    },
    focus: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMFocus(target, webDOMObjectQuery(this));
      }
    },
    blur: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMBlur(target, webDOMObjectQuery(this));
      }
    },
    submit: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMSubmit(target, webDOMObjectQuery(this));
      }
    },
    reset: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMReset(target, webDOMObjectQuery(this));
      }
    },
    rect: {
      configurable: true,
      enumerable: false,
      value() {
        return webDOMRectFromElement(this.element);
      }
    },
    scroll: {
      configurable: true,
      enumerable: false,
      value(left, top = null) {
        const target = webDOMObjectRoot(this);
        if (!target)
          return left === undefined ? null : false;
        return left === undefined
          ? webDOMGetScroll(target, webDOMObjectQuery(this))
          : webDOMSetScroll(target, webDOMObjectQuery(this), left, top);
      }
    },
    scrollIntoView: {
      configurable: true,
      enumerable: false,
      value(options = true) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMScrollIntoView(target, webDOMObjectQuery(this), options);
      }
    },
    showModal: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMShowModal(target, webDOMObjectQuery(this));
      }
    },
    close: {
      configurable: true,
      enumerable: false,
      value(returnValue = "") {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMClose(target, webDOMObjectQuery(this), returnValue);
      }
    },
    showPopover: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMShowPopover(target, webDOMObjectQuery(this));
      }
    },
    hidePopover: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMHidePopover(target, webDOMObjectQuery(this));
      }
    },
    togglePopover: {
      configurable: true,
      enumerable: false,
      value(force) {
        const target = webDOMObjectRoot(this);
        return !!target && webDOMTogglePopover(target, webDOMObjectQuery(this), force);
      }
    },
    sync: {
      configurable: true,
      enumerable: false,
      value() {
        const target = webDOMObjectRoot(this);
        return target ? webDOMSync(target, webDOMObjectQuery(this)) : null;
      }
    }
  });
  return object;
}

function updateWebElementStateDataset(el, state) {
  const activeStates = Object.entries(state || {})
    .filter((entry) => entry[1])
    .map((entry) => entry[0]);
  if (activeStates.length)
    el.dataset.kryState = activeStates.join(" ");
  else
    delete el.dataset.kryState;
}

function webDOMGeneratedId(object) {
  const raw = object?.ref || object?.node?.path || object?.node?.name || object?.node?.key || "node";
  const text = String(raw).replace(/[^A-Za-z0-9_-]+/g, "-").replace(/^-+|-+$/g, "");
  return "kry-" + (text || "node");
}

function ensureWebDOMElementId(root, object) {
  const el = object?.element || null;
  if (!root || !el)
    return "";
  const current = el.id || el.attributes?.id || "";
  if (current) {
    if (el.__kryDocNode)
      el.__kryDocNode.domId = String(current);
    root.__kryElementsByDomId?.set(String(current), el);
    return String(current);
  }
  const base = webDOMGeneratedId(object);
  let id = base;
  let index = 2;
  while (root.__kryElementsByDomId?.has(id) && root.__kryElementsByDomId.get(id) !== el)
    id = `${base}-${index++}`;
  setAttr(el, "id", id);
  if (el.__kryDocNode)
    el.__kryDocNode.domId = id;
  root.__kryElementsByDomId?.set(id, el);
  return id;
}

function resolveWebDOMRelationToken(root, token) {
  const text = String(token || "").trim();
  if (!text)
    return "";
  const object = webDOMObject(root, text);
  if (object)
    return ensureWebDOMElementId(root, object);
  for (const el of root?.__kryChildren?.values?.() || []) {
    const node = el.__kryDocNode || null;
    if (node && webNodeIdentity(node).aliases.includes(text))
      return ensureWebDOMElementId(root, makeWebDOMObject(root, node, el, text));
  }
  return text;
}

function resolveWebDOMRelationList(root, value) {
  return String(value || "")
    .split(/\s+/)
    .map((token) => resolveWebDOMRelationToken(root, token))
    .filter(Boolean)
    .join(" ");
}

function resolveWebDOMUseMapToken(root, token) {
  const text = String(token || "").trim();
  if (!text)
    return "";
  const object = webDOMObject(root, text);
  const mapObject = object || (() => {
    for (const el of root?.__kryChildren?.values?.() || []) {
      const node = el.__kryDocNode || null;
      if (node && webNodeIdentity(node).aliases.includes(text))
        return makeWebDOMObject(root, node, el, text);
    }
    return null;
  })();
  if (!mapObject)
    return text.startsWith("#") ? text : "#" + text;
  const el = mapObject.element || null;
  const current = el?.getAttribute?.("name") || el?.attributes?.name ||
    mapObject.node?.domName || "";
  if (current)
    return "#" + String(current).replace(/^#/, "");
  const id = ensureWebDOMElementId(root, mapObject);
  setAttr(el, "name", id);
  if (mapObject.node)
    mapObject.node.domName = id;
  root?.__kryElementsByDomName?.set?.(id, el);
  return id ? "#" + id : "";
}

function resolveWebDOMRelations(root) {
  if (!root)
    return;
  for (const el of root.__kryChildren?.values?.() || []) {
    const docNode = el.__kryDocNode || null;
    if (!docNode)
      continue;
    setAttr(el, "aria-describedby", resolveWebDOMRelationList(root, docNode.ariaDescribedBy));
    setAttr(el, "aria-details", resolveWebDOMRelationToken(root, docNode.ariaDetails));
    setAttr(el, "aria-errormessage", resolveWebDOMRelationToken(root, docNode.ariaErrorMessage));
    setAttr(el, "aria-flowto", resolveWebDOMRelationList(root, docNode.ariaFlowTo));
    setAttr(el, "aria-labelledby", resolveWebDOMRelationList(root, docNode.ariaLabelledBy));
    setAttr(el, "aria-activedescendant", resolveWebDOMRelationToken(root, docNode.ariaActiveDescendant));
    setAttr(el, "aria-controls", resolveWebDOMRelationList(root, docNode.ariaControls));
    setAttr(el, "aria-owns", resolveWebDOMRelationList(root, docNode.ariaOwns));
    if (docNode.headers)
      setAttr(el, "headers", resolveWebDOMRelationList(root, docNode.headers));
    else if (!docNode.extraAttrs?.headers)
      removeAttr(el, "headers");
    setAttr(el, "for", String(docNode.tag || "").toLowerCase() === "output"
      ? resolveWebDOMRelationList(root, docNode.htmlFor)
      : resolveWebDOMRelationToken(root, docNode.htmlFor));
    setAttr(el, "list", resolveWebDOMRelationToken(root, docNode.dataList));
    setAttr(el, "usemap", resolveWebDOMUseMapToken(root, docNode.useMap));
    setAttr(el, "form", resolveWebDOMRelationToken(root, docNode.formOwner));
    setAttr(el, "popovertarget", resolveWebDOMRelationToken(root, docNode.popoverTarget));
  }
  syncWebDOMRootIndexes(root);
}

function webDOMRelationList(target, value) {
  const root = mountedRoot(target);
  const text = String(value || "").trim();
  if (!root || !text)
    return [];
  return text
    .split(/\s+/)
    .map((token) => webDOMObject(root, token))
    .filter(Boolean);
}

function webDOMReverseRelationList(target, node, field) {
  const root = mountedRoot(target);
  const ref = webNodeRef(node);
  if (!root || !node || !ref)
    return [];
  const out = [];
  for (const el of root.__kryChildren?.values?.() || []) {
    const candidate = el.__kryDocNode || null;
    if (!candidate || candidate === node)
      continue;
    const related = webDOMRelationList(root, candidate[field] || "");
    if (related.some((object) => object.node === node || object.ref === ref))
      out.push(makeWebDOMObject(root, candidate, el, webNodeRef(candidate)));
  }
  return out;
}

function webNodeIsFormControl(node) {
  return ["button", "fieldset", "input", "output", "select", "textarea"]
    .includes(String(node?.tag || "").toLowerCase());
}

function webDOMFormControls(target, node) {
  const explicit = webDOMReverseRelationList(target, node, "formOwner");
  const root = mountedRoot(target);
  if (!root || !node?.path)
    return explicit;
  const descendants = [];
  for (const el of root.__kryChildren?.values?.() || []) {
    const candidate = el.__kryDocNode || null;
    if (!candidate || candidate === node || !webNodeIsFormControl(candidate))
      continue;
    if (candidate.path?.startsWith(node.path + "/"))
      descendants.push(makeWebDOMObject(root, candidate, el, webNodeRef(candidate)));
  }
  return mergeWebDOMRelationObjects(explicit, descendants);
}

function webNodeIsLabel(node) {
  return String(node?.tag || "").toLowerCase() === "label";
}

function webNodeIsOutput(node) {
  return String(node?.tag || "").toLowerCase() === "output";
}

function webDOMImplicitLabelControl(target, node) {
  if (!webNodeIsLabel(node))
    return null;
  return webDOMDescendants(target, node.path)
    .find((object) => webNodeIsFormControl(object?.node)) || null;
}

function webDOMImplicitLabels(target, node) {
  const root = mountedRoot(target);
  if (!root || !node?.path || !webNodeIsFormControl(node))
    return [];
  const out = [];
  let parent = webDOMParent(root, node.path);
  while (parent) {
    if (webNodeIsLabel(parent.node))
      out.push(parent);
    parent = webDOMParent(root, parent.node.path);
  }
  return out;
}

function mergeWebDOMRelationObjects(...lists) {
  const out = [];
  const seen = new Set();
  for (const list of lists) {
    for (const object of list || []) {
      const key = object?.ref || webNodeRef(object?.node) || "";
      if (!object || !key || seen.has(key))
        continue;
      seen.add(key);
      out.push(object);
    }
  }
  return out;
}

function webDOMScopedHeaderList(target, node, scope) {
  const expected = new Set(Array.isArray(scope) ? scope.map((item) => String(item).toLowerCase())
    : [String(scope || "").toLowerCase()]);
  return webDOMRelationList(target, node?.headers || "")
    .filter((object) => expected.has(String(object?.node?.scope || "").toLowerCase()));
}

function webNodeIsSemanticGroup(node) {
  return !!node && (String(node.role || "").toLowerCase() === "group" ||
    implicitRole(node) === "group");
}

function webNodeIsDisabledScope(node) {
  return !!node && !!node.state?.disabled &&
    (node.kind === "Disabled" || String(node.tag || "").toLowerCase() === "fieldset" ||
      webNodeIsSemanticGroup(node));
}

function webNodeRoleName(node) {
  return String(node?.role || implicitRole(node) || "").toLowerCase();
}

const webLandmarkRoles = new Set([
  "banner", "complementary", "contentinfo", "form", "main",
  "navigation", "region", "search"
]);

function webNodeIsLandmark(node) {
  return webLandmarkRoles.has(webNodeRoleName(node));
}

function webNodeCollectionMemberRoles(node) {
  const role = webNodeRoleName(node);
  const tag = String(node?.tag || "").toLowerCase();
  switch (role) {
  case "menu":
  case "menubar":
    return new Set(["menuitem", "menuitemcheckbox", "menuitemradio"]);
  case "tablist":
    return new Set(["tab"]);
  case "tree":
    return new Set(["treeitem"]);
  case "listbox":
  case "combobox":
    return new Set(["option"]);
  case "list":
    return new Set(["listitem"]);
  default:
    break;
  }
  if (tag === "ul" || tag === "ol")
    return new Set(["listitem"]);
  if (tag === "datalist")
    return new Set(["option"]);
  return null;
}

function webNodeCanOwnCollectionMember(owner, member) {
  const roles = webNodeCollectionMemberRoles(owner);
  return !!roles && roles.has(webNodeRoleName(member));
}

function webNodeCanOwnCaption(owner, caption) {
  const ownerTag = String(owner?.tag || "").toLowerCase();
  const captionTag = String(caption?.tag || "").toLowerCase();
  return (ownerTag === "figure" && captionTag === "figcaption") ||
    (ownerTag === "table" && captionTag === "caption");
}

function webNodeCanOwnSummary(owner, summary) {
  return String(owner?.tag || "").toLowerCase() === "details" &&
    String(summary?.tag || "").toLowerCase() === "summary";
}

function webNodeCanOwnLegend(owner, legend) {
  return String(owner?.tag || "").toLowerCase() === "fieldset" &&
    String(legend?.tag || "").toLowerCase() === "legend";
}

function webNodeCanOwnDescriptionItem(owner, item) {
  const ownerTag = String(owner?.tag || "").toLowerCase();
  const itemTag = String(item?.tag || "").toLowerCase();
  return ownerTag === "dl" && (itemTag === "dt" || itemTag === "dd");
}

function webNodeCaptionOwner(rt, node) {
  if (!rt || !node)
    return null;
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeCanOwnCaption(parent, node))
      return parent;
    parent = webNodeParent(rt, parent.path);
  }
  return null;
}

function webNodeCaptionItems(rt, node) {
  if (!rt || !node)
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeCaptionOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeSummaryOwner(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() !== "summary")
    return null;
  const nodes = webFrameNodeMap(webDocumentFrame(rt));
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = nodes.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnSummary(parent, node))
      return parent;
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webNodeSummaryItems(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() !== "details")
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeSummaryOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeLegendOwner(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() !== "legend")
    return null;
  const nodes = webFrameNodeMap(webDocumentFrame(rt));
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = nodes.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnLegend(parent, node))
      return parent;
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webNodeLegendItems(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() !== "fieldset")
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node ||
        String(candidate.tag || "").toLowerCase() !== "legend")
      return false;
    const owner = webNodeLegendOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeDescriptionListOwner(rt, node) {
  const tag = String(node?.tag || "").toLowerCase();
  if (!rt || !node || (tag !== "dt" && tag !== "dd"))
    return null;
  const nodes = webFrameNodeMap(webDocumentFrame(rt));
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = nodes.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnDescriptionItem(parent, node))
      return parent;
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webNodeDescriptionListItems(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() !== "dl")
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    const tag = String(candidate?.tag || "").toLowerCase();
    if (!candidate || candidate === node || (tag !== "dt" && tag !== "dd"))
      return false;
    const owner = webNodeDescriptionListOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeIsSelectedCollectionMember(node) {
  if (!node)
    return false;
  if (node.state?.selected)
    return true;
  const ariaSelected = String(node.ariaAttrs?.selected || "").toLowerCase();
  const ariaCurrent = String(node.ariaAttrs?.current || "").toLowerCase();
  return ariaSelected === "true" || (ariaCurrent && ariaCurrent !== "false");
}

function webDOMGroupOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() === "legend")
    return null;
  let parentPath = node.parentPath || "";
  while (parentPath && parentPath !== node.path) {
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeIsSemanticGroup(parent))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMGroupMembers(target, node) {
  const root = mountedRoot(target);
  if (!root || !webNodeIsSemanticGroup(node))
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMGroupOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMDisabledOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node)
    return null;
  let parentPath = node.parentPath || "";
  while (parentPath && parentPath !== node.path) {
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeIsDisabledScope(parent)) {
      if (String(parent.tag || "").toLowerCase() === "fieldset" &&
          webNodeCanOwnLegend(parent, node)) {
        parentPath = parent.parentPath || "";
        continue;
      }
      return webDOMObjectForNode(root, parent);
    }
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMDisabledMembers(target, node) {
  const root = mountedRoot(target);
  if (!root || !webNodeIsDisabledScope(node))
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMDisabledOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMLandmarkOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node)
    return null;
  let parentPath = node.parentPath || "";
  while (parentPath && parentPath !== node.path) {
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeIsLandmark(parent))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMLandmarkMembers(target, node) {
  const root = mountedRoot(target);
  if (!root || !webNodeIsLandmark(node))
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMLandmarkOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMCollectionOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node)
    return null;
  let parentPath = node.parentPath || "";
  while (parentPath && parentPath !== node.path) {
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnCollectionMember(parent, node))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMCollectionItems(target, node) {
  const root = mountedRoot(target);
  if (!root || !webNodeCollectionMemberRoles(node))
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMCollectionOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMCaptionOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node)
    return null;
  let parentPath = node.parentPath || "";
  while (parentPath && parentPath !== node.path) {
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnCaption(parent, node))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMCaptionItems(target, node) {
  const root = mountedRoot(target);
  if (!root || !node)
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMCaptionOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMSummaryOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() !== "summary")
    return null;
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnSummary(parent, node))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMSummaryItems(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() !== "details")
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node)
      continue;
    const owner = webDOMSummaryOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMLegendOwner(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() !== "legend")
    return null;
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnLegend(parent, node))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMLegendItems(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() !== "fieldset")
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    if (!object?.node || object.node === node ||
        String(object.node.tag || "").toLowerCase() !== "legend")
      continue;
    const owner = webDOMLegendOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMDescriptionListOwner(target, node) {
  const root = mountedRoot(target);
  const tag = String(node?.tag || "").toLowerCase();
  if (!root || !node || (tag !== "dt" && tag !== "dd"))
    return null;
  const seen = new Set([node.path]);
  let parentPath = node.parentPath || "";
  while (parentPath && !seen.has(parentPath)) {
    seen.add(parentPath);
    const parent = root.__kryNodes?.get(parentPath) || null;
    if (!parent)
      break;
    if (webNodeCanOwnDescriptionItem(parent, node))
      return webDOMObjectForNode(root, parent);
    parentPath = parent.parentPath || "";
  }
  return null;
}

function webDOMDescriptionListItems(target, node) {
  const root = mountedRoot(target);
  if (!root || !node || String(node.tag || "").toLowerCase() !== "dl")
    return [];
  const ref = webNodeRef(node);
  const out = [];
  for (const object of webDOMObjects(root)) {
    const tag = String(object?.node?.tag || "").toLowerCase();
    if (!object?.node || object.node === node || (tag !== "dt" && tag !== "dd"))
      continue;
    const owner = webDOMDescriptionListOwner(root, object.node);
    if (owner?.node === node || (ref && owner?.ref === ref))
      out.push(object);
  }
  return out;
}

function webDOMSelectedCollectionOwner(target, node) {
  return webNodeIsSelectedCollectionMember(node)
    ? webDOMCollectionOwner(target, node)
    : null;
}

function webDOMSelectedCollectionItems(target, node) {
  return webDOMCollectionItems(target, node)
    .filter((object) => webNodeIsSelectedCollectionMember(object?.node));
}

function webDOMActiveCollectionOwner(target, node) {
  const owners = webDOMReverseRelationList(target, node, "ariaActiveDescendant");
  return owners.find((object) => webNodeCanOwnCollectionMember(object?.node, node)) || null;
}

function webDOMActiveCollectionItems(target, node) {
  const active = webDOMRelationList(target, node?.ariaActiveDescendant);
  return active.filter((object) => webNodeCanOwnCollectionMember(node, object?.node));
}

function webDOMSiblingObject(target, node, offset) {
  const root = mountedRoot(target);
  if (!root || !node)
    return null;
  const siblings = webDOMChildren(root, node.parentPath || "")
    .filter((object) => object?.node);
  const index = siblings.findIndex((object) => object.node === node ||
    object.node.path === node.path);
  const sibling = index < 0 ? null : siblings[index + offset] || null;
  return sibling || null;
}

function webDOMRelationsForNode(target, node) {
  if (!node)
    return null;
  return {
    previousSibling: webDOMSiblingObject(target, node, -1),
    nextSibling: webDOMSiblingObject(target, node, 1),
    groupOwner: webDOMGroupOwner(target, node),
    groupMembers: webDOMGroupMembers(target, node),
    disabledOwner: webDOMDisabledOwner(target, node),
    disabledMembers: webDOMDisabledMembers(target, node),
    landmarkOwner: webDOMLandmarkOwner(target, node),
    landmarkMembers: webDOMLandmarkMembers(target, node),
    collectionOwner: webDOMCollectionOwner(target, node),
    collectionItems: webDOMCollectionItems(target, node),
    captionOwner: webDOMCaptionOwner(target, node),
    captionItems: webDOMCaptionItems(target, node),
    summaryOwner: webDOMSummaryOwner(target, node),
    summaryItems: webDOMSummaryItems(target, node),
    legendOwner: webDOMLegendOwner(target, node),
    legendItems: webDOMLegendItems(target, node),
    descriptionListOwner: webDOMDescriptionListOwner(target, node),
    descriptionListItems: webDOMDescriptionListItems(target, node),
    selectedCollectionOwner: webDOMSelectedCollectionOwner(target, node),
    selectedCollectionItems: webDOMSelectedCollectionItems(target, node),
    activeCollectionOwner: webDOMActiveCollectionOwner(target, node),
    activeCollectionItems: webDOMActiveCollectionItems(target, node),
    describedBy: webDOMRelationList(target, node.ariaDescribedBy),
    describes: webDOMReverseRelationList(target, node, "ariaDescribedBy"),
    details: webDOMRelationList(target, node.ariaDetails)[0] || null,
    detailedBy: webDOMReverseRelationList(target, node, "ariaDetails"),
    errorMessage: webDOMRelationList(target, node.ariaErrorMessage)[0] || null,
    errorFor: webDOMReverseRelationList(target, node, "ariaErrorMessage"),
    flowTo: webDOMRelationList(target, node.ariaFlowTo),
    flowFrom: webDOMReverseRelationList(target, node, "ariaFlowTo"),
    controls: webDOMRelationList(target, node.ariaControls),
    controlledBy: webDOMReverseRelationList(target, node, "ariaControls"),
    owns: webDOMRelationList(target, node.ariaOwns),
    ownedBy: webDOMReverseRelationList(target, node, "ariaOwns"),
    headers: webDOMRelationList(target, node.headers),
    headerFor: webDOMReverseRelationList(target, node, "headers"),
    rowHeaders: webDOMScopedHeaderList(target, node, ["row", "rowgroup"]),
    columnHeaders: webDOMScopedHeaderList(target, node, ["col", "colgroup"]),
    rowGroupHeaders: webDOMScopedHeaderList(target, node, "rowgroup"),
    columnGroupHeaders: webDOMScopedHeaderList(target, node, "colgroup"),
    labelFor: webNodeIsLabel(node) && (webDOMRelationList(target, node.htmlFor)[0] ||
      webDOMImplicitLabelControl(target, node)) || null,
    outputFor: webNodeIsOutput(node) ? webDOMRelationList(target, node.htmlFor) : [],
    outputBy: webDOMReverseRelationList(target, node, "htmlFor")
      .filter((object) => webNodeIsOutput(object?.node)),
    dataList: webDOMRelationList(target, node.dataList)[0] || null,
    listedBy: webDOMReverseRelationList(target, node, "dataList"),
    imageMap: webDOMRelationList(target, node.useMap)[0] || null,
    mappedImages: webDOMReverseRelationList(target, node, "useMap"),
    formOwner: webDOMRelationList(target, node.formOwner)[0] || null,
    formControls: webDOMFormControls(target, node),
    labelledBy: mergeWebDOMRelationObjects(
      webDOMRelationList(target, node.ariaLabelledBy),
      webDOMReverseRelationList(target, node, "htmlFor")
        .filter((object) => webNodeIsLabel(object?.node)),
      webDOMImplicitLabels(target, node)
    ),
    activeDescendant: webDOMRelationList(target, node.ariaActiveDescendant)[0] || null,
    activeDescendantOf: webDOMReverseRelationList(target, node, "ariaActiveDescendant"),
    popoverTarget: webDOMRelationList(target, node.popoverTarget)[0] || null,
    popoverInvokers: webDOMReverseRelationList(target, node, "popoverTarget")
  };
}

function syncWebFieldsetLegend(el, docNode) {
  if (!el || String(docNode?.tag || "").toLowerCase() !== "fieldset")
    return;
  const label = String(docNode.ariaLabel || docNode.title || "").trim();
  let legend = el.__kryLegend || null;
  if (!label) {
    if (legend?.parentNode && typeof legend.parentNode.removeChild === "function")
      legend.parentNode.removeChild(legend);
    el.__kryLegend = null;
    return;
  }
  if (!legend || legend.tagName?.toLowerCase() !== "legend") {
    legend = document.createElement("legend");
    legend.dataset.kryGenerated = "legend";
    el.__kryLegend = legend;
  }
  legend.textContent = label;
  if (el.children?.[0] === legend)
    return;
  if (typeof el.insertBefore === "function") {
    el.insertBefore(legend, el.children?.[0] || null);
    return;
  }
  if (Array.isArray(el.children)) {
    const index = el.children.indexOf(legend);
    if (index >= 0)
      el.children.splice(index, 1);
    legend.parentNode = el;
    el.children.unshift(legend);
    return;
  }
  if (typeof el.appendChild === "function")
    el.appendChild(legend);
}

function applyWebNode(el, docNode, rt) {
  el.__kryDocNode = docNode;
  el.__kryRuntime = rt;
  bindWebDOMObjectProperties(el);
  bindNodeEvents(el);
  const extraClasses = [...(el.__kryExtraClasses || [])];
  docNode.classes = [...new Set([...(docNode.classes || []), ...extraClasses])];
  docNode.extraAttrs = { ...(docNode.extraAttrs || {}), ...(el.__kryExtraAttrs || {}) };
  Object.assign(docNode.state, el.__kryExtraState || {});
  el.className = ["kryon-node", "kryon-" + docNode.kind.toLowerCase(), ...docNode.classes].join(" ");
  el.dataset.kryKind = docNode.kind;
  el.dataset.kryIndex = String(docNode.index);
  el.dataset.kryKey = docNode.key;
  el.dataset.kryRef = webNodeRef(docNode);
  if (docNode.webRef)
    el.dataset.kryWebRef = docNode.webRef;
  else
    delete el.dataset.kryWebRef;
  el.dataset.kryAliases = JSON.stringify(webNodeIdentity(docNode).aliases);
  updateWebElementStateDataset(el, docNode.state);
  if (docNode.path)
    el.dataset.kryPath = docNode.path;
  else
    delete el.dataset.kryPath;
  if (docNode.parentPath)
    el.dataset.kryParentPath = docNode.parentPath;
  else
    delete el.dataset.kryParentPath;
  if (docNode.sourcePath)
    el.dataset.krySource = docNode.sourcePath;
  else
    delete el.dataset.krySource;
  if (docNode.sourceLine)
    el.dataset.kryLine = String(docNode.sourceLine);
  else
    delete el.dataset.kryLine;
  if (docNode.sourceColumn)
    el.dataset.kryColumn = String(docNode.sourceColumn);
  else
    delete el.dataset.kryColumn;
  if (docNode.sourceEndLine)
    el.dataset.kryEndLine = String(docNode.sourceEndLine);
  else
    delete el.dataset.kryEndLine;
  if (docNode.sourceEndColumn)
    el.dataset.kryEndColumn = String(docNode.sourceEndColumn);
  else
    delete el.dataset.kryEndColumn;
  const sourceRef = webNodeSourceRef(docNode);
  if (sourceRef)
    el.dataset.krySourceRef = sourceRef;
  else
    delete el.dataset.krySourceRef;
  const sourceColumnRef = webNodeSourceColumnRef(docNode);
  if (sourceColumnRef)
    el.dataset.krySourceColumnRef = sourceColumnRef;
  else
    delete el.dataset.krySourceColumnRef;
  const sourceRangeRef = webNodeSourceRangeRef(docNode);
  if (sourceRangeRef)
    el.dataset.krySourceRangeRef = sourceRangeRef;
  else
    delete el.dataset.krySourceRangeRef;
  if (docNode.name)
    el.dataset.kryName = docNode.name;
  else
    delete el.dataset.kryName;
  setAttr(el, "id", docNode.domId);
  setAttr(el, "name", docNode.domName);
  setAttr(el, "value", docNode.domValue);
  setAttr(el, "title", docNode.title);
  setAttr(el, "lang", docNode.lang);
  setAttr(el, "dir", docNode.dir);
  setAttr(el, "translate", docNode.translate);
  setAttr(el, "dirname", docNode.dirname);
  setAttr(el, "placeholder", docNode.placeholder);
  setAttr(el, "tabindex", docNode.tabIndex === null ? "" : String(docNode.tabIndex));
  setAttr(el, "role", webDOMRole(docNode));
  setAttr(el, "aria-label", docNode.tag === "fieldset" &&
    (docNode.ariaLabel || docNode.title) ? "" : docNode.ariaLabel);
  setAttr(el, "aria-description", docNode.ariaDescription);
  setAttr(el, "aria-live", docNode.ariaLive);
  if (docNode.onClick)
    el.dataset.kryOnClick = docNode.onClick;
  else
    delete el.dataset.kryOnClick;
  if (docNode.onDoubleClick)
    el.dataset.kryOnDoubleClick = docNode.onDoubleClick;
  else
    delete el.dataset.kryOnDoubleClick;
  if (docNode.onInput)
    el.dataset.kryOnInput = docNode.onInput;
  else
    delete el.dataset.kryOnInput;
  if (docNode.onBeforeInput)
    el.dataset.kryOnBeforeInput = docNode.onBeforeInput;
  else
    delete el.dataset.kryOnBeforeInput;
  if (docNode.onChange)
    el.dataset.kryOnChange = docNode.onChange;
  else
    delete el.dataset.kryOnChange;
  if (docNode.onSelect)
    el.dataset.kryOnSelect = docNode.onSelect;
  else
    delete el.dataset.kryOnSelect;
  if (docNode.onKey)
    el.dataset.kryOnKey = docNode.onKey;
  else
    delete el.dataset.kryOnKey;
  if (docNode.onKeyUp)
    el.dataset.kryOnKeyUp = docNode.onKeyUp;
  else
    delete el.dataset.kryOnKeyUp;
  if (docNode.onInvalid)
    el.dataset.kryOnInvalid = docNode.onInvalid;
  else
    delete el.dataset.kryOnInvalid;
  if (docNode.onSubmit)
    el.dataset.kryOnSubmit = docNode.onSubmit;
  else
    delete el.dataset.kryOnSubmit;
  if (docNode.onReset)
    el.dataset.kryOnReset = docNode.onReset;
  else
    delete el.dataset.kryOnReset;
  if (docNode.onToggle)
    el.dataset.kryOnToggle = docNode.onToggle;
  else
    delete el.dataset.kryOnToggle;
  if (docNode.onClose)
    el.dataset.kryOnClose = docNode.onClose;
  else
    delete el.dataset.kryOnClose;
  if (docNode.onCancel)
    el.dataset.kryOnCancel = docNode.onCancel;
  else
    delete el.dataset.kryOnCancel;
  if (docNode.onFocus)
    el.dataset.kryOnFocus = docNode.onFocus;
  else
    delete el.dataset.kryOnFocus;
  if (docNode.onBlur)
    el.dataset.kryOnBlur = docNode.onBlur;
  else
    delete el.dataset.kryOnBlur;
  if (docNode.onScroll)
    el.dataset.kryOnScroll = docNode.onScroll;
  else
    delete el.dataset.kryOnScroll;
  if (docNode.onMouseEnter)
    el.dataset.kryOnMouseEnter = docNode.onMouseEnter;
  else
    delete el.dataset.kryOnMouseEnter;
  if (docNode.onMouseLeave)
    el.dataset.kryOnMouseLeave = docNode.onMouseLeave;
  else
    delete el.dataset.kryOnMouseLeave;
  if (docNode.onMouseMove)
    el.dataset.kryOnMouseMove = docNode.onMouseMove;
  else
    delete el.dataset.kryOnMouseMove;
  if (docNode.onMouseDown)
    el.dataset.kryOnMouseDown = docNode.onMouseDown;
  else
    delete el.dataset.kryOnMouseDown;
  if (docNode.onMouseUp)
    el.dataset.kryOnMouseUp = docNode.onMouseUp;
  else
    delete el.dataset.kryOnMouseUp;
  if (docNode.onPointerEnter)
    el.dataset.kryOnPointerEnter = docNode.onPointerEnter;
  else
    delete el.dataset.kryOnPointerEnter;
  if (docNode.onPointerLeave)
    el.dataset.kryOnPointerLeave = docNode.onPointerLeave;
  else
    delete el.dataset.kryOnPointerLeave;
  if (docNode.onPointerMove)
    el.dataset.kryOnPointerMove = docNode.onPointerMove;
  else
    delete el.dataset.kryOnPointerMove;
  if (docNode.onPointerDown)
    el.dataset.kryOnPointerDown = docNode.onPointerDown;
  else
    delete el.dataset.kryOnPointerDown;
  if (docNode.onPointerUp)
    el.dataset.kryOnPointerUp = docNode.onPointerUp;
  else
    delete el.dataset.kryOnPointerUp;
  if (docNode.onPointerCancel)
    el.dataset.kryOnPointerCancel = docNode.onPointerCancel;
  else
    delete el.dataset.kryOnPointerCancel;
  if (docNode.onWheel)
    el.dataset.kryOnWheel = docNode.onWheel;
  else
    delete el.dataset.kryOnWheel;
  if (docNode.onContextMenu)
    el.dataset.kryOnContextMenu = docNode.onContextMenu;
  else
    delete el.dataset.kryOnContextMenu;
  if (docNode.onDragStart)
    el.dataset.kryOnDragStart = docNode.onDragStart;
  else
    delete el.dataset.kryOnDragStart;
  if (docNode.onDragEnd)
    el.dataset.kryOnDragEnd = docNode.onDragEnd;
  else
    delete el.dataset.kryOnDragEnd;
  if (docNode.onDragOver)
    el.dataset.kryOnDragOver = docNode.onDragOver;
  else
    delete el.dataset.kryOnDragOver;
  if (docNode.onDrop)
    el.dataset.kryOnDrop = docNode.onDrop;
  else
    delete el.dataset.kryOnDrop;
  if (docNode.onCopy)
    el.dataset.kryOnCopy = docNode.onCopy;
  else
    delete el.dataset.kryOnCopy;
  if (docNode.onCut)
    el.dataset.kryOnCut = docNode.onCut;
  else
    delete el.dataset.kryOnCut;
  if (docNode.onPaste)
    el.dataset.kryOnPaste = docNode.onPaste;
  else
    delete el.dataset.kryOnPaste;
  if (docNode.hasBounds) {
    el.style.position = "absolute";
    el.style.left = docNode.bounds.x + "px";
    el.style.top = docNode.bounds.y + "px";
    el.style.width = Math.max(0, docNode.bounds.width) + "px";
    el.style.height = Math.max(0, docNode.bounds.height) + "px";
    el.style.boxSizing = "border-box";
  } else {
    el.style.position = "";
    el.style.left = "";
    el.style.top = "";
    el.style.width = "";
    el.style.height = "";
  }
  setAttr(el, "disabled", docNode.state.disabled);
  setAttr(el, "aria-disabled", docNode.state.disabled ? "true" : "");
  setAttr(el, "aria-busy", docNode.state.loading ? "true" : "");
  setAttr(el, "aria-selected", docNode.state.selected ? "true" : "");
  setAttr(el, "aria-invalid", docNode.state.invalid ? "true" : (docNode.state.valid ? "false" : ""));
  setAttr(el, "aria-expanded", docNode.state.expanded ? "true" : "");
  setAttr(el, "aria-orientation", docNode.ariaOrientation);
  setAttr(el, "aria-posinset", docNode.ariaPosInSet);
  setAttr(el, "aria-setsize", docNode.ariaSetSize);
  setAttr(el, "aria-haspopup", docNode.ariaHasPopup);
  setAttr(el, "aria-multiselectable", docNode.ariaMultiSelectable);
  setAttr(el, "aria-rowindex", docNode.ariaRowIndex);
  setAttr(el, "aria-colindex", docNode.ariaColIndex);
  setAttr(el, "aria-rowcount", docNode.ariaRowCount);
  setAttr(el, "aria-colcount", docNode.ariaColCount);
  if (docNode.tag === "details" || docNode.tag === "dialog") {
    setAttr(el, "open", docNode.state.open);
    el.open = !!docNode.state.open;
  } else {
    removeAttr(el, "open");
  }
  setAttr(el, "aria-checked",
    docNode.inputType === "checkbox" || docNode.inputType === "radio"
      ? (docNode.state.indeterminate ? "mixed" : (docNode.state.checked ? "true" : "false"))
      : "");
  setAttr(el, "aria-current",
    docNode.tag === "a" && docNode.state.selected ? "page" : "");
  setAttr(el, "aria-level",
    docNode.ariaLevel ||
    (docNode.role === "heading" && docNode.level ? String(docNode.level) : ""));
  applyAriaAttrs(el, docNode.ariaAttrs);
  setAttr(el, "href", docNode.href);
  setAttr(el, "target", docNode.target);
  setAttr(el, "rel", docNode.rel);
  setAttr(el, "part", docNode.part);
  setAttr(el, "slot", docNode.slot);
  applyDataAttrs(el, docNode.dataAttrs);
  setAttr(el, "type", docNode.inputType);
  setAttr(el, "action", docNode.formAction);
  setAttr(el, "method", docNode.formMethod);
  setAttr(el, "enctype", docNode.formEncType);
  setAttr(el, "autocomplete", docNode.autoComplete);
  setAttr(el, "hidden", docNode.hidden);
  setAttr(el, "draggable", docNode.draggable);
  setAttr(el, "spellcheck", docNode.spellCheck);
  setAttr(el, "contenteditable", docNode.contentEditable);
  setAttr(el, "autofocus", docNode.autoFocus);
  setAttr(el, "inert", docNode.inert);
  setAttr(el, "autocapitalize", docNode.autoCapitalize);
  setAttr(el, "enterkeyhint", docNode.enterKeyHint);
  setAttr(el, "download", docNode.download);
  setAttr(el, "formnovalidate", docNode.formNoValidate);
  setAttr(el, "novalidate", docNode.noValidate);
  setAttr(el, "popover", docNode.popover);
  setAttr(el, "popovertargetaction", docNode.popoverTargetAction);
  el.hidden = !!docNode.hidden;
  if (docNode.draggable === "true" || docNode.draggable === "false")
    el.draggable = docNode.draggable === "true";
  if (docNode.spellCheck)
    el.spellcheck = docNode.spellCheck === "true";
  if (docNode.contentEditable)
    el.contentEditable = docNode.contentEditable;
  el.autofocus = !!docNode.autoFocus;
  if ("inert" in el)
    el.inert = !!docNode.inert;
  if ("formNoValidate" in el)
    el.formNoValidate = !!docNode.formNoValidate;
  if ("noValidate" in el)
    el.noValidate = !!docNode.noValidate;
  setAttr(el, "readonly", docNode.readOnly);
  setAttr(el, "required", docNode.required);
  setAttr(el, "min", docNode.min);
  setAttr(el, "max", docNode.max);
  setAttr(el, "step", docNode.step);
  setAttr(el, "minlength", docNode.minLength);
  setAttr(el, "maxlength", docNode.maxLength);
  setAttr(el, "pattern", docNode.pattern);
  setAttr(el, "accept", docNode.accept);
  setAttr(el, "multiple", docNode.multiple);
  setAttr(el, "inputmode", docNode.inputMode);
  setAttr(el, "scope", docNode.scope);
  setAttr(el, "colspan", docNode.colSpan);
  setAttr(el, "rowspan", docNode.rowSpan);
  setAttr(el, "aria-sort", docNode.ariaSort);
  applyExtraAttrs(el, docNode.extraAttrs);
  if (docNode.tag === "img" || docNode.tag === "area") {
    setAttr(el, "alt", docNode.alt);
  }
  if (docNode.tag === "img") {
    setAttr(el, "src", docNode.asset);
  } else if (docNode.tag === "option") {
    setAttr(el, "selected", docNode.state.selected);
    el.selected = !!docNode.state.selected;
    el.textContent = docNode.text;
  } else if (docNode.tag === "input") {
    const nativeValue = docNode.domValue || docNode.value;
    if (nativeValue !== undefined && nativeValue !== null && nativeValue !== "")
      el.setAttribute("value", nativeValue);
    else
      removeAttr(el, "value");
    setAttr(el, "checked",
      docNode.inputType === "checkbox" || docNode.inputType === "radio"
        ? docNode.state.checked
        : false);
    if (docNode.inputType !== "checkbox" && docNode.inputType !== "radio")
      el.value = nativeValue;
    el.checked = !!docNode.state.checked;
    if ("indeterminate" in el)
      el.indeterminate = !!docNode.state.indeterminate;
  } else if (docNode.tag === "textarea") {
    el.value = docNode.value;
  } else if (docNode.tag === "fieldset") {
    syncWebFieldsetLegend(el, docNode);
  } else {
    el.textContent = docNode.text;
  }
}

function ensureMountRoot(node) {
  let root = node.__kryRuntimeRoot || null;
  if (root && root.parentNode === node) {
    bindWebRootProperties(root);
    return root;
  }
  root = document.createElement("div");
  root.className = "kryon-runtime";
  root.dataset.kryRuntime = "web-document";
  root.style.position = "relative";
  root.style.minHeight = "100%";
  root.style.fontFamily = "system-ui, sans-serif";
  root.__kryChildren = new Map();
  bindWebRootProperties(root);
  node.__kryRuntimeRoot = root;
  node.appendChild(root);
  return root;
}

function bindWebRootProperties(root) {
  if (!root || root.__kryRootPropertiesBound)
    return;
  Object.defineProperties(root, {
    kryRuntime: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryRuntime || null;
      }
    },
    kryFrame: {
      configurable: true,
      enumerable: false,
      get() {
        return this.__kryFrame || null;
      }
    },
    kryObjects: {
      configurable: true,
      enumerable: false,
      get() {
        return webDOMObjectsFromRoot(this);
      }
    },
    kryObjectMap: {
      configurable: true,
      enumerable: false,
      get() {
        return webDOMObjectMap(this);
      }
    },
    kryElement: {
      configurable: true,
      enumerable: false,
      value(query) {
        return findWebElement(this, query);
      }
    },
    kryElements: {
      configurable: true,
      enumerable: false,
      value(query) {
        return findWebElements(this, query);
      }
    },
    kryObject: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMObject(this, query);
      }
    },
    kryIdentity: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMIdentity(this, query);
      }
    },
    krySnapshot: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMSnapshot(this, query);
      }
    },
    krySnapshots: {
      configurable: true,
      enumerable: false,
      value(selector = "") {
        return webDOMSnapshots(this, selector);
      }
    },
    kryAccessibilitySnapshot: {
      configurable: true,
      enumerable: false,
      value(selector = "") {
        return webDOMAccessibilitySnapshot(this, selector);
      }
    },
    kryStyleFacts: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMStyleFacts(this, query);
      }
    },
    kryStyleTrace: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMStyleTrace(this, query);
      }
    },
    kryRelations: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMRelations(this, query);
      }
    },
    kryRelationRefs: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMRelationRefs(this, query);
      }
    },
    kryAncestors: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMAncestors(this, query);
      }
    },
    kryPreviousSibling: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMPreviousSibling(this, query);
      }
    },
    kryNextSibling: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMNextSibling(this, query);
      }
    },
    kryPreviousSiblings: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMPreviousSiblings(this, query);
      }
    },
    kryNextSiblings: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMNextSiblings(this, query);
      }
    },
    krySiblings: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMSiblings(this, query);
      }
    },
    kryEventRefs: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMEventRefs(this, query);
      }
    },
    kryQuery: {
      configurable: true,
      enumerable: false,
      value(selector) {
        return webDOMQuery(this, selector);
      }
    },
    kryQueryAll: {
      configurable: true,
      enumerable: false,
      value(selector) {
        return webDOMQueryAll(this, selector);
      }
    },
    kryDescendants: {
      configurable: true,
      enumerable: false,
      value(query = "") {
        return webDOMDescendants(this, query);
      }
    },
    kryQueryWithin: {
      configurable: true,
      enumerable: false,
      value(query, selector) {
        return webDOMQueryWithin(this, query, selector);
      }
    },
    kryQueryAllWithin: {
      configurable: true,
      enumerable: false,
      value(query, selector) {
        return webDOMQueryAllWithin(this, query, selector);
      }
    },
    kryAtSource: {
      configurable: true,
      enumerable: false,
      value(sourcePath, sourceLine, sourceColumn = 0) {
        return webDOMObjectAtSource(this, sourcePath, sourceLine, sourceColumn);
      }
    },
    kryAtSourceRange: {
      configurable: true,
      enumerable: false,
      value(sourcePath, sourceLine, sourceColumn = 0) {
        return webDOMObjectAtSourceRange(this, sourcePath, sourceLine, sourceColumn);
      }
    },
    kryOverlappingSourceRange: {
      configurable: true,
      enumerable: false,
      value(sourcePath, sourceLine, sourceColumn = 0,
            sourceEndLine = sourceLine, sourceEndColumn = sourceColumn) {
        return webDOMObjectOverlappingSourceRange(this, sourcePath, sourceLine,
          sourceColumn, sourceEndLine, sourceEndColumn);
      }
    },
    kryOverlappingSourceRanges: {
      configurable: true,
      enumerable: false,
      value(sourcePath, sourceLine, sourceColumn = 0,
            sourceEndLine = sourceLine, sourceEndColumn = sourceColumn) {
        return webDOMObjectsOverlappingSourceRange(this, sourcePath, sourceLine,
          sourceColumn, sourceEndLine, sourceEndColumn);
      }
    },
    krySourceMap: {
      configurable: true,
      enumerable: false,
      get() {
        return webDOMSourceMap(this);
      }
    },
    kryListen: {
      configurable: true,
      enumerable: false,
      value(query, type, handler, options) {
        return webDOMAddEventListener(this, query, type, handler, options);
      }
    },
    kryDelegate: {
      configurable: true,
      enumerable: false,
      value(selector, type, handler, options) {
        return webDOMAddDelegatedEventListener(this, selector, type, handler, options);
      }
    },
    kryObserve: {
      configurable: true,
      enumerable: false,
      value(selector, handler, options) {
        return webDOMObserve(this, selector, handler, options);
      }
    },
    kryBind: {
      configurable: true,
      enumerable: false,
      value(selector, handlers, options) {
        return webDOMBind(this, selector, handlers, options);
      }
    },
    kryAddClass: {
      configurable: true,
      enumerable: false,
      value(query, className) {
        return webDOMAddClass(this, query, className);
      }
    },
    kryRemoveClass: {
      configurable: true,
      enumerable: false,
      value(query, className) {
        return webDOMRemoveClass(this, query, className);
      }
    },
    kryToggleClass: {
      configurable: true,
      enumerable: false,
      value(query, className, force) {
        return webDOMToggleClass(this, query, className, force);
      }
    },
    kryHasClass: {
      configurable: true,
      enumerable: false,
      value(query, className) {
        return webDOMHasClass(this, query, className);
      }
    },
    kryGetAttr: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMGetAttribute(this, query, name);
      }
    },
    krySetAttr: {
      configurable: true,
      enumerable: false,
      value(query, name, value = "") {
        return webDOMSetAttribute(this, query, name, value);
      }
    },
    kryRemoveAttr: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMRemoveAttribute(this, query, name);
      }
    },
    kryHasAttr: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMHasAttribute(this, query, name);
      }
    },
    kryGetProp: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMGetProperty(this, query, name);
      }
    },
    krySetProp: {
      configurable: true,
      enumerable: false,
      value(query, name, value) {
        return webDOMSetProperty(this, query, name, value);
      }
    },
    kryGetStyle: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMGetStyle(this, query, name);
      }
    },
    krySetStyle: {
      configurable: true,
      enumerable: false,
      value(query, name, value = "") {
        return webDOMSetStyle(this, query, name, value);
      }
    },
    kryRemoveStyle: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMRemoveStyle(this, query, name);
      }
    },
    kryComputedStyle: {
      configurable: true,
      enumerable: false,
      value(query, name = "") {
        return webDOMComputedStyle(this, query, name);
      }
    },
    kryGetState: {
      configurable: true,
      enumerable: false,
      value(query, name) {
        return webDOMGetState(this, query, name);
      }
    },
    krySetState: {
      configurable: true,
      enumerable: false,
      value(query, name, value) {
        return webDOMSetState(this, query, name, value);
      }
    },
    kryToggleState: {
      configurable: true,
      enumerable: false,
      value(query, name, force) {
        return webDOMToggleState(this, query, name, force);
      }
    },
    kryText: {
      configurable: true,
      enumerable: false,
      value(query, text) {
        return text === undefined ? webDOMGetText(this, query) : webDOMSetText(this, query, text);
      }
    },
    kryValue: {
      configurable: true,
      enumerable: false,
      value(query, value) {
        return value === undefined ? webDOMGetValue(this, query) : webDOMSetValue(this, query, value);
      }
    },
    kryDispatch: {
      configurable: true,
      enumerable: false,
      value(query, type, init = {}) {
        return webDOMDispatchEvent(this, query, type, init);
      }
    },
    kryClick: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMClick(this, query);
      }
    },
    kryFocus: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMFocus(this, query);
      }
    },
    kryBlur: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMBlur(this, query);
      }
    },
    krySubmit: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMSubmit(this, query);
      }
    },
    kryReset: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMReset(this, query);
      }
    },
    kryRect: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMRect(this, query);
      }
    },
    kryScroll: {
      configurable: true,
      enumerable: false,
      value(query, left, top = null) {
        return left === undefined ? webDOMGetScroll(this, query) : webDOMSetScroll(this, query, left, top);
      }
    },
    kryScrollIntoView: {
      configurable: true,
      enumerable: false,
      value(query, options = true) {
        return webDOMScrollIntoView(this, query, options);
      }
    },
    kryShowModal: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMShowModal(this, query);
      }
    },
    kryClose: {
      configurable: true,
      enumerable: false,
      value(query, returnValue = "") {
        return webDOMClose(this, query, returnValue);
      }
    },
    kryShowPopover: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMShowPopover(this, query);
      }
    },
    kryHidePopover: {
      configurable: true,
      enumerable: false,
      value(query) {
        return webDOMHidePopover(this, query);
      }
    },
    kryTogglePopover: {
      configurable: true,
      enumerable: false,
      value(query, force) {
        return webDOMTogglePopover(this, query, force);
      }
    },
    krySync: {
      configurable: true,
      enumerable: false,
      value(query = "") {
        return webDOMSync(this, query);
      }
    }
  });
  root.__kryRootPropertiesBound = true;
}

function webDOMObjectsFromRoot(root) {
  if (!root)
    return [];
  return [...(root.__kryChildren?.values?.() || [])]
    .map((element) => {
      const node = element.__kryDocNode;
      return makeWebDOMObject(root, node, element);
    })
    .filter(Boolean);
}

function dispatchWebDOMLifecycle(root, type, frame, object) {
  const el = object?.element || null;
  if (!root || !el || typeof el.dispatchEvent !== "function")
    return false;
  const event = createWebDOMEvent(type, {
    detail: {
      frame,
      root,
      object,
      node: object.node,
      element: el
    }
  });
  if (!event)
    return false;
  bindWebDOMEventProperties(event);
  el.dispatchEvent(event);
  return true;
}

export function renderWebDocument(rt, target) {
  const node = typeof target === "string" && typeof document !== "undefined"
    ? document.querySelector(target)
    : target;
  if (!node || typeof document === "undefined") {
    if (rt)
      rt.mounted = !!node;
    return rt;
  }
  const frame = webDocumentFrame(rt);
  applyDocumentMetadata(frame.metadata);
  const root = ensureMountRoot(node);
  root.__kryRuntime = rt || null;
  root.__kryFrame = frame;
  const children = root.__kryChildren || new Map();
  const elementsByPath = new Map();
  const live = new Set();
  root.__kryNodes = new Map();
  root.__kryElementsByPath = new Map();
  root.__kryElementsByRef = new Map();
  root.__kryDomObjects = new Map();
  root.__kryElementsByName = new Map();
  root.__kryElementsByDomId = new Map();
  root.__kryElementsByDomName = new Map();
  root.__kryElementsBySource = new Map();
  root.__kryDomObjectsBySource = new Map();
  root.__kryFormValues = new Map();
  for (const docNode of frame.nodes) {
    const identity = docNode.tag + ":" + (docNode.path || docNode.key);
    let el = children.get(identity);
    const existed = !!el && el.tagName?.toLowerCase() === docNode.tag;
    if (!el || el.tagName?.toLowerCase() !== docNode.tag) {
      el = document.createElement(docNode.tag);
      children.set(identity, el);
    }
    applyWebNode(el, docNode, rt);
    applyResolvedWebStyle(el, rt?.webStyleSheets ? resolveWebStyle(docNode, rt.webStyleSheets) : null);
    el.__kryMountRoot = root;
    updateElementFormValue(el);
    if (docNode.path && docNode.path !== docNode.parentPath)
      elementsByPath.set(docNode.path, el);
    if (docNode.path)
      root.__kryNodes.set(docNode.path, docNode);
    const ref = webNodeRef(docNode);
    if (ref) {
      root.__kryDomObjects.set(ref, makeWebDOMObject(root, docNode, el, ref));
      root.__kryElementsByRef.set(ref, el);
    }
    const sourceRef = webNodeSourceRef(docNode);
    if (sourceRef) {
      const sourceObject = makeWebDOMObject(root, docNode, el, sourceRef);
      if (!root.__kryDomObjects.has(sourceRef))
        root.__kryDomObjects.set(sourceRef, sourceObject);
      pushIndex(root.__kryDomObjectsBySource, sourceRef, sourceObject);
    }
    const sourceColumnRef = webNodeSourceColumnRef(docNode);
    if (sourceColumnRef) {
      const sourceColumnObject = makeWebDOMObject(root, docNode, el, sourceColumnRef);
      if (!root.__kryDomObjects.has(sourceColumnRef))
        root.__kryDomObjects.set(sourceColumnRef, sourceColumnObject);
      pushIndex(root.__kryDomObjectsBySource, sourceColumnRef, sourceColumnObject);
    }
    const sourceRangeRef = webNodeSourceRangeRef(docNode);
    if (sourceRangeRef) {
      const sourceRangeObject = makeWebDOMObject(root, docNode, el, sourceRangeRef);
      if (!root.__kryDomObjects.has(sourceRangeRef))
        root.__kryDomObjects.set(sourceRangeRef, sourceRangeObject);
      pushIndex(root.__kryDomObjectsBySource, sourceRangeRef, sourceRangeObject);
    }
    if (docNode.path)
      root.__kryElementsByPath.set(docNode.path, el);
    if (docNode.name)
      root.__kryElementsByName.set(docNode.name, el);
    if (docNode.domId)
      root.__kryElementsByDomId.set(docNode.domId, el);
    if (docNode.domName)
      root.__kryElementsByDomName.set(docNode.domName, el);
    if (sourceRef)
      pushIndex(root.__kryElementsBySource, sourceRef, el);
    if (sourceColumnRef)
      pushIndex(root.__kryElementsBySource, sourceColumnRef, el);
    if (sourceRangeRef)
      pushIndex(root.__kryElementsBySource, sourceRangeRef, el);
    const parent = docNode.parentPath && elementsByPath.get(docNode.parentPath)
      ? elementsByPath.get(docNode.parentPath)
      : root;
    parent.appendChild(el);
    live.add(identity);
    dispatchWebDOMLifecycle(root, existed ? "kry-update" : "kry-mount", frame,
      makeWebDOMObject(root, docNode, el, ref));
  }
  const stale = Array.from(children.entries()).filter(([identity]) => !live.has(identity));
  for (const [, el] of stale) {
    dispatchWebDOMLifecycle(root, "kry-unmount", frame,
      makeWebDOMObject(root, el.__kryDocNode || null, el));
  }
  for (const [identity, el] of stale) {
      if (el.parentNode && typeof el.parentNode.removeChild === "function")
        el.parentNode.removeChild(el);
      children.delete(identity);
  }
  for (const docNode of frame.nodes) {
    const identity = docNode.tag + ":" + (docNode.path || docNode.key);
    const el = children.get(identity);
    syncWebFieldsetLegend(el, docNode);
  }
  root.__kryChildren = children;
  syncWebDOMRootIndexes(root);
  resolveWebDOMRelations(root);
  if (rt)
    rt.mounted = true;
  if (typeof root.dispatchEvent === "function") {
    root.dispatchEvent(createWebDOMEvent("kry-render", {
      detail: {
        frame,
        root,
        objects: webDOMObjectsFromRoot(root)
      }
    }));
  }
  return rt;
}

function mountedRoot(target) {
  const node = typeof target === "string" && typeof document !== "undefined"
    ? document.querySelector(target)
    : target;
  return node?.__kryRuntimeRoot || (node?.__kryChildren ? node : null) || null;
}

export function webDOMRoot(target) {
  return mountedRoot(target);
}

export function webDOMFrame(target) {
  return mountedRoot(target)?.__kryFrame || null;
}

export function findWebNode(rt, query) {
  const text = String(query || "");
  const frame = webDocumentFrame(rt);
  return frame.nodes.find((node) =>
    node.path === text || node.name === text || node.key === text ||
    node.webRef === text || node.domId === text || sourceRefMatches(node, text)) ||
    null;
}

export function webNodeQueryAll(rt, selector) {
  const text = String(selector || "").trim();
  if (!text)
    return [];
  const frame = webDocumentFrame(rt);
  const exact = frame.nodes.filter((node) =>
    node.path === text || node.name === text || node.key === text ||
    node.webRef === text || node.domId === text || sourceRefMatches(node, text));
  if (exact.length)
    return exact;
  const parsed = parseSelector(text);
  return frame.nodes.filter((node) => selectorMatchesWebNode(parsed, node));
}

export function webNodeQuery(rt, selector) {
  return webNodeQueryAll(rt, selector)[0] || null;
}

export function webNodeMatches(rt, query, selector) {
  const node = webNodeQuery(rt, query);
  return !!node && selectorMatchesWebNode(parseSelector(String(selector || "").trim()), node);
}

function webFrameNodeMap(frame) {
  const nodes = new Map();
  for (const node of frame?.nodes || [])
    if (node.path)
      nodes.set(node.path, node);
  return nodes;
}

export function webNodeParent(rt, query) {
  const frame = webDocumentFrame(rt);
  const node = webNodeQuery(rt, query);
  const parentPath = node?.parentPath || "";
  if (!node || !parentPath || parentPath === node.path)
    return null;
  return webFrameNodeMap(frame).get(parentPath) || null;
}

export function webNodeAncestors(rt, query) {
  const ancestors = [];
  let node = webNodeParent(rt, query);
  while (node) {
    ancestors.push(node);
    node = webNodeParent(rt, node.path);
  }
  return ancestors;
}

export function webNodePreviousSibling(rt, query) {
  return webNodePreviousSiblingFromFrame(webNodeQuery(rt, query));
}

export function webNodeNextSibling(rt, query) {
  return webNodeNextSiblingFromFrame(webNodeQuery(rt, query));
}

export function webNodePreviousSiblings(rt, query) {
  return webNodePreviousSiblingsFromFrame(webNodeQuery(rt, query));
}

export function webNodeNextSiblings(rt, query) {
  const node = webNodeQuery(rt, query);
  const siblings = webNodeSiblingsFromFrame(node);
  const index = siblings.indexOf(node);
  return index < 0 ? [] : siblings.slice(index + 1);
}

export function webNodeSiblings(rt, query) {
  const node = webNodeQuery(rt, query);
  return webNodeSiblingsFromFrame(node).filter((sibling) => sibling !== node);
}

export function webNodeChildren(rt, query = "") {
  const frame = webDocumentFrame(rt);
  const text = String(query || "").trim();
  const parent = text ? webNodeQuery(rt, text) : null;
  if (text && !parent)
    return [];
  const nodes = webFrameNodeMap(frame);
  return frame.nodes.filter((node) => {
    const parentPath = node.parentPath || "";
    if (parent)
      return parentPath === parent.path && node.path !== parent.path;
    return !parentPath || parentPath === node.path || !nodes.has(parentPath);
  });
}

export function webNodeDescendants(rt, query = "") {
  const frame = webDocumentFrame(rt);
  const text = String(query || "").trim();
  const parent = text ? webNodeQuery(rt, text) : null;
  if (text && !parent)
    return [];
  const parentPath = parent?.path || "";
  if (!parentPath)
    return frame.nodes;
  return frame.nodes.filter((node) =>
    node.path !== parentPath &&
    node.path.startsWith(parentPath + "/"));
}

export function webNodeClosest(rt, query, selector) {
  let node = webNodeQuery(rt, query);
  const parsed = parseSelector(String(selector || "").trim());
  while (node) {
    if (selectorMatchesWebNode(parsed, node))
      return node;
    node = webNodeParent(rt, node.path);
  }
  return null;
}

export function webNodeQueryAllWithin(rt, query, selector) {
  const text = String(selector || "").trim();
  if (!text)
    return [];
  const scope = webNodeQuery(rt, query);
  const parsed = parseSelector(text);
  return webNodeDescendants(rt, query)
    .filter((node) => selectorMatchesWebNode(parsed, node, scope));
}

export function webNodeQueryWithin(rt, query, selector) {
  return webNodeQueryAllWithin(rt, query, selector)[0] || null;
}

export function webNodesAtSource(rt, sourcePath, sourceLine, sourceColumn = 0) {
  const ref = webSourceRef(sourcePath, sourceLine, sourceColumn);
  return ref ? webNodeQueryAll(rt, ref) : [];
}

export function webNodeAtSource(rt, sourcePath, sourceLine, sourceColumn = 0) {
  return webNodesAtSource(rt, sourcePath, sourceLine, sourceColumn)[0] || null;
}

export function webNodesAtSourceRange(rt, sourcePath, sourceLine, sourceColumn = 0) {
  const path = String(sourcePath || "").trim();
  const line = Number.isFinite(Number(sourceLine)) ? Math.trunc(Number(sourceLine)) : 0;
  const column = Number.isFinite(Number(sourceColumn)) ? Math.trunc(Number(sourceColumn)) : 0;
  if (!path || line <= 0)
    return [];
  return sortWebNodesDeepestFirst(webDocumentFrame(rt).nodes
    .filter((node) => sourcePositionWithinNode(node, path, line, column)));
}

export function webNodeAtSourceRange(rt, sourcePath, sourceLine, sourceColumn = 0) {
  return webNodesAtSourceRange(rt, sourcePath, sourceLine, sourceColumn)[0] || null;
}

export function webNodesOverlappingSourceRange(rt, sourcePath, sourceLine,
                                                sourceColumn = 0,
                                                sourceEndLine = sourceLine,
                                                sourceEndColumn = sourceColumn) {
  const path = String(sourcePath || "").trim();
  if (!path)
    return [];
  return sortWebNodesDeepestFirst(webDocumentFrame(rt).nodes
    .filter((node) => sourceRangeOverlapsNode(node, path, sourceLine,
      sourceColumn, sourceEndLine, sourceEndColumn)));
}

export function webNodeOverlappingSourceRange(rt, sourcePath, sourceLine,
                                               sourceColumn = 0,
                                               sourceEndLine = sourceLine,
                                               sourceEndColumn = sourceColumn) {
  return webNodesOverlappingSourceRange(rt, sourcePath, sourceLine,
    sourceColumn, sourceEndLine, sourceEndColumn)[0] || null;
}

export function webSourceMap(rt) {
  return webDocumentFrame(rt).nodes
    .filter((node) => webNodeHasSource(node))
    .map((node) => webNodeIdentity(node));
}

export function findWebElement(target, query) {
  const root = mountedRoot(target);
  if (!root)
    return null;
  const text = String(query || "");
  if (root.__kryChildren?.has(text))
    return root.__kryChildren.get(text);
  if (root.__kryElementsByPath?.has(text))
    return root.__kryElementsByPath.get(text);
  if (root.__kryElementsByRef?.has(text))
    return root.__kryElementsByRef.get(text);
  if (root.__kryElementsByName?.has(text))
    return root.__kryElementsByName.get(text);
  if (root.__kryElementsByDomId?.has(text))
    return root.__kryElementsByDomId.get(text);
  if (root.__kryElementsByDomName?.has(text))
    return root.__kryElementsByDomName.get(text);
  if (root.__kryElementsBySource?.has(text))
    return root.__kryElementsBySource.get(text)[0] || null;
  for (const el of root.__kryChildren?.values?.() || []) {
    const node = el.__kryDocNode;
    if (node && (node.path === text || node.webRef === text || node.key === text ||
                 sourceRefMatches(node, text)))
      return el;
  }
  const selector = parseSelector(text);
  for (const el of root.__kryChildren?.values?.() || []) {
    const node = el.__kryDocNode;
    if (node && selectorMatchesWebNode(selector, node))
      return el;
  }
  return null;
}

export function findWebElements(target, query) {
  return webDOMQueryAll(target, query).map((object) => object.element);
}

export function webDOMObject(target, query) {
  const root = mountedRoot(target);
  if (!root)
    return null;
  const text = String(query || "");
  if (root.__kryDomObjects?.has(text))
    return root.__kryDomObjects.get(text);
  const element = findWebElement(target, text);
  const node = element?.__kryDocNode || null;
  return makeWebDOMObject(root, node, element);
}

export function webDOMIdentity(target, query) {
  const object = webDOMObject(target, query);
  return object ? webNodeIdentity(object.node) : null;
}

export function webDOMEventRefs(target, query) {
  const object = webDOMObject(target, query);
  return object ? webNodeEventRefs(object.node) : null;
}

export function webDOMStyleFacts(target, query) {
  const object = webDOMObject(target, query);
  return object ? webNodeStyleFacts(object.node) : null;
}

export function webDOMStyleTrace(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(root, query) : null;
  return object ? traceWebStyle(object.node, root.__kryRuntime?.webStyleSheets || []) : null;
}

export function webDOMRelations(target, query) {
  const object = webDOMObject(target, query);
  return object ? webDOMRelationsForNode(target, object.node) : null;
}

function webDOMRelationRefsForRelations(relations) {
  return {
    previousSibling: relations?.previousSibling?.ref || "",
    nextSibling: relations?.nextSibling?.ref || "",
    describedBy: (relations?.describedBy || []).map((relation) => relation.ref),
    describes: (relations?.describes || []).map((relation) => relation.ref),
    details: relations?.details?.ref || "",
    detailedBy: (relations?.detailedBy || []).map((relation) => relation.ref),
    errorMessage: relations?.errorMessage?.ref || "",
    errorFor: (relations?.errorFor || []).map((relation) => relation.ref),
    flowTo: (relations?.flowTo || []).map((relation) => relation.ref),
    flowFrom: (relations?.flowFrom || []).map((relation) => relation.ref),
    controls: (relations?.controls || []).map((relation) => relation.ref),
    controlledBy: (relations?.controlledBy || []).map((relation) => relation.ref),
    owns: (relations?.owns || []).map((relation) => relation.ref),
    ownedBy: (relations?.ownedBy || []).map((relation) => relation.ref),
    headers: (relations?.headers || []).map((relation) => relation.ref),
    headerFor: (relations?.headerFor || []).map((relation) => relation.ref),
    rowHeaders: (relations?.rowHeaders || []).map((relation) => relation.ref),
    columnHeaders: (relations?.columnHeaders || []).map((relation) => relation.ref),
    rowGroupHeaders: (relations?.rowGroupHeaders || []).map((relation) => relation.ref),
    columnGroupHeaders: (relations?.columnGroupHeaders || []).map((relation) => relation.ref),
    labelFor: relations?.labelFor?.ref || "",
    outputFor: (relations?.outputFor || []).map((relation) => relation.ref),
    outputBy: (relations?.outputBy || []).map((relation) => relation.ref),
    dataList: relations?.dataList?.ref || "",
    listedBy: (relations?.listedBy || []).map((relation) => relation.ref),
    imageMap: relations?.imageMap?.ref || "",
    mappedImages: (relations?.mappedImages || []).map((relation) => relation.ref),
    formOwner: relations?.formOwner?.ref || "",
    formControls: (relations?.formControls || []).map((relation) => relation.ref),
    labelledBy: (relations?.labelledBy || []).map((relation) => relation.ref),
    groupOwner: relations?.groupOwner?.ref || "",
    groupMembers: (relations?.groupMembers || []).map((relation) => relation.ref),
    disabledOwner: relations?.disabledOwner?.ref || "",
    disabledMembers: (relations?.disabledMembers || []).map((relation) => relation.ref),
    landmarkOwner: relations?.landmarkOwner?.ref || "",
    landmarkMembers: (relations?.landmarkMembers || []).map((relation) => relation.ref),
    collectionOwner: relations?.collectionOwner?.ref || "",
    collectionItems: (relations?.collectionItems || []).map((relation) => relation.ref),
    captionOwner: relations?.captionOwner?.ref || "",
    captionItems: (relations?.captionItems || []).map((relation) => relation.ref),
    summaryOwner: relations?.summaryOwner?.ref || "",
    summaryItems: (relations?.summaryItems || []).map((relation) => relation.ref),
    legendOwner: relations?.legendOwner?.ref || "",
    legendItems: (relations?.legendItems || []).map((relation) => relation.ref),
    descriptionListOwner: relations?.descriptionListOwner?.ref || "",
    descriptionListItems: (relations?.descriptionListItems || []).map((relation) => relation.ref),
    selectedCollectionOwner: relations?.selectedCollectionOwner?.ref || "",
    selectedCollectionItems: (relations?.selectedCollectionItems || []).map((relation) => relation.ref),
    activeCollectionOwner: relations?.activeCollectionOwner?.ref || "",
    activeCollectionItems: (relations?.activeCollectionItems || []).map((relation) => relation.ref),
    activeDescendant: relations?.activeDescendant?.ref || "",
    activeDescendantOf: (relations?.activeDescendantOf || []).map((relation) => relation.ref),
    popoverTarget: relations?.popoverTarget?.ref || "",
    popoverInvokers: (relations?.popoverInvokers || []).map((relation) => relation.ref)
  };
}

export function webDOMRelationRefs(target, query) {
  const relations = webDOMRelations(target, query);
  return relations ? webDOMRelationRefsForRelations(relations) : null;
}

function webDOMObjectForNode(root, node) {
  if (!root || !node)
    return null;
  const element = node.path ? root.__kryElementsByPath?.get(node.path) : null;
  return makeWebDOMObject(root, node, element);
}

export function webDOMObjectFromElement(element) {
  let el = element || null;
  while (el) {
    const node = el.__kryDocNode || null;
    if (node)
      return makeWebDOMObject(el.__kryMountRoot || mountedRoot(el), node, el);
    el = el.parentNode || null;
  }
  return null;
}

function rawWebDOMObjectFromEvent(eventOrTarget) {
  const target = eventOrTarget?.target || eventOrTarget?.currentTarget || eventOrTarget;
  return webDOMObjectFromElement(target || null);
}

function bindWebDOMEventProperties(event) {
  if (!event || typeof event !== "object" || event.__kryEventPropertiesBound)
    return event;
  try {
    Object.defineProperties(event, {
      kryRef: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.ref || "";
        }
      },
      kryPath: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.path || "";
        }
      },
      kryAliases: {
        configurable: true,
        enumerable: false,
        get() {
          return webNodeIdentity(rawWebDOMObjectFromEvent(this)?.node).aliases;
        }
      },
      kryIndex: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.index || 0;
        }
      },
      kryKind: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.kind || "";
        }
      },
      kryTag: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          return object?.node?.tag || String(object?.element?.tagName || "").toLowerCase();
        }
      },
      krySourceRef: {
        configurable: true,
        enumerable: false,
        get() {
          return webNodeSourceRef(rawWebDOMObjectFromEvent(this)?.node);
        }
      },
      krySourceColumnRef: {
        configurable: true,
        enumerable: false,
        get() {
          return webNodeSourceColumnRef(rawWebDOMObjectFromEvent(this)?.node);
        }
      },
      krySourceRangeRef: {
        configurable: true,
        enumerable: false,
        get() {
          return webNodeSourceRangeRef(rawWebDOMObjectFromEvent(this)?.node);
        }
      },
      krySourcePath: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.sourcePath || "";
        }
      },
      krySourceLine: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.sourceLine || 0;
        }
      },
      krySourceColumn: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.sourceColumn || 0;
        }
      },
      krySourceEndLine: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.sourceEndLine || 0;
        }
      },
      krySourceEndColumn: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.node?.sourceEndColumn || 0;
        }
      },
      kryObject: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this);
        }
      },
      kryRoot: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          return object?.element?.__kryMountRoot ||
            mountedRoot(this.currentTarget || this.target || null);
        }
      },
      kryIdentity: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          return object ? webNodeIdentity(object.node) : null;
        }
      },
      krySnapshot: {
        configurable: true,
        enumerable: false,
        get() {
          return webDOMSnapshotFromEvent(this);
        }
      },
      kryParent: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.parent || null;
        }
      },
      kryAncestors: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.ancestors || [];
        }
      },
      kryPreviousSibling: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.previousSibling || null;
        }
      },
      kryNextSibling: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.nextSibling || null;
        }
      },
      kryPreviousSiblings: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.previousSiblings || [];
        }
      },
      kryNextSiblings: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.nextSiblings || [];
        }
      },
      krySiblings: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.siblings || [];
        }
      },
      kryChildren: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.children || [];
        }
      },
      kryDescendants: {
        configurable: true,
        enumerable: false,
        get() {
          return rawWebDOMObjectFromEvent(this)?.descendants || [];
        }
      },
      kryRelations: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          const root = object?.element?.__kryMountRoot ||
            mountedRoot(this.currentTarget || this.target || null);
          return object && root ? webDOMRelationsForNode(root, object.node) : null;
        }
      },
      kryRelationRefs: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          const root = object?.element?.__kryMountRoot ||
            mountedRoot(this.currentTarget || this.target || null);
          return object && root ? webDOMRelationRefs(root, object.ref) : null;
        }
      },
      kryEventRefs: {
        configurable: true,
        enumerable: false,
        get() {
          const object = rawWebDOMObjectFromEvent(this);
          return object ? webNodeEventRefs(object.node) : null;
        }
      },
      kryMatches: {
        configurable: true,
        enumerable: false,
        value(selector) {
          return rawWebDOMObjectFromEvent(this)?.matches(selector) || false;
        }
      },
      kryClosest: {
        configurable: true,
        enumerable: false,
        value(selector) {
          return rawWebDOMObjectFromEvent(this)?.closest(selector) || null;
        }
      },
      kryQuery: {
        configurable: true,
        enumerable: false,
        value(selector) {
          return rawWebDOMObjectFromEvent(this)?.query(selector) || null;
        }
      },
      kryQueryAll: {
        configurable: true,
        enumerable: false,
        value(selector) {
          return rawWebDOMObjectFromEvent(this)?.queryAll(selector) || [];
        }
      }
    });
    Object.defineProperty(event, "__kryEventPropertiesBound", {
      configurable: true,
      enumerable: false,
      value: true
    });
  } catch {
    // Some host Event implementations are sealed; helper return values still work.
  }
  return event;
}

export function webDOMDecorateEvent(eventOrTarget) {
  const object = rawWebDOMObjectFromEvent(eventOrTarget);
  if (eventOrTarget?.target || eventOrTarget?.currentTarget)
    bindWebDOMEventProperties(eventOrTarget);
  return object;
}

export function webDOMObjectFromEvent(eventOrTarget) {
  return webDOMDecorateEvent(eventOrTarget);
}

export function webDOMIdentityFromEvent(eventOrTarget) {
  const object = webDOMObjectFromEvent(eventOrTarget);
  return object ? webNodeIdentity(object.node) : null;
}

export function webDOMElementMatches(element, selector) {
  const object = webDOMObjectFromElement(element);
  return !!object && selectorMatchesWebNode(parseSelector(String(selector || "").trim()), object.node);
}

export function webDOMObjects(target) {
  const root = mountedRoot(target);
  return webDOMObjectsFromRoot(root);
}

export function webDOMObjectMap(target) {
  const map = new Map();
  for (const object of webDOMObjects(target)) {
    for (const alias of webNodeIdentity(object.node).aliases) {
      if (alias && !map.has(alias))
        map.set(alias, object);
    }
  }
  return map;
}

export function webDOMObserve(target, selector, handler, options = {}) {
  const root = mountedRoot(target);
  if (!root || typeof handler !== "function" ||
      typeof root.addEventListener !== "function")
    return null;
  const text = String(selector || "").trim();
  const current = () => text ? webDOMQueryAll(root, text) : webDOMObjects(root);
  const emit = (event = null) => handler(current(), {
    root,
    frame: root.__kryFrame || null,
    event
  });
  const listener = (event) => emit(event);
  root.addEventListener("kry-render", listener);
  if (!options || options.immediate !== false)
    emit(null);
  return () => {
    if (typeof root.removeEventListener === "function")
      root.removeEventListener("kry-render", listener);
  };
}

function normalizeWebDOMBindHandlers(handlers) {
  if (typeof handlers === "function")
    return { mount: handlers, update: null, unmount: null };
  if (!handlers || typeof handlers !== "object")
    return null;
  return {
    mount: typeof handlers.mount === "function" ? handlers.mount : null,
    update: typeof handlers.update === "function" ? handlers.update : null,
    unmount: typeof handlers.unmount === "function" ? handlers.unmount : null
  };
}

export function webDOMBind(target, selector, handlers, options = {}) {
  const root = mountedRoot(target);
  const callbacks = normalizeWebDOMBindHandlers(handlers);
  if (!root || !callbacks || typeof root.addEventListener !== "function")
    return null;
  const active = new Map();
  const refresh = (event = null) => {
    const detail = { root, frame: root.__kryFrame || null, event };
    const next = new Map();
    for (const object of webDOMQueryAll(root, selector)) {
      const ref = object.ref || object.node?.path || "";
      if (!ref)
        continue;
      const previous = active.get(ref);
      if (previous) {
        next.set(ref, previous);
        previous.object = object;
        if (callbacks.update)
          callbacks.update(object, detail, previous.previousObject || null);
        previous.previousObject = object;
        continue;
      }
      const cleanup = callbacks.mount ? callbacks.mount(object, detail) : null;
      next.set(ref, {
        object,
        previousObject: object,
        cleanup: typeof cleanup === "function" ? cleanup : null
      });
    }
    for (const [ref, record] of active.entries()) {
      if (next.has(ref))
        continue;
      if (record.cleanup)
        record.cleanup(record.object, detail);
      if (callbacks.unmount)
        callbacks.unmount(record.object, detail);
    }
    active.clear();
    for (const [ref, record] of next.entries())
      active.set(ref, record);
  };
  const listener = (event) => refresh(event);
  root.addEventListener("kry-render", listener);
  if (!options || options.immediate !== false)
    refresh(null);
  return () => {
    if (typeof root.removeEventListener === "function")
      root.removeEventListener("kry-render", listener);
    const detail = { root, frame: root.__kryFrame || null, event: null };
    for (const record of active.values()) {
      if (record.cleanup)
        record.cleanup(record.object, detail);
      if (callbacks.unmount)
        callbacks.unmount(record.object, detail);
    }
    active.clear();
  };
}

const webDOMInternalAttributeNames = new Set([
  "class", "id", "name", "value", "title", "lang", "dir", "translate", "dirname",
  "placeholder", "tabindex", "role",
  "aria-label", "aria-description", "aria-describedby", "aria-labelledby",
  "aria-activedescendant",
  "aria-controls", "aria-owns", "aria-sort", "aria-orientation",
  "aria-level", "aria-posinset", "aria-setsize", "aria-haspopup",
  "aria-multiselectable", "aria-live",
  "href", "target", "rel", "for", "list", "usemap", "form", "part", "slot", "type", "action", "method", "enctype",
  "autocomplete", "hidden", "draggable", "spellcheck", "contenteditable",
  "autofocus", "inert", "autocapitalize", "enterkeyhint", "download", "formnovalidate", "novalidate", "popover",
  "popovertarget", "popovertargetaction", "readonly", "required", "min",
  "max", "step", "minlength", "maxlength", "pattern", "accept", "multiple",
  "inputmode", "headers", "scope", "colspan", "rowspan", "alt", "src",
  "checked", "disabled", "selected", "open"
]);

function dataSetKeyToAttrName(key) {
  return String(key || "").replace(/[A-Z]/g, (ch) => "-" + ch.toLowerCase());
}

function syncWebDOMRootIndexes(root) {
  if (!root)
    return;
  root.__kryNodes = new Map();
  root.__kryElementsByPath = new Map();
  root.__kryElementsByRef = new Map();
  root.__kryDomObjects = new Map();
  root.__kryElementsByName = new Map();
  root.__kryElementsByDomId = new Map();
  root.__kryElementsByDomName = new Map();
  root.__kryElementsBySource = new Map();
  root.__kryDomObjectsBySource = new Map();
  root.__kryFormValues = new Map();
  for (const el of root.__kryChildren?.values?.() || []) {
    const docNode = el.__kryDocNode || null;
    if (!docNode)
      continue;
    updateElementFormValue(el);
    if (docNode.path) {
      root.__kryNodes.set(docNode.path, docNode);
      root.__kryElementsByPath.set(docNode.path, el);
    }
    const ref = webNodeRef(docNode);
    if (ref) {
      root.__kryDomObjects.set(ref, makeWebDOMObject(root, docNode, el, ref));
      root.__kryElementsByRef.set(ref, el);
    }
    const sourceRef = webNodeSourceRef(docNode);
    if (sourceRef) {
      const sourceObject = makeWebDOMObject(root, docNode, el, sourceRef);
      if (!root.__kryDomObjects.has(sourceRef))
        root.__kryDomObjects.set(sourceRef, sourceObject);
      pushIndex(root.__kryDomObjectsBySource, sourceRef, sourceObject);
      pushIndex(root.__kryElementsBySource, sourceRef, el);
    }
    const sourceColumnRef = webNodeSourceColumnRef(docNode);
    if (sourceColumnRef) {
      const sourceColumnObject = makeWebDOMObject(root, docNode, el, sourceColumnRef);
      if (!root.__kryDomObjects.has(sourceColumnRef))
        root.__kryDomObjects.set(sourceColumnRef, sourceColumnObject);
      pushIndex(root.__kryDomObjectsBySource, sourceColumnRef, sourceColumnObject);
      pushIndex(root.__kryElementsBySource, sourceColumnRef, el);
    }
    const sourceRangeRef = webNodeSourceRangeRef(docNode);
    if (sourceRangeRef) {
      const sourceRangeObject = makeWebDOMObject(root, docNode, el, sourceRangeRef);
      if (!root.__kryDomObjects.has(sourceRangeRef))
        root.__kryDomObjects.set(sourceRangeRef, sourceRangeObject);
      pushIndex(root.__kryDomObjectsBySource, sourceRangeRef, sourceRangeObject);
      pushIndex(root.__kryElementsBySource, sourceRangeRef, el);
    }
    if (docNode.name)
      root.__kryElementsByName.set(docNode.name, el);
    if (docNode.domId)
      root.__kryElementsByDomId.set(docNode.domId, el);
    if (docNode.domName)
      root.__kryElementsByDomName.set(docNode.domName, el);
  }
}

function syncWebDOMElementFromNative(root, el) {
  const docNode = el?.__kryDocNode || null;
  if (!root || !el || !docNode)
    return null;
  const attrs = plainElementMap(el.attributes);
  const classes = String(el.className || "").split(/\s+/).filter(Boolean);
  docNode.classes = classes.filter((name) =>
    name !== "kryon-node" && name !== "kryon-" + docNode.kind.toLowerCase());
  docNode.domId = attrs.id ?? el.id ?? "";
  docNode.domName = attrs.name ?? el.name ?? "";
  docNode.domValue = attrs.value ?? docNode.domValue ?? "";
  docNode.title = attrs.title ?? docNode.title ?? "";
  docNode.lang = attrs.lang ?? docNode.lang ?? "";
  docNode.dir = attrs.dir ?? docNode.dir ?? "";
  docNode.translate = attrs.translate ?? docNode.translate ?? "";
  docNode.dirname = attrs.dirname ?? docNode.dirname ?? "";
  docNode.placeholder = attrs.placeholder ?? docNode.placeholder ?? "";
  docNode.role = attrs.role ?? docNode.role ?? "";
  syncDOMAriaRelationAttributes(docNode, attrs);
  docNode.dataList = attrs.list ?? docNode.dataList ?? "";
  docNode.useMap = attrs.usemap ?? docNode.useMap ?? "";
  docNode.formOwner = attrs.form ?? docNode.formOwner ?? "";
  docNode.part = attrs.part ?? docNode.part ?? "";
  docNode.slot = attrs.slot ?? docNode.slot ?? "";
  docNode.inert = !!el.inert || attrs.inert !== undefined;
  docNode.autoCapitalize = attrs.autocapitalize ?? docNode.autoCapitalize ?? "";
  docNode.enterKeyHint = attrs.enterkeyhint ?? docNode.enterKeyHint ?? "";
  docNode.headers = attrs.headers ?? docNode.headers ?? "";
  docNode.scope = attrs.scope ?? docNode.scope ?? "";
  docNode.colSpan = attrs.colspan ?? docNode.colSpan ?? "";
  docNode.rowSpan = attrs.rowspan ?? docNode.rowSpan ?? "";
  docNode.ariaSort = attrs["aria-sort"] ?? docNode.ariaSort ?? "";
  docNode.ariaOrientation = attrs["aria-orientation"] ?? docNode.ariaOrientation ?? "";
  docNode.ariaLevel = attrs["aria-level"] ?? docNode.ariaLevel ?? "";
  docNode.ariaPosInSet = attrs["aria-posinset"] ?? docNode.ariaPosInSet ?? "";
  docNode.ariaSetSize = attrs["aria-setsize"] ?? docNode.ariaSetSize ?? "";
  docNode.ariaHasPopup = attrs["aria-haspopup"] ?? docNode.ariaHasPopup ?? "";
  docNode.ariaMultiSelectable = attrs["aria-multiselectable"] ??
    docNode.ariaMultiSelectable ?? "";
  docNode.tabIndex = attrs.tabindex !== undefined && Number.isFinite(Number(attrs.tabindex))
    ? Math.trunc(Number(attrs.tabindex)) : docNode.tabIndex;
  const dataAttrs = {};
  const ariaAttrs = {};
  const extraAttrs = {};
  for (const [name, value] of Object.entries(attrs)) {
    const attr = String(name).toLowerCase();
    if (attr.startsWith("data-kry-"))
      continue;
    if (attr.startsWith("data-")) {
      dataAttrs[attr.slice(5)] = value;
      continue;
    }
    if (attr.startsWith("aria-")) {
      const ariaName = attr.slice(5);
      if (!["label", "description", "describedby", "details", "errormessage",
             "flowto", "controls", "owns", "sort", "orientation", "level", "posinset", "setsize",
             "haspopup", "multiselectable", "live"].includes(ariaName))
        ariaAttrs[ariaName] = value;
      continue;
    }
    if (!webDOMInternalAttributeNames.has(attr))
      extraAttrs[attr] = value;
  }
  for (const [key, value] of Object.entries(el.dataset || {})) {
    const attr = dataSetKeyToAttrName(key);
    if (attr && !attr.startsWith("kry-"))
      dataAttrs[attr] = String(value ?? "");
  }
  docNode.dataAttrs = dataAttrs;
  docNode.ariaAttrs = ariaAttrs;
  docNode.extraAttrs = extraAttrs;
  for (const key of ["disabled", "checked", "selected", "open"])
    docNode.state[key] = !!el[key] || attrs[key] !== undefined;
  if (docNode.tag !== "input" && docNode.tag !== "textarea" && el.textContent !== undefined && el.textContent !== null)
    docNode.text = String(el.textContent);
  updateElementFormValue(el);
  syncDOMScrollMutation(el);
  docNode.styleFacts = webNodeStyleFacts(docNode);
  syncWebDOMRootIndexes(root);
  return makeWebDOMObject(root, docNode, el);
}

export function webDOMSync(target, query = "") {
  const root = mountedRoot(target);
  if (!root)
    return query ? null : [];
  const text = String(query || "").trim();
  if (text) {
    const el = findWebElement(root, text);
    const object = syncWebDOMElementFromNative(root, el);
    resolveWebDOMRelations(root);
    return object;
  }
  const objects = [];
  for (const el of root.__kryChildren?.values?.() || []) {
    const object = syncWebDOMElementFromNative(root, el);
    if (object)
      objects.push(object);
  }
  syncWebDOMRootIndexes(root);
  resolveWebDOMRelations(root);
  return objects;
}

function plainElementMap(source) {
  if (!source)
    return {};
  const out = {};
  if (typeof source.length === "number" && typeof source.item === "function") {
    for (let i = 0; i < source.length; i++) {
      const attr = source.item(i);
      if (attr?.name)
        out[attr.name] = String(attr.value ?? "");
    }
    return out;
  }
  for (const [key, value] of Object.entries(source || {})) {
    if (typeof value !== "function" && value !== undefined && value !== null)
      out[key] = String(value);
  }
  return out;
}

export function webNodeEventRefs(node) {
  return {
    click: node?.onClick || "",
    doubleClick: node?.onDoubleClick || "",
    input: node?.onInput || "",
    beforeInput: node?.onBeforeInput || "",
    change: node?.onChange || "",
    select: node?.onSelect || "",
    key: node?.onKey || "",
    keyUp: node?.onKeyUp || "",
    invalid: node?.onInvalid || "",
    submit: node?.onSubmit || "",
    reset: node?.onReset || "",
    toggle: node?.onToggle || "",
    close: node?.onClose || "",
    cancel: node?.onCancel || "",
    focus: node?.onFocus || "",
    blur: node?.onBlur || "",
    scroll: node?.onScroll || "",
    mouseEnter: node?.onMouseEnter || "",
    mouseLeave: node?.onMouseLeave || "",
    mouseMove: node?.onMouseMove || "",
    mouseDown: node?.onMouseDown || "",
    mouseUp: node?.onMouseUp || "",
    pointerEnter: node?.onPointerEnter || "",
    pointerLeave: node?.onPointerLeave || "",
    pointerMove: node?.onPointerMove || "",
    pointerDown: node?.onPointerDown || "",
    pointerUp: node?.onPointerUp || "",
    pointerCancel: node?.onPointerCancel || "",
    wheel: node?.onWheel || "",
    contextMenu: node?.onContextMenu || "",
    dragStart: node?.onDragStart || "",
    dragEnd: node?.onDragEnd || "",
    dragOver: node?.onDragOver || "",
    drop: node?.onDrop || "",
    copy: node?.onCopy || "",
    cut: node?.onCut || "",
    paste: node?.onPaste || ""
  };
}

function webNodeRelationList(rt, value) {
  const text = String(value || "").trim();
  if (!rt || !text)
    return [];
  return text
    .split(/\s+/)
    .map((token) => webNodeQuery(rt, token))
    .filter(Boolean);
}

function webNodeReverseRelationList(rt, node, field) {
  const ref = webNodeRef(node);
  if (!rt || !node || !ref)
    return [];
  const out = [];
  for (const candidate of webDocumentFrame(rt).nodes || []) {
    if (!candidate || candidate === node)
      continue;
    const related = webNodeRelationList(rt, candidate[field] || "");
    if (related.some((relatedNode) => relatedNode === node || webNodeRef(relatedNode) === ref))
      out.push(candidate);
  }
  return out;
}

function webNodeFormControls(rt, node) {
  const explicit = webNodeReverseRelationList(rt, node, "formOwner");
  if (!rt || !node?.path)
    return explicit;
  const descendants = webNodeDescendants(rt, node.path)
    .filter((candidate) => webNodeIsFormControl(candidate));
  return mergeWebNodeRelations(explicit, descendants);
}

function webNodeImplicitLabelControl(rt, node) {
  if (!webNodeIsLabel(node))
    return null;
  return webNodeDescendants(rt, node.path)
    .find((candidate) => webNodeIsFormControl(candidate)) || null;
}

function webNodeImplicitLabels(rt, node) {
  if (!rt || !node?.path || !webNodeIsFormControl(node))
    return [];
  const out = [];
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeIsLabel(parent))
      out.push(parent);
    parent = webNodeParent(rt, parent.path);
  }
  return out;
}

function mergeWebNodeRelationRefs(...lists) {
  const out = [];
  const seen = new Set();
  for (const list of lists) {
    for (const node of list || []) {
      const ref = webNodeRef(node);
      if (!ref || seen.has(ref))
        continue;
      seen.add(ref);
      out.push(ref);
    }
  }
  return out;
}

function webNodeScopedHeaderList(rt, node, scope) {
  const expected = new Set(Array.isArray(scope) ? scope.map((item) => String(item).toLowerCase())
    : [String(scope || "").toLowerCase()]);
  return webNodeRelationList(rt, node?.headers || "")
    .filter((header) => expected.has(String(header?.scope || "").toLowerCase()));
}

function webNodeGroupOwner(rt, node) {
  if (!rt || !node || String(node.tag || "").toLowerCase() === "legend")
    return null;
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeIsSemanticGroup(parent))
      return parent;
    parent = webNodeParent(rt, parent.path);
  }
  return null;
}

function webNodeGroupMembers(rt, node) {
  if (!rt || !webNodeIsSemanticGroup(node))
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeGroupOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeDisabledOwner(rt, node) {
  if (!rt || !node)
    return null;
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeIsDisabledScope(parent)) {
      if (String(parent.tag || "").toLowerCase() === "fieldset" &&
          webNodeCanOwnLegend(parent, node)) {
        parent = webNodeParent(rt, parent.path);
        continue;
      }
      return parent;
    }
    parent = webNodeParent(rt, parent.path);
  }
  return null;
}

function webNodeDisabledMembers(rt, node) {
  if (!rt || !webNodeIsDisabledScope(node))
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeDisabledOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeCollectionOwner(rt, node) {
  if (!rt || !node)
    return null;
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeCanOwnCollectionMember(parent, node))
      return parent;
    parent = webNodeParent(rt, parent.path);
  }
  return null;
}

function webNodeCollectionItems(rt, node) {
  if (!rt || !webNodeCollectionMemberRoles(node))
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeCollectionOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeLandmarkOwner(rt, node) {
  if (!rt || !node)
    return null;
  let parent = webNodeParent(rt, node.path);
  while (parent) {
    if (webNodeIsLandmark(parent))
      return parent;
    parent = webNodeParent(rt, parent.path);
  }
  return null;
}

function webNodeLandmarkMembers(rt, node) {
  if (!rt || !webNodeIsLandmark(node))
    return [];
  const ref = webNodeRef(node);
  return (webDocumentFrame(rt).nodes || []).filter((candidate) => {
    if (!candidate || candidate === node)
      return false;
    const owner = webNodeLandmarkOwner(rt, candidate);
    return owner === node || (ref && webNodeRef(owner) === ref);
  });
}

function webNodeSelectedCollectionOwner(rt, node) {
  return webNodeIsSelectedCollectionMember(node)
    ? webNodeCollectionOwner(rt, node)
    : null;
}

function webNodeSelectedCollectionItems(rt, node) {
  return webNodeCollectionItems(rt, node)
    .filter((candidate) => webNodeIsSelectedCollectionMember(candidate));
}

function webNodeActiveCollectionOwner(rt, node) {
  return webNodeReverseRelationList(rt, node, "ariaActiveDescendant")
    .find((candidate) => webNodeCanOwnCollectionMember(candidate, node)) || null;
}

function webNodeActiveCollectionItems(rt, node) {
  return webNodeRelationList(rt, node?.ariaActiveDescendant)
    .filter((candidate) => webNodeCanOwnCollectionMember(node, candidate));
}

function webNodeRefs(nodes) {
  return (nodes || []).map((node) => webNodeRef(node)).filter(Boolean);
}

function mergeWebNodeRelations(...lists) {
  const out = [];
  const seen = new Set();
  for (const list of lists) {
    for (const node of list || []) {
      const ref = webNodeRef(node);
      if (!node || !ref || seen.has(ref))
        continue;
      seen.add(ref);
      out.push(node);
    }
  }
  return out;
}

function webNodeRelationsForNode(rt, node) {
  if (!node)
    return null;
  return {
    previousSibling: webNodePreviousSiblingFromFrame(node),
    nextSibling: webNodeNextSiblingFromFrame(node),
    groupOwner: webNodeGroupOwner(rt, node),
    groupMembers: webNodeGroupMembers(rt, node),
    disabledOwner: webNodeDisabledOwner(rt, node),
    disabledMembers: webNodeDisabledMembers(rt, node),
    landmarkOwner: webNodeLandmarkOwner(rt, node),
    landmarkMembers: webNodeLandmarkMembers(rt, node),
    collectionOwner: webNodeCollectionOwner(rt, node),
    collectionItems: webNodeCollectionItems(rt, node),
    captionOwner: webNodeCaptionOwner(rt, node),
    captionItems: webNodeCaptionItems(rt, node),
    summaryOwner: webNodeSummaryOwner(rt, node),
    summaryItems: webNodeSummaryItems(rt, node),
    legendOwner: webNodeLegendOwner(rt, node),
    legendItems: webNodeLegendItems(rt, node),
    descriptionListOwner: webNodeDescriptionListOwner(rt, node),
    descriptionListItems: webNodeDescriptionListItems(rt, node),
    selectedCollectionOwner: webNodeSelectedCollectionOwner(rt, node),
    selectedCollectionItems: webNodeSelectedCollectionItems(rt, node),
    activeCollectionOwner: webNodeActiveCollectionOwner(rt, node),
    activeCollectionItems: webNodeActiveCollectionItems(rt, node),
    describedBy: webNodeRelationList(rt, node.ariaDescribedBy),
    describes: webNodeReverseRelationList(rt, node, "ariaDescribedBy"),
    details: webNodeRelationList(rt, node.ariaDetails)[0] || null,
    detailedBy: webNodeReverseRelationList(rt, node, "ariaDetails"),
    errorMessage: webNodeRelationList(rt, node.ariaErrorMessage)[0] || null,
    errorFor: webNodeReverseRelationList(rt, node, "ariaErrorMessage"),
    flowTo: webNodeRelationList(rt, node.ariaFlowTo),
    flowFrom: webNodeReverseRelationList(rt, node, "ariaFlowTo"),
    controls: webNodeRelationList(rt, node.ariaControls),
    controlledBy: webNodeReverseRelationList(rt, node, "ariaControls"),
    owns: webNodeRelationList(rt, node.ariaOwns),
    ownedBy: webNodeReverseRelationList(rt, node, "ariaOwns"),
    headers: webNodeRelationList(rt, node.headers),
    headerFor: webNodeReverseRelationList(rt, node, "headers"),
    rowHeaders: webNodeScopedHeaderList(rt, node, ["row", "rowgroup"]),
    columnHeaders: webNodeScopedHeaderList(rt, node, ["col", "colgroup"]),
    rowGroupHeaders: webNodeScopedHeaderList(rt, node, "rowgroup"),
    columnGroupHeaders: webNodeScopedHeaderList(rt, node, "colgroup"),
    labelFor: webNodeIsLabel(node) && (webNodeRelationList(rt, node.htmlFor)[0] ||
      webNodeImplicitLabelControl(rt, node)) || null,
    outputFor: webNodeIsOutput(node) ? webNodeRelationList(rt, node.htmlFor) : [],
    outputBy: webNodeReverseRelationList(rt, node, "htmlFor")
      .filter((candidate) => webNodeIsOutput(candidate)),
    dataList: webNodeRelationList(rt, node.dataList)[0] || null,
    listedBy: webNodeReverseRelationList(rt, node, "dataList"),
    imageMap: webNodeRelationList(rt, node.useMap)[0] || null,
    mappedImages: webNodeReverseRelationList(rt, node, "useMap"),
    formOwner: webNodeRelationList(rt, node.formOwner)[0] || null,
    formControls: webNodeFormControls(rt, node),
    labelledBy: mergeWebNodeRelations(
      webNodeRelationList(rt, node.ariaLabelledBy),
      webNodeReverseRelationList(rt, node, "htmlFor")
        .filter((candidate) => webNodeIsLabel(candidate)),
      webNodeImplicitLabels(rt, node)
    ),
    activeDescendant: webNodeRelationList(rt, node.ariaActiveDescendant)[0] || null,
    activeDescendantOf: webNodeReverseRelationList(rt, node, "ariaActiveDescendant"),
    popoverTarget: webNodeRelationList(rt, node.popoverTarget)[0] || null,
    popoverInvokers: webNodeReverseRelationList(rt, node, "popoverTarget")
  };
}

export function webNodeRelations(rt, query) {
  return webNodeRelationsForNode(rt, webNodeQuery(rt, query));
}

function webNodeRelationRefsForNode(rt, node) {
  if (!node)
    return null;
  return {
    previousSibling: webNodeRef(webNodePreviousSiblingFromFrame(node)) || "",
    nextSibling: webNodeRef(webNodeNextSiblingFromFrame(node)) || "",
    describedBy: webNodeRefs(webNodeRelationList(rt, node.ariaDescribedBy)),
    describes: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaDescribedBy")),
    details: webNodeRef(webNodeRelationList(rt, node.ariaDetails)[0]) || "",
    detailedBy: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaDetails")),
    errorMessage: webNodeRef(webNodeRelationList(rt, node.ariaErrorMessage)[0]) || "",
    errorFor: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaErrorMessage")),
    flowTo: webNodeRefs(webNodeRelationList(rt, node.ariaFlowTo)),
    flowFrom: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaFlowTo")),
    controls: webNodeRefs(webNodeRelationList(rt, node.ariaControls)),
    controlledBy: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaControls")),
    owns: webNodeRefs(webNodeRelationList(rt, node.ariaOwns)),
    ownedBy: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaOwns")),
    headers: webNodeRefs(webNodeRelationList(rt, node.headers)),
    headerFor: webNodeRefs(webNodeReverseRelationList(rt, node, "headers")),
    rowHeaders: webNodeRefs(webNodeScopedHeaderList(rt, node, ["row", "rowgroup"])),
    columnHeaders: webNodeRefs(webNodeScopedHeaderList(rt, node, ["col", "colgroup"])),
    rowGroupHeaders: webNodeRefs(webNodeScopedHeaderList(rt, node, "rowgroup")),
    columnGroupHeaders: webNodeRefs(webNodeScopedHeaderList(rt, node, "colgroup")),
    labelFor: webNodeIsLabel(node) && webNodeRef(webNodeRelationList(rt, node.htmlFor)[0] ||
      webNodeImplicitLabelControl(rt, node)) || "",
    outputFor: webNodeIsOutput(node) ? webNodeRefs(webNodeRelationList(rt, node.htmlFor)) : [],
    outputBy: webNodeRefs(webNodeReverseRelationList(rt, node, "htmlFor")
      .filter((candidate) => webNodeIsOutput(candidate))),
    dataList: webNodeRef(webNodeRelationList(rt, node.dataList)[0]) || "",
    listedBy: webNodeRefs(webNodeReverseRelationList(rt, node, "dataList")),
    imageMap: webNodeRef(webNodeRelationList(rt, node.useMap)[0]) || "",
    mappedImages: webNodeRefs(webNodeReverseRelationList(rt, node, "useMap")),
    formOwner: webNodeRef(webNodeRelationList(rt, node.formOwner)[0]) || "",
    formControls: webNodeRefs(webNodeFormControls(rt, node)),
    labelledBy: mergeWebNodeRelationRefs(
      webNodeRelationList(rt, node.ariaLabelledBy),
      webNodeReverseRelationList(rt, node, "htmlFor")
        .filter((candidate) => webNodeIsLabel(candidate)),
      webNodeImplicitLabels(rt, node)
    ),
    groupOwner: webNodeRef(webNodeGroupOwner(rt, node)) || "",
    groupMembers: webNodeRefs(webNodeGroupMembers(rt, node)),
    disabledOwner: webNodeRef(webNodeDisabledOwner(rt, node)) || "",
    disabledMembers: webNodeRefs(webNodeDisabledMembers(rt, node)),
    landmarkOwner: webNodeRef(webNodeLandmarkOwner(rt, node)) || "",
    landmarkMembers: webNodeRefs(webNodeLandmarkMembers(rt, node)),
    collectionOwner: webNodeRef(webNodeCollectionOwner(rt, node)) || "",
    collectionItems: webNodeRefs(webNodeCollectionItems(rt, node)),
    captionOwner: webNodeRef(webNodeCaptionOwner(rt, node)) || "",
    captionItems: webNodeRefs(webNodeCaptionItems(rt, node)),
    summaryOwner: webNodeRef(webNodeSummaryOwner(rt, node)) || "",
    summaryItems: webNodeRefs(webNodeSummaryItems(rt, node)),
    legendOwner: webNodeRef(webNodeLegendOwner(rt, node)) || "",
    legendItems: webNodeRefs(webNodeLegendItems(rt, node)),
    descriptionListOwner: webNodeRef(webNodeDescriptionListOwner(rt, node)) || "",
    descriptionListItems: webNodeRefs(webNodeDescriptionListItems(rt, node)),
    selectedCollectionOwner: webNodeRef(webNodeSelectedCollectionOwner(rt, node)) || "",
    selectedCollectionItems: webNodeRefs(webNodeSelectedCollectionItems(rt, node)),
    activeCollectionOwner: webNodeRef(webNodeActiveCollectionOwner(rt, node)) || "",
    activeCollectionItems: webNodeRefs(webNodeActiveCollectionItems(rt, node)),
    activeDescendant: webNodeRef(webNodeRelationList(rt, node.ariaActiveDescendant)[0]) || "",
    activeDescendantOf: webNodeRefs(webNodeReverseRelationList(rt, node, "ariaActiveDescendant")),
    popoverTarget: webNodeRef(webNodeRelationList(rt, node.popoverTarget)[0]) || "",
    popoverInvokers: webNodeRefs(webNodeReverseRelationList(rt, node, "popoverTarget"))
  };
}

export function webNodeRelationRefs(rt, query) {
  return webNodeRelationRefsForNode(rt, webNodeQuery(rt, query));
}

function webNodeSnapshotForNode(rt, node) {
  if (!node)
    return null;
  const identity = webNodeIdentity(node);
  const query = node.path || node.webRef || identity.ref;
  const range = webNodeValueRange(node);
  return {
    ref: identity.ref,
    aliases: identity.aliases,
    identity,
    index: node.index || 0,
    kind: node.kind || "",
    tag: node.tag || "",
    role: node.role || implicitRole(node),
    path: node.path || "",
    webRef: node.webRef || "",
    parentPath: node.parentPath || "",
    parentRef: webNodeRef(webNodeParent(rt, query)) || "",
    childRefs: webNodeChildren(rt, query).map((child) => webNodeRef(child)).filter(Boolean),
    relationRefs: webNodeRelationRefsForNode(rt, node),
    eventRefs: webNodeEventRefs(node),
    name: node.name || "",
    key: node.key || "",
    id: node.domId || "",
    domName: node.domName || "",
    classes: [...(node.classes || [])],
    sourcePath: node.sourcePath || "",
    sourceLine: node.sourceLine || 0,
    sourceColumn: node.sourceColumn || 0,
    sourceEndLine: node.sourceEndLine || 0,
    sourceEndColumn: node.sourceEndColumn || 0,
    sourceRef: webNodeSourceRef(node),
    sourceColumnRef: webNodeSourceColumnRef(node),
    sourceRangeRef: webNodeSourceRangeRef(node),
    styleFacts: webNodeStyleFacts(node),
    text: node.text ?? "",
    value: node.domValue ?? node.value ?? "",
    alt: node.alt || "",
    asset: node.asset || "",
    src: node.asset || "",
    min: range.min,
    max: range.max,
    valueNow: range.valueNow,
    state: { ...(node.state || {}) },
    attrs: {},
    dataset: {},
    style: {},
    rect: null,
    scroll: null
  };
}

export function webNodeSnapshot(rt, query) {
  return webNodeSnapshotForNode(rt, webNodeQuery(rt, query));
}

export function webNodeSnapshots(rt, selector = "") {
  const text = String(selector || "").trim();
  const nodes = text ? webNodeQueryAll(rt, text) : (webDocumentFrame(rt).nodes || []);
  return nodes.map((node) => webNodeSnapshotForNode(rt, node)).filter(Boolean);
}

function webDOMObjectSnapshot(target, object) {
  if (!object)
    return null;
  const node = object.node || {};
  const el = object.element || {};
  const relations = webDOMRelationsForNode(target, node);
  const identity = webNodeIdentity(node);
  const range = webNodeValueRange(node);
  return {
    ref: object.ref || "",
    aliases: identity.aliases,
    identity,
    index: node.index || 0,
    kind: node.kind || "",
    tag: node.tag || String(el.tagName || "").toLowerCase(),
    role: node.role || implicitRole(node),
    path: node.path || "",
    webRef: node.webRef || "",
    parentPath: node.parentPath || "",
    parentRef: webDOMParent(target, node.path)?.ref || "",
    childRefs: webDOMChildren(target, node.path).map((child) => child.ref),
    relationRefs: webDOMRelationRefsForRelations(relations),
    eventRefs: webNodeEventRefs(node),
    name: node.name || "",
    key: node.key || "",
    id: node.domId || el.id || "",
    domName: node.domName || "",
    classes: [...(node.classes || [])],
    sourcePath: node.sourcePath || "",
    sourceLine: node.sourceLine || 0,
    sourceColumn: node.sourceColumn || 0,
    sourceEndLine: node.sourceEndLine || 0,
    sourceEndColumn: node.sourceEndColumn || 0,
    sourceRef: webNodeSourceRef(node),
    sourceColumnRef: webNodeSourceColumnRef(node),
    sourceRangeRef: webNodeSourceRangeRef(node),
    styleFacts: webNodeStyleFacts(node),
    text: webDOMGetText(target, node.path) ?? node.text ?? "",
    value: webDOMGetValue(target, node.path),
    alt: node.alt || el.getAttribute?.("alt") || "",
    asset: node.asset || "",
    src: node.asset || el.getAttribute?.("src") || "",
    min: range.min,
    max: range.max,
    valueNow: range.valueNow,
    state: { ...(node.state || {}) },
    attrs: plainElementMap(el.attributes),
    dataset: plainElementMap(el.dataset),
    style: plainElementMap(el.style),
    rect: webDOMRect(target, node.path),
    scroll: webDOMGetScroll(target, node.path)
  };
}

export function webDOMSnapshot(target, query) {
  return webDOMObjectSnapshot(target, webDOMObject(target, query));
}

export function webDOMSnapshots(target, selector = "") {
  const text = String(selector || "").trim();
  const objects = text ? webDOMQueryAll(target, text) : webDOMObjects(target);
  return objects.map((object) => webDOMObjectSnapshot(target, object)).filter(Boolean);
}

export function webDOMAccessibilitySnapshot(target, selector = "") {
  const root = mountedRoot(target);
  const frame = root?.__kryFrame || null;
  return {
    title: frame?.metadata?.title || "",
    description: frame?.metadata?.description || "",
    nodes: webDOMSnapshots(root || target, selector).map(webAccessibilityNodeFromDOMSnapshot)
  };
}

export function webDOMSnapshotFromElement(element) {
  const object = webDOMObjectFromElement(element);
  const root = object?.element?.__kryMountRoot || mountedRoot(object?.element || null);
  return object ? webDOMObjectSnapshot(root, object) : null;
}

export function webDOMSnapshotFromEvent(eventOrTarget) {
  const object = webDOMObjectFromEvent(eventOrTarget);
  const root = object?.element?.__kryMountRoot || mountedRoot(object?.element || null);
  return object ? webDOMObjectSnapshot(root, object) : null;
}

export function webDOMParent(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(target, query) : null;
  const parentPath = object?.node?.parentPath || "";
  if (!root || !object || !parentPath || parentPath === object.node.path)
    return null;
  return webDOMObjectForNode(root, root.__kryNodes?.get(parentPath));
}

export function webDOMAncestors(target, query) {
  const ancestors = [];
  let object = webDOMParent(target, query);
  while (object) {
    ancestors.push(object);
    object = webDOMParent(target, object.node.path);
  }
  return ancestors;
}

export function webDOMPreviousSibling(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(target, query) : null;
  return object ? webDOMSiblingObject(root, object.node, -1) : null;
}

export function webDOMNextSibling(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(target, query) : null;
  return object ? webDOMSiblingObject(root, object.node, 1) : null;
}

function webDOMSiblingObjects(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(target, query) : null;
  return object ? webDOMChildren(root, object.node.parentPath || "")
    .filter((sibling) => sibling?.node) : [];
}

export function webDOMPreviousSiblings(target, query) {
  const object = webDOMObject(target, query);
  const siblings = webDOMSiblingObjects(target, query);
  const index = siblings.findIndex((sibling) =>
    sibling.node === object?.node || sibling.node.path === object?.node?.path);
  return index <= 0 ? [] : siblings.slice(0, index);
}

export function webDOMNextSiblings(target, query) {
  const object = webDOMObject(target, query);
  const siblings = webDOMSiblingObjects(target, query);
  const index = siblings.findIndex((sibling) =>
    sibling.node === object?.node || sibling.node.path === object?.node?.path);
  return index < 0 ? [] : siblings.slice(index + 1);
}

export function webDOMSiblings(target, query) {
  const object = webDOMObject(target, query);
  return webDOMSiblingObjects(target, query)
    .filter((sibling) => sibling.node !== object?.node &&
      sibling.node.path !== object?.node?.path);
}

export function webDOMChildren(target, query = "") {
  const root = mountedRoot(target);
  if (!root)
    return [];
  const text = String(query || "").trim();
  const parent = text ? webDOMObject(target, text) : null;
  if (text && !parent)
    return [];
  return webDOMObjects(target).filter((object) => {
    const parentPath = object.node.parentPath || "";
    if (parent)
      return parentPath === parent.node.path && object.node.path !== parent.node.path;
    return !parentPath || parentPath === object.node.path || !root.__kryNodes?.has(parentPath);
  });
}

export function webDOMDescendants(target, query = "") {
  const root = mountedRoot(target);
  if (!root)
    return [];
  const text = String(query || "").trim();
  const parent = text ? webDOMObject(target, text) : null;
  if (text && !parent)
    return [];
  const parentPath = parent?.node?.path || "";
  if (!parentPath)
    return webDOMObjects(target);
  return webDOMObjects(target).filter((object) =>
    object.node.path !== parentPath &&
    object.node.path.startsWith(parentPath + "/"));
}

export function webDOMClosest(target, query, selector) {
  const root = mountedRoot(target);
  let object = root ? webDOMObject(target, query) : null;
  const parsed = parseSelector(String(selector || "").trim());
  while (object) {
    if (selectorMatchesWebNode(parsed, object.node))
      return object;
    object = webDOMParent(target, object.node.path);
  }
  return null;
}

export function webDOMQueryAll(target, selector) {
  const text = String(selector || "").trim();
  if (!text)
    return [];
  const root = mountedRoot(target);
  if (root?.__kryDomObjectsBySource?.has(text))
    return [...root.__kryDomObjectsBySource.get(text)];
  const exact = webDOMObject(target, text);
  if (exact)
    return [exact];
  const parsed = parseSelector(text);
  return webDOMObjects(target)
    .filter((object) => selectorMatchesWebNode(parsed, object.node));
}

export function webDOMQuery(target, selector) {
  return webDOMQueryAll(target, selector)[0] || null;
}

export function webDOMQueryAllWithin(target, query, selector) {
  const text = String(selector || "").trim();
  if (!text)
    return [];
  const scope = webDOMObject(target, query)?.node || null;
  const parsed = parseSelector(text);
  return webDOMDescendants(target, query)
    .filter((object) => selectorMatchesWebNode(parsed, object.node, scope));
}

export function webDOMQueryWithin(target, query, selector) {
  return webDOMQueryAllWithin(target, query, selector)[0] || null;
}

export function webDOMMatches(target, query, selector) {
  const object = webDOMObject(target, query);
  return !!object && selectorMatchesWebNode(parseSelector(String(selector || "").trim()), object.node);
}

export function webDOMObjectsAtSource(target, sourcePath, sourceLine, sourceColumn = 0) {
  const ref = webSourceRef(sourcePath, sourceLine, sourceColumn);
  return ref ? webDOMQueryAll(target, ref) : [];
}

export function webDOMObjectAtSource(target, sourcePath, sourceLine, sourceColumn = 0) {
  return webDOMObjectsAtSource(target, sourcePath, sourceLine, sourceColumn)[0] || null;
}

export function webDOMObjectsAtSourceRange(target, sourcePath, sourceLine, sourceColumn = 0) {
  const root = mountedRoot(target);
  const path = String(sourcePath || "").trim();
  const line = Number.isFinite(Number(sourceLine)) ? Math.trunc(Number(sourceLine)) : 0;
  const column = Number.isFinite(Number(sourceColumn)) ? Math.trunc(Number(sourceColumn)) : 0;
  if (!root || !path || line <= 0)
    return [];
  return sortWebDOMObjectsDeepestFirst(webDOMObjects(target)
    .filter((object) => sourcePositionWithinNode(object.node, path, line, column)));
}

export function webDOMObjectAtSourceRange(target, sourcePath, sourceLine, sourceColumn = 0) {
  return webDOMObjectsAtSourceRange(target, sourcePath, sourceLine, sourceColumn)[0] || null;
}

export function webDOMObjectsOverlappingSourceRange(target, sourcePath,
                                                     sourceLine,
                                                     sourceColumn = 0,
                                                     sourceEndLine = sourceLine,
                                                     sourceEndColumn = sourceColumn) {
  const root = mountedRoot(target);
  const path = String(sourcePath || "").trim();
  if (!root || !path)
    return [];
  return sortWebDOMObjectsDeepestFirst(webDOMObjects(target)
    .filter((object) => sourceRangeOverlapsNode(object.node, path,
      sourceLine, sourceColumn, sourceEndLine, sourceEndColumn)));
}

export function webDOMObjectOverlappingSourceRange(target, sourcePath,
                                                    sourceLine,
                                                    sourceColumn = 0,
                                                    sourceEndLine = sourceLine,
                                                    sourceEndColumn = sourceColumn) {
  return webDOMObjectsOverlappingSourceRange(target, sourcePath, sourceLine,
    sourceColumn, sourceEndLine, sourceEndColumn)[0] || null;
}

export function webDOMSourceMap(target) {
  return webDOMObjects(target).filter((object) => webNodeHasSource(object.node));
}

function cleanDOMEventType(type) {
  return String(type || "").trim();
}

export function webDOMAddEventListener(target, query, type, handler, options) {
  const eventType = cleanDOMEventType(type);
  const el = eventType && typeof handler === "function" ? findWebElement(target, query) : null;
  if (!el || typeof el.addEventListener !== "function")
    return null;
  const listener = (event) => {
    bindWebDOMEventProperties(event);
    const object = webDOMObjectFromElement(event?.target || el) ||
      webDOMObjectFromElement(el);
    return handler(event, object);
  };
  el.addEventListener(eventType, listener, options);
  return () => {
    if (typeof el.removeEventListener === "function")
      el.removeEventListener(eventType, listener, options);
  };
}

export function webDOMAddDelegatedEventListener(target, selector, type, handler, options) {
  const root = mountedRoot(target);
  const eventType = cleanDOMEventType(type);
  if (!root || !eventType || typeof handler !== "function" ||
      typeof root.addEventListener !== "function")
    return null;
  const parsed = parseSelector(String(selector || "").trim());
  const listener = (event) => {
    bindWebDOMEventProperties(event);
    let object = webDOMObjectFromElement(event?.target || null);
    while (object) {
      if (selectorMatchesWebNode(parsed, object.node))
        return handler(event, object);
      const parentPath = object.node?.parentPath || "";
      object = parentPath && parentPath !== object.node.path
        ? webDOMObjectForNode(root, root.__kryNodes?.get(parentPath))
        : null;
    }
    return undefined;
  };
  root.addEventListener(eventType, listener, options);
  return () => {
    if (typeof root.removeEventListener === "function")
      root.removeEventListener(eventType, listener, options);
  };
}

function cleanDOMClassName(name) {
  const value = String(name || "").trim();
  return value && !/\s/.test(value) ? value : "";
}

function syncDOMClassMutation(el) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  const classNames = String(el.className || "").split(/\s+/).filter(Boolean);
  docNode.classes = classNames.filter((name) =>
    name !== "kryon-node" && name !== "kryon-" + docNode.kind.toLowerCase());
  docNode.styleFacts = webNodeStyleFacts(docNode);
  applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
    ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
    : null);
  return docNode;
}

function setDOMClass(target, query, className, enabled) {
  const name = cleanDOMClassName(className);
  const el = name ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const extras = el.__kryExtraClasses || new Set();
  if (enabled)
    extras.add(name);
  else
    extras.delete(name);
  el.__kryExtraClasses = extras;
  const base = ["kryon-node", "kryon-" + (el.__kryDocNode?.kind || "").toLowerCase()];
  const declared = (el.__kryDocNode?.classes || []).filter((item) => item !== name);
  el.className = [...new Set([...base, ...declared, ...extras])].filter(Boolean).join(" ");
  syncDOMClassMutation(el);
  return true;
}

export function webDOMAddClass(target, query, className) {
  return setDOMClass(target, query, className, true);
}

export function webDOMRemoveClass(target, query, className) {
  return setDOMClass(target, query, className, false);
}

export function webDOMToggleClass(target, query, className, force) {
  const name = cleanDOMClassName(className);
  const el = name ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const has = (el.__kryExtraClasses || new Set()).has(name) ||
    String(el.className || "").split(/\s+/).includes(name);
  return setDOMClass(target, query, name, force === undefined ? !has : !!force);
}

export function webDOMHasClass(target, query, className) {
  const name = cleanDOMClassName(className);
  const el = name ? findWebElement(target, query) : null;
  return !!el && String(el.className || "").split(/\s+/).includes(name);
}

const webDOMStateNames = new Set([
  "disabled", "loading", "selected", "checked", "invalid", "valid",
  "indeterminate", "default", "autofill", "placeholder-shown", "expanded",
  "open", "hover", "pressed", "focus"
]);

function cleanDOMStateName(name) {
  const key = String(name || "").trim().toLowerCase().replace(/_/g, "-");
  if (key === "focused")
    return "focus";
  if (key === "active")
    return "pressed";
  if (key === "focus-visible")
    return "focus";
  return webDOMStateNames.has(key) ? key : "";
}

function syncDOMStateMutation(el) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  docNode.styleFacts = webNodeStyleFacts(docNode);
  applyWebNode(el, docNode, el.__kryRuntime || null);
  applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
    ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
    : null);
  return docNode;
}

export function webDOMSetState(target, query, name, value) {
  const key = cleanDOMStateName(name);
  const el = key ? findWebElement(target, query) : null;
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return false;
  const extra = { ...(el.__kryExtraState || {}) };
  extra[key] = !!value;
  el.__kryExtraState = extra;
  docNode.state[key] = !!value;
  syncDOMStateMutation(el);
  return true;
}

export function webDOMToggleState(target, query, name, force) {
  const key = cleanDOMStateName(name);
  const el = key ? findWebElement(target, query) : null;
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return false;
  const next = force === undefined ? !docNode.state[key] : !!force;
  return webDOMSetState(target, query, key, next);
}

export function webDOMGetState(target, query, name) {
  const key = cleanDOMStateName(name);
  const el = key ? findWebElement(target, query) : null;
  const docNode = el?.__kryDocNode;
  return docNode ? !!docNode.state[key] : undefined;
}

function cleanDOMAttributeName(name) {
  const value = String(name || "").trim();
  return value && /^[A-Za-z_:][A-Za-z0-9_:.-]*$/.test(value) ? value : "";
}

function syncDOMAriaRelationAttributes(docNode, attrs) {
  if (!docNode)
    return;
  const read = (name) => Object.prototype.hasOwnProperty.call(attrs || {}, name)
    ? String(attrs[name] ?? "") : "";
  docNode.ariaLabel = read("aria-label");
  docNode.ariaDescription = read("aria-description");
  docNode.ariaDescribedBy = read("aria-describedby");
  docNode.ariaDetails = read("aria-details");
  docNode.ariaErrorMessage = read("aria-errormessage");
  docNode.ariaFlowTo = read("aria-flowto");
  docNode.ariaLabelledBy = read("aria-labelledby");
  docNode.ariaActiveDescendant = read("aria-activedescendant");
  docNode.ariaControls = read("aria-controls");
  docNode.ariaOwns = read("aria-owns");
}

function syncDOMAttributeMutation(el) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  docNode.extraAttrs = { ...(el.__kryExtraAttrs || {}) };
  syncDOMAriaRelationAttributes(docNode, plainElementMap(el.attributes));
  docNode.styleFacts = webNodeStyleFacts(docNode);
  applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
    ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
    : null);
  return docNode;
}

export function webDOMSetAttribute(target, query, name, value) {
  const attr = cleanDOMAttributeName(name);
  const el = attr ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const attrs = { ...(el.__kryExtraAttrs || {}) };
  attrs[attr] = value === undefined || value === null ? "" : String(value);
  el.__kryExtraAttrs = attrs;
  setAttr(el, attr, attrs[attr]);
  syncDOMAttributeMutation(el);
  return true;
}

export function webDOMRemoveAttribute(target, query, name) {
  const attr = cleanDOMAttributeName(name);
  const el = attr ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const attrs = { ...(el.__kryExtraAttrs || {}) };
  delete attrs[attr];
  el.__kryExtraAttrs = attrs;
  removeAttr(el, attr);
  syncDOMAttributeMutation(el);
  return true;
}

export function webDOMGetAttribute(target, query, name) {
  const attr = cleanDOMAttributeName(name);
  const el = attr ? findWebElement(target, query) : null;
  if (!el)
    return undefined;
  if (typeof el.getAttribute === "function") {
    const value = el.getAttribute(attr);
    return value === null ? undefined : value;
  }
  return el.attributes ? el.attributes[attr] : undefined;
}

export function webDOMHasAttribute(target, query, name) {
  return webDOMGetAttribute(target, query, name) !== undefined;
}

function cleanDOMPropertyName(name) {
  const value = String(name || "").trim();
  return value && /^[A-Za-z_$][A-Za-z0-9_$]*$/.test(value) ? value : "";
}

const domPropertyStateAliases = {
  disabled: "disabled",
  checked: "checked",
  selected: "selected",
  open: "open",
  invalid: "invalid"
};

function syncDOMPropertyMutation(el, prop) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  const stateName = domPropertyStateAliases[prop];
  if (stateName) {
    const extra = { ...(el.__kryExtraState || {}) };
    extra[stateName] = !!el[prop];
    el.__kryExtraState = extra;
    docNode.state[stateName] = !!el[prop];
    syncDOMStateMutation(el);
  } else if (prop === "value" || prop === "textContent") {
    updateElementFormValue(el, prop === "textContent" ? String(el.textContent || "") : webElementValue(el));
    if (prop === "textContent")
      docNode.text = String(el.textContent || "");
    docNode.styleFacts = webNodeStyleFacts(docNode);
  } else if (prop === "scrollLeft" || prop === "scrollTop") {
    syncDOMScrollMutation(el);
  } else {
    docNode.styleFacts = webNodeStyleFacts(docNode);
  }
  return docNode;
}

export function webDOMSetProperty(target, query, name, value) {
  const prop = cleanDOMPropertyName(name);
  const el = prop ? findWebElement(target, query) : null;
  if (!el)
    return false;
  try {
    el[prop] = value;
  } catch {
    return false;
  }
  syncDOMPropertyMutation(el, prop);
  return true;
}

export function webDOMGetProperty(target, query, name) {
  const prop = cleanDOMPropertyName(name);
  const el = prop ? findWebElement(target, query) : null;
  return el ? el[prop] : undefined;
}

export function webDOMSetStyle(target, query, name, value) {
  const prop = cleanDOMStyleName(name);
  const el = prop ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const styles = { ...(el.__kryInlineStyles || {}) };
  styles[prop] = value === undefined || value === null ? "" : String(value);
  el.__kryInlineStyles = styles;
  setStyleProperty(el.style, prop, styles[prop]);
  return true;
}

export function webDOMRemoveStyle(target, query, name) {
  const prop = cleanDOMStyleName(name);
  const el = prop ? findWebElement(target, query) : null;
  if (!el)
    return false;
  const styles = { ...(el.__kryInlineStyles || {}) };
  delete styles[prop];
  el.__kryInlineStyles = styles;
  removeStyleProperty(el.style, prop);
  if (el.__kryRuntime?.webStyleSheets && el.__kryDocNode)
    applyResolvedWebStyle(el, resolveWebStyle(el.__kryDocNode, el.__kryRuntime.webStyleSheets));
  return true;
}

export function webDOMGetStyle(target, query, name) {
  const prop = cleanDOMStyleName(name);
  const el = prop ? findWebElement(target, query) : null;
  return el ? getStyleProperty(el.style, prop) : undefined;
}

export function webDOMComputedStyle(target, query, name = "") {
  const el = findWebElement(target, query);
  if (!el)
    return undefined;
  const style = typeof globalThis.getComputedStyle === "function"
    ? globalThis.getComputedStyle(el)
    : (el.style || {});
  const prop = String(name || "").trim();
  if (!prop)
    return style;
  if (typeof style.getPropertyValue === "function") {
    const cssValue = style.getPropertyValue(prop);
    if (cssValue !== "")
      return cssValue;
  }
  return getStyleProperty(style, prop);
}

function webDOMRectFromElement(el) {
  if (!el)
    return null;
  let rect = null;
  if (typeof el.getBoundingClientRect === "function")
    rect = el.getBoundingClientRect();
  const docNode = el.__kryDocNode || {};
  const bounds = docNode.bounds || {};
  const x = Number.isFinite(Number(rect?.x)) ? Number(rect.x) :
    Number.isFinite(Number(rect?.left)) ? Number(rect.left) :
    Number.isFinite(Number(bounds.x)) ? Number(bounds.x) : 0;
  const y = Number.isFinite(Number(rect?.y)) ? Number(rect.y) :
    Number.isFinite(Number(rect?.top)) ? Number(rect.top) :
    Number.isFinite(Number(bounds.y)) ? Number(bounds.y) : 0;
  const width = Number.isFinite(Number(rect?.width)) ? Number(rect.width) :
    Number.isFinite(Number(bounds.width)) ? Number(bounds.width) :
    Number.isFinite(Number(el.clientWidth)) ? Number(el.clientWidth) : 0;
  const height = Number.isFinite(Number(rect?.height)) ? Number(rect.height) :
    Number.isFinite(Number(bounds.height)) ? Number(bounds.height) :
    Number.isFinite(Number(el.clientHeight)) ? Number(el.clientHeight) : 0;
  const left = Number.isFinite(Number(rect?.left)) ? Number(rect.left) : x;
  const top = Number.isFinite(Number(rect?.top)) ? Number(rect.top) : y;
  const right = Number.isFinite(Number(rect?.right)) ? Number(rect.right) : left + width;
  const bottom = Number.isFinite(Number(rect?.bottom)) ? Number(rect.bottom) : top + height;
  return { x, y, width, height, left, top, right, bottom };
}

function syncDOMScrollMutation(el) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  docNode.scrollLeft = Number(el.scrollLeft) || 0;
  docNode.scrollTop = Number(el.scrollTop) || 0;
  docNode.styleFacts = webNodeStyleFacts(docNode);
  applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
    ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
    : null);
  return docNode;
}

export function webDOMRect(target, query) {
  return webDOMRectFromElement(findWebElement(target, query));
}

export function webDOMGetScroll(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return null;
  return {
    left: Number(el.scrollLeft) || 0,
    top: Number(el.scrollTop) || 0,
    width: Number(el.scrollWidth) || Number(el.clientWidth) || 0,
    height: Number(el.scrollHeight) || Number(el.clientHeight) || 0
  };
}

export function webDOMSetScroll(target, query, left, top = null) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  const nextLeft = Number.isFinite(Number(left)) ? Number(left) : 0;
  const nextTop = top === null || top === undefined
    ? Number(el.scrollTop) || 0
    : Number.isFinite(Number(top)) ? Number(top) : 0;
  if (typeof el.scrollTo === "function")
    el.scrollTo(nextLeft, nextTop);
  else {
    el.scrollLeft = nextLeft;
    el.scrollTop = nextTop;
  }
  syncDOMScrollMutation(el);
  return true;
}

export function webDOMScrollIntoView(target, query, options = true) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.scrollIntoView === "function")
    el.scrollIntoView(options);
  return true;
}

export function webDOMGetText(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return undefined;
  return el.textContent === undefined || el.textContent === null
    ? ""
    : String(el.textContent);
}

export function webDOMSetText(target, query, text) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  const value = text === undefined || text === null ? "" : String(text);
  el.textContent = value;
  const docNode = el.__kryDocNode;
  if (docNode) {
    docNode.text = value;
    docNode.value = value;
    docNode.styleFacts = webNodeStyleFacts(docNode);
    applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
      ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
      : null);
  }
  return true;
}

export function webDOMGetValue(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return undefined;
  const docNode = el.__kryDocNode;
  return webElementValue(el, docNode) ?? docNode?.value;
}

export function webDOMSetValue(target, query, value) {
  const el = findWebElement(target, query);
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return false;
  let next = value;
  if (docNode.inputType === "checkbox" || docNode.inputType === "radio") {
    next = !!value;
    el.checked = next;
    setAttr(el, "checked", next);
  } else if (docNode.tag === "input" || docNode.tag === "textarea") {
    next = value === undefined || value === null ? "" : String(value);
    el.value = next;
    if (docNode.tag === "input")
      setAttr(el, "value", next);
  } else {
    next = value === undefined || value === null ? "" : String(value);
    el.textContent = next;
    docNode.text = next;
  }
  updateElementFormValue(el, next);
  docNode.styleFacts = webNodeStyleFacts(docNode);
  applyResolvedWebStyle(el, el.__kryRuntime?.webStyleSheets
    ? resolveWebStyle(docNode, el.__kryRuntime.webStyleSheets)
    : null);
  return true;
}

export function webDOMClick(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.click === "function")
    el.click();
  else if (typeof el.onclick === "function")
    el.onclick();
  return true;
}

export function webDOMFocus(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.focus === "function")
    el.focus();
  else if (typeof el.onfocus === "function")
    el.onfocus();
  return true;
}

export function webDOMBlur(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.blur === "function")
    el.blur();
  else if (typeof el.onblur === "function")
    el.onblur();
  return true;
}

export function webDOMSubmit(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.requestSubmit === "function")
    el.requestSubmit();
  else if (typeof el.submit === "function")
    el.submit();
  else if (typeof el.onsubmit === "function")
    el.onsubmit({ preventDefault() {} });
  return true;
}

export function webDOMReset(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.reset === "function")
    el.reset();
  else if (typeof el.onreset === "function")
    el.onreset({ preventDefault() {} });
  return true;
}

function setElementOpenState(el, open) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return false;
  const extra = { ...(el.__kryExtraState || {}) };
  extra.open = !!open;
  el.__kryExtraState = extra;
  docNode.state.open = !!open;
  syncDOMStateMutation(el);
  return true;
}

export function webDOMShowModal(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.showModal === "function")
    el.showModal();
  return setElementOpenState(el, true);
}

export function webDOMClose(target, query, returnValue = "") {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.close === "function")
    el.close(returnValue);
  return setElementOpenState(el, false);
}

export function webDOMShowPopover(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.showPopover === "function")
    el.showPopover();
  return setElementOpenState(el, true);
}

export function webDOMHidePopover(target, query) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  if (typeof el.hidePopover === "function")
    el.hidePopover();
  return setElementOpenState(el, false);
}

export function webDOMTogglePopover(target, query, force) {
  const el = findWebElement(target, query);
  if (!el)
    return false;
  const next = force === undefined ? !el.__kryDocNode?.state?.open : !!force;
  if (typeof el.togglePopover === "function")
    el.togglePopover(next);
  return setElementOpenState(el, next);
}

function defineEventInitValue(event, key, value) {
  if (!event || key === "type" || key === "target" || key === "currentTarget")
    return;
  try {
    event[key] = value;
  } catch {
    try {
      Object.defineProperty(event, key, { configurable: true, value });
    } catch {
      // Some browser event fields are intentionally read-only.
    }
  }
}

function createWebDOMEvent(type, init = {}) {
  const name = String(type || "").trim();
  if (!name)
    return null;
  const options = init && typeof init === "object" ? init : {};
  const eventInit = {
    bubbles: options.bubbles !== undefined ? !!options.bubbles : true,
    cancelable: options.cancelable !== undefined ? !!options.cancelable : true,
    composed: options.composed !== undefined ? !!options.composed : false
  };
  let event = null;
  try {
    if ((name.startsWith("key") || name === "beforeinput") &&
        typeof globalThis.KeyboardEvent === "function") {
      event = new globalThis.KeyboardEvent(name, { ...eventInit, ...options });
    } else if ((name === "input" || name === "beforeinput") &&
               typeof globalThis.InputEvent === "function") {
      event = new globalThis.InputEvent(name, { ...eventInit, ...options });
    } else if ((name.startsWith("mouse") || name === "click" ||
                name.startsWith("pointer")) &&
               typeof globalThis.MouseEvent === "function") {
      event = new globalThis.MouseEvent(name, { ...eventInit, ...options });
    } else if (Object.prototype.hasOwnProperty.call(options, "detail") &&
               typeof globalThis.CustomEvent === "function") {
      event = new globalThis.CustomEvent(name, { ...eventInit, detail: options.detail });
    } else if (typeof globalThis.Event === "function") {
      event = new globalThis.Event(name, eventInit);
    }
  } catch {
    event = null;
  }
  if (!event) {
    event = {
      type: name,
      ...eventInit,
      defaultPrevented: false,
      preventDefault() { this.defaultPrevented = true; }
    };
  }
  for (const [key, value] of Object.entries(options))
    defineEventInitValue(event, key, value);
  return event;
}

export function webDOMDispatchEvent(target, query, type, init = {}) {
  const el = findWebElement(target, query);
  const event = el ? createWebDOMEvent(type, init) : null;
  if (!el || !event)
    return false;
  bindWebDOMEventProperties(event);
  if (typeof el.dispatchEvent === "function")
    return el.dispatchEvent(event) !== false;
  const handler = el["on" + event.type];
  if (typeof handler === "function")
    handler(event);
  return !event.defaultPrevented;
}

export function webFormValue(target, query) {
  const root = mountedRoot(target);
  if (!root)
    return undefined;
  return root.__kryFormValues?.get(String(query || ""));
}

export function webFormValues(target, query = "") {
  const root = mountedRoot(target);
  if (root && query) {
    const form = findWebElement(root, query);
    return webFormValuesForNode(root, form?.__kryDocNode || null);
  }
  return webFormValuesFromRoot(root);
}

function webFormValuesFromRoot(root) {
  return root?.__kryFormValues
    ? Object.fromEntries(root.__kryFormValues.entries())
    : {};
}

export function mount(rt, target) {
  return renderWebDocument(rt, target);
}

export function GetRoutePath() {
  ensureRouteListeners();
  syncRouteFromBrowser();
  return routeState.path;
}

export function GetRouteHash() {
  ensureRouteListeners();
  syncRouteFromBrowser();
  return routeState.hash;
}

export function GetRouteVersion() {
  ensureRouteListeners();
  syncRouteFromBrowser();
  return routeState.version;
}

export function PushRoute(path) {
  ensureRouteListeners();
  return updateBrowserRoute(path, false);
}

export function ReplaceRoute(path) {
  ensureRouteListeners();
  return updateBrowserRoute(path, true);
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

export function Scale(value) {
  return value | 0;
}

export function GetScreenWidth() {
  return 800;
}

export function GetScreenHeight() {
  return 600;
}

export function GetViewWidth() {
  return GetScreenWidth();
}

export function GetViewHeight() {
  return GetScreenHeight();
}

export function GetPageSidePadding() {
  return Scale(24);
}

export function GetThemeBackground() { return GetTheme().colors.background; }
export function GetThemeSurface() { return GetTheme().colors.surface; }
export function GetThemeText() { return GetTheme().colors.text; }
export function GetThemeButton() { return GetTheme().colors.accent; }
export function GetThemeButtonHover() { return GetTheme().colors.accentHover || GetThemeButton(); }
export function GetThemeCircle() { return GetTheme().colors.focus || GetThemeButton(); }
export function GetThemeIcon() { return GetTheme().colors.icon; }
export function GetThemeLink() { return GetTheme().colors.link; }

export function SystemThemePrefersDark() { return false; }

function colorToCss(value) {
  if (typeof value === "string")
    return value;
  if (!value || typeof value !== "object")
    return "";
  const a = value.a === undefined ? 255 : value.a;
  if (a >= 255)
    return `rgb(${value.r || 0}, ${value.g || 0}, ${value.b || 0})`;
  return `rgba(${value.r || 0}, ${value.g || 0}, ${value.b || 0}, ${a / 255})`;
}

export function SetPageTitle(title) {
  pageMeta.title = String(title || "");
}

export function SetPageDescription(description) {
  pageMeta.description = String(description || "");
}

export function SetPageCanonicalURL(url) {
  pageMeta.canonicalURL = String(url || "");
}

export function SetPageThemeColor(color) {
  pageMeta.themeColor = colorToCss(color);
}

export function Fade(color, alpha) {
  return Object.assign({}, color, { a: Math.round((alpha || 0) * 255) });
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

export function DarkenColor(color, amount) {
  return Color(
    clampByte((color?.r || 0) - amount),
    clampByte((color?.g || 0) - amount),
    clampByte((color?.b || 0) - amount),
    color?.a ?? 255
  );
}

export function LightenColor(color, amount) {
  return Color(
    clampByte((color?.r || 0) + amount),
    clampByte((color?.g || 0) + amount),
    clampByte((color?.b || 0) + amount),
    color?.a ?? 255
  );
}

export function TextFormat(format, ...values) {
  let index = 0;
  return String(format).replace(/%[sdif]/g, () => String(values[index++] ?? ""));
}

export function GetUIClipboardTextValue() { return ""; }

export function UpdateFileDialog() { return 0; }

export function IsKeyPressed(_key) { return false; }

export function IsKeyDown(_key) { return false; }

export function IsMouseButtonReleased(_button) { return false; }

export function GetThemeMetrics() {
  return {
    radiusSmall: 4, radiusMedium: 8, radiusLarge: 12, radiusPill: 999,
    borderWidth: 1, focusWidth: 2, focusGap: 2,
    space1: 4, space2: 8, space3: 12, space4: 16, space5: 24, space6: 32,
    controlHeightSmall: 32, controlHeightMedium: 40, controlHeightLarge: 48,
    controlPaddingSmall: 12, controlPaddingMedium: 16, controlPaddingLarge: 20,
    controlGap: 8, fontSizeSmall: 13, fontSizeMedium: 14, fontSizeLarge: 16,
    iconSizeSmall: 14, iconSizeMedium: 16, iconSizeLarge: 20,
    shadowOffsetY: 2, shadowBlur: 8, disabledOpacity: 0.58,
    transitionFastMS: 80, transitionNormalMS: 140,
  };
}

let fancyEffectsEnabled = true;

export function SetFancyEffectsEnabled(enabled) {
  fancyEffectsEnabled = !!enabled;
}

export function FancyEffectsEnabled() {
  return fancyEffectsEnabled ? 1 : 0;
}

export function Canvas(canvas) {
  return {
    active: false,
    dragging: false,
    selected_index: -1,
    selectedIndex: -1,
    world: NewVector2(),
    canvas
  };
}

export function CanvasHitTest(canvas, screen) {
  return { canvas, screen, hit: false, active: false, world: screen || NewVector2() };
}

const runtimeCallNames = [
  "AppBackground", "Background", "Text", "Paragraph",
  "Abbreviation", "Address", "Area", "Article", "Aside", "Base",
  "BidirectionalIsolate", "BidirectionalOverride",
  "Box", "Line", "Bevel", "Icon", "Image", "Button", "Card", "Selectable",
  "Audio", "BlockQuote", "Bold", "Cite", "Code", "CodeBlock",
  "Data", "DataList", "Deleted",
  "DescriptionDetails", "DescriptionList", "DescriptionTerm",
  "Details", "Dialog", "Embed", "Emphasis",
  "Figcaption", "Figure", "Footer", "Form", "Header", "HGroup",
  "IFrame", "ImageMap", "Inserted", "Italic",
  "Keyboard", "Label", "LineBreak", "ListItem", "Main",
  "Legend", "Mark", "Meta", "Meter", "Navigation", "NoScript", "EmbeddedObject", "OrderedList",
  "OptionGroup", "Option", "Output", "Param", "Pre", "Quote",
  "Ruby", "RubyParenthesis", "RubyText", "Sample", "Script", "Search", "Select",
  "Slot", "Small", "Source", "Strong", "StyleElement", "Subscript", "Summary",
  "Superscript", "Table", "TableBody", "TableCaption",
  "TableColumn", "TableColumnGroup", "TableFoot",
  "TableHead", "TableRow", "Template", "Time", "Title",
  "Track", "UnorderedList", "Variable", "Video",
  "WordBreakOpportunity",
  "Bullet", "Separator",
  "Link", "TextField", "TextArea", "Dropdown", "SegmentedControl",
  "Slider", "Menu",
  "Toggle", "Checkbox", "Radio", "Progress", "Plot",
  "Drag", "Input", "Spinbox",
  "DragDrop",
  "Screen", "Page", "Section", "Heading", "ParagraphText",
  "Column", "Row", "Stack", "Flow", "Grid", "Scroll",
  "Modal", "TitleBar", "TabBar",
  "NavigationBar", "Toolbar", "Toast", "Fieldset", "PanedView",
  "Collapsible", "ListBox", "TreeView", "TableView", "ColorPicker",
  "CanvasGrid", "SetCurrentTheme"
];

for (const name of runtimeCallNames) {
  if (!Object.prototype.hasOwnProperty.call(globalThis, "__kryonRuntimeInit")) {
    // Marker only; named exports are declared below for ESM static analysis.
  }
}
globalThis.__kryonRuntimeInit = true;

export function AppBackground(...args) { return struct("AppBackground", args); }
export function Abbreviation(...args) { return struct("Abbreviation", args); }
export function Address(...args) { return struct("Address", args); }
export function Area(...args) { return struct("Area", args); }
export function Background(...args) { return struct("Background", args); }
export function Base(...args) { return struct("Base", args); }
export function Bevel(...args) { return struct("Bevel", args); }
export function Article(...args) { return struct("Article", args); }
export function Aside(...args) { return struct("Aside", args); }
export function BidirectionalIsolate(...args) { return struct("BidirectionalIsolate", args); }
export function BidirectionalOverride(...args) { return struct("BidirectionalOverride", args); }
export function Audio(...args) { return struct("Audio", args); }
export function Bullet(...args) { return struct("Bullet", args); }
export function Button(...args) { return struct("Button", args); }
export function BlockQuote(...args) { return struct("BlockQuote", args); }
export function Bold(...args) { return struct("Bold", args); }
export function Card(...args) { return struct("Card", args); }
export function CanvasGrid(...args) { return struct("CanvasGrid", args); }
export function Checkbox(...args) { return struct("Checkbox", args); }
export function Cite(...args) { return struct("Cite", args); }
export function Collapsible(...args) { return struct("Collapsible", args); }
export function ColorPicker(...args) { return struct("ColorPicker", args); }
export function Column(...args) { return struct("Column", args); }
export function Code(...args) { return struct("Code", args); }
export function CodeBlock(...args) { return struct("CodeBlock", args); }
export function Data(...args) { return struct("Data", args); }
export function DataList(...args) { return struct("DataList", args); }
export function Deleted(...args) { return struct("Deleted", args); }
export function DescriptionDetails(...args) { return struct("DescriptionDetails", args); }
export function DescriptionList(...args) { return struct("DescriptionList", args); }
export function DescriptionTerm(...args) { return struct("DescriptionTerm", args); }
export function Details(...args) { return struct("Details", args); }
export function Dialog(...args) { return struct("Dialog", args); }
export function Drag(...args) { return struct("Drag", args); }
export function DragDrop(...args) { return struct("DragDrop", args); }
export function Dropdown(...args) { return struct("Dropdown", args); }
export function Embed(...args) { return struct("Embed", args); }
export function Emphasis(...args) { return struct("Emphasis", args); }
export function Figcaption(...args) { return struct("Figcaption", args); }
export function Figure(...args) { return struct("Figure", args); }
export function Footer(...args) { return struct("Footer", args); }
export function Form(...args) { return struct("Form", args); }
export function Header(...args) { return struct("Header", args); }
export function HGroup(...args) { return struct("HGroup", args); }
export function IFrame(...args) { return struct("IFrame", args); }
export function ImageMap(...args) { return struct("ImageMap", args); }
export function Input(...args) { return struct("Input", args); }
export function Inserted(...args) { return struct("Inserted", args); }
export function Italic(...args) { return struct("Italic", args); }
export function Keyboard(...args) { return struct("Keyboard", args); }
export function Label(...args) { return struct("Label", args); }
export function Legend(...args) { return struct("Legend", args); }
export function SegmentedControl(...args) { return struct("SegmentedControl", args); }
export function Icon(...args) { return struct("Icon", args); }
export function Fieldset(...args) { return struct("Fieldset", args); }
export function Line(...args) { return struct("Line", args); }
export function LineBreak(...args) { return struct("LineBreak", args); }
export function Link(...args) { return struct("Link", args); }
export function ListBox(...args) { return struct("ListBox", args); }
export function ListItem(...args) { return struct("ListItem", args); }
export function Main(...args) { return struct("Main", args); }
export function Mark(...args) { return struct("Mark", args); }
export function Meta(...args) { return struct("Meta", args); }
export function Menu(...args) { return struct("Menu", args); }
export function Flow(...args) { return struct("Flow", args); }
export function Grid(...args) { return struct("Grid", args); }
export function Heading(...args) { return struct("Heading", args); }
export function Modal(...args) { return struct("Modal", args); }
export function Meter(...args) { return struct("Meter", args); }
export function NavigationBar(...args) { return struct("NavigationBar", args); }
export function Navigation(...args) { return struct("Navigation", args); }
export function NoScript(...args) { return struct("NoScript", args); }
export function EmbeddedObject(...args) { return struct("EmbeddedObject", args); }
export function OrderedList(...args) { return struct("OrderedList", args); }
export function OptionGroup(...args) { return struct("OptionGroup", args); }
export function Option(...args) { return struct("Option", args); }
export function Output(...args) { return struct("Output", args); }
export function Param(...args) { return struct("Param", args); }
export function Page(...args) { return struct("Page", args); }
export function PanedView(...args) { return struct("PanedView", args); }
export function Paragraph(...args) { return struct("Paragraph", args); }
export function ParagraphText(...args) { return struct("ParagraphText", args); }
export function Image(...args) { return struct("Image", args); }
export function Plot(...args) { return struct("Plot", args); }
export function Pre(...args) { return struct("Pre", args); }
export function Progress(...args) { return struct("Progress", args); }
export function Quote(...args) { return struct("Quote", args); }
export function Ruby(...args) { return struct("Ruby", args); }
export function RubyParenthesis(...args) { return struct("RubyParenthesis", args); }
export function RubyText(...args) { return struct("RubyText", args); }
export function Radio(...args) { return struct("Radio", args); }
export function Box(...args) { return struct("Box", args); }
export function Row(...args) { return struct("Row", args); }
export function Sample(...args) { return struct("Sample", args); }
export function Screen(...args) { return struct("Screen", args); }
export function Scroll(...args) { return struct("Scroll", args); }
export function Script(...args) { return struct("Script", args); }
export function Search(...args) { return struct("Search", args); }
export function Select(...args) { return struct("Select", args); }
export function Selectable(...args) { return struct("Selectable", args); }
export function Separator(...args) { return struct("Separator", args); }
export function SetCurrentTheme(...args) { return struct("SetCurrentTheme", args); }
export function Section(...args) { return struct("Section", args); }
export function Slot(...args) { return struct("Slot", args); }
export function Small(...args) { return struct("Small", args); }
export function Source(...args) { return struct("Source", args); }
export function Toast(...args) { return struct("Toast", args); }
export function Slider(...args) { return struct("Slider", args); }
export function Spinbox(...args) { return struct("Spinbox", args); }
export function Stack(...args) { return struct("Stack", args); }
export function Strong(...args) { return struct("Strong", args); }
export function StyleElement(...args) { return struct("StyleElement", args); }
export function Subscript(...args) { return struct("Subscript", args); }
export function Summary(...args) { return struct("Summary", args); }
export function Superscript(...args) { return struct("Superscript", args); }
export function TabBar(...args) { return struct("TabBar", args); }
export function Table(...args) { return struct("Table", args); }
export function TableBody(...args) { return struct("TableBody", args); }
export function TableCaption(...args) { return struct("TableCaption", args); }
export function TableCell(...args) { return struct("TableCell", args); }
export function TableColumn(...args) { return struct("TableColumn", args); }
export function TableColumnGroup(...args) { return struct("TableColumnGroup", args); }
export function TableFoot(...args) { return struct("TableFoot", args); }
export function TableHead(...args) { return struct("TableHead", args); }
export function TableRow(...args) { return struct("TableRow", args); }
export function TableView(...args) { return struct("TableView", args); }
export function Template(...args) { return struct("Template", args); }
export function Text(...args) { return struct("Text", args); }
export function TextArea(...args) { return struct("TextArea", args); }
export function TextField(...args) { return struct("TextField", args); }
export function TitleBar(...args) { return struct("TitleBar", args); }
export function Title(...args) { return struct("Title", args); }
export function Time(...args) { return struct("Time", args); }
export function Track(...args) { return struct("Track", args); }
export function TreeView(...args) { return struct("TreeView", args); }
export function Toggle(...args) { return struct("Toggle", args); }
export function Toolbar(...args) { return struct("Toolbar", args); }
export function UnorderedList(...args) { return struct("UnorderedList", args); }
export function Variable(...args) { return struct("Variable", args); }
export function Video(...args) { return struct("Video", args); }
export function WordBreakOpportunity(...args) { return struct("WordBreakOpportunity", args); }
