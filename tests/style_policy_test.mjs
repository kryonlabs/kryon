import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";
import * as host from "../web/kryon-runtime.js";

const owner = { open: 0 };
const reference = host.ref(owner, "open");
const sourceValue = { style: { offset: [0, 1] }, open: reference };
const copiedValue = host.copyValue(sourceValue);
copiedValue.style.offset[1] = -1;
assert.equal(sourceValue.style.offset[1], 1, "nested value fields must not alias");
assert.equal(copiedValue.open, reference, "explicit references must retain identity");
copiedValue.open.value = 1;
assert.equal(owner.open, 1, "copied references must still update their owner");
assert.equal(host.copyValue(null), null);
assert.equal(host.copyValue(12), 12);

for (const [type, fields] of Object.entries({
  Vector2: ["x", "y"], Vector3: ["x", "y", "z"],
  Vector4: ["x", "y", "z", "w"],
  Rectangle: ["x", "y", "width", "height"], Color: ["r", "g", "b", "a"]
})) {
  const original = host.recordValue(type, fields.map((_, index) => index + 1));
  const copied = host.copyValue(original);
  fields.forEach((field, index) => {
    copied[field] = 10 + index;
    assert.equal(copied[index], 10 + index);
    assert.equal(original[field], index + 1);
  });
  assert.deepEqual(host.recordValue(type, []), fields.map(() => 0));
  assert.deepEqual(host.recordValue(type, {}), Object.fromEntries(fields.map(field => [field, 0])));
}

const family = { light: host.ThemeDefaultLight(), dark: host.ThemeDefaultDark() };
host.SetThemeFamily(family);
host.SetThemeMode(host.THEME_MODE_DARK);
assert.equal(host.GetThemeMode(), 2);
assert.equal(host.GetTheme(), family.dark);
host.SetThemeMode(host.THEME_MODE_LIGHT);
assert.equal(host.GetThemeMode(), 1);
assert.equal(host.GetTheme(), family.light);
const previousMatchMedia = globalThis.matchMedia;
try {
  let prefersDark = true;
  globalThis.matchMedia = () => ({ matches: prefersDark });
  host.SetThemeMode(99);
  assert.equal(host.GetThemeMode(), host.THEME_MODE_SYSTEM);
  assert.equal(host.GetTheme(), family.dark);
  prefersDark = false;
  assert.equal(host.GetTheme(), family.light);
} finally {
  if (previousMatchMedia === undefined)
    delete globalThis.matchMedia;
  else
    globalThis.matchMedia = previousMatchMedia;
  host.SetTheme(family.light);
}

// Load actual generated modules, including their runtime and policy imports.
// No source rewriting or injected cross-module bindings are permitted here.
const style = await import(pathToFileURL(process.argv[2]).href);
const namedStyle = { fields: host.StyleTypeface, typeface: "semibold" };
assert.equal(style.Style_MergeValues(null, undefined, undefined,
  namedStyle, { fields: 0, typeface: "ignored" }).typeface, "semibold");
assert.equal(style.Style_MergeValues(null, undefined, undefined,
  namedStyle, { fields: host.StyleTypeface, typeface: "" }).typeface, "");
const textPolicy = await import(new URL("./text.js", pathToFileURL(process.argv[2])).href);
for (const [requested, inherited, fallback, expected] of [
  [13, 27, 18, 13], [0, 27, 18, 27], [-1, 27, 18, 27],
  [0, 0, 18, 18], [0, -1, 18, 18], [0, 0, 0, 16],
]) {
  assert.equal(style.Style_ResolveFont(null, undefined, undefined, requested, inherited, fallback), expected);
  assert.equal(textPolicy.Text_ResolveTextStyle(null, undefined, undefined,
    requested, inherited, fallback, 0, 0, 0xffffffff, false, false, false, false, 0).font, expected);
}
assert.equal(textPolicy.Text_ResolveTextStyle(null, undefined, undefined,
  0, 27, 16, 0, 0x11223380, 0xffffffff, true, false, false, true, 0).color, 0x11223380);
