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
assert.equal(generated.Valid_ApplyPreviewMode(rt, state, host, 1), 2);
assert.equal(runtime.GetTheme().mode, 1);
assert.equal(generated.Valid_ApplyPreviewMode(rt, state, host, 2), 3);
assert.equal(runtime.GetTheme().mode, 2);
assert.equal(generated.Valid_FractionalPreviewMode(rt, state, host, 1.75), 1);
assert.equal(generated.Valid_FractionalPreviewMode(rt, state, host, -1.75), -1);
const snap = generated.frame(rt, state, host);
assert.equal(state.count, 1);
assert.equal(state.viewport_width, 320);
assert.equal(state.viewport_height, 240);
assert.equal(snap.frame.length, 3);
assert.equal(snap.frame[0].name, "Screen");
assert.equal(snap.frame[1].name, "Text");
assert.equal(snap.frame[2].name, "Button");
assert.equal(typeof snap.frame[2].args, "object");
assert.deepEqual(snap.frame[2].args.bounds, [10, 50, 120, 28]);
assert.equal(snap.frame[2].args.label, "Tap");
assert.equal(snap.frame[2].args.style.normal.radius, 6);
assert.equal(snap.frame[2].args.style.normal.fields, 16);
assert.equal(generated.Valid_CallHost(rt, state, host), 42);

rt.target = { clientWidth: 640, clientHeight: 480 };
generated.frame(rt, state, host);
assert.equal(state.count, 2);
assert.equal(state.viewport_width, 640);
assert.equal(state.viewport_height, 480);
rt.target.clientWidth = 0;
rt.target.clientHeight = 0;
generated.frame(rt, state, host);
assert.equal(state.viewport_width, 320);
assert.equal(state.viewport_height, 240);

const defaultSnapshot = generated.frame();
assert.equal(generated.moduleState.viewport_width, 320);
assert.equal(generated.moduleState.viewport_height, 240);
assert.equal(defaultSnapshot.frame.length, 3);

const mounted = generated.main(null, host);
assert.equal(mounted.mounted, false);

const styleRuntime = runtime.createRuntime();
runtime.beginFrame(styleRuntime);
generated.Valid_StyleCopies(styleRuntime, state, host);
const styleFrame = runtime.endFrame(styleRuntime).frame;
assert.deepEqual(styleFrame[0].args.bounds, [10, 20, 50, 40]);
const styles = styleFrame.map(item => item.args.style.normal);
assert.deepEqual(styles.map(style => style.font_size), [19, 24, 32]);
assert.deepEqual(styles.map(style => style.content_offset), [[0, 1], [0, 1], [0, -1]]);

for (const action of [generated.Valid_DirectAction, generated.Valid_StoredAction, generated.Valid_AssignedAction]) {
  const actionRuntime = runtime.createRuntime({ app: generated.app });
  for (const tapped of [false, true, false]) {
    if (tapped) actionRuntime.QueueTap(30, 110);
    runtime.beginFrame(actionRuntime);
    assert.equal(action(actionRuntime, state, host, 20), tapped);
    const result = runtime.endFrame(actionRuntime);
    assert.equal(result.frame.length, 1);
    assert.equal(result.frame[0].name, "Button");
    assert.deepEqual(result.frame[0].args.bounds, [20, 100, 80, 32]);
  }
}
