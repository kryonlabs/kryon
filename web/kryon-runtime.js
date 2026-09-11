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
export const SideTop = 0;
export const SideBottom = 1;
export const SideLeft = 2;
export const SideRight = 3;
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
export const PICTURE_FIT_STRETCH = 0;
export const PICTURE_FIT_CONTAIN = 1;
export const PICTURE_FIT_COVER = 2;

export function createRuntime(options = {}) {
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
    tabBarScopes: [],
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
  if (rt.tabBarScopes.length !== 0)
    throw new Error("unclosed tab bar scope at frame boundary");
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
  if (rt.tabBarScopes.length !== 0)
    throw new Error("unclosed tab bar scope at frame boundary");
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

export function widget(rt, name, args, state = null) {
  const item = { kind: "widget", name, args };
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
  if (name === "EndTabItem") {
    const scope = rt.tabBarScopes[rt.tabBarScopes.length - 1];
    if (!scope || !scope.itemOpen)
      throw new Error("EndTabItem without selected BeginTabItem");
    scope.itemOpen = false;
    return false;
  }
  if (name === "EndTabBar") {
    const scope = rt.tabBarScopes[rt.tabBarScopes.length - 1];
    if (!scope || scope.itemOpen)
      throw new Error("unbalanced EndTabBar");
    rt.tabBarScopes.pop();
    return false;
  }
  if (name === "BeginTabBar") {
    const scope = handleTabBar(rt, state, args,
      !rt.disabledStack.some(Boolean));
    if (scope.open)
      rt.tabBarScopes.push(scope);
    return scope.open;
  }
  if (name === "BeginTabItem") {
    const scope = rt.tabBarScopes[rt.tabBarScopes.length - 1];
    if (!scope || scope.itemOpen)
      throw new Error("unbalanced BeginTabItem");
    const index = numberValue(args, -1);
    if (index < 0 || index >= scope.count || index !== scope.selected)
      return false;
    scope.itemOpen = true;
    return true;
  }
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
  case "Combobox":
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
    const args = item.args && typeof item.args === "object"
      ? JSON.stringify(item.args, (_, value) => typeof value === "bigint" ? String(value) : value)
      : item.args;
    el.textContent = item.name + (args ? "(" + args + ")" : "");
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

export function Scale(value) {
  return value | 0;
}

export function GetScreenWidth() {
  return 800;
}

export function GetScreenHeight() {
  return 600;
}

export function GetUIViewWidth() {
  return GetScreenWidth();
}

export function GetUIPageSidePadding() {
  return Scale(24);
}

export function GetThemeBackground() { return GetTheme().colors.background; }
export function GetThemeSurface() { return GetTheme().colors.surface; }
export function GetThemeText() { return GetTheme().colors.text; }
export function GetThemeButton() { return GetTheme().colors.accent; }
export function GetThemeIcon() { return GetTheme().colors.icon; }
export function GetThemeLink() { return GetTheme().colors.link; }

export function SystemThemePrefersDark() { return false; }

export function Fade(color, alpha) {
  return Object.assign({}, color, { a: Math.round((alpha || 0) * 255) });
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

export function DarkenUIColor(color, amount) {
  return Color(
    clampByte((color?.r || 0) - amount),
    clampByte((color?.g || 0) - amount),
    clampByte((color?.b || 0) - amount),
    color?.a ?? 255
  );
}

export function LightenUIColor(color, amount) {
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

export function BeginFrameBox(bounds) {
  return { bounds, cursor: NewVector2(bounds?.x || 0, bounds?.y || 0) };
}

export function FramePack(frame, side, size) {
  return { frame, side, size };
}

export function GridCell(grid, column, row, columnSpan = 1, rowSpan = 1) {
  return { grid, column, row, columnSpan, rowSpan };
}

export function Place(bounds, item) {
  return { bounds, item };
}

export function BeginUIScrollContainer(...args) {
  return struct("BeginUIScrollContainer", args);
}

export function CanvasHitTest(canvas, screen) {
  return { canvas, screen, hit: false, active: false, world: screen || NewVector2() };
}

const runtimeCallNames = [
  "Background", "Bevel", "BottomNav", "Button", "Card", "CanvasGrid", "Checkbox",
  "ClearBackground", "Collapsible", "Column", "Combobox", "Dropdown", "EndCanvas",
  "EndScroll", "Href", "Icon", "LabelFrame", "ListBox",
  "MenuButton", "Modal", "Notebook", "Paragraph", "Picture", "Progress", "Radio", "Rect",
  "Row", "Screen", "Scroll", "SelectableText", "SetCurrentTheme",
  "SetThemeDarkMode", "ShowToast", "Slider", "Spinbox", "Stack", "TabBar",
  "Text", "TextArea", "TextField", "TextLines", "TitleBar",
  "SplitButton", "Toggle", "Toolbar", "TopNav"
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
export function BeginButton(...args) { return struct("BeginButton", args); }
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
export function Icon(...args) { return struct("Icon", args); }
export function LabelFrame(...args) { return struct("LabelFrame", args); }
export function ListBox(...args) { return struct("ListBox", args); }
export function MenuButton(...args) { return struct("MenuButton", args); }
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
export function SplitButton(...args) { return struct("SplitButton", args); }
export function Stack(...args) { return struct("Stack", args); }
export function TabBar(...args) { return struct("TabBar", args); }
export function Text(...args) { return struct("Text", args); }
export function TextArea(...args) { return struct("TextArea", args); }
export function TextField(...args) { return struct("TextField", args); }
export function TextLines(...args) { return struct("TextLines", args); }
export function TitleBar(...args) { return struct("TitleBar", args); }
export function Toggle(...args) { return struct("Toggle", args); }
export function Toolbar(...args) { return struct("Toolbar", args); }
export function TopNav(...args) { return struct("TopNav", args); }