assert.equal(textPolicy.Text_ResolveTextStyle(null, undefined, undefined,
  0, 27, 16, 0x44556680, 0x11223380, 0xffffffff, true, true, false, true, 0).color, 0x44556639);
for (const present of [false, true]) {
  const appearance = textPolicy.Text_ResolveTextStyle(null, undefined, undefined,
    0, 27, 16, 0, 0x112233ff, 0xffffffff, true, present, false, false, 0);
  assert.equal(appearance.color, present ? 0 : 0x112233ff);
}
const splitButton = await import(new URL("./split_button.js", pathToFileURL(process.argv[2])).href);
for (const [width, height, resolvedWidth, actionWidth] of [
  [120, 40, 120, 80], [24, 40, 80, 40], [80, 40, 80, 40],
  [100.5, 27.25, 100.5, 73.25],
]) {
  assert.deepEqual(splitButton.SplitButton_ResolveLayout(null, undefined, undefined, width, height), {
    width: resolvedWidth, action_width: actionWidth, menu_offset: actionWidth,
    menu_width: height, divider_inset: 8,
  });
}
for (const [requested, minimum, content, padding, expected] of [
  [0, 40, 18, 8, 40], [0, 40, 27, 20, 67],
  [0, 40, 60, -10, 60], [24, 40, 27, 20, 24],
  [0, 0, -1, -1, 0],
]) {
  assert.equal(style.Style_FitHeight(null, undefined, undefined,
    requested, minimum, content, padding), expected);
}
const surface = await import(new URL("./surface.js", pathToFileURL(process.argv[2])).href);
const borderlessHoverBevel = surface.Surface_LightfieldLayer(null, undefined, undefined,
  5, 72, 36, 8, 1, 0x092039ff, 0, 0x409cffff, 0x409cffff, 1, 0, 0, false, 1, 0x00172dff);
assert.equal(borderlessHoverBevel.end_color, surface.Surface_Opacity(null, undefined, undefined,
  surface.Surface_ChromaColor(null, undefined, undefined, 0x409cffff, 255), 0.5775));
const broadLightFace = surface.Surface_LightfieldLayer(null, undefined, undefined,
  3, 720, 40, 8, 1, 0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
  0, 0, 0, false, 1, 0xffffffff);
assert.equal(broadLightFace.color, 0x006cffff);
const tallLightReflection = surface.Surface_LightfieldLayer(null, undefined, undefined,
  8, 72, 48, 8, 1, 0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
  0, 0, 0, false, 1, 0xffffffff);
const tallReflectedColor = surface.Surface_LiftColor(null, undefined, undefined,
  surface.Surface_LiftColor(null, undefined, undefined, 0x006cffff, 1), 1);
assert.equal(tallLightReflection.end_color, surface.Surface_Opacity(null, undefined, undefined,
  surface.Surface_GradientColor(null, undefined, undefined, tallReflectedColor, 0xffffffff, 0.22), 0.76));
