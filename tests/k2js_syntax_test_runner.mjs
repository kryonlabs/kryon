import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [generatedPath, runtimePath] = process.argv.slice(2);
const generated = await import(pathToFileURL(generatedPath).href);
const runtime = await import(pathToFileURL(runtimePath).href);

assert.equal(generated.app.title, "JS Smoke");
assert.equal(generated.app.width, 320);
assert.equal(generated.app.height, 240);

const state = generated.createState();
assert.equal(state.count, 0);
assert.equal(typeof generated.setHost, "function");
assert.equal(typeof generated.frame, "function");

const host = {
  HostValue(value) {
    return value + 41;
  }
};
generated.setHost(host);

const rt = runtime.createRuntime({ app: generated.app });
const snap = generated.frame(rt, state, host);
assert.equal(state.count, 1);
assert.equal(snap.frame.length, 3);
assert.equal(snap.frame[0].name, "Screen");
assert.equal(snap.frame[1].name, "Text");
assert.equal(snap.frame[2].name, "Button");
assert.equal(generated.Valid_CallHost(rt, state, host), 42);

const mounted = generated.main(null, host);
assert.equal(mounted.mounted, false);
