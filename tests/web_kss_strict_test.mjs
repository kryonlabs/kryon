import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [runtimePath] = process.argv.slice(2);
assert.ok(runtimePath, "usage: node tests/web_kss_strict_test.mjs web/kryon-runtime.js");

const runtime = await import(pathToFileURL(runtimePath).href);

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

runtime.clearWebStyleModules();
console.log("web kss strict ok");