assert.equal(tallLightReflection.blur, 0);
assert.ok(tallLightReflection.inner_blur > 0);
assert.equal(surface.Surface_SampleCoverage(null, undefined, undefined, tallLightReflection, -1, 10, 1), 0);
for (const [background, opacity] of [[0xffffffff, 14], [0xd0d0d0ff, 24], [0xffe7ffff, 24], [0xffdfffff, 34], [0x006cffff, 34], [0x101828ff, 34]]) {
  const shadow = surface.Surface_LightfieldLayer(null, undefined, undefined,
    1, 72, 40, 8, 1, background, 0x064cffff, 0x064cffff, 0x064cffff,
    1, 0, 0, false, 1, 0xffffffff);
  assert.equal(shadow.color, opacity);
}
for (let step = 0; step <= 4; step++) {
  const hover = step / 4;
  const bevel = surface.Surface_LightfieldLayer(null, undefined, undefined,
    5, 72, 38, 8, 1, 0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
    hover, 0, 0, false, 1, 0x00172dff);
  const faceY = surface.Surface_FaceOffset(null, undefined, undefined, hover, 0, false);
  assert.equal(bevel.y, faceY + 1 - hover);
  assert.equal(bevel.y + bevel.height, faceY + 37);
  assert.equal(surface.Surface_SampleCoverage(null, undefined, undefined, bevel, 30, -1, 1), 0);
}
for (const [light, alpha] of [[0xff0000ff, 249], [0x0000ffff, 249], [0x00ff00ff, 165], [0x808080ff, 165]]) {
  const bevel = surface.Surface_LightfieldLayer(null, undefined, undefined,
    5, 72, 48, 8, 1, 0x006cffff, 0x006cffff, light, light, 0, 0, 0, false, 1, 0x00172dff);
  assert.equal(bevel.end_color & 255, alpha);
  const pressed = surface.Surface_LightfieldLayer(null, undefined, undefined,
    5, 72, 48, 8, 1, 0x006cffff, 0x006cffff, light, light, 0, 1, 0, false, 1, 0x00172dff);
  assert.equal(pressed.end_color & 255, 0);
}
const innerReflection = surface.Surface_LightfieldLayer(null, undefined, undefined,
  8, 720, 40, 8, 1, 0x006cff80, 0x006cffff, 0x006cffff, 0x409cffff,
  0, 0, 0, false, 1, 0x00172dff);
const innerLight = surface.Surface_LiftColor(null, undefined, undefined,
  surface.Surface_LiftColor(null, undefined, undefined, 0x006cffff, 1), 1);
