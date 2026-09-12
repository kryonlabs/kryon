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
  version: 0
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
  routeState.version++;
  return true;
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
  if (history && typeof history[replace ? "replaceState" : "pushState"] === "function")
    history[replace ? "replaceState" : "pushState"](null, "", url);
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
export const THEME_STYLE_SYSTEM = 0;
export const THEME_STYLE_DEFAULT = 1;
export const SyntaxNone = 0;
export const SyntaxKry = 1;
export const SyntaxC = 2;
export const SyntaxMake = 3;
export const IMAGE_FIT_STRETCH = 0;
export const IMAGE_FIT_CONTAIN = 1;
export const IMAGE_FIT_COVER = 2;

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
    input: {
      events: [],
      focus: 0,
      clipboard: "",
      selections: new Map(),
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
  rt.SetFocus = (id) => { rt.input.focus = Number(id); };
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
  for (const [type, instances] of rt.instances) {
    for (const [key, entry] of instances) {
      if (Instance_InstanceExpired(null, null, null, BigInt(rt.instanceFrame - entry.frameSeen)))
        instances.delete(key);
    }
    if (instances.size === 0)
      rt.instances.delete(type);
  }
  if (rt.input)
    rt.input.focusOrder = [];
  return rt;
}

export function endFrame(rt) {
  if (rt.input) {
    rt.input.lastFocusOrder = rt.input.focusOrder.slice();
    rt.input.events = [];
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
  const item = { kind: "widget", name, args, meta };
  rt.frame.push(item);
  return handleWidget(rt, name, args, state);
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

function propClassList(args) {
  const classes = [];
  const add = (value) => {
    if (value === undefined || value === null)
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
  const rect = String(args || "").match(/\((?:Rectangle|Rect)\)\s*\{([^{}]+)\}/);
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

function textLength(text) {
  return Array.from(String(text || "")).length;
}

function replaceRange(text, start, end, insert) {
  const chars = Array.from(String(text || ""));
  const a = Math.max(0, Math.min(chars.length, start));
  const b = Math.max(a, Math.min(chars.length, end));
  chars.splice(a, b - a, ...Array.from(String(insert)));
  return chars.join("");
}

function selectionFor(rt, id, cursor) {
  const selected = rt.input.selections.get(id);
  if (!selected)
    return { start: cursor, end: cursor };
  return {
    start: Math.min(selected.anchor, selected.cursor),
    end: Math.max(selected.anchor, selected.cursor)
  };
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

function applyTextInput(rt, state, id, textKey, cursorKey, maxCodepoints, secure = false) {
  if (!state || !textKey || !cursorKey || rt.input.focus !== id)
    return;
  for (;;) {
    const event = consumeFirstEvent(rt, (ev) =>
      ev.type === "text" || ev.type === "key" || ev.type === "shortcut");
    if (!event)
      break;
    let cursor = Number(state[cursorKey] || 0);
    if (event.type === "text") {
      const sel = selectionFor(rt, id, cursor);
      const next = replaceRange(state[textKey], sel.start, sel.end, event.text);
      state[textKey] = Array.from(next).slice(0, maxCodepoints).join("");
      state[cursorKey] = Math.min(sel.start + textLength(event.text), textLength(state[textKey]));
      rt.input.selections.delete(id);
    } else if (event.type === "shortcut") {
      if (Number(event.key) === KeyC) {
        const sel = selectionFor(rt, id, cursor);
        if (!secure)
          rt.input.clipboard = Array.from(String(state[textKey] || "")).slice(sel.start, sel.end).join("");
      }
    } else if (Number(event.key) === KeyLeft) {
      state[cursorKey] = Math.max(0, cursor - 1);
      rt.input.selections.delete(id);
    } else if (Number(event.key) === KeyRight) {
      state[cursorKey] = Math.min(textLength(state[textKey]), cursor + 1);
      rt.input.selections.delete(id);
    } else if (Number(event.key) === KeyBackspace) {
      const sel = selectionFor(rt, id, cursor);
      if (sel.start !== sel.end) {
        state[textKey] = replaceRange(state[textKey], sel.start, sel.end, "");
        state[cursorKey] = sel.start;
      } else if (cursor > 0) {
        state[textKey] = replaceRange(state[textKey], cursor - 1, cursor, "");
        state[cursorKey] = cursor - 1;
      }
      rt.input.selections.delete(id);
    } else if (Number(event.key) === KeyTab) {
      moveFocus(rt, id, !!event.shift);
      rt.input.selections.delete(id);
    }
  }
}

function parseTextInputProps(args) {
  const bounds = parseBounds(args);
  const textKey = propIdent(args, "text");
  return {
    bounds,
    textKey,
    cursorKey: propRef(args, "cursor_position"),
    focusID: propNumber(args, "focus_id", 0),
    maxCodepoints: propNumber(args, "max_codepoints", 4095),
    secure: /\.secure\s*=\s*true\b/.test(String(args || ""))
  };
}

function handleTextInput(rt, state, args) {
  const props = parseTextInputProps(args);
  if (props.focusID) {
    if (!rt.input.focusOrder.includes(props.focusID))
      rt.input.focusOrder.push(props.focusID);
    const tap = consumeFirstEvent(rt, (ev) => ev.type === "tap" && hit(props.bounds, ev.x, ev.y));
    if (tap) {
      rt.input.focus = props.focusID;
      if (state && props.cursorKey)
        state[props.cursorKey] = 0;
    }
    applyTextInput(rt, state, props.focusID, props.textKey, props.cursorKey,
                   props.maxCodepoints, props.secure);
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

function handleCard(rt, args) {
  if (!isTruthyProp(args, "clickable") && !isTruthyProp(args, "Clickable"))
    return false;
  return handleButton(rt, args);
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
  if (name === "BeginDisabled") {
    rt.disabledStack.push(numberValue(args) !== 0);
    return false;
  }
  if (name === "EndDisabled") {
    if (rt.disabledStack.length > 0)
      rt.disabledStack.pop();
    return false;
  }
  if (rt.disabledStack.some(Boolean))
    return false;
  switch (name) {
  case "Button":
    return handleButton(rt, args);
  case "Card":
    return handleCard(rt, args);
  case "BeginCard":
    return false;
  case "TextField":
  case "TextArea":
    return handleTextInput(rt, state, args);
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
  switch (item.name) {
  case "Screen":
  case "Page":
    return "main";
  case "Section":
    return "section";
  case "Heading":
    return "h" + Math.max(1, Math.min(6, propNumber(args, "level", 2)));
  case "Paragraph":
  case "ParagraphText":
    return "p";
  case "Link":
    return "a";
  case "Button":
  case "InvisibleButton":
    return "button";
  case "TextField":
    return "input";
  case "TextArea":
    return "textarea";
  case "Image":
    return "img";
  case "Checkbox":
  case "Toggle":
  case "Radio":
    return "input";
  default:
    return "div";
  }
}

function widgetText(item) {
  const args = item.args || {};
  switch (item.name) {
  case "Text":
  case "Heading":
  case "Paragraph":
  case "ParagraphText":
  case "Link":
    return propString(args, "text", "");
  case "Button":
  case "InvisibleButton":
    return propString(args, "label", "");
  case "TextField":
  case "TextArea":
    return propString(args, "text", propString(args, "value", ""));
  default:
    return "";
  }
}

function widgetHref(item) {
  if (item.name !== "Link")
    return "";
  return propString(item.args, "href", propString(item.args, "url", ""));
}

function widgetInputType(item) {
  switch (item.name) {
  case "Checkbox":
  case "Toggle":
    return "checkbox";
  case "Radio":
    return "radio";
  case "TextField":
    return "text";
  default:
    return "";
  }
}

function widgetLevel(item) {
  if (item.name !== "Heading")
    return 0;
  return Math.max(1, Math.min(6, propNumber(item.args || {}, "level", 2)));
}

function propDataAttrs(meta) {
  const out = {};
  const data = meta && typeof meta.data === "object" && !Array.isArray(meta.data) ? meta.data : null;
  if (!data)
    return out;
  for (const [name, value] of Object.entries(data)) {
    const attr = String(name).trim().replace(/_/g, "-").toLowerCase();
    if (!attr || !/^[a-z0-9][a-z0-9.-]*$/.test(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
  }
  return out;
}

function propAriaAttrs(meta) {
  const out = {};
  const aria = meta && typeof meta.aria === "object" && !Array.isArray(meta.aria) ? meta.aria : null;
  if (!aria)
    return out;
  for (const [name, value] of Object.entries(aria)) {
    const attr = String(name).trim().replace(/_/g, "-").toLowerCase();
    if (!attr || !/^[a-z0-9][a-z0-9.-]*$/.test(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
  }
  return out;
}

function propExtraAttrs(meta) {
  const out = {};
  const attrs = meta && typeof meta.extraAttrs === "object" && !Array.isArray(meta.extraAttrs)
    ? meta.extraAttrs : null;
  if (!attrs)
    return out;
  for (const [name, value] of Object.entries(attrs)) {
    const attr = String(name).trim().toLowerCase();
    if (!attr || !/^[a-z][a-z0-9._:-]*$/.test(attr))
      continue;
    out[attr] = value === undefined || value === null ? "" : String(value);
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

function webNodeFromWidget(item, index) {
  const args = item.args || {};
  const meta = item.meta || {};
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
    expanded: isTruthyProp(args, "expanded"),
    open: isTruthyProp(args, "open"),
    hover: false,
    pressed: false,
    focus: false
  };
  const node = {
    index,
    kind: item.name,
    tag: widgetTag(item),
    key: meta.nodeName || propString(args, "key", propString(args, "id", String(index))),
    name: meta.nodeName || propString(args, "name", ""),
    path: meta.path === undefined || meta.path === null ? "" : String(meta.path),
    parentPath: meta.parentPath === undefined || meta.parentPath === null ? "" : String(meta.parentPath),
    sourcePath: meta.sourcePath === undefined || meta.sourcePath === null ? "" : String(meta.sourcePath),
    sourceLine: Number.isFinite(Number(meta.sourceLine)) ? Math.trunc(Number(meta.sourceLine)) : 0,
    sourceColumn: Number.isFinite(Number(meta.sourceColumn)) ? Math.trunc(Number(meta.sourceColumn)) : 0,
    domId: meta.id === undefined || meta.id === null ? "" : String(meta.id),
    domName: meta.domName === undefined || meta.domName === null ? "" : String(meta.domName),
    classes: [...new Set(classes)],
    title: meta.title === undefined || meta.title === null ? "" : String(meta.title),
    placeholder: meta.placeholder === undefined || meta.placeholder === null ? "" : String(meta.placeholder),
    tabIndex: Number.isFinite(Number(meta.tabIndex)) ? Math.trunc(Number(meta.tabIndex)) : null,
    text: widgetText(item),
    value: widgetText(item),
    domValue: metaString(meta, "domValue"),
    level: widgetLevel(item),
    href: meta.href === undefined || meta.href === null ? widgetHref(item) : String(meta.href),
    target: meta.target === undefined || meta.target === null ? "" : String(meta.target),
    rel: meta.rel === undefined || meta.rel === null ? "" : String(meta.rel),
    htmlFor: metaString(meta, "htmlFor"),
    dataAttrs: propDataAttrs(meta),
    extraAttrs: propExtraAttrs(meta),
    inputType: meta.inputType === undefined || meta.inputType === null ? widgetInputType(item) : String(meta.inputType),
    formAction: meta.formAction === undefined || meta.formAction === null ? "" : String(meta.formAction),
    formMethod: meta.formMethod === undefined || meta.formMethod === null ? "" : String(meta.formMethod),
    formEncType: meta.formEncType === undefined || meta.formEncType === null ? "" : String(meta.formEncType),
    autoComplete: meta.autoComplete === undefined || meta.autoComplete === null ? "" : String(meta.autoComplete),
    hidden: metaBool(meta, "hidden"),
    draggable: metaString(meta, "draggable"),
    spellCheck: metaString(meta, "spellCheck"),
    contentEditable: metaString(meta, "contentEditable"),
    autoFocus: metaBool(meta, "autoFocus"),
    download: metaString(meta, "download"),
    formNoValidate: metaBool(meta, "formNoValidate"),
    noValidate: metaBool(meta, "noValidate"),
    popover: metaString(meta, "popover"),
    popoverTarget: metaString(meta, "popoverTarget"),
    popoverTargetAction: metaString(meta, "popoverTargetAction"),
    readOnly: metaBool(meta, "readOnly"),
    required: metaBool(meta, "required"),
    min: metaString(meta, "min"),
    max: metaString(meta, "max"),
    step: metaString(meta, "step"),
    minLength: metaString(meta, "minLength"),
    maxLength: metaString(meta, "maxLength"),
    pattern: metaString(meta, "pattern"),
    accept: metaString(meta, "accept"),
    multiple: metaBool(meta, "multiple"),
    inputMode: metaString(meta, "inputMode"),
    alt: propString(args, "alt", propString(args, "alt_text", "")),
    asset: propString(args, "asset_path", propString(args, "src", "")),
    role: meta.role === undefined || meta.role === null ? "" : String(meta.role),
    ariaLabel: meta.ariaLabel === undefined || meta.ariaLabel === null ? "" : String(meta.ariaLabel),
    ariaDescription: meta.ariaDescription === undefined || meta.ariaDescription === null ? "" : String(meta.ariaDescription),
    ariaDescribedBy: meta.ariaDescribedBy === undefined || meta.ariaDescribedBy === null ? "" : String(meta.ariaDescribedBy),
    ariaControls: meta.ariaControls === undefined || meta.ariaControls === null ? "" : String(meta.ariaControls),
    ariaLive: meta.ariaLive === undefined || meta.ariaLive === null ? "" : String(meta.ariaLive),
    ariaAttrs: propAriaAttrs(meta),
    onClick: meta.onClick === undefined || meta.onClick === null ? "" : String(meta.onClick),
    onInput: meta.onInput === undefined || meta.onInput === null ? "" : String(meta.onInput),
    onBeforeInput: meta.onBeforeInput === undefined || meta.onBeforeInput === null ? "" : String(meta.onBeforeInput),
    onChange: meta.onChange === undefined || meta.onChange === null ? "" : String(meta.onChange),
    onSelect: meta.onSelect === undefined || meta.onSelect === null ? "" : String(meta.onSelect),
    onKey: meta.onKey === undefined || meta.onKey === null ? "" : String(meta.onKey),
    onInvalid: meta.onInvalid === undefined || meta.onInvalid === null ? "" : String(meta.onInvalid),
    onSubmit: meta.onSubmit === undefined || meta.onSubmit === null ? "" : String(meta.onSubmit),
    onReset: meta.onReset === undefined || meta.onReset === null ? "" : String(meta.onReset),
    onToggle: meta.onToggle === undefined || meta.onToggle === null ? "" : String(meta.onToggle),
    onClose: meta.onClose === undefined || meta.onClose === null ? "" : String(meta.onClose),
    onCancel: meta.onCancel === undefined || meta.onCancel === null ? "" : String(meta.onCancel),
    onFocus: meta.onFocus === undefined || meta.onFocus === null ? "" : String(meta.onFocus),
    onBlur: meta.onBlur === undefined || meta.onBlur === null ? "" : String(meta.onBlur),
    onScroll: meta.onScroll === undefined || meta.onScroll === null ? "" : String(meta.onScroll),
    onMouseEnter: meta.onMouseEnter === undefined || meta.onMouseEnter === null ? "" : String(meta.onMouseEnter),
    onMouseLeave: meta.onMouseLeave === undefined || meta.onMouseLeave === null ? "" : String(meta.onMouseLeave),
    onMouseMove: meta.onMouseMove === undefined || meta.onMouseMove === null ? "" : String(meta.onMouseMove),
    onMouseDown: meta.onMouseDown === undefined || meta.onMouseDown === null ? "" : String(meta.onMouseDown),
    onMouseUp: meta.onMouseUp === undefined || meta.onMouseUp === null ? "" : String(meta.onMouseUp),
    onWheel: meta.onWheel === undefined || meta.onWheel === null ? "" : String(meta.onWheel),
    onDragStart: meta.onDragStart === undefined || meta.onDragStart === null ? "" : String(meta.onDragStart),
    onDragEnd: meta.onDragEnd === undefined || meta.onDragEnd === null ? "" : String(meta.onDragEnd),
    onDragOver: meta.onDragOver === undefined || meta.onDragOver === null ? "" : String(meta.onDragOver),
    onDrop: meta.onDrop === undefined || meta.onDrop === null ? "" : String(meta.onDrop),
    onCopy: meta.onCopy === undefined || meta.onCopy === null ? "" : String(meta.onCopy),
    onCut: meta.onCut === undefined || meta.onCut === null ? "" : String(meta.onCut),
    onPaste: meta.onPaste === undefined || meta.onPaste === null ? "" : String(meta.onPaste),
    action: typeof meta.action === "function" ? meta.action : null,
    inputAction: typeof meta.inputAction === "function" ? meta.inputAction : null,
    beforeInputAction: typeof meta.beforeInputAction === "function" ? meta.beforeInputAction : null,
    changeAction: typeof meta.changeAction === "function" ? meta.changeAction : null,
    selectAction: typeof meta.selectAction === "function" ? meta.selectAction : null,
    keyAction: typeof meta.keyAction === "function" ? meta.keyAction : null,
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
    wheelAction: typeof meta.wheelAction === "function" ? meta.wheelAction : null,
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
    pageThemeColor: colorToCss(args?.theme_color || args?.themeColor || ""),
    bounds,
    hasBounds: bounds.width > 0 || bounds.height > 0,
    scrollLeft: 0,
    scrollTop: 0,
    state
  };
  node.styleFacts = webNodeStyleFacts(node);
  return node;
}

export function webNodeStyleFacts(node) {
  return {
    kind: node?.kind || "",
    tag: node?.tag || "",
    key: node?.key || "",
    name: node?.name || "",
    path: node?.path || "",
    parentPath: node?.parentPath || "",
    sourcePath: node?.sourcePath || "",
    sourceLine: node?.sourceLine || 0,
    sourceColumn: node?.sourceColumn || 0,
    id: node?.domId || "",
    domName: node?.domName || "",
    domValue: node?.domValue || "",
    href: node?.href || "",
    target: node?.target || "",
    rel: node?.rel || "",
    htmlFor: node?.htmlFor || "",
    inputType: node?.inputType || "",
    formAction: node?.formAction || "",
    formMethod: node?.formMethod || "",
    formEncType: node?.formEncType || "",
    autoComplete: node?.autoComplete || "",
    hidden: !!node?.hidden,
    draggable: node?.draggable || "",
    spellCheck: node?.spellCheck || "",
    contentEditable: node?.contentEditable || "",
    autoFocus: !!node?.autoFocus,
    download: node?.download || "",
    formNoValidate: !!node?.formNoValidate,
    noValidate: !!node?.noValidate,
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
    classes: [...(node?.classes || [])],
    dataAttrs: { ...(node?.dataAttrs || {}) },
    ariaAttrs: { ...(node?.ariaAttrs || {}) },
    extraAttrs: { ...(node?.extraAttrs || {}) },
    role: node?.role || "",
    state: { ...(node?.state || {}) }
  };
}

export function webAccessibilitySnapshot(source) {
  const frame = source?.nodes ? source : webDocumentFrame(source);
  return {
    title: frame.metadata?.title || "",
    description: frame.metadata?.description || "",
    nodes: (frame.nodes || []).map((node) => ({
      path: node.path,
      sourcePath: node.sourcePath,
      sourceLine: node.sourceLine,
      sourceColumn: node.sourceColumn,
      name: node.name,
      kind: node.kind,
      tag: node.tag,
      id: node.domId,
      classes: [...node.classes],
      role: node.role || implicitRole(node),
      label: node.ariaLabel || node.text || node.name,
      description: node.ariaDescription,
      text: node.text,
      value: node.tag === "input" || node.tag === "textarea" ? node.value : "",
      href: node.href,
      inputType: node.inputType,
      level: node.level || 0,
      state: { ...node.state }
    }))
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
    if (node.inputType === "checkbox")
      return "checkbox";
    if (node.inputType === "radio")
      return "radio";
    return "textbox";
  }
  if (node.tag === "textarea")
    return "textbox";
  if (/^h[1-6]$/.test(node.tag))
    return "heading";
  if (node.tag === "main")
    return "main";
  return "";
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

function stripKssComments(source) {
  return String(source || "")
    .replace(/\/\*[\s\S]*?\*\//g, "")
    .replace(/\/\/.*$/gm, "");
}

function parseSelector(text) {
  let source = String(text || "").trim();
  const selector = {
    kind: "*",
    id: "",
    classes: [],
    attrs: {},
    state: "",
    specificity: 0
  };
  const stateMatch = source.match(/:([A-Za-z_][\w-]*)\s*$/);
  if (stateMatch) {
    selector.state = stateMatch[1];
    selector.specificity += 10;
    source = source.slice(0, stateMatch.index).trim();
  }
  source = source.replace(/\[([A-Za-z_][\w.-]*)\s*=\s*([^\]]+)\]/g, (_all, key, value) => {
    selector.attrs[key] = String(value).trim().replace(/^["']|["']$/g, "");
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

function parseKssValue(value) {
  const text = String(value || "").trim();
  const number = Number(text.replace(/px$/, ""));
  if (Number.isFinite(number) && /^-?\d+(?:\.\d+)?(?:px)?$/.test(text))
    return number;
  return text;
}

function parseKssDeclarationValue(name, value, tokens) {
  const parsed = parseKssValue(value);
  if (typeof parsed !== "string")
    return parsed;
  const key = parsed.trim();
  const property = String(name || "").toLowerCase();
  if (["background", "background-color", "foreground", "color", "border", "border-color", "focus", "focus-color", "background-end", "background_end"].includes(property))
    return tokens.colors.get(key) ?? parsed;
  if (["radius", "border-width", "border_width", "opacity", "padding-x", "padding_x", "padding-y", "padding_y", "gap", "font-size", "font_size", "icon-size", "icon_size", "offset-x", "offset_x", "offset-y", "offset_y"].includes(property))
    return tokens.lengths.get(key) ?? parsed;
  if (property === "material")
    return tokens.materials.get(key) ?? parsed;
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
        (kind === "length" || kind === "number") ? tokens.lengths :
        kind === "material" ? tokens.materials : null;
      if (!target)
        continue;
      for (const part of group[2].split(";")) {
        const colon = part.indexOf(":");
        if (colon < 0)
          continue;
        const name = part.slice(0, colon).trim();
        if (!name)
          continue;
        target.set(name, parseKssValue(part.slice(colon + 1)));
      }
    }
    cursor = close + 1;
    tokenPattern.lastIndex = close + 1;
  }
  stripped += text.slice(cursor);
  return { text: stripped, tokens };
}

export function parseWebStyleSheet(source) {
  const parsedTokens = parseWebStyleTokens(stripKssComments(source));
  const text = parsedTokens.text;
  const tokens = parsedTokens.tokens;
  const rules = [];
  let pack = "";
  let layer = 0;
  const itemPattern = /@([A-Za-z_][\w-]*)\s+([^;{}]+);|([^@{}]+)\{([^{}]*)\}/g;
  for (let match; (match = itemPattern.exec(text));) {
    if (match[1]) {
      const name = match[1].toLowerCase();
      const value = match[2].trim();
      if (name === "pack")
        pack = value;
      else if (name === "layer")
        layer = webStyleLayers[value.toLowerCase()] ?? layer;
      continue;
    }
    for (const selectorText of String(match[3] || "").split(",")) {
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
  return { pack, rules };
}

function styleStateMatches(name, state) {
  if (!name || name === "any")
    return true;
  const key = String(name).toLowerCase();
  if (key === "normal")
    return !Object.values(state || {}).some(Boolean);
  if (key === "hover" || key === "pressed" || key === "focus" || key === "focused")
    return !!state?.[key] || !!state?.[key === "focused" ? "focus" : key];
  return !!state?.[key];
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
    case "name": return facts.domName;
    case "value": return facts.domValue || facts.value;
    case "type": return facts.inputType;
    case "action": return facts.formAction;
    case "method": return facts.formMethod;
    case "enctype": return facts.formEncType;
    case "autocomplete": return facts.autoComplete;
    case "hidden": return facts.hidden;
    case "draggable": return facts.draggable;
    case "spellcheck": return facts.spellCheck;
    case "contenteditable": return facts.contentEditable;
    case "autofocus": return facts.autoFocus;
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
    case "minlength": return facts.minLength;
    case "maxlength": return facts.maxLength;
    case "inputmode": return facts.inputMode;
    case "multiple": return facts.multiple;
    case "for": return facts.htmlFor;
    default: return facts[key];
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
  if (key.startsWith("aria-"))
    return facts.ariaAttrs?.[key.slice(5)] ?? facts.extraAttrs?.[key];
  if (key.startsWith("aria."))
    return facts.ariaAttrs?.[key.slice(5).replace(/_/g, "-").toLowerCase()] ??
      facts.extraAttrs?.["aria-" + key.slice(5).replace(/_/g, "-").toLowerCase()];
  return undefined;
}

function selectorAriaAttrPresent(key, facts) {
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

function selectorMatchesFacts(selector, facts) {
  if (selector.kind !== "*" && selector.kind.toLowerCase() !== String(facts.kind || "").toLowerCase())
    return false;
  if (selector.id && selector.id !== facts.id && selector.id !== facts.name && selector.id !== facts.key)
    return false;
  for (const cls of selector.classes)
    if (!facts.classes?.includes(cls))
      return false;
  for (const [key, value] of Object.entries(selector.attrs)) {
    if (value === null) {
      if (!selectorAttrPresent(key, facts))
        return false;
    }
    else if (key === "role" && value !== facts.role)
      return false;
    else if (key === "state" && !styleStateMatches(value, facts.state))
      return false;
    else if (key.startsWith("data-") || key.startsWith("data.")) {
      if (String(selectorDataAttrValue(key, facts) ?? "") !== value)
        return false;
    }
    else if (key.startsWith("aria-") || key.startsWith("aria.")) {
      if (String(selectorAriaAttrValue(key, facts) ?? "") !== value)
        return false;
    }
    else if (!["role", "state"].includes(key) && String(selectorNativeAttrValue(key, facts) ?? "") !== value)
      return false;
  }
  return styleStateMatches(selector.state, facts.state);
}

function selectorMatchesWebNode(selector, node) {
  const facts = { ...(node?.styleFacts || {}), ...webNodeStyleFacts(node) };
  return selectorMatchesFacts(selector, facts);
}

export function resolveWebStyle(node, sheets = []) {
  const facts = node?.styleFacts || webNodeStyleFacts(node);
  const resolved = {};
  const scores = {};
  const list = Array.isArray(sheets) ? sheets : [sheets];
  for (const sheet of list) {
    const rules = typeof sheet === "string" ? parseWebStyleSheet(sheet).rules : (sheet?.rules || []);
    for (const rule of rules) {
      if (!selectorMatchesFacts(rule.selector, facts))
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

function applyResolvedWebStyle(el, style) {
  if (!el)
    return;
  for (const name of el.__kryAppliedStyleProps || [])
    el.style[name] = "";
  const applied = new Set();
  const set = (name, value) => {
    if (value === undefined || value === null || value === "")
      return;
    el.style[name] = typeof value === "number" && name !== "opacity" ? value + "px" : String(value);
    applied.add(name);
  };
  style = style || {};
  set("background", style.background ?? style["background-color"]);
  set("color", style.foreground ?? style.color);
  set("borderColor", style.border ?? style["border-color"]);
  set("borderWidth", style["border-width"] ?? style.border_width);
  set("borderRadius", style.radius);
  set("opacity", style.opacity);
  set("paddingLeft", style["padding-x"] ?? style.padding_x);
  set("paddingRight", style["padding-x"] ?? style.padding_x);
  set("paddingTop", style["padding-y"] ?? style.padding_y);
  set("paddingBottom", style["padding-y"] ?? style.padding_y);
  set("gap", style.gap);
  set("fontSize", style["font-size"] ?? style.font_size);
  if (style.border || style["border-color"] || style["border-width"] || style.border_width) {
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
  const frame = {
    app: rt?.app || null,
    nodes: (rt?.frame || []).map(webNodeFromWidget)
  };
  frame.metadata = webDocumentMetadata(frame);
  return frame;
}

function webDocumentMetadata(frame) {
  const page = (frame.nodes || []).find((node) => node.kind === "Page");
  return {
    title: pageMeta.title || page?.pageTitle || frame.app?.title || "",
    description: pageMeta.description || page?.pageDescription || "",
    canonicalURL: pageMeta.canonicalURL || page?.pageCanonicalURL || "",
    themeColor: pageMeta.themeColor || page?.pageThemeColor || ""
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
  el.addEventListener("wheel", (event) => {
    const docNode = el.__kryDocNode;
    const value = Number.isFinite(Number(event?.deltaY)) ? Number(event.deltaY) :
      Number.isFinite(Number(event?.wheelDelta)) ? -Number(event.wheelDelta) : 0;
    if (docNode?.wheelAction)
      docNode.wheelAction(value);
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
      docNode.submitAction(webFormValuesFromRoot(el.__kryMountRoot));
  });
  el.addEventListener("reset", (event) => {
    if (event?.preventDefault)
      event.preventDefault();
    const docNode = el.__kryDocNode;
    if (docNode?.resetAction)
      docNode.resetAction(webFormValuesFromRoot(el.__kryMountRoot));
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
  for (const key of [docNode.path, docNode.name, docNode.key, docNode.domId, docNode.domName]) {
    if (key)
      root.__kryFormValues.set(key, value);
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
  return docNode?.path || docNode?.name || docNode?.key || docNode?.domId || "";
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

function applyWebNode(el, docNode, rt) {
  el.__kryDocNode = docNode;
  el.__kryRuntime = rt;
  bindNodeEvents(el);
  const extraClasses = [...(el.__kryExtraClasses || [])];
  docNode.classes = [...new Set([...(docNode.classes || []), ...extraClasses])];
  docNode.extraAttrs = { ...(docNode.extraAttrs || {}), ...(el.__kryExtraAttrs || {}) };
  Object.assign(docNode.state, el.__kryExtraState || {});
  el.className = ["kryon-node", "kryon-" + docNode.kind.toLowerCase(), ...docNode.classes].join(" ");
  el.dataset.kryKind = docNode.kind;
  el.dataset.kryKey = docNode.key;
  el.dataset.kryRef = webNodeRef(docNode);
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
  if (docNode.name)
    el.dataset.kryName = docNode.name;
  else
    delete el.dataset.kryName;
  setAttr(el, "id", docNode.domId);
  setAttr(el, "name", docNode.domName);
  setAttr(el, "value", docNode.domValue);
  setAttr(el, "title", docNode.title);
  setAttr(el, "placeholder", docNode.placeholder);
  setAttr(el, "tabindex", docNode.tabIndex === null ? "" : String(docNode.tabIndex));
  setAttr(el, "role", docNode.role);
  setAttr(el, "aria-label", docNode.ariaLabel);
  setAttr(el, "aria-description", docNode.ariaDescription);
  setAttr(el, "aria-describedby", docNode.ariaDescribedBy);
  setAttr(el, "aria-controls", docNode.ariaControls);
  setAttr(el, "aria-live", docNode.ariaLive);
  if (docNode.onClick)
    el.dataset.kryOnClick = docNode.onClick;
  else
    delete el.dataset.kryOnClick;
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
  if (docNode.onWheel)
    el.dataset.kryOnWheel = docNode.onWheel;
  else
    delete el.dataset.kryOnWheel;
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
  setAttr(el, "aria-invalid", docNode.state.invalid ? "true" : "");
  setAttr(el, "aria-expanded", docNode.state.expanded ? "true" : "");
  if (docNode.tag === "details" || docNode.tag === "dialog") {
    setAttr(el, "open", docNode.state.open);
    el.open = !!docNode.state.open;
  } else {
    removeAttr(el, "open");
  }
  setAttr(el, "aria-checked",
    docNode.inputType === "checkbox" || docNode.inputType === "radio"
      ? (docNode.state.checked ? "true" : "false")
      : "");
  setAttr(el, "aria-current",
    docNode.tag === "a" && docNode.state.selected ? "page" : "");
  setAttr(el, "aria-level",
    docNode.role === "heading" && docNode.level ? String(docNode.level) : "");
  applyAriaAttrs(el, docNode.ariaAttrs);
  setAttr(el, "href", docNode.href);
  setAttr(el, "target", docNode.target);
  setAttr(el, "rel", docNode.rel);
  setAttr(el, "for", docNode.htmlFor);
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
  setAttr(el, "download", docNode.download);
  setAttr(el, "formnovalidate", docNode.formNoValidate);
  setAttr(el, "novalidate", docNode.noValidate);
  setAttr(el, "popover", docNode.popover);
  setAttr(el, "popovertarget", docNode.popoverTarget);
  setAttr(el, "popovertargetaction", docNode.popoverTargetAction);
  el.hidden = !!docNode.hidden;
  if (docNode.draggable === "true" || docNode.draggable === "false")
    el.draggable = docNode.draggable === "true";
  if (docNode.spellCheck)
    el.spellcheck = docNode.spellCheck === "true";
  if (docNode.contentEditable)
    el.contentEditable = docNode.contentEditable;
  el.autofocus = !!docNode.autoFocus;
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
  applyExtraAttrs(el, docNode.extraAttrs);
  if (docNode.tag === "img") {
    setAttr(el, "src", docNode.asset);
    setAttr(el, "alt", docNode.alt);
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
  } else if (docNode.tag === "textarea") {
    el.value = docNode.value;
  } else {
    el.textContent = docNode.text;
  }
}

function ensureMountRoot(node) {
  let root = node.__kryRuntimeRoot || null;
  if (root && root.parentNode === node)
    return root;
  root = document.createElement("div");
  root.className = "kryon-runtime";
  root.dataset.kryRuntime = "web-document";
  root.style.position = "relative";
  root.style.minHeight = "100%";
  root.style.fontFamily = "system-ui, sans-serif";
  root.__kryChildren = new Map();
  node.__kryRuntimeRoot = root;
  node.appendChild(root);
  return root;
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
  const children = root.__kryChildren || new Map();
  const elementsByPath = new Map();
  const live = new Set();
  root.__kryNodes = new Map();
  root.__kryElementsByPath = new Map();
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
    if (ref)
      root.__kryDomObjects.set(ref, { ref, node: docNode, element: el });
    const sourceRef = webNodeSourceRef(docNode);
    if (sourceRef) {
      const sourceObject = { ref: sourceRef, node: docNode, element: el };
      if (!root.__kryDomObjects.has(sourceRef))
        root.__kryDomObjects.set(sourceRef, sourceObject);
      pushIndex(root.__kryDomObjectsBySource, sourceRef, sourceObject);
    }
    const sourceColumnRef = webNodeSourceColumnRef(docNode);
    if (sourceColumnRef) {
      const sourceColumnObject = { ref: sourceColumnRef, node: docNode, element: el };
      if (!root.__kryDomObjects.has(sourceColumnRef))
        root.__kryDomObjects.set(sourceColumnRef, sourceColumnObject);
      pushIndex(root.__kryDomObjectsBySource, sourceColumnRef, sourceColumnObject);
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
    const parent = docNode.parentPath && elementsByPath.get(docNode.parentPath)
      ? elementsByPath.get(docNode.parentPath)
      : root;
    parent.appendChild(el);
    live.add(identity);
  }
  for (const [identity, el] of Array.from(children.entries())) {
    if (!live.has(identity)) {
      if (el.parentNode && typeof el.parentNode.removeChild === "function")
        el.parentNode.removeChild(el);
      children.delete(identity);
    }
  }
  root.__kryChildren = children;
  if (rt)
    rt.mounted = true;
  return rt;
}

function mountedRoot(target) {
  const node = typeof target === "string" && typeof document !== "undefined"
    ? document.querySelector(target)
    : target;
  return node?.__kryRuntimeRoot || null;
}

export function findWebNode(rt, query) {
  const text = String(query || "");
  const frame = webDocumentFrame(rt);
  return frame.nodes.find((node) =>
    node.path === text || node.name === text || node.key === text ||
    node.domId === text || sourceRefMatches(node, text)) || null;
}

export function webNodeQueryAll(rt, selector) {
  const text = String(selector || "").trim();
  if (!text)
    return [];
  const frame = webDocumentFrame(rt);
  const exact = frame.nodes.filter((node) =>
    node.path === text || node.name === text || node.key === text ||
    node.domId === text || sourceRefMatches(node, text));
  if (exact.length)
    return exact;
  const parsed = parseSelector(text);
  return frame.nodes.filter((node) => selectorMatchesWebNode(parsed, node));
}

export function webNodeQuery(rt, selector) {
  return webNodeQueryAll(rt, selector)[0] || null;
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

export function findWebElement(target, query) {
  const root = mountedRoot(target);
  if (!root)
    return null;
  const text = String(query || "");
  if (root.__kryChildren?.has(text))
    return root.__kryChildren.get(text);
  if (root.__kryElementsByPath?.has(text))
    return root.__kryElementsByPath.get(text);
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
    if (node && (node.path === text || node.key === text ||
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

export function webDOMObject(target, query) {
  const root = mountedRoot(target);
  if (!root)
    return null;
  const text = String(query || "");
  if (root.__kryDomObjects?.has(text))
    return root.__kryDomObjects.get(text);
  const element = findWebElement(target, text);
  const node = element?.__kryDocNode || null;
  return node && element ? { ref: webNodeRef(node), node, element } : null;
}

function webDOMObjectForNode(root, node) {
  if (!root || !node)
    return null;
  const element = node.path ? root.__kryElementsByPath?.get(node.path) : null;
  return element ? { ref: webNodeRef(node), node, element } : null;
}

export function webDOMObjectFromElement(element) {
  let el = element || null;
  while (el) {
    const node = el.__kryDocNode || null;
    if (node)
      return { ref: webNodeRef(node), node, element: el };
    el = el.parentNode || null;
  }
  return null;
}

export function webDOMObjects(target) {
  const root = mountedRoot(target);
  if (!root)
    return [];
  return [...(root.__kryChildren?.values?.() || [])]
    .map((element) => {
      const node = element.__kryDocNode;
      return node ? { ref: webNodeRef(node), node, element } : null;
    })
    .filter(Boolean);
}

export function webDOMParent(target, query) {
  const root = mountedRoot(target);
  const object = root ? webDOMObject(target, query) : null;
  const parentPath = object?.node?.parentPath || "";
  if (!root || !object || !parentPath || parentPath === object.node.path)
    return null;
  return webDOMObjectForNode(root, root.__kryNodes?.get(parentPath));
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
  "disabled", "loading", "selected", "checked", "invalid", "expanded",
  "open", "hover", "pressed", "focus"
]);

function cleanDOMStateName(name) {
  const key = String(name || "").trim().toLowerCase().replace(/-/g, "_");
  if (key === "focused")
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

function syncDOMAttributeMutation(el) {
  const docNode = el?.__kryDocNode;
  if (!el || !docNode)
    return null;
  docNode.extraAttrs = { ...(el.__kryExtraAttrs || {}) };
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

export function webFormValues(target) {
  const root = mountedRoot(target);
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

export function BeginCanvas(canvas) {
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
  "Background", "Bevel", "BottomNav", "Button", "Card", "CanvasGrid", "Checkbox",
  "ClearBackground", "Collapsible", "Column", "Dropdown",
  "Icon", "Fieldset", "Link", "ListBox",
  "Modal", "Paragraph", "Image", "Progress", "Radio", "Rect",
  "Row", "Screen", "Scroll", "SelectableText", "SetCurrentTheme",
  "SetThemeDarkMode", "ShowToast", "Slider", "Spinbox", "Stack", "TabBar",
  "Text", "TextArea", "TextField", "TitleBar",
  "Toggle", "Toolbar"
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
export function Card(...args) { return struct("Card", args); }
export function CanvasGrid(...args) { return struct("CanvasGrid", args); }
export function Checkbox(...args) { return struct("Checkbox", args); }
export function ClearBackground(...args) { return struct("ClearBackground", args); }
export function Collapsible(...args) { return struct("Collapsible", args); }
export function Column(...args) { return struct("Column", args); }
export function Dropdown(...args) { return struct("Dropdown", args); }
export function Icon(...args) { return struct("Icon", args); }
export function Fieldset(...args) { return struct("Fieldset", args); }
export function Link(...args) { return struct("Link", args); }
export function ListBox(...args) { return struct("ListBox", args); }
export function Modal(...args) { return struct("Modal", args); }
export function Paragraph(...args) { return struct("Paragraph", args); }
export function Image(...args) { return struct("Image", args); }
export function Progress(...args) { return struct("Progress", args); }
export function Radio(...args) { return struct("Radio", args); }
export function Rect(...args) { return struct("Rect", args); }
export function Row(...args) { return struct("Row", args); }
export function Screen(...args) { return struct("Screen", args); }
export function Scroll(...args) { return struct("Scroll", args); }
export function SelectableText(...args) { return struct("SelectableText", args); }
export function SetCurrentTheme(...args) { return struct("SetCurrentTheme", args); }
export function SetThemeDarkMode(dark) {
  activeThemeMode = dark ? 2 : 1;
  if (activeThemeFamily) {
    activeTheme = activeThemeMode === 2 ? activeThemeFamily.dark : activeThemeFamily.light;
  }
  return struct("SetThemeDarkMode", [dark]);
}
export function ShowToast(...args) { return struct("ShowToast", args); }
export function Slider(...args) { return struct("Slider", args); }
export function Spinbox(...args) { return struct("Spinbox", args); }
export function Stack(...args) { return struct("Stack", args); }
export function TabBar(...args) { return struct("TabBar", args); }
export function Text(...args) { return struct("Text", args); }
export function TextArea(...args) { return struct("TextArea", args); }
export function TextField(...args) { return struct("TextField", args); }
export function TitleBar(...args) { return struct("TitleBar", args); }
export function Toggle(...args) { return struct("Toggle", args); }
export function Toolbar(...args) { return struct("Toolbar", args); }
