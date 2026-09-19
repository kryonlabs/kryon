import assert from "node:assert/strict";
import * as kryon from "./kryon-runtime.js";

const rt = kryon.createRuntime();

rt.QueueMouseMove(10, 20);
rt.QueueMouseWheel(-2);
rt.QueueMouseButtonDown(kryon.MouseButtonLeft, 15, 25);
rt.QueueShortcut(kryon.KeyC);

kryon.beginFrame(rt);
assert.deepEqual(kryon.GetMousePosition(), {x: 15, y: 25});
assert.deepEqual(kryon.GetMouseDelta(), {x: 15, y: 25});
assert.equal(kryon.GetMouseX(), 15);
assert.equal(kryon.GetMouseY(), 25);
assert.equal(kryon.GetMouseWheelMove(), -2);
assert.deepEqual(kryon.GetMouseWheelMoveV(), {x: 0, y: -2});
assert.equal(kryon.IsMouseButtonPressed(kryon.MouseButtonLeft), true);
assert.equal(kryon.IsMouseButtonDown(kryon.MouseButtonLeft), true);
assert.equal(kryon.IsMouseButtonReleased(kryon.MouseButtonLeft), false);
assert.equal(kryon.IsMouseButtonUp(kryon.MouseButtonLeft), false);
assert.equal(kryon.IsKeyPressed(kryon.KeyC), true);
assert.equal(kryon.IsKeyDown(kryon.KeyC), true);
assert.equal(kryon.IsKeyDown(kryon.KeyLeftControl), true);
kryon.endFrame(rt);

kryon.beginFrame(rt);
assert.equal(kryon.IsMouseButtonPressed(kryon.MouseButtonLeft), false);
assert.equal(kryon.IsMouseButtonDown(kryon.MouseButtonLeft), true);
assert.equal(kryon.GetMouseWheelMove(), 0);
assert.deepEqual(kryon.GetMouseDelta(), {x: 0, y: 0});
assert.equal(kryon.IsKeyPressed(kryon.KeyC), false);
assert.equal(kryon.IsKeyDown(kryon.KeyC), false);
kryon.endFrame(rt);

rt.QueueMouseButtonUp(kryon.MouseButtonLeft, 30, 40);
kryon.beginFrame(rt);
assert.deepEqual(kryon.GetMousePosition(), {x: 30, y: 40});
assert.deepEqual(kryon.GetMouseDelta(), {x: 15, y: 15});
assert.equal(kryon.IsMouseButtonPressed(kryon.MouseButtonLeft), false);
assert.equal(kryon.IsMouseButtonDown(kryon.MouseButtonLeft), false);
assert.equal(kryon.IsMouseButtonReleased(kryon.MouseButtonLeft), true);
assert.equal(kryon.IsMouseButtonUp(kryon.MouseButtonLeft), true);
kryon.endFrame(rt);