const innerColor = surface.Surface_GradientColor(null, undefined, undefined, innerLight, 0xffffffff, Math.fround(0.11));
const innerStrength = Math.fround(Math.fround(0.38) * Math.fround(128 / 255));
assert.equal(innerReflection.end_color, surface.Surface_Opacity(null, undefined, undefined, innerColor, innerStrength));
assert.equal(surface.Surface_SampleCoverage(null, undefined, undefined, innerReflection, -1, 10, 1), 0);
for (const [color, amount, expected] of [
  [0x006cff80, 0, 0x006cff80], [0x006cff80, 0.5, 0x008bff80],
  [0x006cff80, 1, 0x00aaff80], [0x006cff00, 2, 0x00aaff00],
  [0x80808080, 1, 0x80808080],
]) {
  assert.equal(surface.Surface_LiftColor(null, undefined, undefined, color, amount), expected,
    "color lift must preserve the peak, neutral colors, and source alpha");
}
for (const press of [0, 0.25, 0.5, 0.75, 1]) {
  const field = surface.Surface_LightfieldLayer(null, undefined, undefined,
    0, 720, 40, 8, 1, 0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
    0, press, 0, false, 1, 0x00172dff);
  const pressure = Math.fround(1 - Math.fround(press * Math.fround(0.8)));
  const resting = Math.fround(Math.fround(0.05) * pressure);
  const emission = Math.fround(0.25 * (1 - press));
  const strength = Math.fround(resting + emission);
  assert.equal(field.color, surface.Surface_Opacity(null, undefined, undefined, 0x006cffff, strength),
    "wide saturated contact light must fade continuously under pressure");
}
for (const ambient of [0x092039ff, 0xffffffff]) {
  for (const color of [0x006cffff, 0x006cff80, 0x006cff00, 0x999999ff]) {
    const ring = surface.Surface_LoadingRing(null, undefined, undefined,
      40, 40, 18, 0, color, ambient);
    ring.start_angle = 0;
    ring.end_angle = 270;
    for (const [x, y] of [[-0.5, -0.5], [10, -10], [16, 0]]) {
      const sample = surface.Surface_LoadingSample(null, undefined, undefined, ring, x, y);
      assert.equal(sample.glow & 255, 0, "loading glow must preserve the opening, gap, and paint bounds");
    }
    const sample = surface.Surface_LoadingSample(null, undefined, undefined, ring, 11, 0);
    const emits = ambient === 0x092039ff && color !== 0x999999ff && (color & 255) !== 0;
    assert.equal((sample.glow & 255) !== 0, emits, "only visible chromatic dark rings emit a glow");
  }
}
for (const focus of [0, 0.25, 0.5, 0.75, 1]) {
  const shadow = surface.Surface_LightfieldLayer(null, undefined, undefined,
    1, 72, 40, 8, 1, 0xeef5ffff, 0x064cffff, 0x064cffff, 0x064cffff,
    0, 0, focus, false, 1, 0xffffffff);
  assert.equal(shadow.color, Math.trunc(24 * (1 - 0.5 * focus)),
    "focus light must continuously soften the pale contact shadow");
}
for (const height of [32, 40, 42, 44, 48, 64]) {
  for (const press of [0, 0.25, 0.5, 0.75, 1]) {
    let raised = Math.max(0, Math.min(1, (height - 40) / 8));
    raised *= raised * (1 - press);
    const contact = surface.Surface_LightfieldLayer(null, undefined, undefined,
      2, 72, height, 8, 1, 0x006cffff, 0x006cffff, 0x006cffff, 0x409cffff,
      0, press, 0, false, 1, 0x092039ff);
    assert.equal(contact.blur, 2 + 3 * (1 - press) + 3 * raised);
    assert.equal(contact.color & 255, Math.trunc(34 - 26 * press + 70 * raised));
  }
}
for (const [chroma, edgeStrength] of [[0, 0.15], [128, 0.15], [160, 0.225], [192, 0.30], [255, 0.30]]) {
  for (const borderAlpha of [0, 255]) {
    const border = ((chroma << 24) | borderAlpha) >>> 0;
    for (const focusAlpha of [0, 85, 170, 255]) {
      const focusColor = (0x409cff00 | focusAlpha) >>> 0;
      for (const focus of [0, 0.25, 0.5, 0.75, 1]) {
        const bloom = surface.Surface_LightfieldLayer(null, undefined, undefined,
          11, 72, 40, 8, 1, 0x006cffff, border, border, focusColor,
          0, 0, focus, false, 1, 0x092039ff);
        const strength = borderAlpha ? edgeStrength * focus : 0.15 * focus * 0.35;
        const color = surface.Surface_DepthColor(null, undefined, undefined, focusColor, 1);
        assert.equal(bloom.color, surface.Surface_Opacity(null, undefined, undefined, color, strength));
        assert.equal(bloom.outside_only, true);
        assert.equal(surface.Surface_SampleCoverage(null, undefined, undefined, bloom, 36, 20, 1), 0);
      }
    }
  }
}
for (const ambient of [0x092039ff, 0xffffffff]) {
  for (const phase of [0.25, 375.5, 997.75, 1499.75]) {
    const ring = elapsed => surface.Surface_LoadingRing(null, undefined, undefined,
      72, 40, 18, elapsed, 0x006cff80, ambient);
    assert.deepEqual(ring(150000000000 + phase), ring(phase), "long-running loading phase drifted");
  }
}
const flatStyle = style.Style_MergeValues(null, undefined, undefined,
  { fields: host.StyleMaterial, material: host.MaterialFlat },
  { fields: host.StyleMaterial, material: host.MaterialLightfield });
