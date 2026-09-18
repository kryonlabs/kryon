import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [runtimePath] = process.argv.slice(2);
assert.ok(runtimePath, "usage: node tests/web_kss_strict_test.mjs web/kryon-runtime.js");

const runtime = await import(pathToFileURL(runtimePath).href);

// All generated parsers consume the same named environment contract.
const { readFileSync: readEnvironmentFixture } = await import("node:fs");
const kss = await import(new URL("./kss_parser.js", pathToFileURL(runtimePath)));
const environmentCases = readEnvironmentFixture(new URL("./fixtures/kss/environments.txt", import.meta.url), "utf8").trim().split("\n");
assert.equal(environmentCases.length, 7);
for (const line of environmentCases) {
  const fields = line.split(/\s+/);
  assert.equal(fields.length, 11);
  const names = fields.slice(0, 6).map(value => value === "-" ? "" : value);
  const environment = kss.KssParser_KssEnvironmentWithNames(null, null, null,
    kss.KssParser_KssDefaultEnvironment(null, null, null), ...names);
  assert.deepEqual([environment.theme, environment.contrast, environment.density,
    environment.pointer, environment.platform], fields.slice(6).map(Number), line);
  assert.equal(String.fromCharCode(...environment.variant.bytes.slice(0, environment.variant.length)), names[5]);
}

// Theme overlays, environment blocks, and imports all flow through the
// generated runtime/kss_parser.kry module; this layer only maps to CSS.

runtime.clearWebStyleModules();
assert.equal(runtime.registerWebStyleModule("base-tokens",
  "tokens { color { accent: #00ff00; ink: #ffffff; } }"), true);

const source = `
@pack strict-demo;
@import <base-tokens>;
tokens {
  color { accent: #112233; surface: #445566; }
  length { pad: 12; }
}
@theme dark {
  accent: #ffcc00;
}
@env contrast(high) {
  Button { border-width: 2; }
}
@env density(compact) {
  Button { padding-y: 4; }
}
Button {
  background: accent;
  color: ink;
  border-color: surface;
  border-radius: pad;
}
`;

const dark = runtime.parseWebStyleSheet(source, {}, {
  ...runtime.defaultWebStyleEnvironment(),
  theme: "dark", contrast: "high"
});
assert.equal(dark.pack, "strict-demo");
assert.equal(dark.rules.length, 2);
const envRule = dark.rules.find((rule) => rule.style["border-width"] !== undefined);
assert.ok(envRule, "contrast(high) rule survived");
assert.equal(envRule.style["border-width"], 2);
const button = dark.rules.find((rule) => rule.style["border-radius"] !== undefined);
assert.ok(button, "base button rule survived");
assert.equal(button.style.background, "#ffcc00", "dark theme overlay won");
assert.equal(button.style.color, "#ffffff", "imported token resolved");
assert.equal(button.style["border-color"], "#445566");
assert.equal(button.style["border-radius"], 12);
assert.equal(button.style["padding-y"], undefined, "density(compact) rule excluded");

const light = runtime.parseWebStyleSheet(source, {}, {
  ...runtime.defaultWebStyleEnvironment(),
  theme: "light", density: "compact"
});
const compact = light.rules.find((rule) => rule.style["padding-y"] !== undefined);
assert.ok(compact, "density(compact) rule survived");
assert.equal(compact.style["padding-y"], 4);
const lightButton = light.rules.find((rule) => rule.style["border-radius"] !== undefined);
assert.equal(lightButton.style.background, "#112233", "base token used without overlay");
assert.equal(light.rules.some((rule) => rule.style["border-width"] === 2), false,
  "contrast(high) rule excluded");

// Missing imports surface the shared parser diagnostic.
assert.throws(() => runtime.parseWebStyleSheet(
  "@import <missing-module>; Button { color: #fff; }"),
  /missing import/);

// Unknown directives remain errors.
assert.throws(() => runtime.parseWebStyleSheet(
  "@nonsense x; Button { color: #fff; }"),
  /unknown directive/);