assert.equal(flatStyle.material, host.MaterialLightfield);
assert.equal(surface.Surface_MaterialLayerCount(null, undefined, undefined, host.MaterialFlat), 3);
assert.equal(surface.Surface_MaterialLayerCount(null, undefined, undefined, host.MaterialLightfield), 12);
for (let index = 0; index < surface.Surface_MaterialLayerCount(null, undefined, undefined, host.MaterialLightfield); index++) {
  const layer = surface.Surface_MaterialLayer(null, undefined, undefined,
    host.MaterialFlat, index, 80, 40, 8, 2, 0x12345680, 0x789abc80,
    0xffffffff, 0xff000080, 1, 1, 1, false, 1, 0x092039ff);
  assert.equal(layer.blur, 0);
  assert.equal(layer.inner_blur, 0);
  assert.equal(layer.color & 255, index < 3 ? 128 : 0);
}
// The same content box must reach every transpilation target unchanged.
for (const [width, height, paddingX, paddingY, expected] of [
  [200, 100, 25, 4, { x: 25, y: 4, width: 150, height: 92 }],
  [200, 100, 0, 0, { x: 0, y: 0, width: 200, height: 100 }],
  [20, 10, 30, 40, { x: 30, y: 40, width: 0, height: 0 }],
  [20, 10, -1, -2, { x: 0, y: 0, width: 20, height: 10 }],
]) {
  assert.deepEqual(style.Style_ContentBounds(null, undefined, undefined,
    width, height, paddingX, paddingY), expected);
}
const stateStyles = Object.fromEntries(
  ["normal", "hover", "pressed", "focused", "disabled", "loading", "selected"]
    .map((name, index) => [name, { fields: 16, radius: index + 1 }]));
const stateStylesBefore = structuredClone(stateStyles);
for (let state = 0; state <= 7; state++) {
  for (let bits = 0; bits < 64; bits++) {
    const disabled = Boolean(bits & 1), loading = Boolean(bits & 2);
    const pressed = Boolean(bits & 4), hovered = Boolean(bits & 8);
    const focused = Boolean(bits & 16), selected = Boolean(bits & 32);
    let expected = 1;
    if (selected) expected = 7;
    if (focused) expected = 4;
    if (hovered) expected = 2;
    if (pressed) expected = 3;
    if (state !== 0) expected = state;
    if (loading) expected = 6;
    if (disabled) expected = 5;
    assert.deepEqual(style.Style_ResolveInteraction(null, undefined, undefined,
      state, disabled, loading, pressed, hovered, focused, selected), {
      state: expected,
      hovered: state === 0 ? hovered : expected === 2,
      pressed: state === 0 ? pressed : expected === 3,
      focused: state === 0 ? focused : expected === 4,
    });
  }
}
for (let state = 1; state <= 7; state++) {
  const resolved = style.Style_ResolveValues(null, undefined, undefined,
    { fields: 1, background: 0x11223300 }, stateStyles, state);
  assert.equal(resolved.radius, state);
  assert.equal(resolved.background, 0x11223300);
}
assert.deepEqual(stateStyles, stateStylesBefore);
const layer = { x: 0, y: 0, width: 72, height: 40, radius: 8,
  stroke: 0, blur: 0, inner_blur: 0, color: 0x112233ff,
  end_color: 0x8899aa00, gradient: true, gradient_bias: 0 };
assert.equal(surface.Surface_SampleColor(null, undefined, undefined, layer, -1), layer.color);
assert.equal(surface.Surface_SampleColor(null, undefined, undefined, layer, 2), layer.end_color);
layer.gradient_bias = 1;
assert.equal(surface.Surface_SampleColor(null, undefined, undefined, layer, 0.5),
  surface.Surface_GradientColor(null, undefined, undefined, layer.color, layer.end_color, 0.25));
layer.gradient = false;
assert.equal(surface.Surface_SampleColor(null, undefined, undefined, layer, 0.5), layer.color);
const data = (overrides = {}) => ({
  fields: 0, background: 0, foreground: 0, border: 0, focus: 0,
  radius: 0, border_width: 0, opacity: 0, padding_x: 0, padding_y: 0,
  gap: 0, font_size: 0, icon_size: 0, offset_x: 0, offset_y: 0,
  background_end: 0, ...overrides,
});
// Every public presence bit must select exactly its canonical .kry field(s).
// Zero values are deliberate overrides, not absent fields.
const fields = [
  [host.StyleBackground, ["background"]],
  [host.StyleForeground, ["foreground"]],
  [host.StyleBorder, ["border"]],
  [host.StyleFocus, ["focus"]],
  [host.StyleRadius, ["radius"]],
  [host.StyleBorderWidth, ["border_width"]],
  [host.StyleOpacity, ["opacity"]],
  [host.StylePaddingX, ["padding_x"]],
  [host.StylePaddingY, ["padding_y"]],
  [host.StyleGap, ["gap"]],
  [host.StyleFontSize, ["font_size"]],
  [host.StyleIconSize, ["icon_size"]],
  [host.StyleContentOffset, ["offset_x", "offset_y"]],
  [host.StyleBackgroundEnd, ["background_end"]],
];
const allFields = fields.flatMap(([, names]) => names);
for (const [mask, changed] of fields) {
  assert.ok(Number.isInteger(mask) && mask > 0);
  const base = data(Object.fromEntries(allFields.map(name => [name, 1])));
  const override = data({ fields: mask });
  const merged = style.Style_MergeValues(null, undefined, undefined, base, override);
  for (const name of allFields)
    assert.equal(merged[name], changed.includes(name) ? 0 : 1, `presence mask ${mask}: ${name}`);
}
const resolved = data({ padding_x: 11, font_size: 19 });
const normal = data({ fields: 8192, background: 0x112233ff,
  background_end: 0x12345600, radius: 2 });
const hover = data({ background: 0x44556680, radius: 4 });
const press = data({ radius: 8 });
const focus = data({ fields: 8192, background: 0x77889900, radius: 6 });
const originals = structuredClone([resolved, normal, hover, press, focus]);
const frame = style.Style_TransitionFrame(null, undefined, undefined,
  resolved, normal, hover, press, focus, 0.5, 0.25, 0.75);
assert.equal(frame.value.padding_x, 11);
assert.equal(frame.value.font_size, 19);
assert.equal(frame.fill.normal, true);
assert.equal(frame.fill.hover, false);
assert.equal(frame.fill.press, false);
assert.equal(frame.fill.focus, true);
assert.equal(frame.fill.normal_end, 0x12345600);
assert.equal(frame.fill.focus_end, 0);
assert.equal(frame.fill.hover_amount, 0.5);
assert.equal(frame.fill.press_amount, 0.25);
assert.equal(frame.fill.focus_amount, 0.75);
assert.deepEqual([resolved, normal, hover, press, focus], originals);
const button = await import(pathToFileURL(process.argv[3]).href);
for (const surfaceColor of [0x092039ff, 0xffffffff]) {
  const colored = button.Button_ButtonBorder(null, undefined, undefined,
    1, 2, 5, surfaceColor, 0x006cffff, 0, 0, 0, 0);
  assert.equal(colored, button.Button_MixColor(null, undefined, undefined, surfaceColor, 0x006cffff, 25));
  const neutral = button.Button_ButtonBorder(null, undefined, undefined,
    0, 1, 5, surfaceColor, 0x006cffff, 0x183858ff, 0, 0, 0);
  assert.equal(neutral, button.Button_MixColor(null, undefined, undefined, surfaceColor, 0x183858ff, 45));
}
const neutralBody = button.Button_MixColor(null, undefined, undefined, 0x101828ff, 0x334155ff, 85);
const neutralHover = button.Button_ButtonBackground(null, undefined, undefined,
  0, 1, 2, 0x101828ff, 0x2563ebff, 0x3b82f6ff, 0x1d4ed8ff,
  0x334155ff, 0xdc2626ff, 0x059669ff, 0xd97706ff);
assert.equal(neutralHover, button.Button_MixColor(null, undefined, undefined, neutralBody, 0x3b82f6ff, 12),
  "neutral hover must retain its body color beneath the cool reflection");