// Shared fixture: the same matched.kss drives the C, Go, and web suites;
// winners must agree across all three runtimes.
{
  const { readFileSync } = await import("node:fs");
  const fixture = readFileSync(new URL("./fixtures/kss/matched.kss", import.meta.url), "utf8");
  const moduleSource = readFileSync(new URL("./fixtures/kss/matched_module.kss", import.meta.url), "utf8");
  runtime.clearWebStyleModules();
  runtime.registerWebStyleModule("matched-module", moduleSource);
  const sheet = runtime.parseWebStyleSheet(fixture, {}, {
    ...runtime.defaultWebStyleEnvironment(),
    theme: "dark", contrast: "high"
  });
  assert.equal(sheet.pack, "matched.demo");
  assert.equal(sheet.rules.length, 5);
  assert.equal(sheet.rules[0].selector.kind, "Surface");
  assert.equal(sheet.rules[0].style["padding-x"], 7);
  assert.equal(sheet.rules[1].style["border-width"], 2);
  const button = sheet.rules[2];
  assert.equal(button.style.background, "#ffcc00");
  assert.equal(button.style.foreground, "#f0f0f0");
  assert.equal(button.style.border, "#445566");
  assert.equal(button.style.radius, 0);
  assert.equal(button.style["padding-y"], 80);
  assert.equal(button.style["letter-spacing"], 2);
  assert.equal(button.style.material, "Flat");
  assert.equal(button.style.typeface, "semibold");
  assert.equal(sheet.rules[3].style.background, "#304050");
  assert.equal(sheet.rules[4].style.background, "transparent");
  assert.equal(sheet.rules[4].style.foreground, "black");
  assert.equal(sheet.rules[4].style.border, "white");

  // Declared variants: enumerated but inert unless the environment selects
  // them; the active variant overlays tokens (above theme) and adds rules.
  const glowSheet = runtime.parseWebStyleSheet(fixture, {}, {
    ...runtime.defaultWebStyleEnvironment(),
    theme: "dark", contrast: "high", variant: "glow"
  });
  assert.equal(glowSheet.rules.length, 6);
  assert.equal(glowSheet.rules[2].style.background, "#00ff00");
  assert.equal(glowSheet.rules[3].style["letter-spacing"], 9);
  assert.equal(glowSheet.rules[5].style.background, "transparent");

  // Inspector provenance: sheets report the active environment, declared
  // tokens with their origin kind, and per-rule source locations plus the
  // unresolved declaration text for token winners.
  assert.equal(sheet.environment.theme, "dark");
  assert.equal(glowSheet.environment.variant, "glow");
  assert.equal(sheet.tokens["shared-ink"], "pack");
  assert.ok(sheet.rules[2].sourceLine > 0);
  assert.equal(sheet.rules[2].raw.background, "accent");

  // Invalid-input sweep: truncations and deterministic mutations must never
  // crash the generated module.
  let seed = 0x5eed1234;
  const next = () => (seed = (seed * 1103515245 + 12345) >>> 0);
  /* Sample prefixes rather than walking every byte: the structured JS
   * emission trades parse speed for maintainability, so the sweep keeps
   * byte-level coverage through the mutations below instead. */
  for (let cut = 0; cut <= fixture.length; cut += 5) {
    try { runtime.parseWebStyleSheet(fixture.slice(0, cut)); } catch { /* diagnosed */ }
  }
  const mutated = Buffer.from(fixture, "utf8");
  for (let iteration = 0; iteration < 200; iteration++) {
    mutated.fill(0);
    mutated.write(fixture, 0);
    for (let flip = 0; flip < 3; flip++) {
      const index = (next() >> 8) % mutated.length;
      mutated[index] = next() & 0xff;
    }
    try { runtime.parseWebStyleSheet(mutated.toString("latin1")); } catch { /* diagnosed */ }
  }
  runtime.clearWebStyleModules();
}

// KSS formatter: same module as C and Go; segments assemble in the host.
{
  const { readFileSync } = await import("node:fs");
  const kfm = await import("../web/kss_formatter.js");
  kfm.setHost({ StringSlice: (source, start, length) => String(source).slice(start, start + length) });
  const assemble = (source, result) => {
    let out = "";
    for (let i = 0; i < result.count; i++) {
      const segment = result.segments[i];
      if (segment.kind === 0)
        out += source.slice(segment.start, segment.start + segment.length);
      else if (segment.kind === 1)
        out += String.fromCharCode(segment.atom);
      else
        out += "\n" + " ".repeat(segment.length);
    }
    return out;
  };
  const ugly = "@pack fmt.web;\n      tokens {\n   color { accent: #112233; }\n}\n\n\n" +
    "// keep me\nButton {\n    background: accent;\n  radius: 0;\n}";
  const first = kfm.KssFormatter_KssFormat(null, undefined, undefined, ugly);
  assert.equal(first.ok, true);
  const once = assemble(ugly, first);
  assert.ok(once.includes("// keep me"));
  const second = kfm.KssFormatter_KssFormat(null, undefined, undefined, once);
  assert.equal(second.ok, true);
  const twice = assemble(once, second);
  assert.equal(once, twice);
  const before = runtime.parseWebStyleSheet(ugly);
  const after = runtime.parseWebStyleSheet(once);
  assert.equal(after.rules.length, before.rules.length);
  assert.equal(after.rules[0].style.background, before.rules[0].style.background);
  assert.equal(after.rules[0].style.radius, before.rules[0].style.radius);
}

runtime.clearWebStyleModules();
console.log("web kss strict ok");