for (let state = 0; state <= 7; state++) {
  for (let bits = 0; bits < 8; bits++) {
    assert.deepEqual(style.Style_ResolveFlags(null, undefined, undefined, state,
      Boolean(bits & 1), Boolean(bits & 2), Boolean(bits & 4)), {
      disabled: Boolean(bits & 1) || state === 5,
      loading: Boolean(bits & 2) || state === 6,
      selected: Boolean(bits & 4) || state === 7,
    });
  }
}
for (let state = 1; state <= 7; state++) {
  assert.deepEqual(style.Style_ResolveInteraction(null, undefined, undefined, state, false, false, true, true, true, false), {
    state, hovered: state === 2, pressed: state === 3, focused: state === 4,
  });
  assert.deepEqual(style.Style_ResolveInteraction(null, undefined, undefined, 0, false, false, false, true, true, false), {
    state: 2, hovered: true, pressed: false, focused: true,
  });
}
const color = { r: 0, g: 0, b: 0, a: 0 };
const measureStyle = {
  fields: 0, background: color, foreground: color, border: color, focus: color,
  radius: 0, border_width: 0, opacity: 0, padding_x: 16, padding_y: 20,
  icon_size: 8.5, gap: 8, font_size: 0, content_offset: { x: 0, y: 0 },
  background_end: color, material: 0, typeface: "",
};
const measurement = {
  bounds: { x: 0, y: 0, width: 0, height: 0 }, label: "Run", font: 0, id: 0,
  tone: 0, emphasis: 0, size: 0, disabled: false, loading: false, selected: false,
  full_width: false, pill: false, circle: false,
  icon: { id: 1, width: 0, height: 0, mipmaps: 0, format: 0 }, icon_type: 0,
  icon_placement: 0, icon_only: false, square: false, state: 0,
  style: Object.fromEntries(["normal", "hover", "pressed", "focused", "disabled", "loading", "selected"].map(state => [state, measureStyle])),
};
let availableWidth = 0;
const drawingProps = { ...measurement, icon_type: 1, icon: { ...measurement.icon, id: 0 } };
const drawingStyle = { ...measureStyle, icon_size: 18, gap: 8, content_offset: { x: 2, y: -3 } };
const contentDrawing = disclosure => button.Button_PaintContent(null, undefined, undefined,
  drawingProps, { x: 10, y: 20, width: 200, height: 80 }, drawingStyle,
  32, 48, 0x12345680, 0xffffffff, 2, 375, disclosure);
let drawing = contentDrawing(false);
assert.equal(drawing.mark.kind, 2);
assert.deepEqual(drawing.mark.bounds, { x: 64, y: 36, width: 36, height: 36 });
assert.equal(drawing.label.kind, 1);
assert.equal(drawing.label.text, "Run");
assert.deepEqual(drawing.label.bounds, { x: 116, y: 14, width: 48, height: 80 });
drawingProps.icon.id = 7;
assert.equal(contentDrawing(false).mark.kind, 3);
assert.equal(contentDrawing(true).mark.kind, 5);
drawingProps.loading = true;
drawing = contentDrawing(true);
assert.equal(drawing.mark.kind, 4);
assert.equal(drawing.label.kind, 0);
assert.equal(drawing.mark.ring.x, 110);
assert.equal(drawing.mark.ring.y, 60);
assert.equal(drawing.mark.ring.outer_radius, 18);
function checkMeasurement(width, height) {
  assert.deepEqual(button.Button_MeasureBounds(null, undefined, undefined, measurement,
    measureStyle, 40, 27, 24, availableWidth, 1, false), { x: 0, y: 0, width, height });
}
checkMeasurement(72.5, 67);
Object.assign(measurement.bounds, { width: 120, height: 24 });
checkMeasurement(120, 24);
measurement.circle = true;
checkMeasurement(24, 24);
Object.assign(measurement, { circle: false, full_width: true });
measurement.bounds.width = 0;
availableWidth = 300;
checkMeasurement(300, 24);
availableWidth = 10;
checkMeasurement(24, 24);
measurement.icon_only = true;
measurement.bounds.height = 0;
checkMeasurement(40, 40);
Object.assign(measurement, { icon_only: false, full_width: false });
Object.assign(measurement.bounds, { x: 1.25, y: 2.5, width: 123.125, height: 24.0625 });
assert.deepEqual(button.Button_MeasureBounds(null, undefined, undefined, measurement,
  measureStyle, 80, 54, 48, 0, 1.3, false), measurement.bounds);
Object.assign(measurement.bounds, { width: 0, height: 0 });
measurement.icon.id = 0;
assert.deepEqual(button.Button_MeasureBounds(null, undefined, undefined, measurement,
  measureStyle, 80, 54, 48, 0, 2, true), { x: 1.25, y: 2.5, width: 145, height: 134 });
const measurements = [];
const measureHost = width => ({
  MeasureTextWidth(text, font, typeface) {
    measurements.push([text, font, typeface]);
    return width;
  }
});
assert.equal(button.Button_MeasureButton(null, undefined, measureHost(48), measurement,
  measureStyle, 80, 54, 0, 2, true).width, 145);
assert.equal(button.Button_MeasureButton(null, undefined, measureHost(68), measurement,
  measureStyle, 80, 54, 0, 2, true).width, 165);
assert.deepEqual(measurements, [["Run", 54, ""], ["Run", 54, ""]]);
let inputPolls = 0;
for (let state = 0; state <= 7; state++) {
  measurement.state = state;
  const enabled = state !== 5 && state !== 6;
  const input = button.Button_ReadButtonInput(null, undefined, {
    ReadActivation(bounds, id, allowed) {
      inputPolls++;
      assert.deepEqual(bounds, measurement.bounds);
      assert.equal(id, measurement.id);
      assert.equal(allowed, enabled);
      return { activated: true, pressed: true, hovered: true, focused: true };
    }
  }, measurement);
  assert.equal(input.activated, enabled);
  assert.equal(input.interaction.state, state || 3);
  assert.deepEqual(input.interaction, {
    state: state || 3,
    pressed: state === 0 || state === 3,
    hovered: state === 0 || state === 2,
    focused: state === 0 || state === 4,
  });
}
assert.equal(inputPolls, 8, "each Button input phase must poll exactly once");
const theme = await import(pathToFileURL(process.argv[4]).href);
const metrics = theme.Theme_DefaultMetrics(null);
for (const dark of [false, true]) {
  const palette = theme.Theme_DefaultPalette(null, undefined, undefined, dark);
  assert.equal(palette.link, dark ? 0x00bbffff : 0x0033ffff);
  assert.equal(palette.accent, 0x006cffff);
  for (let tone = 0; tone < 5; tone++) {
    for (let emphasis = 0; emphasis < 5; emphasis++) {
      for (let state = 1; state <= 7; state++) {
        for (let size = 0; size < 3; size++) {
          const result = button.Button_DefaultButtonStyle(null, undefined, undefined,
            tone, emphasis, state, size, false, false, palette, metrics);
          const background = button.Button_ButtonBackground(null, undefined, undefined,
            tone, emphasis, state, palette.surface, palette.accent,
            palette.accent_hover, palette.accent_pressed, palette.surface_raised,
            palette.danger, palette.success, palette.warning);
          assert.equal(result.background, background);
          const appearance = button.Button_ResolveAppearance(null, undefined, undefined,
            tone, emphasis, state, size, false, false, false, false, false,
            palette, metrics, stateStyles);
          assert.equal(appearance.background, result.background);
          assert.equal(appearance.radius, state);
          assert.equal(result.fields, 8191 | host.StyleMaterial | (dark && size === 2 ? host.StyleTypeface : 0));
          assert.equal(result.typeface, dark && size === 2 ? "semibold" : "");
          assert.equal(result.material, host.MaterialLightfield);
          assert.equal(result.opacity, 1);
          const softOutline = emphasis === 2 && (state === 1 || state === 2) && size !== 1;
          const softRest = size === 0 && (state === 1 || state === 2);
          const restingRadius = (metrics.radius_medium + metrics.radius_large) * 0.5;
          assert.equal(result.radius, softOutline ? metrics.radius_large
            : softRest ? restingRadius : metrics.radius_medium);
          assert.equal(result.font_size, size === 1 ? metrics.font_size_small
            : size === 2 ? metrics.font_size_large : metrics.font_size_medium);
          assert.equal(result.background_end, 0);
        }
      }
    }
  }
}
console.log("JavaScript cross-module style policy passed");
